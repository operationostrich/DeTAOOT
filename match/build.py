#!/usr/bin/env python
"""
build.py -- link the source into byte-identical copies of TI.EXE with Visual C++ 4.2.

    python match/build.py [--check] [--force]

Compiles every unit listed in src/units.txt whole, in that order, with the 1996 compiler
(/O2), plus the two data files, compiles res/titanic.rc with that release's rc.exe, and
links /MT with the 1996 linker against LIBCMT.LIB and the five DLLs the game imports. Writes

    build/link/TI_original.exe   CyberFlix's December 1996 build as released
    build/link/TI_gog.exe        GOG's copy of it (25 bytes hex-patched afterwards:
                                 match/gog_patches.py; docs/differences.md)

With --check each is compared with the exe named by TITANIC_EXE_UNPATCHED / TITANIC_EXE.

The order of the objects handed to the linker is the order of the original link and decides
where every function and variable lands: src/units.txt records it, do not sort or reorder it.
What compiles is not quite src/: each unit is copied first, with its own globals defined where
the original's source had them, and with the steering tables of this folder applied
(symbol_phase.json, local_pads.json, line_patches.json, unit_layout.json -- docs/differences.md
lists every one). The copies are in build/link/units/.
"""
import json
import os
import re
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import config as cfg                    # noqa: E402
import globals_table as gt              # noqa: E402
import gog_patches                      # noqa: E402
import import_order                     # noqa: E402
ROOT = cfg.ROOT
os.chdir(ROOT)

TU = os.path.join('build', 'tu')        # the compiled copies' wrapper sources and objects
OUT = os.path.join('build', 'link')
LINK96 = os.path.join('build', 'link96')
PADS = os.path.join('build', 'bss_pads.json')
UNITS_TXT = os.path.join('src', 'units.txt')

# The import libraries in the usual order and LIBCMT last (as the /MT default library is searched
# after the command line): TI.EXE's import directory is KERNEL32, USER32, GDI32, COMDLG32, WINMM,
# and it has the GetSaveFileNameA / GetOpenFileNameA thunks right after the game code, before
# LIBCMT's __ftol, and RtlUnwind's (kernel32, called only by LIBCMT) after LIBCMT -- link.exe
# places library code in the order it resolves symbols, searching the libraries in this order.
LIBS = ['kernel32.lib', 'user32.lib', 'gdi32.lib', 'comdlg32.lib', 'winmm.lib', 'libcmt.lib']

# The CRT functions the port calls by Ghidra's names are linked under the names the ORIGINAL
# called them by: the symbols are renamed in each unit object's copy (no declaration moves).
# It decides the order LINK loads LIBCMT's members in: sprintf/sscanf by their own names, and
# strlwr/strupr/stricmp/strcmpi by their OLD names, which resolve through OLDNAMES.LIB --
# searched after LIBCMT -- so those members come last, as in TI.EXE.
CRT_RENAMES = {'__sscanf': '_sscanf', '__sprintf': '_sprintf', '___strcmpi': '_strcmpi',
               '__strlwr': '_strlwr', '__strupr': '_strupr', '__stricmp': '_stricmp'}
# TI.EXE has ONE copy routine (0x0044e440), reached from the memmove sites and the _memcpy
# sites alike: with '_memcpy' here LIBCMT's memcpy.obj came in as a second copy.
CRT_ALIASES = {'__memcpy': '_memmove'}

# Declared image metadata: values the link takes from the clock, not from any source. TI.EXE was
# linked 1996-12-31 (PE TimeDateStamp 0x32c98a4c); cvtres stamped every resource directory two
# seconds earlier (0x32c98a4a). Written after the link; nothing else is copied from the exe.
IMAGE_METADATA = {'pe_timestamp': 0x32c98a4c, 'rsrc_timestamp': 0x32c98a4a}

# String literals only AppInit references. AppInit is a naked inline-asm body under VC++ (no C
# input reaches its bytes; src/platform/display_ui.c), which addresses the unit's pooled literals
# as AppInit+displacement, so the literal COMDATs are referenced by nothing LINK can see except
# the discarded C keeper (AppInit_Source) -- and LINK 4.20 discards a string COMDAT referenced
# only by discarded code. /INCLUDE on the command line keeps them (LINK 4.20 ignores /INCLUDE
# in .drectve).
STRING_KEEPS = [
    '??_C@_0L@OBOC@ACCELTABLE?$AA@',
    '??_C@_02MECO@?$CFd?$AA@',
    '??_C@_0L@BKBI@WINDOWICON?$AA@',
    '??_C@_04DFJM@MENU?$AA@',
    '??_C@_0L@MJPJ@CURS?4WATCH?$AA@',
    '??_C@_02NLON@?9e?$AA@',
    '??_C@_02INOP@?9s?$AA@',
    '??_C@_03GFJP@?916?$AA@',
    '??_C@_02NKLA@?9j?$AA@',
    '??_C@_02CHHI@?9r?$AA@',
    '??_C@_02NGML@?9?$DP?$AA@',
    '??_C@_02IPJO@?9h?$AA@',
]

# --- the link order ---------------------------------------------------------------------------


def read_units():
    """[(kind, path relative to src/)] from src/units.txt: kind 'unit' (game code) or 'data'
    (compiled with the prelude and linked at its place in the list)."""
    out = []
    for l in open(UNITS_TXT, encoding='utf-8'):
        l = l.split('#', 1)[0].strip()
        if not l:
            continue
        a, p = l.split()
        if not os.path.exists(os.path.join('src', p)):
            raise SystemExit('src/units.txt lists %s, which does not exist' % p)
        out.append(('data' if a == 'data' else 'unit', p))
    listed = {p for _, p in out}
    for d, _, files in os.walk('src'):
        for f in files:
            rel = os.path.relpath(os.path.join(d, f), 'src').replace('\\', '/')
            if f.endswith('.c') and rel not in listed and not rel.startswith('port/'):
                raise SystemExit('src/%s is not in src/units.txt: the link order must say where it goes' % rel)
    return out


# --- source helpers ---------------------------------------------------------------------------

