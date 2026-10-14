/* sound_channels.c -- TI.EXE 0x004241e0..0x00425390: the sound system and the four sound channels. */

#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "df_protos.h"
#include "topics/sound.h"
#include "topics/scriptcmd.h"
#include "topics/multimedia.h"
#include "topics/reset_channel.h"


#include "topics/soundslot.h"

/* Region callback of the three sound codecs InitSoundSystem registers. pDesc is a wave descriptor
 * {codec id, data block, ...} (a node entry's +0x32); pOut = {block base, frame-offset index
 * (base+0x2c), index end (count at base+0x28, 4 bytes each), frame size: 0x200 / 0x400 / 0x800 for
 * codec 0x5a / 0x5b / 0x5c, the 11 / 22 / 44 kHz codecs}. */
void __cdecl InitSoundBufferRegionDesc(int *pDesc, int *pOut) /* 0x004241E0 */
{
    /* no local: pDesc[1] is re-read after each store, and the third field
     * is built on the second (the exe keeps base+0x2c in ecx) */
    pOut[0] = pDesc[1];
    pOut[1] = pDesc[1] + 0x2c;
    pOut[2] = pOut[1] + *(int *)(pDesc[1] + 0x28) * 4;
    switch (*pDesc) {
    case 0x5a:
        pOut[3] = 0x200;
        break;
    case 0x5b:
        pOut[3] = 0x400;
        break;
    case 0x5c:
        pOut[3] = 0x800;
        break;
    default:
        break;
    }
}


/* The sound codecs' decode callback: expands one compressed frame into pDst as 8-bit PCM.
 * pState is InitSoundBufferRegionDesc's output: {block base, cursor into the frame-offset index
 * (advanced 4 per frame), index end, output length}. A frame opens with a raw seed sample; every
 * output byte is sample + 0x40. Then per op byte: 0x00-0x7f a literal sample; 0x80-0xbf a delta run
 * of N+1 pairs (N = low 6 bits), each pair from one byte d, adding kAudioDeltaTable1[d] then
 * kAudioDeltaTable2[d]; 0xc0-0xff (low 6 bits + 1) repeats of the current sample.
 * Fatal 0x61d: index overrun; 0x61e: output length mismatch. */
void __cdecl DecodeAudioBuffer(char *pDst, int *pState)  /* 0x00424240 */
{
    int    off;
    char  *pEnd;
    short  op;
    char  *pOut;
    short  cnt;
    unsigned char *pIn;
    short  sample;   /* running sample accumulator */
    char   fill;     /* computed before the run test (0x004242a6) */
    short  d;

    off = *(int *)pState[1];
    pState[1] += 4;
    if ((unsigned int)pState[2] < (unsigned int)pState[1])
        FatalError(0, 0x61d);
    pOut = pDst;
    pIn = (unsigned char *)(pState[0] + off);
    pEnd = pOut + pState[3];
    DF_TRACE("DecodeAudioBuffer: dst=%p len=%d off=%d", (void *)pOut, pState[3], off);
    sample = *pIn++;
    *pOut++ = (char)(sample + 0x40);
    while (pOut < pEnd) {
        op = *pIn++;
        if (op & 0x80) {
            if (op & 0x40) {
                /* the exe's inline memset */
                cnt = op & 0x3f;
                fill = (char)(sample + 0x40);
                if (cnt >= 0) {
                    cnt++;
                    memset(pOut, fill, (unsigned short)cnt);
                    pOut += (unsigned short)cnt;
                }
            } else {
                for (cnt = op & 0x3f; cnt >= 0; cnt--) {
                    d = *pIn++;
                    sample += kAudioDeltaTable1[d];
                    *pOut++ = (char)(sample + 0x40);
                    sample += kAudioDeltaTable2[d];
                    *pOut++ = (char)(sample + 0x40);
                }
            }
        } else {
            *pOut = (char)(op + 0x40);   /* stored before the increment (0x00424313) */
            sample = op;
            pOut++;
        }
    }
    if (pOut != pEnd)
        FatalError(0, 0x61e);
}


/* Closes the sound device InitSoundSystem opened, if sound is on; fatal 0x61f if the close fails. */
void __stdcall ShutdownSoundSystem(void)  /* 0x00424340 */
{
    short status;

    if (gSoundOn != 0)
    {
        gSoundOn = 0;
        status = (short)CloseSoundDevice();
        if (status != 0)
            FatalError(status, 0x61f);
    }
}


/* Opens the sound device at the best format the -16 / -s flags allow, falling back stereo 16-bit ->
 * stereo 8-bit -> mono 16-bit -> mono 8-bit (OpenSoundDevice(4, channels, 2, bytes per sample)), and
 * registers the three codecs 0x5a / 0x5b / 0x5c (11 / 22 / 44 kHz, frames of 0x200 / 0x400 / 0x800).
 * Fatal 0x620 if sound is already on, 0x621 if no format opens. */
