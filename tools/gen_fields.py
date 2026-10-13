"""Generate oracle/replay/fields.json, the replay dump's field table.

Usage: python3 -I tools/gen_fields.py [--check] [--schema PATH] [--src DIR]
                                      [--vanilla PATH] [--out PATH]

The dump schema (oracle/replay/schema.json) names where each value lives
under "x-fields", one entry per field:

  {"class": "CChitin", "member": "pActiveEngine"}
      a member of a class; "member" may be a path through members whose
      type is another class ("m_cObjectArray.m_pArray")
  {"class": "CGameObjectArrayEntry", "sizeof": true}
      the size of a class: the end of its last commented member, rounded up
      to 4 bytes when any member is 4 bytes or wider. Only right for a
      class whose every member carries an offset comment.
  {"global": "g_pBaldurChitin", "type": "ptr"}
      a global variable, whose address comes from oracle/loader/vanilla.json
      ("globals").

Offsets come from the layout comments in our headers under --src (default
src/): a member line starts with `/* NNNN */` or `/* 0xNNNN */`, the offset
from the start of the object, base classes included. The member's declared
type gives the field type: u8, i8, u16, i16, u32, i32, ptr (any pointer),
resref (RESREF, 8 bytes); an array member adds "count". Methods (anything
with a parenthesis) are ignored.

fields.json maps each field to {"base", "offset", "type"} (plus "count"):
base is the class name, or the global's address for a global; offset is a
hex string. oracle/CMakeLists.txt turns it into the recorder DLL's header,
so the DLL holds no offset of its own.

--check regenerates the table in memory and compares it with --out; it
prints a one-line reason and exits 1 on a difference, else 0.

Standard library only.
"""

import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SCHEMA = os.path.join(ROOT, 'oracle', 'replay', 'schema.json')
FIELDS = os.path.join(ROOT, 'oracle', 'replay', 'fields.json')
VANILLA = os.path.join(ROOT, 'oracle', 'loader', 'vanilla.json')
SRC = os.path.join(ROOT, 'src')

# Declared type -> (field type, size in bytes).
SCALARS = {
    'BYTE': ('u8', 1), 'BOOLEAN': ('u8', 1), 'unsigned char': ('u8', 1),
    'UCHAR': ('u8', 1), 'uint8_t': ('u8', 1),
    'char': ('i8', 1), 'signed char': ('i8', 1), 'CHAR': ('i8', 1),
    'SHORT': ('i16', 2), 'short': ('i16', 2),
    'WORD': ('u16', 2), 'USHORT': ('u16', 2), 'unsigned short': ('u16', 2),
    'INT': ('i32', 4), 'int': ('i32', 4), 'LONG': ('i32', 4),
    'long': ('i32', 4), 'BOOL': ('i32', 4),
    'DWORD': ('u32', 4), 'UINT': ('u32', 4), 'unsigned int': ('u32', 4),
    'ULONG': ('u32', 4), 'STRREF': ('u32', 4), 'COLORREF': ('u32', 4),
    'RESREF': ('resref', 8),
}
PTR_SIZE = 4  # the original is 32-bit

MEMBER_LINE = re.compile(
    r'^\s*/\*\s*(?:0x)?([0-9A-Fa-f]+)\s*\*/\s*([^;]*);')
DECL = re.compile(r'^(.*?)\b([A-Za-z_]\w*)\s*((?:\[[^\]]*\]\s*)*)$')
DEFINE = re.compile(r'^\s*#define\s+([A-Za-z_]\w*)\s+(\d+)\s*$', re.M)


class FieldError(Exception):
    pass


class Member:
    def __init__(self, offset, type_name, name, count, line):
        self.offset = offset
        self.type_name = type_name
        self.name = name
        self.count = count
        self.line = line

    def field_type(self):
        """(field type, size of one element) or (None, None) for a class
        or an unknown type."""
        if '*' in self.type_name:
            return 'ptr', PTR_SIZE
        return SCALARS.get(self.type_name, (None, None))


