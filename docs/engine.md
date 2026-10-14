# DreamFactory Engine — Architecture Overview

Background and subsystem map for the DreamFactory v4.0 engine as implemented in *Titanic: Adventure Out of Time* (1996, CyberFlix, Inc.).

---

## Identity

| Field | Value |
|---|---|
| Engine | DreamFactory v4.0 — Windows RT4 engine, standard version |
| Developer | CyberFlix, Inc. |
| Build date | Dec 31 1996 at 16:47:05 |
| Binary type | Win32 PE, MSVC |
| Entry point | `entry()` CRT stub at `0x0044EA00` |
| WinMain | `0x004321d0` |
| Display | 8 bpp (256 colours), 640×480, WinG or CreateDIBSection |
| Audio | WaveOut via WINMM.DLL; requires 22 kHz device |
| Memory | FIXMEM.VXD / VFIXMEM on Win9x; standard heap on NT |

---

## Macintosh Lineage

CyberFlix's first titles -- *Lunicus* and *Jump Raven* (1993), then *Dust*
(1995) -- shipped for the Macintosh first, with Windows versions following, and
DreamFactory was the in-house engine across all of them. `TI.EXE` reads as that
Mac-shaped engine carried to Win32 **with its own runtime services** rather than
rewritten on the Windows ones. The reading of
the binary below is an inference from its shape, and it is the reason so many of
the port's bugs have a Mac flavour (pack use counts, locked-vs-movable blocks,
"resource not found" meaning "no room").

The Mac build itself survives: the US Disc 1 is a hybrid CD whose HFS side holds
`INSTALL_MAC/Titanic`, a fat application (PowerPC PEF in the data fork, 68k CODE
resources in the resource fork) with no symbols, whose `DFRT` resource reads
"DreamFactory 3.0 (c) 1994 CyberFlix, Inc." where the Windows exe says 4.0. Its
platform layer is pure Toolbox (199 InterfaceLib/MathLib imports: Memory
Manager handles, QuickDraw, Sound Manager double-buffering). It served as a
link-order oracle for the original file boundaries (`src/units.txt` is the result).

| In the binary | Macintosh convention it mirrors |
|---|---|
| Handle-based relocatable heap: handles resolved through descriptors, lock counts that pin a block (`GetResDataPtr` / `UnlockResObject`), compaction that slides unlocked blocks together (`CompactHeap`), an LRU purge across every open container when space runs low (`EvictCachedChildrenForSpace`). See Resource System below | Memory Manager: `Handle`, `HLock`/`HUnlock`, `CompactMem`, `PurgeMem` |
| Every string is length-prefixed (`UpdatePascalLength`, `CopyPascalString`, the `"\0keyrepeat(\""` templates) | Pascal strings, the Toolbox's native string type |
| Paths are colon-separated with the volume first: `OS Drive:Users:...:test3.ti` -- on Windows | HFS paths |
| Cursor resources named `CURS.ARROW`; containers with typed children found through a directory | `CURS` resource type; resource-fork thinking |
| The out-of-memory dialog offers "Compact" | The classic Mac low-memory recovery |
| The C runtime is used for almost nothing: `malloc` obtains the 12 MB pool and 2 KB descriptor pages, and every game allocation goes through the engine's own heap | An engine that already owned these services before it met Win32 |
| Status codes are 16-bit: hundreds of functions return `short`, and the script handlers' dispatchers read only `ax` (why Ghidra shows `CONCAT22` and `& 0xffff0000` returns) | `OSErr`, the Toolbox's 16-bit error type |
| On-disk and shared structs are 2-byte packed (`#pragma pack(push, 2)` in `df_types.h`, `df_keywords.h` and several units) | The 68k compilers' default alignment, kept so data files read the same on both platforms |
| The script keyword tables are C strings that `ConvertKeywordTablesToPascal` rewrites into Pascal strings in place, once, at startup | Mac compilers accept `"\pabs"` literals and VC++ does not: the tables were probably `\p` strings on the Mac, and the Windows port added a conversion pass rather than retype them. The function is a scar of the port |

### What the original source probably looked like

From the matching work. **Evidenced** means the binary or the 1996 compiler's behaviour
requires it; **speculation** is labelled.

