#!/usr/bin/env bash

set -e
set -o pipefail

### Step 1: install jhbuild

MAC_SETUP_DIR="$(cd "$(dirname "$0")" && pwd)"
LOCKFILE="$MAC_SETUP_DIR/jhbuild-version.lock"
MODULEFILE="$MAC_SETUP_DIR/xournalpp.modules"
GTK_OSX_PATCHFILE="$MAC_SETUP_DIR/gtk-osx.patch"
GTK_MODULES="meta-gtk-osx-gtk3 gtksourceview3"

# Use already-installed jhbuild (e.g. from readme/JHBuildMacOS.md) instead of cloning + gtk-osx-setup.
# You can force either mode:
# - JHBUILD_USE_INSTALLED=1  -> use ~/.local/bin/jhbuild (no pyenv)
# - JHBUILD_USE_INSTALLED=0  -> run gtk-osx-setup.sh (uses pyenv; closer to CI)
# If unset, script auto-detects ~/.local/bin/jhbuild.
if [ "${JHBUILD_USE_INSTALLED:-}" = "1" ]; then
    JHBUILD_USE_INSTALLED=1
elif [ "${JHBUILD_USE_INSTALLED:-}" = "0" ]; then
    JHBUILD_USE_INSTALLED=0
elif [ -x "$HOME/.local/bin/jhbuild" ]; then
    JHBUILD_USE_INSTALLED=1
else
    JHBUILD_USE_INSTALLED=0
fi
# When using installed jhbuild, we keep ~/gtk by default (faster re-runs). Set JHBUILD_CLEAN=1 to force a full clean.
JHBUILD_CLEAN="${JHBUILD_CLEAN:-0}"

get_lockfile_entry() {
    local key="$1"
    # Print the value corresponding to the provided lockfile key to stdout
    sed -e "/$key/!d" -e 's/^[^=]*=//' "$LOCKFILE"
}

shallow_clone_into_commit() {
    local repo="$1"            # Target repository
    local lockfile_entry="$2"  # Name of the lockfile entry containing the commit hash
    local dir="$3"             # Destination directory
    local commit=$(get_lockfile_entry $lockfile_entry)

    echo "Syncing commit $commit of $repo into $dir"

    # Reuse existing clone unless JHBUILD_CLEAN=1
    if [ -d "$dir/.git" ] && [ "$JHBUILD_CLEAN" != "1" ]; then
        (cd "$dir" && git remote set-url origin "$repo")
        # If already at commit, do nothing
        if (cd "$dir" && git rev-parse HEAD >/dev/null 2>&1) && [ "$(cd "$dir" && git rev-parse HEAD)" = "$commit" ]; then
            echo "Already at $commit, reusing existing clone"
            return 0
        fi
        (cd "$dir" && git fetch --depth 1 origin "$commit")
        (cd "$dir" && git checkout -q FETCH_HEAD)
        return 0
    fi

    [ -d "$dir" ] && rm -rf "$dir"
    mkdir -p "$dir"

    # Shallow clone into a commit
    (cd "$dir" && git init -b main)
    (cd "$dir" && git remote add origin "$repo")
    (cd "$dir" && git fetch --depth 1 origin "$commit")
    (cd "$dir" && git checkout -q FETCH_HEAD)
}

