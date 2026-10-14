/* wave_device_scene_cmds.c -- TI.EXE 0x00406620..0x0040a180: the wave-out device, the mixer
 * volume, and the set / scene / painting script commands after them. */

#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "df_protos.h"
#include "topics/sound.h"
#include "topics/scriptcmd.h"
#include "topics/scriptprop.h"

static int __cdecl ProbeKernel32GetVersionExReportsWinNT(void);
static int __cdecl OpenWaveOutPrimaryDevice(int rateMult, int bytesPerSample, int channels, int *pFramesPerBuffer, int *pQueueTarget, unsigned int *pFlushMask);
static int __stdcall CloseWaveOutPrimaryDevice(void);

#include "topics/savestate.h"

/* Opens the waveform audio device through OpenWaveOutPrimaryDevice and allocates the 6-dword
 * dispatch table. Returns 0 on success, 1 if it is already open, otherwise an error. Parameters:
 * ppDevice, sampleRate, bytesPerSample (1 = 8-bit, 2 = 16-bit), nChannels (1 = mono, 2 = stereo),
 * pWaveFmt, pBufSize, pExtra. */
int __cdecl OpenWaveDevice(int **ppDevice, int sampleRate, int bytesPerSample,  /* 0x00406620 */
                                   int nChannels, int *pWaveFmt, int *pBufSize,
                                   unsigned int *pExtra)
{
    int  iVar;
    int *pDisp;

    iVar = ClassifyWaveOutDeviceVersionCompat();
    if (iVar != 0)
        return 1;

    gWaveAltDeviceOpen = 0;
    iVar = OpenWaveOutPrimaryDevice(sampleRate, bytesPerSample, nChannels, pWaveFmt, pBufSize, pExtra);
    if (iVar == 0)
    {
        pDisp    = (int *)AllocEngineBuffer(0x18);
        pDisp[0] = (int)DequeueWaveOutBuffer;
        pDisp[1] = (int)SubmitWaveOutBuffer;
        pDisp[2] = (int)WaveOutDispatchNoOp;
        pDisp[3] = (int)GetWaveOutQueuedFrames;
        pDisp[4] = (int)QueryWaveOutVolumeLinear9;
        pDisp[5] = (int)SetWaveOutVolumeLevel;
        *ppDevice = pDisp;
        iVar = 0;
    }
    return iVar;
}


/* Sets gWaveOutOnWin32s / gWaveOutOnWin95 / gWaveOutOnWinNT from GetVersion (the NT probe tells NT
 * from Win9x; the comparison is major * 100 + minor > 0x15e, i.e. above 3.50). Returns 1, with all
 * three clear, for a version below 3.10 or an NT at or below 3.50; otherwise 0. */
int __cdecl ClassifyWaveOutDeviceVersionCompat(void) /* 0x004066a0 */
{
    DWORD ver;
    byte  bLo;
    byte  bHi;

    /* the low word only (mov bx,ax); the NT probe's success case first */
    ver = GetVersion();
    bLo = LOBYTE(LOWORD(ver));
    bHi = HIBYTE(LOWORD(ver));
    if (bLo < 3) {
        gWaveOutOnWin32s  = 0;
        gWaveOutOnWin95  = 0;
        gWaveOutOnWinNT = 0;
        return 1;
    }
    if (bLo == 3 && bHi < 10) {
        gWaveOutOnWin32s  = 0;
        gWaveOutOnWin95  = 0;
        gWaveOutOnWinNT = 0;
        return 1;
    }
    if (ProbeKernel32GetVersionExReportsWinNT() != 0) {
        if (bLo * 100 + bHi > 0x15e) {
            gWaveOutOnWin32s  = 0;
            gWaveOutOnWin95  = 0;
            gWaveOutOnWinNT = 1;
            goto done;   /* shares the final `return 0` (0x00406736: xor eax,eax after the stores) */
        }
        gWaveOutOnWin32s  = 0;
        gWaveOutOnWin95  = 0;
        gWaveOutOnWinNT = 0;
        return 1;
    }
    if (bLo * 100 + bHi > 0x15e) {
        gWaveOutOnWin32s  = 0;
        gWaveOutOnWin95  = 1;
        gWaveOutOnWinNT = 0;
        return 0;
    }
    gWaveOutOnWin95  = 0;
    gWaveOutOnWinNT = 0;
    gWaveOutOnWin32s  = 1;
done:
    return 0;
}


/* Loads KERNEL32, resolves GetVersionExA and returns non-zero when dwPlatformId is
 * VER_PLATFORM_WIN32_NT. Used by ClassifyWaveOutDeviceVersionCompat. */
static int __cdecl ProbeKernel32GetVersionExReportsWinNT(void)  /* 0x004067b0 */
{
    HMODULE                      hKernel;
    FARPROC                      pProc;
    OSVERSIONINFOA               osvi;
    typedef BOOL(WINAPI * PFN_GetVersionExA)(LPOSVERSIONINFOA);

    /* 0x004067b0: only the size field is set (first), and the call's BOOL is
     * not tested -- the port had added a memset and a failure return with no
     * DF_FIX; GetVersionExA does not fail on any Windows that runs this */
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    hKernel = LoadLibraryA("KERNEL32.DLL");
    if (hKernel == NULL)
        return 0;
    pProc = GetProcAddress(hKernel, "GetVersionExA");
    if (pProc == NULL) {
        FreeLibrary(hKernel);
        return 0;
    }
    ((PFN_GetVersionExA)(void *)pProc)(&osvi);
    FreeLibrary(hKernel);
    return osvi.dwPlatformId == VER_PLATFORM_WIN32_NT;
}


/* Opens the wave-out device for PCM at rateMult * 11025 Hz: tries each device in turn (purging
 * sndPlaySound once on device 0), then WAVE_MAPPER; resets it, and sets up a ring of 3 buffers of
 * 0x4000 bytes (a GlobalAlloc'd WAVEHDR plus data, prepared with waveOutPrepareHeader). The frames
 * per buffer, the queue target and the flush mask come back through the out-parameters. Returns 0,
 * or 0x388 on a device error (ReportMMError shows the MM result). */
static int __cdecl OpenWaveOutPrimaryDevice(int rateMult, int bytesPerSample, int channels,  /* 0x00406820 */
                                            int *pFramesPerBuffer, int *pQueueTarget, unsigned int *pFlushMask)
{
    UINT          uNumDevs;
    MMRESULT      mmr;
    HGLOBAL       hMem;
    LPVOID        pv;
    int           i;
    int          *pHdrSlot;
    PCMWAVEFORMAT wfx;    /* 16 bytes: the exe's frame has no cbSize */

    if (gWaveOutOpen != 0)
        return 0;

    /* statement order measured against the exe's schedule */
    wfx.wf.wFormatTag  = WAVE_FORMAT_PCM;
    gWaveOutBlockAlign = channels * bytesPerSample;
    gWaveOutSampleRate       = rateMult * 0x2b11;
    wfx.wf.nChannels   = (WORD)channels;
    wfx.wf.nSamplesPerSec = gWaveOutSampleRate;
    wfx.wf.nAvgBytesPerSec = gWaveOutBlockAlign * gWaveOutSampleRate;
    wfx.wBitsPerSample = (WORD)(bytesPerSample << 3);
    wfx.wf.nBlockAlign    = (WORD)gWaveOutBlockAlign;
    uNumDevs = waveOutGetNumDevs();
    gWaveOutDeviceId = 0;
    while (gWaveOutDeviceId < uNumDevs) {
        mmr = waveOutOpen(&gWaveOut, gWaveOutDeviceId, (LPCWAVEFORMATEX)&wfx,
                          (DWORD_PTR)(void *)WaveOutCallback, 0,
                          CALLBACK_FUNCTION);
        if (mmr == MMSYSERR_NOERROR)
            break;
        if (gWaveOutDeviceId == 0) {
            sndPlaySoundA(NULL, 0);   /* Ghidra 0x004068e1: push 0 / push 0 (the port passed SND_PURGE) */
            mmr = waveOutOpen(&gWaveOut, gWaveOutDeviceId, (LPCWAVEFORMATEX)&wfx,
                              (DWORD_PTR)(void *)WaveOutCallback, 0,
                              CALLBACK_FUNCTION);
            if (mmr == MMSYSERR_NOERROR)
                break;
        }
        gWaveOutDeviceId++;
    }
    if (uNumDevs == gWaveOutDeviceId) {
        gWaveOutDeviceId = (UINT)-1;
        mmr = waveOutOpen(&gWaveOut, WAVE_MAPPER, (LPCWAVEFORMATEX)&wfx,
                          (DWORD_PTR)(void *)WaveOutCallback, 0, CALLBACK_FUNCTION);
        if (mmr == MMSYSERR_NOERROR) {
            mmr = waveOutGetID(gWaveOut, &gWaveOutDeviceId);
            if (mmr != MMSYSERR_NOERROR) {
                ReportMMError(mmr, 0);
                return 0x388;
            }
        } else
            return 0x388;
    }
    InitWaveOutMixerVolumeControl(gWaveOut);
    mmr = waveOutReset(gWaveOut);
    if (mmr != MMSYSERR_NOERROR) {
        ReportMMError(mmr, 2);
        return 0x388;
    }
    gWaveOutFlushMask = 1;
    gWaveOutUseGetPosition = 0;
    gWaveOutResetPending = 0;
    gWaveOutBufferBytes       = 0x4000;
    gWaveOutQueueTarget       = 2;
    gWaveOutRingSize       = 3;
    /* 0x004069c8: mov eax,0x4000 / cdq / idiv -- a plain 32-bit divide. The
     * exe stores cd0 before d64/d5c/d60: the compiler moved it up, and only
     * this source order schedules the constant load where the exe has it. */
    gWaveOutFramesPerBuffer = gWaveOutBufferBytes / gWaveOutBlockAlign;
    i              = 0;
    pHdrSlot             = &gWaveOutHdrs;
    for (; i < gWaveOutRingSize; i++) {
        hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, (SIZE_T)gWaveOutBufferBytes + 0x20U);
        pHdrSlot[0x10]       = (int)hMem;
        pv                 = GlobalLock(hMem);
        *pHdrSlot            = (int)pv;
        if (pv == NULL)
            return 0x388;
        /* the exe re-reads the table slot for each field after the first */
        ((LPWAVEHDR)*pHdrSlot)->dwBufferLength  = gWaveOutBufferBytes;
        ((LPWAVEHDR)*pHdrSlot)->dwBytesRecorded = gWaveOutBufferBytes;
        ((LPWAVEHDR)*pHdrSlot)->dwUser          = 0;
        ((LPWAVEHDR)*pHdrSlot)->dwFlags         = 1;
        ((LPWAVEHDR)*pHdrSlot)->dwLoops         = 0;
        {
            /* lpData points just past the WAVEHDR (pv + 0x20; `lea ecx,[eax+0x20]` at 0x00406a52),
             * and the block is allocated as dwBufferLength + 0x20 for exactly that reason. At
             * pv + 8 the audio sat inside the header: real samples overwrote dwBufferLength and
             * dwFlags and waveOutWrite failed (MMSYSERR_INVALPARAM, fatal 0x1ab). */
            ((LPWAVEHDR)*pHdrSlot)->lpData = (LPSTR)*pHdrSlot + 0x20;
        }
        mmr = waveOutPrepareHeader(gWaveOut, (LPWAVEHDR)*pHdrSlot, sizeof(WAVEHDR));
        if (mmr != MMSYSERR_NOERROR) {
            ReportMMError(mmr, 3);
            return 0x388;
        }
        pHdrSlot++;
    }
    gWaveOutOpen   = 1;
    gWaveOutWriteSlot   = 0xffffffff;
    gWaveOutInFlight   = 0;
    gWaveOutPlaySlot   = 0;
    *pFramesPerBuffer       = gWaveOutBufferBytes / gWaveOutBlockAlign;
    *pQueueTarget       = gWaveOutQueueTarget;
    *pFlushMask       = (unsigned int)gWaveOutFlushMask;
    return 0;
}



/* Finds the speaker line's volume control on the mixer that owns hwo (any mixer when hwo is NULL)
 * and opens the gate gMixerVolumeReady. Called before waveOutReset.
 * Globals: gMixer is the HMIXER; gMixerControlDetails is a MIXERCONTROLDETAILS (24 bytes) whose
 * paDetails points at gMixerVolumeValue, the one MIXERCONTROLDETAILS_UNSIGNED value;
 * gMixerVolumeMin / gMixerVolumeMax are the control's dwMinimum / dwMaximum. Volume levels are 0..9
 * throughout, as in the waveOut path. */
void __cdecl InitWaveOutMixerVolumeControl(HWAVEOUT hwo)  /* 0x00406AF0 */
{
    /* Declaration order is load-bearing (edi = i, esi = mixerEnd). */
    UINT                 mixerEnd;   /* ESI */
    UINT                 i;          /* EDI: the loop walks a copy of mixerId */
    UINT                 mixerId;    /* [esp+0x10] */
    MIXERLINECONTROLSA   mlc;        /* [esp+0x14], 0x18 bytes */
    MIXERCONTROLA        mc;         /* [esp+0x2c], 0x94 bytes */
    MIXERLINEA           ml;         /* [esp+0xc0], 0xa8 bytes */
#define pDetails ((MIXERCONTROLDETAILS *)gMixerControlDetails)   /* direct stores, no pointer local */

    if (gMixerVolumeReady != 0)
        return;
    if (InitializeMixerControlAPIs() != 0) {
        DF_TRACE("mixerVolume: mixer API unavailable (WINMM entry points missing)");
        return;
    }

    if (hwo != NULL) {
        if (((PFN_mixerGetID)gMixerGetIDFn)((HMIXEROBJ)hwo, &mixerId,
                                           MIXER_OBJECTF_HWAVEOUT) != 0) {   /* 0x90000000 */
            DF_TRACE("mixerVolume: waveOut device has no mixer (mixerGetID failed)");
            UnloadWinmm();
            return;
        }
        mixerEnd = mixerId + 1;
    } else {
        mixerId  = 0;
        mixerEnd = ((PFN_mixerGetNumDevs)gMixerGetNumDevsFn)();
    }

    for (i = mixerId; i < mixerEnd; i++) {
        if (((PFN_mixerOpen)gMixerOpenFn)((LPHMIXER)&gMixer, i, 0, 0, 0) != 0) {
            DF_TRACE("mixerVolume: mixer %u: mixerOpen failed", i);
            continue;
        }

        ml.cbStruct        = 0xa8;
        ml.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;           /* 4 */
        if (((PFN_mixerGetLineInfoA)gMixerGetLineInfoFn)((HMIXEROBJ)gMixer, &ml,
                                                  MIXER_GETLINEINFOF_COMPONENTTYPE) != 0) { /* 3 */
            DF_TRACE("mixerVolume: mixer %u: no DST_SPEAKERS line", i);
            ((PFN_mixerClose)gMixerCloseFn)((HMIXER)gMixer);
            continue;
        }

        mlc.cbStruct      = 0x18;
        mlc.dwLineID      = ml.dwLineID;
        mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;                 /* 0x50030001 */
        mlc.cControls     = 1;
        mlc.cbmxctrl      = 0x94;
        mlc.pamxctrl      = &mc;
        if (((PFN_mixerGetLineControlsA)gMixerGetLineControlsFn)((HMIXEROBJ)gMixer, &mlc,
                MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE) != 0) { /* 0x80000002 */
            DF_TRACE("mixerVolume: mixer %u: line %lu has no VOLUME control",
                     i, (unsigned long)ml.dwLineID);
            ((PFN_mixerClose)gMixerCloseFn)((HMIXER)gMixer);
            continue;
        }

        pDetails->cbStruct       = 0x18;
        pDetails->dwControlID    = mc.dwControlID;
        pDetails->cChannels      = 1;
        pDetails->cMultipleItems = mc.cMultipleItems;
        pDetails->cbDetails      = 4;
        pDetails->paDetails      = &gMixerVolumeValue;
        gMixerVolumeMin = (int)mc.Bounds.dwMinimum;
        gMixerVolumeMax = (int)mc.Bounds.dwMaximum;
        break;
    }

    if (i >= mixerEnd) {
        DF_TRACE("mixerVolume: no usable volume control -- waveOutSetVolume fallback");
        UnloadWinmm();
        return;
    }
    gMixerVolumeReady = 1;
    DF_TRACE("mixerVolume: using mixer %u control %lu range %u..%u", i,
             (unsigned long)pDetails->dwControlID,
             (unsigned)gMixerVolumeMin, (unsigned)gMixerVolumeMax);
#undef pDetails
}


