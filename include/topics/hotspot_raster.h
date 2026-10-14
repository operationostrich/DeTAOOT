/* Declarations for the "hotspot_raster" topic: the preamble and shared definitions of the functions grouped under that name. */
#ifndef DF_TOPIC_HOTSPOT_RASTER_H
#define DF_TOPIC_HOTSPOT_RASTER_H


#include <stdint.h>
#include <windows.h>
#include <stdio.h>

#define CONCAT31_SHIFT(u, b) (((uint32_t)(uint8_t)(b)) | ((((uint32_t)(u)) >> 8) << 8))
#define DWORD_FROM_BYTE(b)                                                                 \
    (((uint32_t)(uint8_t)(b)) | (((uint32_t)(uint8_t)(b)) << 8) |                          \
     (((uint32_t)(uint8_t)(b)) << 16) | (((uint32_t)(uint8_t)(b)) << 24))
#define BSWAP32(v) \
    ((((uint32_t)(v) >> 24) | (((uint32_t)(v)&0xFF0000u) >> 8) | \
      (((uint32_t)(v)&0xFF00u) << 8) | (((uint32_t)(v)&0xFFu) << 24)))

#define CONCAT11(hi, lo) ((uint16_t)(((uint16_t)(uint8_t)(hi) << 8) | (uint8_t)(lo)))
#define CONCAT22(hi, lo) ((uint32_t)(((uint32_t)(uint16_t)(hi) << 16) | (uint16_t)(lo)))

void RaiseFatalPlaneConfigError(void);

#include "df_port_types.h"   /* HotspotPlaneFn */
#define DF_PLANE_ROW(cur, hook, delta, plane)                                      do {                                                                               int rowLen_ = *(short *)(cur);                                                 (cur) += 2;                                                                    (hook)((cur), (delta), (plane));                                               (cur) += rowLen_;                                                          } while (0)
#define PickHotspotPlaneFn() (gTrigPlaneDecodeMode == 2 ? (gTrigMirrored != 0 ? (HotspotPlaneFn)RaiseFatalPlaneConfigError : (HotspotPlaneFn)DecodeHotspotPlaneRowForward) : (gTrigMirrored != 0 ? (HotspotPlaneFn)DecodeHotspotPlaneRowBackward : (HotspotPlaneFn)DecodeHotspotPlaneRow))
#endif
