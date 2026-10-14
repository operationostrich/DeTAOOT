# Struct Registry

Central registry of all identified data structures in the DreamFactory engine binary.
Layouts are inferred from Ghidra decompilation. Sizes are confirmed unless marked **estimated**.

---

## Container / File Structures

### FileHeader (1024 bytes, confirmed)

Container file header; first 1024 bytes of every DreamFactory container file.

```c
struct FileHeader {
    int32_t  magic;             /* +0x00  always 0x10000 */
    int32_t  fileSize;          /* +0x04  total file size in bytes */
    int32_t  unknown[2];        /* +0x08  8 bytes unknown */
    int32_t  containerCount;    /* +0x10  number of containers */
    int32_t  type;              /* +0x14  0, 1, or 2 (controls dummy container handling) */
    int32_t  gapWhere;          /* +0x18  index of dummy/gap container */
    char     format[9];         /* +0x1C  "LPPALPPA\0" */
    char     unknown2[987];     /* +0x25  padding / unknown */
};
```

See [container-format.md](container-format.md) for `type` field semantics.

### ContainerHeader (8 bytes)

Header for each container within a file.

```c
struct ContainerHeader {
    int32_t  id;    /* +0  sequential container ID (0-based) */
    uint32_t size;  /* +4  container size in bytes, including this 8-byte header */
};
```

### ScriptSegment (8 bytes)

One segment in a binary DreamFactory script; scripts are null-terminated sequences.

```c
struct ScriptSegment {
    uint16_t cmd;      /* opcode (0 = end of script) */
    uint32_t info;     /* operand; meaning depends on cmd */
    uint16_t unknown;  /* always 0 in practice */
};
```

See [script-format.md](script-format.md) for opcode tables.

### ColorPalette Entry (8 bytes, from MOV/SET header)

```c
struct ColorPaletteEntry {
    int16_t index;  /* palette index (purpose of this field unclear -- possibly always 0..255) */
    int16_t r;
    int16_t g;
    int16_t b;
};
/* 256 entries = 2048 bytes total in MOV/SET container[0] headers */
```

### MovFrame Entry (variable, from MOV container[0])

```c
struct MovFrame {
    int32_t  frameCondition;       /* 1 = start frame */
    int16_t  unknownWord1;
    int16_t  unknownWord2;
    int16_t  height;
    int16_t  width;
    int32_t  locationFrame;        /* container index of frame image data */
    int32_t  locationClickRegion;  /* container index of click region definition */
    int16_t  unknownWord3;         /* 0 or 1; possibly end-of-sequence flag */
    int32_t  frameContainerSize;
    char     frameName[15];
};
```

---

## Win32 / GDI Structures (engine-defined layouts)

### LOGPALETTE (engine instance, at 0x00487d48)

```c
/* Engine LOGPALETTE at 0x00487d48 */
WORD           palVersion;     /* 0x00487d48 = 0x0300 */
WORD           palNumEntries;  /* 0x00487d4a = 0x0100 (256) */
PALETTEENTRY   palPalEntry[256]; /* 0x00487d4c, 4 bytes each */
```

### PAL16ENTRY (2 bytes per channel, 8 bytes per entry, at 0x0048754a)

```c
/* 16-bit colour table used by SetPaletteEntry; stride 8 */
struct PAL16ENTRY {
    WORD r;  /* 8-bit value left-shifted by 8 */
    WORD g;
    WORD b;
    WORD _pad;
};
/* gPal16[256] at 0x0048754a */
```

### SavedWndPos (20 bytes, used in SaveWindowPositions)

```c
struct SavedWndPos {
    HWND hwnd;
    int  left;
    int  top;
    int  right;
    int  bottom;
};
/* array at gSavedWindowPositions; count in gSavedWindowCount */
```

---

## Engine Runtime Structures

### ChannelState (146 bytes, estimated layout)

Three channel state buffers exist: channel A (anim, type 2) at `0x0048a560`, channel B (scene-node, type 1) at `0x0048a5f2`, channel C (multimedia, type 3) at `0x0048a684`. Each is 146 (0x92) bytes.

```c
struct ChannelState {           /* 146 bytes total */
    /* +0x00 */  int16_t  type;         /* channel type: 1=scene-node, 2=anim, 3=multimedia */
    /* +0x02 */  int16_t  flags;        /* state flags */
    /* ...   */  byte     _unknown[14]; /* fields TBD */
    /* +0x12 */  void    *hResource;    /* resource handle; non-zero = already initialised (fatal guard) */
    /* +0x14 */  byte     _rest[128];   /* remaining fields TBD */
};
```

Known offsets: `+0x12` = resource handle (`gChannels[0].hResource` for channel A, `gChannels[2].hResource` for channel C).

### SceneViewport (8 bytes)

```c
struct SceneViewport {  /* gSceneViewport at 0x0048a350 */
    int16_t x;
    int16_t y;
    int16_t h;
    int16_t w;
};
```

### HotspotBuf (partial, estimated)

