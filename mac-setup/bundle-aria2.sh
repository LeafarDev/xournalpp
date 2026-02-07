#!/usr/bin/env bash
# Puts a copy of aria2c into Xournal++.app so end users don't need to install it.
# Run from mac-setup/ with Xournal++.app in the current directory (or pass path).

set -e

APP_PATH="${1:-./Xournal++.app}"
RESOURCES_BIN="$APP_PATH/Contents/Resources/bin"
ARIA2_DEST="$RESOURCES_BIN/aria2c"

if [ ! -d "$APP_PATH/Contents" ]; then
  echo "bundle-aria2: $APP_PATH does not look like an .app bundle, skipping."
  exit 0
fi

if [ -f "$ARIA2_DEST" ]; then
  echo "bundle-aria2: $ARIA2_DEST already present, skipping."
  exit 0
fi

ARIA2_SRC=""
if command -v aria2c >/dev/null 2>&1; then
  ARIA2_SRC="$(command -v aria2c)"
elif [ -x /opt/homebrew/bin/aria2c ]; then
  ARIA2_SRC="/opt/homebrew/bin/aria2c"
elif [ -x /usr/local/bin/aria2c ]; then
  ARIA2_SRC="/usr/local/bin/aria2c"
fi

if [ -z "$ARIA2_SRC" ]; then
  echo "bundle-aria2: aria2c not found (install with: brew install aria2). Skipping; app will use curl if available."
  exit 0
fi

mkdir -p "$RESOURCES_BIN"
cp "$ARIA2_SRC" "$ARIA2_DEST"
chmod 755 "$ARIA2_DEST"
echo "bundle-aria2: copied aria2c to $ARIA2_DEST"
