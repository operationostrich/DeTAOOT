#ifndef DF_TOPIC_TICK_H
#define DF_TOPIC_TICK_H

#include <windows.h>
#include <mmsystem.h>   /* timeGetTime */

extern DWORD gLastPollTime;   /* 0x0045b938 -- ms timestamp of last PumpMessages run  */
extern int   gGamePaused;     /* 0x0045b618 -- 0 = active; non-0 = paused/lost focus  */
extern int   gMsgPumpDepth;   /* 0x0045b934 -- re-entrancy depth counter               */
extern short gSoundOn;  /* 0x0045d6b8 -- non-0: hardware input flush needed; a WORD in the exe */

#endif
