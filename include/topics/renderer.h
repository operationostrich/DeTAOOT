/* Declarations for the "renderer" topic: the preamble and shared definitions of the functions grouped under that name. */
#ifndef DF_TOPIC_RENDERER_H
#define DF_TOPIC_RENDERER_H

#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>

/* MSVC CRT float/format helpers (statically linked in the original binary). */
typedef union {
    float  f;
    double d;
} _CrtFloatUnion;


typedef unsigned char byte;

/* The script keyword lists (src/keyword_tables.c). */
#include "df_keywords.h"

/*
 * RenderNode: large per-node control block in the active render list.
 * Only the "next" field offset is confirmed from the decompilation.
 * The struct is at least 0x544 bytes (next ptr at offset 0x540).
 */
typedef struct RenderNode {
    unsigned int   _body[0x150];    /* opaque fields (render state, buffers, ...) */
    struct RenderNode *pNext;       /* offset 0x540 - next node in list, NULL = end */
} RenderNode;

extern void *gRenderNodeList;  /* 0x0045db8c - head of active render-node list (RenderNode *) */

/* -- from between the functions -- */
extern int              gScanLockInited;  /* gScanLockInited - init flag */
extern CRITICAL_SECTION gScanLock;        /* gScanLock - CRITICAL_SECTION (24 bytes) */
extern int              gScanLockBusy;    /* gScanLockBusy - busy flag after critsec */
extern int    *gCurrentParentNode;       /* gCurrentParentNode */
extern int   **gCurrentParentData;       /* gCurrentParentData  - GetResDataPtr(parentNode) */
extern int    *gCurrentChildListData;    /* gCurrentChildListData  - GetResDataPtr(parentData[0]) */
extern int  gChildSerialCounter;               /* gChildSerialCounter - monotonic order stamp assigned to child slots */
extern short (*gMemoryPressureCallback)(void);            /* gMemoryPressureCallback - optional callback invoked after each compaction pass (GC / defrag hook); NULL = not installed */
#define DF_TRACE_PACKREF(fmt, p, c) \
    do { if ((unsigned)(c) < 8u) DF_TRACE(fmt, (p), (c)); } while (0)
#define DISPLAY_ATTACH_RING_END_SHORTS ((short *)((char *)&gAttachSlots + 4 * 0x3e))
typedef int (__cdecl *PageNodeThunkFn)(unsigned int kind, int w0, int w1);
#endif
