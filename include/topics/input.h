#ifndef DF_TOPIC_INPUT_H
#define DF_TOPIC_INPUT_H
/*
 * Before the game loop begins polling events, every named input channel
 * must be registered with the engine's input layer.  The function
 * ConvertMessageNamesToPascal performs this in a single pass over a static
 * flat table of 27 channel name strings (stride 6 per InputChannelEntry)
 * at 0x0045e120.
 */

#include <windows.h>
#include <mmsystem.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Input channel table
 * --------------------------------------------------------------------- */
/* InputChannelEntry defined in df_types.h */

/*
 * Defined here, not as a single string constant:
 * a plain string constant is one literal, but this needs 27 pointers into
 * 27 individually mutable string buffers -- ConvertMessageNameToPascal rewrites
 * each name in place into a Pascal string (memmove + length-prefix byte),
 * so each backing buffer must be real writable memory, not a shared string
 * literal in .rodata. Content is ground truth, read directly from TI.EXE's
 * .rdata (file offset 0x5cd20 for the table, individual strings just
 * before it) by walking the PE section table and following each entry's
 * pointer -- not guessed. Without this, every pName was NULL (zero-init
 * BSS), segfaulting on the very first ConvertMessageNameToPascal call.
 */
extern char g_ich_opencast[9];
extern char g_ich_openactor[10];
extern char g_ich_closecast[10];
extern char g_ich_closeactor[11];
extern char g_ich_openflat[9];
extern char g_ich_openstage[10];
extern char g_ich_closeflat[10];
extern char g_ich_closestage[11];
extern char g_ich_endwalk[8];
extern char g_ich_endturn[8];
extern char g_ich_openshop[9];
extern char g_ich_openprop[9];
extern char g_ich_closeshop[10];
extern char g_ich_closeprop[10];
extern char g_ich_openpuppet[11];
extern char g_ich_closepuppet[12];
extern char g_ich_menustate[10];
extern char g_ich_boot[5];
extern char g_ich_idle[5];
extern char g_ich_menuselect[11];
extern char g_ich_keydown[8];
extern char g_ich_keyrepeat[10];
extern char g_ich_mousedown[10];
extern char g_ich_openset[8];
extern char g_ich_openscene[10];
extern char g_ich_closeset[9];
extern char g_ich_closescene[11];

extern InputChannelEntry g_inputChannelTable[27];

/* -- from between the functions -- */
#define EVENT_QUEUE gEventQueue
#endif
