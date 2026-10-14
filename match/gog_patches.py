#!/usr/bin/env python
"""
gog_patches.py -- the bytes GOG (or a no-CD crack before it) hex-edited in TI.EXE.

The source in this tree compiles to CyberFlix's 31 December 1996 build as released. GOG ships
that build with a few bytes patched afterwards so it runs without the CD and leaves the display
mode alone on exit: 25 bytes at four sites. No compiler emits those bytes, so build.py links the
original and then writes the patches into a copy (TI_gog.exe), each site found through its
function's address in the link map.

The sites were found by disassembling GOG's exe against the retail disc's March 1997 build (a
separate VC++ 4.1 compile of nearly the same source), where the original conditional jumps are
still in place, and confirmed by a whole-file diff against an unpatched copy of the same
December build (MD5 533bff3dec795dc9e8ff1bccf983bfb4, from the GOG installer base): exactly
these 25 bytes differ. The first two rows are a conditional jump turned into a jump or NOPs;
the other two replace, in each CLUT fade function, a spin-wait on the 60 Hz tick with a push and
a call.

    python match/gog_patches.py     # check that TITANIC_EXE carries exactly these patches
                                    # relative to TITANIC_EXE_UNPATCHED
"""
import os
import struct
import sys
import time

# (exe address, function, offset in function, original bytes, patched bytes, reason / evidence)
PATCHES = [
    (0x0041dec1, 'FreeDisplayLibrary', 0x11, b'\x74\x0b', b'\xeb\x0b',
     'JZ -> JMP over `call RestoreDisplayMode` (if (DAT_00486bf0) RestoreDisplayMode();): '
     'the display mode is left as is on exit. March 1997 0x0041d631 has `74 0b` in the same '
     'sequence (cmp [0x4874e0],0 / je / call 0x41aff0).'),
    (0x00432e59, 'PromptUntilDescriptorVolumePresent', 0x39, b'\x74\x1a', b'\x90\x90',
     'JZ to the "insert the CD" prompt loop (0x00432e75) NOPed: a volume reported absent '
     'is treated as present. March 1997 0x00433a29 has `74 1a` (je 0x433a45) after the same '
     'cmp word [esp+0x12],0; the loop that follows is byte-for-byte the same shape.'),
    (0x00435732, 'FadeCLUTToBlack', 0x32,
     b'\x83\xc4\x10\xe8\xd6\x87\xfe\xff\x3b\xc3\x7c\xf7\x66',
     b'\x83\xc4\x14\x68\x80\xa8\x48\x00\xe8\x61\xb1\x00\x00',
     'The per-step wait `add esp,10 / call GetTick60Hz / cmp eax,ebx / jl back` (spin until '
     'the next 60 Hz tick; ebx is the deadline, incremented once per step at 0x0043571f) '
     'became `add esp,14 / push 0x48a880 / call PresentBackBufferRect` (0x004408a0). '
     'The 0x66 prefix of the following `inc di` is consumed by the call rel32, so it becomes '
     '`inc edi` (same effect). The `add esp,14` pre-pays the push, so esp balances, but the '
     'push lands on the saved-EBP slot: the function returns with EBP = 0x0048a880. '
     'Unpatched original: TI_unpatched.exe (the same build from another source).'),
    (0x00435782, 'FadeCLUTFromBlack', 0x32,
     b'\x83\xc4\x10\xe8\x86\x87\xfe\xff\x3b\xc5\x7c\xf7\x66',
     b'\x83\xc4\x14\x68\x80\xa8\x48\x00\xe8\x11\xb1\x00\x00',
     'Same patch as the Reverse twin above (deadline in ebp here: `cmp eax,ebp`).'),
]

def unpatch(text, text_va):
    """`text` (bytes of TI.EXE's .text, loaded at text_va) with every patch reverted to the
    original bytes. A site that does not hold the patched bytes is left alone."""
    b = bytearray(text)
    for addr, _, _, orig, patched, _ in PATCHES:
        o = addr - text_va
        if bytes(b[o:o + len(patched)]) == patched:
            b[o:o + len(orig)] = orig
    return bytes(b)


