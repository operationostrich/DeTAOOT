"""config.py -- where the external inputs are, and the compiler switches.

Nothing here is shipped with the source: the Visual C++ 4.2 toolchain, the 1996 linker's
runtime, the game's own TI.EXE and the unpatched December 1996 build are yours to supply.
Say where they are with environment variables or with a `match/paths.json` of your own
(it is not checked in):

    TITANIC_VC42           the MSDEV directory of Visual C++ 4.2 (holds BIN, INCLUDE, LIB)
    TITANIC_DISC1          en_vc42ent_disc1.exe, the VC++ 4.2 disc 1 self-extractor (the
                           Feb 1996 MSVCRT40.DLL that the 1996 linker runs beside is in it)
    TITANIC_7ZIP           7z.exe (extracts MSVCRT40.DLL from the disc image)
    TITANIC_EXE            GOG's TI.EXE, to check the patched build against
    TITANIC_EXE_UNPATCHED  CyberFlix's December 1996 exe as released, to check the other

    {"vc42": "...", "disc1": "...", "sevenzip": "...", "exe": "...", "exe_unpatched": "..."}
"""
import json
import os

MATCH = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(MATCH)

_local = {}
_p = os.path.join(MATCH, 'paths.json')
if os.path.exists(_p):
    _local = json.load(open(_p, encoding='utf-8'))


def _get(key, env, default=None):
    return os.environ.get(env) or _local.get(key) or default


VC42 = _get('vc42', 'TITANIC_VC42')
DISC1 = _get('disc1', 'TITANIC_DISC1') or (
    os.path.join(os.path.dirname(VC42.rstrip('\\/')), 'en_vc42ent_disc1.exe') if VC42 else None)
SEVENZIP = _get('sevenzip', 'TITANIC_7ZIP', r'C:\Program Files\7-Zip\7z.exe')
EXE = _get('exe', 'TITANIC_EXE')
EXE_UNPATCHED = _get('exe_unpatched', 'TITANIC_EXE_UNPATCHED')


def need(value, what, var):
    if not value:
        raise SystemExit('%s is not set: export %s=<path> (see match/config.py)' % (what, var))
    return value


def vc42():
    return need(VC42, 'the Visual C++ 4.2 MSDEV directory', 'TITANIC_VC42')


# Compiler switches. /Dstatic= : an unreferenced static function is dropped by the compiler;
# DF_FIXES_OFF compiles the original behaviour (include/df_fixes.h); DF_VC42 selects the
# 1996-compiler spellings; DF_BSS_NAMES maps every uninitialised global onto the name that
# makes the compiler lay the bss out in the exe's order (include/df_bss_names.h).
COMMON = ['/nologo', '/c', '/MT', '/W1', '/DDF_FIXES_OFF', '/DSIZE_T=DWORD', '/DDF_VC42', '/Dstatic=',
          '/DDF_BSS_NAMES']
O2 = ['/O2']

PRELUDE = '''/* the match build's prelude (match/config.py) */
/* the 1996 SDK predates the pointer-sized integer names; MinGW-only keywords */
#define DWORD_PTR DWORD
#define ULONG_PTR unsigned long
#define LONG_PTR long
#define UINT_PTR unsigned int
#define INT_PTR int
#define __thread __declspec(thread)
#define DF_TRACE_H
#define DF_TRACE_ENABLED 0
#define DF_TRACE (void)sizeof
#define DF_TRACE_TOKENS(label, p, n) ((void)0)
#define DF_TRACE_ENTER()  ((void)0)
#define DF_TRACE_LEAVE()  ((void)0)
#define DF_COVERAGE_FLUSH() ((void)0)
#define DF_TRACE_RENDER (void)sizeof
/* the 1996 SDK predates these two names */
#ifndef INVALID_SET_FILE_POINTER
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif
/* each readable global name back to the identifier the fitted symbol tables know
   (match/df_match_names.h): the compiled copies only, never the port */
/*@@MATCH_NAMES@@*/
#include "df_types.h"
/*@@GLOBALS@@*/
#include "df_fixes.h"
'''


def match_name_lines():
    """The `#define <port name> <match name>` lines of match/df_match_names.h, spliced into the
    prelude rather than #included: a `#define` is not a symbol to VC++ 4.2, but an #include'd
    FILE is, and the symbol count is what the register allocator's tie-breaks follow."""
    path = os.path.join(MATCH, 'df_match_names.h')
    return ''.join(l for l in open(path, encoding='utf-8')
                   if l.startswith('#define ') and not l.startswith('#define DF_MATCH_NAMES_H'))


PRELUDE = PRELUDE.replace('/*@@MATCH_NAMES@@*/\n', match_name_lines())


def env():
    e = dict(os.environ)
    vc = vc42()
    e['PATH'] = os.path.join(vc, 'BIN') + ';' + e.get('PATH', '')
    e['INCLUDE'] = ';'.join([os.path.join(ROOT, 'include', 'vc42'), os.path.join(vc, 'INCLUDE'),
                             os.path.join(ROOT, 'include')])
    e['LIB'] = os.path.join(vc, 'LIB')
    e['MSYS_NO_PATHCONV'] = '1'
    return e
