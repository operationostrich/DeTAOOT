"""bss.py -- the uninitialised data: its names and the dead space between its variables.

Names. Within one object VC++ 4.2 emits the .bss variables in ascending order of a 10-bit hash
of the name as written in the source:

    h = 0; for ch in name: h = ((h << 2) + (h >> 4) + ord(ch)) & 0xffffffff
    K = (h ^ (h >> 16)) & 1023

ties broken by reverse order of first declaration; use order, type, size and other identifiers
do not matter. After placing, an object of 4 bytes or less is 4-aligned, larger ones 8-aligned.
The original names are lost, but the exe holds only their order: so each uninitialised global
has a suffixed name whose K rises with its TI.EXE address (strictly, over the whole program --
384 of them, under the 1024 buckets -- so every unit's subset is in order without knowing who
owns what). include/df_bss_names.h maps each name with `#define`, so declarations, definitions
and uses all change together and no unit gains a symbol. df_globals.h includes it when
DF_BSS_NAMES is defined, which the match build does. The header is checked in and its names
are stable: the names also move register tie-breaks, and symbol_phase.json is fitted with them.

Pads. TI.EXE's uninitialised data has holes no code references: variables the original declared
and never used (they still take their slot in the object's .bss), or an array declared larger
than any access shows. Each unit's bss block is laid out by VC++ 4.2 in name-hash order, objects
of 4 bytes or less 4-aligned, larger ones 8-aligned, each object's .bss contribution aligned to
its largest member's alignment. main() simulates that layout for every unit with our declared
sizes, starting at the unit's first global's TI.EXE address, and where the next global sits
later in TI.EXE than the simulation puts it, emits a pad `char df_bsspad_<addr>[n];` named so
its hash key equals that global's: VC++ 4.2 breaks a hash tie by reverse order of first
declaration, and the pads are defined at the end of the unit copy (after every function, so no
function sees one more symbol), so each lands just in front of its global. A unit's tail pad
reaches the next unit's first global. Where the simulation is already PAST a global's TI.EXE
address (our object too large, or a struct split into pieces that cannot keep their alignment
as separate objects) it says so and carries on: that is a typing question, not padding.

    python match/bss.py [--end 0xADDR]     # writes build/bss_pads.json, prints the report

--end: where the last unit's block ends in TI.EXE (LIBCMT's first bss variable).
"""
import itertools
import json
import os
import re
import string
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import config as cfg            # noqa: E402
import globals_table as gt      # noqa: E402
import build                    # noqa: E402

CH = string.ascii_letters + string.digits
HEADER = os.path.join('include', 'df_bss_names.h')


def K(name):
    h = 0
    for ch in name:
        h = ((h << 2) + (h >> 4) + ord(ch)) & 0xFFFFFFFF
    return (h ^ (h >> 16)) & 1023


def name_with_key(base, k):
    if K(base) == k:
        return base
    for n in (1, 2, 3):
        for t in itertools.product(CH, repeat=n):
            cand = base + '_' + ''.join(t)
            if K(cand) == k:
                return cand
    raise ValueError('no name for %s with K %d' % (base, k))


def bss_globals():
    """(TI.EXE address, MATCH name) of every uninitialised global: the header is keyed by the
    name the compiled copies see (match/df_match_names.h maps the port's name onto it)."""
    out = []
    for n, l in build.global_definitions().items():
        m = re.search(r'/\*\s*0x(00[0-9a-fA-F]{6})\s*\*/\s*$', l)
        if m and int(m.group(1), 16) >= build.BSS_START and '=' not in l.split('/*')[0]:
            out.append((int(m.group(1), 16), gt.match_of(n)))
    out += [(a, n) for a, n, _u in build.UNIT_BSS]
    return sorted(out)


