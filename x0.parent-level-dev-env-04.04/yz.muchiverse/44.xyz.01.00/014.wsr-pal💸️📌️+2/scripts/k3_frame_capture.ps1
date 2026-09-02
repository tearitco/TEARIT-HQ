# k3_frame_capture.ps1 - K3-style frame + RGB receipt capture for wsr-pal
# Mirrors house _.0.aigent-testing-k3.txt methodology for Windows.
#
# Usage (from project root):
#   powershell -ExecutionPolicy Bypass -File scripts\k3_frame_capture.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\k3_frame_capture.ps1 -Topic gl-empty -InjectEnter
#   powershell -ExecutionPolicy Bypass -File scripts\k3_frame_capture.ps1 -Topic key-smoke -KeyCodes 49,13
#
# Evidence paths (wsr-pal):
#   pieces/display/current_frame.txt
#   pieces/display/frame_history.txt          (renderer audit log)
#   pieces/display/rgb_frame.receipt.txt      (upstream RGB writer)
#   pieces/display/gl_display.receipt.txt     (downstream GL upload)
#   pieces/display/rgb_frame.raw              (pixel buffer)
#   pieces/keyboard/history.txt               (KEY_PRESSED injection target)
#
# Root report: FRAME_REPORT_<yyyyMMdd-HHmm>_<topic>.txt

param(
    [string]$Topic = "smoke",
    [int[]]$KeyCodes = @(),
    [switch]$InjectEnter,
    [int]$WaitSeconds = 2
)

$ErrorActionPreference = "Continue"
$ROOT = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $ROOT

if ($InjectEnter) { $KeyCodes = @(13) }

$stamp = Get-Date -Format "yyyyMMdd-HHmm"
$reportName = "FRAME_REPORT_${stamp}_${Topic}.txt"
$reportPath = Join-Path $ROOT $reportName
$rawHistCopy = Join-Path $ROOT "FRAME_HISTORY_RAW_${stamp}.txt"

function Get-SafeContent([string]$rel, [int]$maxLines = 0) {
    $p = Join-Path $ROOT $rel
    if (-not (Test-Path $p)) { return @("(MISSING: $rel)") }
    if ($maxLines -gt 0) { return Get-Content $p -TotalCount $maxLines }
    return Get-Content $p
}

function Get-FileMeta([string]$rel) {
    $p = Join-Path $ROOT $rel
    if (-not (Test-Path $p)) { return "MISS  $rel" }
    $i = Get-Item $p
    return ("OK    {0,-45} size={1,8} mtime={2}" -f $rel, $i.Length, $i.LastWriteTime)
}

function Inject-Key([int]$code) {
    $kb = Join-Path $ROOT "pieces\keyboard\history.txt"
    $ts = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    # K3 exact format
    $line = "[$ts] KEY_PRESSED: $code"
    Add-Content -Path $kb -Value $line
    return $line
}

function Get-Kv([string[]]$text, [string]$key) {
    foreach ($l in $text) {
        if ($l -like "$key=*") { return ($l -split "=", 2)[1] }
    }
    return ""
}

# --- baseline ---
$beforeFrame = Get-SafeContent "pieces\display\current_frame.txt"
$beforeRgb = Get-SafeContent "pieces\display\rgb_frame.receipt.txt"
$beforeGl = Get-SafeContent "pieces\display\gl_display.receipt.txt"

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("FRAME_REPORT $stamp topic=$Topic")
$lines.Add("Project: 014.wsr-pal (Windows K3 capture)")
$lines.Add("Guide:   house _.0.aigent-testing-k3.txt")
$lines.Add("CWD:     $ROOT")
$lines.Add("")
$lines.Add("=== R1 process health ===")
# Do not touch Process.Path (hangs on some Windows hosts)
$procs = Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -match "orchestrator|chtpm|renderer|keyboard|gl_mirror|prisc"
}
if ($procs) {
    foreach ($p in $procs) {
        $lines.Add(("  LIVE  pid={0} name={1}" -f $p.Id, $p.ProcessName))
    }
} else {
    $lines.Add("  FAIL  no wsr-related processes found")
}

$lines.Add("")
$lines.Add("=== artifact inventory ===")
@(
    "pieces\display\current_frame.txt",
    "pieces\display\current_layout.txt",
    "pieces\display\frame_history.txt",
    "pieces\display\frame_changed.txt",
    "pieces\display\renderer_pulse.txt",
    "pieces\display\rgb_frame.raw",
    "pieces\display\rgb_frame.receipt.txt",
    "pieces\display\rgb_frame_changed.txt",
    "pieces\display\gl_display.receipt.txt",
    "pieces\keyboard\history.txt",
    "pieces\apps\player_app\history.txt",
    "pieces\apps\player_app\interact_relay.txt",
    "debug.txt"
) | ForEach-Object { $lines.Add("  " + (Get-FileMeta $_)) }

$lines.Add("")
$lines.Add("=== R3 layout ===")
$layout = (Get-SafeContent "pieces\display\current_layout.txt" | Out-String).Trim()
$lines.Add("  $layout")

$lines.Add("")
$lines.Add("=== BEFORE current_frame (head) ===")
$beforeFrame | Select-Object -First 20 | ForEach-Object { $lines.Add($_) }

$lines.Add("")
$lines.Add("=== BEFORE rgb_frame.receipt ===")
$beforeRgb | ForEach-Object { $lines.Add($_) }

$lines.Add("")
$lines.Add("=== BEFORE gl_display.receipt ===")
$beforeGl | ForEach-Object { $lines.Add($_) }

