# Converts the RsdnFormatter language definitions (Format/CodeFormat/Syntax/*.json,
# MIT, © Russian Software Developer Network) into the tables of src/languages.cpp.
# Languages that RsdnFormatter lacks are described in the same format next to the
# script, in tools/syntax, and are read from there.
#
# The keywords are taken from the JSON, sorted by code units and, for case-insensitive
# languages, converted to lowercase, as the scanner's binary search requires. The JSON
# describes comments and strings with .NET regular expressions, which wxl does not
# provide, so their translation into opening and closing sequences is written by hand
# here, in $Delimiters.
#
# Run from the root of the wxl tree:
#   pwsh -NoProfile -File wxl.highlight/tools/import-syntax.ps1 -Source M:\source\RsdnFormatter

param(
    [string]$Source = 'M:\source\RsdnFormatter',
    [string]$Out = "$PSScriptRoot/../src/languages.cpp"
)

$ErrorActionPreference = 'Stop'

# ---- comment and string rules, translated from the regular expressions ----
#
# Fields: kind open close prefixes escape doubled multiline lineStart singleChar afterSpace
#   kind       comment | string
#   close      empty: the piece ends with the line
#   prefixes   characters one of which may precede open (u"..." in Python)
#   escape     the escape character inside; '' if there is none
#   doubled    a doubled close inside does not end the piece ('it''s' in Pascal)
#   multiline  the piece may cross a line break
#   lineStart  open only at the start of a line (=begin in Ruby, POD in Perl)
#   singleChar one character or one escape sequence inside (a character literal of
#              the C family, so that a Rust lifetime 'a opens no string)
#   afterSpace open only at the start of a word (the shell #, not a comment in
#              ${name#pattern})

function Rule($kind, $open, $close, $prefixes = '', $escape = '', $doubled = $false,
           $multiline = $false, $lineStart = $false, $singleChar = $false,
           $afterSpace = $false) {
    [pscustomobject]@{
        kind = $kind; open = $open; close = $close; prefixes = $prefixes; escape = $escape
        doubled = $doubled; multiline = $multiline; lineStart = $lineStart
        singleChar = $singleChar; afterSpace = $afterSpace
    }
}

# Rules shared by several languages: comments to the end of the line, inline
# comments, quotes. A language combines them and adds its own.
$slashLine  = Rule comment '//' ''
$slashBlock = Rule comment '/*' '*/' -multiline $true
$hashLine   = Rule comment '#' ''
$shellHash  = Rule comment '#' '' -afterSpace $true
$dqString   = Rule string '"' '"' -escape '\'
$sqString   = Rule string "'" "'" -escape '\'
$charLiteral = Rule string "'" "'" -escape '\' -singleChar $true

$cFamily = @(
    $slashLine,
    $slashBlock,
    (Rule string '@"' '"' -doubled $true -multiline $true),
    $dqString,
    $charLiteral
)