/* Loads WINMM.DLL and resolves the mixer control API pointers. */
undefined4 __stdcall InitializeMixerControlAPIs(void)  /* 0x00406CB0 */
{
  gWinmmLib = (int)LoadLibraryA("WINMM.DLL" /* 0x0045b11c */);
  if (gWinmmLib == 0) {   /* compared uncast: only this spelling gives the exe's second store at 0x00406cd8 */
    return 1;
  }
  gMixerGetIDFn = (int)GetProcAddress((HMODULE)gWinmmLib,"mixerGetID" /* 0x0045b110 */);
  gMixerGetNumDevsFn = (int)GetProcAddress((HMODULE)gWinmmLib,"mixerGetNumDevs" /* 0x0045b100 */);
  gMixerOpenFn = (int)GetProcAddress((HMODULE)gWinmmLib,"mixerOpen" /* 0x0045b0f4 */);
  gMixerCloseFn = (int)GetProcAddress((HMODULE)gWinmmLib,"mixerClose" /* 0x0045b0e8 */);
  gMixerGetLineInfoFn = (int)GetProcAddress((HMODULE)gWinmmLib,"mixerGetLineInfoA" /* 0x0045b0d4 */);
  gMixerGetLineControlsFn = (int)GetProcAddress((HMODULE)gWinmmLib,"mixerGetLineControlsA" /* 0x0045b0bc */);
  gMixerGetControlDetailsFn = (int)GetProcAddress((HMODULE)gWinmmLib,"mixerGetControlDetailsA" /* 0x0045b0a4 */);
  gMixerSetControlDetailsFn = (int)GetProcAddress((HMODULE)gWinmmLib,"mixerSetControlDetails" /* 0x0045b08c */);
  if (((((gMixerGetIDFn != 0) && (gMixerGetNumDevsFn != 0)) && (gMixerOpenFn != 0)) &&
      ((gMixerCloseFn != 0 && (gMixerGetLineInfoFn != 0)))) &&
     ((gMixerGetLineControlsFn != 0 && ((gMixerGetControlDetailsFn != 0 && ((FARPROC)gMixerSetControlDetailsFn != (FARPROC)0x0)))))) {
    return 0;
  }
  FreeLibrary((HMODULE)gWinmmLib);
  return 1;
}


/* Releases gWinmmLib (FreeLibrary). */
void __stdcall UnloadWinmm(void) /* 0x00406DC0 */
{
    FreeLibrary(gWinmmLib);
}


/* Shows a MessageBox with string resources 0x99 and 0x9a formatted with mmResult and tag,
 * word-wrapped through WordWrapText, with the game UI suspended. If the user clicks OK it calls
 * FatalErrorWithCode(0x1ab), which exits. */
void __cdecl ReportMMError(MMRESULT mmResult, int tag)  /* 0x00406dd0 */
{
    char   fmtBuf[512];   /* format string buffer */  /* local_400 */
    char   msgBuf[512];   /* formatted message buffer */  /* local_200 */
    char  *unusedPtr;
    int    result;

    strcpy(fmtBuf, (char *)GetStringResource(0x99));
    strcat(fmtBuf, (char *)GetStringResource(0x9a));

    sprintf(msgBuf, fmtBuf, mmResult, tag);

    SuspendGameUiForModalDialog();
    WordWrapText(msgBuf, fmtBuf);

    /* 0x2031, as pushed at 0x00406e83 */
    result = MessageBoxA(NULL, fmtBuf, (LPCSTR)GetStringResource(0x9b),
                         MB_OKCANCEL | MB_ICONEXCLAMATION | MB_TASKMODAL);
    RestoreGameUiAfterModalDialog();

    if (result == IDOK) {
        FatalErrorWithCode(0x1ab);
    }
}


/* The waveOut CALLBACK_FUNCTION completion handler. On WOM_DONE (0x3bd) it searches the WAVEHDR
 * pointer ring (gWaveOutHdrs, gWaveOutRingSize entries) for the completed header (matched by the
 * dwParam1 pointer); if found it advances the head (gWaveOutPlaySlot) to just past it, decrements
 * the in-flight count (gWaveOutInFlight), wraps the head at capacity and, if buffers remain in
 * flight, stamps the new head's WAVEHDR at +0xc with timeGetTime() (the submission time that
 * SubmitWaveOutBuffer writes there). It takes 5 parameters, the real waveOutProc ABI, so that the
 * __stdcall cleanup matches what winmm pushes; dwParam2 is unused (Ghidra saw 4).
 *
 * Port note: DF_FIX_WAVE_RING_LOCK takes gWaveRingLock around the counters, which the original
 * leaves unsynchronised: modern winmm calls this from a worker thread, racing PumpSoundMixer.
 * It must not be the mixer lock, which the main thread holds across waveOutReset, so that
 * deadlock froze the game inside wdmaud.drv. The body was once a
 * no-op, the root cause of fatal 0x1ad "Too many sound buffers playing" (class 7). */
void CALLBACK WaveOutCallback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,  /* 0x00406ed0 */
                                 DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    int slot;
    uint32_t *pRing = (uint32_t *)&gWaveOutHdrs;

    (void)hwo;
    (void)dwInstance;
    (void)dwParam2;

    if (uMsg == WOM_DONE) {
        WAVE_RING_LOCK();
        slot = 0;
        while (slot < gWaveOutRingSize && pRing[slot] != (uint32_t)dwParam1)
            slot++;
        DF_TRACE("WOM_DONE: hdr=%p slot=%d/%d inFlight=%d", (void *)dwParam1,
                 slot, (int)gWaveOutRingSize, (int)gWaveOutInFlight);
        if (slot < gWaveOutRingSize) {
            gWaveOutPlaySlot = slot + 1;
            gWaveOutInFlight = gWaveOutInFlight - 1;
            if (gWaveOutRingSize <= gWaveOutPlaySlot)
                gWaveOutPlaySlot = 0;
            if (gWaveOutInFlight != 0)
                *(DWORD *)((char *)(uintptr_t)pRing[gWaveOutPlaySlot] + 0xc) = timeGetTime();
        }
        WAVE_RING_UNLOCK();
    }
}


/* Wave dispatch op 0: advances the write cursor of the WAVEHDR ring and returns the next slot's
 * data pointer. markReset sets gWaveOutResetPending, so the following SubmitWaveOutBuffer resets
 * the device first. */
uint32_t __cdecl DequeueWaveOutBuffer(uint32_t *pHdrOut, int markReset) /* 0x00406F50 */
{
    int slot;

    if (gWaveOutOpen == 0)
        return 0x388U;
    if (gWaveOutWriteSlot != (unsigned int)-1)
        FatalErrorWithCode(0x1ac);
    /* Read the ring state under g_waveRingLock: the completion callback runs
     * on a winmm worker thread and mutates the same counters. */
    WAVE_RING_LOCK();
    DF_TRACE("DequeueWaveOutBuffer: inFlight=%d of %d (cursor=%d)",
             (int)gWaveOutInFlight, (int)gWaveOutRingSize, (int)gWaveOutPlaySlot);
    if (gWaveOutInFlight >= gWaveOutRingSize) {
        WAVE_RING_UNLOCK();
        FatalErrorWithCode(0x1ad);
    }
    /* the exe works on the global itself: stores the sum, then wraps it */
    gWaveOutWriteSlot = gWaveOutInFlight + gWaveOutPlaySlot;
    if (gWaveOutWriteSlot >= gWaveOutRingSize)            /* signed: cmp / jg */
        gWaveOutWriteSlot = gWaveOutWriteSlot - gWaveOutRingSize;
    slot = (int)gWaveOutWriteSlot;
    /* Returns the buffer's DATA pointer (WAVEHDR.lpData, offset 0: `mov ecx,[edx]` at
     * 0x00406fc7), not the header. The caller writes mixed audio straight through it; returning
     * the header made the mixer overwrite dwBufferLength and dwFlags and waveOutWrite failed. */
    *pHdrOut = *(uint32_t *)(uintptr_t)((uint32_t *)&gWaveOutHdrs)[slot];
    DF_TRACE("  deq slot=%d hdr=%p lpData=%p len=%lu", slot,
             (void *)(uintptr_t)((uint32_t *)&gWaveOutHdrs)[slot],
             (void *)(uintptr_t)*pHdrOut,
             (unsigned long)((LPWAVEHDR)(uintptr_t)((uint32_t *)&gWaveOutHdrs)[slot])->dwBufferLength);
    WAVE_RING_UNLOCK();
    if (markReset != 0)
        gWaveOutResetPending = 1;
    return 0U;
}


/* Wave dispatch op 1: waveOutWrite on the dequeued header. */
uint32_t __stdcall SubmitWaveOutBuffer(void) /* 0x00406FE0 */
{
    MMRESULT mmr;

    if (gWaveOutOpen == 0)
        return 0x388U;
    if (gWaveOutWriteSlot == (unsigned int)-1)
        FatalErrorWithCode(0x1ae);
    if (gWaveOutResetPending != 0) {
#if DF_TRACE_ENABLED
        /* Reset cost vs. reset interval. The play-position estimate
         * (GetWaveOutQueuedFrames) is stamped AFTER this block,
         * so any time spent here is never counted as played. */
        static DWORD s_tPrevReset;
        DWORD tResetStart = timeGetTime(), tResetEnd;
#endif
        mmr = waveOutReset(gWaveOut);
        if (mmr != MMSYSERR_NOERROR) {
            ReportMMError(mmr, 6);
            return 0x388U;
        }
#if DF_TRACE_ENABLED
        tResetEnd = timeGetTime();
#endif
        gWaveOutResetPending = 0;
        ProcessWaveOutMessages();
#if DF_TRACE_ENABLED
        DF_TRACE("waveOutReset: reset=%lums drain=%lums sincePrevReset=%lums",
                 (unsigned long)(tResetEnd - tResetStart),
                 (unsigned long)(timeGetTime() - tResetEnd),
                 (unsigned long)(tResetStart - s_tPrevReset));
        s_tPrevReset = tResetStart;
#endif
    }
    /* The exe indexes the header table afresh at each use (no local). */
#define pHdr ((LPWAVEHDR)(uintptr_t)((uint32_t *)&gWaveOutHdrs)[gWaveOutWriteSlot])
    if (gWaveOutInFlight == 0) {
        gWaveOutPlaySlot = (int)gWaveOutWriteSlot;
        pHdr->dwUser = timeGetTime();   /* +0xc */
        waveOutPause(gWaveOut);
    }
    mmr = waveOutWrite(gWaveOut, pHdr, sizeof(WAVEHDR));
    DF_TRACE("waveOutWrite hdr=%p len=%lu flags=0x%lx -> mmr=%u (inFlight was %d)",
             (void *)pHdr, (unsigned long)pHdr->dwBufferLength,
             (unsigned long)pHdr->dwFlags, (unsigned)mmr, (int)gWaveOutInFlight);
#undef pHdr
    if (mmr != MMSYSERR_NOERROR) {
        ReportMMError(mmr, 7);
        return 0x388U;
    }
    if (gWaveOutInFlight == 0)
        waveOutRestart(gWaveOut);
    /* Increment under the ring lock -- the completion callback decrements it
     * from a winmm worker thread. Note waveOutWrite above is deliberately
     * OUTSIDE the lock: a buffer can complete (and the callback fire) before
     * waveOutWrite even returns. */
    WAVE_RING_LOCK();
    gWaveOutInFlight = gWaveOutInFlight + 1;
    WAVE_RING_UNLOCK();
    gWaveOutWriteSlot = (unsigned int)-1;
    return 0U;
}


/* On Win32s only: pumps the message queue until every submitted wave buffer has completed
 * (gWaveOutInFlight == 0). */
void __cdecl ProcessWaveOutMessages(void)  /* 0x004070D0 */
{
  MSG msg;

  if (gWaveOutInFlight != 0 && gWaveOutOnWin32s != 0) {
    do {
      if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
      }
    } while (gWaveOutInFlight != 0);
  }
}


/* Wave dispatch op 2: intentional no-op. */
uint32_t __stdcall WaveOutDispatchNoOp(void) /* 0x00407130 */
{
    return 0U;
}


/* Wave dispatch op 3: writes to *pQueuedFrames the number of frames still queued: the in-flight
 * buffers times the frames per buffer, minus the part of the head buffer already played
 * (waveOutGetPosition samples when gWaveOutUseGetPosition is set, else the time since the head was
 * stamped, from timeGetTime and the sample rate), clamped to the buffer. Returns 0x388 if the
 * device is not open. */
