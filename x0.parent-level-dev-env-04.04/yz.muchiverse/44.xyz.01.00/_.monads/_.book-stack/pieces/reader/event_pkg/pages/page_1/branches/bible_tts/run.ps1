# bible_tts branch (Windows) — same assets as bible_text + optional TTS
$ErrorActionPreference = "Continue"
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Here "..\bible_text\run.ps1")
# TTS: edge-tts if available (optional)
Write-Host "(TTS optional on Win — install edge-tts / use system narrator if desired)"
