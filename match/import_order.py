"""import_order.py -- the stand-in references that make the linked import table TI.EXE's.

The mechanism (measured with a logging MSVCRT40.DLL shim around LINK 4.20):
  * LINK sorts the .idata$5 (and .idata$4) contributions with ONE qsort (1996 MSVCRT40, the
    VC++ 4.2 CRT qsort.c algorithm), comparator strcmp of the import member's DLL name
    ("KERNEL32.dll", "USER32.dll", "GDI32.dll", "WINMM.dll", "comdlg32.dll" -- case-sensitive,
    so comdlg32 sorts last).
  * The array is every function member LINK loaded, in load order, plus each DLL's
    NULL_THUNK_DATA -- INCLUDING members /OPT:REF later discards.
  * The hint/name table (.idata$6) is NOT sorted: it is in load order. So TI.EXE's own
    hint/name table gives the original link's load order of every KEPT import.
  * Hence every IAT difference between an honest link of this source and TI.EXE comes from
    DISCARDED imports: references made only by code /OPT:REF dropped. They change the class
    sequence the qsort sees. DEAD_REFS realises one solution: insert_refs() adds them to the
    object copies (symbol-table insertion, relocation indices shifted; no code or data byte
    changes) and the linked IAT, ILT and hint table are then identical to TI.EXE's.
The import NAMES below are placeholders: only the DLL and the position in the undefined list
matter, and a discarded import leaves no trace in TI.EXE. One block of discarded code is no
longer a stand-in: container_store.c's display-attach loader thread (StartDisplayAttachLoaderThread ..
StopDisplayAttachLoaderThread, recovered from the March 1997 build) is compiled as source,
uncalled, and its own references (_beginthread, CreateEventA, ResetEvent, WaitForSingleObject,
Sleep) take the places the stand-ins for it held.
"""
import struct

# (object stem, function whose symbol-table group the references follow, references)
DEAD_REFS = [
    ('display_ui', 'Shutdown', ['__imp__GetTickCount@0', '__imp__KillTimer@8']),
    ('display_ui', 'QdLineTo', ['__imp__Ellipse@20']),
    ('display_ui', 'GetSceneCursorPos', ['__imp__GetFocus@0']),
    ('display_ui', 'RestoreMenuBar', ['__imp__CheckMenuItem@12']),
    ('container_store', 'FlushNodeFileBuffers', ['__imp__SetEndOfFile@4']),
    ('link_dead', 'dead_user', ['__imp__EnableMenuItem@12', '__imp__GetMenu@4', '__imp__IsIconic@4']),
]


def insert_refs(path, after_func, names):
    """Insert undefined externals at the end of `after_func`'s symbol-table group (before the
    next .text / .debug$F section symbol); relocation symbol indices shifted."""
    d = bytearray(open(path, 'rb').read())
    nsec = struct.unpack_from('<H', d, 2)[0]
    symptr, nsym = struct.unpack_from('<II', d, 8)
    strtab = symptr + nsym * 18
    strs = bytearray(d[strtab:strtab + struct.unpack_from('<I', d, strtab)[0]])

    def name_of(i):
        e = d[symptr + 18 * i: symptr + 18 * i + 18]
        if e[:4] == b'\0\0\0\0':
            off = struct.unpack_from('<I', e, 4)[0]
            return bytes(strs[off:strs.index(b'\0', off)]).decode('latin-1')
        return e[:8].rstrip(b'\0').decode('latin-1')
    i, at = 0, None
    while i < nsym and at is None:
        n = name_of(i)
        if n == '_' + after_func or n.startswith('_' + after_func + '@'):
            j = i + 1 + d[symptr + 18 * i + 17]
            while j < nsym and name_of(j) not in ('.text', '.debug$F'):
                j += 1 + d[symptr + 18 * j + 17]
            at = j
        i += 1 + d[symptr + 18 * i + 17]
    if at is None:
        raise SystemExit('%s: no function %s' % (path, after_func))
    recs = b''
    for n in names:
        raw = n.encode()
        if len(raw) <= 8:
            nm = raw.ljust(8, b'\0')
        else:
            nm = struct.pack('<II', 0, len(strs)); strs.extend(raw + b'\0')
        recs += struct.pack('<8sIhHBB', nm, 0, 0, 0x20, 2, 0)
    struct.pack_into('<I', strs, 0, len(strs))
    for s in range(nsec):
        rel = struct.unpack_from('<I', d, 20 + 40 * s + 24)[0]
        for r in range(struct.unpack_from('<H', d, 20 + 40 * s + 32)[0]):
            si = struct.unpack_from('<I', d, rel + 10 * r + 4)[0]
            if si >= at:
                struct.pack_into('<I', d, rel + 10 * r + 4, si + len(names))
    out = bytearray(d[:symptr + 18 * at] + recs + d[symptr + 18 * at:strtab] + strs)
    struct.pack_into('<I', out, 12, nsym + len(names))
    open(path, 'wb').write(out)