unsigned int __cdecl GetWaveOutQueuedFrames(int *pQueuedFrames)  /* 0x00407140 */
{
    MMRESULT   mmr;
    int        queued;
    int        played;
    int        inFlight;
    MMTIME     mmTime;  /* local_c */

    if (gWaveOutOpen == 0)
        return 0x388;
    /* one read of the in-flight count (the WOM_DONE callback decrements it on
     * a winmm thread): the exe tests the loaded register, never memory again */
    inFlight     = *(volatile int *)&gWaveOutInFlight;
    gWaveOutSyncInFlight = inFlight;
    queued        = inFlight * gWaveOutFramesPerBuffer;
    gWaveOutSyncPlaySlot = gWaveOutPlaySlot;
    if (inFlight != 0) {
        if (gWaveOutUseGetPosition != 0) {
            mmTime.wType = TIME_SAMPLES;
            mmr = waveOutGetPosition(gWaveOut, &mmTime, sizeof(mmTime));
            if (mmr != MMSYSERR_NOERROR)
                ReportMMError(mmr, 8);
            if (mmTime.wType != TIME_SAMPLES)
                ReportMMError(mmr, 9);
            played = (int)mmTime.u.sample;
        } else {
            /* signed: the exe divides with cdq / idiv */
            played = (int)(timeGetTime() -
                           ((LPWAVEHDR)(uintptr_t)(unsigned int)gWaveOutHdrs[gWaveOutSyncPlaySlot])->dwUser) *
                     gWaveOutSampleRate / 1000;
        }
        if (played < 0)
            played = 0;
        else if (gWaveOutFramesPerBuffer <= played)
            played = gWaveOutFramesPerBuffer - 1;
        queued -= played;
    }
    DF_TRACE("SyncPos: inFlight=%d framesPerBuf=%d played=%d -> queued=%d "
             "(useGetPos=%d)", (int)gWaveOutInFlight, (int)gWaveOutFramesPerBuffer,
             gWaveOutInFlight != 0 ? played : 0, queued, (int)gWaveOutUseGetPosition);
    *pQueuedFrames = queued;
    return 0;
}


/* Reads the wave-out volume back as a 0..9 level through waveOutGetVolume (or the mixer control
 * when gMixerVolumeReady is set). Returns 0x388 on a device error; a driver without volume support
 * reports 9. */
unsigned int __cdecl QueryWaveOutVolumeLinear9(unsigned int *pLevel)  /* 0x00407220 */
{
    MMRESULT     mmr;
    DWORD        dwVol;

    /* 0x00407220: the exe's order -- not-supported first, then the error,
     * then the scale, with the masked volume stored back into dwVol */
    if (gMixerVolumeReady != 0)
        return QueryMixerVolumeLevel9(pLevel);
    if (gWaveOutOpen == 0)
        return 0x388;
    if (gWaveOutOnWin32s == 0)
        mmr = waveOutGetVolume((HWAVEOUT)(uintptr_t)gWaveOut, &dwVol);
    else
        mmr = waveOutGetVolume((HWAVEOUT)(uintptr_t)gWaveOutDeviceId, &dwVol);
    if (mmr == MMSYSERR_NOTSUPPORTED) {
        *pLevel = 9;
        return 0;
    }
    if (mmr != MMSYSERR_NOERROR) {
        ReportMMError(mmr, 10);
        return 0x388;
    }
    dwVol = LOWORD(dwVol);
    dwVol = dwVol * 9 / 0xffff;
    DF_TRACE("vol: waveOutGetVolume -> level %u", (unsigned)dwVol);
    *pLevel = dwVol;
    return 0;
}


/* Reads the mixer volume back as a 0..9 level; returns 0 on success, 1 on failure.
 * level = (cur - min) * 9 / (max - min), rounded, clamped to 9.
 *
 * Under VC++ 4.2 (the match build) this is a __declspec(naked) inline-asm body, because no C
 * input to that compiler produces the exe's bytes: the exe keeps the three zero stores of the
 * two 64-bit integer conversions separate, where every C spelling merges them into one
 * `xor ecx,ecx` (a C2 constant-merge decision; docs/differences.md). The C body below is the believed original source and what the GCC port runs;
 * under _MSC_VER it is compiled as QueryMixerVolumeLevel9_Pool (the linker discards it) so that
 * its 9.0 and 0.5 enter the unit's .rdata pool in the original order (SetMixerVolumeLevel9
 * reuses the 0.5).
 * Asm spelling rules (measured): a global named in __asm is asm-referenced for the rest of the
 * unit (later functions reload it), so globals and literals are addressed as this function's
 * own symbol plus a displacement (DATA_); `[esp+0]` loses its zero disp8 in every spelling, so
 * those six instructions are _emit'ed. */
#ifdef _MSC_VER
#define DF_NAKED __declspec(naked)
#define DATA_(a) QueryMixerVolumeLevel9 + (0x##a - 0x004072d0)   /* a global or literal, no C name */
long __cdecl _ftol(void);
DF_NAKED unsigned int __cdecl QueryMixerVolumeLevel9(unsigned int *pLevelOut)  /* 0x004072D0 */
{
    __asm {
            sub esp, 0x8
            mov eax, dword ptr [DATA_(00460d8c)]    ; _DAT_00460d8c_pzz
            push   0x0
            push   offset gMixerControlDetails
            push   eax
            call   dword ptr [DATA_(00460c18)]    ; _DAT_00460c18_qzz
            test eax, eax
            je short L_23
            mov eax, 0x1
            add esp, 0x8
            ret
L_23:       mov eax, dword ptr [DATA_(00460d74)]    ; _DAT_00460d74_ix
            mov ecx, dword ptr [DATA_(00460c3c)]    ; _DAT_00460c3c_zz
            _emit 0x89
            __asm _emit 0x44
            __asm _emit 0x24
            __asm _emit 0x00    ; mov    dword ptr [esp+0x0],eax
            mov dword ptr [esp+0x4], 0x0
            _emit 0xdf
            __asm _emit 0x6c
            __asm _emit 0x24
            __asm _emit 0x00    ; fild   qword ptr [esp+0x0]
            mov eax, dword ptr [DATA_(00460c40)]    ; _DAT_00460c40_pxw
            _emit 0x89
            __asm _emit 0x4c
            __asm _emit 0x24
            __asm _emit 0x00    ; mov    dword ptr [esp+0x0],ecx
            mov dword ptr [esp+0x4], 0x0
            _emit 0xdf
            __asm _emit 0x6c
            __asm _emit 0x24
            __asm _emit 0x00    ; fild   qword ptr [esp+0x0]
            sub eax, ecx
            _emit 0x89
            __asm _emit 0x44
            __asm _emit 0x24
            __asm _emit 0x00    ; mov    dword ptr [esp+0x0],eax
            fsubp st(1), st
            mov dword ptr [esp+0x4], 0x0
            fmul   qword ptr [DATA_(0045a020)]    ; 9.0
            _emit 0xdf
            __asm _emit 0x6c
            __asm _emit 0x24
            __asm _emit 0x00    ; fild   qword ptr [esp+0x0]
            fdivp st(1), st
            fadd   qword ptr [DATA_(0045a028)]    ; 0.5
            call _ftol
            cmp eax, 0x9
            jbe short L_84
            mov eax, 0x9
L_84:       mov ecx, dword ptr [esp+0xc]
            mov dword ptr [ecx], eax
            xor eax, eax
            add esp, 0x8
            ret
    }
}
#undef DATA_
/* the believed source, compiled for its constants only (see above); discarded by the linker */
unsigned int __cdecl QueryMixerVolumeLevel9_Pool(unsigned int *pLevelOut)
#else
unsigned int __cdecl QueryMixerVolumeLevel9(unsigned int *pLevelOut)  /* 0x004072D0 */
#endif
{
    double   v;
    uint32_t level;

    if (((PFN_mixerControlDetails)gMixerGetControlDetailsFn)((HMIXEROBJ)gMixer,
            (MIXERCONTROLDETAILS *)gMixerControlDetails, 0) != 0)
        return 1;
    /* 0x0045a020, 0x0045a028; the cast is the exe's inline __ftol call */
    v = ((double)(uint32_t)gMixerVolumeValue - (double)(uint32_t)gMixerVolumeMin) * 9.0;
    level = (uint32_t)(long)(v / (double)(uint32_t)(gMixerVolumeMax - gMixerVolumeMin) + 0.5);
    if (9 < level)
        level = 9;
    *pLevelOut = level;
    return 0;
}


/* Wave dispatch op 5: maps a 0..9 level to waveOutSetVolume (or to the mixer control when
 * gMixerVolumeReady is set). Returns 0x388 on a device error. */
int __cdecl SetWaveOutVolumeLevel(int level) /* 0x00407360 */
{
    uint32_t vol;
    uint32_t dw;
    MMRESULT mmr;

    if (gMixerVolumeReady != 0)
        return SetMixerVolumeLevel9(level);
    if (gWaveOutOpen == 0)
        return 0x388;   /* the exe returns 0x388 here; the port returned 1 */
    /* 0x00407386: shl/sub gives level*0xFFFF, then CLTD; IDIV -- signed. */
    vol = (uint32_t)(((int)level * 0xffff) / 9) & 0xffffU;
    /* two calls: the compiler hoists the shared `push dw` above the branch */
    dw = vol;
    dw = dw | (dw << 16);
    if (gWaveOutOnWin32s == 0)
        mmr = waveOutSetVolume(gWaveOut, dw);
    else
        mmr = waveOutSetVolume(gWaveOutDeviceId, dw);
    DF_TRACE("vol: waveOutSetVolume level=%d -> 0x%04x (mmr=%u)",
             level, (unsigned)vol, (unsigned)mmr);
    if (mmr == MMSYSERR_NOTSUPPORTED)   /* cmp eax,8 / jne: tested first, as the exe does */
        return 0;
    if (mmr != MMSYSERR_NOERROR) {
        ReportMMError(mmr, 8);
        return 0x388;   /* the exe returns 0x388 (0x004073d9), as the getter does; the port returned 1 */
    }
    return 0;
}


/* Set the mixer volume from a 0..9 level. Returns 0 on success, 1 on failure.
 * value = min + (max - min) * level / 9, rounded; the range is loaded as an
 * UNSIGNED 64-bit integer (FILD qword with a zero high dword). */
int __cdecl SetMixerVolumeLevel9(int level)  /* 0x004073F0 */
{
    double v;

    v = (double)(uint32_t)(gMixerVolumeMax - gMixerVolumeMin);
    v = v * (double)level * 0.1111111111111111 + 0.5;   /* 0x0045a030, 0x0045a028 */
    gMixerVolumeValue = (long)v + gMixerVolumeMin;   /* the cast is the exe's inline __ftol call */
    DF_TRACE("mixerVolume: set level=%d -> %u", level, (unsigned)gMixerVolumeValue);
    return ((PFN_mixerControlDetails)gMixerSetControlDetailsFn)((HMIXEROBJ)gMixer,
               (MIXERCONTROLDETAILS *)gMixerControlDetails, 0) != 0;
}


/* Closes the wave device and frees the dispatch object. The normal path is
 * CloseWaveOutPrimaryDevice (gWaveAltDeviceOpen == 0); otherwise the alternate path,
 * CloseDirectSoundBuffer and ReleaseDirectSound. */
unsigned int __cdecl CloseWaveDevice(int hDevice)  /* 0x00407450 */
{
    unsigned int uResult;

    if (gWaveAltDeviceOpen != 0)              /* the exe falls through into this branch */
    {
        uResult = (unsigned int)CloseDirectSoundBuffer();
        ReleaseDirectSound();
    }
    else
        uResult = (unsigned int)CloseWaveOutPrimaryDevice();
    FreeEngineBuffer(hDevice);
    return uResult;
}


/* Second step of the alternate-device shutdown: calls the object's vtable slot +8 and frees the
 * library handle. */
int __stdcall ReleaseDirectSound(void)  /* 0x00407480 */
{
  /* The object is the one argument (push eax; mov eax,[eax]; call [eax+8]); the slot is
   * __stdcall, so there is no ADD ESP. Dispatch-slot arity, class 2. */
  void *pObj = (void *)DAT_00460c64;
  (*(void (__stdcall **)(void *))(*(char **)pObj + 8))(pObj);
  FreeLibrary(DAT_00460c60);
  return 0;
}

/* First step of the alternate-device shutdown: stops the audio timer, calls the object's vtable
 * slots +0x48 and +8, frees the buffers and closes the mixer (CloseMixerVolumeControl). */
uint32_t __stdcall CloseDirectSoundBuffer(void)  /* 0x004074A0 */
{
  CleanupAudioTimerEvent();
  /* obj->vtbl->fn(obj): the exe pushes the object, loads its vtable and calls [vtbl+0x48], then
   * [vtbl+8]; stdcall, no ADD ESP. The port had indexed the object itself. Class 2. */
  (*(void (__stdcall **)(void *))(*(char **)(intptr_t)DAT_00460c6c + 0x48))((void *)(intptr_t)DAT_00460c6c);
  (*(void (__stdcall **)(void *))(*(char **)(intptr_t)DAT_00460c6c + 8))((void *)(intptr_t)DAT_00460c6c);
  FreeEngineBuffer(DAT_00460c68);
  FreeEngineBuffer(DAT_00460c9c);
  CloseMixerVolumeControl();   /* the mixer close; the port had drained messages here instead */
  return 0;
}


/* Closes the mixer (mixerClose) if the volume control was opened, clears gMixerVolumeReady and
 * unloads winmm. */
void __cdecl CloseMixerVolumeControl(void)  /* 0x004074F0 */
{
  if (gMixerVolumeReady != 0) {
    ((PFN_mixerClose)gMixerCloseFn)((HMIXER)gMixer);   /* 0x004074f9: PUSH [0x00460d8c] */
    gMixerVolumeReady = 0;
    UnloadWinmm();
    return;
  }
  return;
}


/* Terminates the audio timer event and ends the timer period setting. */
undefined4 __stdcall CleanupAudioTimerEvent(void)  /* 0x00407520 */
{
  timeKillEvent(DAT_00460d80);
  timeEndPeriod(DAT_00460d84);
  return 0;
}


/* Resets the device, unprepares and frees the ring's WAVEHDR buffers, closes it and closes the
 * mixer volume control. Returns 0, or 0x388 on a device error. */
static int __stdcall CloseWaveOutPrimaryDevice(void)  /* 0x00407540 */
{
    MMRESULT mmr;
    HGLOBAL *ph;
    int      i;

    if (gWaveOutOpen == 0)
        return 0;
    mmr = waveOutReset(gWaveOut);
    if (mmr != MMSYSERR_NOERROR) {
        ReportMMError(mmr, 9);
        return 0x388;
    }
    i = 0;
    if (0 < gWaveOutRingSize) {
        ph = &gWaveOutHdrHandles;
        do {
            mmr = waveOutUnprepareHeader(
                gWaveOut,
                (LPWAVEHDR)(uintptr_t)(*(int *)((int *)ph - 0x10)),
                sizeof(WAVEHDR));
            if (mmr != MMSYSERR_NOERROR) {
                ReportMMError(mmr, 5);
                return 0x388;
            }
            GlobalUnlock(*ph);
            GlobalFree(*ph);
            ph = ph + 1;
            i = i + 1;
        } while (i < gWaveOutRingSize);
    }
    mmr = waveOutClose(gWaveOut);
    if (mmr != MMSYSERR_NOERROR) {
        ReportMMError(mmr, 4);
        return 0x388;
    }
    ProcessWaveOutMessages();
    CloseMixerVolumeControl();
    gWaveOutOpen = 0;
    gWaveAltDeviceOpen = 0;
    return 0;
}


