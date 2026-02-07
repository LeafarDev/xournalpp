#!/usr/bin/env bash
# Puts MicroTeX (LaTeX→SVG headless) into Xournal++.app for chat formula rendering.
# 1) If 'microtex' or 'LaTeX' is on PATH or in a known build dir, copies it + res/.
# 2) Otherwise clones and builds MicroTeX from source (requires cmake, cairo, gtkmm, etc.).
# Run from mac-setup/ with Xournal++.app in the current directory (or pass path).

set -e

APP_PATH="${1:-./Xournal++.app}"
RESOURCES_BIN="$APP_PATH/Contents/Resources/bin"
MICROTEX_DEST="$RESOURCES_BIN/microtex"
RES_DEST="$RESOURCES_BIN/res"
REPO_URL="https://github.com/NanoMichael/MicroTeX.git"
MICROTEX_CLONE_DIR="${MICROTEX_CLONE_DIR:-$(dirname "$0")/MicroTeX}"
DEPS_DIR="${MICROTEX_DEPS_DIR:-$(cd "$(dirname "$0")" && pwd)/microtex-deps}"
TINYXML2_VER="10.0.0"
TINYXML2_URL="https://github.com/leethomason/tinyxml2/archive/refs/tags/${TINYXML2_VER}.tar.gz"

if [ ! -d "$APP_PATH/Contents" ]; then
  echo "bundle-microtex: $APP_PATH does not look like an .app bundle, skipping."
  exit 0
fi

if [ -f "$MICROTEX_DEST" ] && [ -d "$RES_DEST" ]; then
  echo "bundle-microtex: $MICROTEX_DEST and res/ already present, skipping."
  exit 0
fi

# Prefer copying from system or known build
BINARY_SRC=""
RES_SRC=""
if command -v microtex >/dev/null 2>&1; then
  BINARY_SRC="$(command -v microtex)"
  # res might be next to binary or in same prefix
  RES_SRC="$(dirname "$BINARY_SRC")/res"
elif command -v LaTeX >/dev/null 2>&1; then
  BINARY_SRC="$(command -v LaTeX)"
  RES_SRC="$(dirname "$BINARY_SRC")/res"
fi
if [ -n "$BINARY_SRC" ] && [ -x "$BINARY_SRC" ]; then
  mkdir -p "$RESOURCES_BIN"
  cp "$BINARY_SRC" "$MICROTEX_DEST"
  chmod 755 "$MICROTEX_DEST"
  if [ -d "$RES_SRC" ]; then
    rm -rf "$RES_DEST"
    cp -R "$RES_SRC" "$RES_DEST"
  fi
  echo "bundle-microtex: copied from $BINARY_SRC to $MICROTEX_DEST"
  exit 0
fi

# Build from source
echo "bundle-microtex: building MicroTeX from source..."
if ! command -v cmake >/dev/null 2>&1; then
  echo "bundle-microtex: cmake not found, skipping build. Install with: brew install cmake"
  exit 0
fi

# Ensure tinyxml2 is available (build locally if missing)
TINYXML2_INCLUDE_DIR=""
TINYXML2_LIBRARY=""
if ! pkg-config --exists tinyxml2 >/dev/null 2>&1; then
  echo "bundle-microtex: tinyxml2 not found, building locally..."
  mkdir -p "$DEPS_DIR"
  TINYXML2_SRC="$DEPS_DIR/tinyxml2-$TINYXML2_VER"
  if [ ! -d "$TINYXML2_SRC" ]; then
    curl -L "$TINYXML2_URL" -o "$DEPS_DIR/tinyxml2.tar.gz"
    tar -xzf "$DEPS_DIR/tinyxml2.tar.gz" -C "$DEPS_DIR"
  fi
  TINYXML2_BUILD="$TINYXML2_SRC/build"
  mkdir -p "$TINYXML2_BUILD"
  pushd "$TINYXML2_BUILD" >/dev/null
  cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$DEPS_DIR" >/dev/null
  cmake --build . --config Release >/dev/null
  cmake --install . >/dev/null
  popd >/dev/null
  export PKG_CONFIG_PATH="$DEPS_DIR/lib/pkgconfig:${PKG_CONFIG_PATH}"
  export CMAKE_PREFIX_PATH="$DEPS_DIR:${CMAKE_PREFIX_PATH}"
  mkdir -p "$DEPS_DIR/lib/pkgconfig"
  cat <<EOF > "$DEPS_DIR/lib/pkgconfig/tinyxml2.pc"
