#!/usr/bin/env python
"""
cold_build.py -- prove that this tree, copied somewhere clean, builds the exes byte for byte.

    python match/cold_build.py [--ref-original EXE] [--ref-gog EXE] [--no-lock-check]

The normal build (match/build.py) runs in a tree that carries state no step wrote this time:
compiled copies, the 1996 linker set-up, bss pads. This copies only the tracked-looking source
(everything except build/, .git and caches) into build/cold/<stamp>/ and builds there from
nothing, in the documented order:

  1. match/toolchain_lock.py --check (the VC++ 4.2 binaries, headers, libraries, MSVCRT40.DLL);
  2. copy the tree;
  3. match/build.py in the copy (bss pads, the 1996 linker set-up from the disc image, every
     unit, resources, link, the GOG patch step);
  4. compare the two exes with their references: the unpatched December 1996 build
     (TITANIC_EXE_UNPATCHED, or --ref-original) and GOG's TI.EXE (TITANIC_EXE / --ref-gog).

Exit 0 only when both are byte-identical. Progress is logged with timestamps to
build/cold/progress.log; the result to build/cold/report.md.
"""
import datetime
import hashlib
import os
import shutil
import stat
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import config as cfg           # noqa: E402
import toolchain_lock as tl    # noqa: E402

ROOT = cfg.ROOT
COLD = os.path.join(ROOT, 'build', 'cold')
PROGRESS = os.path.join(COLD, 'progress.log')
SKIP_DIRS = {'build', '.git', '__pycache__', '.vscode', '.claude'}


def log(msg):
    line = '[%s] %s' % (datetime.datetime.now().strftime('%H:%M:%S'), msg)
    print(line, flush=True)
    os.makedirs(COLD, exist_ok=True)
    with open(PROGRESS, 'a', encoding='utf-8') as f:
        f.write(line + '\n')


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def rmtree(path):
    def onerr(fn, p, exc):
        os.chmod(p, stat.S_IWRITE)
        fn(p)
    if os.path.exists(path):
        shutil.rmtree(path, onerror=onerr)


def copy_tree(dst):
    n = 0
    for d, dirs, files in os.walk(ROOT):
        dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
        for f in files:
            if f.endswith('.pyc'):
                continue
            s = os.path.join(d, f)
            t = os.path.join(dst, os.path.relpath(s, ROOT))
            os.makedirs(os.path.dirname(t), exist_ok=True)
            shutil.copy2(s, t)
            n += 1
    return n


def first_difference(a, b):
    n = min(len(a), len(b))
    diffs = [i for i in range(n) if a[i] != b[i]]
    return len(diffs), diffs[:8]


def main():
    argv = sys.argv[1:]
    ref_orig = argv[argv.index('--ref-original') + 1] if '--ref-original' in argv else cfg.EXE_UNPATCHED
    ref_gog = argv[argv.index('--ref-gog') + 1] if '--ref-gog' in argv else cfg.EXE
    os.makedirs(COLD, exist_ok=True)
    open(PROGRESS, 'a').write('\n')
    log('cold build starts')
    if '--no-lock-check' not in argv:
        log('step 1 of 4: toolchain lock check')
        problems = tl.check(log)
        if problems:
            for p in problems:
                log('   ' + p)
            log('STOP: the toolchain is not the locked one (--no-lock-check overrides)')
            return 2
        log('   all %d inputs as recorded' % len(tl.load()['files']))
    label = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    clone = os.path.join(COLD, label)
    log('step 2 of 4: copy the tree into %s' % clone)
    log('   %d files copied' % copy_tree(clone))
    log('step 3 of 4: match/build.py in the copy')
    t0 = time.time()
    logfile = os.path.join(COLD, label + '.build.log')
    r = subprocess.run([sys.executable, os.path.join('match', 'build.py')], cwd=clone, capture_output=True,
                       text=True, errors='replace')
    open(logfile, 'w', encoding='utf-8', errors='replace').write(r.stdout + r.stderr)
    log('   exit %d in %.0fs; log %s' % (r.returncode, time.time() - t0, logfile))
    for l in [l for l in (r.stdout + r.stderr).splitlines() if l.strip()][-3:]:
        log('      ' + l[:150])
    exes = {'original': os.path.join(clone, 'build', 'link', 'TI_original.exe'),
            'gog': os.path.join(clone, 'build', 'link', 'TI_gog.exe')}
    if r.returncode or not all(os.path.exists(p) for p in exes.values()):
        log('STOP: the build failed')
        return 2
    log('step 4 of 4: compare with the references')
    rows, ok = [], True
    for key, ref, what in (('original', ref_orig, "CyberFlix's December 1996 exe as released"),
                           ('gog', ref_gog, "GOG's TI.EXE")):
        if not ref or not os.path.exists(ref):
            rows.append((key, what, 'no reference supplied', sha256(exes[key]), ''))
            ok = False
            log('   %s: no reference supplied' % key)
            continue
        a, b = open(exes[key], 'rb').read(), open(ref, 'rb').read()
        same = a == b
        ok &= same
        detail = '' if same else '%d bytes differ (first offsets %s), sizes %d / %d' % (
            (first_difference(a, b)[0],) + (', '.join('0x%x' % o for o in first_difference(a, b)[1]),) + (len(a), len(b)))
        rows.append((key, what, 'IDENTICAL' if same else 'DIFFERS', sha256(exes[key]), detail))
        log('   %s: %s (%s)' % (key, 'IDENTICAL' if same else 'DIFFERS', sha256(exes[key])))
    report = ['# Cold build\n\n',
              'Generated by `match/cold_build.py` on %s from a copy of the tree in `%s`.\n\n'
              % (datetime.datetime.now().strftime('%Y-%m-%d %H:%M'), os.path.relpath(clone, ROOT).replace('\\', '/')),
              '| exe | reference | result | sha256 | detail |\n|---|---|---|---|---|\n']
    for key, what, res, h, detail in rows:
        report.append('| %s | %s | %s | `%s` | %s |\n' % (key, what, res, h, detail))
    open(os.path.join(COLD, 'report.md'), 'w', encoding='utf-8').write(''.join(report))
    log('done: %s' % ('REPRODUCED' if ok else 'NOT REPRODUCED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