/* Script command opensetfile (opcode 12032), guarded by gSet. Evaluates one required descriptor
 * argument and, after a 0xfb4 separator, optionally two more; validates the primary descriptor,
 * passes all three to OpenSetNode, refreshes the subsystem state and finalises through
 * CompleteSetInit. */
short __cdecl ActionCmd_OpenSetFile(char *pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x00407620 */
{
    /* One skip count for both evaluators, and the argument block walks the
     * parameter with its start kept (byte-exact, 0x00407620). */
    short  status;
    int    skip;
    short *start;
    short  opt1Desc[4];  /* local_218 */
    short  opt2Desc[4];  /* local_210 */
    short  nameDesc[4];  /* local_208 */
    byte   opt2Str[256];  /* local_200 */
    byte   opt1Str[256];  /* local_100 */

    /* An OPEN errors when a set is ALREADY open (gSet != 0 returns 0x28, as
     * ActionCmd_OpenStageFile does with gStage and 0xb). The port had the polarity inverted, so
     * the first opensetfile always failed with 0x28 and OpenSetNode was never reached. */
    if (gSet.active != 0)
        return 0x28;

    start = pArgBlock;
    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)nameDesc, &skip);
    if (status != 0)
        return status;

    opt1Str[0] = 0;
    opt2Str[0] = 0;
    pArgBlock += skip * 4;

    if (*pArgBlock == 0xfb4)
    {
        pArgBlock += 4;
        status = EvalTwoScriptArgs(pCtx, (int)(intptr_t)pArgBlock, pTable,
                             (int *)opt1Desc, (int *)opt2Desc, &skip);
        if (status != 0)
            return status;

        pArgBlock += skip * 4;

        status = TakeScriptString(opt1Desc, opt1Str);
        if (status != 0)
            return status;

        status = TakeScriptString(opt2Desc, opt2Str);
        if (status != 0)
            return status;
    }

    *pSubSkip = ((int)pArgBlock - (int)start) >> 3;

    status = TakeScriptString(nameDesc, gScratchStr2);
    if (status == 0)
    {
        status = ValidateDescriptorPath(gScratchStr2, 0x5343444f)   /* 'SCDO' */;
        if (status == 0)
        {
            status = OpenSetNode(gScratchStr2, (short *)&gSet, opt1Str, opt2Str);
            if (status == 0)
            {
                PlaceActorsAtWaypoints();
                PlacePropsAtWaypoints();
                SelectViewByNameOrNearestAngle();
                status = CompleteSetInit();
            }
        }
    }

    return status;
}


/* Opens the set resource named by pDesc for ActionCmd_OpenSetFile: takes the matching pending
 * render node, attaches its child 0 and fills pState (shorts and ints from the set row, plus the
 * optional names pOpt1 / pOpt2). If the row is not a set (type word 0x40000) it detaches, closes the
 * node and returns 0x38. */
short __cdecl OpenSetNode(byte *pDesc, short *pState, byte *pOpt1, byte *pOpt2)  /* 0x00407780 */
{
    /* Shape from 0x00407780: the wrong-type case first, each selector read
     * once into a local, the row's short fields loaded as words, and the
     * optional names as if/else calls. */
    int i;
    short u;
    int local4;
    int *walk;
    int *row;
    short err;
    int *hist;
    uint sel;

    err = TakeMatchingPendingRenderNode(pDesc, (int *)((char *)pState + 8));
    if (err != 0)
        FatalError(err, 0xff4);

    err = AttachRenderChild(*(int **)((char *)pState + 8), 0u, (int **)((char *)pState + 12));
    if (err != 0)
        FatalError(err, 0xff5);

    row = (int *)GetResDataPtr(*(int **)((char *)pState + 12));
    if (*row != 0x40000) {
        UnlockResObject(*(int **)((char *)pState + 12));
        u = DetachRenderChild(*(int *)((char *)pState + 8), 0u);
        if (u != 0)
            FatalError(u, 0xff6);
        err = DeferRenderNodeClose(*(int *)((char *)pState + 8));
        if (err != 0)
            FatalError(err, 0xff7);
        return 0x38;
    }
    {
        if ((short)row[0x21] <= gOverlayW && *(short *)((char *)row + 0x86) <= gOverlayH) {
            pState[0] = 1;
            pState[1] = 1;
            pState[2] = 1;
            pState[3] = 1;
            sel = (uint)row[0x17];
            *(uint *)((char *)pState + 16) = sel;

            err = AttachRenderChild(*(int **)((char *)pState + 8), sel, (int **)(void *)&local4);
            if (err != 0)
                FatalError(err, 0xffa);

            pState[13] = 0;
            /* The cast binds to the NUMERATOR: only the low half of row[0x282] is the value (the
             * high half is other data; pState[12] takes the same field as a short, the far limit,
             * 9900 in the purser's corridor). Dividing the full int and truncating gave 14065
             * instead of 412, so the near-plane cull in CalcActorRenderParams rejected everything
             * closer than 3516 and every foreground actor vanished. */
            pState[10] = (short)(*(short *)((char *)row + 0xa08) / *(short *)((char *)row + 0x9fa));
            pState[11] = *(short *)((char *)row + 0xa06);
            pState[12] = *(short *)((char *)row + 0xa08);     /* low half of row[0x282] */
            pState[14] = *(short *)((char *)row + 0xa0a);
            pState[15] = *(short *)((char *)row + 0x84);      /* row[0x21] */
            pState[16] = *(short *)((char *)row + 0x86);
            pState[17] = *(short *)((char *)row + 0x80);      /* row[0x20] */
            pState[18] = *(short *)((char *)row + 0x82);
            DF_TRACE("InitSubsystemA: row[0x21]=%d row+0x86=%d row[0x20]=%d "
                     "row+0x82=%d | limits a866=%d a868=%d | row[0x282]=%d "
                     "row+0x9fa=%d",
                     (int)(short)row[0x21], (int)*(short *)((char *)row + 0x86),
                     (int)(short)row[0x20], (int)*(short *)((char *)row + 0x82),
                     (int)gOverlayW, (int)gOverlayH,
                     (int)row[0x282], (int)*(short *)((char *)row + 0x9fa));
            CopyPascalString((byte *)(row + 0x1c), (byte *)((char *)pState + 0x3c));
            if (*pOpt1 != 0)
                CopyPascalString(pOpt1, (byte *)((char *)pState + 0x4c));
            else
                CopyPascalString((byte *)((char *)row + 0xa0e), (byte *)((char *)pState + 0x4c));
            if (*pOpt2 != 0)
                CopyPascalString(pOpt2, (byte *)((char *)pState + 0x5c));
            else
                CopyPascalString((byte *)((char *)row + 0xa1e), (byte *)((char *)pState + 0x5c));
            sel = (uint)row[0x18];
            *(uint *)((char *)pState + 0x74) = sel;
            *(int *)((char *)pState + 0x78) = row[0x19];

            err = AttachRenderChild(*(int **)((char *)pState + 8), sel,
                                    (int **)((char *)pState + 0x7c));
            if (err != 0)
                FatalError(err, 0xffb);

            sel = (uint)row[0x16];
            *(uint *)((char *)pState + 0x6c) = sel;
            err = AttachRenderChild(*(int **)((char *)pState + 8), sel,
                                    (int **)((char *)pState + 0x70));
            if (err != 0)
                FatalError(err, 0xffc);

            /* The handle AttachRenderChild just wrote at byte 0x70 (param_2 + 0x38 as shorts);
             * byte 0x6c is the SELECTOR passed in to that attach, not a handle: GetResDataPtr on
             * it gives a garbage pointer and loop count. */
            hist = (int *)GetResDataPtr(*(int **)((char *)pState + 0x70));
            i = 0;
            walk = hist + 2;
            DF_TRACE("InitSubsystemA childloop: handle=%08x hist=%p count=%d "
                     "raw[0..7]=%08x %08x %08x %08x %08x %08x %08x %08x",
                     *(unsigned *)((char *)pState + 0x70), (void *)hist, *hist,
                     hist[0], hist[1], hist[2], hist[3],
                     hist[4], hist[5], hist[6], hist[7]);
            if (0 < *hist) {
                do {
                    DF_TRACE("  childloop[%d/%d]: walk=%p (+%d) walk[7]=%d (0x%08x) "
                             "walk[6]=0x%08x walk[8]=0x%08x",
                             i, *hist, (void *)walk,
                             (int)((char *)walk - (char *)hist),
                             walk[7], (unsigned)walk[7],
                             (unsigned)walk[6], (unsigned)walk[8]);
                    if (walk[7] != 0) {
                        err = AttachRenderChild(*(int **)((char *)pState + 8), (unsigned)walk[7],
                                                (int **)(void *)&local4);
                        if (err != 0)
                            FatalError(err, 0xffd);
                        u = DetachRenderChild(*(int *)((char *)pState + 8), (unsigned)walk[7]);
                        if (u != 0)
                            FatalError(u, 0xffe);
                    }
                    walk = (int *)((char *)walk + 0x36);
                    i++;
                } while (i < *hist);
            }
            UnlockResObject(*(int **)((char *)pState + 0x70));
            UnlockResObject(*(int **)((char *)pState + 12));
            CommitSetViewport();
            return 0;
        }
        UnlockResObject(*(int **)((char *)pState + 12));
        u = DetachRenderChild(*(int *)((char *)pState + 8), 0u);
        if (u != 0)
            FatalError(u, 0xff8);
        err = DeferRenderNodeClose(*(int *)((char *)pState + 8));
        if (err != 0)
            FatalError(err, 0xff9);
        return 0x39;
    }
}


/* Script command closesetfile (opcode 12033), no arguments, guarded by gSet. Runs
 * SetPreShutdownStep; if the active context changed during it, returns early. Otherwise tears the
 * set down and refreshes the overlay. */
short __cdecl ActionCmd_CloseSetFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x00407AB0 */
{
    int   iVar;
    short status;

    (void)pCtx; (void)pArgBlock; (void)pTable;
    *pSubSkip = 0;

    if (gSet.active == 0)
        return 0x28;

    iVar = *(int *)((char *)DAT_0048a780 + 0x530);
    status = SetPreShutdownStep();
    if (status != 0)                    /* test ax,ax / jne: the pre-shutdown's error is returned */
        return status;

    if (gSet.active == 0)
        return 0;

    if (*(int *)((char *)DAT_0048a780 + 0x530) != iVar)
        return 0;

    CloseSetNode((short *)&gSet);
    BumpSceneOverlayModeFloor(1);
    ResetSetMoveState();
    return 0;
}


/* Detaches the set's render children (selectors 0, +0x74, +0x6c and +16), drains the display
 * attach slots for the context and defers closing the render node. */
void __cdecl CloseSetNode(short *pState) /* 0x00407b30 */
{
    short err;
    short u;

    if (*pState == 0)
        return;

    u = DetachRenderChild(*(int *)((char *)pState + 8), 0u);
    if (u != 0)
        FatalError(u, 0xfff);
    u = DetachRenderChild(*(int *)((char *)pState + 8), *(unsigned *)((char *)pState + 0x74));
    if (u != 0)
        FatalError(u, 0x1000);
    u = DetachRenderChild(*(int *)((char *)pState + 8), *(unsigned *)((char *)pState + 0x6c));
    if (u != 0)
        FatalError(u, 0x1001);
    u = DetachRenderChild(*(int *)((char *)pState + 8), *(unsigned *)((char *)pState + 16));
    if (u != 0)
        FatalError(u, 0x1002);

    DrainDisplayAttachSlotsForContext(*(int *)((char *)pState + 8));

    err = DeferRenderNodeClose(*(int *)((char *)pState + 8));
    if (err != 0)
        FatalError(err, 0x1003);
    *pState = 0;
}


/* Action half of currentscene (opcode 16029): switches the current scene by name. The query half
 * is ScriptCmd_CurrentScene. */
short __cdecl
ActionCmd_CurrentScene(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x00407C00 */
{
    short  u;
    short  mode;
    int    saved;
    short  typed[4];
    byte   row[42];       /* the 42-byte playfield row (the exe's frame is 0x134) */
    byte   local100[256];

    if (gSet.active == 0)
        return 0x28;

    CopyPascalString((byte *)&gViewName, local100);
    /* EvalSceneRowArg writes a 42-byte ROW here, not a bare name; the name is at +0x1a (the
     * field FindSceneRowByName matches on). Resolve every ESP displacement against the ESP at
     * that instruction (0x00407c00: row at E0-0x12C, name at E0-0x112 = row+0x1a); +0x12 gives a
     * bogus length byte and CopyPascalString overruns. Reading the name at +0 compared binary
     * fields against the scene name, so currentscene ("scene14") reported success without
     * switching and the next currentview ("view37") answered "No such object". */
    if ((short)EvalSceneRowArg(pCtx, pArgBlock, pTable, pSubSkip, row) != 0) {
        /* Not a playfield row: a mode keyword. The skip count goes straight
         * to the caller's pointer, as in the exe. */
        u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSubSkip);
        if (u != 0)
            return u;
        u = TakeScriptString(typed, (byte *)gScratchStr);
        if (u != 0)
            return u;
        mode = 0;
        if (PascalStringsEqualNoCase((byte *)gScratchStr, (byte *)UpdatePascalLength((char *)kStrStrait)) != 0)
            mode = 3;
        if (PascalStringsEqualNoCase((byte *)gScratchStr, (byte *)UpdatePascalLength((char *)kStrLeft)) != 0)
            mode = 1;
        if (PascalStringsEqualNoCase((byte *)gScratchStr, (byte *)UpdatePascalLength((char *)kStrRight)) != 0)
            mode = 2;
        if (mode == 0)
            return 10;
        if (mode == 3 && (int)GetStraightMoveForCurrentView() <= 0)
            return 0;
        saved = *(int *)((char *)DAT_0048a780 + 0x530);
        u = SendCloseSceneMessage();
        if (u != 0)
            return u;
        if (gSet.active == 0)
            return 0;
        if (*(int *)((char *)DAT_0048a780 + 0x530) != saved)
            return 0;
        StartSetMove(mode);
        return 0;
    }
    if (PascalStringsEqualNoCase(row + 0x1a, (byte *)&gSceneName) != 0)
        return 0;
    saved = *(int *)((char *)DAT_0048a780 + 0x530);
    u = SendCloseSceneMessage();
    if (u != 0)
        return u;
    if (gSet.active == 0)
        return 0;
    if (*(int *)((char *)DAT_0048a780 + 0x530) != saved)
        return 0;
    CopyPascalString(local100, (byte *)&gViewName);
    CopyPascalString(row + 0x1a, (byte *)&gSceneName);
    SelectViewByNameOrNearestAngle();
    return SendOpenSceneMessage();
}


