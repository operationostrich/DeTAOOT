# Every difference between this source and the original TI.EXE

This tree builds, with Visual C++ 4.2 and the 1996 linker, an executable that is
byte-identical to CyberFlix's `TI.EXE` (the 31 December 1996 build, as GOG ships it),
and, before GOG's 25 patched bytes are applied, byte-identical to the unpatched
original. The MinGW build of the same source is the playable port.

The source is nonetheless not what CyberFlix had on disk, in three ways. Each is
documented here, in order of importance.

1. Two functions are hand-pinned assembly, because no C input to this compiler
   produces their bytes.
2. GOG hex-patched 25 bytes after the link. The source compiles to the original;
   the patches are re-applied after linking.
3. Twenty-four places where the playable port deliberately departs from the original
   behaviour, each under a `DF_FIX_*` switch. The byte-matching build compiles with
   all of them off, so none reaches the exe.

A fourth category changes no behaviour and no byte: the tables that steer the 1996
compiler's register and instruction choices (`match/*.json`). They are listed at
the end.

---

## 1. Two functions that are assembly

`AppInit` (0x0041bb40, `src/platform/display_ui.c`) and `QueryMixerVolumeLevel9`
(0x004072d0, `src/sound/wave_device_scene_cmds.c`) are the only functions in the game
whose bytes no C source reaches with this compiler. Under `_MSC_VER` each is a
`__declspec(naked)` body of the original's instructions, with every call, global and
literal expressed symbolically so the linker produces the same relocations; the C we
believe CyberFlix wrote is kept beside it as the GCC branch, and is what the port runs.

### What each one needs

**`AppInit`**, 25 bytes in two places. Before the first `RegisterClassA` call the
original stores the window class's `style`, `cbClsExtra` and `cbWndExtra`, *then*
loads the address of `RegisterClassA` from the import table; every compile of ours
loads it first. And the colour-depth flag is stored to its global one instruction later
than the original places it. Both are instruction-scheduling decisions: the compiler
orders the store before the load only if it believes the two may touch the same
memory. By the rules read out of the compiler itself, an import-table slot never
conflicts with a stack store; the only construct that overrides that is an "aliased"
memory record, which the C++ front end creates for `this` pointers and virtual calls
and the C front end never creates at all.

**`QueryMixerVolumeLevel9`**, 66 bytes. The original stores the constant 0 three
times as separate immediate stores, the high halves of three 64-bit values. The
compiler's back end creates those zeros itself while lowering the integer-to-double
conversion, then its constant-merging pass turns them into one `xor ecx,ecx` and three
register stores. Nothing in the source reaches the point where that decision is made:
every spelling of the conversion arrives at the back end as the same intermediate code.

### What was tried

| Hypothesis | Result |
|---|---|
| Source spellings | Several hundred per function over six passes: statement order, casts, named temporaries, `volatile`, `register`, unions, `__int64`, pointer forms, block scopes, labels, `do {} while (0)`, dead address-takers, empty `__asm {}` blocks, and the compiler-steering tables below. Best 25 and 66 bytes. |
| Forcing the decision inside the compiler | With the one dependence (AppInit) or the one constant weight (QueryMixer) overridden under a debugger, both compile byte-exact. So the diagnosis is certain; only the source that would cause it is missing. |
| GOG's patches | Both functions are identical in the unpatched original. |
| Compiler switches | 25 switch sets (`/Ox /O1 /Os /Oa /Ow /Op /Ob0 /Ob2 /Oi- /Oy- /Og- /G3 /G4 /G5 /GB /Gf- /Gy- /QIfdiv` and mixes). None reaches 0; `/O2` is the only set under which the rest of each unit matches. |
| The C++ front end | Compiled as C++, QueryMixer still shares one zero register and AppInit still hoists the import load. Neither function uses a C++ construct. |
| Visual C++ 4.1 | A different compiler build. It gives the same two residues and matches 215 of the other 218 functions in those two units, so a stale 4.1 object is not the answer. |
| The 4.x patch set (VCPATCH, VC41A, VC41B, VC42B, 42BB1B2, MSDEV42B, SDK42B) | None changes a compiler binary: `C1`, `C1XX`, `C2`, `CL`, `LINK` and `LIBCMT.LIB` are byte-identical before and after. 4.2b updates SDK headers, import libraries, MFC and two wizards; the declarations these functions use are identical. |
| The operating system | The compiler consults nothing environmental when generating code, and 1,303 functions using the same constructs come out identical. |
| Partial inline assembly for just the differing region | Impossible: in a function that is not `naked`, any `push` or explicit `esp` use inside an `__asm` block forces a full frame, and the exe's `push` sits between the stores; a block's interference also recolours the neighbouring code. |

