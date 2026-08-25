#!/bin/bash
# install_deps.sh - Cross-platform dependency installer for CHTPM
# Supports: macOS (Homebrew), Linux (apt/dnf), Windows (MSYS2/Git Bash)

echo "=== CHTPM Dependency Installer ==="
echo "Detecting OS..."

OS_TYPE="$(uname -s)"
case "$OS_TYPE" in
    Darwin)
        echo "Platform: macOS"
        if ! command -v brew >/dev/null 2>&1; then
            echo "Error: Homebrew not found. Please install it from https://brew.sh/"
            exit 1
        fi
        echo "Installing macOS dependencies..."
        brew install freetype pkg-config openssl
        ;;
    Linux)
        echo "Platform: Linux"
        if command -v apt-get >/dev/null 2>&1; then
            echo "Detected Debian/Ubuntu (apt)..."
            sudo apt-get update
            sudo apt-get install -y build-essential libglut3-dev libglu1-mesa-dev libgl1-mesa-dev libfreetype6-dev pkg-config libssl-dev
        elif command -v dnf >/dev/null 2>&1; then
            echo "Detected Fedora/RHEL (dnf)..."
            sudo dnf install -y freeglut-devel mesa-libGLU-devel freetype-devel pkgconf-pkg-config openssl-devel
        else
            echo "Warning: Unknown Linux distribution. Please install GLUT, GLU, FreeType, and OpenSSL development headers manually."
        fi
        ;;
    MSYS*|MINGW*|CYGWIN*)
        echo "Platform: Windows (MSYS2/MinGW/Git Bash)"
        if command -v pacman >/dev/null 2>&1; then
            echo "Installing MSYS2/MinGW dependencies..."
            pacman -S --noconfirm --needed \
                mingw-w64-x86_64-gcc \
                mingw-w64-x86_64-freeglut \
                mingw-w64-x86_64-freetype \
                mingw-w64-x86_64-pkg-config \
                mingw-w64-x86_64-openssl
        else
            echo "Error: pacman not found. If on Windows, MSYS2 is the recommended environment for this project."
            exit 1
        fi
        ;;
    *)
        echo "Error: Unsupported OS type: $OS_TYPE"
        exit 1
        ;;
esac

echo ""
echo "=== Setup Complete ==="
echo "You can now compile the project using: ./button.sh compile"
