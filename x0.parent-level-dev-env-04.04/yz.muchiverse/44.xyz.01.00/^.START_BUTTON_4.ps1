# ^.START_BUTTON_4.ps1 - house root launcher (Windows twin of ^.START_BUTTON_4.sh)
# Launches the START_BUTTON from the house root directory.
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $here
& powershell -ExecutionPolicy Bypass -File (Join-Path $here "_.START_BUTTON\button.ps1") run