```c
struct HotspotBuf {     /* gHotspotBuf at 0x0048a248 */
    int16_t  h;         /* +0  height */
    int16_t  w;         /* +2  width */
    void    *pPx;       /* +4  pixel buffer (gSceneW x gSceneH bytes) */
    void    *pBufHandle; /* +8  resource handle */
    /* ... additional fields TBD */
};
```

### RenderNode (partial, estimated)

```c
struct RenderNode {     /* gRenderNodeList head at 0x0045db8c */
    /* ... */
    RenderNode *pNext;  /* +0x540  next in singly-linked list; walked by FlushAllRenderNodes */
    /* ... */
};
```

### EventQueueEntry (18 bytes)

```c
struct EventQueueEntry {    /* base at gEventQueue, stride 0x12 */
    byte     data[16];      /* +0x00  event payload (type-dependent) */
    uint16_t type;          /* +0x10  event type */
};
/* count in gEventQueueCount */
```

### ScanNode (40 bytes, 10 ints)

Used by the hotspot raster system in `hotspot_raster.c`. Up to `gScanNodeCount` active nodes; base at `gScanNodeBase`.

```c
struct ScanNode {        /* stride 0x28 = 40 bytes = 10 ints */
    int  pos;            /* [0]  current position */
    int  target;         /* [1]  target position */
    int  delta;          /* [2]  signed step delta (target - pos) */
    int  absDelta;       /* [3]  abs(delta) */
    int  accumulator;    /* [4]  Bresenham error accumulator */
    int  step;           /* [5]  step size */
    int  doubleAbsDelta; /* [6]  absDelta * 2 */
    int  doubleStep;     /* [7]  step * 2 */
    int  errorTerm;      /* [8]  doubleAbsDelta - step */
    int  countdown;      /* [9]  decremented by DecrementAndTrimScanNodes; node trimmed when 0 */
};
```

---

## Script / Property Binding Tables

### Slot 0 Entry (0xA0 = 160 bytes)

Named-entry table (`gActorTable`), stride 0xA0, count in `gActorCount`.

```c
struct Slot0Entry {            /* 160 bytes */
    /* +0x00 */  byte   _header[0x2e];  /* fields TBD */
    /* +0x2e */  int    bindingKey;     /* matched by FindPaintingBindingKeyAtPoint */
    /* +0x50 */  byte   name[0x10];    /* Pascal name (15 chars + length byte) */
    /* +0x60 */  byte   auxName[0x10]; /* Pascal auxiliary name */
    /* +0x74 */  int16_t renderChild0; /* render child selector */
    /* +0x76 */  int16_t aux76;        /* patched by ActionCmd_ActorDist */
    /* +0x78 */  int16_t aux78;        /* binding ID; -1 = unbound */
    /* +0x7a */  int16_t aux7a;        /* min 1 or -1 */
    /* +0x7c */  int16_t aux7c;
    /* +0x7e */  int16_t aux7e;
    /* +0x80 */  int16_t aux80;        /* min 1 */
    /* +0x82 */  byte    _gap82[2];
    /* +0x84 */  int    coordX;        /* packed X; set by UpdateNamedCoordsFromSecondDescriptor */
    /* +0x88 */  int    coordY;        /* packed Y / aux byte */
    /* +0x8C */  int16_t aux8c;
    /* +0x8E */  byte   commitFlag;    /* 1 = committed/dirty */
    /* +0x8F */  byte   _tail[17];     /* pad to 0xA0 */
};
```

### Slot 1 Entry (0x1C = 28 bytes, 7 ints)

Multimedia descriptor table (`gCastTable`), stride 0x1C, count in `gCastCount`.

```c
struct Slot1Entry {        /* 28 bytes = 7 ints */
    int  fields[3];        /* [0..2] TBD */
    int  namePtr;          /* [3]  Pascal name pointer (returned by ScriptCmd_IndexToCast) */
    int  fields2[3];       /* [4..6] TBD */
};
```

### Slot 2 Entry (0x28 = 40 bytes, 10 ints)

State entry table (`gTrackTable`), stride 0x28, count in `gTrackCount`.

```c
struct Slot2Entry {        /* 40 bytes = 10 ints */
    int16_t fields[0x0e];  /* [0..0xd] TBD */
    byte    name[0x10];    /* [+0x1e] Pascal name (15 chars) */
};
```

### Slot 2 State Row (0x9E = 158 bytes)

State table row (`gPropTable`), stride 0x9E, count in `gPropCount`.

```c
struct StateRow {          /* 158 bytes */
    /* +0x00 */  int16_t active;      /* [0] active flag */
    /* +0x02 */  int16_t bindingId;   /* [1] binding ID */
    /* +0x04 */  int     key;         /* [2] match key (used by ApplyStateSlot2BindingBySlot3Key) */
    /* +0x06 */  byte    _pad[0x46];
    /* +0x4c */  byte    commitMarker; /* set by WarmPropRenderChildren */
    /* +0x4e */  byte    stateName[0x10]; /* Pascal state name */
    /* +0x7a */  int     aux7a;       /* binding ID or -1 */
    /* +0x7c */  int     aux7c;
    /* +0x7e */  int     aux7e;
    /* +0x80 */  int     aux80;
    /* +0x84 */  int     coordX;
    /* +0x88 */  int     coordY;
    /* +0x8E */  byte    commitFlag;
};
```