- **Files:** 32 translation units, recovered from link order (Mac and March builds), data
  placement and cross-file call evidence; `src/` mirrors them. **Speculation:** they were
  engine modules (interpreter, renderer, sound, containers) shared across CyberFlix titles,
  with a thin Titanic layer -- most of the 32 are engine subsystems.
- **Globals, evidenced:** each file defined its own (an object's initialised data, then its
  literals, sit together in `.data`), not one `globals.c`.
  They had real names: VC++ 4.2 lays out uninitialised variables by a hash of the name, and
  the exe's `.bss` order is consistent only with names we do not have. Some were declared and
  never used (the 21 `.bss` pads), and so were some locals (`match/local_pads.json`) --
  probably debug-only code compiled out of the release build. Five "regions" were single
  structs (a `BITMAPINFOHEADER`, a `LOGPALETTE`, the CLUT, the text-entry table).
  **Speculation:** Apple-style names, `gSoundVolume` for globals, `sRingHead` for file
  statics, `kSomething` for constants -- the convention nearly every Mac programmer of the
  era used.
- **Headers, evidenced:** shared declarations came from headers, with named parameters
  (they count as symbols, which is part of why `symbol_phase.json` is needed). They were
  incomplete: some callers in other files called a `short` function with no prototype in
  scope and widened its result as a K&R implicit `int`.
- **Keyword tables, near certain:** a static `{name, id}` initialiser array per letter in
  the interpreter's source, ids from an `enum`. The March 1997 build reordered the keywords,
  which renumbered their ids.
- **Fatal-error codes:** the message reads "Fatal error at line %d", yet the codes cluster
  at 4000-4400 across unrelated files, and the March build shifts a run of them by the same
  46. Either an error-number list edited in the middle, or `__LINE__` of one large file;
  not settled.
- **Resources:** a hand-written `.rc` (STRINGTABLE, MENU, ACCELERATORS, two DIALOGs, cursor
  and icon files). The `RAW` custom types are probably the Mac resource-fork types the
  engine loaded there, carried over so the same loader code works.

## Subsystem Map

```
OS loader
  └─ CRT entry()
        └─ WinMain (0x004321d0)
              ├─ AppInit (0x0041bb40)          -- window reg, palette, graphics, CD check, font
              │     ├─ Memory manager          -- VxD or standard heap; pool 4–12 MB
              │     ├─ Graphics backend        -- WinG (WING32.DLL) or GDI CreateDIBSection
              │     ├─ Palette system          -- 256-entry CLUT; gamma tables; sys-palette steal
              │     ├─ Display mode            -- ChangeDisplaySettingsA; 640x480 8bpp required
              │     └─ Input                   -- joystick + keyboard event queue
              ├─ InitCLUTPalettes              -- load CLUT.SYSTEM / CLUT.BLACK resources
              ├─ RunGameLoop (0x004316e0)
              │     ├─ ConvertKeywordTablesToPascal  -- script keyword names -> Pascal strings
              │     ├─ ConvertMessageNamesToPascal
              │     ├─ [per-scene restart loop]
              │     │     ├─ BeginGameSession        -- channels A/B/C init; hotspot buffer; viewports
              │     │     ├─ [inner game loop]
              │     │     │     ├─ PumpMessages
              │     │     │     ├─ QdInitCursor
              │     │     │     ├─ DispatchChannelMessage  -- routes events to A/B/C handlers
              │     │     │     └─ PollEventQueue + dispatch by type
              │     │     └─ EndGameSession
              │     └─ [restart if DAT_0048a990 == 2]
              ├─ FreeCLUTPalettes
              └─ Shutdown (0x0041dd30)
```

---

## Key Subsystems

### Render Graph (`src/renderer.c`)

Node-based render graph. Each render node has a class, a child list (hash-bucketed MRU), and lifecycle hooks (init, tick, destroy). Key operations: `AttachRenderChild`, `DetachRenderChild`, `DestroyRenderNode`, `DeferRenderNodeClose`.

### Scene / Hotspot (`src/scene.c`, `src/hotspot_raster.c`)

`BeginGameSession` / `EndGameSession` bracket each scene. The hotspot system maintains `g_hotspotBuf` (8-bit per-pixel interaction buffer) updated each frame via `RasterStampChildIntoSceneHotspotBuf`. Trig (trigger) hotspots use a Bresenham-based raster pipeline (`hotspot_raster.c`) for scaled sprite overlap detection.

