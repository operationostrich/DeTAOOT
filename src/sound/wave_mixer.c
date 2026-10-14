/* wave_mixer.c -- TI.EXE 0x00401000..0x00405880: the wave mixer (channel mixing, wave-out
 * header queue, audio block conversion). */

#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "df_protos.h"
#include "topics/sound.h"
#include "topics/multimedia.h"

static void __stdcall InitWaveHeaders(void);
static void __stdcall InitWaveBuffers(void);
static void __stdcall TeardownWaveHeaders(void);
static void __stdcall FreeWaveBuffers(void);
static int FindVoiceSourceInPendingDescriptors(int sourceId);
static int FindVoiceSourceInWaveHdrChains(int sourceId);
static int __cdecl AdvanceWaveHdrQueuedBytes(int hdrPlus4, int maxBytes);
static void __cdecl CommitWaveHdrPendingBytesToQueued(int hdrPlus4);

#if DF_FIX_WAVE_RING_LOCK
/* gWaveRingLock: a dedicated lock for the waveOut ring bookkeeping (gWaveOutInFlight,
 * gWaveOutPlaySlot, the WAVEHDR ring). NOT in the original, which guards all of it with the
 * mixer lock gSoundMixerLock: on Win9x winmm delivered completions on the caller's thread, where
 * re-entry is harmless. Modern Windows delivers them on a worker thread while the mixer lock is
 * held across waveOutReset, so the callback could never run and the game hung in wdmaud.drv.
 * The callback takes only this lock. Lock order where both are held: mixer lock first. */
CRITICAL_SECTION g_waveRingLock;
int              g_waveRingLockReady;

/* PORT-HELPER: part of DF_FIX_WAVE_RING_LOCK (no original counterpart). */
void WaveRingLockInit(void)
{
    if (!g_waveRingLockReady) {
        InitializeCriticalSection(&g_waveRingLock);
        g_waveRingLockReady = 1;
    }
}

/* PORT-HELPER: part of DF_FIX_WAVE_RING_LOCK (no original counterpart). */
void WaveRingLockDelete(void)
{
    if (g_waveRingLockReady) {
        g_waveRingLockReady = 0;
        DeleteCriticalSection(&g_waveRingLock);
    }
}
#endif /* DF_FIX_WAVE_RING_LOCK */

/* WaveHdrQueueRow, 0x32 = 50 bytes, 4 rows at gWaveHdrQueues 0x004609a0 (ends on
 * gWaveFlushLatencyCount 0x00460a68). In-file block (not a shared header: that would shift every unit's symbols).
 * Replaces this file's function-local `typedef struct {GainTriple a; GainTriple b;
 * char rest[0x32-30];} HdrRow;` in InitWaveHeaders (a symbol-count change: match/symbol_phase.json compensates).
 * head/tail/cur hold header-block pointers; they are int here and the sites cast, exactly as
 * today's `*(int **)&qb->cur` already does. */
#pragma pack(push, 1)
typedef struct WaveHdrQueueRow {
    int        head;           /* +0x00 gWaveHdrQueues */
    int        tail;           /* +0x04 DAT_004609a4 */
    int        cur;            /* +0x08 DAT_004609a8 */
    GainTriple gainA;          /* +0x0c DAT_004609ac (InitWaveHeaders seeds it with kUnityGain) */
    GainTriple gainB;          /* +0x1b (extraFmt copy target) */
    int        recycleMode;    /* +0x2a DAT_004609ca */
    int        recycleTarget;  /* +0x2e DAT_004609ce */
} WaveHdrQueueRow;             /* 0x32 */
#pragma pack(pop)
#define gWaveHdrQueueTbl (*(WaveHdrQueueRow (*)[4])gWaveHdrQueues)

/* Sums one sample from each of N per-channel mix buffers (MIX_CHANNEL_STRIDE = 0x8010 bytes apart)
 * into the wave buffer, for n samples; this is the 8-bit, 1-channel case. 8-bit samples are
 * unsigned around 0x80, so an even channel count adds 0x80 back. The eight MixChannels* mixers are
 * reached only through g_mix8Bit / g_mix16Bit, indexed by the active channel count (CopyMixBufferSegment). */
void __cdecl MixChannels8x1(unsigned char *src, unsigned char *dst, int n)  /* 0x00401000 */
{
    for (; n != 0; n--) {
        *dst++ = src[0];
        src++;
    }
}

/* The 8-bit, 2-channel mixer; see MixChannels8x1. */
void __cdecl MixChannels8x2(unsigned char *src, unsigned char *dst, int n)  /* 0x00401020 */
{
    for (; n != 0; n--) {
        *dst++ = src[0x8010] + src[0] + 0x80;
        src++;
    }
}

/* The 8-bit, 3-channel mixer; see MixChannels8x1. */
void __cdecl MixChannels8x3(unsigned char *src, unsigned char *dst, int n)  /* 0x00401050 */
{
    for (; n != 0; n--) {
        *dst++ = src[0x8010] + src[0x10020] + src[0];
        src++;
    }
}

/* The 8-bit, 4-channel mixer; see MixChannels8x1. */
void __cdecl MixChannels8x4(unsigned char *src, unsigned char *dst, int n)  /* 0x00401080 */
{
    for (; n != 0; n--) {
        *dst++ = src[0x18030] + src[0x8010] + src[0x10020] + src[0] + 0x80;
        src++;
    }
}

/* The 16-bit, 1-channel mixer; see MixChannels8x1. */
void __cdecl MixChannels16x1(short *src, short *dst, int n)  /* 0x004010c0 */
{
    for (; n != 0; n--) {
        *dst++ = src[0];
        src++;
    }
}

/* The 16-bit, 2-channel mixer; see MixChannels8x1. */
void __cdecl MixChannels16x2(short *src, short *dst, int n)  /* 0x004010f0 */
{
    for (; n != 0; n--) {
        *dst++ = src[0x4008] + src[0];
        src++;
    }
}

/* The 16-bit, 3-channel mixer; see MixChannels8x1. */
void __cdecl MixChannels16x3(short *src, short *dst, int n)  /* 0x00401120 */
{
    for (; n != 0; n--) {
        *dst++ = src[0x4008] + src[0x8010] + src[0];
        src++;
    }
}

/* The 16-bit, 4-channel mixer; see MixChannels8x1. */
void __cdecl MixChannels16x4(short *src, short *dst, int n)  /* 0x00401160 */
{
    for (; n != 0; n--) {
        *dst++ = src[0xc018] + src[0x4008] + src[0x8010] + src[0];
        src++;
    }
}

/* Brings up the audio output: creates the sound critical section, allocates a DMA buffer from VxD
 * memory when it is available, creates the secondary sound heap and opens the wave device through
 * OpenWaveDevice. On success it stores the format and the log2 of the bytes per frame (the shift
 * for DMA offsets) and runs the buffer and header init; on failure it undoes the heap, the VxD
 * memory and the critical section. Returns 0, or an error code.
 * nBuffers is always 4 (from InitSoundSystem), nChannels 1 or 2, sampleRate a quality enum (always
 * 2 from the caller), bytesPerSample 1 or 2. */
int __cdecl OpenSoundDevice(int nBuffers, int nChannels, int sampleRate, int bytesPerSample)  /* 0x004011a0 */
{
    int iVar;

    gSoundMixerLocked = 0;
    WaveRingLockInit();
    InitializeCriticalSection((LPCRITICAL_SECTION)&gSoundMixerLock);
    EnterCriticalSection((LPCRITICAL_SECTION)&gSoundMixerLock);
    gSoundMixerLocked = 1;

    if (GetHeapDescPtr() != 0)
        *(int *)&gSoundVxdHeapSize = 0;
    else {
        gSoundVxdHeapSize = nBuffers * 0x8010 + 0x40000;
        InitVxDHeap(gSoundVxdHeapSize, 0x400);
    }

    AllocSecondaryHeap((int *)&gSoundHeap, nBuffers * 0x8010 + 0x40000);
    PushHeapContext((int *)&gSoundHeap);
    iVar = OpenWaveDevice(&gWaveDeviceOps, sampleRate, bytesPerSample, nChannels,
                          &gMixChunkFrames, &gMixQueueTarget, (unsigned int *)&gMixFlushMask);
    if (iVar != 0) {
        PopHeapContext();
        FreeSecondaryHeap((int *)&gSoundHeap);
        if (gSoundVxdHeapSize != 0)
            ShutdownVxDHeap();
        gSoundMixerLocked = 0;
        LeaveCriticalSection((LPCRITICAL_SECTION)&gSoundMixerLock);
        DeleteCriticalSection((LPCRITICAL_SECTION)&gSoundMixerLock);
        WaveRingLockDelete();
        return iVar;
    }

    gMixBufferCount = nBuffers;
    gMixSilenced = 0;
    gSoundBufferDescCount = 0;
    gMixSampleRate = sampleRate;
    gMixBytesPerSample = bytesPerSample;
    gMixSampleChannels = nChannels;
    gMixFrameBytes = bytesPerSample * nChannels;
    switch (*(int *)&gMixFrameBytes) {      /* log2 of the bytes per sample frame */
    case 1:
        gMixFrameShift = 0;
        break;
    case 2:
        gMixFrameShift = 1;
        break;
    case 4:
        gMixFrameShift = 2;
        break;
    }
    InitWaveBuffers();
    InitWaveHeaders();
    PopHeapContext();
    gSoundMixerLocked = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)&gSoundMixerLock);
    return 0;
}


/* Initialises the per-buffer wave header rows at gWaveHdrQueues: each 0x32-byte row holds two
 * 15-byte gain blocks, seeded with kUnityGain. Calls AllocInitWaveOutHeaderForBufferIdx for each buffer. */
static void __stdcall InitWaveHeaders(void)  /* 0x00401340 */
{
    /* one 0x32-byte row from DAT_004609ac: two 15-byte gain blocks, then the rest */
    int   i;

    DF_TRACE("InitWaveHeaders: nBufs=%d base=%p defaults=%d/%d/%d",
             (int)gMixBufferCount, (void *)&DAT_004609ac,
             (int)kUnityGain, (int)kUnityGainWord1, (int)kUnityGainWord2);
    /* indexed off the global row table (not a free pointer): the exe hoists the
     * loop's count reload over the stores, so they cannot alias it. The
     * 15-byte default gain block at kUnityGain is copied whole to both. */
    for (i = 0; i < gMixBufferCount; i++) {
        AllocInitWaveOutHeaderForBufferIdx(i);
        gWaveHdrQueueTbl[i].gainA = *(GainTriple *)&kUnityGain;
        gWaveHdrQueueTbl[i].gainB = *(GainTriple *)&kUnityGain;
    }
}



/* Allocates the 0x176-byte wave header block for a buffer, initialises its payload, clears its two
 * link words and makes it the head, tail and current node of the buffer's queue row. */
void __cdecl AllocInitWaveOutHeaderForBufferIdx(int bufIdx)  /* 0x004013c0 */
{
    unsigned int  *pHdr;
    WaveHdrQueueRow *qb;     /* the row is formed before the allocation (edi) */

    qb      = WaveHdrQueueBase(bufIdx);
    pHdr  = (unsigned int *)AllocEngineBuffer(0x176u);
    *pHdr = 0;
    InitWaveOutHdrPayload(pHdr + 1, (int *)0);
    *(unsigned int *)((char *)pHdr + 0x172) = 0;
    *(unsigned int *)((char *)pHdr + 0x16e) = 0;
    *(unsigned int **)&qb->cur  = (unsigned int **)pHdr;
    *(unsigned int **)&qb->tail  = (unsigned int **)pHdr;
    *(unsigned int **)&qb->head  = (unsigned int **)pHdr;
    qb->recycleMode = 0;
    qb->recycleTarget = 0;
}


/* Initialises the 0x176-byte wave-out payload (*pPayload): codec lookup, buffer sizing, optional
 * allocator callback and coefficient clears. */
uint32_t __cdecl InitWaveOutHdrPayload(uint32_t *pPayload, int *pFormat) /* 0x00401420 */
{
    /* The exe addresses everything from pPayload + 0xc (q), copies the 15-byte
     * block at pFormat + 0x18 and the whole 39-byte format as struct
     * assignments, and takes the "rate <= mixer rate" arm first. */
#pragma pack(push, 1)
    typedef struct { char b[15]; } B15;
    typedef struct { char b[39]; } B39;
#pragma pack(pop)
    typedef void (__cdecl *RegionInitFn)(int *, int *);
    char *q;
    int   rate;
    int   len;
    int   i;
    int  *p;
    int   n;

    if (pFormat == NULL || pFormat[1] == 0) {
        *pPayload = 0;
        pPayload[2] = 0;
        pPayload[1] = 0;
        return 0;
    }
    if (*pFormat == 0) {
        q = (char *)(pPayload + 3);
        *(int *)(q + 0x08) = pFormat[1];
        *(int *)(q + 0x0c) = pFormat[3];
        *(int *)(q + 0x14) = pFormat[5];
        *(int *)(q + 0x10) = pFormat[4];
        *(B15 *)(q + 0x18) = *(B15 *)(pFormat + 6);
        switch (*(int *)(q + 0x14) * *(int *)(q + 0x10)) {
        case 1: *(int *)(q + 4) = 0; break;
        case 2: *(int *)(q + 4) = 1; break;
        case 4: *(int *)(q + 4) = 2; break;
        }
        rate = *(int *)(q + 0x0c);
        if (gMixSampleRate >= rate) {
            *(int *)q = gMixSampleRate / rate;
            len = pFormat[2] * *(int *)q;
        } else {
            *(int *)q = -(rate / gMixSampleRate);
            len = -(pFormat[2] / *(int *)q);
        }
        pPayload[2] = (uint32_t)len;
        *pPayload = 1;
        pPayload[1] = 0;
        return 1;
    }
    q = (char *)(pPayload + 3);
    for (i = 0; i < gSoundBufferDescCount; i++)      /* gSoundBufferDescs: int[30], 6 x 5-int records */
        if (gSoundBufferDescs[i * 5] == *pFormat)
            break;
    if (gSoundBufferDescCount == i) {
        DF_TRACE("!! InitWaveOutHdrPayload: codec id 0x%x not registered (%d entries: 0x%x 0x%x 0x%x) "
                 "fmt=[%d %d %d %d %d %d %d %d %d] pFmt=%p",
                 pFormat[0], gSoundBufferDescCount, gSoundBufferDescs[0], gSoundBufferDescs[5], gSoundBufferDescs[10],
                 pFormat[0], pFormat[1], pFormat[2], pFormat[3], pFormat[4],
                 pFormat[5], pFormat[6], pFormat[7], pFormat[8], (void *)pFormat);
        FatalErrorWithCode(0x12d);
    }
    *(int **)(q + 0x4e) = gSoundBufferDescs + i * 5;
    *(int *)(q + 0x08) = pFormat[1];
    *(int *)(q + 0x0c) = pFormat[3];
    *(int *)(q + 0x14) = pFormat[5];
    *(int *)(q + 0x10) = pFormat[4];
    *(B15 *)(q + 0x18) = *(B15 *)(pFormat + 6);
    *(B39 *)(q + 0x27) = *(B39 *)pFormat;
    switch (*(int *)(q + 0x14) * *(int *)(q + 0x10)) {
    case 1: *(int *)(q + 4) = 0; break;
    case 2: *(int *)(q + 4) = 1; break;
    case 4: *(int *)(q + 4) = 2; break;
    }
    rate = *(int *)(q + 0x0c);
    if (gMixSampleRate >= rate) {
        *(int *)q = gMixSampleRate / rate;
        len = pFormat[2] * *(int *)q;
    } else {
        *(int *)q = -(rate / gMixSampleRate);
        len = -(pFormat[2] / *(int *)q);
    }
    *(void **)(q + 0x52) = AllocEngineBuffer(*(unsigned int *)(*(int *)(q + 0x4e) + 8));
    *(int *)(q + 0x156) = 0;
    *(int *)(q + 0x15a) = 0;
    {
        /* Dispatch slot +0x10 (InitSoundBufferRegionDesc) takes two arguments, (pFormat, buffer):
         * the exe pushes the buffer, then pFormat (0x00401630). Class 2, dispatch-slot arity. */
        ((RegionInitFn)*(void **)(*(int *)(q + 0x4e) + 0x10))(pFormat, *(int **)(q + 0x52));
    }
    p = (int *)(q + 0xd6);
    n = 0x20;
    do {
        *p++ = 0;
        n--;
        p[-0x21] = 0;
    } while (n != 0);
    *pPayload = 2;
    pPayload[2] = (uint32_t)len;
    pPayload[1] = 0;
    return 2;
}


