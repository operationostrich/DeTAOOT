/*
 * df_types.h -- Ghidra / DreamFactory decompile type aliases.
 * Force-included by the build (-include / /FI) so every translation unit
 * sees the same typedefs without editing 43 source files.
 */
#ifndef DF_TYPES_H
#define DF_TYPES_H

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* MinGW defines 'pascal' as a calling-convention macro; decompile uses it as a var name. */
#ifdef pascal
#undef pascal
#endif

typedef unsigned char  byte;
typedef unsigned short ushort;
typedef unsigned int   uint;          /* Ghidra "uint" */

typedef uint8_t  undefined1;
typedef uint16_t undefined2;
typedef uint32_t undefined4;
typedef uint64_t undefined8;

#ifndef CONCAT22
#define CONCAT22(hi, lo) \
    ((uint32_t)(((uint32_t)(uint16_t)(hi) << 16) | (uint16_t)(lo)))
#endif

#ifndef CONCAT31
#define CONCAT31(hi, lo) \
    ((uint32_t)(((uint32_t)(uint8_t)(hi) << 24) | (uint32_t)(lo)))
#endif

/* Ghidra struct tag names used without 'struct' keyword in decompiled sources. */
#define tagMSG MSG
#define tagPALETTEENTRY PALETTEENTRY
#define tagPOINT POINT
#define tagRECT RECT

#define longdouble long double
#define bool int
#ifndef true
#define true 1
#define false 0
#endif

typedef unsigned char undefined3[3];

typedef struct SavedWndPos {
    HWND hWnd;
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} SavedWndPos;

/*
 * Packed: hResource must land at byte offset 0x12 (18) to match every
 * *(void**)(pChannel + 0x12) style access across scene.c/anim.c/multimedia.c,
 * and the struct must be exactly 146 (0x92) bytes so that gChannels[1]/[2]
 * land on the real channel-B/channel-C addresses (0x5f2, 0x684 -- verified via
 * address arithmetic against the three known channel base addresses). Default
 * struct alignment would pad hResource to offset 20 and the struct to 148
 * bytes, which is wrong -- confirmed via a standalone offsetof/sizeof test
 * compiled with this project's mingw toolchain.
 */
#pragma pack(push, 1)
typedef struct ChannelState {
    int16_t  type;
    int16_t  flags;
    byte     _unknown[14];
    void    *hResource;
    byte     _rest[124];
} ChannelState;
#pragma pack(pop)
/* The comment above is a claim; this is the proof. 146 = 0x92 is the gap
 * between the three channel bases (0x48a560/0x48a5f2/0x48a684), and the
 * save image copied from gChannels depends on it. */
typedef char ChannelState_size_check[(sizeof(ChannelState) == 146) ? 1 : -1];

#define tagMEMORYSTATUS MEMORYSTATUS

#define ASSIGN_HIWORD(Var, Val) \
    ((Var) = ((Var) & 0xFFFFu) | (((uint32_t)(uint16_t)(Val)) << 16))
#define ASSIGN_LOWORD(Var, Val) \
    ((Var) = ((Var) & 0xFFFF0000u) | ((uint32_t)(uint16_t)(Val)))

#include <math.h>

/* Ghidra 'code' type: base type for function-pointer casts */
typedef void code;

/* Ghidra sub-word field access helpers (read-only) */
#define LOWORD_OF(v)  ((uint16_t)(uint32_t)(v))
#define HIWORD_OF(v)  ((uint16_t)((uint32_t)(v) >> 16))
#define WORD2_OF(v)   ((uint32_t)((uint64_t)(v) >> 16))  /* ._2_4_ on 8-byte */

/* One script value as the interpreter passes it: {short type; 4-byte payload
 * at +2; short tag}. Copying it as a struct is what compiles to the exe's two
 * dword moves (ScriptCmd_PointX / PointY). */
