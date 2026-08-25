# add_to_path.ps1 - Permanently add MSYS2/MinGW to your User PATH
# This script updates both the Windows registry (permanent) and the current session.

$newPath = "C:\msys64\mingw64\bin"

Write-Host "Checking if MSYS2 is at $newPath..." -ForegroundColor Gray
if (-not (Test-Path $newPath)) {
    Write-Warning "Warning: $newPath does not exist yet! Make sure you installed MSYS2."
}

# 1. Get the current permanent User PATH
$oldPath = [Environment]::GetEnvironmentVariable("Path", "User")

# 2. Check if the path is already there to avoid duplicates
if ($oldPath -like "*$newPath*") {
    Write-Host "Success: $newPath is already in your permanent PATH." -ForegroundColor Green
} else {
    Write-Host "Adding $newPath to permanent User PATH..." -ForegroundColor Cyan
    $updatedPath = "$oldPath;$newPath"
    [Environment]::SetEnvironmentVariable("Path", $updatedPath, "User")
    Write-Host "Permanent PATH updated successfully." -ForegroundColor Green
}

# 3. Update the current session immediately
if ($env:Path -notlike "*$newPath*") {
    $env:Path += ";$newPath"
    Write-Host "Current session PATH updated." -ForegroundColor Cyan
}

# 4. Verification
Write-Host "`nVerifying GCC installation:" -ForegroundColor White
gcc --version

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nSUCCESS! You can now run .\compile_all.ps1" -ForegroundColor Green
} else {
    Write-Warning "`nGCC not found. Make sure you ran the 'pacman' command inside the MSYS2 terminal first!"
}