### Script Engine (`src/scriptcmd.c`, `src/scriptprop.c`, `src/multimedia.c`)

Scripts are binary (8-byte segments, see [script-format.md](script-format.md)). Dispatch:
- `DispatchActionCommand` — void action opcodes (12xxx, extended 16xxx)
- `DispatchScriptCommand` — query/compute opcodes (16xxx getter, 20xxx, 24xxx)
- `ProcessScriptBlock` — evaluates expression segments into the 40-entry work queue

Property bindings are managed by `scriptprop.c`: method table (16 entries), object table (16 entries), property table (32 entries). See [structs.md](structs.md) for entry layouts.

The interpreter chain, from a named script method to a dispatched opcode — all of it
walked live under gdb in the running port:

```
DispatchBootMessageString        e.g. boot(), idle()
  └─ EvalMethodCall
        └─ ExecuteMethodBody         bytecode base = hMethod (the handle, not pData)
              └─ BindMethodArguments
                    └─ InterpretStatements
                          └─ ProcessScriptBlock
                                └─ DispatchActionCommand / DispatchScriptCommand
```

`BOOTFILE` is the root container: `SceneChannelHandler`'s `msg=9` handler invokes
`boot()` once (guarded by `DAT_0048a130`) and `idle()` thereafter. Method paths are
tokenized via `methodpath.c`, whose `LookupScriptKeyword` (the keyword tables in
`keyword_tables.c`) turns
`(` / `)` into the `0xfb2`/`0xfb3` argument-block markers `EvalMethodCall` needs.

For the full opcode table see [opcodes.md](opcodes.md).

### Channel System (`src/anim.c`, `src/multimedia.c`, `src/scene.c`)

Three persistent channels (A=anim, B=scene-node, C=multimedia) receive and route events. See [channels.md](channels.md).

### Resource System (`src/resource.c`, `src/nodestore.c`, `src/container.c`)

Resources are reference-counted heap objects. `AllocResObject` / `ReleaseResource` / `FreeResObject` manage lifecycle. Container files (see [container-format.md](container-format.md)) are opened via `OpenContainerNode`; data is read into node-store blocks. The heap uses a secondary VxD allocator on Win9x.

The heap itself is the engine's own code, not a library: it sits at
`0x004331c0`-`0x00434c20` in the engine's address range (the linked CRT starts at
`0x0044E000`), reports through the game's STRINGTABLE ("Memory error at line %d",
"Bad pack use count at line %d", "Not enough memory for new handles") with the
engine's `FatalError(code, line)` convention, and calls `malloc` only to obtain
the pool (`heap.c`) and its descriptor pages. Pool size comes from STRINGTABLE
6/7/8: minimum 4000 KB, maximum 12000 KB, base 512 KB (+4 MB), chosen by a RAM
probe in `AllocMemoryPool` (`mempool.c`); the `memPool:` trace line prints the
result at startup. When a reservation fails, `ReserveHeapSpace` evicts
least-recently-used children across all open nodes (`EvictCachedChildrenForSpace`) and
compacts (`CompactHeap`); a failure after that surfaces as error 0x66 = 102,
which the string table labels "resource not found" but which means *no room*.
See "Macintosh Lineage" above for why it is shaped this way.

### Sound (`src/sound.c`)

WaveOut-based mixer. Primary device opened at startup. Key path: `LoadWaveDataOntoSoundChannel` → `ConsumePlayedFramesFromWaveHdrQueue` → `PumpSoundMixer`. ADPCM is CyberFlix-proprietary (incompatible with IMA/MS-ADPCM).

`CopyMixBufferSegment` selects its sample-copy routine through a table of raw x86
machine code baked into the 1996 binary, which is not decompilable; it is reimplemented
in C (8-bit sums with a 0x80 bias, 16-bit sums signed words). Run with the engine's own
`-16 -s` flags, or `InitSoundSystem` falls through its quality ladder to mono 8-bit and
the output is mostly quantisation hiss.

### Containers / Asset Files