#pragma pack(push, 2)
typedef struct {
    short type;         /* +0: 2 bool/short, 3 string slot, 4 int, 5 identifier */
    short lo;           /* +2: payload low word */
    short hi;           /* +4: payload high word */
    short tag;          /* +6 */
} ScriptTypedValue;     /* 8 bytes */
#pragma pack(pop)
/* A Macintosh-heritage Point, {v, h} = {y, x}, passed BY VALUE as one dword. The
 * script side packs it as (x << 16) | y; PointInSceneRect (0x00434f80) reads the
 * two words straight from its parameter slot, which only a struct parameter
 * compiles to (a packed uint reloads or shifts). */
#pragma pack(push, 2)
typedef struct ScenePointYX { short y; short x; } ScenePointYX;
#pragma pack(pop)

typedef struct HotspotBuf {
    int16_t  h;
    int16_t  w;
    void    *pPx;
    void    *pBufHandle;
#ifdef DF_VC42
    byte     _rest[108];  /* 120 bytes: InitHotspotBuffer writes 120, TI.EXE spaces each instance 0x78 */
#else
    byte     _rest[124];
#endif
} HotspotBuf;

/* -----------------------------------------------------------------------
 * ParsedMethodTarget -- the 42-byte playfield row copied out by
 * FindSceneRowByName (0x00409ee0: 10 dwords + 1 word,
 * source stride 0x2a).
 *
 * Ground truth frames (0x00408190 ScriptCmd_SendToSceneFx, 0x00409520
 * ScriptCmd_SendToPaintingFx) declare it as `local_2c[5]; uint local_16;
 * byte local_12[18]` -- i.e. byte offsets 0, 22, 26 from the buffer base,
 * NOT 0/20/24. `selector` is therefore an UNALIGNED dword and the struct
 * must stay packed; the old layout read 2 bytes short of the real field,
 * yielding values shaped {low half of the next field, high half of this
 * one}. port-bug-classes.md #3.
 * --------------------------------------------------------------------- */
#pragma pack(push, 1)
typedef struct ParsedMethodTargetTag {
    int          fields[5];   /* +0x00 */
    int16_t      _pad;        /* +0x14 */
    unsigned int selector;    /* +0x16 (22) */
    byte         name[16];    /* +0x1a (26) */
} ParsedMethodTarget;         /* 42 bytes */
#pragma pack(pop)
typedef char ParsedMethodTarget_size_check[(sizeof(ParsedMethodTarget) == 42) ? 1 : -1];

/* -----------------------------------------------------------------------
 * ResolvedSoundInfo -- the 46-byte page-entry row (stride 0x2e) copied out
 * by LookupViewRow (0x0040acc0: 11 dwords +
 * 1 word).
 *
 * Ground truth frames (0x004093c0 ActionCmd_PaintingScript, 0x00409520)
 * declare it as `local_130[6]; uint local_116` -- byte offset 26 for the
 * selector, not 24. 26 = the row's own 0x1a child-selector field, and 30 =
 * its 0x1e name field. Same unaligned-subfield idiom as ParsedMethodTarget
 * above and as the resolveField fix in 0x00409290.
 * --------------------------------------------------------------------- */
#pragma pack(push, 1)
typedef struct ResolvedSoundInfoTag {
    int          fields[6];   /* +0x00 */
    int16_t      _pad;        /* +0x18 */
    unsigned int value;       /* +0x1a (26) -- row field 0x1a */
    byte         rowTail[16]; /* +0x1e (30) -- row name field 0x1e */
} ResolvedSoundInfo;          /* 46 bytes */
#pragma pack(pop)
typedef char ResolvedSoundInfo_size_check[(sizeof(ResolvedSoundInfo) == 46) ? 1 : -1];

typedef struct ResolvedBufferInfoTag {
    int  fields[5];
    byte name[16];
} ResolvedBufferInfo;

typedef struct {
    WORD wRed;    /* r << 8 */
    WORD wGreen;  /* g << 8 */
    WORD wBlue;   /* b << 8 */
    WORD _pad;
} PAL16ENTRY;