download_jhbuild_sources() {
    shallow_clone_into_commit "https://gitlab.gnome.org/GNOME/gtk-osx.git" "gtk-osx" ~/gtk-osx-custom
    # Patch gtk-osx module sets (idempotente: só aplica se ainda não estiver aplicado)
    (
        cd ~/gtk-osx-custom
        # Se for run “limpo”, garante árvore sem modificações antes de aplicar patch
        if [ "$JHBUILD_CLEAN" = "1" ]; then
            git reset --hard HEAD
            git clean -fd
        fi
        if git apply --check "$GTK_OSX_PATCHFILE" >/dev/null 2>&1; then
            echo "Applying gtk-osx patch..."
            git apply "$GTK_OSX_PATCHFILE"
        else
            echo "gtk-osx patch already applied or not needed, skipping"
        fi
        git status -sb
    )

    if [ "$JHBUILD_USE_INSTALLED" = "1" ]; then
        echo "Using already-installed jhbuild (skip cloning jhbuild sources)"
    else
        # Make a shallow clone of jhbuild's sources. This way we detect if cloning fails and avoid the deep clone in gtk-osx-setup.sh
        JHBUILD_BRANCH=$(sed -e '/^JHBUILD_RELEASE_VERSION=/!d' -e 's/^[^=]*=//' -e 's/^\"\([^\"]*\)\"$/\1/' ~/gtk-osx-custom/gtk-osx-setup.sh)
        echo "Cloning jhbuild version $JHBUILD_BRANCH"
        rm -rf "$HOME/Source/jhbuild"
        mkdir -p "$HOME/Source"
        git clone --depth 1 -b $JHBUILD_BRANCH https://gitlab.gnome.org/GNOME/jhbuild.git "$HOME/Source/jhbuild"
    fi

    shallow_clone_into_commit "https://github.com/xournalpp/xournalpp-pipeline-dependencies" "xournalpp-pipeline-dependencies" ~/xournalpp-pipeline-dependencies
}

echo "::group::Download jhbuild sources"
download_jhbuild_sources
echo "::endgroup::"

install_jhbuild() {
    if [ "$JHBUILD_USE_INSTALLED" = "1" ]; then
        # Use installed jhbuild: keep ~/gtk by default; set JHBUILD_CLEAN=1 to wipe and start from scratch.
        if [ "$JHBUILD_CLEAN" = "1" ]; then
            rm -rf ~/gtk ~/.config/jhbuildrc ~/.config/jhbuildrc-custom ~/.cache/jhbuild
        else
            rm -rf ~/.config/jhbuildrc ~/.config/jhbuildrc-custom
        fi
        mkdir -p ~/.config
    else
        # gtk-osx/pyenv mode: keep ~/.new_local, ~/Source, ~/.cache by default (faster re-runs).
        # Set JHBUILD_CLEAN=1 to wipe and rerun gtk-osx-setup.sh from scratch.
        NEED_SETUP=0
        if [ "$JHBUILD_CLEAN" = "1" ]; then
            rm -rf ~/gtk ~/.new_local ~/.config/jhbuildrc ~/.config/jhbuildrc-custom ~/Source ~/.cache/jhbuild
            NEED_SETUP=1
        else
            rm -rf ~/.config/jhbuildrc ~/.config/jhbuildrc-custom
            # If jhbuild from gtk-osx is not present, run setup once.
            if [ ! -x "$HOME/.new_local/bin/jhbuild" ]; then
                NEED_SETUP=1
            fi
        fi
        if [ "$NEED_SETUP" = "1" ]; then
            bash ~/gtk-osx-custom/gtk-osx-setup.sh
        fi
    fi

    # Copy default jhbuild config files from the pinned commit
    install -m644 ~/gtk-osx-custom/jhbuildrc-gtk-osx ~/.config/jhbuildrc
    install -m644 ~/gtk-osx-custom/jhbuildrc-gtk-osx-custom-example ~/.config/jhbuildrc-custom

    # When using installed jhbuild, gtk-osx jhbuildrc expects PYENV_ROOT (set by gtk-osx-setup.sh).
    # Point the stub at this machine's Python so pkg-config and libs are found and nothing breaks.
    if [ "$JHBUILD_USE_INSTALLED" = "1" ]; then
        JHBUILD_PYENV_STUB="$HOME/.cache/jhbuild-pyenv-stub"
        rm -rf "$JHBUILD_PYENV_STUB"
        PYTHON_PREFIX=$(python3 -c "import sys; print(sys.prefix)" 2>/dev/null) || true
        PYTHON_VER=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')" 2>/dev/null) || true
        if [ -n "$PYTHON_PREFIX" ] && [ -d "$PYTHON_PREFIX/lib" ]; then
            mkdir -p "$JHBUILD_PYENV_STUB/versions/${PYTHON_VER:-3}"
            ln -s "$PYTHON_PREFIX/lib" "$JHBUILD_PYENV_STUB/versions/${PYTHON_VER:-3}/lib"
            JHBUILD_PYENV_VERSION="${PYTHON_VER:-3}"
        else
            mkdir -p "$JHBUILD_PYENV_STUB/versions/3/lib"
            JHBUILD_PYENV_VERSION="3"
        fi
    fi
}

