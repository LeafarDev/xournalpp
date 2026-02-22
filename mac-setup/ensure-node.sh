#!/usr/bin/env bash
# Ensures Node is on PATH: uses system node/npm if present, else downloads Node to build/.node-bin.
# Must be sourced: . mac-setup/ensure-node.sh   then npm/node are available.
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
NODE_CACHE="$BUILD_DIR/.node-bin"
NODE_VERSION="v20.18.0"

if command -v node >/dev/null 2>&1 && command -v npm >/dev/null 2>&1; then
  :
elif [ -x "$NODE_CACHE/bin/node" ] && [ -x "$NODE_CACHE/bin/npm" ]; then
  export PATH="$NODE_CACHE/bin:$PATH"
else
  echo "=== Downloading Node.js $NODE_VERSION (no system install required) ==="
  mkdir -p "$NODE_CACHE"
  ARCH=$(uname -m)
  OS=$(uname -s)
  case "$OS" in
    Darwin)
      if [ "$ARCH" = "arm64" ]; then
        TAR="node-${NODE_VERSION}-darwin-arm64.tar.gz"
      else
        TAR="node-${NODE_VERSION}-darwin-x64.tar.gz"
      fi
      ;;
    Linux)
      if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
        TAR="node-${NODE_VERSION}-linux-arm64.tar.xz"
      else
        TAR="node-${NODE_VERSION}-linux-x64.tar.xz"
      fi
      ;;
    *)
      echo "ensure-node: unsupported OS $OS" >&2
      exit 1
      ;;
  esac
  URL="https://nodejs.org/dist/${NODE_VERSION}/${TAR}"
  if [ ! -f "$NODE_CACHE/$TAR" ]; then
    curl -sSLf -o "$NODE_CACHE/$TAR" "$URL"
  fi
  tar -xf "$NODE_CACHE/$TAR" -C "$NODE_CACHE" --strip-components=1
  export PATH="$NODE_CACHE/bin:$PATH"
fi
