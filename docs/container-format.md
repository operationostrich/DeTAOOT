# DreamFactory Container File Format

Reverse-engineering spec for the DreamFactory container format used by CyberFlix games.  
Applies to: **Titanic: Adventure Out of Time** (confirmed), Timelapse (confirmed), Dust (partial, older engine).  
Source: binary analysis + the source code of DFET (DreamFactory Extraction Tool, a third-party extractor).

**All confirmed DreamFactory container/data formats:**

| Extension | Contents |
|---|---|
| `BOOTFILE` | Scene root container — startup scripts; game loads this first via `InitSceneResource` to get scene dimensions |
| `SET` | Room/scene packages (Titanic only) — pre-rendered background frames, Z-depth maps, scripts, transitions |
| `MOV` | Movie/animation — single inspectable object, clickable frame sequence, or full cutscene with audio |
| `PUP` | Puppet (character) files — dialogue text, conversation scripts + logic, facial animation tables, voice audio |
| `CST` | Cast files — puppet exterior sprites and animations (walking, idle, scaling by Z distance) |
| `STG` | Stage files — scripts and images for UI elements, mini-games |
| `SHP` | Shop (prop) files — foreground prop images drawn over SET backgrounds; interactive items, doors, buttons |
| `TRK` | Track files — background music / long audio |
| `SFX` | Sound effects archive — short clips |
| `SND` | Legacy sound format (older DF versions) |
| `11K` | Low-quality music archive — used on machines with < 6 MB RAM instead of TRK files |

**Key design notes (from DFET documentation):**
- `.SET` files contain everything for a room: every camera view is a separate pre-rendered frame; scene positions have per-frame position/rotation data (not grid-based in TAOOT). Each frame also has a **Z-frame** (depth map, 1 byte per pixel) used for puppet occlusion.
- `.PUP` files exist in pairs (`*1.PUP` = before sinking, `*2.PUP` = during sinking) to split across the two game CDs.
- Audio is split across multiple containers each ≤ 64 KB. All audio is 8-bit mono ADPCM (22050 Hz standard, 11025 Hz in `.11K`). The ADPCM tables are CyberFlix-proprietary and not compatible with standard IMA or MS-ADPCM.
- Scripts are binary (see below) — already parsed/interpreted when stored; the binary form is actually *larger* than the text form.

All multi-byte integers are **little-endian**.

---

## File Structure Overview

```
[0x000 – 0x3FF]   Header              (1024 bytes, exact)
[0x400 – ...]     Container Offset Table  (containerCount × int32)
[...]             Container data (variable)
```

The DFET source calls each data unit a **container** (not "block").

---

## File Header (1024 bytes, confirmed from engine binary + DFET `FileHeader` struct)

```c
struct FileHeader {              /* total = 1024 bytes exactly */
    int32_t  magic;              /* +0x00  always 0x10000 (verified by engine; DFET reports 0x1) */
    int32_t  fileSize;           /* +0x04  total file size in bytes (verified against actual) */
    int32_t  unknown[2];         /* +0x08  8 bytes unknown */
    int32_t  containerCount;     /* +0x10  number of containers (engine reads from +0x10) */
    int32_t  type;               /* +0x14  file variant: 0, 1, or 2 */
    int32_t  gapWhere;           /* +0x18  index of dummy/gap container(s) */
    char     format[9];          /* +0x1C  always "LPPALPPA\0" (may be one field off) */
    char     unknown2[987];      /* +0x25  padding / unknown data */
};
```

**`type` field semantics** (from DFET `readFileIntoMemory`):
- `type == 1`: one dummy container at index `gapWhere`
- `type == 2`: two dummy containers at indices `gapWhere-1` and `gapWhere`
- `type == 0` (else): containers whose offset ≤ 1024 are treated as empty 8-byte stubs

**`format` field:** confirmed as the literal string `"LPPALPPA"` (9 bytes, null-terminated at +0x28).  
Earlier notes suggesting `"PLAP"×2` were incorrect byte-order readings.

---

## Container Offset Table (starts at byte 0x400)

An array of `int32_t` values, one per container.  Count = `fileHeader.containerCount`.  
Each value is the **absolute byte offset** of that container within the file.  
Containers with offset ≤ 1024 are empty stubs (see `type == 0` above).

---

## Container Format

| Offset | Size | Type | Description |
|---|---|---|---|
| 0 | 4 | int32 | Container ID (sequential, 0-based) |
| 4 | 4 | uint32 | Container size in bytes (includes this 8-byte header) |
| 8 | varies | — | Container payload |

---

## MOV File — Container 0 (Header) Layout

From DFET `DFmov::initContainerData`:

```c
/* container[0].data offsets */
+0x02  int32_t  version          /* must be 4 for DFv4 */
+0x18  int32_t  unknown[18]      /* 72 bytes */
+0x60  int32_t  audioLocation    /* container index of audio info block */
+0x64  int32_t  memoryAllocInfo  /* container index */
+0x68  int32_t  unknownRef2
+0x6C  ColorPalette[256]         /* 256 × {int16_t index, int16_t R, int16_t G, int16_t B} = 2048 bytes */
+0x86C int16_t  globalHeight     /* frame height for all frames */
+0x86E int16_t  globalWidth      /* frame width for all frames */
+0x870 int32_t  unknownInt
+0x874 int32_t  frameCount
       Frame[]  frames           /* frame table entries follow */
```

Each `Frame` entry (from `MovHeader::Frame`):
- `int32_t frameCondition` — 1 = start frame
- `int16_t unknownWord1, unknownWord2`
- `int16_t height, width`
- `int32_t locationFrame` — container index of frame image data
- `int32_t locationClickRegion` — container index of click region definition
- `int16_t unknownWord3` — 0 or 1 (possibly end-of-sequence flag)
- `int32_t frameContainerSize`
- `char frameName[15]`