/* Builds the opensceneCall script string around gSceneName in gScratchStr and runs it with
 * RunSceneMessage. Does nothing when gSet is not active. */
short __stdcall SendOpenSceneMessage(void) /* 0x00407e50 */
{
    if (gSet.active == 0)    /* the scene guard its twin has; the port lacked it (Ghidra 0x00407e50) */
        return 10;
    CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
    AppendPascalString(&gSceneName, &gScratchStr);
    AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteCommaOpensceneCall), &gScratchStr);
    return RunSceneMessage(&gScratchStr);
}


/* Builds the closesceneCall script string around gSceneName in gScratchStr and runs it with
 * RunSceneMessage. */
short __cdecl SendCloseSceneMessage(void) /* 0x00407ec0 */
{
    if (gSet.active == 0)
        return 10;
    CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
    AppendPascalString(&gSceneName, &gScratchStr);
    AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteCommaClosesceneCall), &gScratchStr);   /* the destination is pushed before the lookup call */
    return RunSceneMessage(&gScratchStr);
}


/* Runs the opensetCall script (RunSetMessage); if the set is still active and the context word at
 * +0x530 has not changed, sends the opensceneCall for the current scene through RunSceneMessage.
 * Returns 10 if no set is active. */
short __stdcall CompleteSetInit(void) /* 0x00407f30 */
{
    int i0;
    short u;

    if (gSet.active == 0)
        return 10u;

    i0 = *(int *)((char *)DAT_0048a780 + 0x530);
    u = RunSetMessage((byte *)UpdatePascalLength(kStrOpensetCall));
    if (u == 0) {
        if (gSet.active == 0)
            return 0;
        if (*(int *)((char *)DAT_0048a780 + 0x530) != i0)
            return 0;

        CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
        AppendPascalString(&gSceneName, &gScratchStr);
        AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteCommaOpensceneCall), &gScratchStr);
        u = RunSceneMessage(&gScratchStr);
    }
    return u;
}


/* Sends the closesceneCall for the current scene (RunSceneMessage) and, if the set is still
 * active and the context word at +0x530 has not changed, the closesetCall (RunSetMessage).
 * Returns 10 if no set is active. ActionCmd_CloseSetFile runs it before tearing the set down. */
short __stdcall SetPreShutdownStep(void) /* 0x00407fe0 */
{
    int i0;
    short u;

    if (gSet.active == 0)
        return 10u;

    i0 = *(int *)((char *)DAT_0048a780 + 0x530);
    CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
    AppendPascalString(&gSceneName, &gScratchStr);
    AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteCommaClosesceneCall), &gScratchStr);
    u = RunSceneMessage(&gScratchStr);
    if (u == 0) {
        if (gSet.active == 0)
            return 0;
        if (*(int *)((char *)DAT_0048a780 + 0x530) != i0)
            return 0;
        u = RunSetMessage((byte *)UpdatePascalLength(&kStrClosesetCall));
    }
    return u;
}


/* Dispatches a script message to the current scene: looks the scene node up by name through
 * WaitHeapThenResolveMethodPath, builds a synthetic invocation context with the scene's script
 * strings (kStrSceneMessage for slots 1 and 2, kStrSystemColon for slot 3) and invokes
 * ActionCmd_SendToScene with it. Called from TickWalkEntry for type 3/4/5 bindings. Returns 0, or
 * the first error. */
short __cdecl RunSceneMessage(byte *pDesc)  /* 0x00408090 */
{
    short           st;
    void           *pData;
    int            *hMethod;  /* local_5c */
    int             subSkip;  /* local_58 */
    int             resolveOut;  /* local_54 */
    /* Ghidra's local_50..local_12 are the fields of ONE MethodCallFrame at ebp-0x50 (+0 isLast,
     * +2 saved0x14c, +0x14 pData, +0x1e buf0, +0x2e buf1, +0x3e buf2), the shape
     * DispatchServerMessageString builds. As six separate locals the callee's frame walk read
     * whatever GCC put between them (class 3, stack adjacency). */
    MethodCallFrame frame;

    st = WaitHeapThenResolveMethodPath(pDesc + 1, (uint)*pDesc, &hMethod, &resolveOut);
    if (st != 0)
        return st;
    pData            = (void *)GetResDataPtr(hMethod);
    frame.pData      = pData;
    frame.isLast     = 1;
    frame.saved0x14c = 0xffffffffu;
    CopyPascalString((byte *)UpdatePascalLength(kStrSceneMessage), frame.buf0);
    CopyPascalString((byte *)UpdatePascalLength(kStrSceneMessage), frame.buf2);
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), frame.buf1);
    st = ActionCmd_SendToScene((int)(intptr_t)&frame, (int)(intptr_t)pData, 0, &subSkip);
    UnlockResObject(hMethod);
    FreeResObject(hMethod);
    return st;
}


/* Script command sendtoscene (opcode 12034): a thin wrapper over ScriptCmd_SendToSceneFx with a
 * NULL result pointer. */
short __cdecl ActionCmd_SendToScene(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00408170 */
{
    return ScriptCmd_SendToSceneFx(pCtx, pArgBlock, pTable, 0, pSubSkip);
}


/* Body of sendtoscene (opcodes 12034 / 20085), guarded by gSet: one typed argument and a 0xfb4
 * separator, parsed with FindSceneRowByName, then three method frames (scene, set, bootfile) run
 * through EvalMethodCall. */
short __cdecl ScriptCmd_SendToSceneFx(char *pCtx, int pArgBlock, int *pTable,  /* 0x00408190 */
                                      void *pResult, int *pSubSkip)
{
    int                start;           /* the parameter walks; this keeps its start (mov edi,esi) */
    short              sVar;
    int                skipCount;
    short              sceneNameDesc[4];  /* local_120 */
    /* MethodCallFrame[3], not three locals: the callee walks them as one array (class 3). No
     * zeroing: the exe sets every field it reads, in this order. Declared before the parsed
     * target (the frames sit below it on the stack), each filled through a pointer as in
     * ScriptCmd_SendToBootFx, which keeps each owner reload after the handle store. */
    MethodCallFrame    f[3];
    MethodCallFrame   *pf;
    ParsedMethodTarget parsedTarget;
    int               *h;

    if (gSet.active == 0)
        return 0x28;

    start  = pArgBlock;
    sVar = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, (int *)sceneNameDesc, &skipCount);
    if (sVar != 0)
        return sVar;

    pArgBlock += skipCount * 8;
    if (*(short *)pArgBlock != 0xfb4)
        return 0x1c;

    pArgBlock += 8;

    sVar = TakeScriptString(sceneNameDesc, gScratchStr);
    if (sVar != 0)
        return sVar;

    sVar = FindSceneRowByName(gScratchStr, parsedTarget.fields);
    if (sVar != 0)
        return sVar;

    pf = &f[0];
    pf->pOwner     = (void *)DAT_0048a780;                        /* local_108 */
    pf->selector   = parsedTarget.selector;                       /* local_110 */
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector); /* local_fe */
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];                  /* local_116 */
    pf->pData      = (void *)GetResDataPtr(h);                    /* local_104 */
    CopyPascalString((byte *)UpdatePascalLength(kStrSceneScriptColon), pf->buf1);     /* local_ea */
    CopyPascalString(parsedTarget.name, pf->buf0);                /* local_fa */
    CopyPascalString(parsedTarget.name, pf->buf2);                /* local_da */
    pf->isLast     = 0;

    pf = &f[1];
    pf->pOwner     = (void *)DAT_0048a780;                        /* local_ba */
    pf->selector   = DAT_0048a788;                                /* local_c2 */
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector); /* local_b0 */
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];                  /* local_c8 */
    pf->pData      = (void *)GetResDataPtr(h);                    /* local_b6 */
    CopyPascalString((byte *)UpdatePascalLength(kStrSetScriptColon), pf->buf1);     /* local_9c */
    CopyPascalString(&DAT_0048a7b4, pf->buf0);                    /* local_ac */
    CopyPascalString(parsedTarget.name, pf->buf2);                /* local_8c */
    pf->isLast     = 0;

    pf = &f[2];
    pf->pOwner     = (int *)gBootNode;                         /* local_6c */
    pf->selector   = 2u;                                          /* local_74 */
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, 2u);         /* local_62 */
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];                  /* local_7a */
    pf->pData      = (void *)GetResDataPtr(h);                    /* local_68 */
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), pf->buf1);     /* local_4e */
    CopyPascalString((byte *)UpdatePascalLength(kStrPostScript), pf->buf0);     /* local_5e */
    CopyPascalString(parsedTarget.name, pf->buf2);                /* local_3e */
    pf->isLast     = 1;

    sVar = EvalMethodCall((short *)f, (void *)pCtx, (short *)pArgBlock, pTable, pResult, &skipCount);

    UnlockResObject((int *)f[0].hMethod);
    ReleaseMethodHandle((int)f[0].hMethod);  /* 0x0042AF80 */
    UnlockResObject((int *)f[1].hMethod);
    ReleaseMethodHandle((int)f[1].hMethod);  /* 0x0042AF80 */
    UnlockResObject((int *)f[2].hMethod);
    ReleaseMethodHandle((int)f[2].hMethod);  /* 0x0042AF80 */

    pArgBlock += skipCount * 8;
    *pSubSkip = (pArgBlock - start) >> 3;
    return sVar;
}


/* Resolves the heap path from a Pascal descriptor, mirrors the script templates, runs
 * ActionCmd_SendToSet, then finalises and uninstalls the handle. */
short __cdecl RunSetMessage(byte *pDesc) /* 0x004084b0 */
{
    int            *ph;
    int             posErr;
    int             skipOut;
    MethodCallFrame frame;
    short           st;
    int            *pData;

    st = WaitHeapThenResolveMethodPath(pDesc + 1, (uint)*pDesc, &ph, &posErr);
    if (st != 0)
        return st;
    frame.pData      = pData = (int *)GetResDataPtr(ph);
    frame.isLast     = 1;
    frame.saved0x14c = 0xffffffffu;
    CopyPascalString((byte *)UpdatePascalLength(&kStrSetMessage), frame.buf0);
    CopyPascalString((byte *)UpdatePascalLength(&kStrSetMessage), frame.buf2);
    CopyPascalString((byte *)UpdatePascalLength(&kStrSystemColon), frame.buf1);
    st = (short)ActionCmd_SendToSet((int)(intptr_t)&frame, (short *)pData, NULL, &skipOut);
    UnlockResObject(ph);
    FreeResObject(ph);
    return st;
}


/* Script command sendtoset (opcode 12067): a thin wrapper over ScriptCmd_SendToSetFx. The exe
 * pushes five arguments and cleans 0x14: SendToSetFx(p1, p2, p3, NULL, p4), because the action
 * path has no result slot. */
short __cdecl ActionCmd_SendToSet(int pCtx, short *pArgBlock, int *pTable, int *pSkipOut)  /* 0x00408590 */
{
    return (uint)(unsigned short)ScriptCmd_SendToSetFx((void *)(intptr_t)pCtx, pArgBlock,
                                                       pTable, NULL, pSkipOut);
}


/* Body of sendtoset (opcode 12067), guarded by gSet (0x28 when no set is active). Builds two
 * method frames and calls EvalMethodCall: frame 1 is the set's context (DAT_0048a780, selector
 * DAT_0048a788, names kStrSetScriptColon / DAT_0048a7b4), frame 2 the bootfile (gBootNode,
 * selector 2, names kStrSystemColon / kStrPostScript / DAT_0048a7b4). The skip count goes to
 * *pSkipOut. */
undefined2 __cdecl ScriptCmd_SendToSetFx(void *pCtxB, short *pInstr, int *pTable,
                                         void *pResult, int *pSkipOut)  /* 0x004085b0 */
{
    int            start;               /* the parameter walks; this keeps its start */
    short          status;
    int            skipCount;            /* skip count output from EvalMethodCall */  /* local_a0 */
    int           *h1, *h2;
    /* MethodCallFrame[2]: as separate locals EvalMethodCall's chain walk landed 0x4e bytes off
     * (class 3, as in SendToSceneFx). */
    MethodCallFrame f[2];
    MethodCallFrame *pf;   /* frames filled through a pointer, as ScriptCmd_SendToBootFx */

    if (gSet.active == 0)
        return 0x28;

    start = (int)pInstr;

    /* frame 1: owner and selector come from the frame fields, the saved word is read back
     * through the owner (0x004085d6..0x00408602) */
    pf = &f[0];
    pf->pOwner     = (void *)DAT_0048a780;
    pf->selector   = DAT_0048a788;
    pf->hMethod    = h1 = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h1);

    CopyPascalString((byte *)UpdatePascalLength(&kStrSetScriptColon), pf->buf1);
    CopyPascalString(&DAT_0048a7b4, pf->buf0);
    CopyPascalString(&DAT_0048a7b4, pf->buf2);
    pf->isLast = 0;

    pf = &f[1];
    pf->pOwner     = (int *)gBootNode;
    pf->selector   = 2u;
    pf->hMethod    = h2 = AcquireMethodHandle((int *)pf->pOwner, 2u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h2);

    CopyPascalString((byte *)UpdatePascalLength(&kStrSystemColon), pf->buf1);   /* "System: " */
    CopyPascalString((byte *)UpdatePascalLength(&kStrPostScript), pf->buf0);   /* "Post Script" */
    CopyPascalString(&DAT_0048a7b4, pf->buf2);
    pf->isLast = 1;

    status = EvalMethodCall((short *)&f[0], pCtxB, pInstr, pTable, pResult, &skipCount);

    UnlockResObject(f[0].hMethod);
    ReleaseMethodHandle((int)f[0].hMethod);
    UnlockResObject(f[1].hMethod);
    ReleaseMethodHandle((int)f[1].hMethod);

    pInstr += skipCount * 4;
    *pSkipOut = ((int)pInstr - start) >> 3;
    return status;
}


/* Script command setscript (opcode 12035), no arguments, guarded by gSet. Builds a descriptor from
 * kStrSetScriptColon plus the active set's resource name (DAT_0048a7b4) and finalises it with
 * ClearScriptResult. */
short __cdecl ActionCmd_SetScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x00408770 */
{
    (void)pCtx; (void)pArgBlock; (void)pTable;
    *pSubSkip = 0;
    if (gSet.active == 0)
        return 0x28;

    CopyPascalString((byte *)UpdatePascalLength(kStrSetScriptColon), gScratchStr2);
    AppendPascalString(&DAT_0048a7b4, gScratchStr2);
    ClearScriptResult(DAT_0048a780, DAT_0048a788, gScratchStr2, 0);   /* 0x004087b6 */
    return 0;
}