$rgbCs = Get-Kv $beforeRgb "checksum_fnv1a64"
$glCs = Get-Kv $beforeGl "loaded_rgba_checksum_fnv1a64"
$lines.Add("")
$lines.Add("=== RGB vs GL checksum gate ===")
$lines.Add("  rgb checksum_fnv1a64          = $rgbCs")
$lines.Add("  gl  loaded_rgba_checksum      = $glCs")
if ($rgbCs -and $glCs) {
    $normGl = $glCs.ToLower().Replace("0x", "")
    $normRgb = $rgbCs.ToLower().Replace("0x", "")
    if ($normGl -eq $normRgb) {
        $lines.Add("  VERDICT: PASS (GL uploaded same buffer rgb_render wrote)")
    } else {
        $lines.Add("  VERDICT: FAIL (GL texture checksum != RGB receipt - stale/empty mirror)")
    }
} else {
    $lines.Add("  VERDICT: INCONCLUSIVE (missing receipt field)")
}

# --- optional key injection ---
$injected = @()
if ($KeyCodes.Count -gt 0) {
    $lines.Add("")
    $lines.Add("=== R2 key injection ===")
    foreach ($k in $KeyCodes) {
        $inj = Inject-Key $k
        $injected += $inj
        $lines.Add("  injected: $inj")
    }
    Start-Sleep -Seconds $WaitSeconds
    $lines.Add("  waited ${WaitSeconds}s for parser/module cycle")
}

$afterFrame = Get-SafeContent "pieces\display\current_frame.txt"
$afterRgb = Get-SafeContent "pieces\display\rgb_frame.receipt.txt"
$afterGl = Get-SafeContent "pieces\display\gl_display.receipt.txt"

$lines.Add("")
$lines.Add("=== AFTER current_frame (head) ===")
$afterFrame | Select-Object -First 20 | ForEach-Object { $lines.Add($_) }

$lines.Add("")
$lines.Add("=== AFTER rgb_frame.receipt ===")
$afterRgb | ForEach-Object { $lines.Add($_) }

$lines.Add("")
$lines.Add("=== AFTER gl_display.receipt ===")
$afterGl | ForEach-Object { $lines.Add($_) }

$frameChanged = (($beforeFrame -join "`n") -ne ($afterFrame -join "`n"))
$lines.Add("")
$lines.Add("=== frame delta ===")
$lines.Add("  current_frame changed: $frameChanged")

$rawPath = Join-Path $ROOT "pieces\display\rgb_frame.raw"
if (Test-Path $rawPath) {
    $bytes = [System.IO.File]::ReadAllBytes($rawPath)
    $white = 0; $black = 0; $px = [int]($bytes.Length / 4)
    for ($i = 0; $i -lt $bytes.Length; $i += 4) {
        $lum = ([int]$bytes[$i] + [int]$bytes[$i + 1] + [int]$bytes[$i + 2]) / 3
        if ($lum -ge 200) { $white++ }
        elseif ($lum -eq 0) { $black++ }
    }
    $lines.Add("")
    $lines.Add("=== rgb_frame.raw pixel gate ===")
    $lines.Add("  bytes=$($bytes.Length) pixels=$px black=$black whiteish=$white")
    if ($white -gt 100) {
        $lines.Add("  VERDICT: PASS (visible text pixels present in RGB buffer)")
    } elseif ($px -gt 0 -and $black -eq $px) {
        $lines.Add("  VERDICT: FAIL (RGB buffer entirely black - glyph/render empty)")
    } else {
        $lines.Add("  VERDICT: INCONCLUSIVE (sparse/nonstandard buffer)")
    }
}

$lines.Add("")
$lines.Add("=== keyboard history tail ===")
Get-SafeContent "pieces\keyboard\history.txt" | Select-Object -Last 8 | ForEach-Object { $lines.Add("  $_") }

$lines.Add("")
$lines.Add("=== frame_history tail (renderer audit) ===")
Get-SafeContent "pieces\display\frame_history.txt" | Select-Object -Last 15 | ForEach-Object { $lines.Add($_) }

$lines.Add("")
$lines.Add("=== verdicts ===")
$alive = [bool]$procs
$frameOk = (Test-Path (Join-Path $ROOT "pieces\display\current_frame.txt")) -and ((Get-Item (Join-Path $ROOT "pieces\display\current_frame.txt")).Length -gt 100)
$lines.Add("  R1 stack live:           $(if ($alive) {'PASS'} else {'FAIL'})")
$lines.Add("  R3 frame non-empty:      $(if ($frameOk) {'PASS'} else {'FAIL'})")
$lines.Add("  RGB/GL checksum match:   (see gate above)")
if ($injected.Count) {
    $lines.Add("  Key injection performed: YES")
    foreach ($inj in $injected) { $lines.Add("    $inj") }
} else {
    $lines.Add("  Key injection performed: NO")
}
$lines.Add("")
$lines.Add("Safe to proceed with code changes: REVIEW THIS REPORT FIRST (K3 gate).")
$lines.Add("End of FRAME_REPORT.")

$lines | Set-Content -Path $reportPath -Encoding UTF8
$fh = Join-Path $ROOT "pieces\display\frame_history.txt"
if (Test-Path $fh) { Copy-Item $fh $rawHistCopy -Force }

Write-Host "Wrote $reportName" -ForegroundColor Green
Write-Host ("  raw history: {0}" -f (Split-Path $rawHistCopy -Leaf))
Write-Host ""
Get-Content $reportPath
