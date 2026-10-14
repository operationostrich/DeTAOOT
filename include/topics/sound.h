#ifndef DF_TOPIC_SOUND_H
#define DF_TOPIC_SOUND_H

#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#endif

#include "df_wave_ring.h"
#define WAVE_HDR_QUEUE_STRIDE 0x32
#define WaveHdrQueueBase(bufIdx) ((WaveHdrQueueRow *)((char *)&gWaveHdrQueues + (bufIdx) * WAVE_HDR_QUEUE_STRIDE))
#include "df_port_types.h"   /* FixedGain / GainTriple */
typedef char SoundSlotTable_size_check[(sizeof(gSoundSlots) == 4 * 26 * sizeof(int)) ? 1 : -1];
#define MIX_CHANNEL_STRIDE 0x8010
typedef UINT     (WINAPI *PFN_mixerGetID)(HMIXEROBJ, UINT *, DWORD);
typedef UINT     (WINAPI *PFN_mixerGetNumDevs)(void);
typedef MMRESULT (WINAPI *PFN_mixerOpen)(LPHMIXER, UINT, DWORD_PTR, DWORD_PTR, DWORD);
typedef MMRESULT (WINAPI *PFN_mixerClose)(HMIXER);
typedef MMRESULT (WINAPI *PFN_mixerGetLineInfoA)(HMIXEROBJ, LPMIXERLINEA, DWORD);
typedef MMRESULT (WINAPI *PFN_mixerGetLineControlsA)(HMIXEROBJ, LPMIXERLINECONTROLSA, DWORD);
typedef MMRESULT (WINAPI *PFN_mixerControlDetails)(HMIXEROBJ, LPMIXERCONTROLDETAILS, DWORD);
#endif
