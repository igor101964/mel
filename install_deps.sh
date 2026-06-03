#!/bin/bash
# install_deps.sh — Install mel build dependencies
# Art2Dec SoftLab · mshell Ecosystem
# Supports: Ubuntu 22.04/24.04 x86_64, Debian 12/13 ARM64, macOS Sequoia (Intel & Apple Silicon)

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()    { echo -e "${CYAN}[mel]${NC} $1"; }
ok()      { echo -e "${GREEN}[ok]${NC} $1"; }
warn()    { echo -e "${YELLOW}[warn]${NC} $1"; }
die()     { echo -e "${RED}[error]${NC} $1"; exit 1; }

echo ""
echo "  mel — Terminal Editor · Art2Dec SoftLab"
echo "  Dependency installer"
echo "  ────────────────────────────────────────"
echo ""

# ── Detect OS ──────────────────────────────────────────────────────────────
detect_os() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
        ARCH=$(uname -m)   # x86_64 or arm64
        return
    fi

    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        OS_ID="${ID}"
        OS_VER="${VERSION_ID}"
        ARCH=$(uname -m)

        case "$OS_ID" in
            ubuntu)  OS="ubuntu"  ;;
            debian)  OS="debian"  ;;
            raspbian) OS="debian" ;;  # Raspberry Pi OS is Debian-based
            *)
                warn "Unknown Linux distro: $OS_ID. Trying apt-get..."
                OS="debian"
                ;;
        esac
    else
        die "Cannot detect OS. /etc/os-release not found."
    fi
}

detect_os

info "Detected: OS=${OS} ARCH=${ARCH} VER=${OS_VER:-n/a}"
echo ""

# ── Linux: Ubuntu / Debian ─────────────────────────────────────────────────
install_linux() {
    # Check apt-get available
    if ! command -v apt-get &>/dev/null; then
        die "apt-get not found. This script supports Ubuntu and Debian only on Linux."
    fi

    info "Updating package lists..."
    sudo apt-get update -q

    info "Installing build tools..."
    sudo apt-get install -y \
        build-essential \
        gcc \
        make

    info "Installing mel dependencies..."
    sudo apt-get install -y \
        libcurl4-openssl-dev \
        libjson-c-dev \
        xclip \
        fonts-noto-color-emoji

    # Verify
    echo ""
    info "Verifying installed packages..."
    dpkg -l libcurl4-openssl-dev libjson-c-dev build-essential xclip fonts-noto-color-emoji 2>/dev/null \
        | grep "^ii" | awk '{print "  "$2" "$3}' \
        || warn "Could not verify — check manually"
}

# ── macOS: Homebrew ────────────────────────────────────────────────────────
install_macos() {
    # Xcode Command Line Tools
    if ! xcode-select -p &>/dev/null; then
        info "Installing Xcode Command Line Tools..."
        xcode-select --install
        echo ""
        warn "Please complete Xcode CLT installation, then re-run this script."
        exit 0
    else
        ok "Xcode Command Line Tools: already installed"
    fi

    # Homebrew
    if ! command -v brew &>/dev/null; then
        info "Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

        # Add brew to PATH for Apple Silicon
        if [[ "$ARCH" == "arm64" ]]; then
            eval "$(/opt/homebrew/bin/brew shellenv)"
        fi
    else
        ok "Homebrew: already installed ($(brew --version | head -1))"
    fi

    info "Installing mel dependencies via Homebrew..."
    brew install curl json-c

    # xclip не нужен на macOS — pbpaste встроен
    ok "Clipboard: pbpaste is built-in on macOS"
    # Emoji шрифты встроены в macOS
    ok "Emoji fonts: built-in on macOS"

    # Verify
    echo ""
    info "Verifying installed packages..."
    for pkg in curl json-c; do
        if brew list "$pkg" &>/dev/null; then
            ok "$pkg: $(brew info "$pkg" | head -1)"
        else
            warn "$pkg: not found after install — check brew output above"
        fi
    done
}

# ── Dispatch ───────────────────────────────────────────────────────────────
case "$OS" in
    ubuntu|debian)
        info "Platform: Linux ${OS} ${OS_VER} ${ARCH}"
        case "$ARCH" in
            x86_64)  info "Target: Ubuntu/Debian x86_64" ;;
            aarch64) info "Target: Debian ARM64 (Raspberry Pi 4/5)" ;;
            *)        warn "Unsupported arch: $ARCH — trying anyway" ;;
        esac
        install_linux
        ;;
    macos)
        case "$ARCH" in
            x86_64) info "Platform: macOS Intel x86_64" ;;
            arm64)  info "Platform: macOS Apple Silicon (M1/M2/M3/M4)" ;;
            *)       warn "Unknown macOS arch: $ARCH" ;;
        esac
        install_macos
        ;;
    *)
        die "Unsupported OS: $OS"
        ;;
esac

# ── Build check ────────────────────────────────────────────────────────────
echo ""
echo "  ────────────────────────────────────────"
ok "Dependencies installed successfully."
echo ""
info "To build mel, run:"
echo "    make"
echo ""
info "Or manually:"
echo "    gcc mel.c -o mel -std=c99 -lcurl -ljson-c"
echo ""