DEF_RE = re.compile(r'^(?:[\w\*]+\s+)*\*?([A-Za-z_]\w*)\s*\([^;]*$')
BSS_START = 0x00460860      # last non-zero raw .data byte is 0x00460859
# TI.EXE's .data holds the initialised data first (each unit's block in link order, then
# LIBCMT's), and the uninitialised data from here on, again unit by unit. A global below this
# address that globals.c leaves uninitialised was `= 0` in the original source: VC++ 4.2 puts an
# explicitly zero-initialised global in the initialised part.
INIT_END = 0x00460870
CRT_START = 0x0044E3A0      # __ftol, the first LIBCMT function; the game code runs to 0x0044e393


def read(path):
    return open(path, encoding='utf-8', errors='replace').read().split('\n')


def find_functions(lines):
    """[(name, def_line, body_end_line, is_static)] for every definition."""
    out, i = [], 0
    while i < len(lines):
        m = DEF_RE.match(lines[i])
        if m and not lines[i].lstrip().startswith(('extern', 'typedef')):
            j = i
            while j < len(lines) and '{' not in lines[j] and ';' not in lines[j]:
                j += 1
            if j >= len(lines) or ';' in lines[j]:
                i += 1
                continue
            depth, k = 0, j
            while k < len(lines):
                depth += lines[k].count('{') - lines[k].count('}')
                if depth == 0:
                    break
                k += 1
            name = m.group(1)
            static = lines[i].lstrip().startswith('static') or (
                i > 0 and lines[i - 1].strip().startswith('static') and not lines[i - 1].strip().endswith(';'))
            out.append((name, i, k, static))
            i = k + 1
            continue
        i += 1
    return out


def comment_start(lines, def_line):
    """Walk up over the comment block and blank lines attached to a definition."""
    i = def_line
    if i > 0 and re.match(r'^[A-Za-z_][\w \*]*$', lines[i - 1].strip()) and not lines[i - 1].strip().endswith(';'):
        i -= 1
    while i > 0:
        s = lines[i - 1].strip()
        if s == '' or s.startswith(('/*', '*', '//')):
            i -= 1
        else:
            break
    return i


def strip_comments(lines):
    """The same lines with every /* */ comment blanked, across line breaks."""
    out, inc = [], False
    for l in lines:
        r, i = [], 0
        while i < len(l):
            if inc:
                j = l.find('*/', i)
                if j < 0:
                    i = len(l)
                    break
                i = j + 2
                inc = False
            else:
                j = l.find('/*', i)
                if j < 0:
                    r.append(l[i:])
                    break
                r.append(l[i:j])
                i = j + 2
                inc = True
        out.append(''.join(r))
    return out


_functions = None


def function_addresses():
    """{function name: TI.EXE address} of the game code (match/functions.tsv)."""
    global _functions
    if _functions is None:
        _functions = {}
        for l in open(os.path.join(HERE, 'functions.tsv'), encoding='utf-8'):
            c = l.rstrip('\n').split('\t')
            if len(c) >= 2 and c[0].startswith('0x'):
                _functions[c[1]] = int(c[0], 16)
    return _functions


# --- the steering tables ----------------------------------------------------------------------
_tables = {}


def table(name):
    if name not in _tables:
        p = os.path.join(HERE, name)
        _tables[name] = json.load(open(p, encoding='utf-8')) if os.path.exists(p) else {}
    return _tables[name]


def apply_phase(lines, name):
    """`lines` with each symbol_phase.json row's k declarations on a new line above the
    function's definition (above its return-type line and its annotation, if it has them).
    VC++ 4.2 breaks register and compare-direction ties by the index of the named symbols it has
    created so far, so a function's code depends on how many symbols precede it; the original had
    symbols (debug-only locals, macros' temporaries, declarations in headers we do not have)
    that no decompile shows. Each row says "the original had k more symbols than our source by
    this point" (modulo 65536)."""
    want = dict((f, k) for f, k in table('symbol_phase.json').get(name, []))
    if not want:
        return lines
    out = list(lines)
    for f in sorted(find_functions(lines), key=lambda f: -f[1]):
        k = want.get(f[0])
        if k:
            j = f[1]
            while j > 0:
                prev = lines[j - 1].strip()
                if prev.startswith('// FUNCTION'):
                    j -= 1
                    break
                if not prev or prev.endswith(('}', ';', '*/')) or prev.startswith(('#', '/*', '*', '//')):
                    break
                j -= 1
            decl = ['extern int df_phase_%s_%d;' % (f[0], i) for i in range(k)]
            out.insert(j, '\n'.join(' '.join(decl[i:i + 8]) for i in range(0, k, 8)))
    return out


_DECL_SKIP = re.compile(r'^(return|if|else|for|while|do|switch|case|default|goto|break|continue|sizeof)\b')


def _local_decl_names(line):
    """The names a local declaration line declares, or None when it is not one."""
    s = re.sub(r'/\*.*?\*/', '', line).strip()
    if not s.endswith(';') or _DECL_SKIP.match(s) or s.startswith(('typedef', 'extern', '#')):
        return None
    m = re.match(r'^((?:(?:const|volatile|register|static|unsigned|signed|struct|union|enum)\s+)*'
                 r'[A-Za-z_]\w*)((?:\s+|\s*\*+\s*)[A-Za-z_*].*);$', s)
    if not m or m.group(1).split()[-1] in ('return',):
        return None
    names = []
    depth, cur = 0, ''
    for ch in m.group(2):                      # split the declarators on top-level commas
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        if ch == ',' and depth == 0:
            names.append(cur)
            cur = ''
        else:
            cur += ch
    names.append(cur)
    out = []
    for d in names:
        d = d.split('=')[0]
        n = re.match(r'^[\s*]*([A-Za-z_]\w*)\s*(\[[^\]]*\]\s*)*$', d)
        if not n:
            return None                        # a call, a cast, a function pointer: not ours
        out.append(n.group(1))
    return out