configure_jhbuild_envvars() {
    # Avoid leaking Python env vars into brew-python tools (meson), which can break imports via sitecustomize.
    unset PYTHONPATH
    unset PYTHONHOME

    if [ "$JHBUILD_USE_INSTALLED" = "1" ]; then
        JHBUILD_BIN="$HOME/.local/bin"
        # gtk-osx jhbuildrc requires PYENV_ROOT and PYENV_VERSION; stub points at this machine's Python lib
        export PYENV_ROOT="${PYENV_ROOT:-$HOME/.cache/jhbuild-pyenv-stub}"
        export PYENV_VERSION="${PYENV_VERSION:-${JHBUILD_PYENV_VERSION:-3}}"
    else
        JHBUILD_BIN="$HOME/.new_local/bin"
    fi

    if ! [ -f ~/.zshrc ] || ! grep -q "$JHBUILD_BIN" ~/.zshrc; then
        echo "Permanently adding jhbuild to path..."
        echo "export PATH=\"$JHBUILD_BIN\":\"\$PATH\"" >> ~/.zshrc
    fi

    # Add jhbuild to PATH for this shell
    export PATH="$JHBUILD_BIN:$PATH"
}

# When using installed jhbuild, ensure the Python that runs jhbuild has deps installed (avoids breakage).
# Sources: local mac-setup/jhbuild-python-requirements.txt, or set JHBUILD_REQUIREMENTS_URL to fetch from a URL.
install_jhbuild_python_deps() {
    if [ "$JHBUILD_USE_INSTALLED" != "1" ]; then
        return 0
    fi
    local req_file=""
    if [ -n "${JHBUILD_REQUIREMENTS_URL:-}" ]; then
        local tmp_req
        tmp_req=$(mktemp -t jhbuild-requirements.XXXXXX.txt)
        if curl -sSfL --retry 2 -o "$tmp_req" "$JHBUILD_REQUIREMENTS_URL"; then
            req_file="$tmp_req"
        else
            echo "Warning: could not fetch JHBUILD_REQUIREMENTS_URL, skipping Python deps from URL"
            rm -f "$tmp_req"
        fi
    fi
    if [ -z "$req_file" ]; then
        req_file="$MAC_SETUP_DIR/jhbuild-python-requirements.txt"
        [ -f "$req_file" ] || return 0
    fi
    if ! grep -v -e '^[[:space:]]*#' -e '^[[:space:]]*$' "$req_file" | grep -q .; then
        [ "$req_file" = "$MAC_SETUP_DIR/jhbuild-python-requirements.txt" ] || rm -f "$req_file"
        return 0
    fi
    echo "Installing Python deps for jhbuild..."
    python3 -m pip install --user -r "$req_file" || true
    # Ensure pip-installed entrypoints (e.g. meson) are found before Homebrew's wrappers.
    # This avoids issues when /opt/homebrew/bin/meson is tied to a broken brew-python env.
    PY_USER_BIN="$(python3 -c 'import site; print(site.getuserbase() + \"/bin\")' 2>/dev/null || true)"
    if [ -n "$PY_USER_BIN" ] && [ -d "$PY_USER_BIN" ]; then
        export PATH="$PY_USER_BIN:$PATH"
    fi
    [ "$req_file" = "$MAC_SETUP_DIR/jhbuild-python-requirements.txt" ] || rm -f "$req_file"
}

