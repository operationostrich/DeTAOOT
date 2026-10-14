/* Declarations for the "heap" topic: the preamble and shared definitions of the functions grouped under that name. */
#ifndef DF_TOPIC_HEAP_H
#define DF_TOPIC_HEAP_H


#include <windows.h>

extern void *gDescPageTail;
extern void *gDescPageHead;
extern int  *gDescFreeList;
extern int   gHeapPageCount;
extern int  *gDescLastEntry;

#endif