def apply_line_patches(lines, name):
    """`lines` with line_patches.json's replacements made: a source line replaced, in the
    compiled copy only, by a spelling that steers the compiler's internal numbering (temp ids,
    web ids) with no change in behaviour; src/ keeps the readable line. Matched by the stripped
    text; an `old` that is not on exactly one line is a hard error."""
    rows = table('line_patches.json').get(name, [])
    if not rows:
        return lines
    out = list(lines)
    for row in rows:
        old, new = row['old'].strip(), row['new'].strip()
        if 'occurrence' in row:
            k = int(row['occurrence'])
            hits = [i for i, l in enumerate(out) if l.strip() in (old, new)]
            if k >= len(hits):
                raise SystemExit('line_patches.json: %s: occurrence %d of %r, only %d lines'
                                 % (name, k, old[:80], len(hits)))
            if out[hits[k]].strip() != new:
                out[hits[k]] = re.match(r'\s*', out[hits[k]]).group(0) + new
            continue
        if any(l.strip() == new for l in out):
            continue
        hits = [i for i, l in enumerate(out) if l.strip() == old]
        if len(hits) != 1:
            raise SystemExit('line_patches.json: %s: %d lines read %r' % (name, len(hits), old[:80]))
        out[hits[0]] = re.match(r'\s*', out[hits[0]]).group(0) + new
    return out


def apply_local_pads(lines, name):
    """`lines` with line_patches.json's replacements and local_pads.json's declarations:
    n unused `int df_lp_<function>_<i>;` declared inside that function, in front of the local
    declaration line that declares `before` (default: the first local that is not a typedef).
    The compiler numbers a function's parameters, then its locals in declaration order, and
    breaks register-colouring ties by that index mod 32; locals the original declared and the
    optimizer removed move the survivors' indices, and no decompiler can see them."""
    lines = apply_line_patches(lines, name)
    want = table('local_pads.json').get(name, {})
    if not want:
        return lines
    out = list(lines)
    funcs = {f[0]: f for f in find_functions(lines)}
    for fn in want:
        if fn not in funcs:
            raise SystemExit('local_pads.json: %s has no function %s' % (name, fn))
    for fn, row in sorted(want.items(), key=lambda kv: -funcs[kv[0]][1]):
        _, dl, end, _ = funcs[fn]
        n, before = int(row['count']), row.get('before')
        if any('df_lp_%s_0;' % fn in l for l in lines[dl:end]):
            continue
        j = dl
        while '{' not in lines[j]:
            j += 1
        at = None
        for i in range(j + 1, end):
            names = _local_decl_names(lines[i])
            if names is None:
                s = lines[i].strip()
                if not s or s.startswith(('/*', '*', '//', 'typedef')) or s.endswith('*/'):
                    continue
                break
            if before is None or before == names[0]:
                at = i
                break
            if before in names:
                raise SystemExit('local_pads.json: %s %s: %r is not the first name on its '
                                 'declaration line (%s)' % (name, fn, before, lines[i].strip()))
        if at is None:
            raise SystemExit('local_pads.json: %s %s: no local declaration %s' %
                             (name, fn, repr(before) if before else '(the function has none)'))
        ind = re.match(r'\s*', lines[at]).group(0)
        decl = ['int df_lp_%s_%d;' % (fn, i) for i in range(n)]
        out[at] = ind + ' '.join(decl) + ' ' + out[at].lstrip()
    return out


# --- the globals ------------------------------------------------------------------------------

# Functions a unit declares itself, next to the code that calls them, so pruned_protos() does
# not also pull their df_protos.h prototypes into the unit's header (a header prototype is a
# symbol every function of the unit counts; a local one counts only for the code after it).
# container_store.c: the display-attach loader thread, original source the Dec 1996 link
# discarded (/OPT:REF), between IsDisplayAttachLoaderRunning and SetupDisplayAttachSlotRecord.
LOCAL_PROTOS = {
    'container_store.c': {'RestorePrimaryHeap', 'ReserveHeapSpace', 'MoveResObjectToHeapTop',
                          'StartDisplayAttachLoaderThread', 'StopDisplayAttachLoaderThread'},
}
# Uninitialised globals a unit defines in its own source (under #pragma bss_seg) instead of
# globals.c, with their TI.EXE address and the unit: heap_resources.c's descriptor heap, and
# container_store.c's loader thread handle and stop flag -- globals of the code the original link
# discarded, which nothing in TI.EXE references but whose slots TI.EXE's bss keeps (as a
# df_globals.h extern they would be two more symbols in every unit). All are 4 bytes (bss.py
# relies on it).
UNIT_BSS = [(0x0048a9a0, 'gDescPageTail', 'heap_resources.c'),
            (0x0048a9a4, 'gDescLastEntry', 'heap_resources.c'),
            (0x0048a9f4, 'gDescFreeList', 'heap_resources.c'),
            (0x0048a9f8, 'gDescPageHead', 'heap_resources.c'),
            (0x0048a9fc, 'gHeapPageCount', 'heap_resources.c'),
            (0x00489d98, 'g_hAttachLoaderThread', 'container_store.c'),
            (0x00489d9c, 'g_attachLoaderStop', 'container_store.c')]
# Placeholders for globals a unit defines itself under a real name (bss.UNIT_BSS): no definition
# from globals.c.
UNIT_DEFINED = {'DAT_0048a9a0', 'DAT_0048a9a4', 'DAT_0048a9f4', 'DAT_0048a9f8', 'DAT_0048a9fc'}


def vc_lines(path):
    """The lines of a file that the VC++ 4.2 build compiles: of an `#ifdef` / `#ifndef` on
    _MSC_VER, DF_VC42 or __GNUC__ only the branch the 1996 compiler takes; every other
    conditional's lines as they are."""
    stack = []
    for l in open(path, encoding='utf-8'):
        m = re.match(r'\s*#\s*(ifdef|ifndef|if|elif|else|endif)\b\s*(\w*)', l)
        if m:
            d, sym = m.group(1), m.group(2)
            if d in ('ifdef', 'ifndef'):
                vc = {'_MSC_VER': True, 'DF_VC42': True, '__GNUC__': False}.get(sym)
                stack.append(None if vc is None else (vc if d == 'ifdef' else not vc))
            elif d == 'if':
                stack.append(None)
            elif d == 'else' and stack and stack[-1] is not None:
                stack[-1] = not stack[-1]
            elif d == 'endif' and stack:
                stack.pop()
            continue
        if all(v is not False for v in stack):
            yield l


