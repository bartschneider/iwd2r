"""A private, per-run view of a game install, so that engines running at the
same time do not share the folders the engine writes.

The engine clears and writes temp/, tempsave/, cache/ and the save folders
under its data folder and rewrites the INI files, as the original does; two
engines on one install trip over each other (the second asserts "Problems
removing files from Temp directory"). make_view(data, view) builds a folder
that looks like the install: a link to every top-level entry, except the ones
tools/dataview_rules.json lists as written by the engine, which are real and
private to the view:

  empty_dirs    created empty (temp, tempsave, cache, save, ScrnShot)
  copied_dirs   copied from the install (mpsave: the load-game list reads the
                saves there and the engine deletes mpsave/default at start;
                Characters: the character list reads it and saving writes it)
  copied_files  patterns of files copied (*.ini)

Links: symbolic links when view is on a Linux file system. On the Windows
drives (/mnt/c/... from WSL) a Linux symlink is not seen by a Windows
engine, so folders become NTFS junctions (cmd.exe mklink /J, which works
from WSL on drvfs and needs no privilege) and files hard links (same volume),
else copies. A junction or hard link must not be written through; the engine
writes only the entries that are real in the view.

make_view is idempotent. remove_view deletes a view without following its
links into the install. Standard library only.
"""

import fnmatch
import json
import os
import shutil
import subprocess

RULES_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          'dataview_rules.json')


def load_rules(path=RULES_PATH):
    with open(path) as f:
        rules = json.load(f)
    return {key: list(rules.get(key, []))
            for key in ('empty_dirs', 'copied_dirs', 'copied_files')}


def on_windows_drive(path):
    """True for a /mnt/<letter>/ path (a Windows drive seen from WSL)."""
    parts = os.path.abspath(path).split(os.sep)
    return (len(parts) > 2 and parts[1] == 'mnt' and len(parts[2]) == 1
            and parts[2].isalpha())


def _wslpath_w(path):
    return subprocess.run(['wslpath', '-w', path], check=True,
                          capture_output=True, text=True).stdout.strip()


def _junction(target, link):
    proc = subprocess.run(
        ['cmd.exe', '/c', 'mklink', '/J', _wslpath_w(link),
         _wslpath_w(target)], cwd='/mnt/c', capture_output=True, text=True)
    if proc.returncode != 0:
        raise OSError('mklink /J %s failed: %s' % (link, proc.stdout.strip()))


def _inside(path, folder):
    path = os.path.realpath(path)
    folder = os.path.realpath(folder)
    return path == folder or path.startswith(folder + os.sep)


def _link(src, dst, windows):
    if not windows:
        os.symlink(src, dst)
    elif os.path.isdir(src):
        _junction(src, dst)
    else:
        try:
            os.link(src, dst)
        except OSError:
            shutil.copy2(src, dst)


def make_view(data_dir, view_dir, rules=None, windows=None):
    """Creates (or completes) view_dir as a private view of data_dir and
    returns view_dir. windows forces (True) or forbids (False) the Windows
    link kinds; the default picks them when view_dir is on a Windows drive."""
    data_dir = os.path.abspath(data_dir)
    view_dir = os.path.abspath(view_dir)
    if _inside(view_dir, data_dir):
        raise ValueError('the view %s is inside the data folder %s' %
                         (view_dir, data_dir))
    if _inside(data_dir, view_dir):
        raise ValueError('the data folder %s is inside the view %s' %
                         (data_dir, view_dir))
    if rules is None:
        rules = load_rules()
    if windows is None:
        windows = on_windows_drive(view_dir)
    empty = {n.lower() for n in rules['empty_dirs']}
    copied_dirs = {n.lower() for n in rules['copied_dirs']}
    patterns = [p.lower() for p in rules['copied_files']]
    os.makedirs(view_dir, exist_ok=True)
    present = set()
    for name in sorted(os.listdir(data_dir)):
        src = os.path.join(data_dir, name)
        dst = os.path.join(view_dir, name)
        low = name.lower()
        present.add(low)
        if os.path.lexists(dst):
            continue
        if low in empty and os.path.isdir(src):
            os.mkdir(dst)
        elif low in copied_dirs and os.path.isdir(src):
            shutil.copytree(src, dst, symlinks=True)
        elif os.path.isfile(src) and any(
                fnmatch.fnmatchcase(low, p) for p in patterns):
            shutil.copy2(src, dst)
        else:
            _link(src, dst, windows)
    # Writable folders the install lacks still exist in the view, empty
    # (an install made without saves has no mpsave, and New Game's
    # LoadGame asserts when it cannot copy mpsave/default into temp).
    for name in rules['empty_dirs'] + rules['copied_dirs']:
        if name.lower() not in present:
            os.makedirs(os.path.join(view_dir, name), exist_ok=True)
    return view_dir


def remove_view(view_dir):
    """Deletes a view made by make_view without following its links into
    the install (symlinks, junctions and hard links are removed as entries;
    a hard link removes only the view's name)."""
    if not os.path.lexists(view_dir):
        return
    if os.path.islink(view_dir) or not os.path.isdir(view_dir):
        os.unlink(view_dir)
        return
    for name in os.listdir(view_dir):
        path = os.path.join(view_dir, name)
        if os.path.islink(path) or not os.path.isdir(path):
            os.unlink(path)
        elif _is_junction(path):
            os.rmdir(path)
        else:
            shutil.rmtree(path)
    os.rmdir(view_dir)


def _is_junction(path):
    """A junction made on a Windows drive shows through drvfs as a
    symlink in WSL (islink is already true); this catches the same on
    interpreters that report it separately."""
    isj = getattr(os.path, 'isjunction', None)
    return bool(isj and isj(path))
