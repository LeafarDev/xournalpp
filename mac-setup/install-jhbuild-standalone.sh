#!/usr/bin/env bash
#
# Install JHBuild on macOS (standalone, as in readme/JHBuildMacOS.md).
# Run from anywhere. Does not use gtk-osx; for that use CI_jhbuild.sh instead.
#
set -e

echo "=== JHBuild standalone install (macOS) ==="

# 1. Ensure ~/.local/bin exists and is in PATH
mkdir -p "$HOME/.local/bin"
if ! echo ":$PATH:" | grep -q ":${HOME}/.local/bin:"; then
    echo "Add to your ~/.zshrc (or ~/.bash_profile):"
    echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
    echo "Then run: source ~/.zshrc"
    read -p "Continue anyway? [y/N] " -n 1 -r; echo
    [[ $REPLY =~ ^[Yy]$ ]] || exit 1
fi

# 2. Remove existing jhbuild (optional; comment out to keep)
read -p "Remove existing ~/jhbuild and ~/.local/bin/jhbuild? [y/N] " -n 1 -r; echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf "$HOME/jhbuild" "$HOME/.local/bin/jhbuild" "$HOME/.local/share/jhbuild" \
           "$HOME/.cache/jhbuild" "$HOME/.config/jhbuildrc" "$HOME/.jhbuildrc"
fi

# 3. Create and clone
mkdir -p "$HOME/jhbuild"
cd "$HOME/jhbuild"
if [[ ! -d jhbuild/.git ]]; then
    echo "Cloning JHBuild..."
    rm -rf jhbuild
    git clone --depth=1 https://gitlab.gnome.org/GNOME/jhbuild.git
fi
cd jhbuild

# 4. Build and install
if [[ ! -f Makefile ]]; then
    echo "Running autogen.sh --simple-install..."
    if ! command -v autoconf &>/dev/null; then
        echo "Install autotools: brew install autoconf automake libtool pkg-config"
        exit 1
    fi
    ./autogen.sh --simple-install
fi
echo "Running make..."
make
echo "Running make install..."
make install

echo "Done. Ensure ~/.local/bin is in PATH, then run: jhbuild --version"
