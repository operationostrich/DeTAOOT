/* Declarations for the "appinit" topic: the preamble and shared definitions of the functions grouped under that name. */
#ifndef DF_TOPIC_APPINIT_H
#define DF_TOPIC_APPINIT_H


#include <windows.h>
#include <string.h>
#include <stdlib.h>

extern int       *gThePort;   /* 0x004883c8 -- retained scene node reference; NULL until first QdSetPort */
extern HPALETTE   gPalette;      /* 0x00485f4c -- logical HPALETTE; created by SyncLogicalPalette, deleted in Shutdown */

#endif