GLOBALS_C = os.path.join('src', 'data', 'globals.c')
_defs = None


def global_definitions():
    """{name: definition line of src/data/globals.c} for the globals the VC++ build defines."""
    global _defs
    if _defs is None:
        _defs = {}
        rx = re.compile(r'^[A-Za-z].*?(?:\(\s*\*\s*([A-Za-z_]\w*)\s*\)\s*\([^)]*\)|\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*)'
                        r'\s*(?:=.*)?;\s*/\*\s*0x(00[0-9a-fA-F]{6})\s*\*/\s*$')
        for l in vc_lines(GLOBALS_C):
            m = rx.match(l.rstrip('\n'))
            if m:
                _defs[m.group(1) or m.group(2)] = l.rstrip('\n')
        for n in UNIT_DEFINED:
            _defs.pop(gt.name_of(n), None)
    return _defs


def owner_of(name):
    """The unit whose data/bss block holds global `name`, or None (match/owners.tsv)."""
    return gt.owners().get(name)


def literal_macros():
    """The globals (port names) df_globals.h turns into string literals with the fixes off."""
    out = set()
    for l in open(os.path.join('include', 'df_globals.h'), encoding='utf-8'):
        m = re.match(r'#define (\w+) \("', l)
        if m:
            out.add(m.group(1))
    return out


def own_globals_text(name):
    """Definitions (from src/data/globals.c) of the globals this unit owns, in address order."""
    defs = global_definitions()
    macros = literal_macros()       # a Pascal constant that was a literal in the original: never defined
    mine = []
    for l in sorted((l for n, l in defs.items() if owner_of(n) == name and n not in macros),
                    key=lambda l: int(re.search(r'0x(00[0-9a-fA-F]{6})\s*\*/\s*$', l).group(1), 16)):
        a = int(re.search(r'0x(00[0-9a-fA-F]{6})\s*\*/\s*$', l).group(1), 16)
        code, _, cmt = l.partition(';')
        if a < INIT_END and '=' not in code:
            l = code + ' = {0};' + cmt
        mine.append(l)
    return "/* this unit's own globals */\n" + '\n'.join(mine) + '\n'


LIT_ADDR = re.compile(r'"\s*/\*\s*0x(00[0-9a-fA-F]{6})\s*\*/')
IDENT = re.compile(r'\b[A-Za-z_]\w*\b')


def unit_pads():
    """{unit: pad definitions} from build/bss_pads.json (match/bss.py), {} when it is not there."""
    return json.load(open(PADS)) if os.path.exists(PADS) else {}