void __stdcall InitSoundSystem(void)  /* 0x00424370 */
{
    short status;
    int   openResult;
    int   desc[5];   /* {codec id, frame size, flags, decode callback, region callback} */

    if (gSoundOn != 0)
        FatalError(0, 0x620);

    status = 1;

    if (gSound16BitFlag != 0 && gStereoSoundFlag != 0)
    {
        gSoundStereo = 1;
        gSound16Bit = 1;
        openResult = OpenSoundDevice(4, 2, 2, 2);
        status = (short)openResult;
    }

    if (status != 0 && gStereoSoundFlag != 0)
    {
        gSoundStereo = 1;
        gSound16Bit = 0;
        openResult = OpenSoundDevice(4, 2, 2, 1);
        status = (short)openResult;
    }
    if (status != 0 && gSound16BitFlag != 0)
    {
        gSoundStereo = 0;
        gSound16Bit = 1;
        openResult = OpenSoundDevice(4, 1, 2, 2);
        status = (short)openResult;
    }

    if (status != 0)
    {
        gSoundStereo = 0;
        gSound16Bit = 0;
        openResult = OpenSoundDevice(4, 1, 2, 1);
        status = (short)openResult;
    }
    /* a separate test (status is 0 when the block was skipped): with the
     * FatalError inside the block VC++ 4.2 stores the two zeros as
     * immediates; the exe holds them in a register (xor ecx,ecx) */
    if (status != 0)
        FatalError(status, 0x621);

    desc[2] = 0x10;                         /* flags */
    desc[4] = (int)InitSoundBufferRegionDesc;
    desc[3] = (int)DecodeAudioBuffer;

    desc[0] = 0x5a;
    desc[1] = 0x200;
    RegisterSoundCodec(desc);

    desc[0] = 0x5b;
    desc[1] = 0x400;
    RegisterSoundCodec(desc);

    desc[0] = 0x5c;
    desc[1] = 0x800;
    RegisterSoundCodec(desc);

    gSoundOn = 1;
}


/* Returns the wave device's volume level (wave dispatch slot 4), or 0 when sound is off.
 *
 * Port note: Ghidra shows level uninitialised; it is the query's out-parameter (0x004244f4 pushes
 * its address). A hardcoded 0 here once made callers take the device for idle. */
short __stdcall PollWaveDeviceIfSoundActive(void)  /* 0x004244E0 */
{
  int level;                          /* an out-parameter: the exe does not zero it */  /* local_4 */

  if (gSoundOn == 0) {
    return 0;
  }
  InvokeWaveDeviceQueryVolume(&level);
  return (short)level;
}


/* Sets the wave output volume to a 0..9 level if sound is on: `wavevolume` (16033) and the digit
 * keys -> InvokeWaveDeviceSetVolume (wave dispatch slot 5) -> waveOutSetVolume. The level is a short
 * (0x0042451a: MOVSX).
 *
 * Port note: once decoded as RunSlotIndexScriptPollIfActive(void), which dropped the level at all
 * twelve call sites. */
void __cdecl SetWaveVolumeIfDeviceReady(short level) /* 0x00424510 */
{
    DF_TRACE("vol: SetWaveVolumeIfDeviceReady level=%d gate(gSoundOn)=%d",
             (int)level, (int)gSoundOn);
    if (gSoundOn != 0)
        InvokeWaveDeviceSetVolume((int)level);   /* MOVSX: sign-extended */
}


/* Tears down a voice row (a sound node entry laid out like a SoundChannelSlot): if sound is on,
 * silences the mix slot playing its data (+0x36); then unlocks its resource (+0x2e) and detaches
 * its render child (+0x28 group, +0x24 node). Fatal 0x622 if the detach fails. */
void __cdecl TeardownVoicePlaybackRow(int pVoiceRow)  /* 0x00424530 */
{
    int            chIdx;
    short          u;

    if (gSoundOn != 0 &&
        TryResolveWaveMixSlotForVoiceSource(*(int *)(pVoiceRow + 0x36), &chIdx) == 0) {
        LockSoundMixer();
        SilenceSoundChannel(chIdx);
        UnlockSoundMixer(0);
    }
    UnlockResObject(*(int **)(pVoiceRow + 0x2e));
    u = DetachRenderChild(*(int *)(pVoiceRow + 0x28), *(int *)(pVoiceRow + 0x24));
    if (u != 0)
        FatalError(u, 0x622);
}


/* Builds a sound node entry for the sound resource (pGroup, nodeId), which it attaches and keeps.
 * Resource header: +0x1a channel count, +0x1c sample rate, +0x24 total samples. Entry: +0 key (the
 * id), +4 volume 0xff, +6 pan 0x80, +8 name (Pascal), +0x24/+0x28 node and group, +0x2e handle,
 * +0x32 the wave descriptor a channel loads {codec 0x5a/0x5b/0x5c, data, frames, rate code 1/2/4},
 * +0x42/+0x46 both 1, +0x4a and +0x59 gain triples. Fatal 0x623 if the attach fails, 0x624 on a
 * rate other than 11025 / 22050 / 44100 Hz. */
