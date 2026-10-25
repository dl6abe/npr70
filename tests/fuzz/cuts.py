"""Cuts parts out of the firmware sources for the fuzz targets (tools/fuzz.sh). Each cut starts with a `#line` mark
with the real path of the source, so that the coverage tools name the real file and the real line numbers."""
import os


def load(srcdir, name):
    path = os.path.realpath(os.path.join(srcdir, name))
    return open(path, newline='').read().replace('\r\n', '\n').split('\n'), path


def cut(src, start, end=None):
    """The lines from the first line that starts with `start` up to (not including) the next one that starts with `end`
    (to the end of the file when `end` is None)."""
    lines, path = src
    a = next(i for i, l in enumerate(lines) if l.lstrip().startswith(start))
    b = next(i for i in range(a + 1, len(lines)) if lines[i].lstrip().startswith(end)) if end else len(lines)
    return '#line %d "%s"\n' % (a + 1, path) + '\n'.join(lines[a:b]) + '\n'
