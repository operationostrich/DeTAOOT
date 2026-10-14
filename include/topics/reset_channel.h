/* Declarations for the "reset_channel" topic: the preamble and shared definitions of the functions grouped under that name. */
#ifndef DF_TOPIC_RESET_CHANNEL_H
#define DF_TOPIC_RESET_CHANNEL_H

#include <windows.h>
#include <stdint.h>
#include <string.h>

typedef unsigned char byte;
#ifndef CONCAT22
#define CONCAT22(hi, lo) \
    ((uint32_t)(((uint32_t)(uint16_t)(hi) << 16) | (uint16_t)(lo)))
#endif


extern void   *gCursorAlt; /* 0x0048a2c8 */
/* SceneViewport / HotspotBuf defined in df_types.h and aliased in df_globals.h */
extern SceneViewport gSceneViewport;
extern HotspotBuf gHotspotBuf;


#endif