/* Allocates the wave buffer pool (nBuffers x 0x8010 bytes) and zeroes all wave buffer state
 * pointers and counters. Called by OpenSoundDevice once OpenWaveDevice has succeeded. */
static void __stdcall InitWaveBuffers(void)  /* 0x00401690 */
{
    int  i;
    int *p;

    /* chained assignments: the exe stores right to left (a90, 894, 888, 890),
     * after the two zeros; the one loop becomes two rep stosd, the SECOND
     * statement's first (the exe clears gWaveBufferLoopGatePending, then gWaveBufferActive) */
    p = (int *)AllocEngineBuffer(gMixBufferCount * 0x8010);
    gMixRingFillFrames = gMixFramesPending = 0;
    gMixRingBase = gMixRingTailCursor = gMixRingWriteCursor = gMixRingReadCursor = (int)p;
    gMixRingEnd = gMixRingTailCursor + 0x8010;

    for (i = 0; i < gMixBufferCount; i++) {
        gWaveBufferActive[i] = 0;
        gWaveBufferLoopGatePending[i] = 0;
    }

    gWaveBindsPending = 0;
    gWaveFeedLatencySum = 0;
    gWaveFeedLatencyCount = 0;
    gWaveFlushLatencySum = 0;
    gWaveFlushLatencyCount = 0;
    gMixFramesQueuedTotal = 0;
}


/* Shuts down what OpenSoundDevice opened: closes the wave device, frees the buffers and headers,
 * the secondary heap and the VxD DMA memory, and destroys the critical section. Returns the result
 * of the device close (0 on success). */
unsigned int __stdcall CloseSoundDevice(void)  /* 0x00401720 */
{
    unsigned int uVar;

    EnterCriticalSection((LPCRITICAL_SECTION)&gSoundMixerLock);
    gSoundMixerLocked = 1;

    PushHeapContext((int *)&gSoundHeap);
    uVar = CloseWaveDevice(gWaveDeviceOps);
    FreeWaveBuffers();
    TeardownWaveHeaders();
    PopHeapContext();

    FreeSecondaryHeap((int *)&gSoundHeap);
    if (gSoundVxdHeapSize != 0)
        ShutdownVxDHeap();

    gSoundMixerLocked = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)&gSoundMixerLock);
    DeleteCriticalSection((LPCRITICAL_SECTION)&gSoundMixerLock);
    WaveRingLockDelete();
    return uVar;
}


/* Frees every buffer's wave header chain (FreeWaveOutHeaderChainForBufferIdx); the counterpart of InitWaveHeaders. */
static void __stdcall TeardownWaveHeaders(void)  /* 0x004017b0 */
{
    int i;

    for (i = 0; i < gMixBufferCount; i++)
        FreeWaveOutHeaderChainForBufferIdx(i);
}


/* Walks one buffer's chain of 0x172 links, freeing each node's decode slots and engine buffer. */
void __cdecl FreeWaveOutHeaderChainForBufferIdx(int bufIdx)  /* 0x004017d0 */
{
    int node;                          /* the node first: it gets esi, the saved next edi */
    int nextNode;

    node = gWaveHdrQueueTbl[bufIdx].head;   /* inline in the exe: lea / lea / mov */
    while (node != 0) {
        nextNode = *(int *)(node + 0x16e);
        FreeWaveOutHdrDecodeSlots((int *)(node + 4));
        FreeEngineBuffer(node);
        node = nextNode;
    }
}


/* If the channel type (*pHdrPlus4) is 2, frees the 32 engine-buffer child slots stored as ints
 * from byte +0x62 of the header block, and the single slot at byte +0x5e. */
void __cdecl FreeWaveOutHdrDecodeSlots(int *pHdrPlus4)  /* 0x00401810 */
{
    int  remaining;                         /* the count is declared first: it gets edi, the pointer ebx */
    int *pSlot;

    if (*pHdrPlus4 == 2) {
        pSlot = (int *)((char *)pHdrPlus4 + 0x62);
        pHdrPlus4 += 3;                 /* add esi,0xc: the parameter steps to the hdr body */
        remaining = 0x20;
        do {
            if (*pSlot != 0)
                FreeEngineBuffer(*pSlot);
            pSlot++;
            remaining--;
        } while (remaining != 0);
        FreeEngineBuffer(*(int *)((char *)pHdrPlus4 + 0x52));
    }
}


/* Frees the wave buffer pool allocated by InitWaveBuffers. */
static void __stdcall FreeWaveBuffers(void)  /* 0x00401850 */
{
    FreeEngineBuffer(gMixRingBase);
}


/* Binds a wave to a sound channel through BindWaveFormatToSoundChannel, with the loop gate flags
 * cleared, under the sound heap context. Returns 0x385 if the mixer is not locked, 900 if the
 * wave has no frames (pWaveData word 2). */
unsigned int __cdecl LoadWaveDataOntoSoundChannel(int bufIdx, void *pWaveData) /* 0x00401860 */
{
    unsigned int **pp;

    if (gSoundMixerLocked == 0)
        return 0x385;
    pp = (unsigned int **)pWaveData;
    if ((int)pp[2] <= 0)                /* cmp 0 / jg, as 0x00401870 has it */
        return 900;
    PushHeapContext(&gSoundHeap);
    SetSoundChannelLoopGateFlags(bufIdx, 0);
    BindWaveFormatToSoundChannel(bufIdx, pWaveData, 0);
    PopHeapContext();
    return 0;
}


/* Allocates a 0x2f-byte copy of the caller's WAVEFORMATEX-like format, updates the header
 * bookkeeping for bufIdx and optionally clears the previous chain. */
void __cdecl BindWaveFormatToSoundChannel(int bufIdx, void *pFmtIn, int isFirst) /* 0x004018C0 */
{
    typedef struct { char b[0x27]; } WaveFmtEntry;   /* rep movsd 9 + movsw + movsb */
    uint32_t *puDst;

    puDst = (uint32_t *)AllocEngineBuffer(0x2f);
    *(WaveFmtEntry *)puDst = *(WaveFmtEntry *)pFmtIn;
    *(uint32_t *)((char *)puDst + 0x27) = 0;
    /* the third argument is stored at +0x2b, as the queued twin QueueWaveFormatOnSoundChannel
     * does (the port had dropped it) */
    *(uint32_t *)((char *)puDst + 0x2b) = isFirst;
    if (gWaveBufferActive[bufIdx] != 0)
        ClearSoundChannelPendingDescriptors(bufIdx);
    gWaveBufferFormatQueue[bufIdx] = (int)puDst;
    if (gWaveBufferActive[bufIdx] == 0)
        gWaveBindsPending++;
    gWaveBufferActive[bufIdx] = 1;
}


/* Frees the wave header auxiliary chain for a buffer and clears its refresh flags. */
void __cdecl ClearSoundChannelPendingDescriptors(int bufIdx) /* 0x00401930 */
{
    int walk;
    int next;

    walk = gWaveBufferFormatQueue[bufIdx];
    while (walk != 0) {
        next = *(int *)(walk + 0x27);
        FreeEngineBuffer(walk);
        walk = next;
    }
    gWaveBufferActive[bufIdx] = 0;
    gWaveBufferLoopGatePending[bufIdx] = 0;
}


/* Applies one sound channel template entry, bracketing QueueWaveFormatOnSoundChannel with the
 * sound heap context. Returns 0x385 if the sound subsystem is inactive, 900 if the frame count
 * (pData[2]) is below 1. */
int __cdecl QueueWaveDataOntoSoundChannel(int ch, uint32_t *pData, int isFirst)  /* 0x00401970 */
{
    if (gSoundMixerLocked == 0)
        return 0x385;
    if ((int)pData[2] <= 0)             /* cmp 0 / jg, as 0x00401980 has it */
        return 900;
    PushHeapContext(&gSoundHeap);
    QueueWaveFormatOnSoundChannel(ch, pData, isFirst);
    PopHeapContext();
    return 0;
}


/* If the channel (gWaveBufferActive[ch]) is not busy, applies the template at once. Otherwise
 * queues it: a 0x2f-byte deferred node holding 0x26 bytes of template data, isFirst at +0x2b and a
 * next pointer at +0x27, appended to the list at gWaveBufferFormatQueue[ch]. */
void __cdecl QueueWaveFormatOnSoundChannel(int ch, uint32_t *pData, uint32_t isFirst)  /* 0x004019C0 */
{
    typedef struct { char b[0x27]; } WaveFmtEntry;   /* rep movsd 9 + movsw + movsb */
    uint32_t *pNode;
    int       iCur;
    int       iPrev;

    if (gWaveBufferActive[ch] == 0) {
        BindWaveFormatToSoundChannel(ch, pData, (int)isFirst);
        return;
    }

    pNode = (uint32_t *)AllocEngineBuffer(0x2f);
    *(WaveFmtEntry *)pNode = *(WaveFmtEntry *)pData;
    *(uint32_t *)((int)pNode + 0x27) = 0;
    *(uint32_t *)((int)pNode + 0x2b) = isFirst;

    iCur = gWaveBufferFormatQueue[ch];
#if DF_FIX_SOUND_TEMPLATE_QUEUE_HEAD
    if (iCur != 0) {
        do {
            iPrev = iCur;
            iCur  = *(int *)(iPrev + 0x27);
        } while (iCur != 0);
        *(uint32_t **)(iPrev + 0x27) = pNode;
    } else {
        gWaveBufferFormatQueue[ch] = (int)pNode;
    }
#else
    /* The exe never sets the head: on an empty list iPrev is its unwritten
     * stack slot (0x00401a39), so the node goes through a stale pointer. */
    while (iCur != 0) {
        iPrev = iCur;
        iCur  = *(int *)(iPrev + 0x27);
    }
    *(uint32_t **)(iPrev + 0x27) = pNode;
#endif
}


/* Binds an all-zero format to the channel (BindWaveFormatToSoundChannel) under the sound heap
 * context. Returns 0x385 if the mixer is not locked. */
unsigned int __cdecl SilenceSoundChannel(int bufIdx) /* 0x00401a50 */
{
    unsigned int fmt[10];               /* the exe reserves 0x28: a 10-dword frame with fmt[1] = 0 */

    if (gSoundMixerLocked == 0)
        return 0x385;
    fmt[1] = 0;                         /* the only store the exe makes into it */
    PushHeapContext(&gSoundHeap);
    BindWaveFormatToSoundChannel(bufIdx, fmt, 0);
    PopHeapContext();
    return 0;
}


/* Wrapper over QuerySoundChannelIdle. */
int __cdecl IsSoundChannelIdle(int bufIdx)  /* 0x00401aa0 */
{
    return QuerySoundChannelIdle(bufIdx);
}


/* Returns 1 if the channel's wave header queue is valid and empty, 0 otherwise. */
int __cdecl QuerySoundChannelIdle(int bufIdx)  /* 0x00401AB0 */
{
  WaveHdrQueueRow *qb;       /* the queue row, formed once */
  int *node;
  int framesLeft;  /* local_4 */

  DrainWaveBufferToMixBudget(bufIdx);
  qb = WaveHdrQueueBase(bufIdx);
  if (qb->recycleMode != 0) {
    node = *(int **)&qb->cur;
    if (node != *(int **)&qb->head)
      return 0;
    if (*(int **)&qb->tail != *(int **)&qb->head)
      return 0;
    return *node == 0;
  }
  /* GetWaveOutQueuedFrames (dispatch slot 3, +0xc) takes an out-parameter: LEA EAX,[ESP+0xc];
   * PUSH EAX; CALL [EBX+0xc] (0x00401b04). Called as a zero-argument pointer it crashed on logo.mov. */
  {
    typedef void (__cdecl *WaveOp3)(int *);
    ((WaveOp3)(*(void **)((char *)(intptr_t)gWaveDeviceOps + 0xc)))(&framesLeft);
  }
  node = *(int **)&qb->cur;
  do {
    framesLeft -= QueryWaveHdrQueuedByteDelta((int)(node + 1));
    if (framesLeft <= 0) break;
    node = *(int **)((int)node + 0x172);
  } while (*(int **)&qb->head != node);
  while (1) {
    if (*node != 0)
      return 0;
    if (*(int **)&qb->tail == node)
      break;
    node = *(int **)((int)node + 0x16e);
  }
  return 1;
}


/* Reads the queued byte delta dword from the wave header workspace (+4 from pHdrPlus4). */
int __cdecl QueryWaveHdrQueuedByteDelta(int *pHdrPlus4) /* 0x00401B70 */
{
    return (int)*(uint32_t *)(pHdrPlus4 + 1);
}


/* Advances and trims the buffer's queued wave header chain against a byte budget of
 * (gMixQueueTarget + 1) * gMixChunkFrames. */