### Method Binding Table Entry (0x37 shorts = 110 bytes)

`gWalkTable`, 16 entries, stride 0x37 shorts (110 bytes).

```c
struct MethodBindingEntry {  /* 110 bytes */
    int16_t active;          /* [0] */
    int16_t bindingId;       /* [1] */
    /* [2..8] TBD */
    int   **hResourcePtr;    /* [9] pointer to resource handle pointer */
    /* [0x10..0x16] TBD */
    byte    name[0x20];      /* [0x17] Pascal name (31 chars) */
};
```

### Object Binding Table Entry (0x25 shorts = 74 bytes)

`gCricketTable`, 16 entries, stride 0x25 shorts (74 bytes).

```c
struct ObjectBindingEntry {  /* 74 bytes */
    int16_t active;          /* [0] */
    int16_t bindingId;       /* [1] */
    /* [2..0x1c] TBD */
    byte    name[0x20];      /* [0x1d] Pascal name (31 chars) */
};
```

### Property Binding Table Entry (0x15 shorts = 42 bytes)

`gLoopTable`, 32 entries, stride 0x15 shorts (42 bytes).

```c
struct PropertyBindingEntry {  /* 42 bytes */
    int16_t active;            /* [0] */
    int16_t bindingId;         /* [1] */
    int16_t typeHash;          /* [2] */
    /* [3..4] TBD */
    byte    name[0x10];        /* [5..12] Pascal name (15 chars) */
    byte    altName[0x10];     /* [13..20] secondary Pascal name */
};
```

---

## Sound Structures

### SoundChannelSlot (104 bytes, 26 ints)

Three slots: slot 0 (`gSoundSlots`, primary), slot 1 (`DAT_00489f60`), slot 2 (`DAT_00489fc8`).

```c
struct SoundChannelSlot {  /* 104 bytes = 26 ints */
    int  priorityKey;      /* [0]  first int is priority key */
    int  fields[25];       /* [1..25] TBD */
};
```

Correction: the 15 bytes at slot `+0x59..+0x67` are exactly one
`GainTriple` (include/df_port_types.h, 15 bytes). The five alias fields there
(`int` +0x59, `int` +0x5d, `int` +0x61, `short` +0x65, `byte` +0x67; `DAT_00489f51` ..
`DAT_00489f5f` in slot 0) are its members, not five independent fields.

### Scroll rect and clip rect

- `gScrollRect` (0x0048a2c0, `short[4]`) is a QuickDraw rect `{y1, x1, y2, x2}`, the existing
  `SceneRect` typedef (include/topics/gameloop.h). It is not `{scrollY, scrollX, viewH, viewW}`:
  the msg-0x10 centring code (debugger.c) adds left and right. `DAT_0048a2c4`
  is its `{y2, x2}` half read as one `int`, spelled `(*(int *)&gScrollRect[2])`.
- `gClipRect` (0x0048a880, `short[4]`) has the other order, `{x1, y1, x2, y2}` (the hot-spot
  clip test in sprite_hotspot_raster.c). Its members `[1..3]` are spelled `gClipRect[1]` ..
  `gClipRect[3]`.

---

## Notes

- **Pascal string** format throughout: `byte length` at offset 0, followed by `length` bytes of ASCII text (no null terminator in the length-counted region, though a null often follows). `UpdatePascalLength` sets the length byte from the C-string tail.
- Sizes marked **estimated** are inferred from stride calculations and field offsets; confirm against Ghidra when decoding each struct's functions.
- For globals holding instances of these structs, see [globals.md](globals.md).

### B1 retypes -- existing types only

| Global | Address | Was | Now | Notes |
|---|---|---|---|---|
| `gEventQueue` | 0x00486970 | `byte[576]` | `GameEvent[32]` | 18-byte packed records (df_types.h); `puFlags` walks `gEventQueue[0].typeMask` |
| `gFindData` | 0x00489da0 | `byte[320]` | `WIN32_FIND_DATAA` | 320 bytes; `DAT_00489dcc` is now `gFindData.cFileName[0]` |
| `gBitmapHeader` (was `DAT_00486e80`) | 0x00486e80 | `int` (VC region `int[10]`) | `BITMAPINFOHEADER` (VC region `BITMAPINFOHEADER[1]`) | the ten members at +4..+0x24 are spelled `gBitmapHeader.biWidth` .. `.biClrImportant` in `AppInit` and `InitHotspotBuffer`; their `var` rows keep the storage names |
| `gLogPal.palNumEntries` | 0x00487d4a | `*(WORD *)&DAT_00487d4a` | `gLogPal.palNumEntries` | word store |
| `gDibColorTable[0].rgbGreen/rgbRed` | 0x00486ea9/a | byte-offset alias | member alias | no uses |

`gScrollRect` -> `SceneRect` was done in B2 (below), by a guarded declaration instead of a hoist.
