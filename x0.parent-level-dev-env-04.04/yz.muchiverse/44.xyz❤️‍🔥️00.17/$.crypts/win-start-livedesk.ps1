# win-start-livedesk.ps1 — Win livedesk start (same as working Start-Process).
# Does NOT call crypt_autostart.exe (that CreateProcess path dropped pals).
# Desktop starter and crypt_autostart.exe (Win) exec this file.
$ErrorActionPreference = "Continue"
$Crypts = Split-Path -Parent $MyInvocation.MyCommand.Path
$House = Split-Path -Parent $Crypts
Set-Location -LiteralPath $House

function ConvertTo-WinHousePath([string]$Path) {
    if ([string]::IsNullOrEmpty($Path)) { return $Path }
    $parts = $Path -split '[\\/]+'
    $aliased = foreach ($p in $parts) {
        if ($p.Length -ge 2 -and $p[0] -eq [char]'*' -and $p[1] -eq [char]'.') {
            '_' + $p.Substring(1)
        } else { $p }
    }
    $arr = @($aliased)
    if ($arr[0] -match '^[A-Za-z]:$') {
        if ($arr.Count -eq 1) { return ($arr[0] + '\') }
        return ($arr[0] + '\' + ($arr[1..($arr.Count - 1)] -join '\'))
    }
    return ($arr -join '\')
}

function Convert-LaunchPath([string]$Tok) {
    $t = $Tok.Trim().Trim("'").Trim('"')
    if ($t -eq '.' -or $t -eq '') { return $t }
    $ix = $t.IndexOf('xyzfs/')
    if ($ix -lt 0) { $ix = $t.IndexOf('xyzfs\') }
    if ($ix -ge 0) { $t = $t.Substring($ix) }
    $t = ConvertTo-WinHousePath $t
    if ($t.EndsWith('.+x')) { $t = $t.Substring(0, $t.Length - 3) + '.exe' }
    return $t
}

foreach ($n in @('tp_desktop_window_rgb','khtpm_strip_parser','khtpm_taskbar_manager_main','crypt_autostart')) {
    Get-Process -Name $n -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}
Start-Sleep -Milliseconds 400

$pdl = Join-Path $Crypts "autostart.pdl"
if (-not (Test-Path -LiteralPath $pdl)) { exit 1 }

Get-Content -LiteralPath $pdl | ForEach-Object {
    $line = $_
    if ($line -notmatch '^\s*LAUNCH') { return }
    $parts = $line -split '\|', 3
    if ($parts.Count -lt 3) { return }
    $val = $parts[2].Trim()
    $toks = [regex]::Matches($val, "'([^']*)'") | ForEach-Object { $_.Groups[1].Value }
    if (-not $toks -or $toks.Count -lt 1) { return }
    $exeRel = Convert-LaunchPath $toks[0]
    $exe = $exeRel
    if (-not [System.IO.Path]::IsPathRooted($exe)) {
        $exe = Join-Path $House $exeRel
    }
    if (-not (Test-Path -LiteralPath $exe)) { return }
    $arglist = @()
    for ($i = 1; $i -lt $toks.Count; $i++) {
        $a = Convert-LaunchPath $toks[$i]
        if ($a -eq '.') { $a = '.' }
        $arglist += $a
    }
    if ($arglist.Count -gt 0) {
        Start-Process -FilePath $exe -ArgumentList $arglist -WorkingDirectory $House
    } else {
        Start-Process -FilePath $exe -WorkingDirectory $House
    }
    Start-Sleep -Milliseconds 200
}
exit 0