void __cdecl InitSoundNodeEntryFromResource(unsigned int *pGroup, unsigned int nodeId,  /* 0x004245b0 */
                              byte *pName, unsigned int *pEntry)
{
    int          sampleRate;
    short        status;
    unsigned int pSoundData;
    int         *hSound;  /* local_4 */

    status = AttachRenderChild(pGroup, nodeId, &hSound);
    if (status != 0) {
        FatalError(status, 0x623);
    }
    MoveResObjectToHeapTop(hSound);
    pSoundData = (unsigned int)GetResDataPtr((void *)hSound);

    /* char-pointer field addressing and this store order (+0x18 before
     * +0x28/+0x24) give the exe's schedule of the pGroup load (r3) */
    *(unsigned int *)((char *)pEntry + 0x42) = 1;
    *(unsigned int *)((char *)pEntry + 0x46) = 1;

    sampleRate = *(int *)(pSoundData + 0x1c);
    *(int *)((char *)pEntry + 0x3a) = *(int *)(pSoundData + 0x24) / (int)*(short *)(pSoundData + 0x1a);

    if (sampleRate == 0x2b11) {                              /* 11025 Hz */
        *(unsigned int *)((char *)pEntry + 0x3e) = 1;
        *(unsigned int *)((char *)pEntry + 0x32) = 0x5a;
    } else if (sampleRate == 0x5622) {                       /* 22050 Hz */
        *(unsigned int *)((char *)pEntry + 0x3e) = 2;
        *(unsigned int *)((char *)pEntry + 0x32) = 0x5b;
    } else if (sampleRate == 0xac44) {                       /* 44100 Hz */
        *(unsigned int *)((char *)pEntry + 0x3e) = 4;
        *(unsigned int *)((char *)pEntry + 0x32) = 0x5c;
    } else {
        FatalError(0, 0x624);
    }

    pEntry[7]  = pSoundData;
    pEntry[8]  = (unsigned int)hSound;
    *(int **)((char *)pEntry + 0x2e) = hSound;
    *(unsigned int *)((char *)pEntry + 0x36) = pSoundData;
    *pEntry    = nodeId;
    pEntry[6]  = 0;
    pEntry[10] = (unsigned int)pGroup;
    pEntry[9]  = nodeId;

    *(short *)(pEntry + 1)      = 0xff;   /* volume: full */
    *(short *)((char *)pEntry + 6) = 0x80;   /* pan: centre */

    PackGainTripleFromVolumePan((int *)((char *)pEntry + 0x59), 0xff, 0x80);
    DF_TRACE("InitSoundNodeEntryFromResource: rateTag=0x%x chans=%d totalSamples=%d "
             "stepRate=%d rateCode=%d gain=%d/2^%d",
             (unsigned)*(int *)(pSoundData + 0x1c), (int)*(short *)(pSoundData + 0x1a),
             *(int *)(pSoundData + 0x24),
             *(int *)((char *)pEntry + 0x3a), *(int *)((char *)pEntry + 0x3e),
             *(int *)((char *)pEntry + 0x59), (int)*((unsigned char *)pEntry + 0x5d));
    PackVolumePairToGainTriple((int *)((char *)pEntry + 0x4a), 1.0, 1.0);
    CopyPascalString(pName, (byte *)(pEntry + 2));
}


/* Packs a node's volume (gainByte, 0..0xff) and pan (curveByte, 0x80 centre) into a gain triple:
 * left and right are volume/255, scaled down on the right below 0x80 and on the left above it.
 *
 * Port note: PackVolumePairToGainTriple takes doubles (0x00401ff0 reads qwords); its prototype is
 * load-bearing -- a float declaration once silenced every sound node. */
void __cdecl PackGainTripleFromVolumePan(int *pOut, short gainByte, short curveByte)  /* 0x004246f0 */
{
    double cL;
    double cR;

    DF_TRACE("packByte: gain=%d curve=%d", (int)gainByte, (int)curveByte);
    if (curveByte < 0x80) {
        cL = 1.0;
        cR = (double)curveByte * (1.0 / 128.0);
    } else if (curveByte > 0x80) {   /* the exe tests > 0x80 second, == 0x80 falls last */
        cL = (double)(0xff - curveByte) * (1.0 / 127.0);
        cR = 1.0;
    } else {
        cL = 1.0;
        cR = 1.0;
    }
    PackVolumePairToGainTriple(
        pOut,
        (double)(int)gainByte * cL * (1.0 / 255.0),
        cR * (double)(int)gainByte * (1.0 / 255.0));
}


/* Initialises a sound node entry with no resource: name at +8, id at [0] and [9], volume 0xff,
 * pan 0x80. pGroup is unused in the exe. */
void __cdecl InitSoundNodeEntry(uint32_t pGroup, uint32_t id, byte *pName,
                                uint32_t *pEntry)  /* 0x004247B0 */
{
  (void)pGroup;
  CopyPascalString(pName, (byte *)(pEntry + 2));
  pEntry[9] = id;
  *pEntry = id;
  pEntry[6] = 0;
  *(short *)(pEntry + 1) = 0xff;
  *(short *)((int)pEntry + 6) = 0x80;
}