def place_globals(name, src_path):
    """The compiled copy of a unit: src/<path> with each global it owns defined where the
    original source had it. VC++ emits a unit's data in source order -- globals at their
    definition, string literals in the function that uses them -- so in TI.EXE's .data a global
    sits after the literals of the functions defined before it. Each global goes right after
    the function owning the nearest of the unit's literals below it in the exe (literals carry
    their address: `"..." /* 0x0045badc */`, and the Pascal constants are named globals), or
    before the first function when no literal precedes it. A definition after the top-of-file
    extern is not a new symbol, so the code is unchanged. Also: the unit's bss pads, the local
    pads and line patches, and the symbol-phase table."""
    addr = function_addresses()
    pascal = {}
    for l in open(os.path.join('include', 'df_globals.h'), encoding='utf-8'):
        m = re.match(r'#define (\w+) \("', l)
        if m and gt.addr_of(m.group(1)) is not None:
            pascal[m.group(1)] = gt.addr_of(m.group(1))
    lines = read(src_path)
    fs = find_functions(lines)
    spans = [(comment_start(lines, f[1]), f[2]) for f in fs]
    lits = []
    for i, (s, e) in enumerate(spans):
        code = '\n'.join(lines[s:e + 1])
        lits += [(int(a, 16), i) for a in LIT_ADDR.findall(code)]
        lits += [(pascal[n], i) for n in set(IDENT.findall(code)) & set(pascal)]
    lits.sort()
    insert = {}
    for d in own_globals_text(name).split('\n')[1:]:
        m = re.search(r'/\*\s*0x(00[0-9a-fA-F]{6})\s*\*/\s*$', d)
        if not m:
            continue
        g = int(m.group(1), 16)
        # uninitialised data has its own area: only the order among those matters
        before = [i for a, i in lits if a < g] if g < INIT_END else []
        if before:
            at = spans[max(before)][1] + 1
            while at < len(lines) and lines[at].strip().startswith('#endif'):
                at += 1
        else:
            at = max(i for i, l in enumerate(lines[:120]) if l.startswith('#include')) + 1
        insert.setdefault(at, []).append(d)
    # `// FUNCTION: TI 0x...` above every catalogued definition: a comment, no code effect, but
    # the symbol-phase insertion keys on it
    marks = {}
    for fname, dl, end, st in fs:
        a = addr.get(fname)
        if a is None or a >= CRT_START:
            continue
        i = dl
        while i > 0 and lines[i - 1].strip() and not lines[i - 1].rstrip().endswith(('*/', ';', '}', '{')) \
                and not lines[i - 1].lstrip().startswith(('#', '//')):
            i -= 1
        marks[i] = '// FUNCTION: TI 0x%08x' % a
    # VC++ 4.2 makes `int x;` a COMMON symbol, which link.exe lays out per object in REVERSE
    # link order; TI.EXE's uninitialised data is per unit in link order. `#pragma bss_seg` makes
    # them ordinary .bss definitions, which the linker keeps in order.
    top = max(i for i, l in enumerate(lines[:120]) if l.startswith('#include')) + 1
    out = []
    for i, l in enumerate(lines):
        if i in insert:
            if i == top:
                out.append('#pragma bss_seg(".bss")')
            out.extend(insert[i])
            if i == top:
                out.append('#pragma bss_seg()')
        if i in marks:
            out.append(marks[i])
        out.append(l)
    out.extend(insert.get(len(lines), []))
    # the dead space of the unit's bss block (match/bss.py), after every function so that no
    # function sees one more symbol
    pads = unit_pads().get(name)
    if pads:
        out += ['#pragma bss_seg(".bss")'] + pads + ['#pragma bss_seg()']
    out = apply_local_pads(out, name)
    out = apply_phase(out, name)
    path = os.path.join(OUT, 'units', name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    return path


# --- the header layout ------------------------------------------------------------------------
# unit_layout.json, {unit: {"pre": k, "mid": m, "early": [...], "late": [...], "shift": {...},
# "ishift": {...}, "hdr": {...}, "gtype": {...}}}: a tie between two candidates goes by the
# candidates' positions in the symbol table, and a global's or an import's position is where its
# HEADER declared it. The table says per unit how the original's headers differed from ours:
# k symbols before <windows.h>, m between it and the globals, these globals declared before all
# the others, those after; "hdr" puts fillers in front of one of <windows.h>'s own #includes (a
# private copy of WINDOWS.H, first on the include path); "shift" moves ONE global k places down
# the symbol table and nothing else; "ishift" does the same for one imported function's SDK
# prototype (a copy of winuser.h etc.); "gtype" redeclares a global with another type. Applied
# to the compiled copy only.

def layout_for(name):
    return table('unit_layout.json').get(name, {})


SHIFT_PERIOD = 4352
_gl_lines = None


def reordered_globals(name, lay):
    """Path of a copy of df_globals.h with the layout's `early` globals declared first and its
    `late` ones last, each list in its own order (a global inside an #if block stays put)."""
    global _gl_lines
    if _gl_lines is None:
        _gl_lines = open(os.path.join('include', 'df_globals.h'), encoding='utf-8').read().split('\n')
    early = [gt.match_of(n) for n in lay.get('early', ())]
    late = [gt.match_of(n) for n in lay.get('late', ())]
    moved, keep, depth = {}, [], 0
    for l in _gl_lines:
        m = re.match(r'extern\s[^;]*?\b(\w+)\s*(?:\[[^\]]*\])*\s*;', l)
        depth += l.startswith('#if') - l.startswith('#endif')
        if m and depth == 1 and (gt.match_of(m.group(1)) in early or gt.match_of(m.group(1)) in late):
            moved[gt.match_of(m.group(1))] = l
        else:
            keep.append(l)
    first = next(k for k, l in enumerate(keep) if l.startswith('extern'))
    last = max(k for k, l in enumerate(keep) if l.startswith('#endif'))
    path = os.path.abspath(os.path.join(TU, 'globals_' + name[:-2] + '.h'))
    os.makedirs(TU, exist_ok=True)
    out = (keep[:first] + [moved[g] for g in early if g in moved] + keep[first:last] +
           [moved[g] for g in late if g in moved] + keep[last:])
    gtype = {gt.match_of(k): v for k, v in (lay.get('gtype') or {}).items()}
    if gtype:
        res = []
        for l in out:
            m = re.match(r'(extern\s+)([^;]*?)(\b\w+\s*(?:\[[^\]]*\])*\s*;.*)$', l)
            nm = gt.match_of(re.match(r'\w+', m.group(3)).group(0)) if m else None
            res.append(m.group(1) + gtype[nm] + ' ' + m.group(3) if nm in gtype else l)
        out = res
    shift = {gt.match_of(k): v for k, v in (lay.get('shift') or {}).items()}
    if shift:
        # one global's position alone: k fillers before it and SHIFT_PERIOD - k after, so every
        # later symbol moves by 4352 (= 17 * 256: the compiler's operand hash slips at it; the
        # exact period is 65536, which an entry gets as [k, 65536])
        res = []
        for l in out:
            m = re.match(r'extern\s[^;]*?\b(\w+)\s*(?:\[[^\]]*\])*\s*;', l)
            k = shift.get(gt.match_of(m.group(1))) if m else None
            period = SHIFT_PERIOD
            if isinstance(k, list):
                k, period = k
            if k:
                mn = gt.match_of(m.group(1))
                res += ['extern int df_gs_%s_a%d;' % (mn, i) for i in range(k)]
                res.append(l)
                res += ['extern int df_gs_%s_b%d;' % (mn, i) for i in range(period - k)]
            else:
                res.append(l)
        out = res
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(out))
    return path


_winh = None
_sdk = {}
SDK_HEADERS = ('WINBASE.H', 'WINGDI.H', 'WINUSER.H', 'MMSYSTEM.H', 'COMMDLG.H', 'WINREG.H', 'SHELLAPI.H',
               'WINNLS.H', 'WINVER.H', 'WINCON.H', 'WINNETWK.H')


def sdk_decl(imp):
    """(header file, first line, last line) of the SDK prototype of an imported function."""
    if not _sdk:
        for h in SDK_HEADERS:
            hp = os.path.join(cfg.vc42(), 'INCLUDE', h)
            if os.path.exists(hp):
                _sdk[h] = open(hp, encoding='latin-1').read().split('\n')
    one_line = re.compile(r'^\w[\w\s\*]*\s\*?%s\s*\(' % re.escape(imp))
    for h, L in _sdk.items():
        for i, l in enumerate(L):
            if re.match(re.escape(imp) + r'\s*\(', l) or one_line.match(l):
                a = i
                while a > 0 and L[a - 1].strip() and not L[a - 1].rstrip().endswith((';', '}')) \
                        and not L[a - 1].startswith('#'):
                    a -= 1
                b = i
                while b < len(L) - 1 and not L[b].rstrip().endswith(';'):
                    b += 1
                return h, a, b
    return None


