"""Compare two saved games file by file, as data.

Usage: python3 -I tools/savediff.py SAVE_A SAVE_B

A save folder (mpsave/NNNNNNNNN-Name) holds the game file ICEWIND2.GAM,
the areas the party has seen in ICEWIND2.SAV, the world map WORLDMAP.WMP
and bitmaps. Two engines that saved the same game should write the same
data: replay crossload (tools/oracle.py) compares the save each engine made
with this before it loads them, so a field both our saver and our loader
get wrong, which the loaded states cannot show, still shows here.

- ICEWIND2.GAM byte by byte, except GAM_MASKED: 0x325-0x331, the original's
  uninitialised path fragment (stack bytes the original copies into the
  file; ours writes zeros there).
- ICEWIND2.SAV: SAV V1.0, a list of (name length, name, uncompressed size,
  compressed size, zlib data); each entry decompressed and compared byte by
  byte, and an area (.ARE, V9.1) also by its parts (are_summary): the
  counts of entrances, regions, containers, vertices and doors, each
  region's vertex count, and each door's vertex counts and bounding boxes.
- WORLDMAP.WMP byte by byte.
- The bitmaps (ICEWIND2.BMP, the screen shot, and PORTRT<n>.BMP) are what
  the renderer drew, not game state, and are not compared.

compare_saves returns (report lines, the number of files that differ); the
command prints the lines and exits 1 when any differs. Standard library
only.
"""

import os
import struct
import sys
import zlib

GAM = 'ICEWIND2.GAM'
SAV = 'ICEWIND2.SAV'
WMP = 'WORLDMAP.WMP'
GAM_MASKED = (0x325, 0x331)  # inclusive
SAV_SIGNATURE = b'SAV V1.0'
# A run of differing bytes ends after this many equal ones.
RUN_GAP = 4
RUNS_SHOWN = 8


class SaveError(Exception):
    pass


def read_sav(data):
    """{entry name: decompressed bytes} of a SAV V1.0 file's data."""
    if data[:8] != SAV_SIGNATURE:
        raise SaveError('not a SAV V1.0 file')
    pos, out = 8, {}
    try:
        while pos < len(data):
            n, = struct.unpack_from('<I', data, pos)
            pos += 4
            name = data[pos:pos + n].rstrip(b'\0').decode('latin-1')
            pos += n
            size, packed = struct.unpack_from('<II', data, pos)
            pos += 8
            raw = zlib.decompress(data[pos:pos + packed])
            pos += packed
            if len(raw) != size:
                raise SaveError('%s: %d bytes, the header says %d'
                                % (name, len(raw), size))
            out[name] = raw
    except (struct.error, zlib.error) as e:
        raise SaveError('truncated or corrupt at %d: %s' % (pos, e))
    return out


def mask_gam(data):
    """The game file with GAM_MASKED zeroed."""
    first, last = GAM_MASKED
    if len(data) <= first:
        return data
    end = min(last + 1, len(data))
    return data[:first] + bytes(end - first) + data[end:]


def byte_runs(a, b, gap=RUN_GAP):
    """[(first, last)] of the runs of differing bytes in a and b, over
    their common length (a run ends after gap equal bytes)."""
    runs = []
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            if runs and i <= runs[-1][1] + gap:
                runs[-1][1] = i
            else:
                runs.append([i, i])
    return [tuple(r) for r in runs]


def are_summary(data):
    """An area file's parts (ARE V9.1, IESDP: the V1.0 header with 16
    bytes more at 0x54): {'counts': {...}, 'regions': [(name, vertices)],
    'doors': [(name, open vertices, closed vertices, open box, closed
    box)]}; None for a file too short to hold them."""
    try:
        regions_count, = struct.unpack_from('<H', data, 0x6A)
        regions_offset, = struct.unpack_from('<I', data, 0x6C)
        entrances_offset, entrances_count = struct.unpack_from('<II', data,
                                                               0x78)
        containers_count, = struct.unpack_from('<H', data, 0x84)
        vertices_count, = struct.unpack_from('<H', data, 0x90)
        doors_count, doors_offset = struct.unpack_from('<II', data, 0xB4)
        regions = []
        for i in range(regions_count):
            o = regions_offset + i * 0xC4
            name = data[o:o + 32].rstrip(b'\0').decode('latin-1')
            count, = struct.unpack_from('<H', data, o + 0x2A)
            regions.append((name, count))
        doors = []
        for i in range(doors_count):
            o = doors_offset + i * 0xC8
            name = data[o:o + 32].rstrip(b'\0').decode('latin-1')
            opened, closed = struct.unpack_from('<HH', data, o + 0x30)
            boxes = struct.unpack_from('<8H', data, o + 0x38)
            doors.append((name, opened, closed, boxes[:4], boxes[4:]))
    except struct.error:
        return None
    return {'counts': {'entrances': entrances_count,
                       'regions': regions_count,
                       'containers': containers_count,
                       'vertices': vertices_count, 'doors': doors_count},
            'regions': regions, 'doors': doors}


