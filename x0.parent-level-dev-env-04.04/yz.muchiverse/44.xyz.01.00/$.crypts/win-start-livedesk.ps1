# win-start-livedesk.ps1 - Win livedesk start.
# Entry point for BOTH the desktop starter (livedesk-start-button.exe) and
# crypt_autostart.exe (Win), so fixing this one file fixes both.
#
# Does NOT call crypt_autostart.exe - that CreateProcess path dropped pals.
#
# REAL FIX 2026-09-26: the taskbar leg below no longer replays
# autostart.pdl's retired rows. The pdl still says
#   tool-bar -> +x/khtpm_strip_parser.+x '.'
# and one +x/tp_desktop_window_rgb.+x row per pal - that is the RETIRED
# stack (see build_khtpm_strip.sh's own 2026-09-01 note: both binaries
# were folded into khtpm_core_render.c and the sources deleted). Launching
# them is what put a dead blank taskbar and dead entities on the desktop.
# The taskbar now goes through the Windows runner, which builds the
# current sources, stops the old stack, and launches detached.
#
# The pdl is NOT edited, deliberately: it is the shared Linux autostart
# contract, and its own comment (2026-08-31) explains that on Linux only
# "tool-bar" still matters because khtpm_taskbar_manager.c's ktb_init()
# calls livedesk_spawn_active_desk() and spawns the real entity list
# itself. Changing the pdl to suit Windows would fork a shared config.
# Instead the retired rows are skipped here, and everything else in the
# pdl still launches exactly as before.
$ErrorActionPreference = "Continue"
$Crypts = Split-Path -Parent $MyInvocation.MyCommand.Path
$House = Split-Path -Parent $Crypts
Set-Location -LiteralPath $House

# Retired binaries. Never launched on Windows, and actively stopped so a
# previously-started instance cannot linger and draw a dead bar.
$Retired = @('tp_desktop_window_rgb', 'khtpm_strip_parser')

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

foreach ($n in @('tp_desktop_window_rgb','khtpm_strip_parser','khtpm_taskbar_manager_main','crypt_autostart','khtpm_core_render','khtpm_entity')) {
    Get-Process -Name $n -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}
Start-Sleep -Milliseconds 400

# ---- the taskbar, via the Windows runner -------------------------------
# 'boot' not 'new': the desktop start button must be snappy, and the
# runner's own boot path is launch-only (it builds only if a binary is
# genuinely missing). Use 'new' from a shell when you want a rebuild.
$Runner = Join-Path $House "_.monads\_.livedesk-taskbar\ops\run_khtpm_strip_win.ps1"
if (Test-Path -LiteralPath $Runner) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $Runner boot | Out-Null
} else {
    Write-Warning "taskbar runner missing: $Runner - starting with NO taskbar"
}
# Give the bars a moment so the pdl replay below cannot race them.
Start-Sleep -Seconds 2

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
       # Skip the retired taskbar/entity rows - the runner above owns the
       # taskbar now, and tp_desktop_window_rgb has no business running.
       $leaf = [System.IO.Path]::GetFileNameWithoutExtension($exeRel)
       if ($Retired -contains $leaf) { return }
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