def private_windows_h(name, pads, ishift=None):
    """Directory holding a WINDOWS.H with `pads` ({sub-header: k}) fillers before its includes,
    and copies of the SDK headers in which `ishift` ({import: k}) moves one prototype alone."""
    global _winh
    if _winh is None:
        _winh = open(os.path.join(cfg.vc42(), 'INCLUDE', 'WINDOWS.H'), encoding='latin-1').read().split('\n')
    out, done = [], set()
    for l in _winh:
        m = re.match(r'#include <(\w+)\.h>', l.strip())
        h = m.group(1).lower() if m else None
        if h in pads and h not in done:
            out += ['extern int df_pad_%s_%d;' % (h, k) for k in range(pads[h])]
            done.add(h)
        out.append(l)
    d = os.path.abspath(os.path.join(TU, 'winhdr', name[:-2]))
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'WINDOWS.H'), 'w', encoding='latin-1') as f:
        f.write('\n'.join(out))
    edits = {}
    for imp, k in (ishift or {}).items():
        loc = sdk_decl(imp)
        if loc and k:
            edits.setdefault(loc[0], []).append((loc[1], loc[2], imp, k))
    for h in SDK_HEADERS:
        if h not in edits and os.path.exists(os.path.join(d, h)):
            os.remove(os.path.join(d, h))
    for h, es in edits.items():
        L = list(_sdk[h])
        for a, b, imp, k in sorted(es, reverse=True):
            L[b + 1:b + 1] = ['extern int df_is_%s_b%d;' % (imp, i) for i in range(SHIFT_PERIOD - k)]
            L[a:a] = ['extern int df_is_%s_a%d;' % (imp, i) for i in range(k)]
        with open(os.path.join(d, h), 'w', encoding='latin-1') as f:
            f.write('\n'.join(L))
    return d


# --- compiling --------------------------------------------------------------------------------

def expand_project_includes(path, seen=None, skip=()):
    """The file's text with its project headers (include/...) inlined, once each."""
    seen = set() if seen is None else seen
    out = []
    for l in read(path):
        m = re.match(r'\s*#include\s+"([^"]+)"', l)
        hp = os.path.join(ROOT, 'include', m.group(1)) if m else None
        if hp and os.path.exists(hp):
            if hp not in seen and m.group(1) not in skip:
                seen.add(hp)
                out.append(expand_project_includes(hp, seen, skip))
            continue
        out.append(l)
    return '\n'.join(out)


def pruned_protos(unit_dir, name, path):
    """A copy of include/df_protos.h holding only the prototypes of functions the unit's code
    (and its other headers) names. The original files declared what they used; every
    prototype in the symbol table is one more symbol, so a unit that saw all ~1270 of them
    would be a different compile."""
    os.makedirs(unit_dir, exist_ok=True)
    text = expand_project_includes(path, skip={'df_protos.h'})
    code = '\n'.join(strip_comments(text.split('\n')))
    names = set(re.findall(r'\b[A-Za-z_]\w*\b', code)) - LOCAL_PROTOS.get(name, set())
    kept = []
    for l in open(os.path.join('include', 'df_protos.h'), encoding='utf-8'):
        m = re.search(r'\b([A-Za-z_]\w*)\s*\(', l.split('/*')[0])
        if m and '(' in l and l.rstrip().endswith((';', '*/')) and not l.startswith('#') \
                and m.group(1) not in names:
            continue
        kept.append(l)
    open(os.path.join(unit_dir, 'df_protos.h'), 'w', encoding='utf-8').write(''.join(kept))


def cl(args):
    return [os.path.join(cfg.vc42(), 'BIN', 'CL.EXE')] + cfg.COMMON + cfg.O2 + args


def run(cmd):
    r = subprocess.run(cmd, env=cfg.env(), capture_output=True, text=True, cwd=ROOT)
    return r.returncode, r.stdout + r.stderr


def compile_unit(name, path):
    """(obj path or None, error lines) for the compiled copy `path` of unit `name`."""
    os.makedirs(TU, exist_ok=True)
    src = os.path.join(TU, name[:-2] + '.c')
    globs = '#include "df_globals.h"\n'
    lay = layout_for(name)
    if lay.get('early') or lay.get('late') or lay.get('shift') or lay.get('gtype'):
        globs = globs.replace('#include "df_globals.h"\n',
                              '#include "%s"\n' % reordered_globals(name, lay).replace('\\', '/'))
    if lay.get('mid'):
        # before the globals: moves every project symbol, the globals included
        globs = ''.join('extern int df_pad_mid_%d;\n' % k for k in range(lay['mid'])) + globs
    with open(src, 'w', encoding='utf-8') as f:
        pre = ''.join('extern int df_pad_pre_%d;\n' % k for k in range(lay.get('pre', 0)))
        f.write(cfg.PRELUDE.replace('#include "df_types.h"\n', pre + '#include "df_types.h"\n', 1)
                .replace('/*@@GLOBALS@@*/', globs))
        f.write('#include "%s"\n' % os.path.abspath(path).replace('\\', '/'))
    obj = src[:-2] + '.obj'
    pdir = os.path.join(TU, 'pruned', name[:-2])
    pruned_protos(pdir, name, path)
    extra = ['/I' + os.path.abspath(pdir)]
    if lay.get('hdr') or lay.get('ishift'):
        extra = ['/I' + private_windows_h(name, lay.get('hdr') or {}, lay.get('ishift'))] + extra
    rc, log = run(cl(extra + ['/I' + os.path.join(ROOT, 'src'), '/Fo' + obj, src]))
    open(src[:-2] + '.log', 'w').write(log)
    errs = [l for l in log.splitlines() if ' error ' in l]
    return (obj if rc == 0 and os.path.exists(obj) else None), errs


def unowned_globals_c():
    """src/data/globals.c without the definitions some unit owns (each unit defines its own in
    its compiled copy); what is left is the packed regions and anything unowned."""
    defs = global_definitions()
    owned = set(defs[n] for n in defs if owner_of(n))
    path = os.path.join(OUT, 'globals_unowned.c')
    with open(path, 'w', encoding='utf-8') as f:
        for l in open(GLOBALS_C, encoding='utf-8'):
            if l.rstrip('\n') not in owned:
                f.write(l)
    return path