def apply(text, text_va, func_va=None):
    """`text` with every patch applied. func_va: {function name: address in THIS image}, so a
    site is found by its function and offset when the layout differs from TI.EXE's; without it
    the TI.EXE addresses are used. Returns (bytes, applied, skipped) where skipped lists
    (function, why) for a site whose original bytes are not there."""
    b = bytearray(text)
    applied, skipped = [], []
    for addr, fn, off, orig, patched, _ in PATCHES:
        if func_va is not None:
            if fn not in func_va:
                skipped.append((fn, 'not in the map'))
                continue
            site = func_va[fn] + off
        else:
            site = addr
        o = site - text_va
        cur = bytes(b[o:o + len(orig)])
        if cur == patched:
            applied.append((fn, site, 'already patched'))
        elif cur == orig:
            b[o:o + len(patched)] = patched
            applied.append((fn, site, 'patched'))
        else:
            skipped.append((fn, 'expected %s at 0x%08x, found %s' % (orig.hex(), site, cur.hex())))
    return bytes(b), applied, skipped


def text_section(pe_bytes):
    """(file offset, raw size, virtual address) of a PE's .text section."""
    pe = struct.unpack_from('<I', pe_bytes, 0x3c)[0]
    nsec = struct.unpack_from('<H', pe_bytes, pe + 6)[0]
    optsz = struct.unpack_from('<H', pe_bytes, pe + 20)[0]
    base = struct.unpack_from('<I', pe_bytes, pe + 24 + 28)[0]
    off = pe + 24 + optsz
    for _ in range(nsec):
        if pe_bytes[off:off + 8].rstrip(b'\0') == b'.text':
            vs, va, rs, ro = struct.unpack_from('<IIII', pe_bytes, off + 8)
            return ro, rs, base + va
        off += 40
    raise ValueError('no .text section')


def apply_to_pe(path, func_va):
    """Patch the exe at `path` in place; returns (applied, skipped) as apply() does. The PE
    checksum is not touched (link.exe leaves it 0 without /RELEASE)."""
    b = bytearray(open(path, 'rb').read())
    ro, rs, tva = text_section(b)
    new, applied, skipped = apply(bytes(b[ro:ro + rs]), tva, func_va)
    if new != bytes(b[ro:ro + rs]):
        b[ro:ro + rs] = new
        # a freshly linked exe can be held for a moment (virus scanner): retry briefly
        for i in range(20):
            try:
                with open(path, 'r+b') as f:
                    f.write(bytes(b))
                break
            except PermissionError:
                if i == 19:
                    raise
                time.sleep(0.5)
    return applied, skipped


def main():
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import config as cfg
    if not (cfg.EXE and cfg.EXE_UNPATCHED):
        print('set TITANIC_EXE (GOG\'s TI.EXE) and TITANIC_EXE_UNPATCHED (the December 1996 exe as released)')
        return 2
    raw, orig_raw = open(cfg.EXE, 'rb').read(), open(cfg.EXE_UNPATCHED, 'rb').read()
    ro, rs, tva = text_section(raw)
    bad = 0
    for addr, fn, off, orig, patched, why in PATCHES:
        cur = raw[ro + addr - tva: ro + addr - tva + len(patched)]
        ok = cur == patched
        bad += not ok
        print('0x%08x %-32s %s -> %s  %s' % (addr, fn + '+0x%x' % off, orig.hex(), patched.hex(),
                                            'present' if ok else 'MISSING (found %s)' % cur.hex()))
    if len(orig_raw) != len(raw):
        print('size %d != %d' % (len(orig_raw), len(raw)))
        return 1
    diff = [i for i in range(len(raw)) if raw[i] != orig_raw[i]]
    expected = set()
    for addr, _, _, orig, patched, _ in PATCHES:
        for k in range(len(orig)):
            if orig[k] != patched[k]:
                expected.add(addr - tva + ro + k)
    extra = sorted(set(diff) - expected)
    missing = sorted(expected - set(diff))
    print('unpatched vs GOG: %d bytes differ, %d explained by PATCHES' % (len(diff), len(set(diff) & expected)))
    if extra:
        bad += 1
        print('  UNEXPLAINED (file offsets): ' + ' '.join('0x%x' % o for o in extra[:40]))
    if missing:
        bad += 1
        print('  patch bytes the two files do NOT differ at: ' + ' '.join('0x%x' % o for o in missing[:40]))
    if unpatch(raw[ro:ro + rs], tva) != orig_raw[ro:ro + rs]:
        bad += 1
        print('  unpatch() of the GOG .text != the unpatched .text')
    else:
        print('  unpatch(GOG .text) == unpatched .text')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
