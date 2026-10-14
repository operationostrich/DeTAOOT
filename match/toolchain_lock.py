#!/usr/bin/env python
"""
toolchain_lock.py -- pin every external input of the match build by content hash.

A changed compiler, header or library would move bytes in ways that are hard to debug, so the
build's inputs that are not in this tree are recorded in toolchain.lock.json and checked:

  * the Visual C++ 4.2 programs the build invokes -- CL.EXE (and the C1.EXE / C2.EXE it
    spawns), LINK.EXE (and the CVTRES.EXE it runs for the .res), RC.EXE -- plus every DLL under
    MSDEV\\BIN any of them imports (MSPDB41.DLL, RCDLL.DLL, ...), found by reading their import
    tables;
  * every header under MSDEV\\INCLUDE (the units pull them in through <windows.h>);
  * the libraries the link resolves from: the five import libraries, LIBCMT.LIB and OLDNAMES.LIB
    (LIBCMT's default-library directive);
  * the Feb 1996 MSVCRT40.DLL that LINK.EXE runs beside, hashed as 7-Zip extracts it from the
    disc 1 self-extractor, and that self-extractor itself;
  * the two reference exes (GOG's TI.EXE, and the unpatched December 1996 build).

The MinGW toolchain is not an input of the match build (it builds the playable port only).

    python match/toolchain_lock.py --write     # (re)write match/toolchain.lock.json
    python match/toolchain_lock.py --check     # exit 1 on any missing, changed or new file

Paths are recorded relative to three roots (the VC++ directory, the folder holding the exes, the
disc image), so the lock does not depend on where a machine keeps them.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import config as cfg   # noqa: E402
import pe as peutil    # noqa: E402

LOCK = os.path.join(cfg.MATCH, 'toolchain.lock.json')
# the programs the link build runs; their imported BIN DLLs are added by binaries()
ENTRY_BINARIES = ['CL.EXE', 'C1.EXE', 'C2.EXE', 'LINK.EXE', 'CVTRES.EXE', 'RC.EXE']
LIBRARIES = ['KERNEL32.LIB', 'USER32.LIB', 'GDI32.LIB', 'COMDLG32.LIB', 'WINMM.LIB', 'LIBCMT.LIB', 'OLDNAMES.LIB']
MSVCRT40_IN_SFX = r'MSDEV\REDIST\ANSI\MSVCRT40.DLL'


def roots():
    return {
        'vc42': os.path.abspath(cfg.vc42()),
        'game': os.path.dirname(os.path.abspath(cfg.EXE)) if cfg.EXE else None,
        'disc1': os.path.abspath(cfg.DISC1) if cfg.DISC1 else None,
    }


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def imported_dlls(path):
    """Upper-case names of the DLLs a PE imports ([] for anything the reader cannot parse)."""
    try:
        return sorted(set(dll for dll, _ in peutil.PE(path).imports()))
    except Exception:
        return []


def binaries(bin_dir):
    """ENTRY_BINARIES plus every DLL in bin_dir they import, transitively."""
    present = {f.upper(): f for f in os.listdir(bin_dir)}
    todo, seen = list(ENTRY_BINARIES), []
    while todo:
        n = todo.pop(0).upper()
        if n in seen or n not in present:
            continue
        seen.append(n)
        for dll in imported_dlls(os.path.join(bin_dir, present[n])):
            if dll in present and dll not in seen:
                todo.append(dll)
    return [present[n] for n in seen]


def extract_msvcrt40(dst_dir):
    subprocess.run([cfg.SEVENZIP, 'e', '-y', '-o' + dst_dir, cfg.DISC1, MSVCRT40_IN_SFX],
                   check=True, capture_output=True)
    return os.path.join(dst_dir, 'MSVCRT40.DLL')


def inventory():
    """{key: absolute path} of every external input, keys relative to the roots. A path that
    does not exist is kept (the check reports it missing)."""
    r = roots()
    files = {}
    bin_dir = os.path.join(r['vc42'], 'BIN')
    for f in binaries(bin_dir):
        files['vc42/BIN/' + f] = os.path.join(bin_dir, f)
    inc = os.path.join(r['vc42'], 'INCLUDE')
    for d, _, names in os.walk(inc):
        for f in names:
            p = os.path.join(d, f)
            files['vc42/INCLUDE/' + os.path.relpath(p, inc).replace('\\', '/')] = p
    for f in LIBRARIES:
        files['vc42/LIB/' + f] = os.path.join(r['vc42'], 'LIB', f)
    # the keys name the files by their usual names whatever the user calls their copies
    if cfg.EXE:
        files['game/TI.EXE'] = os.path.abspath(cfg.EXE)
    if cfg.EXE_UNPATCHED:
        files['game/TI_unpatched.exe'] = os.path.abspath(cfg.EXE_UNPATCHED)
    if r['disc1']:
        files['disc1'] = r['disc1']
    return files


def hash_all(files, log=print):
    out = {}
    tmp = tempfile.mkdtemp(prefix='titanic_lock_')
    try:
        if cfg.DISC1 and os.path.exists(cfg.DISC1) and os.path.exists(cfg.SEVENZIP):
            try:
                dll = extract_msvcrt40(tmp)
                out['msvcrt40/MSVCRT40.DLL'] = {'sha256': sha256(dll), 'size': os.path.getsize(dll),
                                                 'from': 'disc1:' + MSVCRT40_IN_SFX}
            except subprocess.CalledProcessError as e:
                log('7-Zip could not extract MSVCRT40.DLL: ' + (e.stderr or b'').decode(errors='replace').strip())
        for key in sorted(files):
            p = files[key]
            if os.path.exists(p):
                out[key] = {'sha256': sha256(p), 'size': os.path.getsize(p)}
            else:
                out[key] = {'missing': True}
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return out


def write():
    files = inventory()
    hashes = hash_all(files)
    missing = [k for k, v in hashes.items() if v.get('missing')]
    if missing:
        print('NOT WRITTEN: %d inputs missing: %s' % (len(missing), ', '.join(missing)))
        return 1
    import datetime
    lock = {'written': datetime.datetime.now().isoformat(timespec='seconds'), 'roots': roots(), 'files': hashes}
    with open(LOCK, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(lock, f, indent=1, sort_keys=True)
        f.write('\n')
    print('wrote %s: %d inputs' % (LOCK, len(hashes)))
    return 0


def load():
    if not os.path.exists(LOCK):
        return None
    return json.load(open(LOCK, encoding='utf-8'))


def check(log=print):
    """Hashes every input again and compares with the lock: returns the list of problems
    ('missing KEY', 'changed KEY', 'new KEY'), [] when everything is as recorded. The two
    reference exes are optional: a key the user did not supply is skipped."""
    lock = load()
    if lock is None:
        return ['no lock file: run toolchain_lock.py --write']
    want = lock['files']
    have = hash_all(inventory(), log)
    problems = []
    for k in sorted(set(want) | set(have)):
        w, h = want.get(k), have.get(k)
        if k.startswith('game/') and h is None:
            continue
        if w is None:
            problems.append('new %s (not in the lock)' % k)
        elif h is None or h.get('missing'):
            problems.append('missing %s' % k)
        elif w['sha256'] != h['sha256']:
            problems.append('changed %s (%d -> %d bytes)' % (k, w['size'], h['size']))
    return problems


def verify_file(path, key):
    """Does the file at `path` carry the lock's hash for `key`? None when the lock lacks it."""
    lock = load()
    if lock is None or key not in lock['files']:
        return None
    return sha256(path) == lock['files'][key]['sha256']


def main():
    if '--write' in sys.argv:
        return write()
    if '--check' in sys.argv:
        problems = check()
        lock = load()
        if problems:
            print('toolchain lock: %d problems' % len(problems))
            for p in problems:
                print('   ', p)
            return 1
        print('toolchain lock: all %d inputs as recorded (%s)' % (len(lock['files']), lock['written']))
        return 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main())