void __cdecl DrainWaveBufferToMixBudget(int bufIdx) /* 0x00401b80 */
{
    /* the queue walk in the exe's shape: one successor rule, written out
     * twice (the budget loop and the zero-adjust loop) */
    int   node;
    WaveHdrQueueRow *qb;
    int   budget;
    int   p;

    budget = (gMixQueueTarget + 1) * gMixChunkFrames;   /* before qb/node: sets the exe's ebx/edi */
    qb     = WaveHdrQueueBase(bufIdx);
    node   = qb->cur;
    budget -= QueryWaveHdrQueuedByteDelta((int *)(node + 4));
    while (budget > 0) {
        if (qb->recycleMode != 0 && qb->recycleTarget == node && qb->recycleMode != 1)
            node = qb->tail;
        else
            node = *(int *)(node + 0x172);
        if (node == 0 || qb->cur == node)
            return;
        budget -= QueryWaveHdrQueuedByteDelta((int *)(node + 4));
    }
    AdjustWaveHdrWriteCursor((int *)(node + 4), -budget);
    if (qb->recycleMode == 0) {
        qb->head = node;
        p = *(int *)(node + 0x172);
        *(int *)(node + 0x172) = 0;
        while (p != 0) {
            node = *(int *)(p + 0x172);
            FreeWaveOutHdrDecodeSlots((int *)(p + 4));
            FreeEngineBuffer(p);
            p = node;
        }
    } else {
        for (;;) {
            /* nested, the recycle-slot arm first: the exe lays the qb[4] arm at the loop top */
            if (qb->recycleMode != 0 && qb->recycleTarget == node) {
                if (qb->recycleMode == 1)
                    node = *(int *)(node + 0x172);
                else
                    node = qb->tail;
            } else
                node = *(int *)(node + 0x172);
            if (node == 0 || qb->cur == node)
                return;
            AdjustWaveHdrWriteCursor((int *)(node + 4), 0);
        }
    }
}


/* Adjusts the header's write cursor by delta; for a type 2 header it also calls the type's
 * adjustment function. */
void __cdecl AdjustWaveHdrWriteCursor(int *pHdr, int delta)  /* 0x00401C80 */
{
  int *pDst;

  if (*pHdr == 2) {
    pDst = pHdr + 3;                 /* a temp: the exe forms the address before the length */
    UpdateWaveHdrReadPosition(pDst, pHdr[1] - delta);
  }
  return;
}


/* Updates the header's read position, advancing the read cursor and freeing the buffer slots it
 * has consumed. */
void __cdecl UpdateWaveHdrReadPosition(int *pBody, int position)  /* 0x00401CA0 */
{
  /* the ring of 32 buffer slots at +0x56 and their end positions at +0xd6 */
#pragma pack(push, 1)
  typedef struct { char hdr[0x56]; int buf[32]; int end[32]; int rd; int wr; } WaveSlotRing;
#pragma pack(pop)
  int wrIdx;
  int *pLastBuf;
  int slotLead;
  int scaledPos;
  int rdIdx;

  /* the positive step first; the slot arrays indexed afresh at each use */
  if (*pBody > 0) {
    scaledPos = position / *pBody;
  }
  else {
    scaledPos = -(position * *pBody);
  }
  slotLead = *(int *)(*(int *)((int)pBody + 0x4e) + 4);
  rdIdx = *(int *)((int)pBody + 0x156);
  if (*(int *)((int)pBody + 0x15a) != rdIdx) {
    do {
      if (scaledPos < ((WaveSlotRing *)pBody)->end[rdIdx] + slotLead) break;
      ((WaveSlotRing *)pBody)->end[rdIdx] = 0;
      FreeEngineBuffer(((WaveSlotRing *)pBody)->buf[rdIdx]);
      ((WaveSlotRing *)pBody)->buf[rdIdx] = 0;
      if (++rdIdx == 0x20) {
        rdIdx = 0;
      }
    } while (*(int *)((int)pBody + 0x15a) != rdIdx);
  }
  wrIdx = *(int *)((int)pBody + 0x15a);
  *(int *)((int)pBody + 0x156) = rdIdx;
  pLastBuf = (int *)((int)pBody + wrIdx * 4 + 0x56);
  scaledPos = *pLastBuf;
  if (scaledPos != 0) {
    FreeEngineBuffer(scaledPos);
    *pLastBuf = 0;
  }
  return;
}


/* Returns 0x385 if the mixer is not locked. Enabling sets gWaveBufferLoopGatePending[ch] to 1 when
 * the queue row's recycle mode is 0; disabling sets it to -1 when the recycle mode is set. */
unsigned int __cdecl SetSoundChannelLoopGateFlags(int bufIdx, int enable) /* 0x00401d40 */
{
    /* set case first; the row is addressed inline at each test */
    if (gSoundMixerLocked == 0)
        return 0x385;
    if (enable != 0) {
        if (WaveHdrQueueBase(bufIdx)->recycleMode == 0)
            gWaveBufferLoopGatePending[bufIdx] = 1;
    } else if (WaveHdrQueueBase(bufIdx)->recycleMode != 0)
        gWaveBufferLoopGatePending[bufIdx] = (int)0xffffffff;
    return 0;
}


/* Applies the gain through ApplySoundChannelGain; always returns 0. */
unsigned int __cdecl SetSoundChannelGain(int bufIdx, void *pGain) /* 0x00401d90 */
{
    ApplySoundChannelGain(bufIdx, pGain);
    return 0;
}


/* Copies the optional 0xe-byte extra format tail into the buffer's queue row (stride 0x32), then
 * updates the queue anchors through GetWaveOutHdrBaseGain and MultiplyGainTriples. */
void __cdecl ApplySoundChannelGain(int bufIdx, void *pExtra) /* 0x00401DB0 */
{
    WaveHdrQueueRow *qb;
    int           scratchGain[4];  /* local_10 */

    /* the row formed once; the 15-byte gain block copied as one struct */
    qb = WaveHdrQueueBase(bufIdx);
    if (pExtra != NULL)
        qb->gainB = *(GainTriple *)pExtra;
    GetWaveOutHdrBaseGain((int *)(qb->cur + 4), scratchGain);
    DF_TRACE("extraFmt: buf=%d pExtra=%p nodeType=%d fmt=[%d/2^%d] scratch=[%d/2^%d]",
             bufIdx, pExtra, *(int *)(qb->cur + 4),
             qb->gainB.a.value, (int)qb->gainB.a.shift,
             scratchGain[0], (int)*((unsigned char *)scratchGain + 4));
    MultiplyGainTriples(&qb->gainA, &qb->gainB,
                                     (GainTriple *)scratchGain);
}


/* Fetches a node's base gain triple into pScratch16: for a raw-PCM node the static unity-gain
 * default (kUnityGain), for a decoder node the gains stored in the node at +0x24. */
void __cdecl GetWaveOutHdrBaseGain(int *pQueueHeadPlus4, int *pScratch16) /* 0x00401E20 */
{
    /* a switch with a whole 15-byte GainTriple copy per case (cases 1 and 2
     * are separate identical blocks in the exe) */
    switch (*pQueueHeadPlus4) {
    case 0:
        /* Raw-PCM node: use the static unity-gain default at 0x0045b030. */
        *(GainTriple *)pScratch16 = *(GainTriple *)&kUnityGain;
        break;
    case 1:
        /* Decoder node: the gains live at node +0x24 (words 9..12, byte +0x32). */
        *(GainTriple *)pScratch16 = *(GainTriple *)(pQueueHeadPlus4 + 9);
        break;
    case 2:
        *(GainTriple *)pScratch16 = *(GainTriple *)(pQueueHeadPlus4 + 9);
        break;
    }
}


/* Looks the voice source up with FindVoiceSourceInPendingDescriptors. Writes the buffer index to
 * *pOutSlot and returns 0, or returns 1 if the source is not found. */
int TryResolveWaveMixSlotForVoiceSource(int sourceId, int *pOutSlot) /* 0x00401eb0 */
{
    int r = FindVoiceSourceInPendingDescriptors(sourceId);
    if (r == -1)                        /* cmp eax,-1 */
        return 1;
    *pOutSlot = r;
    return 0;
}


/* First stage of the voice source search: walks each active buffer's queue chain
 * (gWaveBufferFormatQueue[bi], linked at +0x27) for a node whose +4 is sourceId and returns the
 * buffer index; failing that, returns the second stage's answer.
 *
 * Port note: the port had merged the two stages into one loop over the header chains with this
 * stage's test and an empty second stage, so a source present only in a header chain was never found. */
static int FindVoiceSourceInPendingDescriptors(int sourceId) /* 0x00401ee0 */
{
    int bi;
    int node;

    for (bi = 0; bi < gMixBufferCount; bi++) {
        if (gWaveBufferActive[bi] != 0) {
            for (node = gWaveBufferFormatQueue[bi]; node != 0; node = *(int *)((char *)node + 0x27)) {
                if (*(int *)((char *)node + 4) == sourceId)
                    return bi;
            }
        }
    }
    return FindVoiceSourceInWaveHdrChains(sourceId);
}



/* Second stage of the voice source search: walks each buffer's WAVEHDR chain (gWaveHdrQueues rows,
 * linked at +0x16e) and asks MatchSoundNodeChannelKey whether a node's channel key matches.
 * Returns the buffer index, or -1. Was an empty stub (ClearVoiceSourceBindingWithNoWaveHdrMatch). */
static int FindVoiceSourceInWaveHdrChains(int sourceId) /* 0x00401F30 */
{
    int  bi;
    int  hdr;
    int *pRow;

    bi = 0;
    if (bi < gMixBufferCount) {
        pRow = (int *)&gWaveHdrQueues;
        do {
            for (hdr = *pRow; hdr != 0; hdr = *(int *)((char *)hdr + 0x16e)) {
                if (MatchSoundNodeChannelKey((int *)(hdr + 4), sourceId) != 0)
                    return bi;
            }
            pRow = (int *)((char *)pRow + 0x32);
            bi++;
        } while (bi < gMixBufferCount);
    }
    return -1;
}



/* Returns non-zero when the node's type (word 0) is 1 or 2 and pNode[5] equals key. */
int __cdecl MatchSoundNodeChannelKey(int *pNode, int key)  /* 0x00401F90 */
{
    switch (*pNode) {                   /* a switch: two `return 0` blocks in the exe */
    case 0:
        return 0;
    case 1:
        return pNode[5] == key;
    case 2:
        return pNode[5] == key;
    default:
        return 0;
    }
}


/* Calls the wave device's slot +8 and then sets gMixSilenced. Used by
 * SuspendGameUiForModalDialog / RestoreGameUiAfterModalDialog while sound is running (gSoundOn). */
unsigned int __cdecl SetSoundMixerSilenced(unsigned int silenced)  /* 0x00401fd0 */
{
    void (**ppfn)(unsigned int);

    /* the slot takes one argument, silenced (push esi; call [eax+8]); the callee cleans the
     * stack, hence no ADD ESP */
    ppfn = (void (**)(unsigned int))((char *)(uintptr_t)(unsigned int)gWaveDeviceOps + 8);
    (*ppfn)(silenced);
    gMixSilenced = (int)silenced;
    return 0;
}


/* Packs two volume gains into a GainTriple; returns 0.
 *
 * The gains are doubles (the exe reads qword [esp+8] / [esp+0x10] and every caller pushes 8-byte
 * constants). Each is scaled by 65536.0 before a 16.16 store and by 256.0 before an 8.8 store,
 * and stored before its shift byte.
 *
 * Port note: the port had float parameters and no scale, so every volume below 1.0 truncated to 0. 
 * */
int __cdecl PackVolumePairToGainTriple(int *pTriple, double xs, double ys) /* 0x00401ff0 */
{
    double dAvg;

    DF_TRACE("packVol: xs=%d/1000 ys=%d/1000", (int)(xs * 1000.0), (int)(ys * 1000.0));
    if (xs < 1.0) {
        *pTriple = (long)(xs * 65536.0);
        *(unsigned char *)((char *)pTriple + 4) = 0x10;
    } else if (xs == 1.0) {
        *(unsigned char *)((char *)pTriple + 4) = 0;
        *pTriple = 1;
    } else {
        *pTriple = (long)(xs * 256.0);
        *(unsigned char *)((char *)pTriple + 4) = 8;
    }
    if (ys < 1.0) {
        *(int *)((char *)pTriple + 5) = (long)(ys * 65536.0);
        *(unsigned char *)((char *)pTriple + 9) = 0x10;
    } else if (ys == 1.0) {
        *(int *)((char *)pTriple + 5) = 1;
        *(unsigned char *)((char *)pTriple + 9) = 0;
    } else {
        *(int *)((char *)pTriple + 5) = (long)(ys * 256.0);
        *(unsigned char *)((char *)pTriple + 9) = 8;
    }
    dAvg = (ys + xs) * 0.5;
    if (dAvg < 1.0) {
        *(int *)((char *)pTriple + 10) = (long)(dAvg * 65536.0);
        *(unsigned char *)((char *)pTriple + 0xe) = 0x10;
        return 0;
    }
    if (dAvg == 1.0) {
        *(int *)((char *)pTriple + 10) = 1;
        *(unsigned char *)((char *)pTriple + 0xe) = 0;
        return 0;
    }
    *(int *)((char *)pTriple + 10) = (long)(dAvg * 256.0);
    *(unsigned char *)((char *)pTriple + 0xe) = 8;
    return 0;
}


/* out = a * b over three fixed-point gains (each value / 2^shift, in a packed 15-byte GainTriple:
 * int value, byte shift, three times). The shift is picked by magnitude: below 1.0 it keeps 16
 * fractional bits, above 1.0 only 8, and exactly 1.0 is stored as the exact unity 1<<0. The
 * constants are the exe's doubles at 0x0045a000 (1.0), 0x0045a008 (65536.0), 0x0045a010 (256.0).
 * Together with GetWaveOutHdrBaseGain it computes the gain descriptor that
 * ConvertAudioBlockToOutputFormat multiplies every sample by; while both were empty stubs the
 * descriptor stayed zero and the engine played silence.
 *
 * Written out three times, as the exe has it (the copies differ in store order). The operands are
 * struct fields: with plain pointers this compiler evaluates the third parameter's quotient first,
 * with struct pointers the second, and the exe does the second in the first block. */
int __cdecl MultiplyGainTriples(GainTriple *pOut, GainTriple *pFmt,
                                              GainTriple *pScr) /* 0x00402120 */
{
    double g;

    g = ((double)pScr->a.value / (double)(1 << pScr->a.shift)) *
        ((double)pFmt->a.value / (double)(1 << pFmt->a.shift));
    DF_TRACE("gain0: %d/2^%d * %d/2^%d -> %d/1000", pFmt->a.value, pFmt->a.shift,
             pScr->a.value, pScr->a.shift, (int)(g * 1000.0));
    if (g < 1.0) {
        pOut->a.value = (int)(g * 65536.0);
        pOut->a.shift = 0x10;
    } else if (g == 1.0) {
        pOut->a.shift = 0;
        pOut->a.value = 1;
    } else {
        pOut->a.value = (int)(g * 256.0);
        pOut->a.shift = 8;
    }

    g = ((double)pFmt->b.value / (double)(1 << pFmt->b.shift)) *
        ((double)pScr->b.value / (double)(1 << pScr->b.shift));
    DF_TRACE("gain1: -> %d/1000", (int)(g * 1000.0));
    if (g < 1.0) {
        pOut->b.value = (int)(g * 65536.0);
        pOut->b.shift = 0x10;
    } else if (g == 1.0) {
        pOut->b.value = 1;
        pOut->b.shift = 0;
    } else {
        pOut->b.value = (int)(g * 256.0);
        pOut->b.shift = 8;
    }

    g = ((double)pFmt->c.value / (double)(1 << pFmt->c.shift)) *
        ((double)pScr->c.value / (double)(1 << pScr->c.shift));
    DF_TRACE("gain2: -> %d/1000", (int)(g * 1000.0));
    if (g < 1.0) {
        pOut->c.value = (int)(g * 65536.0);
        pOut->c.shift = 0x10;
    } else if (g == 1.0) {
        pOut->c.value = 1;
        pOut->c.shift = 0;
    } else {
        pOut->c.value = (int)(g * 256.0);
        pOut->c.shift = 8;
    }
    return 0;
}