def compile_data(path):
    stem = os.path.basename(path)[:-2]
    src = os.path.join(OUT, stem + '.wrap.c')
    use = unowned_globals_c() if stem == 'globals' else os.path.join(ROOT, 'src', path)
    with open(src, 'w', encoding='utf-8') as f:
        f.write(cfg.PRELUDE.replace('/*@@GLOBALS@@*/', ''))
        f.write('#include "%s"\n' % os.path.abspath(use).replace('\\', '/'))
    obj = src[:-2] + '.obj'
    rc, log = run(cl(['/I' + os.path.join(ROOT, 'src'), '/Fo' + obj, src]))
    return (obj if rc == 0 else None), log


# --- COFF helpers -----------------------------------------------------------------------------

def renamed_obj(src, dst, mapping):
    """A copy of the COFF object `src` with the symbols in `mapping` renamed (a short name in
    place, a long one appended to the string table)."""
    d = bytearray(open(src, 'rb').read())
    symptr, nsym = struct.unpack_from('<II', d, 8)
    strtab = symptr + nsym * 18
    i = 0
    while i < nsym:
        off = symptr + 18 * i
        nm = bytes(d[off:off + 8])
        if nm[:4] == b'\0\0\0\0':
            p = strtab + struct.unpack_from('<I', nm, 4)[0]
            nm = bytes(d[p:d.index(b'\0', p)])
        else:
            nm = nm.rstrip(b'\0')
        new = mapping.get(nm.decode('latin-1'))
        if new:
            new = new.encode()
            if len(new) <= 8:
                d[off:off + 8] = new.ljust(8, b'\0')
            else:
                size = struct.unpack_from('<I', d, strtab)[0]
                d += new + b'\0'
                struct.pack_into('<I', d, strtab, size + len(new) + 1)
                d[off:off + 8] = struct.pack('<II', 0, size)
        i += 1 + d[off + 17]
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    open(dst, 'wb').write(d)
    return dst


def alias_obj(path, aliases):
    """A section-less COFF object holding one weak external per alias
    (IMAGE_WEAK_EXTERN_SEARCH_ALIAS): `weak` resolves to `target`."""
    strtab = bytearray(b'\0\0\0\0')
    syms = []

    def name(n):
        b = n.encode()
        if len(b) <= 8:
            return b.ljust(8, b'\0')
        off = len(strtab)
        strtab.extend(b + b'\0')
        return struct.pack('<II', 0, off)

    for weak, target in aliases.items():
        ti = len(syms)
        syms.append(struct.pack('<8sIhHBB', name(target), 0, 0, 0x20, 2, 0))       # EXTERNAL, undefined
        syms.append(struct.pack('<8sIhHBB', name(weak), 0, 0, 0x20, 105, 1))       # WEAK_EXTERNAL
        syms.append(struct.pack('<II10s', ti, 3, b''))                             # aux: tag, SEARCH_ALIAS
    struct.pack_into('<I', strtab, 0, len(strtab))
    hdr = struct.pack('<HHIIIHH', 0x14c, 0, 0, 20, len(syms), 0, 0)
    open(path, 'wb').write(hdr + b''.join(syms) + bytes(strtab))
    return path


def map_functions(path):
    """{name: va} for the functions in a link.exe map."""
    out = {}
    for l in open(path, errors='replace'):
        m = re.match(r'\s*[0-9a-f]{4}:[0-9a-f]{8}\s+_(\w+?)(@\d+)?\s+([0-9a-f]{8})\s', l)
        if m:
            out[m.group(1)] = int(m.group(3), 16)
    return out


def stamp_image_metadata(path):
    b = bytearray(open(path, 'rb').read())
    pe = struct.unpack_from('<I', b, 0x3c)[0]
    struct.pack_into('<I', b, pe + 8, IMAGE_METADATA['pe_timestamp'])
    n = struct.unpack_from('<H', b, pe + 6)[0]
    so = pe + 24 + struct.unpack_from('<H', b, pe + 20)[0]
    for i in range(n):
        nm, vs, va, rs, ro = struct.unpack_from('<8sIIII', b, so + 40 * i)
        if nm.startswith(b'.rsrc'):
            def walk(off):
                struct.pack_into('<I', b, ro + off + 4, IMAGE_METADATA['rsrc_timestamp'])
                nn, ni = struct.unpack_from('<HH', b, ro + off + 12)
                for k in range(nn + ni):
                    ptr = struct.unpack_from('<I', b, ro + off + 16 + 8 * k + 4)[0]
                    if ptr & 0x80000000:
                        walk(ptr & 0x7fffffff)
            walk(0)
    open(path, 'wb').write(bytes(b))


# --- the 1996 linker --------------------------------------------------------------------------
# LINK.EXE 4.20 sorts the import table (one qsort over every .idata$5 contribution, keyed on the
# DLL name only, so entries of one DLL are ties) with the qsort it imports from MSVCRT40.DLL. On
# a modern Windows that resolves to today's msvcrt.dll, whose qsort leaves ties in a different
# order than 1996's: the same objects linked the same way get a different import table. So the
# link runs from a copy of LINK.EXE beside the original VC++ 4.x MSVCRT40.DLL (an
# application-directory DLL wins: msvcrt40 is not a KnownDLL). MSVCRT40.DLL is taken from the
# VC++ 4.2 disc 1 self-extractor with 7-Zip.

def linker_1996():
    exe = os.path.join(LINK96, 'LINK.EXE')
    if not os.path.exists(exe) or not os.path.exists(os.path.join(LINK96, 'MSVCRT40.DLL')):
        os.makedirs(LINK96, exist_ok=True)
        for f in ('LINK.EXE', 'MSPDB41.DLL'):
            shutil.copy(os.path.join(cfg.vc42(), 'BIN', f), os.path.join(LINK96, f))
        if not os.path.exists(os.path.join(LINK96, 'MSVCRT40.DLL')):
            cfg.need(cfg.DISC1, 'the VC++ 4.2 disc 1 image', 'TITANIC_DISC1')
            subprocess.run([cfg.SEVENZIP, 'e', '-y', '-o' + LINK96, cfg.DISC1, r'MSDEV\REDIST\ANSI\MSVCRT40.DLL'],
                           check=True, capture_output=True)
    return os.path.abspath(exe)