def are_lines(a, b):
    """The lines naming how two area files' parts differ."""
    sa, sb = are_summary(a), are_summary(b)
    if sa is None or sb is None:
        return ['    the area header is truncated']
    lines = []
    for key in sa['counts']:
        if sa['counts'][key] != sb['counts'][key]:
            lines.append('    %s: %d, %d' % (key, sa['counts'][key],
                                             sb['counts'][key]))
    for x, y in zip(sa['regions'], sb['regions']):
        if x != y:
            lines.append('    region %s: %d vertices; %s%d' % (
                x[0], x[1], '' if x[0] == y[0] else y[0] + ': ', y[1]))
    for x, y in zip(sa['doors'], sb['doors']):
        if x != y:
            lines.append('    door %s: vertices %d open, %d closed, boxes %s '
                         '%s; %s%d, %d, %s %s' % (
                             x[0], x[1], x[2], x[3], x[4],
                             '' if x[0] == y[0] else y[0] + ': ',
                             y[1], y[2], y[3], y[4]))
    return lines


def data_lines(name, a, b):
    """'' when a and b are equal, else the lines describing how."""
    if a == b:
        return []
    runs = byte_runs(a, b)
    lines = ['  %s: DIFFERENT (%d and %d bytes, %d run%s of differing bytes)'
             % (name, len(a), len(b), len(runs), '' if len(runs) == 1
                else 's')]
    for first, last in runs[:RUNS_SHOWN]:
        lines.append('    0x%04X-0x%04X: %s / %s' % (
            first, last, a[first:last + 1][:16].hex(),
            b[first:last + 1][:16].hex()))
    if len(runs) > RUNS_SHOWN:
        lines.append('    ... %d more' % (len(runs) - RUNS_SHOWN))
    if name.upper().endswith('.ARE'):
        lines += are_lines(a, b)
    return lines


def file_named(folder, name):
    for entry in os.listdir(folder):
        if entry.lower() == name.lower():
            return os.path.join(folder, entry)
    return None


def read_file(folder, name):
    path = file_named(folder, name)
    if path is None:
        return None
    with open(path, 'rb') as f:
        return f.read()


def compare_saves(a, b):
    """(lines, files that differ) for the save folders a and b."""
    lines = []
    different = 0

    def missing(name, x, y):
        nonlocal different
        if x is None and y is None:
            return True
        if x is None or y is None:
            lines.append('  %s: MISSING in %s' % (name, a if x is None
                                                  else b))
            different += 1
            return True
        return False

    gam_a, gam_b = read_file(a, GAM), read_file(b, GAM)
    if not missing(GAM, gam_a, gam_b):
        found = data_lines(GAM, mask_gam(gam_a), mask_gam(gam_b))
        different += bool(found)
        lines += found or ['  %s: same (0x%X-0x%X masked)' % (
            (GAM,) + GAM_MASKED)]
    sav_a, sav_b = read_file(a, SAV), read_file(b, SAV)
    if not missing(SAV, sav_a, sav_b):
        try:
            entries_a, entries_b = read_sav(sav_a), read_sav(sav_b)
        except SaveError as e:
            lines.append('  %s: unreadable: %s' % (SAV, e))
            different += 1
            entries_a = entries_b = {}
        for name in sorted(set(entries_a) | set(entries_b)):
            label = '%s:%s' % (SAV, name)
            x, y = entries_a.get(name), entries_b.get(name)
            if missing(label, x, y):
                continue
            found = data_lines(label, x, y)
            different += bool(found)
            lines += found or ['  %s: same' % label]
    wmp_a, wmp_b = read_file(a, WMP), read_file(b, WMP)
    if not missing(WMP, wmp_a, wmp_b):
        found = data_lines(WMP, wmp_a, wmp_b)
        different += bool(found)
        lines += found or ['  %s: same' % WMP]
    return lines, different


def main(argv=None):
    args = sys.argv[1:] if argv is None else argv
    if len(args) != 2:
        print(__doc__.split('\n\n')[1])
        return 2
    for folder in args:
        if not os.path.isdir(folder):
            print('savediff: %s is not a folder' % folder)
            return 2
    lines, different = compare_saves(*args)
    for line in lines:
        print(line)
    print('savediff: %s' % ('%d file%s' % (
        different, ' differs' if different == 1 else 's differ')
        if different else 'the saves hold the same data'))
    return 1 if different else 0


if __name__ == '__main__':
    sys.exit(main())