/* Adds a sound buffer descriptor {id, bufSize, flags, callback, context} (5 ints) to
 * gSoundBufferDescs and bumps gSoundBufferDescCount. Returns 0x387 if the id is already
 * registered, 0 otherwise. */
unsigned int __cdecl RegisterSoundCodec(int *pDesc)  /* 0x004022f0 */
{
    typedef struct { int d[5]; } SndBufRec;   /* one record, copied whole (rep movsd 5) */
    int  i;

    /* gSoundBufferDescs is int[30]: 6 records of 5 ints (see src/data/globals.c) */
    for (i = 0; i < gSoundBufferDescCount; i++) {
        if (((SndBufRec *)gSoundBufferDescs)[i].d[0] == *pDesc)
            return 0x387;       /* duplicate ID */
    }

    ((SndBufRec *)gSoundBufferDescs)[gSoundBufferDescCount] = *(SndBufRec *)pDesc;
    gSoundBufferDescCount++;
    return 0;
}


/* Acquires the sound critical section (gSoundMixerLock) and sets gSoundMixerLocked. Used by
 * ServiceSoundMixer to gate hardware audio state access; counterpart of UnlockSoundMixer. */
int __stdcall LockSoundMixer(void)  /* 0x00402350 */
{
    EnterCriticalSection((LPCRITICAL_SECTION)&gSoundMixerLock);
    gSoundMixerLocked = 1;
    return 0;
}


/* Releases the sound critical section. If locked: pushes the sound heap context, queries the
 * mix-buffer level (GetHeapFreeBytes) and flushes the wave buffers when it is below 0x10000,
 * submits the output position, pops the context, clears the busy flag and leaves the critical
 * section. Returns 0, or 0x385 if the mixer was not locked. */
int __cdecl UnlockSoundMixer(unsigned int pos)  /* 0x00402370 */
{
    int i;

    if (gSoundMixerLocked == 0)
        return 0x385;
    PushHeapContext(&gSoundHeap);
    if (GetHeapFreeBytes() < 0x10000) {
        for (i = 0; i < gMixBufferCount; i++)
            DrainWaveBufferToMixBudget(i);
        ClearAllSoundChannelPendingDescriptors();
        PumpSoundMixer(0);   /* two calls: the exe pushes 0 here */
    } else
        PumpSoundMixer(pos);
    PopHeapContext();
    gSoundMixerLocked = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)&gSoundMixerLock);
    return 0;
}
/* The wave-out drain / tick after UnlockSoundMixer. The device's position callback
 * (gWaveDeviceOps[3]) says how many frames are still queued; minus what real elapsed time has
 * already played of the current buffer, that is real, time-aware backpressure. The silence path
 * (gMixSilenced != 0) just keeps the device fed. The live path drives the per-channel feed,
 * computes the submit count from the backpressure (or flushes gMixQueueTarget buffers when
 * flushRequest names a flush), mixes into the dequeued wave buffers and keeps the running latency
 * averages. Fatal 0x13b if frames were left over. The dequeued buffer pointer (pDst) is written by
 * gWaveDeviceOps[0] (DequeueWaveOutBuffer), never initialised here.
 *
 * Byte match: the silence branch is laid out first, both branches call the position callback
 * themselves, the shift counts carry no "& 0x1f" (Ghidra's rendering of x86 shifts), the submit
 * count is an if/else and the mix remainder is computed once. */
void __cdecl PumpSoundMixer(uint flushRequest)  /* 0x004023f0 */
{
    typedef void     (__cdecl *WaveOp)(void);
    typedef void     (__cdecl *WaveOp3)(int *);
    typedef uint32_t (__cdecl *WaveOp0)(uint32_t *, int);
#define DISP ((WaveOp *)(uintptr_t)(unsigned int)gWaveDeviceOps)
    int   bufIdx;
    int   arrOffset;
    int   framesBefore;
    uint  flushNow;
    int   framesQueued;  /* local_c */
    char *pDst;
    int   framesAfter;  /* iStack_4 */

    if (gMixSilenced != 0) {
        ((WaveOp3)DISP[3])(&framesQueued);
        framesBefore = 0;
        while (framesQueued <= gMixChunkFrames * gMixQueueTarget) {
            framesQueued = framesQueued + gMixChunkFrames;
            framesBefore++;
        }
        framesBefore--;
        if (framesBefore != 0) {
            do {
                ((WaveOp0)DISP[0])((uint32_t *)&pDst, 0);
                FillWaveSilence((void *)pDst, gMixChunkFrames);
                DISP[1]();
                framesBefore--;
            } while (framesBefore != 0);
        }
    } else {
        ((WaveOp3)DISP[3])(&framesQueued);
        framesBefore = framesQueued;
        flushNow = flushRequest & (uint)gMixFlushMask;
        if (flushNow != 0) {
            if (gWaveFlushLatencyCount != 0) {
                framesQueued = framesQueued - gWaveFlushLatencySum / gWaveFlushLatencyCount;
                if ((gWaveFeedLatencyCount != 0) && (gWaveBindsPending != 0)) {
                    framesQueued = framesQueued - (gWaveFeedLatencySum / gWaveFeedLatencyCount) * gWaveBindsPending;
                }
                if (framesQueued < 0)
                    framesQueued = 0;
            }
            gMixFramesPending = gMixFramesPending + framesQueued;
            gMixRingReadCursor = gMixRingReadCursor - (framesQueued << gMixFrameShift);
            if ((uint)gMixRingReadCursor < (uint)gMixRingBase)
                gMixRingReadCursor = gMixRingReadCursor + 0x8010;
            gMixFramesQueuedTotal = gMixFramesQueuedTotal - framesQueued;
        }
        if (gWaveBindsPending != 0) {
            bufIdx = 0;
            if (0 < gMixBufferCount) {
                arrOffset = 0;
                do {
                    if (*(int *)((char *)&gWaveBufferActive + arrOffset) != 0) {
                        ConsumePlayedFramesFromWaveHdrQueue(bufIdx, gMixFramesPending);
                        if (*(int *)((char *)&gWaveBufferLoopGatePending + arrOffset) != 0) {
                            SetWaveHdrRecycleFlag(bufIdx, (int)(0 < *(int *)((char *)&gWaveBufferLoopGatePending + arrOffset)));
                            *(int *)((char *)&gWaveBufferLoopGatePending + arrOffset) = 0;
                        }
                        RecycleOrTeardownWaveHdrChain(bufIdx);
                        AppendPendingDescriptorsToWaveHdrQueue(bufIdx);
                        ClearSoundChannelPendingDescriptors(bufIdx);
                        RenderSoundChannelToMixRing(bufIdx, gMixFramesPending);
                    }
                    arrOffset = arrOffset + 4;
                    bufIdx = bufIdx + 1;
                } while (bufIdx < gMixBufferCount);
            }
            if (flushNow != 0) {
                ((WaveOp3)DISP[3])(&framesAfter);
                gWaveFeedLatencySum = gWaveFeedLatencySum + (framesBefore - framesAfter);
                gWaveFeedLatencyCount = gWaveFeedLatencyCount + gWaveBindsPending;
                framesBefore        = framesAfter;
            }
            gWaveBindsPending = 0;
        }
        bufIdx = 0;
        if (0 < gMixBufferCount) {
            do {
                if (gWaveBufferLoopGatePending[bufIdx] != 0) {
                    SetWaveHdrRecycleFlag(bufIdx, (int)(0 < (int)gWaveBufferLoopGatePending[bufIdx]));
                    gWaveBufferLoopGatePending[bufIdx] = 0;
                }
                bufIdx++;
            } while (bufIdx < gMixBufferCount);
        }
        if (flushNow != 0) {
            bufIdx = gMixQueueTarget;       /* a flush submits every buffer */
        } else {
            bufIdx = 0;
            while (framesQueued <= gMixChunkFrames * gMixQueueTarget) {
                framesQueued = framesQueued + gMixChunkFrames;
                bufIdx++;
            }
            bufIdx--;
        }
        DF_TRACE("submit count = %d (queued=%d chunk=%d nBufs=%d)", bufIdx,
                 framesQueued, (int)gMixChunkFrames, (int)gMixQueueTarget);
        if (0 < bufIdx) {
            AdvanceWaveMixTick(bufIdx * gMixChunkFrames);
            for (; bufIdx != 0; bufIdx--) {
                /* The flush flag is DequeueWaveOutBuffer's second argument (0x00402674 pushes EDI):
                 * it sets gWaveOutResetPending, so SubmitWaveOutBuffer resets the device first and
                 * completes the buffers in flight. A hardcoded 0 piled the flush onto the queued
                 * buffers: fatal 0x1ad, "Too many sound buffers playing". */
                ((WaveOp0)DISP[0])((uint32_t *)&pDst, (int)flushNow);
                arrOffset = (int)(gMixRingEnd - gMixRingReadCursor) >> gMixFrameShift;
                if (arrOffset > gMixFramesPending)
                    arrOffset = gMixFramesPending;
                if (arrOffset > gMixChunkFrames)
                    arrOffset = gMixChunkFrames;
                CopyMixBufferSegment((void *)pDst, arrOffset);
                pDst = pDst + (arrOffset << gMixFrameShift);
                arrOffset = gMixChunkFrames - arrOffset;
                if (arrOffset != 0)
                    CopyMixBufferSegment((void *)pDst, arrOffset);
                if (flushNow != 0) {
                    ((WaveOp3)DISP[3])(&framesAfter);
                    gWaveFlushLatencyCount = gWaveFlushLatencyCount + 1;
                    gWaveFlushLatencySum = gWaveFlushLatencySum + (framesBefore - framesAfter);
                    flushNow = 0;
                }
                DISP[1]();
            }
        }
        if (gWaveFlushLatencyCount >= 0x80) {
            gWaveFlushLatencySum = (gWaveFlushLatencySum << 5) / gWaveFlushLatencyCount;
            gWaveFlushLatencyCount = 0x20;
        }
        if (gWaveFeedLatencyCount >= 0x80) {
            gWaveFeedLatencySum = (gWaveFeedLatencySum << 5) / gWaveFeedLatencyCount;
            gWaveFeedLatencyCount = 0x20;
        }
        if (gWaveFeedLatencySum < 0)
            gWaveFeedLatencySum = 0;
        if (gWaveFlushLatencySum < 0)
            gWaveFlushLatencySum = 0;
        if (gMixFramesPending != 0)
            FatalErrorWithCode(0x13b);
    }
#undef DISP
}


/* Fills nFrames of pDst with silence: 0x80 (the unsigned 8-bit midpoint) when gMixBytesPerSample
 * is 1, 0x00 otherwise. Writes 4 bytes at a time using the per-frame shift (gMixFrameShift), then
 * the tail bytes. */
void __cdecl FillWaveSilence(void *pDst, int nFrames)  /* 0x004027C0 */
{
    /* post-decrement loops (mov eax,ecx / dec ecx / test eax,eax); the shift
     * count is the bare int (Ghidra's `& 0x1f` is the x86 masking) */
    int       nBytes;
    int       nWords;
    int       nTail;
    uint32_t *p;
    byte     *pb;

    nBytes = nFrames << gMixFrameShift;
    nWords = nBytes >> 2;
    nTail  = nBytes & 3;

    if (gMixBytesPerSample == 1) {
        p = (uint32_t *)pDst;
        while (nWords--)
            *p++ = 0x80808080;
        pb = (byte *)p;
        while (nTail--)
            *pb++ = 0x80;
    } else {
        p = (uint32_t *)pDst;
        while (nWords--)
            *p++ = 0;
        pb = (byte *)p;
        while (nTail--)
            *pb++ = 0;
    }
}


/* Advances the queued wave headers of buffer bufIdx toward the play cursor byteBudget, calling
 * AdvanceWaveHdrQueuedBytes / CommitWaveHdrPendingBytesToQueued on the WAVEHDR body at node + 1. */
void __cdecl ConsumePlayedFramesFromWaveHdrQueue(int bufIdx, int byteBudget)  /* 0x00402840 */
{
    int   nodeChanged;
    int   recycleStarted;
    int   consumed;
    int  *node;
    WaveHdrQueueRow *qb;
    int   rem;   /* the budget walked in a callee-saved register (ebx) */

    nodeChanged  = 0;
    qb     = WaveHdrQueueBase(bufIdx);
    rem    = byteBudget;
    node = *(int **)&qb->cur;
    if ((((qb->recycleMode == 0) || (*(int **)&qb->recycleTarget != node)) ||
         (*(int *)((char *)node + 0x16e) != 0)) ||
        ((*(int *)((char *)node + 0x172) != 0 || (*node != 0)))) {
        recycleStarted = 0;
        for (; 0 < rem; rem = rem - consumed) {
            consumed = AdvanceWaveHdrQueuedBytes((int)(node + 1), rem);
            if (consumed != rem) {
                if ((qb->recycleMode == 0) && (*(int *)((char *)node + 0x172) == 0)) {
                    qb->recycleMode = 2;
                    recycleStarted                = 1;
                    *(int **)&qb->recycleTarget = node;
                }
                if ((qb->recycleMode <= 1) || (*(int **)&qb->recycleTarget != node)) {
                    node = *(int **)((char *)node + 0x172);
                } else {
                    if (*(int **)&qb->tail != node) {
                        do {
                            CommitWaveHdrPendingBytesToQueued((int)(node + 1));
                            node = *(int **)((char *)node + 0x16e);
                        } while (*(int **)&qb->tail != node);
                    }
                    CommitWaveHdrPendingBytesToQueued((int)(node + 1));
                }
                if (node == (int *)0) {
                    if (qb->recycleMode == 0) {
                        FatalErrorWithCode(0x137);
                    } else {
                        node = *(int **)&qb->head;
                        if (*(int **)&qb->tail != node) {
                            do {
                                CommitWaveHdrPendingBytesToQueued((int)(node + 1));
                                node = *(int **)((char *)node + 0x16e);
                            } while (*(int **)&qb->tail != node);
                        }
                        CommitWaveHdrPendingBytesToQueued((int)(node + 1));
                    }
                }
                nodeChanged = 1;
                *(int **)&qb->cur = node;
            }
        }
        if (nodeChanged != 0)
            ApplySoundChannelGain(bufIdx, (void *)0);
        if (recycleStarted != 0) {
            qb->recycleMode = 0;
            qb->recycleTarget = 0;
        }
    }
}


/* Consumes up to maxBytes from the WAVEHDR shadow queue at hdrPlus4 (the dword after the
 * queue-root pointer) and returns the number consumed. */
