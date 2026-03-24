#!/bin/bash
set -e

OS=$(uname -s)
ARCH=$(uname -m)
VERSION="latest"
INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"

case "$OS" in
    Linux)   PLATFORM="ubuntu-latest" ;;
    Darwin)  PLATFORM="macos-latest" ;;
    *)       echo "Unsupported OS: $OS"; exit 1 ;;
esac

URL="https://github.com/deondreE/fastbuild/releases/latest/download/fastbuild-${PLATFORM}.tar.gz"

echo "Installing fastbuild..."
curl -L "$URL" | tar xz -C /tmp
sudo mv /tmp/fastbuild "$INSTALL_DIR/"
sudo chmod +x "$INSTALL_DIR/fastbuild"

echo "fastbuild installed to $INSTALL_DIR/fastbuild"
fastbuild --version
