#ifndef DF_TOPIC_RESOURCE_H
#define DF_TOPIC_RESOURCE_H


#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


extern void *gCompactWorkBuf;   /* gCompactWorkBuf -- malloc'd scratch segment table */

extern int *gDescFreeList;      /* DAT_0048a9f4 */
extern void *gCompactWorkBuf;   /* gCompactWorkBuf -- malloc'd scratch segment table */
extern int   gCompactSegCount;  /* gCompactSegCount -- number of segments found       */
extern int   gHeapPageCount;    /* DAT_0048a9fc -- heap page count; sizes scratch  */
extern int *gDescLastEntry;   /* DAT_0048a9a4 -- heap.c: last entry on most-recent page */
extern short kReadBufSizeTable[];
extern int gRenderSlotCounter;
#endif