| Extension | Contents |
|---|---|
| `BOOTFILE` | Scene root — startup scripts; game loads first |
| `SET` | Room/scene — pre-rendered frames, Z-depth maps, scripts |
| `MOV` | Movie/animation — frame sequences with click regions |
| `PUP` | Puppet (character) — dialogue, facial animation, audio |
| `CST` | Cast — puppet sprites and walking animations |
| `STG` | Stage — UI scripts and images |
| `SHP` | Shop/prop — foreground interactive items |
| `TRK` | Track — background music |
| `SFX` | Sound effects archive |
| `11K` | Low-quality music (< 6 MB RAM machines) |

---

## The render pipeline (pixel path)


The port draws. The pixel path, end to end:

```
page-node image -> DecodePageNodeImage (0x00441ca0) decodes into HotspotBuf @0x48cc10
                -> CopyHotspotBufState -> 0x48cc88
                -> BlitMovieRect -> QdCopyBits
                -> RenderSceneDisplayBuffer -> SetDIBColorTable + BitBlt
```

`QdCopyBits` (0x0041f9b0) is the **only** BitBlt in the engine —
everything visible goes through it. Before the port was fixed it ran zero times in a 30-second
run. Seven separate defects stood between `boot()` and a visible frame; all are
catalogued in [port-bug-classes.md](port-bug-classes.md) and commented at their sites.
The two worth knowing structurally:

- **`DecodePageNodeImage` is the image codec**, and its body was fabricated. The
  real one is 463 instructions of hand-written x86 (PUSHAD, `%ebp` reused as a scratch
  counter while also being the frame pointer, string ops, and an 18-entry computed-goto
  table at byte stride 8 — opcodes 4, 8 ... 72). It is ported as a **1:1 inline-asm
  transcription** in `renderer.c`, verified instruction-by-instruction against `TI.EXE`.
  A faithful C translation is still wanted; the asm is the reference oracle to diff
  against. A one-line C wrapper carries the address tag so the catalog still sees it.
- **Fades animate the hardware palette** (`SyncLogicalPalette` -> `AnimatePalette`),
  which recoloured 8-bit screens for free. On a true-colour desktop the pixels were
  colour-converted at BitBlt time, so a palette-only fade is invisible; the port
  re-blits per fade step (commented deviation, port-bug-classes §8).

### Verifying render work

Instrument, don't guess. The measurements that localised each defect, in order:
count blits (`QdCopyBits`), then the rects reaching it, then the
decoded pixel buffer (`nonzero`/total), then the DIB colour table (`nonblack`/256).
A working frame is: non-zero pixels, a mostly-non-black colour table, and a blit whose
source origin is near (0,0) and whose destination is the centred scene rect.

---

## Key Globals Quick-Reference

| Address | Name | Notes |
|---|---|---|
| `0x0048a990` | loop control | 0=run, 1=exit, 2=restart outer loop |
| `0x0048a350` | `g_sceneViewport` | Full-canvas viewport rect |
| `0x0048a248` | `g_hotspotBuf` | Scene hotspot pixel buffer |
| `0x0045db8c` | `g_renderNodeList` | Head of render node list |
| `0x00488264` | app HINSTANCE | Set in AppInit |
| `0x00486460` | cover HWND | Splash/loading cover window |

For the full globals table see [globals.md](globals.md).

---

## Memory Layout

| Region | Purpose |
|---|---|
| Game heap (4–12 MB) | Primary pool; base at `DAT_004862d0`; size at `DAT_00486e70` |
| Secondary heap | Animation compositing scratch (409,600 bytes) |
| VxD block (Win9x) | `FIXMEM.VXD` / `VFIXMEM` physical memory block |
| Resource objects | Allocated via `AllocResObject` from the game heap |
| Node-store blocks | Paged block storage for container data |

---

## Further Reading

- [differences.md](differences.md) — every way this source differs from the original exe
- [build.md](build.md) — build, run, and trace instructions
- [globals.md](globals.md) — all identified global addresses by subsystem
- [structs.md](structs.md) — data structure layouts
- [channels.md](channels.md) — channel/message architecture
- [opcodes.md](opcodes.md) — script opcode reference (opcode → handler, decode status)
- [script-format.md](script-format.md) — binary script wire format
- [container-format.md](container-format.md) — container file format spec