def sizes(names, spell=None):
    """{name: sizeof} through VC++ 4.2, with df_globals.h as the units see it; `spell` is the
    identifier to write for each name when it differs (the port's name for a match name)."""
    d = os.path.join('build', 'bss_pads')
    os.makedirs(d, exist_ok=True)
    src = os.path.join(d, 'sizes.c')
    with open(src, 'w') as f:
        f.write(cfg.PRELUDE.replace('/*@@GLOBALS@@*/', '#include "df_globals.h"\n'))
        f.write('unsigned df_sizes[] = {\n%s\n};\n' % ',\n'.join('sizeof(%s)' % n for n in (spell or names)))
    obj = src[:-2] + '.obj'
    r = subprocess.run([os.path.join(cfg.vc42(), 'BIN', 'CL.EXE')] + cfg.COMMON + ['/Fo' + obj, src],
                       env=cfg.env(), capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stdout + r.stderr)
    b = open(obj, 'rb').read()
    n = struct.unpack_from('<H', b, 2)[0]
    for i in range(n):
        nm, _, _, rs, ro = struct.unpack_from('<8sIIII', b, 20 + 40 * i)
        if nm.rstrip(b'\0') == b'.data':
            return {k: struct.unpack_from('<I', b, ro + 4 * j)[0] for j, k in enumerate(names)}
    sys.exit('no .data in ' + obj)


def align(x, a):
    return (x + a - 1) & ~(a - 1)


def main():
    end = int(sys.argv[sys.argv.index('--end') + 1], 16) if '--end' in sys.argv else None
    g = bss_globals()                                  # (TI address, match name), all units
    unit_bss = dict((n, u) for a, n, u in build.UNIT_BSS)
    own = {}
    for a, n in g:
        u = build.owner_of(gt.name_of(n)) or unit_bss.get(n)
        if u:
            own.setdefault(u, []).append((a, n))
    sz = sizes([n for a, n in g if n not in unit_bss], [gt.name_of(n) for a, n in g if n not in unit_bss])
    sz.update({n: 4 for n in unit_bss})
    keys = {}
    for l in open(HEADER):
        m = re.match(r'#define (\w+) (\w+)$', l.strip())
        if m:
            keys[m.group(1)] = K(m.group(2))
    units = sorted(own, key=lambda u: own[u][0][0])
    pads, problems = {}, []
    for ui, u in enumerate(units):
        vs = own[u]
        nxt = own[units[ui + 1]][0][0] if ui + 1 < len(units) else end
        pos = vs[0][0]
        lines = []

        def pad(at, n, key, tag):
            # one pad of n bytes at `at` (4-aligned); a pad over 4 bytes is 8-aligned, so a
            # 4-mod-8 start takes a 4-byte pad first. Declared in reverse (ties: later first).
            out = []
            if n > 4 and at % 8:
                out.append((at, 4))
                at, n = at + 4, n - 4
            if n > 0:
                out.append((at, n))
            for a, k in reversed(out):
                nm = name_with_key('df_bsspad_%08x' % a, key)
                lines.append('char %s[%d];  /* pad 0x%08x..0x%08x %s */' % (nm, k, a, a + k, tag))

        for i, (a, n) in enumerate(vs):
            s = sz[n]
            al = 4 if s <= 4 else 8
            p = align(pos, 4)
            if align(p, al) < a:                       # more than the alignment gap: dead space
                pad(p, a - p, keys[n], 'before ' + n)
                p = a
            p = align(p, al)
            if p != a:
                problems.append('%s %s: TI 0x%08x, earliest 0x%08x (%+d), size %d' % (u, n, a, p, p - a, s))
            pos = p + s
        if nxt is not None:
            p = align(pos, 4)
            # the next object's .bss is aligned to its largest member's alignment: the linker
            # pads to that by itself
            nal = 8 if ui + 1 < len(units) and any(sz[m] > 4 for a, m in own[units[ui + 1]]) else 4
            if align(p, nal) < nxt:
                pad(p, nxt - p, min(keys[vs[-1][1]] + 1, 1023), 'tail of ' + u)
        pads[u] = lines
    os.makedirs(os.path.dirname(build.PADS), exist_ok=True)
    json.dump(pads, open(build.PADS, 'w'), indent=1)
    for u in units:
        print('%-34s %d pads, %d bytes' % (u, len(pads[u]), sum(int(re.search(r'\[(\d+)\]', l).group(1)) for l in pads[u])))
    print('\n%d globals the padding cannot place:' % len(problems))
    for p in problems:
        print('  ' + p)
    print('wrote', build.PADS)


if __name__ == '__main__':
    main()