# The ECMAScript family: the C-style comments and a string in backticks that may
# cross line breaks.
$jsFamily = @(
    $slashLine,
    $slashBlock,
    (Rule string '`' '`' -escape '\' -multiline $true),
    $dqString,
    $sqString
)

$Delimiters = @{
    Assembler   = @((Rule comment ';' ''), (Rule string "'" "'" -doubled $true))
    # C and C++: the prefixes u, U, L and u8 (8 in the list allows u8).
    C           = @($slashLine, $slashBlock,
                    (Rule string '@"' '"' -doubled $true -multiline $true),
                    (Rule string '"' '"' -prefixes 'uUL8' -escape '\'),
                    (Rule string "'" "'" -prefixes 'uUL8' -escape '\' -singleChar $true))
    CSharp      = $cFamily
    Nemerle     = $cFamily
    Nitra       = $cFamily
    Rust        = $cFamily
    Erlang      = @((Rule comment '%' ''), $dqString, $sqString)
    Haskell     = @((Rule comment '--' ''), (Rule comment '{-' '-}' -multiline $true),
                    $dqString, $sqString)
    IDL         = @($slashLine, $slashBlock, $dqString, $sqString)
    Java        = @($slashLine, $slashBlock, $dqString)
    Lisp        = @((Rule comment ';' ''), (Rule comment '#|' '|#' -multiline $true), $dqString)
    MSIL        = @($slashLine, $slashBlock, $dqString)
    ObjC        = @($slashLine, $slashBlock, $dqString, $sqString)
    Ocaml       = @((Rule comment '(*' '*)' -multiline $true),
                    $dqString, (Rule string "'" "'"))
    PHP         = @($slashLine, $hashLine, $slashBlock, $dqString, $sqString)
    Pascal      = @((Rule comment '{' '}' -multiline $true), (Rule comment '(*' '*)' -multiline $true),
                    $slashLine, (Rule string "'" "'" -doubled $true))
    Perl        = @($hashLine, (Rule comment '=' '=cut' -multiline $true -lineStart $true),
                    $dqString, (Rule string "'" "'" -doubled $true))
    Prolog      = @((Rule comment '%' ''), $slashBlock, $dqString)
    Python      = @($hashLine,
                    (Rule comment '"""' '"""' -prefixes 'uUrR' -multiline $true),
                    (Rule comment "'''" "'''" -prefixes 'uUrR' -multiline $true),
                    (Rule string '"' '"' -prefixes 'uUrR' -escape '\'),
                    (Rule string "'" "'" -prefixes 'uUrR' -escape '\'))
    Ruby        = @($hashLine, (Rule comment '=begin' '=end' -multiline $true -lineStart $true),
                    $dqString, $sqString)
    SQL         = @((Rule comment '--' ''), $slashBlock, (Rule string "'" "'" -doubled $true))
    VisualBasic = @((Rule comment "'" ''), (Rule string '"' '"' -doubled $true))
    XSL         = @((Rule comment '<!--' '-->' -multiline $true), (Rule string '"' '"'))

    # ---- languages that RsdnFormatter lacks, described in tools/syntax ----

    JavaScript  = $jsFamily
    TypeScript  = $jsFamily
    CSS         = @($slashBlock, $dqString, $sqString)
    Go          = @($slashLine, $slashBlock, (Rule string '`' '`' -multiline $true),
                    $dqString, $charLiteral)
    # Lua long brackets: the --[[ ]] comment precedes the -- line comment, which
    # would otherwise match its first line.
    Lua         = @((Rule comment '--[[' ']]' -multiline $true), (Rule comment '--' ''),
                    (Rule string '[[' ']]' -multiline $true), $dqString, $sqString)
    CMake       = @((Rule comment '#[[' ']]' -multiline $true -afterSpace $true), $shellHash,
                    (Rule string '"' '"' -escape '\' -multiline $true))
    Bash        = @($shellHash, (Rule string '"' '"' -escape '\' -multiline $true),
                    (Rule string "'" "'" -multiline $true))
    JSON        = @($dqString)
}

# Assembler keywords that the JSON describes with regular expressions (@cpu, ??date,
# (\.|@|\b)model): the same words expressed with boundary kinds.
$AssemblerExtra = @(
    @{ prefix = 'None'; postfix = 'WordBoundary'
       keywords = @('@codesize', '@cpu', '@datasize', '@filename', '@wordsize',
                    '??date', '??filename', '??time') },
    @{ prefix = 'AtSignOrWord'; postfix = 'WordBoundary'; keywords = @('curseg') },
    @{ prefix = 'WordBoundaryOrDoubleQuestion'; postfix = 'WordBoundary'; keywords = @('version') },
    @{ prefix = 'DotOrAtOrWord'; postfix = 'WordBoundary'
       keywords = @('model', 'startup', 'code', 'data', 'fardata') },
    @{ prefix = 'WordBoundary'; postfix = 'None'; keywords = @('carry?', 'data?', 'fardata?') }
)

# Code tag names of each language (FormatterHelper.CodeFormat.cs and
# BBCode/Parser.cs; [code] without a language names no language).
$Aliases = [ordered]@{
    Assembler   = @('asm', 'assembly')
    C           = @('c', 'cpp', 'c++', 'ccode')
    CSharp      = @('c#', 'cs', 'csharp', 'cscode')
    Erlang      = @('erlang', 'erl')
    Haskell     = @('haskell', 'hs')
    IDL         = @('idl', 'midl')
    Java        = @('java')
    Lisp        = @('lisp')
    MSIL        = @('il', 'msil')
    Nemerle     = @('nemerle')
    Nitra       = @('nitra')
    ObjC        = @('objc', 'objectivec')
    Ocaml       = @('ml', 'ocaml')
    PHP         = @('php')
    Pascal      = @('pascal', 'delphi')
    Perl        = @('perl')
    Prolog      = @('prolog')
    Python      = @('py', 'python')
    Ruby        = @('rb', 'ruby')
    Rust        = @('rust')
    SQL         = @('sql')
    VisualBasic = @('vb', 'vbnet', 'vbcode', 'vbscript', 'vbs')
    XSL         = @('xml', 'xsl', 'html')
    JavaScript  = @('js', 'javascript', 'jscript')
    TypeScript  = @('ts', 'typescript')
    CSS         = @('css')
    Go          = @('go', 'golang')
    Lua         = @('lua')
    CMake       = @('cmake')
    Bash        = @('bash', 'sh', 'shell')
    JSON        = @('json')
}

$BoundaryNames = @{
    None = 'none'; WordBoundary = 'word'; Dot = 'dot'; DotOrWord = 'dot_or_word'
    HashWithSpace = 'hash_with_space'; AtSignOrWord = 'at_or_word'
    DotOrAtOrWord = 'dot_or_at_or_word'; DoubleQuestion = 'double_question'
    NotAmpersand = 'not_ampersand'; WordBoundaryOrDoubleQuestion = 'word_or_double_question'
    ExclamationAndWordBoundary = 'exclamation_and_word'; OpenParen = 'open_paren'
}

function Lit([string]$s) {
    $escaped = $s.Replace('\', '\\').Replace('"', '\"')
    return "u`"$escaped`""
}

function Ch([string]$c) {
    if ($c -eq '') { return '0' }
    if ($c -eq '\') { return "u'\\'" }
    if ($c -eq "'") { return "u'\''" }
    return "u'$c'"
}

function Bool($b) { if ($b) { 'true' } else { 'false' } }

$syntaxDir = Join-Path $Source 'Format/CodeFormat/Syntax'
$ownDir = Join-Path $PSScriptRoot 'syntax'

# The RsdnFormatter languages first, then those of tools/syntax: adding a language to
# tools/syntax then leaves the alias-table indices of the RsdnFormatter languages unchanged.
$files = @(Get-ChildItem $syntaxDir -Filter '*.json' | Sort-Object Name) +
         @(Get-ChildItem $ownDir -Filter '*.json' | Sort-Object Name)

$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine(@'
// Language tables for highlighting. GENERATED by tools/import-syntax.ps1 from two
// sources: the RsdnFormatter definitions (Format/CodeFormat/Syntax/*.json, MIT,
// © Russian Software Developer Network) and the definitions in tools/syntax. Do not
// edit this file; edit the script or the source definitions.
//
// The keywords of each set are sorted by code units (lowercase for case-insensitive
// languages), since the scanner uses binary search. The sources describe comments
// and strings with regular expressions; their translation into opening and closing
// sequences is the $Delimiters table in the script.

module wxl.highlight;

import std;

namespace wxl::highlight {

namespace {

using enum kind_t;
using enum boundary_t;
'@)

$languageEntries = @()

foreach ($file in $files) {
    $json = Get-Content -Raw -Path $file.FullName | ConvertFrom-Json
    $name = $json.name
    $ci = ($json.options -and $json.options.Contains('i'))
    if (-not $Delimiters.ContainsKey($name)) { throw "No comment and string rules for $name" }
    if (-not $Aliases.Contains($name)) { throw "No code tag names for $name" }

    [void]$sb.AppendLine("// ---- $name ----")
    [void]$sb.AppendLine()

    # Delimiters.
    [void]$sb.AppendLine("constexpr delimited_rule k${name}Delimiters[] = {")
    foreach ($r in $Delimiters[$name]) {
        $line = "    {{{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}}}," -f `
            $r.kind, (Lit $r.open), (Lit $r.close), (Lit $r.prefixes), (Ch $r.escape), `
            (Bool $r.doubled), (Bool $r.multiline), (Bool $r.lineStart), (Bool $r.singleChar), `
            (Bool $r.afterSpace)
        [void]$sb.AppendLine($line)
    }
    [void]$sb.AppendLine('};')
    [void]$sb.AppendLine()

    # Keyword sets.
    $sets = @()
    $index = 0
    $groups = @()
    foreach ($p in $json.patterns) {
        if ($p.type -eq 'keyword' -and $p.keywords.Count -gt 0) {
            $groups += @{ prefix = $p.prefix; postfix = $p.postfix; keywords = @($p.keywords) }
        }
    }
    if ($name -eq 'Assembler') { $groups += $AssemblerExtra }

    foreach ($g in $groups) {
        $words = [System.Collections.Generic.List[string]]::new()
        foreach ($w in $g.keywords) {
            if ([string]::IsNullOrEmpty($w)) { continue }
            $words.Add($(if ($ci) { $w.ToLowerInvariant() } else { $w }))
        }
        $arr = $words.ToArray()
        [Array]::Sort($arr, [System.StringComparer]::Ordinal)
        $arr = @($arr | Select-Object -Unique)
        $longest = ($arr | ForEach-Object { $_.Length } | Measure-Object -Maximum).Maximum

        $setName = "k${name}Words$index"
        [void]$sb.AppendLine("constexpr std::u16string_view $setName[] = {")
        $line = '   '
        foreach ($w in $arr) {
            $token = ' ' + (Lit $w) + ','
            if ($line.Length + $token.Length -gt 96) {
                [void]$sb.AppendLine($line)
                $line = '   '
            }
            $line += $token
        }
        [void]$sb.AppendLine($line)
        [void]$sb.AppendLine('};')
        [void]$sb.AppendLine()

        $prefix = $BoundaryNames[$(if ($g.prefix) { $g.prefix } else { 'None' })]
        $postfix = $BoundaryNames[$(if ($g.postfix) { $g.postfix } else { 'None' })]
        $sets += "    {{{0}, {1}, {2}, {3}}}," -f $prefix, $postfix, $setName, $longest
        $index++
    }

    [void]$sb.AppendLine("constexpr keyword_set k${name}Keywords[] = {")
    foreach ($s in $sets) { [void]$sb.AppendLine($s) }
    [void]$sb.AppendLine('};')
    [void]$sb.AppendLine()

    $languageEntries += "    {{{0}, {1}, k{2}Delimiters, k{2}Keywords}}," -f (Lit $name), (Bool $ci), $name
}

[void]$sb.AppendLine('// ---- all languages ----')
[void]$sb.AppendLine()
[void]$sb.AppendLine('constexpr language kLanguages[] = {')
foreach ($e in $languageEntries) { [void]$sb.AppendLine($e) }
[void]$sb.AppendLine('};')
[void]$sb.AppendLine()

[void]$sb.AppendLine('struct alias_t {')
[void]$sb.AppendLine('    std::u16string_view tag;')
[void]$sb.AppendLine('    std::size_t language;  // index into kLanguages')
[void]$sb.AppendLine('};')
[void]$sb.AppendLine()
[void]$sb.AppendLine('// Lowercase names of code tags and the languages they name.')
[void]$sb.AppendLine('constexpr alias_t kAliases[] = {')
$li = 0
foreach ($file in $files) {
    $name = (Get-Content -Raw -Path $file.FullName | ConvertFrom-Json).name
    foreach ($a in $Aliases[$name]) {
        [void]$sb.AppendLine(("    {{{0}, {1}}},  // {2}" -f (Lit $a), $li, $name))
    }
    $li++
}
[void]$sb.AppendLine('};')
[void]$sb.AppendLine()
[void]$sb.AppendLine(@'
}  // namespace

const language* find_language(std::u16string_view tag) noexcept {
    // Tag names are short; the comparison ignores ASCII case, as the markup tags do
    // ([C#] and [c#] name one language).
    for (const alias_t& alias : kAliases) {
        if (alias.tag.size() != tag.size()) continue;
        bool same = true;
        for (std::size_t i = 0; i < tag.size() && same; ++i) {
            char16_t c = tag[i];
            if (c >= u'A' && c <= u'Z') c = static_cast<char16_t>(c + 32);
            same = c == alias.tag[i];
        }
        if (same) return &kLanguages[alias.language];
    }
    return nullptr;
}

std::span<const language> languages() noexcept { return kLanguages; }

}  // namespace wxl::highlight
'@)

$text = $sb.ToString().Replace("`r`n", "`n")
$outPath = [System.IO.Path]::GetFullPath($Out)
[System.IO.File]::WriteAllText($outPath, $text, [System.Text.UTF8Encoding]::new($false))
Write-Host "Written: $outPath ($($files.Count) languages)"