def build_resources():
    """(.res path or None, log lines): res/titanic.rc compiled with the 1996 rc.exe, then the
    icons, menu, dialogs and accelerators (res/typed/, the game's own bytes: no resource
    compiler accepts raw bytes for those predefined types) appended in the order the original's
    .rsrc holds them (res/data_order.json)."""
    res = os.path.join(OUT, 'titanic.res')
    rc, log = run([os.path.join(cfg.vc42(), 'BIN', 'RC.EXE'), '/fo' + res, '/i' + os.path.join(ROOT, 'res'),
                   os.path.join(ROOT, 'res', 'titanic.rc')])
    if rc != 0:
        return None, [l for l in log.splitlines() if l.strip()][:3]
    full = os.path.join(OUT, 'titanic_full.res')
    subprocess.run([sys.executable, os.path.join(ROOT, 'res', 'append_typed_res.py'), res, full,
                    '--order-file', os.path.join(ROOT, 'res', 'data_order.json')], check=True)
    return full, []


def main():
    argv = sys.argv[1:]
    check = '--check' in argv
    import bss
    os.makedirs(OUT, exist_ok=True)
    bss.main()                                  # build/bss_pads.json: the dead space of each unit's bss block
    units = read_units()
    # unit copies from an older layout would be compiled twice
    live = {os.path.splitext(os.path.basename(p))[0] for k, p in units}
    udir = os.path.join(OUT, 'units')
    for f in os.listdir(udir) if os.path.isdir(udir) else []:
        if f.split('.')[0] not in live:
            os.remove(os.path.join(udir, f))
    objs, failed = [], []
    for kind, p in units:
        name = os.path.basename(p)
        if kind == 'data':
            obj, log = compile_data(p)
            if obj is None:
                failed.append((name, [l for l in log.splitlines() if 'error' in l][:3]))
            else:
                objs.append(obj)
            continue
        copy = place_globals(name, os.path.join('src', p))
        obj, errs = compile_unit(name, copy)
        if obj is None:
            failed.append((name, errs[:3]))
        else:
            objs.append(renamed_obj(obj, os.path.join(OUT, 'units', os.path.basename(obj)), CRT_RENAMES))
    objs.append(alias_obj(os.path.join(OUT, 'crt_alias.obj'), CRT_ALIASES))
    dead = os.path.join(OUT, 'link_dead.obj')
    # /MT like every unit: without it the object asks for LIBC, and OLDNAMES' strlwr/strupr
    # then resolve there instead of LIBCMT
    rc, log = run(cl(['/Fo' + dead, os.path.join(HERE, 'link_dead.c')]))
    if rc != 0:
        failed.append(('link_dead.c', [l for l in log.splitlines() if 'error' in l][:3]))
    else:
        objs.append(dead)
    # imports only discarded code referenced still take places in LINK's qsort of the IAT:
    # stand-in references in the object copies make it the original's (import_order.py)
    if not failed:
        by_stem = {os.path.basename(o).split('.')[0]: o for o in objs}
        for stem, fn, names in import_order.DEAD_REFS:
            import_order.insert_refs(by_stem[stem], fn, names)
    res, errs = build_resources()
    if res is None:
        failed.append(('titanic.rc', errs))
    for name, errs in failed:
        print('COMPILE FAILED %s:' % name)
        for e in errs:
            print('   ', e.strip()[-160:])
    if failed and '--force' not in argv:
        print('%d inputs failed; fix them or pass --force to link the rest' % len(failed))
        return 1
    exe = os.path.join(OUT, 'TI_original.exe')
    # /VERSION:4.0: the image version is 4.0 (header +0x2c)
    cmd = ([linker_1996(), '/nologo'] + ['/INCLUDE:' + n for n in STRING_KEEPS] +
           ['/SUBSYSTEM:WINDOWS', '/BASE:0x400000', '/VERSION:4.0',
            '/MAP:' + os.path.join(OUT, 'TI.map'), '/OUT:' + exe] + objs + ([res] if res else []) + LIBS)
    rc, log = run(cmd)
    lines = [l for l in log.splitlines() if l.strip()]
    unresolved = [l for l in lines if 'unresolved external' in l]
    dup = [l for l in lines if 'already defined' in l]
    other = [l for l in lines if l not in unresolved and l not in dup]
    print('link: exit %d, %d unresolved, %d duplicate, %d other' % (rc, len(unresolved), len(dup), len(other)))
    for l in (other + dup + unresolved)[:60]:
        print('   ', l.strip()[-170:])
    open(os.path.join(OUT, 'link.log'), 'w').write(log)
    if rc:
        return rc
    stamp_image_metadata(exe)
    print('built', exe, os.path.getsize(exe), 'bytes (CyberFlix, December 1996, as released)')
    # GOG's hex patches: the source compiles to the unpatched original, so the patched copy is
    # made from it afterwards, each site found through its function's address in this link's map
    gog = os.path.join(OUT, 'TI_gog.exe')
    shutil.copyfile(exe, gog)
    applied, skipped = gog_patches.apply_to_pe(gog, map_functions(os.path.join(OUT, 'TI.map')))
    print('GOG patches: %d applied, %d skipped' % (len(applied), len(skipped)))
    for fn, site, how in applied:
        print('    %-32s 0x%08x %s' % (fn, site, how))
    for fn, why in skipped:
        print('    SKIPPED %-24s %s' % (fn, why))
    print('built', gog, os.path.getsize(gog), 'bytes (GOG)')
    rc = 0
    if check:
        for mine, ref, what in ((exe, cfg.EXE_UNPATCHED, 'TITANIC_EXE_UNPATCHED'), (gog, cfg.EXE, 'TITANIC_EXE')):
            if not ref:
                print('--check: %s is not set; %s not compared' % (what, os.path.basename(mine)))
                continue
            same = open(mine, 'rb').read() == open(ref, 'rb').read()
            print('%s %s %s' % (os.path.basename(mine), 'IDENTICAL to' if same else 'DIFFERS from', ref))
            rc |= 0 if same else 1
    return rc


if __name__ == '__main__':
    sys.exit(main())
