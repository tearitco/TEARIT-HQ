# install_deps.ps1 - Install CHTPM dependencies on Windows
# This script uses 'winget' to install MSYS2, which provides GCC and libraries.

Write-Host "=== CHTPM Windows Dependency Installer ===" -ForegroundColor Cyan

# 1. Check for winget
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Error "winget not found. Please install Windows Package Manager or install MSYS2 manually from https://www.msys2.org/"
    exit 1
}

Write-Host "Installing MSYS2 via winget..." -ForegroundColor Gray
winget install -e --id MSYS2.MSYS2 --accept-source-agreements --accept-package-agreements

if ($LASTEXITCODE -ne 0) {
    Write-Warning "winget installation might have failed or MSYS2 is already installed."
}

Write-Host ""
Write-Host "--- NEXT STEPS (Manual Action Required) ---" -ForegroundColor Yellow
Write-Host "1. Open 'MSYS2 MinGW 64-bit' from your Start Menu."
Write-Host "2. Run the following command to install GCC and all required libraries:" -ForegroundColor White
Write-Host "   pacman -S --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-freeglut mingw-w64-x86_64-freetype mingw-w64-x86_64-openssl" -ForegroundColor Green
Write-Host ""
Write-Host "3. After the installation, add the MSYS2 bin directory to your Windows PATH:" -ForegroundColor White
Write-Host "   C:\msys64\mingw64\bin" -ForegroundColor Green
Write-Host ""
Write-Host "4. Restart your terminal (PowerShell) and run '.\compile_all.ps1' again."
Write-Host "--------------------------------------------"
