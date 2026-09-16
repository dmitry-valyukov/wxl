# Переносит определения языков RsdnFormatter (Format/CodeFormat/Syntax/*.json,
# MIT, © Russian Software Developer Network) в таблицы src/languages.cpp.
# Языки, которых у RsdnFormatter нет, описаны в том же виде рядом со
# скриптом, в tools/syntax, и приезжают оттуда же.
#
# Ключевые слова приезжают из JSON как есть — отсортированными по кодовым
# единицам и, для языков без учёта регистра, в нижнем регистре: так их ищет
# двоичным поиском сканер. Комментарии и строки в JSON описаны регулярными
# выражениями .NET, которых у wxl нет и не будет, поэтому их перевод в
# правила «открыл — закрыл» сделан руками и лежит здесь же, в $Delimiters.
#
# Запуск из корня дерева wxl:
#   pwsh -NoProfile -File wxl.highlight/tools/import-syntax.ps1 -Source M:\source\RsdnFormatter

param(
    [string]$Source = 'M:\source\RsdnFormatter',
    [string]$Out = "$PSScriptRoot/../src/languages.cpp"
)

$ErrorActionPreference = 'Stop'

# ---- правила комментариев и строк, переведённые с регулярных выражений ----
#
# Поле: kind open close prefixes escape doubled multiline lineStart singleChar
#   kind       comment | string
#   close      пусто — до конца строки
#   prefixes   символы, один из которых может стоять перед open (u"..." в Python)
#   escape     символ экранирования внутри, '' — нет
#   doubled    удвоенный close внутри — не конец ('it''s' в Pascal)
#   multiline  может пересекать перевод строки
#   lineStart  open только в начале строки (=begin в Ruby, POD в Perl)
#   singleChar внутри ровно один символ или одна escape-последовательность
#              (символьный литерал C-семейства: иначе 'a в Rust съело бы строку)
#   afterSpace open только с начала слова (решётка shell: ${имя#хвост} — не
#              комментарий)

function Rule($kind, $open, $close, $prefixes = '', $escape = '', $doubled = $false,
           $multiline = $false, $lineStart = $false, $singleChar = $false,
           $afterSpace = $false) {
    [pscustomobject]@{
        kind = $kind; open = $open; close = $close; prefixes = $prefixes; escape = $escape
        doubled = $doubled; multiline = $multiline; lineStart = $lineStart
        singleChar = $singleChar; afterSpace = $afterSpace
    }
}

# Куски, которые повторяются от языка к языку: комментарий до конца строки,
# внутристрочный комментарий, кавычки. Языку остаётся собрать свой набор из
# них и дописать своё.
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

# Семейство ECMAScript: к сишным комментариям строка в обратных кавычках,
# которая живёт через переводы строк.
$jsFamily = @(
    $slashLine,
    $slashBlock,
    (Rule string '`' '`' -escape '\' -multiline $true),
    $dqString,
    $sqString
)

$Delimiters = @{
    Assembler   = @((Rule comment ';' ''), (Rule string "'" "'" -doubled $true))
    C           = $cFamily
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

    # ---- языки, которых у RsdnFormatter нет: их описания в tools/syntax ----

    JavaScript  = $jsFamily
    TypeScript  = $jsFamily
    CSS         = @($slashBlock, $dqString, $sqString)
    Go          = @($slashLine, $slashBlock, (Rule string '`' '`' -multiline $true),
                    $dqString, $charLiteral)
    # Длинные скобки Lua: комментарий --[[ ]] раньше построчного --, иначе
    # тот съел бы его первую строку.
    Lua         = @((Rule comment '--[[' ']]' -multiline $true), (Rule comment '--' ''),
                    (Rule string '[[' ']]' -multiline $true), $dqString, $sqString)
    CMake       = @((Rule comment '#[[' ']]' -multiline $true -afterSpace $true), $shellHash,
                    (Rule string '"' '"' -escape '\' -multiline $true))
    Bash        = @($shellHash, (Rule string '"' '"' -escape '\' -multiline $true),
                    (Rule string "'" "'" -multiline $true))
    JSON        = @($dqString)
}

# Группа ключевых слов ассемблера, которую JSON держит регулярными
# выражениями (@cpu, ??date, (\.|@|\b)model): те же слова через виды границ.
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

# Имена тегов кода — какой язык (FormatterHelper.CodeFormat.cs и
# BBCode/Parser.cs; [code] без языка — не язык вовсе).
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
    return "L`"$escaped`""
}

function Ch([string]$c) {
    if ($c -eq '') { return '0' }
    if ($c -eq '\') { return "L'\\'" }
    if ($c -eq "'") { return "L'\''" }
    return "L'$c'"
}

function Bool($b) { if ($b) { 'true' } else { 'false' } }

$syntaxDir = Join-Path $Source 'Format/CodeFormat/Syntax'
$ownDir = Join-Path $PSScriptRoot 'syntax'

# Сначала перенесённые из RsdnFormatter, потом свои: так номера языков в
# таблице псевдонимов не разъезжаются, когда своих прибавляется.
$files = @(Get-ChildItem $syntaxDir -Filter '*.json' | Sort-Object Name) +
         @(Get-ChildItem $ownDir -Filter '*.json' | Sort-Object Name)

$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine(@'
// Таблицы языков подсветки. СГЕНЕРИРОВАНО tools/import-syntax.ps1 из двух
// источников: определений RsdnFormatter (Format/CodeFormat/Syntax/*.json,
// MIT, © Russian Software Developer Network) и наших собственных описаний
// в tools/syntax — править не здесь, а в скрипте или в источнике.
//
// Ключевые слова каждого набора отсортированы по кодовым единицам (у
// языков без учёта регистра — в нижнем регистре): сканер ищет двоичным
// поиском. Комментарии и строки в источнике описаны регулярными
// выражениями; их перевод в правила «открыл — закрыл» — таблица
// $Delimiters в скрипте.

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
    if (-not $Delimiters.ContainsKey($name)) { throw "Нет правил комментариев и строк для $name" }
    if (-not $Aliases.Contains($name)) { throw "Нет имён тегов для $name" }

    [void]$sb.AppendLine("// ---- $name ----")
    [void]$sb.AppendLine()

    # Разделители.
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

    # Наборы ключевых слов.
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
        [void]$sb.AppendLine("constexpr std::wstring_view $setName[] = {")
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

[void]$sb.AppendLine('// ---- все языки ----')
[void]$sb.AppendLine()
[void]$sb.AppendLine('constexpr language kLanguages[] = {')
foreach ($e in $languageEntries) { [void]$sb.AppendLine($e) }
[void]$sb.AppendLine('};')
[void]$sb.AppendLine()

[void]$sb.AppendLine('struct alias_t {')
[void]$sb.AppendLine('    std::wstring_view tag;')
[void]$sb.AppendLine('    std::size_t language;  // индекс в kLanguages')
[void]$sb.AppendLine('};')
[void]$sb.AppendLine()
[void]$sb.AppendLine('// Имена тегов кода в нижнем регистре — какой язык.')
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

const language* find_language(std::wstring_view tag) noexcept {
    // Имя тега короткое; сравнение без учёта регистра ASCII, как у самих
    // тегов разметки ([C#] и [c#] — один язык).
    for (const alias_t& alias : kAliases) {
        if (alias.tag.size() != tag.size()) continue;
        bool same = true;
        for (std::size_t i = 0; i < tag.size() && same; ++i) {
            wchar_t c = tag[i];
            if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c + 32);
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
Write-Host "Записано: $outPath ($($files.Count) языков)"
