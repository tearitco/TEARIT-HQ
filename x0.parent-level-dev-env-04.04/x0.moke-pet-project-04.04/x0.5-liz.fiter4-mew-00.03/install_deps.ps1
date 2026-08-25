# install_deps.ps1 - Windows dependency installer for CHTPM
# This script installs MSYS2 and the required development libraries (GCC, GLUT, FreeType, etc.)

Write-Host "=== CHTPM Windows Dependency Installer ===" -ForegroundColor Cyan

# 1. Check for MSYS2
$msysPath = "C:\msys64"
$msysInstalled = Test-Path $msysPath

if (-not $msysInstalled) {
    Write-Host "MSYS2 not found at $msysPath. Attempting to install via winget..." -ForegroundColor Gray
    
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        Write-Error "winget not found. Please install MSYS2 manually from https://www.msys2.org/ and restart this script."
        exit 1
    }

    winget install -e --id MSYS2.MSYS2 --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "winget installation might have failed. Please check if MSYS2 is installed."
    } else {
        Write-Host "MSYS2 installed successfully." -ForegroundColor Green
        $msysInstalled = $true
    }
} else {
    Write-Host "MSYS2 found at $msysPath." -ForegroundColor Green
}

# 2. Install dependencies via pacman
if (Test-Path "$msysPath\usr\bin\bash.exe") {
    Write-Host "Installing/Updating dependencies via pacman..." -ForegroundColor Cyan
    
    $pacmanCmd = "pacman -S --noconfirm --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-freeglut mingw-w64-x86_64-freetype mingw-w64-x86_64-pkg-config mingw-w64-x86_64-openssl"
    
    # Run pacman inside the MSYS2 environment
    Start-Process -FilePath "$msysPath\usr\bin\bash.exe" -ArgumentList "-lc", "'$pacmanCmd'" -Wait
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Dependencies installed successfully." -ForegroundColor Green
    } else {
        Write-Warning "pacman command might have failed. You may need to run it manually in an MSYS2 terminal."
    }
} else {
    Write-Warning "MSYS2 bash not found. Please run 'pacman' manually in the MSYS2 terminal:"
    Write-Host "pacman -S --noconfirm --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-freeglut mingw-w64-x86_64-freetype mingw-w64-x86_64-pkg-config mingw-w64-x86_64-openssl" -ForegroundColor Gray
}

# 3. Handle PATH
$binPath = "$msysPath\mingw64\bin"
if ($env:Path -notlike "*$binPath*") {
    Write-Host "`nWould you like to add $binPath to your User PATH? (y/n)" -ForegroundColor Yellow
    $choice = Read-Host
    if ($choice -eq 'y') {
        $oldPath = [Environment]::GetEnvironmentVariable("Path", "User")
        if ($oldPath -notlike "*$binPath*") {
            $updatedPath = "$oldPath;$binPath"
            [Environment]::SetEnvironmentVariable("Path", $updatedPath, "User")
            Write-Host "Permanent User PATH updated. Please RESTART your terminal for changes to take effect." -ForegroundColor Green
        } else {
            Write-Host "Path already in User environment variables." -ForegroundColor Gray
        }
        # Update current session too
        $env:Path += ";$binPath"
        Write-Host "Current session PATH updated." -ForegroundColor Gray
    }
} else {
    Write-Host "MSYS2 MinGW64 bin is already in PATH." -ForegroundColor Green
}

Write-Host "`n=== Setup Complete ===" -ForegroundColor Cyan
Write-Host "You can now compile the project using: .\button.ps1 compile"
