# bible_text branch (Windows) — assets from Desktop\assets-4win
$ErrorActionPreference = "Continue"
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
# Walk up to *.monads/*.book-stack
$BookStack = (Resolve-Path (Join-Path $Here "..\..\..\..\..\..")).Path
$AssetRootFile = Join-Path $BookStack "assets_root_win.txt"
$AssetRoot = "C:\Users\jbro8\OneDrive\Desktop\assets-4win\bible-ench.twins+ai]b2"
if (Test-Path -LiteralPath $AssetRootFile) {
    $line = Get-Content -LiteralPath $AssetRootFile | Where-Object { $_ -and $_ -notmatch '^\s*#' } | Select-Object -First 1
    if ($line) { $AssetRoot = $line.Trim() }
}
$RanDir = Join-Path $AssetRoot "bible.ch2en.ran"
if (-not (Test-Path -LiteralPath $RanDir)) {
    Write-Host "MISSING bible assets at $RanDir"
    exit 1
}
Set-Location -LiteralPath $RanDir
$bin = $null
foreach ($c in @("bible_verses.exe","bible_verses.+x","bible_verses")) {
    $p = Join-Path $RanDir $c
    if (Test-Path -LiteralPath $p) { $bin = $p; break }
}
if (-not $bin) {
    # try compile from .c if gcc present
    $src = Join-Path $RanDir "bible_verses.c"
    if ((Test-Path $src) -and (Get-Command gcc -EA SilentlyContinue)) {
        $bin = Join-Path $RanDir "bible_verses.exe"
        & gcc -O2 $src -o $bin 2>$null
    }
}
if (-not $bin -or -not (Test-Path -LiteralPath $bin)) {
    Write-Host "MISSING bible_verses binary in $RanDir (compile bible_verses.c)"
    exit 1
}
$en = Join-Path $RanDir "bible.en_translation.txt"
$ch = Join-Path $RanDir "bible]ch.txt"
$out = & cmd /c "`"$bin`" --short `"$en`" `"$ch`"" 2>$null
if (-not $out) { $out = "(no verse output)" }
# wrap ~70 cols
$wrapped = ($out -join "`n") -split "(.{1,70})(\s+|$)" | Where-Object { $_ -and $_ -notmatch '^\s+$' }
$tmp = Join-Path $env:TEMP ("bible_verse_{0}.txt" -f $PID)
$wrapped -join "`n" | Set-Content -LiteralPath $tmp -Encoding utf8
# show via notepad if khtpm_show_text missing
$show = $null
$houseGuess = Split-Path (Split-Path $BookStack -Parent) -Parent
$showCand = Join-Path $houseGuess "&.widgits\tile-picker\ops\+x\khtpm_show_text.+x"
if (Test-Path -LiteralPath $showCand) {
    $pkg = Join-Path $houseGuess "#.desktop\entities\book-stack"
    & cmd /c "`"$showCand`" `"$pkg`" `"$tmp`""
} else {
    notepad.exe $tmp
    Write-Host $out
}
