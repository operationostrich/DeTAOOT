/* Declarations for the "multimedia" topic: the preamble and shared definitions of the functions grouped under that name. */
#ifndef DF_TOPIC_MULTIMEDIA_H
#define DF_TOPIC_MULTIMEDIA_H

#include <windows.h>
#include <string.h>
#include <stdint.h>

#ifndef CONCAT22
#define CONCAT22(hi, lo) ((uint32_t)(((uint32_t)(uint16_t)(hi) << 16) | (uint16_t)(lo)))
#endif
#ifndef CONCAT13
#define CONCAT13(hibyte, lo24) (((uint32_t)(uint8_t)(hibyte) << 24) | ((uint32_t)(lo24) & 0xFFFFFFu))
/* -- Forward declarations -- */
void __stdcall ServiceSoundMixer(void);
void __cdecl   InvokeWaveDeviceSetVolume(int level);
void __stdcall InitDebugger(void);
void __stdcall AllocScriptStringSlots(void);
void __stdcall ShutdownDebugger(void);
void __stdcall FreeScriptStringSlots(void);
void __cdecl DebuggerChannelHandler(int pChannel, short msg, int *pParam);
short __stdcall NetPollNotification(void);
short __stdcall NetPollMessage(void);
short __cdecl PollPuppetEventInput(uint32_t *pEntryIdOut);
void __cdecl DispatchMenuSelectScriptCall(int pChannel, byte *pParam);
void __cdecl DispatchKeyRepeatScriptCall(short keyCode);
void __cdecl GetPlayingSoundNameByPriority(byte *pNameBuf, uint32_t *pRowCtx, uint32_t *pAux);
short __cdecl CheckDescriptorVolumeResolvable(byte *pDesc, short *pFlagOut);
short __cdecl DispatchBootMessageString(byte *pName);  /* 0x0042a470 */

#endif

extern HotspotBuf gHotspotBuf;                        /* 0x0048a248 -- scene.c */
#include "df_keywords.h"
#endif