static int __cdecl AdvanceWaveHdrQueuedBytes(int hdrPlus4, int maxBytes)  /* 0x004029a0 */
{
    int *p = (int *)hdrPlus4;
    int  queued = p[1];

    /* the budget case is the fall-through (cmp max,queued / jg) */
    if (maxBytes <= queued) {
        queued -= maxBytes;
        p[2] += maxBytes;
        p[1] = queued;
        return maxBytes;
    }
    maxBytes = queued;
    p[2] += maxBytes;
    p[1] = 0;
    return maxBytes;
}


/* Flushes the accumulated pending bytes (+8) back into the queued-byte budget (+4). */
static void __cdecl CommitWaveHdrPendingBytesToQueued(int hdrPlus4)  /* 0x004029d0 */
{
    int pend;

    pend = *(int *)(hdrPlus4 + 8);
    *(int *)(hdrPlus4 + 8) = 0;
    *(int *)(hdrPlus4 + 4) = *(int *)(hdrPlus4 + 4) + pend;
}


/* If recycling is active, frees and reallocates the header chain; otherwise walks and tears down
 * the old entries. */
void __cdecl RecycleOrTeardownWaveHdrChain(int bufIdx)  /* 0x004029F0 */
{
  WaveHdrQueueRow *qb;     /* the queue row, formed once (edi) */
  int oldNode;
  int curNode;

  qb = WaveHdrQueueBase(bufIdx);
  if (qb->recycleMode != 0) {
    FreeWaveOutHeaderChainForBufferIdx(bufIdx);
    AllocInitWaveOutHeaderForBufferIdx(bufIdx);
    qb->recycleMode = 1;
    qb->recycleTarget = qb->head;
    return;
  }
  oldNode = qb->tail;
  curNode = qb->cur;
  while (curNode != oldNode) {
    qb->tail = *(uint32_t *)(oldNode + 0x172);
    FreeWaveOutHdrDecodeSlots((int *)(oldNode + 4));
    FreeEngineBuffer(oldNode);
    oldNode = qb->tail;
    if (oldNode == 0)
      FatalErrorWithCode(0x138);
  }
  *(uint32_t *)(oldNode + 0x16e) = 0;
  ResetWaveHdrChannelPendingCount(oldNode + 4);
}


/* Zeroes a wave header channel's pending byte count (channel + 8). */
void __cdecl ResetWaveHdrChannelPendingCount(int pChannel)  /* 0x00402A90 */
{
  *(uint32_t *)(pChannel + 8) = 0;
}


/* Sets or clears the recycle flag of bufIdx's queue row, acting only when the flag changes.
 * Enabling points the recycle target at the buffer's anchor row (gWaveHdrQueues + bufIdx * 0x32)
 * if it is NULL; disabling clears both. */
void __cdecl SetWaveHdrRecycleFlag(int bufIdx, int flag)  /* 0x00402AA0 */
{
    WaveHdrQueueRow *row = (WaveHdrQueueRow *)((char *)&gWaveHdrQueues + bufIdx * 0x32);   /* one 0x32-byte device row; fields at +0x2a / +0x2e */

    if (((row->recycleMode == 0) || (flag == 0)) &&
        ((row->recycleMode != 0) || (flag != 0))) {
        row->recycleMode = flag;
        if (flag != 0) {                /* test / je: the set case first */
            if (row->recycleTarget == 0)
                row->recycleTarget = row->head;
        } else {
            row->recycleTarget = 0;
        }
    }
}


/* Advances the mix playback position by a frame delta, handling buffer wraparound and calling the
 * frame processing callbacks of each active channel. */
void __cdecl AdvanceWaveMixTick(int targetFrames)  /* 0x00402AF0 */
{
  int newFrames;
  int runFrames;
  int bufIdx;
  int bufOffset;

  /* Shift counts are the bare int (the exe loads cl from it; Ghidra's `& 0x1f`
   * is the x86 masking, not source). The buffer cursors are compared unsigned
   * (jb): they are addresses. The buffer address is formed inline in the
   * call (the exe pushes the count first). */
  newFrames = targetFrames - gMixFramesPending;
  if (0 < newFrames) {
    gMixRingFillFrames = gMixRingFillFrames + newFrames;
    gMixFramesPending = gMixFramesPending + newFrames;
    runFrames = (int)(gMixRingEnd - gMixRingWriteCursor) >> gMixFrameShift;
    if (runFrames > newFrames) {
      runFrames = newFrames;
    }
    bufIdx = 0;
    if (0 < gMixBufferCount) {
      bufOffset = 0;
      do {
        RenderWaveHdrQueueToMixBuffer((char *)(gMixRingWriteCursor + bufOffset), bufIdx, runFrames);
        bufOffset = bufOffset + 0x8010;
        DrainWaveBufferToMixBudget(bufIdx);
        bufIdx = bufIdx + 1;
      } while (bufIdx < gMixBufferCount);
    }
    gMixRingWriteCursor = gMixRingWriteCursor + (runFrames << gMixFrameShift);
    gMixFramesQueuedTotal = gMixFramesQueuedTotal + runFrames;
    if ((unsigned int)gMixRingWriteCursor >= (unsigned int)gMixRingEnd) {
      gMixRingWriteCursor = gMixRingBase;
    }
    newFrames = newFrames - runFrames;
    if (0 < newFrames) {
      bufIdx = 0;
      if (0 < gMixBufferCount) {
        bufOffset = 0;
        do {
          RenderWaveHdrQueueToMixBuffer((char *)(gMixRingWriteCursor + bufOffset), bufIdx, newFrames);
          bufOffset = bufOffset + 0x8010;
          DrainWaveBufferToMixBudget(bufIdx);
          bufIdx = bufIdx + 1;
        } while (bufIdx < gMixBufferCount);
      }
      gMixFramesQueuedTotal = gMixFramesQueuedTotal + newFrames;
      gMixRingWriteCursor = gMixRingWriteCursor + (newFrames << gMixFrameShift);
    }
    newFrames = gMixRingFillFrames - (0x8010 >> gMixFrameShift);
    if (0 < newFrames) {
      gMixRingFillFrames = gMixRingFillFrames - newFrames;
      gMixRingTailCursor = gMixRingTailCursor + (newFrames << gMixFrameShift);
      if ((unsigned int)gMixRingTailCursor >= (unsigned int)gMixRingEnd) {
        gMixRingTailCursor = gMixRingTailCursor - 0x8010;
      }
    }
  }
  return;
}
/* Writes nFrames PCM frames into the wave-out queue for bufIdx. Each buffer is a 0x32-byte row of
 * gWaveHdrQueues (+0 recycle chain head, +4 / +8 chain tail and current node, +0xc write
 * position, +0x2a recycle mode, +0x2e recycle target) holding a linked list of 0x176-byte nodes.
 * When the current node is full it moves to a fresh node, the recycle slot, or the stale tail torn
 * down back to the recycle target. */
void __cdecl RenderWaveHdrQueueToMixBuffer(char *pSrc, int bufIdx, int nFrames)  /* 0x00402C30 */
{
    int       *pNextLink;
    int        iNew;
    uint32_t   uNew;
    WaveHdrQueueRow *pRow;
    uint32_t  *puCur;
    uint32_t  *puNext;
    int        iConsumed;
    uint32_t   bHead;

    pRow  = (WaveHdrQueueRow *)((char *)gWaveHdrQueues + bufIdx * 0x32);
    puCur = *(uint32_t **)&pRow->cur;
    if (0 < nFrames) {
        while (1) {
            pNextLink    = (int *)((int)puCur + 0x16e);
            bHead     = (uint32_t)(*pNextLink == 0);
            iConsumed = RenderWaveHdrFrames(puCur + 1, pSrc, nFrames, bHead, (int *)&pRow->gainA);
            if (nFrames != iConsumed) {
                if (*pNextLink == 0) {
                    if (pRow->recycleMode != 0) {
                        if (pRow->recycleMode == 1) {
                            /* the recycle slot */
                            pRow->recycleMode = 2;
                            puCur = *(uint32_t **)&pRow->recycleTarget;
                        } else {
                            /* tear the stale tail down to the recycle target */
                            puCur = *(uint32_t **)&pRow->head;
                            if (*(uint32_t **)&pRow->recycleTarget != puCur) {
                                do {
                                    puNext = *(uint32_t **)((int)puCur + 0x16e);
                                    FreeWaveOutHdrDecodeSlots(puCur + 1);
                                    FreeEngineBuffer((int)puCur);
                                    puCur = puNext;
                                } while (*(uint32_t **)&pRow->recycleTarget != puCur);
                            }
                            *(uint32_t **)&pRow->head = puCur;
                            *(uint32_t *)((int)puCur + 0x172) = 0;
                        }
                    } else {
                        /* a fresh node */
                        iNew    = AllocEngineBuffer(0x176);
                        *pNextLink = iNew;
                        *(uint32_t **)(iNew + 0x172) = puCur;
                        puCur   = (uint32_t *)*pNextLink;
                        *(uint32_t **)&pRow->cur = puCur;
                        *(uint32_t **)&pRow->tail = puCur;
                        *(uint32_t *)((int)puCur + 0x16e) = 0;
                        uNew    = InitWaveOutHdrPayload(puCur + 1, (int *)0x0);
                        *puCur  = uNew;
                        pRow->recycleMode = 0;
                        pRow->recycleTarget = 0;
                    }
                } else {
                    puCur = (uint32_t *)*pNextLink;
                }
                RewindWaveHdrNode(puCur + 1);
                ApplySoundChannelGain(bufIdx, (void *)0);
            }
            nFrames -= iConsumed;
            if (nFrames <= 0)
                break;
            pSrc  += iConsumed << gMixFrameShift;
        }
        *(uint32_t **)&pRow->cur = puCur;
    }
}


/* Copies up to nFrames of audio from pSrc into the wave header node at pHdr, by node type: 0 =
 * silence fill, 1 = DecodeMediaFrameSegmentDirect, 2 = DecodeMediaFrameSegmentBuffered. Returns
 * the number of frames consumed. */
int __cdecl RenderWaveHdrFrames(uint32_t *pHdr, char *pSrc, int nFrames,
                                      uint32_t bIsHead, int *pAnchor)  /* 0x00402D70 */
{
    int iConsumed;
    int iLimit;

    if ((bIsHead != 0) && (*pHdr == 0) && ((int)pHdr[2] < nFrames))
        pHdr[2] = (uint32_t)nFrames;

    iLimit = (int)pHdr[2];
    if (nFrames < iLimit)
        iLimit = nFrames;

    if (iLimit != 0) {
        iConsumed = (int)*pHdr;
        DF_TRACE("waveHdr node type=%d frames=%d", iConsumed, iLimit);
        switch (iConsumed) {
        case 0:
            FillWaveSilence((uint32_t *)pSrc, iLimit);
            break;
        case 1:
            DecodeMediaFrameSegmentDirect((int *)(pHdr + 3), (int)pHdr[1], iLimit, pSrc, pAnchor);
            break;
        case 2:
            DecodeMediaFrameSegmentBuffered((int *)(pHdr + 3), (int)pHdr[1], iLimit, pSrc, pAnchor);
            break;
        }
        pHdr[1] = pHdr[1] + (uint32_t)iLimit;
        pHdr[2] = pHdr[2] - (uint32_t)iLimit;
    }
    return iLimit;
}


/* The type 1 (direct-mapped) wave header decoder. Computes the source pointer from the stride,
 * base and shift in pState, ((iPos / stride) << shift) + base, and hands nFrames / stride rows to
 * ConvertAudioBlockToOutputFormat. A negative stride multiplies the position by |stride|. */
void __cdecl DecodeMediaFrameSegmentDirect(int *pState, int iPos, int nFrames,
                                           char *pDst, int *pAnchor)  /* 0x00402E30 */
{
    int iStride;
    int iRows;
    byte *pBlock;

    iStride = *pState;
    if (0 < iStride) {
        pBlock = (byte *)((iPos / iStride << *(byte *)(pState + 1)) + pState[2]);
        iRows = nFrames / iStride;
        if (nFrames % iStride != 0)
            iRows++;
        ConvertAudioBlockToOutputFormat(
            pBlock,
            (uint32_t)pState[3], pState[4], pState[5],
            pDst, gMixSampleRate, gMixBytesPerSample, gMixSampleChannels,
            iRows, pAnchor, iStride);
    } else {
        pBlock = (byte *)(pState[2] - (iStride * iPos << *(byte *)(pState + 1)));
        ConvertAudioBlockToOutputFormat(
            pBlock,
            (uint32_t)pState[3], pState[4], pState[5],
            pDst, gMixSampleRate, gMixBytesPerSample, gMixSampleChannels,
            nFrames, pAnchor, iStride);
    }
}


#pragma pack(push, 1)
typedef struct { int value; char shift; } FG2;
typedef struct { FG2 a; FG2 b; FG2 c; } GT2;
#pragma pack(pop)
/* The audio sample converter: resamples one decoded block into the wave device's output format
 * (rate ratio, channel count, bits per sample) and applies the per-buffer gain. The nested
 * switches are format combinations, not image layouts. Ratio k > 0 repeats each frame k times,
 * -k averages k frames; all arithmetic is signed (the exe uses SAR throughout).
 * */