class Headers:
    """The class layouts in a directory of headers, read on demand."""

    def __init__(self, src):
        self.texts = {}
        for dirpath, _, filenames in os.walk(src):
            for name in sorted(filenames):
                if name.endswith('.h'):
                    path = os.path.join(dirpath, name)
                    with open(path, encoding='utf-8', errors='replace') as f:
                        self.texts[os.path.relpath(path, src)] = f.read()
        self.defines = {}
        for text in self.texts.values():
            for name, value in DEFINE.findall(text):
                self.defines[name] = int(value)
        self.classes = {}

    def _dimension(self, text, where):
        text = text.strip()
        if text.isdigit():
            return int(text)
        if text in self.defines:
            return self.defines[text]
        raise FieldError('%s: array size %s is not a number or a #define'
                         % (where, text))

    def members(self, name):
        """{member name: Member} of class name."""
        if name in self.classes:
            return self.classes[name]
        found = None
        pattern = re.compile(r'\b(?:class|struct)\s+%s\b[^;{()]*\{'
                             % re.escape(name))
        for path, text in sorted(self.texts.items()):
            m = pattern.search(text)
            if m:
                found = (path, text, m.end())
                break
        if found is None:
            raise FieldError('no class %s in the headers' % name)
        path, text, start = found
        depth, i = 1, start
        while depth and i < len(text):
            depth += {'{': 1, '}': -1}.get(text[i], 0)
            i += 1
        body = text[start:i - 1]
        first_line = text.count('\n', 0, start) + 1
        members = {}
        for n, line in enumerate(body.split('\n')):
            m = MEMBER_LINE.match(line.split('//')[0])
            if not m or '(' in m.group(2):
                continue
            where = '%s:%d' % (path, first_line + n)
            decl = DECL.match(m.group(2).strip())
            if not decl or not decl.group(1).strip():
                continue
            type_name = ' '.join(decl.group(1).split())
            count = None
            if decl.group(3):
                count = 1
                for dim in re.findall(r'\[([^\]]*)\]', decl.group(3)):
                    count *= self._dimension(dim, where)
            members[decl.group(2)] = Member(int(m.group(1), 16), type_name,
                                            decl.group(2), count, where)
        if not members:
            raise FieldError('class %s (%s) has no /* NNNN */ member '
                             'comments' % (name, path))
        self.classes[name] = members
        return members

    def resolve(self, class_name, path):
        """(offset, Member) of a member path in class_name."""
        offset, current, member = 0, class_name, None
        parts = path.split('.')
        for i, part in enumerate(parts):
            members = self.members(current)
            if part not in members:
                raise FieldError('class %s has no commented member %s'
                                 % (current, part))
            member = members[part]
            offset += member.offset
            if i + 1 < len(parts):
                if member.field_type()[0] is not None or member.count:
                    raise FieldError('%s.%s is not a class member (%s)'
                                     % (current, part, member.type_name))
                current = member.type_name
        return offset, member

    def size_of(self, class_name):
        end, widest = 0, 1
        for member in self.members(class_name).values():
            kind, size = member.field_type()
            if kind is None:
                raise FieldError('sizeof %s: member %s has a class type (%s)'
                                 % (class_name, member.name,
                                    member.type_name))
            end = max(end, member.offset + size * (member.count or 1))
            widest = max(widest, min(size, 4))
        return (end + widest - 1) // widest * widest


def generate(schema, headers, globals_):
    """The fields.json dict for a parsed schema."""
    specs = schema.get('x-fields')
    if not isinstance(specs, dict) or not specs:
        raise FieldError('the schema has no "x-fields"')
    out = {}
    for field, spec in specs.items():
        try:
            if 'global' in spec:
                name = spec['global']
                if name not in globals_:
                    raise FieldError('no address for global %s in '
                                     'vanilla.json "globals"' % name)
                if spec.get('type') not in ('u8', 'i8', 'u16', 'i16', 'u32',
                                            'i32', 'ptr'):
                    raise FieldError('global %s needs a "type"' % name)
                entry = {'base': '0x%X' % int(globals_[name], 16),
                         'offset': '0x0000', 'type': spec['type']}
            elif spec.get('sizeof'):
                entry = {'base': spec['class'],
                         'offset': '0x%04X' % headers.size_of(spec['class']),
                         'type': 'size'}
            else:
                offset, member = headers.resolve(spec['class'],
                                                 spec['member'])
                kind = member.field_type()[0]
                if kind is None:
                    raise FieldError('%s.%s has type %s, not a scalar, '
                                     'pointer or RESREF'
                                     % (spec['class'], spec['member'],
                                        member.type_name))
                entry = {'base': spec['class'], 'offset': '0x%04X' % offset,
                         'type': kind}
                if member.count:
                    entry['count'] = member.count
        except (KeyError, TypeError) as e:
            raise FieldError('field %s: malformed spec %r (%s)'
                             % (field, spec, e))
        except FieldError as e:
            raise FieldError('field %s: %s' % (field, e))
        out[field] = entry
    return out


def render(fields):
    return json.dumps(fields, indent=2) + '\n'


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--check', action='store_true',
                    help='compare with --out instead of writing it')
    ap.add_argument('--schema', default=SCHEMA)
    ap.add_argument('--src', default=SRC)
    ap.add_argument('--vanilla', default=VANILLA)
    ap.add_argument('--out', default=FIELDS)
    args = ap.parse_args(argv)
    with open(args.schema) as f:
        schema = json.load(f)
    with open(args.vanilla) as f:
        globals_ = json.load(f).get('globals', {})
    try:
        text = render(generate(schema, Headers(args.src), globals_))
    except FieldError as e:
        print('gen_fields: %s' % e)
        return 1
    if args.check:
        try:
            with open(args.out) as f:
                current = f.read()
        except OSError as e:
            print('gen_fields: cannot read %s: %s' % (args.out, e))
            return 1
        if current != text:
            print('gen_fields: %s is out of date; run tools/gen_fields.py'
                  % os.path.relpath(args.out, ROOT))
            return 1
        return 0
    with open(args.out, 'w') as f:
        f.write(text)
    print('wrote %s (%d fields)' % (os.path.relpath(args.out, ROOT),
                                    len(json.loads(text))))
    return 0


if __name__ == '__main__':
    sys.exit(main())