/* Frees the slot of every channel that has gone idle by zeroing its priority key. gSoundSlotTbl is
 * four 0x68-byte slots, one per channel (0 voice, 1 and 2 sounds, 3 theme); key 0 = idle. The 19
 * slot-field aliases in include/df_globals.h index off this one table: never resize it. */
void __stdcall ClearInactiveSoundChannelTemplateKeys(void) /* 0x004247f0 */
{
    short ch;

    for (ch = 0; ch < 4; ch++) {
        if (IsSoundChannelIdle((int)ch) != 0)
            gSoundSlotTbl[(int)ch].key = 0;
    }
}


/* Returns 1 when sound is off or both sound channels (1 and 2) are idle. */
short __stdcall AreSoundChannelsIdle(void)  /* 0x00424820 */
{
  if (gSoundOn == 0) {
    return 1;
  }
  ClearInactiveSoundChannelTemplateKeys();
  if (gSoundSlotTbl[1].key != 0) {
    return 0;
  }
  return gSoundSlotTbl[2].key == 0;
}


/* Returns 1 when sound is off or the voice channel (0) is idle. */
short __stdcall CheckSoundChannelStopped(void)  /* 0x00424850 */
{
    if (gSoundOn == 0) {
        return 1;
    }
    ClearInactiveSoundChannelTemplateKeys();
    return gSoundSlotTbl[0].key == 0;
}



/* Silences every busy channel whose slot handle (+0x28, the sound's group) is handle. */
void __cdecl SilenceSoundChannelsByHandle(int handle)  /* 0x00424880 */
{
  int slotIdx;
  short i;
  
  i = 0;
  if (gSoundOn != 0) {
    ClearInactiveSoundChannelTemplateKeys();
    LockSoundMixer();
    do {
      slotIdx = (int)i;
      if ((gSoundSlotTbl[slotIdx].key != 0) && (gSoundSlotTbl[slotIdx].handle == handle)) {
        SilenceSoundChannel(slotIdx);
      }
      i = i + 1;
    } while (i < 4);
    UnlockSoundMixer(0);
  }
  return;
}


/* Silences each channel whose flag is non-zero -- note the parameter order 1, 2, 3, 0 -- then
 * flushes the mix (UnlockSoundMixer(1)). No-op when sound is off. */
void __cdecl StopSoundChannels(short bCh1, short bCh2, short bCh3, short bCh0)  /* 0x004248e0 */
{
    if (gSoundOn != 0) {
#if DF_FIX_SKIP_IDLE_CHANNEL_STOP
        /* UnlockSoundMixer(1) below flushes the device (waveOutReset) and
         * re-mixes from the play cursor. When every requested channel is
         * already idle -- the same test AssignSoundChannel uses to free a
         * slot -- silencing it changes nothing in the mix, so skip the
         * flush. The radio tuner calls halttheme on every drag tick; each
         * reset blocks ~9 ms on modern winmm and chopped the static. */
        if ((bCh1 == 0 || IsSoundChannelIdle(1) != 0) &&
            (bCh2 == 0 || IsSoundChannelIdle(2) != 0) &&
            (bCh3 == 0 || IsSoundChannelIdle(3) != 0) &&
            (bCh0 == 0 || IsSoundChannelIdle(0) != 0))
            return;
#endif
        LockSoundMixer();
        if (bCh1 != 0)
            SilenceSoundChannel(1);
        if (bCh2 != 0)
            SilenceSoundChannel(2);
        if (bCh3 != 0)
            SilenceSoundChannel(3);
        if (bCh0 != 0)
            SilenceSoundChannel(0);
        UnlockSoundMixer(1);
    }
}


/* If the theme channel (3) is playing, copies its slot's name to pNamedDescBuf and returns its handle
 * (+0x28) and aux (+0x24); otherwise "None" and two zeros. The three functions after it do the same
 * for channels 0, 1 and 2. */
void __cdecl GetPlayingThemeName(byte *pNamedDescBuf, int *pRowCtx, int *pAux) /* 0x00424950 */
{
    int idle;
    byte *pbVar2;

    if (gSoundOn != 0) {
        idle = IsSoundChannelIdle(3);
        if (idle == 0) {
            CopyPascalString(gSoundSlotTbl[3].name, pNamedDescBuf);
            *pRowCtx = gSoundSlotTbl[3].handle;
            *pAux    = gSoundSlotTbl[3].aux;
            return;
        }
    }
    CopyPascalString((byte *)UpdatePascalLength(kStrNone), pNamedDescBuf);   /* the destination is pushed before the lookup call */
    *pRowCtx = 0;
    *pAux    = 0;
}