---

## SET File — Container 0 (Header) Layout

From DFET `DFset::initContainerData`:

```c
/* container[0].data offsets */
+0x02  int32_t  version          /* must be 4 */
+0x18  int32_t  mapLight         /* container index of light Z-depth map */
+0x1C  int32_t  mapDark          /* container index of dark Z-depth map */
+0x20  int32_t  (unknown)        /* 4 bytes */
+0x24  int16_t  mapHeight
+0x26  int16_t  mapWidth
       (6 bytes unknown)
+0x30  int16_t  setDimensionsY
+0x32  int16_t  setDimensionsX
       (18 bytes unknown)
       double   setDimensionsYf  /* float version */
       double   setDimensionsXf
       int32_t  sceneRegister    /* container index of scene list */
       int32_t  transitionRegister
       int32_t  actorRegister
       int32_t  mainScript       /* container index of root script */
       int32_t  mainSceneRegister
       int32_t  sceneCount
       int32_t  unknown0x68
       int32_t  unknown0x6C
       byte     setNameLen
       char     setName[setNameLen]
+0x84  int16_t  viewPortWidth
+0x86  int16_t  viewPortHeight
       double   coords[3]        /* camera XYZ */
       double   rotations[2]     /* camera rotation */
       ColorPalette[256]         /* palette — only first 128 entries used */
```

---

## Script Binary Format

> **Full reference:** See [script-format.md](script-format.md) for the complete script wire format, all opcode tables, and keyword list. The section below is a summary.

From DFET `DFscript.h/cpp`. Binary scripts are stored as a sequence of **8-byte segments**, terminated by a segment with `cmd == 0`.

```c
struct ScriptSegment {      /* 8 bytes */
    uint16_t cmd;           /* opcode — 0 = end of script */
    uint32_t info;          /* operand (meaning depends on cmd) */
    uint16_t unknown;       /* always 0 in practice */
};
```

**Literal types** (`cmd` values 3–6):

| cmd | Name | `info` meaning |
|---|---|---|
| 3 | STRING | byte offset from current segment to Pascal string |
| 4 | INTEGER | literal integer value |
| 5 | VARIABLE | byte offset from current segment to Pascal variable name |
| 6 | BREAK | tab depth of the following line (for indentation) |

**Operator opcodes** (8001–8013 — these are the opcodes in `ProcessScriptBlock`'s work queue):

| cmd | Symbol | cmd | Symbol |
|---|---|---|---|
| 8001 | `+` | 8002 | `-` |
| 8003 | `*` | 8004 | `/` |
| 8005 | `&` | 8006 | `\|` |
| 8007 | `@` | 8008 | `=` |
| 8009 | `!=` | 8010 | `>` |
| 8011 | `<` | 8012 | `>=` |
| 8013 | `<=` | | |

**Punctuation** (4018–4020): `(` `)`  `,`

**Control flow** (4xxx range):

| cmd | Keyword | cmd | Keyword |
|---|---|---|---|
| 4001 | `code` | 4004 | `endcode` |
| 4002 | `global` | 4003 | `local` |
| 4005 | `exitcode` | 4006 | `if` |
| 4007 | `endif` | 4008 | `else` |
| 4010 | `endswitch` | 4011 | `case` |
| 4012 | `for` | 4015 | `endfor` |
| 4017 | `endwhile` | 4018 | `(` |
| 4019 | `)` | 4020 | `,` |
| 4022 | `false` | 4023 | `not` |
| 4024 | `return` | 4025 | `passcode` |
| 4026 | `me` | 4028 | `dumplocal` |
| 4029 | `dumpglobal` | | |

**Engine commands** (12xxx = void actions, 16xxx = single-value getters, 20xxx = computed values, 24xxx = transitions).  
Selected important ones:

| cmd | Name | cmd | Name |
|---|---|---|---|
| 12031 | `bootscript` | 12032 | `opensetfile` |
| 12033 | `closesetfile` | 12013 | `opencastfile` |
| 12014 | `closecastfile` | 12018 | `openpuppetfile` |
| 12042 | `closepuppetfile` | 12019 | `opentrackfile` |
| 12020 | `closetrackfile` | 12056 | `openshopfile` |
| 12057 | `closeshopfile` | 12060 | `openstagefile` |
| 12061 | `closestagefile` | 12034 | `sendtoscene` |
| 12016 | `sendtoactor` | 12045 | `sendtopuppet` |
| 12055 | `sendtoprop` | 12017 | `playmovie` |
| 12021 | `playtheme` | 12025 | `bothsound` |
| 12038 | `clut` | 12039 | `cursor` |
| 12051 | `blackscreen` | 12049 | `blacktoscreen` |
| 12050 | `screentoblack` | 12047 | `castscript` |
| 12071 | `quit` | | |
| 16029 | `currentscene` | 16011 | `currentview` |
| 16034 | `currentcd` | 20053 | `currentset` |
| 20019 | `countscenes` | 20118 | `countviews` |
| 20001 | `random` | 20027 | `freemem` |
| 20121 | `heapsize` | 20067 | `fileexists` |
| 24001 | `barndoorclose` | 24002 | `barndooropen` |
| 24003 | `irisclose` | 24004 | `irisopen` |
| 24005–8 | `scroll*` | 24019 | `nodraw` |

The full command map is in DFET's `DFscript.h`.

**Connection to `ProcessScriptBlock` in multimedia.c:**  
The opcodes 8001–8013 seen in the script format **are the same opcodes** processed by `ProcessScriptBlock`'s 40-entry work queue (documented in multimedia.c). The script interpreter evaluates segments in sequence, pushing operands and dispatching operators.

---


