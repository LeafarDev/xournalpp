#!/usr/bin/env bash
# Bundles Tectonic TeX engine into Xournal++.app for chat LaTeX rendering.
# Downloads a prebuilt binary from GitHub releases.

set -e

APP_PATH="${1:-./Xournal++.app}"
RESOURCES_BIN="$APP_PATH/Contents/Resources/bin"
TECTONIC_DEST="$RESOURCES_BIN/tectonic"
VERSION="0.15.0"
BASE_URL="https://github.com/tectonic-typesetting/tectonic/releases/download/tectonic%40${VERSION}"

if [ ! -d "$APP_PATH/Contents" ]; then
  echo "bundle-tectonic: $APP_PATH does not look like an .app bundle, skipping."
  exit 0
fi

if [ -f "$TECTONIC_DEST" ]; then
  echo "bundle-tectonic: $TECTONIC_DEST already present, skipping."
  exit 0
fi

ARCH="$(uname -m)"
if [ "$ARCH" = "arm64" ]; then
  TARBALL="tectonic-${VERSION}-aarch64-apple-darwin.tar.gz"
elif [ "$ARCH" = "x86_64" ]; then
  TARBALL="tectonic-${VERSION}-x86_64-apple-darwin.tar.gz"
else
  echo "bundle-tectonic: unsupported arch $ARCH, skipping."
  exit 0
fi

TMP_DIR="$(mktemp -d)"
cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT

URL="$BASE_URL/$TARBALL"

echo "bundle-tectonic: downloading $URL"
curl -L "$URL" -o "$TMP_DIR/$TARBALL"

mkdir -p "$TMP_DIR/extract"
tar -xzf "$TMP_DIR/$TARBALL" -C "$TMP_DIR/extract"

TECTONIC_BIN="$(find "$TMP_DIR/extract" -type f -name tectonic -perm -111 | head -n 1)"
if [ -z "$TECTONIC_BIN" ]; then
  echo "bundle-tectonic: tectonic binary not found in archive."
  exit 0
fi

mkdir -p "$RESOURCES_BIN"
cp "$TECTONIC_BIN" "$TECTONIC_DEST"
chmod 755 "$TECTONIC_DEST"

echo "bundle-tectonic: installed Tectonic to $TECTONIC_DEST"