/* GetPlayingThemeName for the voice channel (0). */
void __cdecl GetPlayingVoiceName(byte *pNamedDescBuf, int *pA, int *pB) /* 0x004249d0 */
{
    int idle;
    byte *pbVar2;

    if (gSoundOn != 0) {
        idle = IsSoundChannelIdle(0);
        if (idle == 0) {
            /* the name field's address; the port once passed its contents (class 1) */
            CopyPascalString(gSoundSlotTbl[0].name, pNamedDescBuf);
            *pA = gSoundSlotTbl[0].handle;
            *pB = gSoundSlotTbl[0].aux;
            return;
        }
    }
    CopyPascalString((byte *)UpdatePascalLength(kStrNone), pNamedDescBuf);
    *pA = 0;
    *pB = 0;
}


/* GetPlayingThemeName for sound channel 1. */
void __cdecl GetPlayingSoundName1(byte *pNameBuf, uint32_t *pRowCtx, uint32_t *pAux)  /* 0x00424A50 */
{
  int idle;
  byte *pbVar2;

  if (gSoundOn != 0) {
    idle = IsSoundChannelIdle(1);
    if (idle == 0) {
      CopyPascalString(gSoundSlotTbl[1].name, pNameBuf);
      *pRowCtx = gSoundSlotTbl[1].handle;
      *pAux = gSoundSlotTbl[1].aux;
      return;
    }
  }
  CopyPascalString((byte *)UpdatePascalLength(&kStrNone), pNameBuf);
  *pRowCtx = 0;
  *pAux = 0;
  return;
}


/* GetPlayingThemeName for sound channel 2. */
void __cdecl GetPlayingSoundName2(byte *pNameBuf, uint32_t *pRowCtx, uint32_t *pAux)  /* 0x00424AD0 */
{
  int idle;
  byte *pbVar2;

  if (gSoundOn != 0) {
    idle = IsSoundChannelIdle(2);
    if (idle == 0) {
      CopyPascalString(gSoundSlotTbl[2].name, pNameBuf);
      *pRowCtx = gSoundSlotTbl[2].handle;
      *pAux = gSoundSlotTbl[2].aux;
      return;
    }
  }
  CopyPascalString((byte *)UpdatePascalLength(&kStrNone), pNameBuf);
  *pRowCtx = 0;
  *pAux = 0;
  return;
}


/* Reports the sound playing on channel 1 or 2: channel 2 if only it plays, else whichever has the
 * higher priority key (a tie goes to 2); channel 1 (or "None") when 2 is idle. */
void __cdecl GetPlayingSoundNameByPriority(byte *pNameBuf, uint32_t *pRowCtx, uint32_t *pAux) /* 0x00424B50 */
{
    int idle;

    if (gSoundOn != 0) {
        idle = IsSoundChannelIdle(2);
        if (idle == 0) {
            idle = IsSoundChannelIdle(1);
            if (idle != 0) {
                GetPlayingSoundName2(pNameBuf, pRowCtx, pAux);
                return;
            }
            if (gSoundSlotTbl[2].key < gSoundSlotTbl[1].key) {
                GetPlayingSoundName1(pNameBuf, pRowCtx, pAux);
                return;
            }
            GetPlayingSoundName2(pNameBuf, pRowCtx, pAux);
            return;
        }
    }
    GetPlayingSoundName1(pNameBuf, pRowCtx, pAux);
}


/* Plays a theme -- a chain of sound node entries linked through +0x18 -- on the theme channel (3).
 * A chain may close on itself (SetSoundNodeLoop links a node to itself): the walk finds the node it
 * loops back to by re-scanning the prefix, queues every node once (flagging that node) and then sets
 * the channel's loop gate. Fatal 0x627 if the re-scan check fails. The first entry is copied into
 * slot 3 (DAT_0048a030) and its gain triple applied. */
void __cdecl PlayThemeList(void *pTarget)  /* 0x00424be0 */
{
    void   **pCur;        /* walk cursor */
    void   **pPrev;       /* last node accepted */
    void   **pScan;       /* re-scan cursor from the head */
    void * volatile pLoopHead;   /* cycle entry, or NULL (local_4; kept in memory) */
    int      nVisited;    /* nodes accepted so far */
    int      nRemaining;  /* countdown while re-scanning */

    nVisited  = 0;
    if (gSoundOn == 0)
        return;
    pLoopHead = NULL;
    pCur      = (void **)pTarget;
    pPrev     = (void **)pLoopHead;

    do {
        pScan      = (void **)pTarget;
        nRemaining = nVisited;
        if (pCur != (void **)pTarget) {
            do {
                if (nRemaining == 0)
                    goto accept;      /* ran out of prefix: not a cycle */
                nRemaining--;
                pScan = (void **)pScan[6];
            } while (pScan != pCur);
        }
        if (nRemaining != 0) {
            pLoopHead = pCur;         /* found pCur again: chain loops here */
            break;
        }
    accept:
        if (pScan != pCur)
            FatalError(0, 0x627);
        nVisited++;
        pPrev = pCur;
        pCur  = (void **)pCur[6];
    } while (pCur != NULL);

    gSoundSlotTbl[3] = *(SoundChannelSlot *)pTarget;
    {
        int *t = (int *)pTarget;
        DF_TRACE("templateNode %p: [0]=%d [1]=0x%x [2]=0x%x [3]=0x%x [4]=0x%x [5]=0x%x [6]=next %p "
                 "+0x32 fmt=[0x%x 0x%x 0x%x 0x%x] +0x59=[0x%x]",
                 pTarget, t[0], t[1], t[2], t[3], t[4], t[5], (void *)(intptr_t)t[6],
                 *(int *)((char *)pTarget + 0x32), *(int *)((char *)pTarget + 0x36),
                 *(int *)((char *)pTarget + 0x3a), *(int *)((char *)pTarget + 0x3e),
                 *(int *)((char *)pTarget + 0x59));
    }
    LockSoundMixer();
    SetSoundChannelGain(3, (void *)((char *)pTarget + 0x59));
    do {
        QueueWaveDataOntoSoundChannel(3, (void *)((char *)pTarget + 0x32),
                     (int)(short)(unsigned short)(pLoopHead == pTarget));
        if (pPrev == (void **)pTarget) break;
        pTarget = ((void **)pTarget)[6];
    } while (pTarget != (void *)0);
    if (pLoopHead != (void *)0) {
        SetSoundChannelLoopGateFlags(3, 1);
    }
    UnlockSoundMixer(1);
}