/* Script command scenescript (opcode 12036), no arguments. Queries the current scene row into a
 * temporary buffer, prepends kStrSceneScriptColon to its name and finalises the descriptor with
 * ClearScriptResult. */
short __cdecl ActionCmd_SceneScript(int pCtx, short *pArgBlock, int *pTable,  /* 0x004087e0 */
                                   int *pSubSkip)
{
    short status;
    byte *pbVar;
    /* EvalSceneRowArg writes a 42-byte ROW; this was byte[18] and every call smashed 24 bytes
     * of the frame. The exe reserves 0x2c and passes [ESP] as the row. */
    byte rowOut[44];

    status = EvalSceneRowArg(pCtx, pArgBlock, pTable, pSubSkip, rowOut);
    if (status != 0)
        return status;

    CopyPascalString((byte *)UpdatePascalLength(kStrSceneScriptColon), gScratchStr2);   /* nested: the exe pushes the row before the call */
    /* the script-facing name is at row+0x1a, not row+0 (0x004087e0: LEA EAX,[ESP+0x22], row at
     * E0-0x2c) */
    AppendPascalString(rowOut + 0x1a, gScratchStr2);
    ClearScriptResult(DAT_0048a780, *(int *)((char *)rowOut + 0x16), gScratchStr2, 0);   /* 0x00408833 */
    return 0;
}


/* Action half of setvisible (opcode 16007), guarded by gSet: stores a bool argument in
 * DAT_0048a77a (the flag ScriptCmd_SetVisible reads back) and raises the overlay mode floor
 * (BumpSceneOverlayModeFloor). Returns 0x0e if the argument is not a bool. */
short __cdecl
ActionCmd_SetVisible(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip) /* 0x00408860 */
{
    short  u;
    short  typed[4];

    if (gSet.active == 0)
        return 0x28;
    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSubSkip);
    if (u != 0)
        return u;
    if (typed[0] != 2)
        return 0x0e;
    DAT_0048a77a = (short)*(int *)(typed + 1);
    BumpSceneOverlayModeFloor(1);
    return 0;
}


/* Action half of currentview (opcode 16011): switches the current scene to another of its views,
 * by name. The query half is ScriptCmd_CurrentView. */
short __cdecl
ActionCmd_CurrentView(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip) /* 0x004088D0 */
{
    short  typed[4];
    short   u;
    byte   aBuf[256];
    byte   bBuf[256];
    int    saved530;

    if (gSet.active == 0)
        return 0x28;
    /* the skip count goes straight to the caller's pointer, as in the exe */
    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(typed, aBuf);
    if (u != 0)
        return u;
    /* Skip commit when the new name already matches &gViewName (Compare returns 1 if equal). */
    if (PascalStringsEqualNoCase(aBuf, (byte *)&gViewName) != 0)
        return 0;
    if (SceneHasViewNamed((byte *)&gSceneName, aBuf) == 0)
        return 10;
    CopyPascalString((byte *)&gSceneName, bBuf);
    saved530 = *(int *)((char *)DAT_0048a780 + 0x530);
    u        = SendCloseSceneMessage();
    if (u != 0)
        return u;
    if (gSet.active == 0)
        return 0;
    if (*(int *)((char *)DAT_0048a780 + 0x530) != saved530)
        return 0;
    CopyPascalString(bBuf, (byte *)&gSceneName);
    CopyPascalString(aBuf, (byte *)&gViewName);
    SelectViewByNameOrNearestAngle();
    return SendOpenSceneMessage();
}


/* Action half of currentdeg (opcode 16031): sets the current angle (gCurrentDeg) from an int
 * argument. The query half is ScriptCmd_CurrentDeg. */
short __cdecl
ActionCmd_CurrentDeg(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip) /* 0x00408A20 */
{
    short   u;
    short  typed[4];
    int    i0;
    byte   local200[256];
    byte   local100[256];

    if (gSet.active == 0)
        return 0x28;
    /* the skip count goes straight to the caller's pointer, as in the exe */
    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSubSkip);
    if (u != 0)
        return u;
    if (typed[0] != 4)
        return 0x0e;
    u = (short)*(int *)(typed + 1) & 0xff;   /* the angle, formed before the buffer address */
    u = FindViewNameNearestAngle(u, local200);
    if (u != 0)
        return u;
    if (PascalStringsEqualNoCase(local200, (byte *)&gViewName) != 0)
        return 0;
    CopyPascalString((byte *)&gSceneName, local100);
    i0 = *(int *)((char *)DAT_0048a780 + 0x530);
    u = SendCloseSceneMessage();
    if (u != 0)
        return u;
    if (gSet.active == 0)
        return 0;
    if (*(int *)((char *)DAT_0048a780 + 0x530) != i0)
        return 0;
    CopyPascalString(local100, (byte *)&gSceneName);
    CopyPascalString(local200, (byte *)&gViewName);
    SelectViewByNameOrNearestAngle();
    return SendOpenSceneMessage();
}


/* Script command roadahead (opcode 20116): takes two descriptors, a scene and a view name, and
 * returns a bool: whether ResolveWalkPathSelectorForView finds a walk path for that view.
 * Error 0x0b if no set is active.
 *
 * Port note: five parameters, not four. The disassembly (0x00408b75) reads [ESP+0x54] and pushes
 * it as EvalTwoScriptArgs's skip-count out-pointer, so the consumed-entry count goes straight to
 * the dispatcher's subSkip; Ghidra's four-parameter prototype hides it. The port kept the count in
 * a dead local, the dispatcher saw subSkip = 0, looked for the 0xfb3 closer right after the opcode
 * and failed with error 2 "Expected paren" (class 2; the same shape recurs in the handlers
 * below). */
short __cdecl ScriptCmd_RoadAhead(int *pCtx, int pArgBlock, int *pTable,
                                 short *pResult, int *pSkipCount) /* 0x00408B60 */
{
    short evalStatus;
    short  status;
    uint  animIdx;
    uint  rowScratch[12];
    short argA[4];
    short argB[4];

    if (gSet.active == 0)
        return 0xBu;

    evalStatus = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)argA, (int *)argB, pSkipCount);
    status = (uint)(ushort)evalStatus;
    if (evalStatus != 0)
        return status;

    status = TakeScriptString(argA, gScratchStr);
    if (status != 0)
        return status;

    status = TakeScriptString(argB, gScratchStr2);
    if (status != 0)
        return status;

    status = LookupViewRow(gScratchStr, gScratchStr2, rowScratch);
    if (status != 0)
        return status;

    animIdx = ResolveWalkPathSelectorForView(gScratchStr, gScratchStr2);
    if ((int)animIdx <= 0)
        *(int *)(pResult + 1) = 0;
    else
        *(int *)(pResult + 1) = 1;

    pResult[3] = 0;
    *pResult   = 2;
    return 0;
}


/* Query half of setvisible (opcode 16007): returns DAT_0048a77a as a type-2 (short) result when
 * gSet is active, otherwise 0. */
short ScriptCmd_SetVisible(int *pCtx, short *pArgBlock, int *pTable,
                            short *pResult, int *pSkipCount) /* 0x00408C30 */
{
    *pSkipCount = 0;
    if (gSet.active == 0) {
        *(int *)(pResult + 1) = 0;
    } else {
        *(int *)(pResult + 1) = (int)DAT_0048a77a;
    }
    pResult[3] = 0;
    *pResult = 2;   /* type: short */
    return 0;
}


/* Query half of currentview (opcode 16011): returns a string for the current view. Without an
 * active set it is the kStrNone fallback; with GetActiveSetMove() >= 0 it is kStrMoving;
 * otherwise it is the view name gViewName. Returns AllocScriptString's status (each path ends in
 * that call with no xor ax,ax; DispatchScriptCommand returns that ax), where the port was void and
 * the dispatcher forced status 0. */
short ScriptCmd_CurrentView(int *pCtx, short *pArgBlock, int *pTable,
                            short *pResult, int *pSkipCount) /* 0x00408C70 */
{
    int iVar;

    *pSkipCount = 0;
    if (gSet.active == 0)
        return AllocScriptString((byte *)UpdatePascalLength(&kStrNone), pResult);   /* the lookup written inline */
    iVar = GetActiveSetMove();
    if (iVar >= 0)
        return AllocScriptString((byte *)UpdatePascalLength(&kStrMoving), pResult);
    return AllocScriptString(&gViewName, pResult);
}


/* Query half of currentdeg (opcode 16031): returns gCurrentDeg as a type-4 (int) result when a set
 * is active, otherwise -1. */
short ScriptCmd_CurrentDeg(int *pCtx, short *pArgBlock, int *pTable,
                           short *pResult, int *pSkipCount) /* 0x00408CE0 */
{
    *pSkipCount = 0;
    if (gSet.active == 0) {
        *(int *)(pResult + 1) = -1;
    } else {
        *(int *)(pResult + 1) = (int)gCurrentDeg;
    }
    pResult[3] = 0;
    *pResult = 4;   /* type: int */
    /* The status is a short: the exe ends with `xor ax,ax` (0x00408d1a), i.e. `return 0` from a
     * short-returning function, not a mask of a wider value (Ghidra's `& 0xffff0000`). */
    return 0;
}


/* Script command currentset (opcode 20053): returns the name of the open set file (leaf of the
 * path), or "None" when no set is open. */
short __cdecl ScriptCmd_CurrentSet(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x00408D20 */
{
    uint uVar;

    *pSkipCount = 0;                    /* the five-argument handler: skip count cleared first */

    if (gSet.active != 0) {
        uVar = AllocScriptString(&DAT_0048a7b4, pResult);
        CopyPascalString((byte *)((char *)DAT_0048a780 + 0x24), gScriptResult);
        TruncatePathToLeaf(gScriptResult);
        return uVar;
    }
    return AllocScriptString((byte *)UpdatePascalLength(&kStrNone), pResult);
}


/* Script command pointinset (opcode 20041): takes a point and returns a bool: whether it lies in
 * the scene rectangle (PointInSceneRect against the trigger overlay's viewport pair) while the set
 * is visible (DAT_0048a77a). Returns 0x0e if the argument is not a point (type 4). */
short __cdecl ScriptCmd_PointInSet(int *pCtx, short *pArgBlock, int *pTable,
                                                     short *pResult, int *pSkipCount) /* 0x00408D90 */
{
    short u;
    short typed[4];
    short rect[4];       /* the viewport pair: 8 bytes (0x00408e04) */
    int   work[30];      /* the 120-byte trigger buffer */

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSkipCount);
    if (u != 0)
        return u;
    if (typed[0] != 4)
        return 0x0e;

    pResult[3]            = 0;
    *(int *)(pResult + 1) = 0;
    *pResult              = 2;
    if (gSet.active != 0 && DAT_0048a77a != 0) {
        CopyTrigOverlayBufferAndViewportPair((short *)work, (int *)rect);
        *(int *)(pResult + 1) = (short)PointInSceneRect(*(ScenePointYX *)(typed + 1), rect);
        return 0;
    }
    return 0;
}


/* Query half of currentscene (opcode 16029): returns the current scene's name (&gSceneName) when
 * a set is active, otherwise the kStrNone fallback. Returns AllocScriptString's status, as
 * ScriptCmd_CurrentView. */
short ScriptCmd_CurrentScene(int *pCtx, short *pArgBlock, int *pTable,
                             short *pResult, int *pSkipCount) /* 0x00408E50 */
{
    *pSkipCount = 0;
    if (gSet.active == 0)
        return AllocScriptString((byte *)UpdatePascalLength(&kStrNone), pResult);   /* the lookup written inline: the other arguments are pushed before it is called */
    return AllocScriptString(&gSceneName, pResult);
}


/* Script command countscenes (opcode 20019): returns gSceneCount as a type-4 int when a set is
 * active. */
short __cdecl ScriptCmd_CountScenes(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount)  /* 0x00408e90 */
{
    int count;

    *pSkipCount = 0;                    /* stored before the scene check */
    if (gSet.active == 0)
        return 0x28;
    count = gSceneCount;               /* read before the stores, as the exe does */
    *pResult = 4;   /* type: int */
    *(int *)(pResult + 1) = count;
    pResult[3] = 0;
    return 0;
}


/* Script command indextoscene (opcode 20020): one int argument, a 1-based row index. Copies the
 * 0x2a-byte row from the scene table (gSceneTable) and returns its name through AllocScriptString. */
short __cdecl ScriptCmd_IndexToScene(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00408ed0 */
                                                   short *pResult, int *pSkipCount)
{
    typedef struct { byte b[0x2a]; } SceneRow;   /* copied whole: rep movsd 10 + movsw */
    short        status;
    short       argBuf[4];
    SceneRow    rowBuf;

    if (gSet.active == 0)
        return CONCAT22(0, 0x28);

    status = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)argBuf, pSkipCount);
    if (status != 0)
        return status;

    if (argBuf[0] != 4)
        return CONCAT22((short)(status >> 16), 0x0E);

    /* the index is read from the argument block at each use */
    if (*(int *)(argBuf + 1) < 1 || gSceneCount < *(int *)(argBuf + 1))
        return CONCAT22((short)(status >> 16), 10);

    rowBuf = *(SceneRow *)((byte *)((SceneRow *)GetResDataPtr(gSceneTable) + *(int *)(argBuf + 1)) - sizeof(SceneRow));
    UnlockResObject(gSceneTable);

    /* the scene name is the Pascal string at row +0x1a (Ghidra's local_12
     * inside local_2c; the exe pushes lea [esp+0x2e]), not the row start */
    return AllocScriptString(rowBuf.b + 0x1a, pResult);
}


/* Script command countpaintings (opcode 20018): two descriptors (scene and view) are resolved
 * with LookupViewRow, and the result is the first dword of the child resource data attached for
 * that view, as a type-4 int. */
short __cdecl ScriptCmd_CountPaintings(int *pCtx, int pArgBlock, int *pTable,  /* 0x00408f90 */
                                                      short *pResult, int *pSkipCount)
{
    typedef struct { short w[23]; } PlayfieldRow46;
    short  st;
    int   *hChild;
    short  argA[4];  /* local_40 */
    short  argB[4];  /* local_38 */
    PlayfieldRow46 row;

    if (gSet.active == 0)
        return 0x0B;

    st = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)argA, (int *)argB, pSkipCount);
    if (st != 0)
        return st;
    st = TakeScriptString(argA, gScratchStr);
    if (st != 0)
        return st;
    st = TakeScriptString(argB, gScratchStr2);
    if (st != 0)
        return st;
    st = LookupViewRow(gScratchStr, gScratchStr2, &row);
    if (st != 0)
        return st;

    *(int *)(pResult + 1) = 0;
    /* The child selector is the unaligned uint at BYTE +26 (Ghidra local_16), not uint index 6
     * (byte 24): reading two bytes short gave child 0x000D0000 and fatal 0x1004 (class 3). */
    if (*(unsigned *)((byte *)&row + 26) != 0u) {
        st = AttachRenderChild(DAT_0048a780, *(unsigned *)((byte *)&row + 26), (int **)&hChild);
        if (st != 0)
            FatalError(st, 0x1004);
        *(int *)(pResult + 1) = *(int *)GetResDataPtr(hChild);
        UnlockResObject(hChild);
        st = DetachRenderChild((int)(intptr_t)DAT_0048a780, *(unsigned *)((byte *)&row + 26));
        if (st != 0)
            FatalError(st, 0x1005);
    }

    pResult[3] = 0;
    *pResult = 4;
    return 0;
}


