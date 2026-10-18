#!/bin/bash
# Run the official mbed export (tools/project.py, gcc_arm, NUCLEO_L432KC) for one mbed-os tag.
#
# The export tools of mbed-os 5.7 and later start under Python 3, but need the small fixes in
# py3.patch (list() around map/filter, LazyDict.items). The tools of 5.6.6 are Python 2 only and
# do not run here. mbed-os itself is not modified: the patch is applied to a git worktree outside
# the project.
#
# usage: ./export_mbed.sh TAG [WORK]
#   TAG   mbed-os version without prefix, e.g. 5.8.6, or a commit hash of 7 to 40 hex digits
#   WORK  working directory, default ~/.cache/npr70-mbed
#   env TOOLS_FROM=TAG  use the export tools of another tag (needed for 5.7.x, whose tools are Python 2)
# result: WORK/export-TAG/{Makefile,mbed_config.h}  (Makefile of the whole mbed-os feature set)
#         WORK/mbed-os-TAG/                         (mbed-os worktree at the tag, tools patched)
# Then run port_makefile.py to merge the result into npr70/Makefile.
set -e
tag=${1:?usage: export_mbed.sh TAG [WORK]}
work=${2:-$HOME/.cache/npr70-mbed}
here=$(cd "$(dirname "$0")" && pwd)
npr70=$(cd "$here/../.." && pwd)

mkdir -p "$work"
cd "$work"

# Python 3.11 venv with the packages of the export tools. pyelftools 0.29 still has py3compat.
if [ ! -x venv/bin/python ]; then
    uv venv -q -p 3.11 venv
    uv pip install -q --python venv/bin/python colorama pyserial prettytable jinja2 intelhex \
        junit-xml pyyaml requests future six beautifulsoup4 fuzzywuzzy jsonschema appdirs \
        'pyelftools==0.29' cmsis-pack-manager
    # mbed-os 5.8 imports Mapping from collections, which Python 3.10 removed
    sp=$(venv/bin/python -c 'import site; print(site.getsitepackages()[0])')
    cat > "$sp/sitecustomize.py" <<'PYEOF'
import collections, collections.abc
for name in ('Mapping', 'MutableMapping', 'Sequence', 'Iterable', 'Callable', 'Set', 'MutableSet'):
    if not hasattr(collections, name):
        setattr(collections, name, getattr(collections.abc, name))
PYEOF
fi
py="$work/venv/bin/python"

# One blobless clone, one worktree per tag
if [ ! -d mbed-git ]; then
    git clone -q --filter=blob:none --no-checkout https://github.com/ARMmbed/mbed-os.git mbed-git
fi
if [ ! -d "mbed-os-$tag" ]; then
    case "$tag" in
        *[!0-9a-f]*) ref="mbed-os-$tag" ;;   # a tag like 5.8.6
        *) ref="$tag" ;;                     # a commit hash (bisection)
    esac
    git -C mbed-git worktree add -q --detach "$work/mbed-os-$tag" "$ref"
    if [ -n "$TOOLS_FROM" ]; then
        # Tags before 5.8 have Python 2 tools. Use the (patched) tools of another tag instead; the
        # sources, targets.json and linker scripts still come from this tag.
        [ -d "mbed-os-$TOOLS_FROM" ] || { echo "run export_mbed.sh $TOOLS_FROM first"; exit 1; }
        rsync -a --delete "mbed-os-$TOOLS_FROM/tools/" "mbed-os-$tag/tools/"
    else
        git -C "mbed-os-$tag" apply "$here/py3.patch" || echo "WARNING: py3.patch does not apply to $tag, set TOOLS_FROM=5.8.6"
    fi
fi

# Program directory as mbed-cli lays it out: sources next to mbed-os
prog="$work/prog-$tag"
rm -rf "$prog" "export-$tag"
mkdir -p "$prog" "export-$tag"
rsync -a "$npr70/source" "$prog/"
ln -s "$work/mbed-os-$tag" "$prog/mbed-os"

cd "$prog"
PYTHONPATH=mbed-os "$py" -W ignore mbed-os/tools/project.py -m NUCLEO_L432KC -i gcc_arm \
    --source source --source mbed-os --build "$work/export-$tag" \
    --profile mbed-os/tools/profiles/debug.json > "$work/export-$tag.log" 2>&1 || {
    tail -15 "$work/export-$tag.log"; exit 1; }
ls -l "$work/export-$tag"