/* Plays a sound node entry on sound channel 1 or 2, replacing the channel with the lower priority key
 * (0 = idle), if the entry's key (+0) is above it and the other channel is not playing that key.
 * This and the four functions after it load a channel from the entry the same way: the slot copy,
 * the wave descriptor at +0x32, the loop gate from the +0x18 link, the gain triple at +0x59. */
void __cdecl AssignSoundChannel(int *pEntry)  /* 0x00424cb0 */
{

    if (gSoundOn != 0) {
        ClearInactiveSoundChannelTemplateKeys();
        /* The key is read as *pEntry at each compare (no local): a local lets
         * VC++ 4.2 keep gSoundSlotTbl[2].key in a register across the branch. */
        if (gSoundSlotTbl[1].key < gSoundSlotTbl[2].key) {
            if ((gSoundSlotTbl[1].key < *pEntry) && (gSoundSlotTbl[2].key != *pEntry)) {
                gSoundSlotTbl[1] = *(SoundChannelSlot *)pEntry;
                LockSoundMixer();
                LoadWaveDataOntoSoundChannel(1, (void *)((int)pEntry + 0x32));
                SetSoundChannelLoopGateFlags(1, (unsigned int)(pEntry[6] != 0));
                SetSoundChannelGain(1, (void *)((int)pEntry + 0x59));
                UnlockSoundMixer(1);
                return;
            }
        }
        else if ((gSoundSlotTbl[2].key < *pEntry) && (gSoundSlotTbl[1].key != *pEntry)) {
            LockSoundMixer();
            gSoundSlotTbl[2] = *(SoundChannelSlot *)pEntry;
            LoadWaveDataOntoSoundChannel(2, (void *)((int)pEntry + 0x32));
            SetSoundChannelLoopGateFlags(2, (int)(pEntry[6] != 0));
            SetSoundChannelGain(2, (void *)((int)pEntry + 0x59));
            UnlockSoundMixer(1);
        }
    }
    return;
}


/* AssignSoundChannel's variant that accepts an equal key: unless the entry's key is below the lower
 * channel key, it restarts the channel already playing that key, else replaces the lower one.
 *
 * Port note: these priority compares were once written on the bare slot arrays, which decayed to
 * fixed addresses and gave constant results. */
void __cdecl ReplaceSoundChannel(int *pEntry)  /* 0x00424da0 */
{

    if (gSoundOn == 0)
        return;
    ClearInactiveSoundChannelTemplateKeys();
    if (gSoundSlotTbl[1].key < gSoundSlotTbl[2].key) {
        if (*pEntry < gSoundSlotTbl[1].key)
            return;
        if (gSoundSlotTbl[2].key == *pEntry) {
assignSlot2:
            LockSoundMixer();
            gSoundSlotTbl[2] = *(SoundChannelSlot *)pEntry;
            LoadWaveDataOntoSoundChannel(2, (void *)((int)pEntry + 0x32));
            SetSoundChannelLoopGateFlags(2, (unsigned int)(pEntry[6] != 0));
            SetSoundChannelGain(2, (void *)((int)pEntry + 0x59));
            UnlockSoundMixer(1);
        } else
            goto assignSlot1;
    } else {
        if (*pEntry < gSoundSlotTbl[2].key)
            return;
        if (gSoundSlotTbl[1].key == *pEntry) {
assignSlot1:
            LockSoundMixer();
            gSoundSlotTbl[1] = *(SoundChannelSlot *)pEntry;
            LoadWaveDataOntoSoundChannel(1, (void *)((int)pEntry + 0x32));
            SetSoundChannelLoopGateFlags(1, (unsigned int)(pEntry[6] != 0));
            SetSoundChannelGain(1, (void *)((int)pEntry + 0x59));
            UnlockSoundMixer(1);
        } else
            goto assignSlot2;
    }
}