/* Script command indextopainting (opcode 20022): two descriptors plus a type-4 index.
 * LookupViewRow finds the view; the row at the index in its counted table is copied (nine ints)
 * and its name returned through AllocScriptString. */
short __cdecl ScriptCmd_IndexToPainting(int *pCtx, int pArgBlock, int *pTable,  /* 0x004090d0 */
                                                    short *pResult, int *pSkipCount)
{
    short  evalStatus;
    short  status;
    short  detachStatus;
    int   *piRow;
    int   *piBase;
    int   *hChild;
    short  argA[4];  /* local_64 */
    short  argB[4];  /* local_5c */
    short  indexArg[4];  /* local_54 */
    int    row46[12];
    int    rowInts[9];
    int    idx;

    if (gSet.active == 0)
        return 0x0B;

    evalStatus = EvalThreeScriptArgs((int)pCtx, pArgBlock, pTable,
                          (int *)argA, (int *)argB, (int *)indexArg, pSkipCount);
    if (evalStatus != 0)
        return evalStatus;
    status = TakeScriptString(argA, gScratchStr);
    if (status != 0)
        return status;
    status = TakeScriptString(argB, gScratchStr2);
    if (status != 0)
        return status;
    if (indexArg[0] != 4)
        return 0x0E;
    status = LookupViewRow(gScratchStr, gScratchStr2, row46);
    if (status != 0)
        return status;
    if (*(uint *)((byte *)row46 + 26) == 0)
        return 10;
    evalStatus = AttachRenderChild(DAT_0048a780, *(uint *)((byte *)row46 + 26), (int **)&hChild);
    if (evalStatus != 0)
        FatalError(evalStatus, 0x1006);
    piBase = (int *)GetResDataPtr(hChild);
    idx = *(int *)(indexArg + 1);
    if (idx > 0 && idx <= *piBase) {
        piRow = piBase + idx * 9 - 7;
        {
            typedef struct { int w[9]; } Row9;
            *(Row9 *)rowInts = *(Row9 *)piRow;
        }
        UnlockResObject(hChild);
        detachStatus = DetachRenderChild((int)(intptr_t)DAT_0048a780, *(uint *)((byte *)row46 + 26));
        if (detachStatus != 0)
            FatalError(detachStatus, 0x1008);
        return AllocScriptString((byte *)rowInts + 20, pResult);
    }
    UnlockResObject(hChild);
    detachStatus = DetachRenderChild((int)(intptr_t)DAT_0048a780, *(uint *)((byte *)row46 + 26));
    if (detachStatus != 0)
        FatalError(detachStatus, 0x1007);
    return 10;
}


/* Script command pointinpainting (opcode 20017), guarded by gSet (error 0x0b without one). Four
 * arguments through EvalFourScriptArgs: three descriptors (scene, view, painting) and a point.
 * LookupViewRow and LookupNamedRowInChild resolve the painting, and PointInSceneRect tests the
 * point against its rectangle. The result is a type-2 bool; error 0x0e on a type mismatch.
 * Five parameters (class 2, see ScriptCmd_RoadAhead): [ESP+0x14] is the Eval helper's skip-count
 * out-pointer. */
short ScriptCmd_PointInPainting(int *pCtx, int pArgBlock, int *pTable,  /* 0x00409290 */
                               short *pResult, int *pSkipCount)
{
    typedef struct { short w[23]; } PlayfieldRow46;
    short status;
    /* EvalFourScriptArgs fills four 8-byte typed values: three descriptors,
     * then the type-4 point LAST (0x004092b4..0x004092e0) */
    short argResult[4];
    short argResult2[4];
    short argResult3[4];
    short argResult4[4];
    /* the 36-byte record LookupNamedRowInChild (0x0040add0)
     * fills; its rectangle is at +8 (0x00409392 LEA EAX,[ESP+0x2c]) */
    int   soundBuf[9];
    PlayfieldRow46 resolveRow;
    byte  descBuf[256];

    if (gSet.active == 0)
        return 0x0b;

    status = EvalFourScriptArgs(pCtx, pArgBlock, pTable, (int *)argResult2, argResult3, argResult4, argResult, pSkipCount);
    if (status != 0)
        return status;
    status = TakeScriptString(argResult2, &gScratchStr);
    if (status != 0)
        return status;
    status = TakeScriptString(argResult3, &gScratchStr2);
    if (status != 0)
        return status;
    status = TakeScriptString(argResult4, descBuf);
    if (status != 0)
        return status;
    if (argResult[0] != 4)
        return 0x0e;
    status = LookupViewRow(&gScratchStr, &gScratchStr2, &resolveRow);
    if (status != 0)
        return status;
    /* the row's child selector: the unaligned uint at BYTE +26, as in ScriptCmd_CountPaintings */
    status = LookupNamedRowInChild(*(uint *)((byte *)&resolveRow + 26), descBuf, soundBuf);
    if (status != 0)
        return status;

    *pResult = 2;   /* type: short */
    pResult[3] = 0;
    /* the rectangle tested is the record's own, at soundBuf + 8: the port had
     * passed a separate, never-written 28-byte local (class 3) */
    *(int *)(pResult + 1) = (short)PointInSceneRect(*(ScenePointYX *)(argResult + 1), (short *)((byte *)soundBuf + 8));
    return 0;
}


/* Script command paintingscript (opcode 12037), guarded by gSet. Three descriptor arguments
 * through EvalThreeScriptArgs; the first two are resolved with LookupViewRow /
 * LookupNamedRowInChild, kStrPaintScriptColon is prepended to the resolved name and the
 * descriptor is finalised with ClearScriptResult.
 * Four parameters (class 2, see ScriptCmd_RoadAhead): [ESP+0x10] is the Eval helper's skip-count
 * out-pointer and pTable is pushed through. */
short __cdecl ActionCmd_PaintingScript(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x004093c0 */
{
    short              sVar;
    short               status;
    
    short              argA[4];  /* local_16c */
    short              argB[4];  /* local_164 */
    short              argC[4];  /* local_15c */
    ResolvedBufferInfo namedRow;  /* local_154 */
    ResolvedSoundInfo  viewRow;  /* local_130 */
    byte               paintingName[256];  /* local_100 */

    if (gSet.active == 0)
        return 0x0b;

    sVar = EvalThreeScriptArgs(pCtx, pArgBlock, pTable,
                               (int *)argA, (int *)argB, (int *)argC, pSubSkip);
    status = (unsigned short)sVar;
    if (sVar != 0)
        return status;

    status = TakeScriptString(argA, gScratchStr);
    if (status != 0)
        return status;

    status = TakeScriptString(argB, gScratchStr2);
    if (status != 0)
        return status;

    status = TakeScriptString(argC, paintingName);
    if (status != 0)
        return status;

    status = LookupViewRow(gScratchStr, gScratchStr2, &viewRow.fields[0]);
    if (status != 0)
        return status;

    status = LookupNamedRowInChild(viewRow.value, paintingName, namedRow.fields);
    if (status != 0)
        return status;

    CopyPascalString((byte *)UpdatePascalLength(kStrPaintScriptColon), gScratchStr2);
    AppendPascalString(namedRow.name, gScratchStr2);
    ClearScriptResult(DAT_0048a780, *(int *)((char *)namedRow.name - 4), gScratchStr2, 0);   /* 0x004094ce: frame +0x28, the dword before the name */
    return 0;
}


/* Script command sendtopainting (opcode 12066): a thin wrapper over ScriptCmd_SendToPaintingFx
 * with a NULL result pointer. */
short __cdecl ActionCmd_SendToPainting(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00409500 */
{
    return ScriptCmd_SendToPaintingFx((int *)pCtx, pArgBlock, pTable, 0, pSubSkip);
}


/* Body of sendtopainting (opcodes 12066 / 20090). Three descriptors through EvalThreeScriptArgs
 * (0x00428fe0): the first is resolved with FindSceneRowByName (name and selector), the other two
 * with LookupViewRow and LookupNamedRowInChild (a selector and a 16-byte script-name buffer). It
 * then runs four method frames through EvalMethodCall:
 *   1. the set context with the painting's selector, prefix kStrPaintScriptColon
 *   2. the set context with the parsed target's selector, prefix kStrSceneScriptColon
 *   3. the set context with the fixed selector DAT_0048a788, prefix kStrSetScriptColon
 *   4. the bootfile (gBootNode, selector 2), prefix kStrSystemColon: the terminating frame */
short __cdecl ScriptCmd_SendToPaintingFx(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSubSkip)  /* 0x00409520 */
{
    short             sVar;
    int               skipC;           /* skip count -- EvalThreeScriptArgs, then EvalMethodCall */
    short             a1[4];           /* arg1 result */
    short             a2[4];           /* arg2 result */
    short             a3[4];           /* arg3 result */
    ResolvedBufferInfo bufInfo;        /* LookupNamedRowInChild output: fields[4]=selector, name=script name */
    /* MUST be MethodCallFrame[4]: the frames are contiguous in the original (isLast at stride
     * 0x4e = sizeof(MethodCallFrame), verified against 0x00446030 / 0x00409520) and
     * EvalMethodCall walks +0x4e from &f[0]; as separate locals they were wherever GCC put them
     * (class 3). Each frame is filled through a pointer, as in ScriptCmd_SendToBootFx. */
    MethodCallFrame    f[4];
    MethodCallFrame   *pf;
    ParsedMethodTarget parsedTarget;   /* FindSceneRowByName output */
    ResolvedSoundInfo  soundInfo;      /* LookupViewRow output */
    byte              paintingName[256];  /* third descriptor string buffer */  /* local_100 */
    int               *h;
    short            *psVar;           /* the argument block's start; pArgBlock walks it   */

    if (gSet.active == 0)
        return 0x0b;

    psVar = (short *)pArgBlock;
    sVar = EvalThreeScriptArgs((int)pCtx, pArgBlock, pTable,
                               (int *)a1, (int *)a2, (int *)a3, &skipC);
    if (sVar != 0)
        return sVar;

    pArgBlock += skipC * 8;
    if (*(short *)pArgBlock != 0xfb4)
        return 0x1c;
    pArgBlock += 8;

    sVar = TakeScriptString(a1, gScratchStr);
    if (sVar != 0) return sVar;

    sVar = TakeScriptString(a2, gScratchStr2);
    if (sVar != 0) return sVar;

    sVar = TakeScriptString(a3, (byte *)paintingName);
    if (sVar != 0) return sVar;

    sVar = FindSceneRowByName(gScratchStr, parsedTarget.fields);
    if (sVar != 0) return sVar;

    sVar = LookupViewRow(gScratchStr, gScratchStr2, &soundInfo.fields[0]);
    if (sVar != 0) return sVar;

    sVar = LookupNamedRowInChild(soundInfo.value, (byte *)paintingName, bufInfo.fields);
    if (sVar != 0) return sVar;

    /* Frame 1: subsystem-A + resolved buffer selector */
    pf = &f[0];
    pf->pOwner     = (void *)DAT_0048a780;
    pf->selector   = (unsigned int)bufInfo.fields[4];
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(kStrPaintScriptColon), pf->buf1);
    CopyPascalString(bufInfo.name, pf->buf0);
    CopyPascalString(bufInfo.name, pf->buf2);
    pf->isLast     = 0;

    /* Frame 2: subsystem-A + parsed-target selector */
    pf = &f[1];
    pf->pOwner     = (void *)DAT_0048a780;
    pf->selector   = parsedTarget.selector;
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(kStrSceneScriptColon), pf->buf1);
    CopyPascalString(parsedTarget.name, pf->buf0);
    CopyPascalString(bufInfo.name, pf->buf2);
    pf->isLast     = 0;

    /* Frame 3: subsystem-A + fixed DAT_0048a788 selector */
    pf = &f[2];
    pf->pOwner     = (void *)DAT_0048a780;
    pf->selector   = DAT_0048a788;
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(kStrSetScriptColon), pf->buf1);
    CopyPascalString(&DAT_0048a7b4, pf->buf0);
    CopyPascalString(bufInfo.name, pf->buf2);
    pf->isLast     = 0;

    /* Frame 4: bootfile node, selector 2 (terminating frame) */
    pf = &f[3];
    pf->pOwner     = (int *)gBootNode;
    pf->selector   = 2u;
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, 2u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), pf->buf1);
    CopyPascalString((byte *)UpdatePascalLength(kStrPostScript), pf->buf0);
    CopyPascalString(bufInfo.name, pf->buf2);
    pf->isLast     = 1;

    sVar = EvalMethodCall((short *)f, (void *)pCtx, (short *)pArgBlock,
                          pTable, pResult, &skipC);

    UnlockResObject((int *)f[0].hMethod);
    ReleaseMethodHandle((int)f[0].hMethod);
    UnlockResObject((int *)f[1].hMethod);
    ReleaseMethodHandle((int)f[1].hMethod);
    UnlockResObject((int *)f[2].hMethod);
    ReleaseMethodHandle((int)f[2].hMethod);
    UnlockResObject((int *)f[3].hMethod);
    ReleaseMethodHandle((int)f[3].hMethod);

    pArgBlock += skipC * 8;
    *pSubSkip = (pArgBlock - (int)psVar) >> 3;
    return sVar;
}


/* Hit test for the current view: when the set is stationary (GetActiveSetMove() < 0), looks up the
 * current scene and view row, attaches its painting child and tests packedPt against each
 * painting's rectangle (PointInSceneRect). On a hit the painting's name is copied to pDescOut.
 * Returns 1 on a hit, 0 otherwise. */
ushort __cdecl FindPaintingAtPoint(int packedPt, byte *pDescOut) /* 0x004099A0 */
{
    typedef struct { short w[23]; } PlayfieldRow46;
    int    *hChild;
    int    *pRow;
    int     rowCount;
    short   u;
    PlayfieldRow46 row;
    ushort  hit;
    int     i;
    int    *piBase;

    hit = 0;
    if (GetActiveSetMove() >= 0)
        return 0;
    u = LookupViewRow(&gSceneName, &gViewName, &row);
    if (u != 0)
        FatalError(u, 0x1009);
    /* the child selector: the unaligned uint at BYTE +26 of the 46-byte row, as in
     * ScriptCmd_CountPaintings */
    if (*(unsigned *)((byte *)&row + 26) == 0)
        return 0;
    u = AttachRenderChild(DAT_0048a780, *(unsigned *)((byte *)&row + 26), &hChild);
    if (u != 0)
        FatalError(u, 0x100a);

    piBase   = (int *)GetResDataPtr(hChild);
    rowCount = *piBase;
    pRow     = piBase + rowCount * 9 - 7;
    for (i = 0; i < rowCount; i++) {
        if (PointInSceneRect(*(ScenePointYX *)&packedPt, (short *)(pRow + 2))) {
            hit = 1;
            CopyPascalString((byte *)(pRow + 5), pDescOut);
            break;
        }
        pRow -= 9;
    }
    UnlockResObject(hChild);
    u = DetachRenderChild((int)(intptr_t)DAT_0048a780, *(unsigned *)((byte *)&row + 26));
    if (u != 0)
        FatalError(u, 0x100b);
    return hit;
}


