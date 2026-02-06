#!/usr/bin/env bash

set -e
set -o pipefail

# Build with current code, remove existing app from /Applications, install new .app
if [ $# -eq 0 ]; then
  echo "Usage: $0 <gtk prefix parent>"
  echo "Example: $0 ~/gtk"
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
APP_NAME="Xournal++.app"
APPLICATIONS_APP="/Applications/$APP_NAME"
prefix_parent="$1"

[ ! -d "$prefix_parent" ] && echo "$prefix_parent doesn't exist." && exit 1
[ ! -d "$prefix_parent"/inst ] && echo "$prefix_parent/inst doesn't exist." && exit 1

ENSURE_JHBUILD=
if [ -z "$UNDER_JHBUILD" ]; then
    if ! which jhbuild > /dev/null; then
        echo "Not running jhbuild environment..."
        echo "Warning: jhbuild not found on PATH! This script may not work as expected."
    else
        echo "Not running jhbuild environment, prepending jhbuild to every command"
        ENSURE_JHBUILD="jhbuild run"
    fi
fi

echo "=== Build com código atualizado ==="
cd "$ROOT_DIR"

build_dir="$ROOT_DIR"/build
mkdir -p "$build_dir"

pushd "$build_dir"
$ENSURE_JHBUILD cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
$ENSURE_JHBUILD cmake --build .
$ENSURE_JHBUILD cmake --install . --prefix "$prefix_parent"/inst
popd

echo "=== Criando .app bundle ==="
cd "$SCRIPT_DIR"
$ENSURE_JHBUILD bash "$SCRIPT_DIR/build-app.sh" "$prefix_parent"

echo ""
echo "=== Removendo $APPLICATIONS_APP ==="
rm -rf "$APPLICATIONS_APP"

echo "=== Copiando novo executável para /Applications ==="
cp -r "$SCRIPT_DIR/$APP_NAME" /Applications/

echo ""
echo "Pronto. $APP_NAME instalado em /Applications."