/* Plays the entry on every sound channel (1, 2 or both) whose key is below the entry's. */
void __cdecl UpgradeSoundChannels(int *pEntry)  /* 0x00424e90 */
{

    if (gSoundOn != 0) {
        ClearInactiveSoundChannelTemplateKeys();
        if ((gSoundSlotTbl[1].key < *pEntry) || (gSoundSlotTbl[2].key < *pEntry)) {
            LockSoundMixer();
            if (gSoundSlotTbl[1].key < *pEntry) {
                gSoundSlotTbl[1] = *(SoundChannelSlot *)pEntry;
                LoadWaveDataOntoSoundChannel(1, (void *)((int)pEntry + 0x32));
                SetSoundChannelLoopGateFlags(1, (unsigned int)(pEntry[6] != 0));
                SetSoundChannelGain(1, (void *)((int)pEntry + 0x59));
            }
            if (gSoundSlotTbl[2].key < *pEntry) {
                gSoundSlotTbl[2] = *(SoundChannelSlot *)pEntry;
                LoadWaveDataOntoSoundChannel(2, (void *)((int)pEntry + 0x32));
                SetSoundChannelLoopGateFlags(2, (unsigned int)(pEntry[6] != 0));
                SetSoundChannelGain(2, (void *)((int)pEntry + 0x59));
            }
            UnlockSoundMixer(1);
        }
    }
}


/* Plays the entry on both sound channels regardless of priority. */
void __cdecl ForceAssignSoundChannels(int *pEntry)  /* 0x00424f60 */
{

    if (gSoundOn != 0) {
        ClearInactiveSoundChannelTemplateKeys();
        LockSoundMixer();

        gSoundSlotTbl[1] = *(SoundChannelSlot *)pEntry;
        LoadWaveDataOntoSoundChannel(1, (void *)((int)pEntry + 0x32));
        SetSoundChannelLoopGateFlags(1, (unsigned int)(pEntry[6] != 0));
        SetSoundChannelGain(1, (void *)((int)pEntry + 0x59));

        gSoundSlotTbl[2] = *(SoundChannelSlot *)pEntry;
        LoadWaveDataOntoSoundChannel(2, (void *)((int)pEntry + 0x32));
        SetSoundChannelLoopGateFlags(2, (unsigned int)(pEntry[6] != 0));
        SetSoundChannelGain(2, (void *)((int)pEntry + 0x59));

        UnlockSoundMixer(1);
    }
}


/* Plays the entry on the voice channel (0) unconditionally. */
void __cdecl AssignPrimarySoundChannel(int *pEntry)  /* 0x00425010 */
{

    if (gSoundOn != 0) {
        gSoundSlotTbl[0] = *(SoundChannelSlot *)pEntry;
        LockSoundMixer();
        LoadWaveDataOntoSoundChannel(0, (void *)((int)pEntry + 0x32));
        SetSoundChannelLoopGateFlags(0, (unsigned int)(pEntry[6] != 0));
        SetSoundChannelGain(0, (void *)((int)pEntry + 0x59));
        UnlockSoundMixer(1);
    }
}


/* The sound node entry accessors. An entry's first 0x68 bytes are the SoundChannelSlot a channel
 * copies (+0 key, +4 volume, +6 pan, +0x18 loop link, +0x36 data, +0x59 gain triple); the setters
 * also update every busy channel playing the node (slot +0x36, DAT_00489f2e for slot 0, equal to
 * the entry's). */

/* Returns the entry's priority key (+0). */
unsigned int __cdecl GetSoundNodeKey(unsigned int *pEntry)  /* 0x00425070 */
{
    return *pEntry;
}


/* Sets the entry's priority key (+0). */
void __cdecl SetSoundNodeKey(unsigned int *pEntry, unsigned int val)  /* 0x00425080 */
{
    *pEntry = val;
}


/* Returns the entry's volume (+4, 0..0xff). */
short __cdecl GetSoundNodeVolume(int nodeCtx)  /* 0x00425090 */
{
    return *(short *)(nodeCtx + 4);
}


/* Sets the entry's volume (+4) when sound is on and it changed: repacks the gain triple (+0x59) with
 * the pan and applies it to every channel playing the node. */
void __cdecl SetSoundNodeVolume(byte *pEntry, short volume)  /* 0x004250a0 */
{
    int   idle;
    int   chanIdx;
    short i;
    GainTriple blk;   /* copied whole (movs of 4+4+4+2+1) */

    if (gSoundOn != 0) {
        if (*(short *)(pEntry + 4) != volume) {
            *(short *)(pEntry + 4) = volume;
            PackGainTripleFromVolumePan((int *)&blk, volume, *(short *)(pEntry + 6));
            *(GainTriple *)(pEntry + 0x59) = blk;
            i = 0;
            LockSoundMixer();
            do {
                chanIdx = (int)i;
                idle = IsSoundChannelIdle(chanIdx);
                if ((idle == 0) &&
                    (gSoundSlotTbl[chanIdx].templateKey == *(int *)(pEntry + 0x36))) {
                    SetSoundChannelGain(chanIdx, &blk);
                    gSoundSlotTbl[chanIdx].gain = blk;
                }
                i++;
            } while (i < 4);
            UnlockSoundMixer(0);
        }
    }
}