#pragma pack(push, 2)
typedef struct {
    char  *pName;
    short  _pad;
} InputChannelEntry;             /* 6-byte stride: ConvertMessageNamesToPascal steps ADD EDI,6 over the 27 entries at 0x0045e120 */
#pragma pack(pop)

typedef struct {
    int16_t x;
    int16_t y;
    int16_t h;
    int16_t w;
} SceneViewport;

/* ABS for long double (Ghidra emits ABS() on FPU operands) */
#ifndef ABS
#define ABS(x) fabsl(x)
#endif

/* -----------------------------------------------------------------------
 * MethodCallFrame -- one script method-call scope entry (0x4e = 78 bytes).
 *
 * EvalMethodCall (scene.c, 0x004472a0) walks a CHAIN of these by raw memory
 * adjacency (`pCtxA += 0x27` shorts = 78 bytes) until it hits one whose
 * `isLast` is set, so the frames a caller builds MUST be contiguous. Declare
 * them as `MethodCallFrame frames[N]`, never as separate locals -- MSVC
 * happened to lay separate locals out adjacently and the original binary
 * relies on that; GCC does not, and the chain walk then runs off into
 * unrelated stack memory.
 *
 * Offsets are ground truth, read off Ghidra's stack layout for
 * ScriptCmd_SendToBootFx (0x0042a570), whose two frames sit at ebp-0x9c
 * and ebp-0x4e -- exactly 0x4e apart:
 *
 *   +0x00 isLast       last entry in the chain (EvalMethodCall's *pCtxA)
 *   +0x02 saved0x14c   caller's pOwner[0x14c] snapshot
 *   +0x06 activeCount  LatchFailedMethodFrame reads this as a short at +6
 *   +0x08 selector     method selector passed to AcquireMethodHandle
 *   +0x0c posIndex     ExecuteMethodBody writes the statement index here
 *   +0x10 pOwner       owning script node
 *   +0x14 pData        GetResDataPtr(hMethod) -- the real bytecode base
 *   +0x18 status       EvalMethodCall's error slot (pCtxA[0xc])
 *   +0x1a hMethod      opaque resource handle
 *   +0x1e buf0/buf1/buf2  three 16-byte Pascal name descriptors
 *
 * Packed: saved0x14c at +0x02 and hMethod at +0x1a are deliberately
 * unaligned, so any padding GCC would insert changes the layout.
 * --------------------------------------------------------------------- */
#pragma pack(push, 1)
typedef struct {
    int16_t  isLast;       /* +0x00 */
    uint32_t saved0x14c;   /* +0x02 */
    int16_t  activeCount;  /* +0x06 */
    uint32_t selector;     /* +0x08 */
    int32_t  posIndex;     /* +0x0c */
    void    *pOwner;       /* +0x10 */
    void    *pData;        /* +0x14 */
    int16_t  status;       /* +0x18 */
    void    *hMethod;      /* +0x1a */
    uint8_t  buf0[16];     /* +0x1e */
    uint8_t  buf1[16];     /* +0x2e */
    uint8_t  buf2[16];     /* +0x3e */
} MethodCallFrame;         /*  0x4e */
#pragma pack(pop)

/* Compile-time layout guard -- a mismatch here silently breaks the chain
 * walk at runtime instead of failing loudly, so check it at build time. */
typedef char MethodCallFrame_size_check[(sizeof(MethodCallFrame) == 0x4e) ? 1 : -1];

/* -----------------------------------------------------------------------
 * DfPluginMsg -- the 0x20-byte message block passed to a directory entry's
 * callback (InvokeEntryCallback, 0x004323f0).
 *
 * Layout read from the five entry points at 0x0040b8d0-0x0040bae0 and from
 * LoadPluginDirectory / UnloadPluginDirectory / ScriptCmd_PluginFx: a verb,
 * then five 6-byte {code, data} slots -- four arguments and one result the
 * callee writes back through the pointer.
 *
 * --------------------------------------------------------------------- */