What remains is either source we cannot guess, or a build of the compiler's back end
that Microsoft never released. The hand-pinned bodies are the honest way to a
byte-identical executable while the C stays visible.

### How the assembly bodies are written

- A global named anywhere inside `__asm` is treated by the compiler as referenced by
  assembly for the rest of the file, and later C functions reload it from memory, so
  the bodies address every global and literal as the function's own symbol plus a
  displacement (`DATA_(addr)`). The linker resolves that to the same address and
  relocation the original has.
- String literals cannot be named in inline assembly, and a file's literals are laid
  out in the order the compiler first meets them. The believed C is therefore also
  compiled under VC++ as a dead "keeper" function (`AppInit_Source`,
  `QueryMixerVolumeLevel9_Pool`) that the linker discards, which seeds the literal and
  floating-point constant pools in the original order. Twelve literals only `AppInit`
  uses are kept alive with `/INCLUDE` on the link line.
- `[esp+0]` loses its zero displacement byte in every assembler spelling, so those six
  instructions in `QueryMixerVolumeLevel9` are emitted as raw bytes.

---

## 2. GOG's post-link patches

Our `TI.EXE` is the December 1996 build with 25 bytes hex-edited afterwards, so the
game runs without the CD and leaves the display mode alone on exit. The whole-file
difference between it and the unpatched original (`TI_unpatched.exe`, from GOG's
installer base) is exactly these four sites. The source compiles to the unpatched
original; the match build writes the patched bytes in after linking
(`match/gog_patches.py`).

| Address | Function | Original | GOG | Effect |
|---|---|---|---|---|
| 0x0041dec1 | `FreeDisplayLibrary` | `jz` over `RestoreDisplayMode` | `jmp` | The display mode is never restored on exit (Windows 95 restored it itself; modern Windows does not) |
| 0x00432e59 | `PromptUntilDescriptorVolumePresent` | `jz` into the "insert the CD" prompt loop | two `nop`s | An absent volume counts as present: no CD check |
| 0x00435732 | `FadeCLUTToBlack` | a spin on the 60 Hz tick after each palette-fade step | `push 0x48a880; call PresentBackBufferRect` | No per-step wait; re-applies the channel-B hotspot scroll state instead (the patch also leaves the function returning with `ebp` clobbered) |
| 0x00435782 | `FadeCLUTFromBlack` | the same spin | the same call | the same |

The playable port keeps GOG's behaviour at the three sites that matter on an installed
copy (`DF_FIX_GOG_NO_CD_CHECK`, `DF_FIX_GOG_CLUT_FADE_NO_WAIT`) and the source's
behaviour at the display-mode site (`DF_FIX_REVERT_DISPLAY_MODE`); see the next section.

---

## 3. Deliberate deviations in the playable port

Each is one `DF_FIX_*` switch in `include/df_fixes.h`, default on in the port and off
in the match build (`DF_FIXES_OFF`). With a switch at 0 the site compiles to the decoded
original. These are not decode bugs: in every case the original's behaviour was read from
the binary and the departure is on purpose, because the original behaviour is broken on a
modern system, or because the original has a bug the port chooses not to reproduce.

