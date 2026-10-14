# Building

Two builds share this source. The **port** (MinGW GCC) is the main one; the **exact rebuild**
(Visual C++ 4.2, `match/`) is optional and exists to prove the source reproduces the original
exe to the byte.

## 1. The playable port

32-bit MinGW GCC and GNU Make (the game is a 32-bit Win32 program). On Windows:

1. Install [MSYS2](https://www.msys2.org/) (a Unix shell environment for Windows)
2. Open the **MSYS2 MinGW 32-bit** terminal (not the default MSYS2 terminal)
3. Install the toolchain:
   ```bash
   pacman -S mingw-w64-i686-gcc mingw-w64-i686-make mingw-w64-i686-binutils
   ```
4. Set the PATH (stays set for that terminal session):
   ```bash
   export PATH="/mingw32/bin:$PATH"
   ```

Python 3 is needed too (`res/append_typed_res.py` builds the resource file). From the repo root,
in a POSIX shell (the Makefile uses `mkdir -p`):

| Command | Result |
|---|---|
| `mingw32-make` | `build/TI.EXE`, optimised (`-O1`) |
| `mingw32-make debug` | `build/TI_dbg.EXE`, `-O0 -g3` |
| `mingw32-make trace` | `build/TI_trace.EXE`: debug plus `DF_TRACE` logging, `-fstack-protector-all`, function-entry coverage |
| `mingw32-make nofix` | Compile check with every `DF_FIX_*` switch at 0 (the decoded original); not expected to run |

Turn one fix off: `mingw32-make debug EXTRA_DEFS=-DDF_FIX_CLUT_FADE_REBLIT=0` (after
`make clean`). `include/df_fixes.h` lists every switch; [differences.md](differences.md)
explains each.

### Running it

You need your own installed copy of the game. The engine finds its data next to the exe
(`GetModuleFileNameA`, as the 1996 installer laid it out), so **copy the built exe into the game
folder** and run it from there:

```
TI_dbg.EXE -j -r -skipdisplaycheck -16 -s
```

| Flag | Meaning |
|---|---|
| `-j` | joystick disabled |
| `-r` | the game's own flag: never change the display resolution (without it a modern compositor re-activates the window in a loop) |
| `-skipdisplaycheck` | port-added: skip the 8-bits-per-pixel check a modern desktop cannot pass |
| `-16 -s` | the game's own flags for 16-bit stereo sound; without them the mixer falls to its mono 8-bit last resort and the music is buried in hiss |

[startup-flow.md](startup-flow.md) has the full flag table and the init and teardown order.

If a still-running instance holds the exe in the game folder, copying the new build over it fails:
close the old one first, or you will test the previous build.

### The trace build

`include/df_trace.h` is force-included into every unit, so trace points can stay in the source
permanently; they compile to nothing unless `DF_TRACE` is defined:

```c
DF_TRACE("opcode %d in %s", opcode, name);
DF_TRACE_ENTER();   /* indent nested output */
DF_TRACE_LEAVE();
```

Output goes to `$DF_TRACE_LOG` (or `df_trace.log` in the working directory). Identical
consecutive lines collapse into a repeat count. The log rotates at 3,000,000 lines per file
(`df_trace.log` becomes `df_trace.log.1`): read a session whole with
`cat df_trace.log.1 df_trace.log`. `DF_TRACE_RENDER=1` in the environment adds the per-frame
render walk (very verbose). `DF_POOL_KB=<kilobytes>` shrinks the resource pool so the eviction
path runs within minutes.

The trace build is compiled with `-fstack-protector-all` on purpose. The commonest bug class in
this port is a recovered function that keeps one contiguous record as several adjacent locals
while the original copies the whole record over the first one; under GCC that silently overruns
the frame and crashes somewhere unrelated. With the protector the program stops in
`__stack_chk_fail` *in the offending function*: break there under gdb and read frame 1. Prefer
the trace build over the debug build for chasing a crash. The build also writes
`df_coverage.txt` next to the log: every function that ever ran.

## 2. The exact rebuild (optional)

`match/` reproduces both exes the project is checked against:

* **`TI_original.exe`** - `SHA256 82BFC60ADC07ED708E1C0BBFFB00572C2C372C1A57B434A5E0DF0E899EB14619` -- CyberFlix's 31 December 1996 build as released;  
* **`TI_gog.exe`** - `SHA256 FBA675D696C356EEEF8184015751719FABDC7D95EE13079F9C28B1AF08D08BFB` -- GOG's copy of it, which differs by 25 bytes at four sites (`match/gog_patches.py`).

> NOTE: There is also a version built on 10th Mar 1997 `SHA256 A1D40D0C91949614926E2C3A410E408C7A5530E93B6BA93803BEC51FBBE2DD27` but it's future work get the byte match for that. It will be interesting to see what tweaks and changes were made.

### What you supply

Nothing in the tree is a Microsoft file. Point the build at your own copies:

| Variable | What |
|---|---|
| `TITANIC_VC42` | the `MSDEV` directory of Visual C++ 4.2 (`cl.exe` 10.20.6166, `link.exe` 4.20.6164; holds `BIN`, `INCLUDE`, `LIB`) |
| `TITANIC_DISC1` | `en_vc42ent_disc1.exe`, the VC++ 4.2 disc 1 self-extractor: the Feb 1996 `MSVCRT40.DLL` that the linker must run beside is taken from it with 7-Zip |
| `TITANIC_7ZIP` | `7z.exe` (default `C:\Program Files\7-Zip\7z.exe`) |
| `TITANIC_EXE` | GOG's `TI.EXE`, to check `TI_gog.exe` against (optional) |
| `TITANIC_EXE_UNPATCHED` | the unpatched December 1996 exe, to check `TI_original.exe` against (optional) |

or write them in a `match/paths.json` (not checked in): `{"vc42": "...", "disc1": "...", "exe": "..."}`.
`match/toolchain.lock.json` records the SHA-256 of every external input -- the compiler and linker
and the DLLs they import, every header under `MSDEV\INCLUDE`, the libraries, `MSVCRT40.DLL`, and
the reference exes. `python match/toolchain_lock.py --check` verifies yours against it; a different
compiler build or header set would move bytes in ways that are hard to debug, so `cold_build.py`
refuses to run on a mismatch.

### Building

```
python match/build.py --check
```

compiles each unit of `src/units.txt` whole with `cl /O2` in that order, compiles the resources
with the 1996 `rc.exe`, links with the 1996 `link.exe`, then writes the two exes to `build/link/`
and compares them with the references. About 25 seconds.

`python match/cold_build.py` copies the tree to `build/cold/<stamp>/`, builds there from nothing
and reports whether both exes are identical to the references.

### How the build is made to reproduce the bytes

The 1996 compiler's register choices depend on things the source as recovered does not show:
how many named symbols have been declared so far, locals the original declared and the optimiser
removed, and the order its headers declared things. The match build compiles a *copy* of each
unit with these steering tables applied; `src/` keeps the readable form and the port never sees
them. All are in `match/` and all are listed in [differences.md](differences.md):

| File | Holds |
|---|---|
| `symbol_phase.json` | per function, how many extra symbols the original had declared before it |
| `local_pads.json` | unused locals the original declared |
| `line_patches.json` | lines replaced by spellings that only steer the compiler's numbering |
| `unit_layout.json` | where in each unit's declarations a global or an import sits |
| `globals.tsv`, `owners.tsv`, `df_match_names.h` | every global's address, the identifier the match build knows it by, and the unit whose `.data`/`.bss` block it sits in |
| `functions.tsv` | the address of every game function |
| `bss.py`, `include/df_bss_names.h` | the names that make the compiler lay the `.bss` out in the exe's order, and the padding for variables the original declared and never used |
| `import_order.py`, `link_dead.c` | stand-ins for code the original link discarded whose references still shaped the import table and the `.reloc` size |

Rules for editing a unit so the rebuild keeps matching: a comment that says a spelling is
"load-bearing" or "register choice" means it; anything that adds or removes a named symbol (a
local, a label, a string literal, a named prototype parameter, a typedef, an `#include`) moves
the symbol count and needs the unit's `symbol_phase.json` rows re-fitted. The fitting tools are not
part of this tree.

### Adding or moving code

A new function goes in the file that holds the code around its address, in address order, and its
prototype goes in `include/df_protos.h` (sorted by address) and its address in
`match/functions.tsv`. A new file goes into `src/units.txt` at the position its code had in the
original; the file name and folder are free.
