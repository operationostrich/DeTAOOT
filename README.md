# DeTAOOT - Titanic: Adventure Out of Time

The reversed engineered C source code for the Windows version of *Titanic: Adventure Out of Time* (CyberFlix, 1996,
DreamFactory engine 4.0), recovered from `TI.EXE`. 

This project produces:

* **A playable port.** Builds with MinGW GCC and runs the game on a modern Windows desktop,
  against your own copy of the game's data. Where the original misbehaves on a modern system
  the port departs from it on purpose, each departure behind a `DF_FIX_*` switch
  ([docs/differences.md](docs/differences.md)).
*  `sha256 82bfc60adc07ed708e1c0bbffb00572c2c372c1a57b434a5e0df0e899eb14619` - A byte-identical rebuild of the TI.exe built on 31st December 1996 with Visual C++ 4.2 and its 1996 linker.
* `sha256 fba675d696c356eeef8184015751719fabdc7d95ee13079f9c28b1af08d08bfb1` - A byte-identical rebuild of GOG's patched version of the 31 December 1996 build.
  
This tree contains no part of the game: no `TI.EXE`, no game data, no compiler, no Microsoft
library. You need your own copy of the game, and for the exact rebuild your own copy of
Visual C++ 4.2 (the MSDN disc 1 image).

## Layout

| Path | What is in it |
|---|---|
| `src/<subsystem>/*.c` | The 32 translation units of the game, grouped by subsystem (below). Every function carries its address in `TI.EXE`: `/* 0x00412345 */` on the definition line |
| `src/units.txt` | **The link order of the original exe**, the one place it is written down. Only the exact rebuild reads it; do not reorder it |
| `src/data/` | `globals.c` (every global, its address beside it) and `keyword_tables.c` (the script language's reserved words) |
| `src/port/` | `trace.c` and `crt_shims.c`: the playable port only, not part of the exe |
| `include/` | `df_*.h`, and `topics/` -- the per-topic headers the units share |
| `res/` | The string table and the resources, taken from the original exe |
| `Makefile` | The MinGW build of the port. Needs none of `match/` |
| `match/` | Optional: the Visual C++ 4.2 build that reproduces the exes, its tables, and the checks |
| `docs/` | [build.md](docs/build.md), [engine.md](docs/engine.md), [differences.md](docs/differences.md) and the engine reference |

The file names and folders were chosen to describe the contents. The original's own file names
are unknown (no build keeps them), and its link order interleaves subsystems, so it was one
flat project. Folders:

| Folder | Files |
|---|---|
| `platform/` | `display_ui` (palette, display mode, window, modal UI, `AppInit`), `winmain_nav` (`WinMain`, navigation, the edit-variable dialog), `network` |
| `memory/` | `heap_resources` (the Mac-style handle heap, resource objects), `container_store` (container files, the node block store), `property_heap` |
| `script/` | `script_block_parser`, `keywords_strings`, `script_dispatch`, `page_node_codec`, `script_bindings` (loops, walks, crickets), `interpreter`, `debugger` (the script debugger window; inert in this build) |
| `sound/` | `wave_mixer`, `wave_device_scene_cmds`, `sound_channels`, `track_sound_cmds` |
| `graphics/` | `sprite_hotspot_raster`, `hotspot_blit`, `clut_rects_resources`, `render_nodes`, `movie_player` |
| `anim/` | `cast_actors`, `channel_messages`, `messages_channel`, `puppet_transitions` |
| `scene/` | `waypoint_paths`, `attach_slots`, `scene_channel_scripts`, `stage` |
| `game/` | `save_props_shops`, `trigger_overlay_actions` (the Titanic-specific files) |

## Building the game

Details on how to run it against your game data, and the trace build: [docs/build.md](docs/build.md).

## Reading the source

Start with [docs/engine.md](docs/engine.md): the engine's architecture and its *Macintosh
Lineage* -- DreamFactory began on the Macintosh, and much that looks odd in the code (Pascal
strings, `short` status codes, 2-byte packing, the keyword tables converted in place at startup,
the handle-based heap) is the Mac engine carried to Win32. The comments say *why* a function is
shaped as it is: what the original does differently, the bugs the port hit and fixed, and the
spellings that exist only because the 1996 compiler produces the original's bytes from them
(change one of those and the exact rebuild stops matching -- the comment says so).

[docs/differences.md](docs/differences.md) is the one document of every way this source differs
from the original exe.
