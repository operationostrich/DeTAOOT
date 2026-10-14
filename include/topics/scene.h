#ifndef DF_TOPIC_SCENE_H
#define DF_TOPIC_SCENE_H

#include <windows.h>
#include <stdint.h>
#include <string.h>
#ifndef CONCAT22
#define CONCAT22(hi, lo) ((uint32_t)(((uint32_t)(uint16_t)(hi) << 16) | (uint16_t)(lo)))
#endif

#include "df_port_types.h"   /* NodeDesc, defined once there */

extern HCURSOR       gCursor;       /* 0x0048a878 -- main game cursor (CURS id 2002) */
extern void         *gCursorAlt;    /* 0x0048a2c8 -- secondary cursor (CURS id 131)  */
        /* 0x0048a862 -- scene canvas width  */
        /* 0x0048a864 -- scene canvas height */
extern SceneViewport gSceneViewport; /* 0x0048a350 -- full-canvas viewport rect       */
extern HotspotBuf    gHotspotBuf;    /* 0x0048a248 -- scene hotspot/interaction buf   */

/* Scene state snapshot and retained reference */
extern int           gThePortDesc[32]; /* 0x004872a8 -- 32-int mirror of last pSceneState passed to QdSetPort */
extern int          *gThePort;               /* 0x004883c8 -- retained ref to current scene node; NULL until first QdSetPort */


extern int           gCursorShowDepth;           /* 0x0045b728 -- present/flip state; loop while < 0 */

extern HPALETTE      gPalette;   /* 0x00485f4c -- logical palette; palette.c / SyncLogicalPalette */


typedef struct {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[256];
} BMI256;
typedef HBITMAP(WINAPI *PFN_CreateDIBSection)(
    HDC hdc, const BITMAPINFO *pbmi, UINT usage, VOID **ppvBits, HANDLE hSection, DWORD offset);
typedef HBITMAP(WINAPI *PFN_WinGCreateBitmap)(
    HDC hdc, const BITMAPINFO *pbmi, VOID **ppvBits);
extern void *gViewportTable[];  /* 0x00486bb0 */
extern int   gViewportCount;    /* 0x0048836c */
extern InputChannelEntry g_inputChannelTable[27]; /* 0x0045e120 -- input.c */
/* pack(2): 14 bytes, as InterpretStatements' own struct ForLoopEntry (interpreter.c) */
#pragma pack(push, 2)
typedef struct {
    short tag;        /* property tag index */
    int   limit;      /* loop bound value */
    int   step;       /* step increment */
    short *pLoopback; /* instruction pointer to loop back to */
} ForLoopEntry;
#pragma pack(pop)
extern HotspotBuf     gTrigHotspotCopy;    /* 0x0048ad48 -- copy of gHotspotBuf (30 DWORDs) */
extern HotspotBuf     gTrigBufScene;       /* 0x0048add0 -- trigger HotspotBuf (full scene dims) */
extern SceneViewport  gTrigViewport;       /* 0x0048ae68 -- trigger viewport (copy of scene viewport) */
extern HotspotBuf     gTrigBufOverlay;     /* 0x0048ae78 -- trigger HotspotBuf (overlay dims) */
typedef struct {
    byte head[0x200];
    byte names[9][0x100];
} PropDescriptorTable;
#endif