setup_custom_modulesets() {
    # Set osx deployment target
    sed -i '' -e "s/^setup_sdk()/setup_sdk(target=\"$(get_lockfile_entry deployment_target)\")/" ~/.config/jhbuildrc-custom

    # Enable custom jhbuild configuration
    cat >> ~/.config/jhbuildrc-custom <<'EOF'

### BEGIN xournalpp macOS CI
use_local_modulesets = True
moduleset = "gtk-osx.modules"
modulesets_dir = os.path.expanduser("~/gtk-osx-custom/modulesets-stable")

# Fix freetype build finding brotli installed through brew or ports, causing the
# harfbuzz build to fail when jhbuild's pkg-config cannot find brotli.
module_cmakeargs['freetype-no-harfbuzz'] = ' -DFT_DISABLE_BROTLI=TRUE '
module_cmakeargs['freetype'] = ' -DFT_DISABLE_BROTLI=TRUE '

# portaudio may fail with parallel build, so disable parallel building.
module_makeargs['portaudio'] = ' -j1 '

repos['ftp.gnu.org'] = 'https://ftpmirror.gnu.org/gnu/'

# Force gtk-doc to use the same python that loaded this config (where pygments is installed).
import site, sys
_usersite = site.getusersitepackages()
_env = module_extra_env.get('gtk-doc', {}).copy()
_env.update({
    'PYTHON': sys.executable,
    'PYTHONPATH': _usersite + os.pathsep + os.environ.get('PYTHONPATH', ''),
})
module_extra_env['gtk-doc'] = _env

### END
EOF

    echo "interact = False" >> ~/.config/jhbuildrc
    echo "exit_on_error = True" >> ~/.config/jhbuildrc
    echo "shallow_clone = True" >> ~/.config/jhbuildrc
    echo "use_local_modulesets = True" >> ~/.config/jhbuildrc
    echo "disable_Werror = False" >> ~/.config/jhbuildrc

    # MODULEFILE contains dependencies to other module sets - they need to be in the same directory
    cp "$MODULEFILE" "$HOME/gtk-osx-custom/modulesets-stable/"
    export MODULEFILE="$(basename $MODULEFILE)"

    echo "verbose=off" >> ~/.wgetrc
}

echo "::group::Setup jhbuild"
[ "$JHBUILD_USE_INSTALLED" = "1" ] && echo "Using already-installed jhbuild (e.g. ~/.local/bin/jhbuild)"
install_jhbuild
configure_jhbuild_envvars
install_jhbuild_python_deps
setup_custom_modulesets
echo "::endgroup::"


### Step 2: Download modules' sources
download() {
    jhbuild update $GTK_MODULES
    jhbuild -m "$MODULEFILE" update meta-xournalpp-deps
    jhbuild -m bootstrap.modules update meta-bootstrap
    echo "Downloaded all jhbuild modules' sources"
}
echo "::group::Download modules' sources"
download
echo "::endgroup::"

diagnose_tools() {
    echo "PATH=$PATH"
    echo "python3=$(command -v python3 || true)"
    python3 --version 2>/dev/null || true
    echo "meson=$(command -v meson || true)"
    meson --version 2>/dev/null || true
    echo "--- inside jhbuild run ---"
    jhbuild run bash -lc 'echo "PATH=$PATH"; echo "python3=$(command -v python3)"; python3 --version; echo "meson=$(command -v meson)"; meson --version' 2>/dev/null || true
}

### Step 3: bootstrap
bootstrap_jhbuild() {
    echo "::group::Diagnose toolchain"
    diagnose_tools
    echo "::endgroup::"
    jhbuild -m bootstrap.modules build --no-network meta-bootstrap
}
echo "::group::Bootstrap jhbuild"
bootstrap_jhbuild
echo "::endgroup::"


### Step 4: build gtk (~15 minutes on a Mac Mini M1 w/ 8 cores)
build_gtk() {
    jhbuild build --no-network $GTK_MODULES
    echo "Finished building gtk"
}
echo "::group::Build gtk"
build_gtk
echo "::endgroup::"


### Step 5: build xournalpp deps

build_xournalpp_deps() {
    jhbuild -m "$MODULEFILE" build --no-network meta-xournalpp-deps
}
echo "::group::Build deps"
build_xournalpp_deps
echo "::endgroup::"


### Step 6: build binary blob

build_binary_blob() {
    jhbuild run python3 ~/xournalpp-pipeline-dependencies/gtk/package-gtk-bin.py -o xournalpp-binary-blob.tar.gz
}
echo "::group::Build blob"
build_binary_blob
echo "::endgroup::"