#pragma pack(push, 2)
typedef struct {
    short  code;              /* DF_PLUGIN_NONE / _INT / _STR */
    void  *data;
} DfPluginArg;                /* 6 bytes */

typedef struct {
    short        verb;        /* +0x00  0 enumerate, 1 finalize, 2 invoke */
    DfPluginArg  arg[4];      /* +0x02 +0x08 +0x0e +0x14 */
    DfPluginArg  result;      /* +0x1a  written by the callee */
} DfPluginMsg;                /* 0x20 */
#pragma pack(pop)

typedef char DfPluginMsg_size_check[(sizeof(DfPluginMsg) == 0x20) ? 1 : -1];

#define DF_PLUGIN_NONE  0x3e8   /* slot unused */
#define DF_PLUGIN_INT   0x3e9   /* data is an integer value */
#define DF_PLUGIN_STR   0x3ea   /* data is a Pascal-string pointer */
/* The other kinds DispatchPluginEngineRequest (0x00436700) checks and returns; the names are
 * inferred from the engine functions each one is handed to. */
#define DF_PLUGIN_RESOURCE      0x3eb   /* a resource handle (AllocResObject) */
#define DF_PLUGIN_NODE          0x3ec   /* a container / render node (OpenContainerNode) */
#define DF_PLUGIN_HOTSPOT       0x3ed   /* a HotspotBuf */
#define DF_PLUGIN_HOTSPOT_STATE 0x3ee   /* a saved HotspotBuf state (CopyHotspotBufState) */
#define DF_PLUGIN_RECT          0x3ef   /* a short[] rect or point pair */
#define DF_PLUGIN_DATA          0x3f0   /* locked resource data (GetResDataPtr) */
#define DF_PLUGIN_CLUT          0x3f1   /* an installed CLUT */
#define DF_PLUGIN_POINT         0x3f2   /* a packed x,y point */

/* -----------------------------------------------------------------------
 * GameEvent -- the 18-byte EventQueueEntry image that DequeueGameEvent
 * (0x0041a630) copies verbatim out of g_eventQueue (0x00486970, stride 0x12).
 *
 * The copy is 4 dwords + 1 short = 18 bytes, and the queue's type filter is
 * read at +0x10 -- so `typeMask` is a real field, not padding. A struct
 * declared 16 bytes here lets every PollEventQueue call overrun its caller's
 * local by 2 bytes.
 *
 * Packed: unpacked, GCC aligns pNode to +12, so ev.pNode reads the payload's
 * HIGH word plus the modifier flags -- every click arrives as
 * (y = screen X, x = 0) and no script hotspot ever matches, while the enqueue
 * side (0x0041a450) is correct all along.
 * --------------------------------------------------------------------- */
#pragma pack(push, 1)
typedef struct
{
    short  eventType;   /* +0   event category -- drives the gameloop switch  */
    short  field_2;     /* +2   buttonId low half / sub-type                  */
    short  field_4;     /* +4   buttonId high half                            */
    short  field_6;     /* +6   timestamp low                                 */
    short  field_8;     /* +8   timestamp high                                */
    byte  *pNode;       /* +10  payload: packed {y,x} point or node handle    */
    short  modifiers;   /* +14  extraParam / modifier flags                   */
    short  typeMask;    /* +16  queue type filter -- read at +0x10 by dequeue */
} GameEvent;
#pragma pack(pop)
typedef char GameEvent_size_check[(sizeof(GameEvent) == 18) ? 1 : -1];

/* Compatible aliases used in scriptcmd.c partial views */
typedef HotspotBuf  HotspotBufTrigAlias;
typedef HotspotBuf  HotspotBufDims;
typedef SceneViewport SceneViewportSubA;

#endif /* DF_TYPES_H */
