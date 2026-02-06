#!/usr/bin/env bash
# Run jhbuild with PYENV_ROOT/PYENV_VERSION set so gtk-osx jhbuildrc loads when using
# an already-installed jhbuild (~/.local/bin). Use this when running jhbuild manually
# (e.g. "jhbuild sysdeps --install meta-bootstrap") so the config does not fail with
# KeyError: 'PYENV_ROOT'.
#
# Usage: bash mac-setup/jhbuild-with-env.sh [jhbuild args...]
# Example: bash mac-setup/jhbuild-with-env.sh sysdeps --install meta-bootstrap

set -e

JHBUILD_PYENV_STUB="${HOME}/.cache/jhbuild-pyenv-stub"

ensure_stub() {
    if [ -d "$JHBUILD_PYENV_STUB/versions" ] && [ -n "$(ls -A "$JHBUILD_PYENV_STUB/versions" 2>/dev/null)" ]; then
        # Use existing stub; set PYENV_VERSION from the version dir present (e.g. 3.9 or 3)
        PYENV_VERSION=$(ls "$JHBUILD_PYENV_STUB/versions" 2>/dev/null | head -1)
        export PYENV_VERSION="${PYENV_VERSION:-3}"
        return 0
    fi
    echo "Creating PYENV stub at $JHBUILD_PYENV_STUB for gtk-osx jhbuildrc..."
    rm -rf "$JHBUILD_PYENV_STUB"
    PYTHON_PREFIX=$(python3 -c "import sys; print(sys.prefix)" 2>/dev/null) || true
    PYTHON_VER=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')" 2>/dev/null) || true
    if [ -n "$PYTHON_PREFIX" ] && [ -d "$PYTHON_PREFIX/lib" ]; then
        mkdir -p "$JHBUILD_PYENV_STUB/versions/${PYTHON_VER:-3}"
        ln -s "$PYTHON_PREFIX/lib" "$JHBUILD_PYENV_STUB/versions/${PYTHON_VER:-3}/lib"
        export PYENV_VERSION="${PYTHON_VER:-3}"
    else
        mkdir -p "$JHBUILD_PYENV_STUB/versions/3/lib"
        export PYENV_VERSION="3"
    fi
}

export PATH="${HOME}/.local/bin:${PATH}"
ensure_stub
export PYENV_ROOT="${PYENV_ROOT:-$JHBUILD_PYENV_STUB}"
export PYENV_VERSION="${PYENV_VERSION:-3}"

exec jhbuild "$@"