prefix=$DEPS_DIR
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: tinyxml2
Description: TinyXML-2
Version: $TINYXML2_VER
Libs: -L\${libdir} -ltinyxml2
Cflags: -I\${includedir}
EOF
  TINYXML2_INCLUDE_DIR="$DEPS_DIR/include"
  if [ -f "$DEPS_DIR/lib/libtinyxml2.dylib" ]; then
    TINYXML2_LIBRARY="$DEPS_DIR/lib/libtinyxml2.dylib"
  elif [ -f "$DEPS_DIR/lib/libtinyxml2.a" ]; then
    TINYXML2_LIBRARY="$DEPS_DIR/lib/libtinyxml2.a"
  fi
else
  TINYXML2_INCLUDE_DIR=""
  TINYXML2_LIBRARY=""
fi

if [ ! -d "$MICROTEX_CLONE_DIR" ]; then
  git clone --depth 1 "$REPO_URL" "$MICROTEX_CLONE_DIR"
fi
BUILD_DIR="$MICROTEX_CLONE_DIR/build"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR" >/dev/null

# Prefer jhbuild env (GTKDIR) for macOS; else use system Homebrew deps
export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-}"
if [ -n "$GTKDIR" ] && [ -d "$GTKDIR/lib/pkgconfig" ]; then
  export PKG_CONFIG_PATH="$GTKDIR/lib/pkgconfig:${PKG_CONFIG_PATH}"
  export CMAKE_PREFIX_PATH="$GTKDIR:$CMAKE_PREFIX_PATH"
fi
# Homebrew (macOS): brew install gtkmm3 cairo tinyxml2 fontconfig
# gtksourceviewmm-3.0 is optional for headless; MicroTeX CMake may require it for UNIX
# Ensure gtkmm/cairomm/gtksourceviewmm are available (use Homebrew if present)
if ! pkg-config --exists gtkmm-3.0 cairomm-1.0 gtksourceviewmm-3.0 >/dev/null 2>&1; then
  if command -v brew >/dev/null 2>&1; then
    echo "bundle-microtex: installing gtkmm/cairomm/gtksourceviewmm via Homebrew..."
    brew install gtkmm3 cairomm gtksourceviewmm3 fontconfig || true
  fi
fi

if ! cmake .. -DHAVE_LOG=OFF -DGRAPHICS_DEBUG=OFF \
  ${TINYXML2_INCLUDE_DIR:+-DTINYXML2_INCLUDE_DIR="$TINYXML2_INCLUDE_DIR"} \
  ${TINYXML2_LIBRARY:+-DTINYXML2_LIBRARY="$TINYXML2_LIBRARY"} \
  2>/dev/null; then
  echo "bundle-microtex: cmake failed (missing gtkmm/cairomm?). Install deps: brew install gtkmm3 cairo tinyxml2 fontconfig"
  popd >/dev/null
  exit 0
fi
make -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)" 2>/dev/null || make -j2
popd >/dev/null

if [ ! -x "$BUILD_DIR/LaTeX" ]; then
  echo "bundle-microtex: build did not produce LaTeX executable, skipping."
  exit 0
fi

mkdir -p "$RESOURCES_BIN"
cp "$BUILD_DIR/LaTeX" "$MICROTEX_DEST"
chmod 755 "$MICROTEX_DEST"
if [ -d "$BUILD_DIR/res" ]; then
  rm -rf "$RES_DEST"
  cp -R "$BUILD_DIR/res" "$RES_DEST"
fi
echo "bundle-microtex: installed MicroTeX to $MICROTEX_DEST (built from source)"
