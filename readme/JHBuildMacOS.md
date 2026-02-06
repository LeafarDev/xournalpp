# JHBuild on macOS

This guide adapts the [GNOME JHBuild setup](https://wiki.gnome.org/Projects/Jhbuild) for macOS. JHBuild downloads, configures, and builds GNOME (or other) development code in a way that does not modify your system. It also resolves dependencies and builds development software as needed.

**Important:** Use only this guide (or one other chosen guide) when setting up JHBuild; mixing different guides can lead to incompatible setups. The [official JHBuild manual](https://gnome.pages.gitlab.gnome.org/jhbuild/) is still useful for command-line and configuration options.

**Note:** This page is for contributors who want JHBuild on macOS. If you are new to GNOME, read the [JHBuild Introduction](https://wiki.gnome.org/Projects/Jhbuild/Introduction) first.

---

## Prerequisites (macOS)

- **Xcode Command Line Tools** (or full Xcode):
  ```bash
  xcode-select --install
  ```
- **Python 3** (macOS includes Python 3; or use Homebrew: `brew install python@3`)
- **Git**: `xcode-select --install` provides `git`, or `brew install git`

---

## Remove existing JHBuild install

If you have used JHBuild before (or started another setup), remove old files so you can start clean.

Ensure none of these exist:

- `~/bin/jhbuild`
- `~/.local/bin/jhbuild`
- `~/.local/share/jhbuild/`
- `~/.cache/jhbuild/`
- `~/.config/jhbuildrc`
- `~/.jhbuildrc`
- `~/jhbuild/`

**⚠️ Be very careful with the following command.** A small mistake (e.g. a misplaced space) can delete personal files.

```bash
rm -rf ~/bin/jhbuild ~/.local/bin/jhbuild ~/.local/share/jhbuild ~/.cache/jhbuild ~/.config/jhbuildrc ~/.jhbuildrc ~/jhbuild
```

You may also have:

- **`~/checkout/`** – created by jhbuild; safe to remove if you did not put your own files there.
- **`/opt/gnome/`** – optional install prefix from some setups; can be removed if present.
- **gtk-osx layout** – if you previously used gtk-osx or Xournal++ macOS build scripts, you may have `~/.new_local/`, `~/Source/`, `~/gtk/`, `~/gtk-osx-custom/`. Remove them only if you want a clean JHBuild install and do not need that environment:
  ```bash
  rm -rf ~/.new_local ~/Source ~/gtk ~/gtk-osx-custom ~/.config/jhbuildrc-custom
  ```

Check your shell config (`~/.zshrc` or `~/.bash_profile`) and remove any line at the bottom that does `export PATH=(long list of directories)`. Such a line can cause serious issues.

If you installed JHBuild via Homebrew, remove it:

```bash
brew remove jhbuild   # if installed
```

---

## Directory layout (macOS)

The layout matches the GNOME guide, with install under your home directory.

### Files that are JHBuild itself

- **Source:** `~/jhbuild/jhbuild/` – clone of the jhbuild repository.
- **Install:**
  - `~/.local/share/jhbuild/` – most JHBuild files.
  - `~/.local/bin/jhbuild` – main executable.
- **PATH:** Ensure `~/.local/bin` (or `~/bin`) is in your `PATH`. We will use `~/.local/bin` below.

### Files created when you run JHBuild

- `~/.cache/jhbuild/downloads/` – downloaded tarballs.
- `~/jhbuild/checkout/` – unpacked and compiled software.
- `~/jhbuild/install/` – installed built software (prefix for “jhbuilt” apps).

---

## Ensure `~/.local/bin` (or `~/bin`) is in your PATH

On macOS with the default zsh:

```bash
mkdir -p ~/.local/bin
```

Add to `~/.zshrc` (or `~/.bash_profile` if you use bash):

```bash
export PATH="$HOME/.local/bin:$PATH"
```

Then reload your shell or run:

```bash
source ~/.zshrc
```

---

## Download and install JHBuild

**Quick run:** From the repository root you can use the script (optional):

```bash
bash mac-setup/install-jhbuild-standalone.sh
```

Or follow the steps below manually.

Create the base directory:

```bash
mkdir -p ~/jhbuild
```

If the directory already exists and you want a clean start, remove it first: `rm -rf ~/jhbuild` then create it again. Old files in `~/jhbuild/` can cause problems.

Clone JHBuild from GitLab:

```bash
cd ~/jhbuild
git clone --depth=1 https://gitlab.gnome.org/GNOME/jhbuild.git
```

Then build and install with the simple install (recommended):

```bash
cd ~/jhbuild/jhbuild
./autogen.sh --simple-install
make
make install
```

If `./autogen.sh` fails (e.g. missing `autoconf`, `automake`), install them with Homebrew:

```bash
brew install autoconf automake libtool pkg-config
```

Then run `./autogen.sh --simple-install` again.

After `make install`, `jhbuild` should be at `~/.local/bin/jhbuild`. Verify:

```bash
jhbuild --version
```

---

## Configuration: `~/.config/jhbuildrc`

JHBuild will create or use `~/.config/jhbuildrc`. To use a custom prefix under your home (e.g. `~/jhbuild/install`), you can set in `~/.config/jhbuildrc`:

```python
# Optional: install built modules under ~/jhbuild/install
# prefix = os.path.expanduser('~/jhbuild/install')
# check_installed = False
```

The [JHBuild manual](https://gnome.pages.gitlab.gnome.org/jhbuild/) and the [configuration reference](https://gnome.pages.gitlab.gnome.org/jhbuild/config-reference.html) describe all options.

---

## System dependencies (macOS)

JHBuild does not install macOS system libraries. For GNOME/GTK modules you typically need (via Homebrew):

```bash
brew install pkg-config glib gobject-introspection
```

For a full GTK stack on macOS (e.g. to build GTK apps), many projects use **gtk-osx** and its own layout (`~/.new_local`, `~/gtk`, etc.). That setup is different from the layout above; see [MacBuild.md](MacBuild.md) for the Xournal++/gtk-osx approach.

---

## Using JHBuild

- Build a module (e.g. GLib):
  ```bash
  jhbuild build glib
  ```
- Update and rebuild:
  ```bash
  jhbuild update
  jhbuild build
  ```
- Run a program in the jhbuild environment:
  ```bash
  jhbuild run ./myapp
  ```

---

## If something goes wrong

### `jhbuild: command not found`

- Ensure `~/.local/bin` is in your `PATH` and that `~/.local/bin/jhbuild` exists.
- Run `source ~/.zshrc` (or `~/.bash_profile`) or open a new terminal.

### Errors about `/opt/gnome`

- Your config or an old guide is using `/opt/gnome` as prefix. Edit `~/.config/jhbuildrc` and set a prefix under your home (e.g. `~/jhbuild/install`) and remove references to `/opt/gnome`.

### Errors about missing dependencies

- On macOS, install the needed libraries with Homebrew (e.g. `brew install <package>`). For Python modules: `pip3 install --user <package>`.

### Problems building a specific module

- Check the [JHBuild manual](https://gnome.pages.gitlab.gnome.org/jhbuild/) and the module’s own build instructions. On macOS, some modules need extra flags or patches (e.g. gtk-osx uses a patched moduleset).

---

## Relation to gtk-osx (Xournal++ .app build)

The Xournal++ macOS **.app** build uses **gtk-osx**, which installs JHBuild and a custom layout:

- JHBuild executable: `~/.new_local/bin/jhbuild`
- Build/install prefix: `~/gtk/inst`
- Config: `~/.config/jhbuildrc` and `~/.config/jhbuildrc-custom`
- Sources: `~/Source/jhbuild`, `~/gtk-osx-custom`, etc.

That setup is **separate** from the layout in this guide. Do not mix the two. For building the Xournal++ .app, follow [MacBuild.md](MacBuild.md) and the `mac-setup/CI_jhbuild.sh` flow instead of this guide.

### Using CI_jhbuild.sh with jhbuild already installed

If you installed jhbuild with this guide (`~/.local/bin/jhbuild`), you can run `mac-setup/CI_jhbuild.sh` and it will **use that jhbuild** instead of cloning and running gtk-osx-setup every time (faster and reuses your Python).

- The script detects `~/.local/bin/jhbuild` and skips installing jhbuild/pyenv.
- It points the gtk-osx config at your **machine’s Python** (via a small stub under `~/.cache/jhbuild-pyenv-stub`) so pkg-config and libs are found and the build does not break.
- Optional: the script will install Python dependencies from `mac-setup/jhbuild-python-requirements.txt` (same Python that runs jhbuild) if that file lists any packages. You can add packages there if the build fails with a missing Python module. You can instead **fetch requirements from a URL** by setting `JHBUILD_REQUIREMENTS_URL` before running the script (e.g. a raw GitHub/GitLab URL that returns a requirements file).

**Running jhbuild manually** with the gtk-osx config (e.g. after you ran `CI_jhbuild.sh` once) requires `PYENV_ROOT` and `PYENV_VERSION` to be set, otherwise you get `KeyError: 'PYENV_ROOT'`. Use the wrapper script so the config loads:

```bash
bash mac-setup/jhbuild-with-env.sh sysdeps --install meta-bootstrap
bash mac-setup/jhbuild-with-env.sh build meta-gtk-osx-gtk3
```

Alternatively, add to your `~/.zshrc` (and run `CI_jhbuild.sh` once to create the stub):

```bash
export PYENV_ROOT="$HOME/.cache/jhbuild-pyenv-stub"
export PYENV_VERSION="3"
```

Then you can run `jhbuild` directly.
