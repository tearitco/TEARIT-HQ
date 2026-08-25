# open_cli.ps1 - Windows shim for HQ menu "cli" (Linux: open_cli.sh).

param(

    [Parameter(Position = 0)]

    [string]$House = "."

)

$ErrorActionPreference = "Continue"

try { $House = (Resolve-Path -LiteralPath $House).Path } catch {}

$cands = @(

    (Join-Path $House "#.desktop\strip_ascii_frame_history.txt"),

    (Join-Path $House "#.desktop\khtpm_strip_frame_history.txt"),

    (Join-Path $House "#.desktop\strip_history.txt")

)

$frame = $cands | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

if (-not $frame) { $frame = $cands[0] }

$Host.UI.RawUI.WindowTitle = "livedesk-cli"

while ($true) {

    Clear-Host

    Write-Host "=== livedesk cli ===  Ctrl+C to close"

    Write-Host $House

    Write-Host $frame

    Write-Host ""

    if (Test-Path -LiteralPath $frame) {

        Get-Content -LiteralPath $frame -Tail 50 -ErrorAction SilentlyContinue

    } else {

        Write-Host "(no ascii frame yet)"

    }

    Start-Sleep -Milliseconds 400

}

