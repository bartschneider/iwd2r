"""Extract game resources from Override or the KEY/BIFF archives.

    python3 tools/bif.py <game dir> <out dir> XPLEVEL.2DA [NAME.EXT ...]

Writes each named resource to <out dir>/<NAME.EXT>. A file of that name in
the game's Override folder (any case) wins; otherwise the resource is looked
up in CHITIN.KEY and read from its BIFF. The formats are IESDP's: KEY V1
(bif entries of 12 bytes, resource entries of 14 bytes whose locator holds
the bif index in bits 20-31 and the file index in bits 0-13) and BIFF V1
(file entries of 16 bytes). Compressed archives (BIFC, BIF V1.0) are refused.

Standard library only. The extracted text is game data: never commit it.
"""

import os
import struct
import sys

# Resource types by extension (IESDP); add more as cases need them.
TYPES = {'2DA': 0x3F4, 'IDS': 0x3F0, 'BCS': 0x3EF, 'INI': 0x802}


class BifError(Exception):
    pass


def parse_key(data):
    """(bif names, {(RESREF, type): (bif index, file index)}) of a KEY V1."""
    if data[:8] != b'KEY V1  ':
        raise BifError('not a KEY V1 file')
    if len(data) < 24:
        raise BifError('KEY header truncated')
    nbif, nres, obif, ores = struct.unpack_from('<4I', data, 8)
    if obif + 12 * nbif > len(data) or ores + 14 * nres > len(data):
        raise BifError('KEY entries run past the end of the file')
    bifs = []
    for i in range(nbif):
        _, name_off, name_len, _ = struct.unpack_from('<IIHH', data,
                                                      obif + 12 * i)
        if name_off + name_len > len(data):
            raise BifError('KEY bif %d name (offset %d, length %d) runs past '
                           'the end of the file' % (i, name_off, name_len))
        name = data[name_off:name_off + name_len].split(b'\0', 1)[0]
        bifs.append(name.decode('latin-1').replace('\\', '/'))
    resources = {}
    for i in range(nres):
        raw, rtype, locator = struct.unpack_from('<8sHI', data, ores + 14 * i)
        resref = raw.split(b'\0', 1)[0].decode('latin-1').upper()
        resources[(resref, rtype)] = (locator >> 20, locator & 0x3FFF)
    return bifs, resources


def read_biff(data, file_index):
    """The bytes of file `file_index` in a BIFF V1 archive."""
    if data[:8] != b'BIFFV1  ':
        raise BifError('not a BIFF V1 archive (%r)' % data[:8])
    if len(data) < 20:
        raise BifError('BIFF header truncated')
    nfiles, _, offset = struct.unpack_from('<3I', data, 8)
    if offset + 16 * nfiles > len(data):
        raise BifError('BIFF file entries run past the end of the archive')
    for i in range(nfiles):
        locator, start, size, _ = struct.unpack_from('<IIIH', data,
                                                     offset + 16 * i)
        if locator & 0x3FFF == file_index:
            if start + size > len(data):
                raise BifError('file %d runs past the end of the archive'
                               % file_index)
            return data[start:start + size]
    raise BifError('no file %d in the archive' % file_index)


def find_ci(root, rel):
    """root/rel matched case-insensitively component by component, or
    None."""
    path = root
    for part in rel.split('/'):
        if not part:
            continue
        try:
            names = os.listdir(path)
        except OSError:
            return None
        match = [n for n in names if n.lower() == part.lower()]
        if not match:
            return None
        path = os.path.join(path, match[0])
    return path


def bif_path(game, name):
    """The archive named in the KEY: under the game folder, or under one of
    its CD<n> folders."""
    found = find_ci(game, name)
    if found:
        return found
    for entry in sorted(os.listdir(game)):
        if entry.lower().startswith('cd') and \
                os.path.isdir(os.path.join(game, entry)):
            found = find_ci(os.path.join(game, entry), name)
            if found:
                return found
    raise BifError('archive %s not found under %s' % (name, game))


def extract(game, filename):
    """The bytes of resource NAME.EXT: Override first, then the KEY/BIFF."""
    stem, _, ext = filename.upper().rpartition('.')
    if not stem or ext not in TYPES:
        raise BifError('unknown resource type: %s' % filename)
    override = find_ci(game, 'override/' + filename)
    if override and os.path.isfile(override):
        with open(override, 'rb') as f:
            return f.read()
    key = find_ci(game, 'chitin.key')
    if not key:
        raise BifError('no CHITIN.KEY in %s' % game)
    with open(key, 'rb') as f:
        bifs, resources = parse_key(f.read())
    entry = resources.get((stem, TYPES[ext]))
    if entry is None:
        raise BifError('%s is not in CHITIN.KEY' % filename)
    bif, index = entry
    if bif >= len(bifs):
        raise BifError('%s names archive %d of %d' % (filename, bif,
                                                       len(bifs)))
    with open(bif_path(game, bifs[bif]), 'rb') as f:
        return read_biff(f.read(), index)


def extract_to(game, out_dir, names):
    """Write each resource to out_dir/<NAME>; returns the paths."""
    os.makedirs(out_dir, exist_ok=True)
    paths = []
    for name in names:
        data = extract(game, name)
        path = os.path.join(out_dir, name.upper())
        with open(path + '.tmp', 'wb') as f:
            f.write(data)
        os.replace(path + '.tmp', path)
        paths.append(path)
    return paths


def main(argv=None):
    args = sys.argv[1:] if argv is None else argv
    if len(args) < 3:
        sys.exit(__doc__.strip())
    try:
        for path in extract_to(args[0], args[1], args[2:]):
            print(path)
    except BifError as e:
        sys.exit('bif: %s' % e)
    return 0


if __name__ == '__main__':
    sys.exit(main())