| Switch | Where | The original | The port |
|---|---|---|---|
| `DF_FIX_CLUT_FADE_REBLIT` | `MovieFadeIn` 0x0044bd60 / `...Reverse` 0x0044bce0 | Fades by `AnimatePalette` alone, which recoloured every pixel on an 8bpp screen | On a true-colour display that changes nothing already drawn, so each fade step re-blits the scene window |
| `DF_FIX_SKIP_DISPLAY_CHECK` | `AppInit` 0x0041bb40 | `CheckDisplayColourDepth` puts up a modal box on any non-8bpp desktop (the real exe gets past it through a Windows compatibility shim a fresh build does not receive) | A port-added `-skipdisplaycheck` flag skips the check; inert unless passed |
| `DF_FIX_OWN_WINDOWS_ONLY` | `RestoreSavedWindowPositions` 0x0041b8c0 | `MoveWindow`s every top-level window on the desktop | Only this process's windows: today the original blocks forever on a window whose thread is not pumping messages |
| `DF_FIX_SYSCOLOR_RESTORE_GUARD` | `ManageSystemPalette(0)` 0x0041b6e0 | Restores the 25 system colours unconditionally | On a true-colour display nothing was ever saved (`GetSystemPaletteUse` returns `SYSPAL_ERROR`), so the original wrote 25 black colours into the live theme on every clean exit; restore only what was saved |
| `DF_FIX_REVERT_DISPLAY_MODE` | `FreeDisplayLibrary` 0x0041deb0 | The source calls `RestoreDisplayMode` on exit (GOG patched the call out, see section 2) | Keep the source's call: modern Windows does not restore a changed mode when the process ends |
| `DF_FIX_GOG_NO_CD_CHECK` | `PromptUntilDescriptorVolumePresent` 0x00432e20 | An "insert the CD" prompt loop, ejecting the drive | GOG's behaviour: an absent volume counts as present |
| `DF_FIX_GOG_CLUT_FADE_NO_WAIT` | `FadeCLUTFromBlack` 0x00435750 / `...Reverse` 0x00435700 | A 60 Hz spin-wait after each palette-fade step | GOG's behaviour: no wait, the hotspot-scroll call instead (what the port has always run; the clobbered `ebp` is not reproduced) |
| `DF_FIX_VIEWPORT_SLOT_GUARD` | `DestroyChannelViewport` 0x00435cf0 and every walk of the viewport table | Frees a viewport and leaves its table slot dangling; no function ever clears a slot (survivable when freed blocks stayed mapped in the engine's own pool) | Clears the slot and probes each slot before use |
| `DF_FIX_WAVE_RING_LOCK` | `WaveOutCallback` 0x00406ed0, `DequeueWaveOutBuffer` 0x00406f50, `SubmitWaveOutBuffer` 0x00406fe0, `OpenSoundDevice` 0x004011a0 | No lock around the waveOut ring counters: Win9x ran the completion callback on the caller's thread | Modern winmm uses a worker thread that races them; a critical section the original does not have |
| `DF_FIX_REPAINT_ON_ACTIVATE` | `HandleActivateAppMessage` 0x0041b480 | After an alt-tab restore, repaints only through the paint event the main loop dequeues; inside a script loop that owns the thread, only dirty rows repaint | Also raises the scene redraw level, so the next tick repaints everything |
| `DF_FIX_SKIP_IDLE_CHANNEL_STOP` | `StopSoundChannels` 0x004248e0 | Every stop flushes the wave device (`waveOutReset`, near-free on Win9x) | Modern `waveOutReset` blocks ~9 ms and plays nothing meanwhile; the radio tuner's per-tick `halttheme` became a buzz. A stop whose channels are all idle returns before the flush |
| `DF_FIX_PARSE_INT_BUFFER` | `ParseLeadingIntFromPascalString` 0x00435880 | Copies the Pascal body into a 20-byte stack buffer with no length check | A 32-byte buffer, copy clamped to 31. Defensive: no script comes near either limit |
| `DF_FIX_CONTAINER_SHARE_MODES` | `ProbeContainerFile` 0x00423c60, `OpenContainerFile` 0x004239b0 | Exclusive existence probe; read-only fallback shares read only | Both share read and write, so the probe does not collide with a container still open (a script `reboot`, a second instance). Flagged in the source as a suspect: it may hide a decode bug in the container reference counting |
| `DF_FIX_GAMELOOP_SEED_SCENESTATE` | `RunGameLoop` 0x004316e0 | Passes a stack slot it never writes into `QdSetPort` and the hotspot paths; the stale contents happened to be harmless | Seeds the slot from the scene-state snapshot each frame (garbage reached `QdSetPortWindow` on the port and crashed) |
| `DF_FIX_ZERO_INTERACTION_FRAME` | `DispatchBootMessageString` 0x0042a470 | Fills only the fields of its stack `MethodCallFrame` that it uses | Zeroes the frame first. No reader of an unset field is known |
| `DF_FIX_ABOVE_NORMAL_PRIORITY` | `AppInit` 0x0041bd4e, `PumpMessages` 0x0041eaa0 | `HIGH_PRIORITY_CLASS` for the whole run | `ABOVE_NORMAL`: high priority starves the rest of a modern desktop |
| `DF_FIX_REVERT_TO_DEFAULT_MODE` | `RevertDisplayMode` 0x0041c670 | Applies the chosen display mode with `CDS_UPDATEREGISTRY` (permanent) and offers a reboot | Restores the default mode and offers a log-off |
| `DF_FIX_SOUND_TEMPLATE_QUEUE_HEAD` | `QueueWaveFormatOnSoundChannel` 0x004019c0 | On an empty deferred list, stores the new node through an unwritten stack slot and never sets the head: an original bug that loses the entry | Sets the list head |
| `DF_FIX_BOOTSTRAP_HEADER_BUFFER` | `BootstrapContainerFile` 0x00422310 | Writes at least 0x600 bytes from a 0x400-byte stack header of which 0x200 are set: stack contents go into every new container file | A zero-filled buffer of the full size |
| `DF_FIX_WRITABLE_PASCAL_LITERALS` | the 168 Pascal-string constants (`DAT_0045b128` on) | String literals whose length byte the engine writes in place: legal in 1996, when the compiler put literals in writable memory | GCC makes literals read-only, so each is a writable array (`src/globals.c`) |
| `DF_FIX_SAVE_SCRATCH_ZEROED` | `WriteSaveGameFile` 0x004139a0 | Builds the save image in a stack block it never clears; the gaps between written fields are stale stack, so two saves of one state differ on disk | Zeroes the block first; the loader reads only the written fields either way |
| `DF_FIX_CMDLINE_BOUNDED_COPY` | `AppInit` 0x0041bb40 | Copies the command line into a 512-byte stack buffer with an unchecked `strcpy` (Win95 capped a command line near 127 characters; today it can be 32767) | A bounded copy, and the 64-byte title scratch widened to 512 |

Every switch can be turned off singly (`mingw32-make debug EXTRA_DEFS=-DDF_FIX_NAME=0`)
or all at once (`mingw32-make nofix`, a compile check: several fixes exist because the
original hangs or deadlocks on a modern desktop). A lab audit script (not part of this tree)
checked that every switch was registered, used and documented; keep to that.

---

## 4. Compiler-steering tables (no behaviour, no bytes)

The 1996 compiler breaks register and instruction-order ties by things that are not in
the source as we have it: the count of named symbols declared so far, local variables
the original declared and the optimiser removed, and spellings that only shift its
internal numbering. The match build applies four tables to a *copy* of each source file
before compiling; `src/` keeps the readable form, and the port never sees them.

| Table | What it holds |
|---|---|
| `match/symbol_phase.json` | Per function, how many extra symbols the original had declared before it (modulo 65536) |
| `match/local_pads.json` | Unused locals the original declared, inserted before a named local |
| `match/line_patches.json` | A line replaced by a spelling that steers the compiler's temporary and web numbering with no code: self-cancelling pointer chains, a dead `&v` that keeps a variable in memory, an empty `__asm {}` that adds interference, a cast-form increment |
| `match/unit_layout.json` | Where in each file's declarations a global or import sits |

Every entry is in the table files in `match/`, keyed by file and function. None changes what the code computes: the behaviour check for the largest set (the
scaled-blit function, 51 lines) ran 24,000 random inputs through both forms with
identical output.

---

## 5. What the link step adds (no behaviour, no source)

Linking the units as they are would not give the original's layout. The link build adds the
following, none of it in `src/` and none of it code the game runs:

| What | Why |
|---|---|
| **Each unit defines the globals it owns**, where the original's source had them | TI.EXE's `.data` and `.bss` hold one block per file in link order; the owner of a global is the unit whose block it sits in (`owners.tsv` in the export) |
| **`.bss` names** (`include/df_bss_names.h`, `DF_BSS_NAMES`) | VC++ 4.2 lays an object's uninitialised variables out by a hash of their names; the original names are lost, so each global has a name whose hash puts it at its address |
| **`.bss` pads** (`char df_bsspad_...[n]`, after the last function of a unit) | Variables the original declared and never used still take their slot |
| **`link_dead.c`**: a use of the double 1.0, and 140 stores | Stand-ins for code `/OPT:REF` discarded whose references still shaped the link: a constant that survives in `.rdata`, and the `.reloc` section's size (2 bytes per relocation of every input the linker loaded) |
| **Stand-in import references** (`DEAD_REFS`) | Imports only discarded code used still take a place in the linker's sort of the import table |
| **CRT symbol renames and one weak alias** | The game's CRT calls are linked under the names the original called them by (`_sprintf`, `_strupr` ...), which decides the order the linker loads `LIBCMT.LIB`'s members in; the one copy routine serves `memcpy` and `memmove` sites alike |
| **`/INCLUDE` of twelve string literals** | `AppInit` is assembly (section 1) and names its literals as `AppInit + offset`, which the linker would otherwise discard as unreferenced |
| **The 1996 linker beside the Feb 1996 `MSVCRT40.DLL`** | Its import-table `qsort` is that DLL's; on a modern Windows the same objects sort differently |
| **Resource order** (`res/data_order.json`) | `cvtres` lays resource data out in `.res` order, which must be the original `.rc` order |
| **Image time stamps** | The PE header and the resource directories carry the link time (31 December 1996); written after the link |
| **The keyword tables' object placed right after `display_ui`** | That is where the original's data put the keyword strings |

`toolchain.lock.json` pins every external input of the link by SHA-256.
