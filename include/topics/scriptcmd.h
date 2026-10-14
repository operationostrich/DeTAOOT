#ifndef DF_TOPIC_SCRIPTCMD_H
#define DF_TOPIC_SCRIPTCMD_H


#include <windows.h>
#include <stdint.h>
#include <string.h>

#ifndef CONCAT22
#define CONCAT22(h, l) \
    ((unsigned)(((unsigned)(h) & 0xFFFFu) << 16) | (unsigned)((unsigned short)(l)))
#endif

/* HotspotBufTrigAlias / SceneViewportSubA / HotspotBufDims defined as aliases in df_types.h */
extern HotspotBufTrigAlias gTrigBufOverlay;                     /* 0x0048ae78 -- scene.c HotspotBuf */
extern SceneViewportSubA gTrigViewport;
extern SceneViewportSubA gSceneViewport;


extern HotspotBufDims gHotspotBuf; /* 0x0048a248 -- scene.c */

extern void *gRenderNodeList;         /* gRenderNodeList */
#pragma pack(push, 1)
typedef struct {
    short type;           /* +0x00 = 1 */
    short pad0;           /* +0x02 = 0 */
    short objX;           /* +0x04 */
    short objY;           /* +0x06 */
    int   range;          /* +0x08 */
    int   basePan;        /* +0x0c */
    int   delta;          /* +0x10 */
    int   targetPan;      /* +0x14 */
    short playerX;        /* +0x18 */
    short playerY;        /* +0x1a */
    unsigned int dist;    /* +0x1c */
    short dirAngle;       /* +0x20 */
    short pad2;           /* +0x22 = 0 */
    byte  unused[6];      /* +0x24 -- never written by the original either */
    byte  ctxDesc[16];    /* +0x2a */
    byte  nodeDesc[18];   /* +0x3a */
} CricketEntry;           /* 0x4c */
#pragma pack(pop)
typedef char CricketEntry_size_check[(sizeof(CricketEntry) == 0x4c) ? 1 : -1];
#endif