/* Script command countviews (opcode 20118): resolves a scene row, attaches its render child and
 * returns the view count at data offset 0x30. */
short __cdecl ScriptCmd_CountViews(int pCtx, short *pArgBlock, int *pTable,
                                  short *pResult, int *pSkipCount) /* 0x00409AC0 */
{
    short  st;
    int   *hChild;
    uint   viewCount;
    /* 44-byte frame slot; FindSceneRowByName writes 42.
     * The row's child id is the unaligned uint at +10 (ground truth), not +8. */
    byte   rowOut[44];

    if (gSet.active == 0)
        return 0xB;

    st = EvalSceneRowArg(pCtx, pArgBlock, pTable, pSkipCount, rowOut);
    if (st != 0)
        return st;

    st = AttachRenderChild(DAT_0048a780, *(uint *)(rowOut + 10), &hChild);
    if (st != 0)
        FatalError(st, 0x100c);
    viewCount = *(uint *)(GetResDataPtr(hChild) + 0x30);
    UnlockResObject(hChild);
    st = DetachRenderChild((int)(intptr_t)DAT_0048a780, *(uint *)(rowOut + 10));
    if (st != 0)
        FatalError(st, 0x100d);

    *pResult               = 4;
    *(uint *)(pResult + 1) = viewCount;
    pResult[3]             = 0;
    return 0;
}


/* Script command indextoview (opcode 20119): returns the name of view number index (1-based) of
 * the scene named by the first argument, through AllocScriptString. Error 10 if the index is out
 * of range, 0x0e if it is not an int. Five parameters (class 2, see ScriptCmd_RoadAhead): the
 * first instruction is MOV EAX,[ESP+0x14], pushed as the Eval helper's skip-count out-pointer. */
short __cdecl ScriptCmd_IndexToView(int *pCtx, int pArgBlock,
                                                                                 int *pTable, short *pResult, int *pSkipCount) /* 0x00409B90 */
{
    typedef struct { short w[21]; } PlayfieldRow42;
    typedef struct { short w[23]; } ViewRow46;
    short  st;
    int   *hChild;
    short  argA[4];
    short  argB[4];
    PlayfieldRow42 parsed;
    int    base;
    ViewRow46 row;

    if (gSet.active == 0)
        return 0xB;

    st = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)argA, (int *)argB, pSkipCount);
    if (st != 0)
        return st;
    st = TakeScriptString(argA, gScratchStr);
    if (st != 0)
        return st;
    st = FindSceneRowByName(gScratchStr, &parsed);
    if (st != 0)
        return st;
    if (argB[0] != 4)
        return 0xE;

    /* the child selector is the unaligned uint at BYTE +10 of the parsed row (Ghidra local_52) */
    st = AttachRenderChild(DAT_0048a780, *(uint *)((byte *)&parsed + 10), &hChild);
    if (st != 0)
        FatalError(st, 0x100e);
    base = GetResDataPtr(hChild);
    if (*(int *)(argB + 1) > 0 && *(int *)(base + 0x30) >= *(int *)(argB + 1)) {
        row = *(ViewRow46 *)(base + 6 + *(int *)(argB + 1) * 0x2e);
        UnlockResObject(hChild);
        st = DetachRenderChild((int)(intptr_t)DAT_0048a780, *(uint *)((byte *)&parsed + 10));
        if (st != 0)
            FatalError(st, 0x1010);
        /* the name handed on is the copied row's +0x1e field (local_12) */
        return AllocScriptString((byte *)&row + 30, pResult);
    }
    UnlockResObject(hChild);
    st = DetachRenderChild((int)(intptr_t)DAT_0048a780, *(uint *)((byte *)&parsed + 10));
    if (st != 0)
        FatalError(st, 0x100f);
    return 10;
}


/* Action half of setparam (opcode 16055), guarded by gSet: two arguments through EvalTwoScriptArgs,
 * sel and value. Stores value as a short in DAT_0048a77c (sel 1) or DAT_0048a77e (sel 2); sel 3 is
 * read-only here (error 0x0e). Four parameters (class 2, see ScriptCmd_RoadAhead): [ESP+0x20] is
 * the skip-count out-pointer and param 3 is pushed through. */
short __cdecl
ActionCmd_SetParam(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00409d20 */
{
    short s1;
    short o1[4], o2[4];

    if (gSet.active == 0)
        return 0xb;
    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)o1, (int *)o2, pSubSkip);
    if (s1 != 0)
        return s1;
    if (o1[0] != 4)
        return 0x0E;
    if (o2[0] != 4)
        return 0x0E;
    switch (*(int *)(o1 + 1)) {
    case 1:
        DAT_0048a77c = (short)*(int *)(o2 + 1);
        return 0;
    case 2:
        DAT_0048a77e = (short)*(int *)(o2 + 1);
        return 0;
    case 3:
        return 0x0E;   /* sel == 3 is read-only here (0x00409d91) */
    }
    return 0x0E;
}


/* Query half of setparam (opcode 16055), guarded by gSet: one int argument selects 1 = DAT_0048a77c
 * (short), 2 = DAT_0048a77e (short), 3 = the GetActiveSetMove() result; returned as a type-4 int.
 * Error 0x0b if no set is active, 0x0e on a type mismatch or an index out of range. */
short ScriptCmd_SetParam(int *pCtx, short *pArgBlock, int *pTable,
                              short *pResult, int *pSkipCount) /* 0x00409DD0 */
{
    short  status;
    int   iVar;
    /* MSVC stack adjacency: original is `short local_8; int local_6;` filled as
     * one 8-byte typed value by ProcessScriptBlock (port-bug-classes.md #3). */
    short argResult[4];   /* [0]=type, [1..2]=value */
    int   argValue;

    if (gSet.active == 0)
        return 0x0b;

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    if (argResult[0] != 4)
        return 0x0e;

    argValue = *(int *)(argResult + 1);
    switch (argValue) {
    case 1: iVar = (int)DAT_0048a77c; break;
    case 2: iVar = (int)DAT_0048a77e; break;
    case 3: iVar = GetActiveSetMove(); break;
    default: return 0x0e;
    }

    *pResult = 4;   /* type: int */
    *(int *)(pResult + 1) = iVar;
    pResult[3] = 0;
    return 0;
}


/* Evaluates one script argument, converts it to a descriptor and resolves that to a scene row,
 * written through pRowOut (42 bytes). Returns 0x28 if gSet is off.
 *
 * Port note: it takes five real __cdecl parameters (checked against the disassembly of 0x00409e70
 * and its three callers, each of which pushes five arguments and cleans 0x14). Ghidra renders it
 * as __stdcall(void) reading in_stack_00000004..00000014 (class 2), and the port had transcribed
 * that literally with a bogus single pOut parameter, so every caller's one argument landed in the
 * pCtx slot and the other four were read from uninitialised stack. */
short __cdecl EvalSceneRowArg(int pCtx, short *pArgBlock, int *pTable,
                                             int *pSkipCount, void *pRowOut)  /* 0x00409E70 */
{
  short status;
  short nameDesc [4];  /* local_8 */

  if (gSet.active == 0) {
    return 0x28;
  }
  status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (uint32_t *)nameDesc, pSkipCount);
  if (status == 0) {
    status = TakeScriptString(nameDesc, &gScratchStr);
    if (status == 0) {
      status = FindSceneRowByName(&gScratchStr, pRowOut);
    }
  }
  return status;
}


/* When a set is active, locks the scene row table (gSceneTable), finds the row whose 26-byte name
 * at +0x1a matches pDesc, copies its 42 bytes (10 dwords and a word) to pOut and unlocks. */
short __cdecl FindSceneRowByName(byte *pDesc, void *pOut)  /* 0x00409ee0 */
{
    /* the 42-byte row as one object: rep movsd, ecx = 10, then movsw */
    typedef struct { short w[21]; } PlayfieldRow;
    PlayfieldRow *pRow;
    int           nRows;
    int           i;

    if (gSet.active == 0)
        return 0x28;

    pRow  = (PlayfieldRow *)GetResDataPtr(gSceneTable);
    nRows = gSceneCount;
    for (i = 0; i < nRows; pRow++, i++) {
        if (PascalStringsEqualNoCase((byte *)pRow + 0x1a, pDesc) != 0) {
            *(PlayfieldRow *)pOut = *pRow;
            UnlockResObject(gSceneTable);
            return 0;
        }
    }
    UnlockResObject(gSceneTable);
    return 10;
}


/* Searches the waypoint table (gWaypointTable) for a named position record and fills pPosOut
 * ({status = 0, coord1, coord2, coord3}) with its 3D coordinates. Each entry (stride 0x36) has a
 * primary name at +0x0c and, when entry[7] != 0, a secondary name at +0x26. Returns 1 on success,
 * 0 if not found or no set is active. */
short __cdecl FindWaypointNodePosition(byte *pName, short *pPosOut)  /* 0x00409f70 */
{
    int    i;
    int   *pTable;
    int   *pEntry;
    uint   isMatch;

    if (gSet.active == 0)
        return 0;

    pTable = (int *)GetResDataPtr((void *)gWaypointTable);
    pEntry = pTable + 2;
    for (i = 0; i < *pTable; pEntry = (int *)((int)pEntry + 0x36), i++) {
        isMatch = PascalStringsEqualNoCase(pName, (byte *)(pEntry + 3));
        if ((short)isMatch != 0) {
            pPosOut[1] = *(short *)((int)pEntry + 6);
            pPosOut[2] = *(short *)((int)pEntry + 8);
            pPosOut[3] = *(short *)((int)pEntry + 10);
            pPosOut[0] = 0;
            UnlockResObject((int *)gWaypointTable);
            return 1;
        }
        if (pEntry[7] != 0) {
            isMatch = PascalStringsEqualNoCase(pName, (byte *)((int)pEntry + 0x26));
            if ((short)isMatch != 0) {
                pPosOut[1] = *(short *)((int)pEntry + 0x20);
                pPosOut[2] = *(short *)((int)pEntry + 0x22);
                pPosOut[3] = *(short *)((int)pEntry + 0x24);
                pPosOut[0] = 0;
                UnlockResObject((int *)gWaypointTable);
                return 1;
            }
        }
    }
    UnlockResObject((int *)gWaypointTable);
    return 0;
}


/* Searches gWaypointTable for the entry whose primary and secondary names match pName1 and pName2
 * (in either order), resolves the linked node handle and returns it through ppHandleOut. Returns 1
 * on success, 0 if not found or no set is active. */
short __cdecl FindWaypointPathBetween(byte *pName1, byte *pName2, int **ppHandleOut)  /* 0x0040a060 */
{
    int    i;
    int   *pHandle;
    int   *pTable;
    int   *pEntry;

    if (gSet.active == 0)
        return 0;

    /* two independent tests, each re-reading the link (the exe tests +0x1c
     * twice); the handle is stored after the release */
    pTable = (int *)GetResDataPtr((void *)gWaypointTable);
    pEntry = pTable + 2;
    for (i = 0; i < *pTable; pEntry = (int *)((int)pEntry + 0x36), i++) {
        /* case 1: pName2 = secondary name, pName1 = primary name */
        if (pEntry[7] != 0 &&
            PascalStringsEqualNoCase(pName2, (byte *)((int)pEntry + 0x26)) != 0 &&
            PascalStringsEqualNoCase(pName1, (byte *)(pEntry + 3)) != 0) {
            pHandle = (int *)CloneRenderChildResource(pEntry[7]);
            UnlockResObject((int *)gWaypointTable);
            *ppHandleOut = pHandle;
            return 1;
        }
        /* case 2: pName2 = primary name, pName1 = secondary name (reversed) */
        if (pEntry[7] != 0 &&
            PascalStringsEqualNoCase(pName2, (byte *)(pEntry + 3)) != 0 &&
            PascalStringsEqualNoCase(pName1, (byte *)((int)pEntry + 0x26)) != 0) {
            pHandle = (int *)CloneRenderChildResource(pEntry[7]);
            UnlockResObject((int *)gWaypointTable);
            ReverseWaypointPathDirection(pHandle);
            *ppHandleOut = pHandle;
            return 1;
        }
    }
    UnlockResObject((int *)gWaypointTable);
    return 0;
}


/* Searches gWaypointTable for an entry whose secondary or primary name matches pName. On a match
 * it resolves the linked node handle with CloneRenderChildResource, updates the parent entry
 * reference at namedEntry + 0x18 with RotateWaypointPathToNearestEntry and returns the handle
 * through ppHandleOut. Returns 1 on success, 0 if not found or no set is active. */
short __cdecl FindWaypointPathToNode(byte *pName, int namedEntry, int **ppHandleOut)  /* 0x0040a180 */
{
    int    i;
    int   *pEntry;
    int   *pTable;
    int   *pHandle;

    if (gSet.active == 0)
        return 0;

    /* two independent tests, each re-reading the link (as the exe does) */
    pTable = (int *)GetResDataPtr((void *)gWaypointTable);
    pEntry = pTable + 2;
    for (i = 0; i < *pTable; pEntry = (int *)((int)pEntry + 0x36), i++) {
        /* secondary name match */
        if (pEntry[7] != 0 &&
            PascalStringsEqualNoCase(pName, (byte *)((int)pEntry + 0x26)) != 0) {
            pHandle = (int *)CloneRenderChildResource(pEntry[7]);
            UnlockResObject((int *)gWaypointTable);
            RotateWaypointPathToNearestEntry(namedEntry + 0x18, pHandle);
            *ppHandleOut = pHandle;
            return 1;
        }
        /* primary name match (reversed link) */
        if (pEntry[7] != 0 &&
            PascalStringsEqualNoCase(pName, (byte *)(pEntry + 3)) != 0) {
            pHandle = (int *)CloneRenderChildResource(pEntry[7]);
            UnlockResObject((int *)gWaypointTable);
            ReverseWaypointPathDirection(pHandle);
            RotateWaypointPathToNearestEntry(namedEntry + 0x18, pHandle);
            *ppHandleOut = pHandle;
            return 1;
        }
    }
    UnlockResObject((int *)gWaypointTable);
    return 0;
}
