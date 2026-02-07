#!/usr/bin/env bash
# Puts the Copilot CLI into Xournal++.app so end users don't need to install it.
# 1) If 'copilot' is on PATH or in Homebrew, copies it.
# 2) Otherwise downloads the official binary from GitHub releases and extracts it.
# Run from mac-setup/ with Xournal++.app in the current directory (or pass path).

set -e

APP_PATH="${1:-./Xournal++.app}"
RESOURCES_BIN="$APP_PATH/Contents/Resources/bin"
COPILOT_DEST="$RESOURCES_BIN/copilot"
REPO="github/copilot-cli"

is_valid_copilot() {
  local bin="$1"
  [ -x "$bin" ] || return 1
  # Avoid VS Code Copilot Chat shim (not a real Copilot CLI)
  if head -n 2 "$bin" | grep -qi "copilotCLIShim"; then
    return 1
  fi
  if head -n 2 "$bin" | grep -qi "Code Helper (Plugin)"; then
    return 1
  fi
  local ver
  ver=$($bin --version 2>&1 | head -n 2)
  if echo "$ver" | grep -qi "Cannot find GitHub Copilot CLI"; then
    return 1
  fi
  if echo "$ver" | grep -qi "Install GitHub Copilot CLI"; then
    return 1
  fi
  return 0
}

if [ ! -d "$APP_PATH/Contents" ]; then
  echo "bundle-copilot: $APP_PATH does not look like an .app bundle, skipping."
  exit 0
fi

if [ -f "$COPILOT_DEST" ]; then
  if is_valid_copilot "$COPILOT_DEST"; then
    echo "bundle-copilot: $COPILOT_DEST already present, skipping."
    exit 0
  fi
  echo "bundle-copilot: existing copilot is invalid, replacing."
  rm -f "$COPILOT_DEST"
fi

# Prefer copying from system if available
COPILOT_SRC=""
if command -v copilot >/dev/null 2>&1; then
  COPILOT_SRC="$(command -v copilot)"
elif [ -x /opt/homebrew/bin/copilot ]; then
  COPILOT_SRC="/opt/homebrew/bin/copilot"
elif [ -x /usr/local/bin/copilot ]; then
  COPILOT_SRC="/usr/local/bin/copilot"
fi

if [ -n "$COPILOT_SRC" ] && is_valid_copilot "$COPILOT_SRC"; then
  mkdir -p "$RESOURCES_BIN"
  cp "$COPILOT_SRC" "$COPILOT_DEST"
  chmod 755 "$COPILOT_DEST"
  echo "bundle-copilot: copied copilot from $COPILOT_SRC to $COPILOT_DEST"
  exit 0
fi

# Download from GitHub releases (macOS only for .app; arch from uname)
OS=$(uname -s)
ARCH=$(uname -m)
case "$OS" in
  Darwin)
    if [ "$ARCH" = "arm64" ]; then
      ASSET="copilot-darwin-arm64.tar.gz"
    else
      ASSET="copilot-darwin-x64.tar.gz"
    fi
    ;;
  Linux)
    if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
      ASSET="copilot-linux-arm64.tar.gz"
    else
      ASSET="copilot-linux-x64.tar.gz"
    fi
    ;;
  *)
    echo "bundle-copilot: unsupported OS $OS, skipping download."
    exit 0
    ;;
esac

echo "bundle-copilot: downloading Copilot CLI from GitHub ($ASSET)..."

# Resolve latest release tag (no jq: use sed)
TAG=$(curl -sSfL "https://api.github.com/repos/$REPO/releases/latest" | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p')
if [ -z "$TAG" ]; then
  echo "bundle-copilot: could not get latest release tag, skipping."
  exit 0
fi

URL="https://github.com/$REPO/releases/download/$TAG/$ASSET"
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

if ! curl -sSfL -o "$TMPDIR/asset.tar.gz" "$URL"; then
  echo "bundle-copilot: download failed ($URL), skipping."
  exit 0
fi

mkdir -p "$TMPDIR/extract"
tar xzf "$TMPDIR/asset.tar.gz" -C "$TMPDIR/extract"

# Tarball may contain ./copilot or ./bin/copilot or similar
BIN=$(find "$TMPDIR/extract" -name "copilot" -type f 2>/dev/null | head -1)
if [ -z "$BIN" ] || [ ! -x "$BIN" ]; then
  echo "bundle-copilot: no copilot binary in tarball, skipping."
  exit 0
fi

mkdir -p "$RESOURCES_BIN"
cp "$BIN" "$COPILOT_DEST"
chmod 755 "$COPILOT_DEST"
echo "bundle-copilot: installed Copilot CLI to $COPILOT_DEST (from $TAG)"