void __cdecl ConvertAudioBlockToOutputFormat(byte *pSrc, uint32_t srcRate, int srcBytes, int srcChans, char *pDst,
            uint32_t dstRate, int dstBytes, int dstChans, int nFrames, int *pGain, uint32_t ratio
            ) /* 0x00402EF0 */
{
    int        r;
    int        v;
    int        l;
    GT2        g;
    int        s;
    int        ol;
    int        or_;
    int        o;
    int        x;         /* memory-only scratch: slots (1ch->2ch, 8->8 bit) ratio 1/2/4 reach it
                             through a volatile lvalue, as the exe's frame slot shows */

    g = *(GT2 *)pGain;
    if (srcChans == 1) {
        if (dstChans == 1) {
            if (srcBytes == 1) {
                if (dstBytes == 1) {
                    switch ((int)ratio) {
                    case -4: {
                        while (nFrames != 0) {
                            v = *pSrc++;
                            o = *pSrc++ - 0x80;
                            v = v - 0x80 + o;
                            o = *pSrc++;
                            v += o - 0x80;
                            o = *pSrc++;
                            v += o - 0x80;
                            v >>= 2;
                            v = (g.c.value * v >> g.c.shift) + 0x80;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    }
                    case -2:
                        while (nFrames != 0) {
                            v = *pSrc++;
                            o = *pSrc++;
                            v = v - 0x80 + o - 0x80;
                            v >>= 1;
                            v = (g.c.value * v >> g.c.shift) + 0x80;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            v = *pSrc++ - 0x80;
                            v = (g.c.value * v >> g.c.shift) + 0x80;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    case 2:
                        while (nFrames != 0) {
                            v = (g.c.value * (*pSrc++ - 0x80) >> g.c.shift) + 0x80;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    case 4: {
                        while (nFrames != 0) {
                            v = *pSrc++ - 0x80;
                            v = (g.c.value * v >> g.c.shift) + 0x80;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    }
                    }
                } else {
                    switch ((int)ratio) {
                    case -4: {
                        while (nFrames != 0) {
                            o = *pSrc++;
                            o -= 0x80;
                            o <<= 8;
                            s = (*pSrc++ - 0x80) << 8;
                            o += s;
                            s = (*pSrc++ - 0x80) << 8;
                            o += s;
                            s = (*pSrc++ - 0x80) << 8;
                            o += s;
                            o >>= 2;
                            o = g.c.value * o >> g.c.shift;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                    case -2:
                        while (nFrames != 0) {
                            s = (*pSrc++ - 0x80) << 8;
                            s += (*pSrc++ - 0x80) << 8;
                            s >>= 1;
                            s = g.c.value * s >> g.c.shift;
                            *(short *)pDst = (short)s;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            v = (*pSrc++ - 0x80) << 8;
                            v = g.c.value * v >> g.c.shift;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 2:
                        while (nFrames != 0) {
                            v = (*pSrc++ - 0x80) << 8;
                            v = g.c.value * v >> g.c.shift;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 4:
                        while (nFrames != 0) {
                            v = (*pSrc++ - 0x80) << 8;
                            v = g.c.value * v >> g.c.shift;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                }
            } else {
                if (dstBytes == 1) {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            v = *(short *)pSrc;
                            pSrc += 2;
                            o = *(short *)pSrc;
                            pSrc += 2;
                            v += o;
                            o = *(short *)pSrc;
                            pSrc += 2;
                            v += o;
                            o = *(short *)pSrc;
                            pSrc += 2;
                            v += o;
                            v >>= 2;
                            v = ((g.c.value * v >> g.c.shift) >> 8) + 0x80;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    case -2: {
                        while (nFrames != 0) {
                            v = *(short *)pSrc;
                            pSrc += 2;
                            o = *(short *)pSrc;
                            pSrc += 2;
                            v += o;
                            v >>= 1;
                            v = ((g.c.value * v >> g.c.shift) >> 8) + 0x80;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    }
                    case 1: {
                        while (nFrames != 0) {
                            v = *(short *)pSrc; pSrc += 2;
                            v = ((g.c.value * v >> g.c.shift) >> 8) + 0x80;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    }
                    case 2: {
                        while (nFrames != 0) {
                            v = ((g.c.value * *(short *)pSrc >> g.c.shift) >> 8) + 0x80;
                            pSrc += 2;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    }
                    case 4:
                        while (nFrames != 0) {
                            v = *(short *)pSrc; pSrc += 2;
                            v = ((g.c.value * v >> g.c.shift) >> 8) + 0x80;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    }
                } else {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            v = *(short *)pSrc;
                            pSrc += 2;
                            o = *(short *)pSrc;
                            pSrc += 2;
                            v += o;
                            o = *(short *)pSrc;
                            pSrc += 2;
                            v += o;
                            o = *(short *)pSrc;
                            pSrc += 2;
                            v += o;
                            v >>= 2;
                            v = g.c.value * v >> g.c.shift;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case -2:
                        while (nFrames != 0) {
                            v = *(short *)pSrc;
                            pSrc += 2;
                            o = *(short *)pSrc;
                            pSrc += 2;
                            v += o;
                            v >>= 1;
                            v = g.c.value * v >> g.c.shift;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            v = g.c.value * (*(short *)pSrc) >> g.c.shift;
                            pSrc += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 2:
                        while (nFrames != 0) {
                            v = *(short *)pSrc;
                            pSrc += 2;
                            v = g.c.value * v >> g.c.shift;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 4:
                        while (nFrames != 0) {
                            v = *(short *)pSrc; pSrc += 2;
                            v = g.c.value * v >> g.c.shift;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                }
            }
        } else {
            if (srcBytes == 1) {
                if (dstBytes == 1) {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            l = *pSrc++ - 0x80;
                            r = *pSrc++ - 0x80;
                            r += l;
                            o = *pSrc++ - 0x80;
                            r += o;
                            o = *pSrc++ - 0x80;
                            r += o;
                            r >>= 2;
                            l = r;
                            r = g.a.value * r;
                            r >>= g.a.shift;
                            l = g.b.value * l >> g.b.shift;
                            r += 0x80;
                            l += 0x80;
                            *pDst++ = (char)r;
                            *pDst++ = (char)l;
                            nFrames--;
                        }
                        break;
                    case -2:
                        while (nFrames != 0) {
                            o = *pSrc++ - 0x80;
                            l = *pSrc++ - 0x80;
                            l += o;
                            l >>= 1;
                            s = l;
                            l = g.a.value * l >> g.a.shift;
                            s = g.b.value * s >> g.b.shift;
                            l += 0x80;
                            s += 0x80;
                            *pDst++ = (char)l;
                            *pDst++ = (char)s;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            v = *pSrc++ - 0x80;
                            /* the exe keeps this product in its frame slot and shifts it there */
                            *(volatile int *)&x = g.a.value * v;
                            *(volatile int *)&x >>= g.a.shift;
                            l = g.b.value * v >> g.b.shift;
                            v = *(volatile int *)&x + 0x80;
                            l += 0x80;
                            *pDst++ = (char)v;
                            *pDst++ = (char)l;
                            nFrames--;
                        }
                        break;
                    case 2:
                        while (nFrames != 0) {
                            v = *pSrc++ - 0x80;
                            *(volatile int *)&x = v;
                            l = g.a.value * v >> g.a.shift;
                            v = g.b.value * *(volatile int *)&x >> g.b.shift;
                            l += 0x80;
                            v += 0x80;
                            *pDst++ = (char)l;
                            *pDst++ = (char)v;
                            *pDst++ = (char)l;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    case 4:
                        while (nFrames != 0) {
                            v = *pSrc++ - 0x80;
                            *(volatile int *)&x = v;
                            l = g.a.value * v >> g.a.shift;
                            v = g.b.value * *(volatile int *)&x >> g.b.shift;
                            l += 0x80;
                            v += 0x80;
                            *pDst++ = (char)l;
                            *pDst++ = (char)v;
                            *pDst++ = (char)l;
                            *pDst++ = (char)v;
                            *pDst++ = (char)l;
                            *pDst++ = (char)v;
                            *pDst++ = (char)l;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    }
                } else {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            s = *pSrc++;
                            s -= 0x80;
                            s <<= 8;
                            s += (*pSrc++ - 0x80) << 8;
                            s += (*pSrc++ - 0x80) << 8;
                            s += (*pSrc++ - 0x80) << 8;
                            s >>= 2;
                            l = g.a.value * s >> g.a.shift;
                            r = g.b.value * s >> g.b.shift;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            *(short *)pDst = (short)r;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case -2:
                        while (nFrames != 0) {
                            v = *pSrc++;
                            v -= 0x80;
                            v <<= 8;
                            v += (*pSrc++ - 0x80) << 8;
                            v >>= 1;
                            o = g.a.value * v >> g.a.shift;
                            v = g.b.value * v;
                            v >>= g.b.shift;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            v = (*pSrc++ - 0x80) << 8;
                            l = g.a.value * v >> g.a.shift;
                            v = g.b.value * v >> g.b.shift;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 2:
                        while (nFrames != 0) {
                            o = (*pSrc++ - 0x80) << 8;
                            l = g.a.value * o >> g.a.shift;
                            r = g.b.value * o >> g.b.shift;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            *(short *)pDst = (short)r;
                            pDst += 2;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            *(short *)pDst = (short)r;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 4:
                        while (nFrames != 0) {
                            s = (*pSrc++ - 0x80) << 8;
                            r = s;
                            s = g.a.value * s >> g.a.shift;
                            o = g.b.value * r >> g.b.shift;
                            *(short *)pDst = (short)s;
                            pDst += 2;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            *(short *)pDst = (short)s;
                            pDst += 2;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            *(short *)pDst = (short)s;
                            pDst += 2;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            *(short *)pDst = (short)s;
                            pDst += 2;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                }
            } else {
                if (dstBytes == 1) {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            l += *(short *)pSrc; pSrc += 2;
                            l += *(short *)pSrc; pSrc += 2;
                            l += *(short *)pSrc; pSrc += 2;
                            l >>= 2;
                            v = ((g.a.value * l >> g.a.shift) >> 8) + 0x80;
                            l = ((g.b.value * l >> g.b.shift) >> 8) + 0x80;
                            *pDst++ = (char)v;
                            *pDst++ = (char)l;
                            nFrames--;
                        }
                        break;
                    case -2:
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            l += *(short *)pSrc; pSrc += 2;
                            l >>= 1;
                            ol = ((g.a.value * l >> g.a.shift) >> 8) + 0x80;
                            l = ((g.b.value * l >> g.b.shift) >> 8) + 0x80;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)l;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            v = *(short *)pSrc; pSrc += 2;
                            l = (g.a.value * v >> g.a.shift) >> 8;
                            v = (g.b.value * v >> g.b.shift) >> 8;
                            l += 0x80;
                            v += 0x80;
                            *pDst++ = (char)l;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    case 2:
                        while (nFrames != 0) {
                            v = *(short *)pSrc; pSrc += 2;
                            ol = ((g.a.value * v >> g.a.shift) >> 8) + 0x80;
                            or_ = ((g.b.value * v >> g.b.shift) >> 8) + 0x80;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)or_;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)or_;
                            nFrames--;
                        }
                        break;
                    case 4:
                        while (nFrames != 0) {
                            v = *(short *)pSrc; pSrc += 2;
                            ol = ((g.a.value * v >> g.a.shift) >> 8) + 0x80;
                            s = ((g.b.value * v >> g.b.shift) >> 8) + 0x80;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)s;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)s;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)s;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)s;
                            nFrames--;
                        }
                        break;
                    }
                } else {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            r = *(short *)pSrc; pSrc += 2;
                            r += *(short *)pSrc; pSrc += 2;
                            r += *(short *)pSrc; pSrc += 2;
                            r += *(short *)pSrc; pSrc += 2;
                            r >>= 2;
                            l = g.a.value * r >> g.a.shift;
                            r = g.b.value * r;
                            r >>= g.b.shift;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            *(short *)pDst = (short)r;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case -2:
                        while (nFrames != 0) {
                            v = *(short *)pSrc; pSrc += 2;
                            v += *(short *)pSrc; pSrc += 2;
                            v >>= 1;
                            s = g.a.value * v >> g.a.shift;
                            v = g.b.value * v;
                            v >>= g.b.shift;
                            *(short *)pDst = (short)s;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            v = *(short *)pSrc; pSrc += 2;
                            s = g.a.value * v >> g.a.shift;
                            v = g.b.value * v;
                            v >>= g.b.shift;
                            *(short *)pDst = (short)s;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 2: {
                        while (nFrames != 0) {
                            v = *(short *)pSrc; pSrc += 2;
                            or_ = g.a.value * v >> g.a.shift;
                            ol = g.b.value * v >> g.b.shift;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                    case 4:
                        while (nFrames != 0) {
                            v = *(short *)pSrc; pSrc += 2;
                            or_ = g.a.value * v >> g.a.shift;
                            ol = g.b.value * v >> g.b.shift;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                }
            }
        }
    } else {
        if (dstChans == 1) {
            if (srcBytes == 1) {
                if (dstBytes == 1) {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            o = *pSrc++;
                            o -= 0x80;
                            s = *pSrc++;
                            s -= 0x80;
                            v = *pSrc++;
                            v -= 0x80;
                            o += v;
                            v = *pSrc++;
                            v -= 0x80;
                            s += v;
                            v = *pSrc++;
                            v -= 0x80;
                            o += v;
                            v = *pSrc++;
                            v -= 0x80;
                            s += v;
                            v = *pSrc++;
                            v -= 0x80;
                            o += v;
                            v = *pSrc++;
                            v -= 0x80;
                            s += v;
                            o >>= 2;
                            s >>= 2;
                            o = (((g.a.value * o >> g.a.shift) + (g.b.value * s >> g.b.shift)) >> 1) + 0x80;
                            *pDst++ = (char)o;
                            nFrames--;
                        }
                        break;
                    case -2:
                        while (nFrames != 0) {
                            l = *pSrc++;
                            r = *pSrc++;
                            o = *pSrc++;
                            l = l - 0x80 + o - 0x80;
                            o = *pSrc++;
                            r = r - 0x80 + o - 0x80;
                            l >>= 1;
                            r >>= 1;
                            l = (((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1) + 0x80;
                            *pDst++ = (char)l;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            l = *pSrc++ - 0x80;
                            r = *pSrc++ - 0x80;
                            l = (((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1) + 0x80;
                            *pDst++ = (char)l;
                            nFrames--;
                        }
                        break;
                    case 2:
                        while (nFrames != 0) {
                            l = *pSrc++ - 0x80;
                            r = *pSrc++ - 0x80;
                            v = (((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1) + 0x80;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    case 4: {
                        while (nFrames != 0) {
                            l = *pSrc++ - 0x80;
                            r = *pSrc++ - 0x80;
                            v = (((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1) + 0x80;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    }
                    }
                } else {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            l = *pSrc++;
                            l -= 0x80;
                            l <<= 8;
                            or_ = *pSrc++;
                            or_ -= 0x80;
                            or_ <<= 8;
                            s = *pSrc++;
                            o = (s - 0x80) << 8;
                            l += o;
                            s = *pSrc++;
                            o = (s - 0x80) << 8;
                            or_ += o;
                            ol = *pSrc++;
                            o = (ol - 0x80) << 8;
                            l += o;
                            o = *pSrc++;
                            o = (o - 0x80) << 8;
                            or_ += o;
                            o = *pSrc++;
                            o = (o - 0x80) << 8;
                            l += o;
                            o = *pSrc++;
                            o = (o - 0x80) << 8;
                            or_ += o;
                            l >>= 2;
                            l = g.a.value * l;
                            l >>= g.a.shift;
                            or_ >>= 2;
                            s = g.b.value * or_ >> g.b.shift;
                            l += s;
                            l >>= 1;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case -2:
                        while (nFrames != 0) {
                            v = *pSrc++;
                            v -= 0x80;
                            v <<= 8;
                            r = *pSrc++;
                            r -= 0x80;
                            r <<= 8;
                            o = *pSrc++;
                            o -= 0x80;
                            o <<= 8;
                            v += o;
                            o = *pSrc++;
                            o -= 0x80;
                            o <<= 8;
                            r += o;
                            v >>= 1;
                            r >>= 1;
                            l = ((g.a.value * v >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            l = (*pSrc++ - 0x80) << 8;
                            r = (*pSrc++ - 0x80) << 8;
                            o = ((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 2: {
                        while (nFrames != 0) {
                            l = (*pSrc++ - 0x80) << 8;
                            r = (*pSrc++ - 0x80) << 8;
                            o = ((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                    case 4:
                        while (nFrames != 0) {
                            l = (*pSrc++ - 0x80) << 8;
                            r = (*pSrc++ - 0x80) << 8;
                            v = ((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                }
            } else {
                if (dstBytes == 1) {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            or_ = *(short *)pSrc; pSrc += 2;
                            s = *(short *)pSrc; pSrc += 2;
                            o = *(short *)pSrc; pSrc += 2;
                            or_ += o;
                            o = *(short *)pSrc; pSrc += 2;
                            s += o;
                            o = *(short *)pSrc; pSrc += 2;
                            or_ += o;
                            o = *(short *)pSrc; pSrc += 2;
                            s += o;
                            o = *(short *)pSrc; pSrc += 2;
                            or_ += o;
                            o = *(short *)pSrc; pSrc += 2;
                            s += o;
                            or_ >>= 2;
                            s >>= 2;
                            s = ((g.a.value * or_ >> g.a.shift) + (g.b.value * s >> g.b.shift)) >> 1;
                            s = (s >> 8) + 0x80;
                            *pDst++ = (char)s;
                            nFrames--;
                        }
                        break;
                    case -2:
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            l += *(short *)pSrc; pSrc += 2;
                            r += *(short *)pSrc; pSrc += 2;
                            l >>= 1;
                            r >>= 1;
                            l = ((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1;
                            l = (l >> 8) + 0x80;
                            *pDst++ = (char)l;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            l = ((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1;
                            l = (l >> 8) + 0x80;
                            *pDst++ = (char)l;
                            nFrames--;
                        }
                        break;
                    case 2:
                        while (nFrames != 0) {
                            l = *(short *)pSrc;
                            pSrc += 2;
                            r = *(short *)pSrc;
                            pSrc += 2;
                            l = ((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1;
                            v = (l >> 8) + 0x80;
                            *pDst++ = (char)v;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    case 4: {
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            r = ((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1;
                            o = (r >> 8) + 0x80;
                            *pDst++ = (char)o;
                            *pDst++ = (char)o;
                            *pDst++ = (char)o;
                            *pDst++ = (char)o;
                            nFrames--;
                        }
                        break;
                    }
                    }
                } else {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            r = *(short *)pSrc; pSrc += 2;
                            o = *(short *)pSrc; pSrc += 2;
                            l = *(short *)pSrc; pSrc += 2;
                            r += l;
                            l = *(short *)pSrc; pSrc += 2;
                            o += l;
                            l = *(short *)pSrc; pSrc += 2;
                            r += l;
                            l = *(short *)pSrc; pSrc += 2;
                            o += l;
                            l = *(short *)pSrc; pSrc += 2;
                            r += l;
                            r >>= 2;
                            l = *(short *)pSrc; pSrc += 2;
                            o += l;
                            o >>= 2;
                            r = g.a.value * r;
                            r >>= g.a.shift;
                            s = g.b.value * o >> g.b.shift;
                            r += s;
                            r >>= 1;
                            *(short *)pDst = (short)r;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case -2: {
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            l += *(short *)pSrc; pSrc += 2;
                            r += *(short *)pSrc; pSrc += 2;
                            l >>= 1;
                            r >>= 1;
                            v = ((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                    case 1:
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            v = ((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 2: {
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            v = ((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            *(short *)pDst = (short)v;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                    case 4: {
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            o = ((g.a.value * l >> g.a.shift) + (g.b.value * r >> g.b.shift)) >> 1;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            *(short *)pDst = (short)o;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                    }
                }
            }
        } else {
            if (srcBytes == 1) {
                if (dstBytes == 1) {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            v = *pSrc++ - 0x80;
                            o = *pSrc++;
                            r = *pSrc++ - 0x80;
                            ol = v + r;
                            v = *pSrc++;
                            o = o - 0x80 + v - 0x80;
                            v = *pSrc++ - 0x80;
                            s = ol + v;
                            v = *pSrc++ - 0x80;
                            o += v;
                            v = *pSrc++ - 0x80;
                            s += v;
                            s >>= 2;
                            v = *pSrc++ - 0x80;
                            o += v;
                            o >>= 2;
                            s = g.a.value * s >> g.a.shift;
                            o = g.b.value * o >> g.b.shift;
                            s += 0x80;
                            o += 0x80;
                            *pDst++ = (char)s;
                            *pDst++ = (char)o;
                            nFrames--;
                        }
                        break;
                    case -2:
                        while (nFrames != 0) {
                            l = *pSrc++;
                            r = *pSrc++;
                            l = l - 0x80 + *pSrc++ - 0x80;
                            r = r - 0x80 + *pSrc++ - 0x80;
                            l >>= 1;
                            r >>= 1;
                            l = g.a.value * l >> g.a.shift;
                            r = g.b.value * r >> g.b.shift;
                            l += 0x80;
                            r += 0x80;
                            *pDst++ = (char)l;
                            *pDst++ = (char)r;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            or_ = *pSrc++ - 0x80;
                            ol = *pSrc++ - 0x80;
                            or_ = g.a.value * or_ >> g.a.shift;
                            ol = g.b.value * ol >> g.b.shift;
                            or_ += 0x80;
                            ol += 0x80;
                            *pDst++ = (char)or_;
                            *pDst++ = (char)ol;
                            nFrames--;
                        }
                        break;
                    case 2:
                        while (nFrames != 0) {
                            ol = *pSrc++ - 0x80;
                            or_ = *pSrc++ - 0x80;
                            ol = g.a.value * ol >> g.a.shift;
                            or_ = g.b.value * or_ >> g.b.shift;
                            ol += 0x80;
                            or_ += 0x80;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)or_;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)or_;
                            nFrames--;
                        }
                        break;
                    case 4:
                        while (nFrames != 0) {
                            l = *pSrc++;
                            v = *pSrc++;
                            l -= 0x80;
                            ol = g.a.value * l >> g.a.shift;
                            v -= 0x80;
                            l = g.b.value * v;
                            v = ol + 0x80;
                            *pDst++ = (char)v;
                            or_ = (l >> g.b.shift) + 0x80;
                            *pDst++ = (char)or_;
                            *pDst++ = (char)v;
                            *pDst++ = (char)or_;
                            *pDst++ = (char)v;
                            *pDst++ = (char)or_;
                            *pDst++ = (char)v;
                            *pDst++ = (char)or_;
                            nFrames--;
                        }
                        break;
                    }
                } else {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            l = *pSrc++;
                            l -= 0x80;
                            l <<= 8;
                            r = *pSrc++;
                            r -= 0x80;
                            r <<= 8;
                            s = *pSrc++;
                            o = (s - 0x80) << 8;
                            l += o;
                            s = *pSrc++;
                            o = (s - 0x80) << 8;
                            r += o;
                            ol = *pSrc++;
                            o = (ol - 0x80) << 8;
                            l += o;
                            o = *pSrc++;
                            o = (o - 0x80) << 8;
                            r += o;
                            o = *pSrc++;
                            o = (o - 0x80) << 8;
                            l += o;
                            o = *pSrc++;
                            o = (o - 0x80) << 8;
                            r += o;
                            l >>= 2;
                            r >>= 2;
                            l = g.a.value * l >> g.a.shift;
                            r = g.b.value * r >> g.b.shift;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            *(short *)pDst = (short)r;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case -2:
                        while (nFrames != 0) {
                            l = *pSrc++;
                            l -= 0x80;
                            l <<= 8;
                            r = *pSrc++;
                            r -= 0x80;
                            r <<= 8;
                            s = *pSrc++;
                            o = (s - 0x80) << 8;
                            l += o;
                            o = *pSrc++;
                            o = (o - 0x80) << 8;
                            r += o;
                            l >>= 1;
                            r >>= 1;
                            l = g.a.value * l >> g.a.shift;
                            r = g.b.value * r >> g.b.shift;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            *(short *)pDst = (short)r;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            l = (*pSrc++ - 0x80) << 8;
                            r = (*pSrc++ - 0x80) << 8;
                            l = g.a.value * l >> g.a.shift;
                            r = g.b.value * r >> g.b.shift;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            *(short *)pDst = (short)r;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 2:
                        while (nFrames != 0) {
                            l = (*pSrc++ - 0x80) << 8;
                            r = (*pSrc++ - 0x80) << 8;
                            ol = g.a.value * l >> g.a.shift;
                            or_ = g.b.value * r >> g.b.shift;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 4: {
                        while (nFrames != 0) {
                            l = (*pSrc++ - 0x80) << 8;
                            r = (*pSrc++ - 0x80) << 8;
                            ol = g.a.value * l >> g.a.shift;
                            or_ = g.b.value * r >> g.b.shift;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                    }
                }
            } else {
                if (dstBytes == 1) {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            s = *(short *)pSrc; pSrc += 2;
                            v = *(short *)pSrc; pSrc += 2;
                            o = *(short *)pSrc; pSrc += 2;
                            s += o;
                            o = *(short *)pSrc; pSrc += 2;
                            v += o;
                            o = *(short *)pSrc; pSrc += 2;
                            s += o;
                            o = *(short *)pSrc; pSrc += 2;
                            v += o;
                            o = *(short *)pSrc; pSrc += 2;
                            s += o;
                            o = *(short *)pSrc; pSrc += 2;
                            v += o;
                            s >>= 2;
                            v >>= 2;
                            s = ((g.a.value * s >> g.a.shift) >> 8) + 0x80;
                            v = ((g.b.value * v >> g.b.shift) >> 8) + 0x80;
                            *pDst++ = (char)s;
                            *pDst++ = (char)v;
                            nFrames--;
                        }
                        break;
                    case -2:
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            o = *(short *)pSrc; pSrc += 2;
                            l += o;
                            o = *(short *)pSrc; pSrc += 2;
                            r += o;
                            l >>= 1;
                            r >>= 1;
                            l = (g.a.value * l >> g.a.shift) >> 8;
                            r = (g.b.value * r >> g.b.shift) >> 8;
                            l += 0x80;
                            r += 0x80;
                            *pDst++ = (char)l;
                            *pDst++ = (char)r;
                            nFrames--;
                        }
                        break;
                    case 1:
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            l = ((g.a.value * l >> g.a.shift) >> 8) + 0x80;
                            r = ((g.b.value * r >> g.b.shift) >> 8) + 0x80;
                            *pDst++ = (char)l;
                            *pDst++ = (char)r;
                            nFrames--;
                        }
                        break;
                    case 2:
                        while (nFrames != 0) {
                            s = *(short *)pSrc; pSrc += 2;
                            v = *(short *)pSrc; pSrc += 2;
                            l = (g.a.value * s >> g.a.shift) >> 8;
                            r = (g.b.value * v >> g.b.shift) >> 8;
                            l += 0x80;
                            r += 0x80;
                            *pDst++ = (char)l;
                            *pDst++ = (char)r;
                            *pDst++ = (char)l;
                            *pDst++ = (char)r;
                            nFrames--;
                        }
                        break;
                    case 4:
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            ol = ((g.a.value * l >> g.a.shift) >> 8) + 0x80;
                            or_ = ((g.b.value * r >> g.b.shift) >> 8) + 0x80;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)or_;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)or_;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)or_;
                            *pDst++ = (char)ol;
                            *pDst++ = (char)or_;
                            nFrames--;
                        }
                        break;
                    }
                } else {
                    switch ((int)ratio) {
                    case -4:
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            o = *(short *)pSrc; pSrc += 2;
                            l += o;
                            o = *(short *)pSrc; pSrc += 2;
                            r += o;
                            o = *(short *)pSrc; pSrc += 2;
                            l += o;
                            o = *(short *)pSrc; pSrc += 2;
                            r += o;
                            o = *(short *)pSrc; pSrc += 2;
                            l += o;
                            o = *(short *)pSrc; pSrc += 2;
                            r += o;
                            l >>= 2;
                            r >>= 2;
                            l = g.a.value * l >> g.a.shift;
                            r = g.b.value * r >> g.b.shift;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            *(short *)pDst = (short)r;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case -2:
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            o = *(short *)pSrc; pSrc += 2;
                            l += o;
                            o = *(short *)pSrc; pSrc += 2;
                            r += o;
                            l >>= 1;
                            r >>= 1;
                            l = g.a.value * l >> g.a.shift;
                            r = g.b.value * r >> g.b.shift;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            *(short *)pDst = (short)r;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 1: {
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            r = *(short *)pSrc; pSrc += 2;
                            l = g.a.value * l >> g.a.shift;
                            r = g.b.value * r >> g.b.shift;
                            *(short *)pDst = (short)l;
                            pDst += 2;
                            *(short *)pDst = (short)r;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                    case 2:
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            v = *(short *)pSrc; pSrc += 2;
                            ol = g.a.value * l >> g.a.shift;
                            or_ = g.b.value * v >> g.b.shift;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    case 4:
                        while (nFrames != 0) {
                            l = *(short *)pSrc; pSrc += 2;
                            v = *(short *)pSrc; pSrc += 2;
                            ol = g.a.value * l >> g.a.shift;
                            or_ = g.b.value * v >> g.b.shift;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            *(short *)pDst = (short)ol;
                            pDst += 2;
                            *(short *)pDst = (short)or_;
                            pDst += 2;
                            nFrames--;
                        }
                        break;
                    }
                }
            }
        }
    }
}


/* The type 2 (ring-buffered) wave header decoder. Manages a circular queue of up to 32 decode
 * slots (head +0x156, tail +0x15a; slot buffer pointer +0x56, start position +0xd6). Finds the
 * slot covering iPos, allocating a new one through AllocEngineBuffer if needed, and hands nFrames
 * rows to ConvertAudioBlockToOutputFormat. */
void __cdecl DecodeMediaFrameSegmentBuffered(int *pState, int iPos, int nFrames,
                                             char *pDst, int *pAnchor)  /* 0x004053E0 */
{
    int    iHead;
    int    iStride;
    int    iChunkSize;
    int    iLocal, iOff;    /* adjacent: C2 breaks the iLocal/iOff compare ties by a hash of
                             * the symbol index, and the exe's two compares need equal hashes */
    int    iRemain;
    int   *pPos;
    char  *pSlot;
    int    iTail;
    int    iLen;
    byte  *pConvSrc;

    iStride = *pState;
    if (0 < iStride) {
        iLocal  = iPos / iStride;
        iRemain = nFrames / iStride;
    } else {
        iLocal  = -(iPos * iStride);
        iRemain = -(nFrames * iStride);
    }
    iChunkSize = *(int *)(*(int *)((char *)pState + 0x4e) + 4);

    while (iRemain != 0) {
        iHead = *(int *)((char *)pState + 0x156);
        iTail = *(int *)((char *)pState + 0x15a);
        while (iHead != iTail) {
            iOff = *(int *)((char *)pState + iHead * 4 + 0xd6);
            if (iLocal >= iOff) {
                iOff += iChunkSize;
                if (iOff > iLocal)
                    break;
            }
            if (++iHead == 0x20)
                iHead = 0;
        }
        if (iHead != iTail) {
            pSlot = (char *)pState + iHead * 4;
            iOff  = iLocal - *(int *)(pSlot + 0xd6);
            iLen  = iChunkSize - iOff;
        } else {
            pSlot = (char *)pState + iHead * 4;
            pPos  = (int *)(pSlot + 0xd6);
            iOff  = iLocal - *pPos;
            iLen  = iChunkSize - iOff;
            if (*(int *)(pSlot + 0x56) == 0)
                *(int *)(pSlot + 0x56) = (int)AllocEngineBuffer(
                    (uint32_t)(iChunkSize << *(byte *)(pState + 1)));
            /* The decode callback (vtable +0xc) takes (pDst, pState): 0x004054b9 pushes
             * [ESI+0x52], then [EBP+0x56], i.e. fn(slot->buffer, *(int **)(pState + 0x52)). */
            {
                typedef void (__cdecl *DecodeFn)(char *, int *);
                int vt = *(int *)((char *)pState + 0x4e);
                ((DecodeFn)(*(void **)((char *)(uintptr_t)vt + 0xc)))(
                    (char *)(uintptr_t)*(int *)(pSlot + 0x56),
                    *(int **)((char *)pState + 0x52));
            }
            if (++*(int *)((char *)pState + 0x15a) == 0x20)
                *(int *)((char *)pState + 0x15a) = 0;
            iTail = *(int *)((char *)pState + 0x15a);
            if (iTail == *(int *)((char *)pState + 0x156)) {
                if (++*(int *)((char *)pState + 0x156) == 0x20)
                    *(int *)((char *)pState + 0x156) = 0;
            }
            *(int *)((char *)pState + iTail * 4 + 0xd6) = *pPos + iChunkSize;
        }
        pConvSrc = (byte *)((iOff << *(byte *)(pState + 1)) + *(int *)(pSlot + 0x56));
        if (iLen > iRemain)
            iLen = iRemain;
        iStride = *pState;
        iRemain -= iLen;
        iLocal  += iLen;
        if (iStride < 0)
            iLen = -(iLen / iStride);
#if DF_TRACE_ENABLED
        {
            int k, slo = 255, shi = 0;
            for (k = 0; k < 64 && k < iLen; k++) {
                if (pConvSrc[k] < slo) slo = pConvSrc[k];
                if (pConvSrc[k] > shi) shi = pConvSrc[k];
            }
        }
#endif
        ConvertAudioBlockToOutputFormat(
            pConvSrc,
            (uint32_t)pState[3], pState[4], pState[5],
            pDst, gMixSampleRate, gMixBytesPerSample, gMixSampleChannels,
            iLen, pAnchor, iStride);
#if DF_TRACE_ENABLED
        {   DF_TRACE("conv: n=%d rate=%d/%d/%d ch=%d bits=%d "
                     "src %02x %02x %02x %02x %02x %02x -> "
                     "dst %02x %02x %02x %02x %02x %02x",
                     iLen, (int)pState[3], (int)pState[4], (int)pState[5],
                     (int)gMixSampleChannels, (int)gMixBytesPerSample * 8,
                     pConvSrc[0], pConvSrc[1], pConvSrc[2],
                     pConvSrc[3], pConvSrc[4], pConvSrc[5],
                     ((unsigned char *)pDst)[0], ((unsigned char *)pDst)[1],
                     ((unsigned char *)pDst)[2], ((unsigned char *)pDst)[3],
                     ((unsigned char *)pDst)[4], ((unsigned char *)pDst)[5]);
        }
#endif
        if (1 < *pState)
            iLen *= *pState;
        pDst += iLen << gMixFrameShift;
    }
}


/* Resets the node's consumed counter to the start (remaining += consumed, consumed = 0). For a
 * type 2 (compressed) node it also zeroes the decoder state and calls the vtable reset function. */
void __cdecl RewindWaveHdrNode(uint32_t *pHdr)  /* 0x004055A0 */
{
    /* InitSoundBufferRegionDesc (vtable +0x10) takes two arguments: 0x004055cf pushes
     * [ECX+0x5e] (buffer), then ECX+0x33 (descriptor). */
    typedef void (__cdecl *RegionInitFn)(int *, int *);
    uint32_t *sub;

    pHdr[2] = pHdr[2] + pHdr[1];
    pHdr[1] = 0;
    if (*pHdr == 2) {
        *(uint32_t *)((char *)pHdr + 0xe2) = 0;
        *(uint32_t *)((char *)pHdr + 0x162) = 0;
        *(uint32_t *)((char *)pHdr + 0x166) = 0;
        /* The call works on the sub-record at +0xc (the exe does ADD ECX,0xC and addresses the
         * descriptor, buffer and object from there: +0x27, +0x52, +0x4e). */
        sub = pHdr + 3;
        ((RegionInitFn)(*(void **)(*(int *)((char *)sub + 0x4e) + 0x10)))(
            (int *)((char *)sub + 0x27),
            *(int **)((char *)sub + 0x52));
    }
}


/* Queues PCM frames into the wave mix buffer for one channel, splitting the copy where it wraps
 * around the end of the circular buffer. */
void __cdecl RenderSoundChannelToMixRing(int bufIdx, int framesPending)  /* 0x004055E0 */
{
  int runFrames;

  /* the remainder is the parameter itself, reduced after the first queue */
  runFrames = gMixRingEnd - gMixRingReadCursor >> gMixFrameShift;
  if (runFrames > framesPending)
    runFrames = framesPending;
  RenderWaveHdrQueueToMixBuffer((char *)(gMixRingReadCursor + bufIdx * 0x8010), bufIdx, runFrames);
  gMixFramesQueuedTotal = gMixFramesQueuedTotal + runFrames;
  framesPending -= runFrames;
  if (framesPending > 0) {
    RenderWaveHdrQueueToMixBuffer((char *)(gMixRingBase + bufIdx * 0x8010), bufIdx, framesPending);
    gMixFramesQueuedTotal = gMixFramesQueuedTotal + framesPending;
  }
}


/* Byte distance between consecutive per-channel mix buffers. Matches the
 * `nBuffers * 0x8010` secondary-heap reservation in OpenSoundDevice and the
 * +0x8010 / +0x10020 / +0x18030 displacements hardcoded in the original
 * mixer routines at 0x00401000-0x00401160. */

/* The mixers by channel count: CopyMixBufferSegment calls entry [gMixBufferCount - 1]. */
void (__cdecl *g_mix8Bit[4])(unsigned char *, unsigned char *, int) = {
    MixChannels8x1, MixChannels8x2, MixChannels8x3, MixChannels8x4
};  /* 0x0045b040 */
void (__cdecl *g_mix16Bit[4])(short *, short *, int) = {
    MixChannels16x1, MixChannels16x2, MixChannels16x3, MixChannels16x4
};  /* 0x0045b050 */

/* Mixes gMixSampleChannels * nFrames samples from the mix ring (gMixRingReadCursor) into the wave
 * buffer through the mixer for the active channel count and sample width, then advances the ring
 * cursor (wrapping at gMixRingEnd) and decrements gMixFramesPending. The port had the eight
 * mixers inlined here as one loop; they are the MixChannels* functions. */
void __cdecl CopyMixBufferSegment(void *pMixWriteCursor, int nFrames)  /* 0x00405650 */
{
    /* A switch: the exe lays both mixer calls out after the cursor update
     * (0x004056a0..), the shape MSVC gives case bodies, not an if/else. */
    switch (gMixBytesPerSample) {
    case 1:
        g_mix8Bit[gMixBufferCount - 1]((unsigned char *)gMixRingReadCursor, (unsigned char *)pMixWriteCursor,
                                    gMixSampleChannels * nFrames);
        break;
    case 2:
        g_mix16Bit[gMixBufferCount - 1]((short *)gMixRingReadCursor, (short *)pMixWriteCursor,
                                     gMixSampleChannels * nFrames);
        break;
    }
#if DF_TRACE_ENABLED
    {
        const int nSamples = (int)gMixSampleChannels * nFrames;
        const unsigned char *src = (const unsigned char *)(uintptr_t)gMixRingReadCursor;
        const int nCh = (int)gMixBufferCount;
            /* Per-channel source amplitude and the mixed result. A channel
             * carrying garbage instead of silence shows up here immediately. */
            int k, c, lo = 0x7fffffff, hi = -0x7fffffff;
            int n = (nSamples < 512) ? nSamples : 512;
            int clo[4], chi[4];
            for (c = 0; c < 4; c++) { clo[c] = 0x7fffffff; chi[c] = -0x7fffffff; }
            for (c = 0; c < nCh && c < 4; c++) {
                for (k = 0; k < n; k++) {
                    int v = (gMixBytesPerSample == 1)
                        ? (int)src[k + c * MIX_CHANNEL_STRIDE] - 0x80
                        : (int)*(const short *)(src + k * 2 + c * MIX_CHANNEL_STRIDE);
                    if (v < clo[c]) clo[c] = v;
                    if (v > chi[c]) chi[c] = v;
                }
            }
            for (k = 0; k < n; k++) {
                int v = (gMixBytesPerSample == 1)
                            ? (int)((unsigned char *)pMixWriteCursor)[k] - 0x80
                            : (int)((short *)pMixWriteCursor)[k];
                if (v < lo) lo = v;
                if (v > hi) hi = v;
            }
            {   /* Count large sample-to-sample jumps: a clean waveform has
                 * few, static/discontinuities have many. */
                int jumps = 0, prev = 0, thresh = (gMixBytesPerSample == 1) ? 32 : 8192;
                for (k = 0; k < n; k++) {
                    int v = (gMixBytesPerSample == 1)
                                ? (int)((unsigned char *)pMixWriteCursor)[k] - 0x80
                                : (int)((short *)pMixWriteCursor)[k];
                    if (k && (v - prev > thresh || prev - v > thresh)) jumps++;
                    prev = v;
                }
                DF_TRACE("mix: %d smp %d-bit %dch ch3[%d..%d] -> [%d..%d] "
                         "jumps=%d/%d%s",
                         nSamples, (int)gMixBytesPerSample * 8, nCh,
                         clo[3], chi[3], lo, hi, jumps, n,
                         (lo == 0 && hi == 0) ? "  (SILENT)" : "");
            }
        }
#endif
    /* the ring cursor and its end are addresses: compared unsigned (0x0040567f JBE) */
    gMixRingReadCursor += nFrames << gMixFrameShift;
    if ((unsigned int)gMixRingReadCursor > (unsigned int)gMixRingEnd)
        FatalErrorWithCode(0x13a);
    if (gMixRingReadCursor == gMixRingEnd)
        gMixRingReadCursor = gMixRingBase;
    gMixFramesPending -= nFrames;
}



/* Walks the buffer's wave header chain and queues each node with AppendWaveOutHdrEntryToQueue,
 * updating the mix cursor fields when flagged. */
void __cdecl AppendPendingDescriptorsToWaveHdrQueue(int bufIdx)  /* 0x00405710 */
{
  char *p;
  int rowOff;
  int flaggedCurNode;

  flaggedCurNode = 0;
  p = (char *)gWaveBufferFormatQueue[bufIdx];
  while (p) {
    AppendWaveOutHdrEntryToQueue(bufIdx, (int *)p);
    if (*(int *)(p + 0x2b) != 0)
      flaggedCurNode = *(int *)((char *)&DAT_004609a8 + bufIdx * 0x32);
    p = *(char **)(p + 0x27);
  }
  rowOff = bufIdx * 0x32;
  if (*(int *)((char *)&DAT_004609ca + rowOff) != 0) {
    if (flaggedCurNode != 0) {
      *(int *)((char *)&DAT_004609ce + rowOff) = flaggedCurNode;
    }
    if (*(int *)((char *)&DAT_004609ce + rowOff) == 0) {
      *(int *)((char *)&DAT_004609ce + rowOff) = *(int *)((int)&gWaveHdrQueues + rowOff);
    }
  }
  return;
}


/* Allocates a 0x176-byte queue entry, initialises its payload and appends it at the tail of the
 * buffer's queue row. If both the current tail and the new entry are idle (status 0), the new
 * entry's pending count is folded into the tail and the entry is freed. */
void __cdecl AppendWaveOutHdrEntryToQueue(int bufIdx, int *pPayload)  /* 0x00405780 */
{
  WaveHdrQueueRow *qb;       /* the queue row, formed first (ebp) */
  int *piNew;
  int *piTail;
  int iStatus;
  int *pHdr;      /* piNew + 1, kept for the teardown (ebx) */

  qb = WaveHdrQueueBase(bufIdx);
  piTail = *(int **)&qb->tail;
  piNew = (int *)AllocEngineBuffer(0x176);
  pHdr = piNew + 1;
  iStatus = InitWaveOutHdrPayload((uint32_t *)pHdr, pPayload);
  *piNew = iStatus;
  if ((*piTail == 0) && (iStatus == 0)) {
    piTail[3] = piTail[3] + piNew[3];
    FreeWaveOutHdrDecodeSlots(pHdr);
    FreeEngineBuffer((int)piNew);
    return;
  }
  *(uint32_t *)((int)piNew + 0x16e) = 0;
  *(int **)&qb->tail = piNew;
  *(int **)((int)piNew + 0x172) = piTail;
  *(int **)((int)piTail + 0x16e) = piNew;
}


/* For each active buffer (gWaveBufferActive[i]), calls ClearSoundChannelPendingDescriptors. */
void __stdcall ClearAllSoundChannelPendingDescriptors(void) /* 0x00405810 */
{
    int bufIdx;

    /* the count is re-read from memory at every test (cmp [mem],esi), even
     * on the path that skips the call: a volatile read */
    for (bufIdx = 0; bufIdx < *(volatile int *)&gMixBufferCount; bufIdx++) {
        if (gWaveBufferActive[bufIdx] != 0)
            ClearSoundChannelPendingDescriptors(bufIdx);
    }
}


/* Locks the sound mixer and, if that succeeds, unlocks it with position 0. */
void __stdcall ServiceSoundMixer(void)  /* 0x00405840 */
{
    int lockResult;

    lockResult = LockSoundMixer();
    if (lockResult == 0)
    {
        UnlockSoundMixer(0);
    }
}


/* Wave device dispatch slot 5 (+0x14): sets the output volume to a 0..9 level. Sibling of
 * InvokeWaveDeviceQueryVolume (slot 4, +0x10); the slot-5 implementation is SetWaveOutVolumeLevel.
 *
 * Port note: this was an empty body (MaybeInvokeHardwarePollCallback), the last link in the volume
 * chain, so no volume control ever reached the device. */
void __cdecl InvokeWaveDeviceSetVolume(int level)  /* 0x00405860 */
{
    typedef void (__cdecl *WaveDevOp5)(int);
    ((WaveDevOp5)(*(void **)((char *)(uintptr_t)gWaveDeviceOps + 0x14)))(level);
}


/* Invokes wave device dispatch slot 4 (+0x10), passing on the caller's out-parameter: the exe does
 * MOV EAX,[ESP+4]; PUSH EAX; CALL [gWaveDeviceOps+0x10], one real argument, which the port used to drop. */
void __cdecl InvokeWaveDeviceQueryVolume(void *pOut)  /* 0x00405880 */
{
  typedef void (__cdecl *WaveDevOp4)(void *);
  ((WaveDevOp4)(*(void **)((char *)(uintptr_t)gWaveDeviceOps + 0x10)))(pOut);
}
