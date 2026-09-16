# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""The property table behind wxl::core's grapheme cluster boundaries.

Needs uv: run it as

    uv run wxl.core/tools/gen-grapheme-table.py

from the repository root, and uv brings the interpreter itself -- the header
above is what it reads. Plain `python` on Windows is often the Store stub.

The script downloads four files of one Unicode version from unicode.org:
three that carry the properties UAX #29 needs (Grapheme_Cluster_Break,
Extended_Pictographic, Indic_Conjunct_Break) and the conformance test for
them. It writes

    wxl.core/src/grapheme_data.inc          the table, included by grapheme.cpp
    wxl.core/tests/data/GraphemeBreakTest.txt   the test, read by grapheme_tests.cpp

The version is pinned, not "latest": the same script run a year apart has to
produce the same file until someone raises UNICODE_VERSION on purpose, and the
test that checks the table has to come from the same release as the table.
Downloads are cached by version, so a run reads the network once per version.

Only the data is generated. The numbering of the properties below is a
contract with grapheme.cpp, which spells the same values out by hand; the
conformance test is what catches the two drifting apart.
"""

import argparse
import pathlib
import urllib.request

UNICODE_VERSION = "18.0.0"

BASE = "https://www.unicode.org/Public/{version}/ucd/"
FILES = {
    "GraphemeBreakProperty.txt": "auxiliary/GraphemeBreakProperty.txt",
    "GraphemeBreakTest.txt": "auxiliary/GraphemeBreakTest.txt",
    "emoji-data.txt": "emoji/emoji-data.txt",
    "DerivedCoreProperties.txt": "DerivedCoreProperties.txt",
}

# Low four bits: Grapheme_Cluster_Break. Anything the file does not list is
# Other, which is what its @missing line says.
GCB = [
    "Other", "CR", "LF", "Control", "Extend", "ZWJ", "Regional_Indicator",
    "Prepend", "SpacingMark", "L", "V", "T", "LV", "LVT",
]
# Bits 4-5: Indic_Conjunct_Break. Bit 6: Extended_Pictographic.
INCB = {"None": 0, "Linker": 1, "Consonant": 2, "Extend": 3}
EXTENDED_PICTOGRAPHIC = 0x40

CODE_POINTS = 0x110000
PAGE_SHIFT = 7          # 128 code points a page -- the smallest table measured
PAGE = 1 << PAGE_SHIFT


def fetch(cache: pathlib.Path, version: str) -> dict[str, str]:
    cache.mkdir(parents=True, exist_ok=True)
    texts = {}
    for name, path in FILES.items():
        local = cache / name
        if not local.exists():
            url = BASE.format(version=version) + path
            print(f"downloading {url}")
            with urllib.request.urlopen(url) as response:
                local.write_bytes(response.read())
        texts[name] = local.read_text(encoding="utf-8")
        first = texts[name].splitlines()[0]
        # Every UCD file names its version on the first line; a cache or a
        # redirect that handed back another release is caught here.
        if name != "emoji-data.txt" and version not in first:
            raise SystemExit(f"{name} is not version {version}: {first}")
    return texts


def records(text: str):
    """(first, last, fields) for every data line of a UCD file."""
    for line in text.splitlines():
        data = line.split("#", 1)[0].strip()
        if not data:
            continue
        fields = [field.strip() for field in data.split(";")]
        span = fields[0].split("..")
        first = int(span[0], 16)
        last = int(span[1], 16) if len(span) > 1 else first
        yield first, last, fields[1:]


def build(texts: dict[str, str]) -> bytearray:
    values = bytearray(CODE_POINTS)

    for first, last, fields in records(texts["GraphemeBreakProperty.txt"]):
        gcb = GCB.index(fields[0])
        for code in range(first, last + 1):
            values[code] = (values[code] & ~0x0F) | gcb

    for first, last, fields in records(texts["DerivedCoreProperties.txt"]):
        if fields[0] != "InCB":
            continue
        incb = INCB[fields[1]]
        for code in range(first, last + 1):
            values[code] = (values[code] & ~0x30) | (incb << 4)

    for first, last, fields in records(texts["emoji-data.txt"]):
        if fields[0] != "Extended_Pictographic":
            continue
        for code in range(first, last + 1):
            values[code] |= EXTENDED_PICTOGRAPHIC

    return values


def paginate(values: bytearray) -> tuple[list[int], list[bytes]]:
    index, pages, seen = [], [], {}
    for start in range(0, CODE_POINTS, PAGE):
        page = bytes(values[start:start + PAGE])
        if page not in seen:
            seen[page] = len(pages)
            pages.append(page)
        index.append(seen[page])
    if len(pages) > 256:
        raise SystemExit(f"{len(pages)} distinct pages do not fit a byte index")
    return index, pages


def byte_rows(data, per_row: int = 24) -> str:
    return "\n".join(
        "    " + ", ".join(f"0x{b:02X}" for b in data[row:row + per_row]) + ","
        for row in range(0, len(data), per_row)
    )


def write_table(path: pathlib.Path, version: str, index: list[int], pages: list[bytes]) -> None:
    flat = b"".join(pages)
    path.write_text(
        f"""// Generated by wxl.core/tools/gen-grapheme-table.py from Unicode {version}
// (GraphemeBreakProperty.txt, DerivedCoreProperties.txt, emoji-data.txt).
// Do not edit: raise UNICODE_VERSION in the script and run it with uv.
//
// A byte per code point: Grapheme_Cluster_Break in the low four bits,
// Indic_Conjunct_Break in bits 4-5, Extended_Pictographic in bit 6. Stored as
// {len(pages)} distinct pages of {PAGE} code points and an index of one byte per
// page: {len(index)} + {len(flat)} bytes.

inline constexpr std::string_view grapheme_table_version = "{version}";

inline constexpr int grapheme_page_shift = {PAGE_SHIFT};

inline constexpr std::uint8_t grapheme_page_index[{len(index)}] = {{
{byte_rows(index)}
}};

inline constexpr std::uint8_t grapheme_pages[{len(flat)}] = {{
{byte_rows(flat)}
}};
""",
        encoding="utf-8",
        newline="\n",
    )


def main() -> None:
    root = pathlib.Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--version", default=UNICODE_VERSION)
    parser.add_argument("--cache", type=pathlib.Path,
                        default=root / "build" / "unicode")
    arguments = parser.parse_args()

    texts = fetch(arguments.cache / arguments.version, arguments.version)
    index, pages = paginate(build(texts))

    write_table(root / "wxl.core" / "src" / "grapheme_data.inc", arguments.version, index, pages)

    tests = root / "wxl.core" / "tests" / "data"
    tests.mkdir(parents=True, exist_ok=True)
    (tests / "GraphemeBreakTest.txt").write_text(
        texts["GraphemeBreakTest.txt"], encoding="utf-8", newline="\n")

    print(f"Unicode {arguments.version}: {len(pages)} pages, "
          f"{len(index) + len(pages) * PAGE} bytes")


if __name__ == "__main__":
    main()
