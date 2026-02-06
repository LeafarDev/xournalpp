#!/usr/bin/env bash
#
# Extract Python (pip) requirements from gtk-osx-setup.sh without affecting your current checkout.
# It clones the pinned gtk-osx commit into a temp directory, parses gtk-osx-setup.sh for pip installs,
# and prints a deduplicated requirements-style list.
#
# Usage:
#   bash mac-setup/extract-gtk-osx-python-requirements.sh > /tmp/gtk-osx-python-requirements.txt
#   bash mac-setup/extract-gtk-osx-python-requirements.sh /path/to/output.txt
#
# Notes:
# - Best-effort parsing: it extracts package tokens passed to "pip install" / "python -m pip install".
# - It intentionally ignores local paths and VCS URLs unless they look like package specs.
#
set -euo pipefail

MAC_SETUP_DIR="$(cd "$(dirname "$0")" && pwd)"
LOCKFILE="$MAC_SETUP_DIR/jhbuild-version.lock"

out="${1:-}"

get_lockfile_entry() {
  local key="$1"
  sed -e "/$key/!d" -e 's/^[^=]*=//' "$LOCKFILE"
}

gtk_commit="$(get_lockfile_entry gtk-osx)"
if [ -z "$gtk_commit" ]; then
  echo "Error: could not read gtk-osx commit from $LOCKFILE" >&2
  exit 1
fi

tmpdir="$(mktemp -d -t gtk-osx-requirements.XXXXXX)"
cleanup() { rm -rf "$tmpdir"; }
trap cleanup EXIT

repo_dir="$tmpdir/gtk-osx"
mkdir -p "$repo_dir"
(cd "$repo_dir" && git init -b main >/dev/null)
(cd "$repo_dir" && git remote add origin "https://gitlab.gnome.org/GNOME/gtk-osx.git" >/dev/null)
(cd "$repo_dir" && git fetch --depth 1 origin "$gtk_commit" >/dev/null)
(cd "$repo_dir" && git checkout -q FETCH_HEAD)

setup_sh="$repo_dir/gtk-osx-setup.sh"
if [ ! -f "$setup_sh" ]; then
  echo "Error: gtk-osx-setup.sh not found in fetched commit" >&2
  exit 1
fi

# Parse pip install invocations into requirements-like lines.
# We purposely:
# - drop common flags (-U, --upgrade, --user, --no-cache-dir, etc.)
# - stop at shell control operators (&&, ||, ;, |)
# - ignore obvious local paths (./foo, /abs/path) and variable expansions ($X, ${X})
python3 - "$setup_sh" <<'PY' > "$tmpdir/requirements.txt"
import re, sys, shlex, pathlib

path = sys.argv[1]
text = pathlib.Path(path).read_text(errors="replace")

# Make it easier to parse multi-line pip installs.
text = text.replace("\\\n", " ")

pip_cmd_re = re.compile(
    r"""(?x)
    (?:
      \bpython(?:3)?\s+-m\s+pip\s+install
      |
      (?:\S*/)?pip(?:3)?\s+install
      |
      (?:\$(?:\{)?[A-Za-z_][A-Za-z0-9_]*PIP[A-Za-z0-9_]*(?:\})?)\s+install   # e.g. $PIP install / ${MY_PIP} install
    )
    \s+
    (?P<args>[^\\n]+)
    """
)

stop_tokens = {"&&", "||", ";", "|"}
drop_flags = {
    "-U", "--upgrade",
    "--user",
    "--no-cache-dir",
    "--disable-pip-version-check",
    "--no-input",
    "--quiet", "-q",
    "--require-hashes",
    "--prefer-binary",
    "--no-binary",
    "--only-binary",
}

def is_probably_spec(tok: str) -> bool:
    # Exclude obvious non-spec tokens
    if tok in stop_tokens: return False
    if "=" in tok: return False
    if tok.startswith(("./", "/", "$", "${")): return False
    if tok.startswith(("-", "--")): return False
    # ignore requirements files and constraints (handled via flags)
    if tok.endswith((".txt", ".in")) and ("/" in tok or tok.startswith(".")): return False
    return True

reqs = set()

for m in pip_cmd_re.finditer(text):
    args = m.group("args").strip()
    # Stop early at control operators
    for op in ("&&", "||", ";", "|"):
        if op in args:
            args = args.split(op, 1)[0].strip()
    try:
        toks = shlex.split(args)
    except ValueError:
        # fallback: whitespace split
        toks = args.split()
    it = iter(toks)
    for tok in it:
        if tok in stop_tokens:
            break
        if tok in drop_flags:
            continue
        # flags with an argument: skip the argument
        if tok in {"-r", "--requirement", "-c", "--constraint", "-t", "--target"}:
            next(it, None)
            continue
        if tok in {"--index-url", "--extra-index-url", "--find-links"}:
            next(it, None)
            continue
        # keep VCS / URL specs (they are valid pip specs)
        if tok.startswith(("git+", "https://", "http://")):
            reqs.add(tok)
            continue
        if is_probably_spec(tok):
            reqs.add(tok)

# Also extract package list from the Pipfile heredoc embedded in gtk-osx-setup.sh
# (This is where most Python tooling deps are declared).
in_packages = False
for line in text.splitlines():
    s = line.strip()
    if s == "[packages]":
        in_packages = True
        continue
    if in_packages and s.startswith("[") and s.endswith("]"):
        in_packages = False
        continue
    if not in_packages:
        continue
    if not s or s.startswith("#"):
        continue
    # examples:
    # pygments = "*"
    # meson = {version=">=0.56.0"}
    m = re.match(r"^([A-Za-z0-9_.-]+)\s*=\s*(.+)$", s)
    if not m:
        continue
    name, rhs = m.group(1), m.group(2)
    if name.lower() == "python_version":
        continue
    # Try to turn simple constraints into pip-style specs
    ver_m = re.search(r'version\s*=\s*"([^"]+)"', rhs)
    if ver_m:
        spec = f"{name}{ver_m.group(1)}"
        reqs.add(spec)
    else:
        reqs.add(name)

for r in sorted(reqs, key=str.lower):
    print(r)
PY

if [ -n "$out" ]; then
  mkdir -p "$(dirname "$out")"
  cp "$tmpdir/requirements.txt" "$out"
else
  cat "$tmpdir/requirements.txt"
fi

