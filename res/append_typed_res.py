#!/usr/bin/env python
"""
append_typed_res.py -- add res/typed/*.bin to a compiled .res file, byte for byte.

    python res/append_typed_res.py in.res out.res [--order-file res/data_order.json]

res/typed/<type>_<name>.bin are TI.EXE's icons, menu, dialogs and accelerator table, carved
out of the original. Neither windres nor Visual C++ 4.2's rc.exe accepts raw bytes for a
predefined resource type, but a .res file is just a sequence of entries, so each one is appended
here and both linkers take the result. Language 0x0409, as in TI.EXE. Used by the Makefile (the
playable port) and by match/build.py.

--order-file (the match build): reorder the records into the order the original exe's resource
DATA blocks are in (res/data_order.json, a list of [type, name]). cvtres lays the data out in
.res order, and TI.EXE's is its .rc source order (icons, two cursors, the menu, accelerators,
dialogs, the other cursors, the RAW blocks, the string table), where ours is rc.exe's output
with the typed resources appended.
"""
import glob
import json
import os
import struct
import sys

TYPED = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'typed')


def ident(v):
    """A .res TYPE/NAME field: 0xFFFF + ordinal, or a zero-terminated UTF-16 name."""
    if v.isdigit():
        return struct.pack('<HH', 0xffff, int(v))
    return (v.upper() + '\0').encode('utf-16le')


def entry(typ, name, data):
    head = ident(typ) + ident(name)
    if len(head) % 4:
        head += b'\0' * (4 - len(head) % 4)
    head += struct.pack('<IHHII', 0, 0x1030, 0x0409, 0, 0)  # version, MOVEABLE|PURE|DISCARDABLE, lang
    rec = struct.pack('<II', len(data), 8 + len(head)) + head + data
    return rec + b'\0' * (-len(rec) % 4)


def records(buf):
    """[(key, bytes)] of a .res file; key = (type, name) as ints or upper-case strings."""
    out, off = [], 0
    while off + 8 <= len(buf):
        dsize, hsize = struct.unpack_from('<II', buf, off)
        size = 8 + (hsize - 8) + dsize
        size += -size % 4
        p = off + 8
        key = []
        for _ in range(2):
            if struct.unpack_from('<H', buf, p)[0] == 0xffff:
                key.append(struct.unpack_from('<H', buf, p + 2)[0]); p += 4
            else:
                e = p
                while struct.unpack_from('<H', buf, e)[0]:
                    e += 2
                key.append(buf[p:e].decode('utf-16le').upper()); p = e + 2
        out.append((tuple(key), bytes(buf[off:off + size])))
        off += size
    return out


def main():
    src, dst = sys.argv[1], sys.argv[2]
    out = bytearray(open(src, 'rb').read())
    out += b'\0' * (-len(out) % 4)
    files = sorted(glob.glob(os.path.join(TYPED, '*.bin')),
                   key=lambda p: (int(os.path.basename(p).split('_', 1)[0]), p))
    for p in files:
        typ, name = os.path.basename(p)[:-4].split('_', 1)
        out += entry(typ, name, open(p, 'rb').read())
    if '--order-file' in sys.argv:
        order = [tuple(k) for k in json.load(open(sys.argv[sys.argv.index('--order-file') + 1]))]
        rank = {k: i for i, k in enumerate(order)}
        recs = records(out)
        head, rest = recs[:1], recs[1:]             # the empty header record stays first
        rest.sort(key=lambda r: rank.get(r[0], len(rank)))
        out = bytearray(b''.join(r for _, r in head + rest))
    open(dst, 'wb').write(bytes(out))


if __name__ == '__main__':
    main()
