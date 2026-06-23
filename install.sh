#!/bin/sh
set -e

REPO="Quantre34/cref"
INSTALL_DIR="${CREF_INSTALL_DIR:-$HOME/.local/bin}"

OS=$(uname -s)
ARCH=$(uname -m)

case "$OS" in
    Darwin) PLATFORM="darwin" ;;
    Linux)  PLATFORM="linux"  ;;
    *)
        echo "Unsupported OS: $OS"
        exit 1
        ;;
esac

case "$ARCH" in
    x86_64)          ARCH_TAG="x86_64" ;;
    arm64 | aarch64) ARCH_TAG="arm64"  ;;
    *)
        echo "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

# macOS x86_64 binary is not released separately — arm64 runs via Rosetta
if [ "$PLATFORM" = "darwin" ] && [ "$ARCH_TAG" = "x86_64" ]; then
    echo "Note: no native Intel binary — downloading arm64 (runs via Rosetta on Intel Macs)"
    ARCH_TAG="arm64"
fi

BINARY="cref-${PLATFORM}-${ARCH_TAG}"
URL="https://github.com/${REPO}/releases/latest/download/${BINARY}"

# Install runtime dependencies
if [ "$PLATFORM" = "darwin" ]; then
    if command -v brew > /dev/null 2>&1; then
        brew install libsodium 2>/dev/null || true
    else
        echo "Warning: Homebrew not found. Install libsodium manually: https://brew.sh"
    fi
elif [ "$PLATFORM" = "linux" ]; then
    if command -v apt-get > /dev/null 2>&1; then
        sudo apt-get install -y libncurses6 libsodium23 2>/dev/null || \
        sudo apt-get install -y libncurses5 libsodium18 2>/dev/null || true
    elif command -v dnf > /dev/null 2>&1; then
        sudo dnf install -y ncurses libsodium 2>/dev/null || true
    elif command -v pacman > /dev/null 2>&1; then
        sudo pacman -S --noconfirm ncurses libsodium 2>/dev/null || true
    fi
fi

mkdir -p "$INSTALL_DIR"

echo "Downloading cref for ${PLATFORM}/${ARCH_TAG}..."
curl -fsSL "$URL" -o "$INSTALL_DIR/cref"
chmod +x "$INSTALL_DIR/cref"

echo "Installed: $INSTALL_DIR/cref"

case ":$PATH:" in
    *":$INSTALL_DIR:"*) ;;
    *)
        echo ""
        echo "Add to your PATH:"
        echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
        ;;
esac