/* Returns the entry's pan (+6, 0x80 centre). */
short __cdecl GetSoundNodePan(int nodeCtx)  /* 0x00425190 */
{
    return *(short *)(nodeCtx + 6);
}


/* SetSoundNodeVolume for the pan (+6). */
void __cdecl SetSoundNodePan(byte *pEntry, short pan)  /* 0x004251a0 */
{
    int   idle;
    int   chanIdx;
    short i;
    GainTriple blk;

    if (gSoundOn != 0) {
        if (*(short *)(pEntry + 6) != pan) {
            *(short *)(pEntry + 6) = pan;
            PackGainTripleFromVolumePan((int *)&blk, *(short *)(pEntry + 4), pan);
            *(GainTriple *)(pEntry + 0x59) = blk;
            i = 0;
            LockSoundMixer();
            do {
                chanIdx = (int)i;
                idle = IsSoundChannelIdle(chanIdx);
                if ((idle == 0) &&
                    (gSoundSlotTbl[chanIdx].templateKey == *(int *)(pEntry + 0x36))) {
                    SetSoundChannelGain(chanIdx, &blk);
                    gSoundSlotTbl[chanIdx].gain = blk;
                }
                i++;
            } while (i < 4);
            UnlockSoundMixer(0);
        }
    }
}


/* Returns 1 if the entry loops (its +0x18 link is set). */
short __cdecl GetSoundNodeLoop(int nodeCtx)  /* 0x00425290 */
{
    return (*(int *)(nodeCtx + 0x18) != 0);
}


/* Makes the entry loop (its +0x18 link points at itself) or not, when sound is on, and sets the loop
 * gate of every channel playing it -- channels 0..2 only: the exe's loop stops before the theme. */
void __cdecl SetSoundNodeLoop(int pEntry, short loop)  /* 0x004252a0 */
{
    int   idle;
    short i;
    int   chanIdx;

    if (gSoundOn != 0) {
        if (loop != 0) {
            *(int *)(pEntry + 0x18) = pEntry;
        } else {
            *(int *)(pEntry + 0x18) = 0;
        }
        i = 0;
        do {
            chanIdx = (int)i;
            idle = IsSoundChannelIdle(chanIdx);
            if ((idle == 0) &&
                (gSoundSlotTbl[chanIdx].templateKey == *(int *)(pEntry + 0x36))) {
                LockSoundMixer();
                SetSoundChannelLoopGateFlags(chanIdx, (int)loop);
                gSoundSlotTbl[chanIdx].flag = pEntry;
                if (loop == 0) {
                    gSoundSlotTbl[chanIdx].flag = 0;
                }
                UnlockSoundMixer(0);
            }
            i++;
        } while (i < 3);
    }
}


/* Silences every busy channel playing entry pEntry's resource (slot +0x28/+0x24 equal to the entry's
 * group and node). The walk ends at the table's end, which is where gSoundStereo sits. */
void __cdecl DeactivateSoundNodeEntry(int pEntry)  /* 0x00425340 */
{
    int  channelIndex;
    int *pChannelSlot;

    if (gSoundOn != 0) {
        channelIndex = 0;
        ClearInactiveSoundChannelTemplateKeys();
        pChannelSlot = (int *)gSoundSlotTbl;
        do {
            if (((*pChannelSlot != 0) && (*(int *)(pEntry + 0x28) == pChannelSlot[10])) &&
                (*(int *)(pEntry + 0x24) == pChannelSlot[9])) {
                SilenceSoundChannel(channelIndex);
            }
            pChannelSlot = pChannelSlot + 0x1a;
            channelIndex = channelIndex + 1;
        } while (pChannelSlot < &gSoundStereo);
    }
}


/* Returns 1 while the voice row's sound still plays: a busy channel bound to its group and node
 * (+0x28/+0x24), or a wave-mix slot TryResolveWaveMixSlotForVoiceSource still finds for its data
 * (+0x36). 0 when sound is off. */
short __cdecl IsVoicePlaybackStillActive(int pVoiceRow)  /* 0x00425390 */
{
  int *p;
  int slotOut;  /* local_4 */

  /* the end test is the table's end, 0x0048a098 = 0x00489ef8 + 4 x 0x68; Ghidra
   * shows a literal 0x48a097 compare, meaningless here (class 5) */
  if (gSoundOn == 0)
    return 0;
  ClearInactiveSoundChannelTemplateKeys();
  for (p = (int *)gSoundSlotTbl; p < (int *)gSoundSlotTbl + 104; p += 0x1a) {
    if (*p != 0 && p[10] == *(int *)(pVoiceRow + 0x28) && p[9] == *(int *)(pVoiceRow + 0x24))
      return 1;
  }
  return TryResolveWaveMixSlotForVoiceSource(*(int *)(pVoiceRow + 0x36), &slotOut) == 0;
}

