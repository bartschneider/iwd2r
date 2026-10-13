# iwd2r. Distributed under LICENSE.md.
"""Write the minimal PE32 that tests/test_pe.cpp parses.

Usage: python3 tests/make_test_pe.py OUTPUT

Image base 0x10000000, entry RVA 0x1000, SizeOfImage 0x3000, SizeOfHeaders
0x200. Two sections:

  .text   RVA 0x1000, virtual size 0x10,  raw 0x200 at 0x200, 0x60000020
  .idata  RVA 0x2000, virtual size 0x100, raw 0x200 at 0x400, 0xC0000040

Two imports: KERNEL32.dll!GetTickCount by name (hint 0x1F5, IAT RVA 0x2048)
and WS2_32.dll ordinal 23 (IAT RVA 0x2058). test_pe.cpp checks these values;
change both together.
"""

import struct
import sys


def build():
    out = bytearray(0x600)
    # DOS header: MZ and e_lfanew.
    out[0:2] = b'MZ'
    struct.pack_into('<I', out, 0x3C, 0x40)
    # PE signature and file header: i386, 2 sections, 0xE0-byte optional header.
    pe = 0x40
    out[pe:pe + 4] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', out, pe + 4, 0x14C, 2, 0, 0, 0, 0xE0, 0x0102)
    # PE32 optional header.
    opt = pe + 24
    struct.pack_into('<HBBIIIIII', out, opt, 0x10B, 6, 0, 0x200, 0x200, 0,
                     0x1000, 0x1000, 0x2000)
    struct.pack_into('<III', out, opt + 28, 0x10000000, 0x1000, 0x200)
    struct.pack_into('<HHHHHH', out, opt + 40, 4, 0, 0, 0, 4, 0)
    struct.pack_into('<IIII', out, opt + 56, 0x3000, 0x200, 0, 2)
    struct.pack_into('<IIIIII', out, opt + 72, 0x100000, 0x1000, 0x100000,
                     0x1000, 0, 16)
    struct.pack_into('<II', out, opt + 96 + 8, 0x2000, 60)  # import directory
    # Section table.
    table = opt + 0xE0
    for i, (name, vsize, rva, raw, off, flags) in enumerate((
            (b'.text', 0x10, 0x1000, 0x200, 0x200, 0x60000020),
            (b'.idata', 0x100, 0x2000, 0x200, 0x400, 0xC0000040))):
        struct.pack_into('<8sIIII12xI', out, table + 40 * i, name, vsize, rva,
                         raw, off, flags)
    out[0x200] = 0xC3  # ret
    # .idata: RVA r is at file offset r - 0x2000 + 0x400.
    def at(rva):
        return rva - 0x2000 + 0x400
    struct.pack_into('<IIIII', out, at(0x2000), 0x2040, 0, 0, 0x2080, 0x2048)
    struct.pack_into('<IIIII', out, at(0x2014), 0x2050, 0, 0, 0x2090, 0x2058)
    struct.pack_into('<II', out, at(0x2040), 0x20A0, 0)
    struct.pack_into('<II', out, at(0x2048), 0x20A0, 0)
    struct.pack_into('<II', out, at(0x2050), 0x80000017, 0)
    struct.pack_into('<II', out, at(0x2058), 0x80000017, 0)
    out[at(0x2080):at(0x2080) + 13] = b'KERNEL32.dll\0'
    out[at(0x2090):at(0x2090) + 11] = b'WS2_32.dll\0'
    struct.pack_into('<H', out, at(0x20A0), 0x1F5)
    out[at(0x20A2):at(0x20A2) + 13] = b'GetTickCount\0'
    return bytes(out)


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__.strip())
    with open(argv[1], 'wb') as f:
        f.write(build())
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
