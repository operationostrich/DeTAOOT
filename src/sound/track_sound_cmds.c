/* track_sound_cmds.c -- TI.EXE 0x0044c410..0x0044dbd0: track files and the sound script commands. */

#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdint.h>
#include "df_protos.h"
#include "topics/input.h"
#include "topics/gameloop.h"
#include "topics/scene.h"
#include "topics/reset_channel.h"
#include "topics/scriptcmd.h"
#include "topics/scriptquery.h"
#include "topics/scriptprop.h"


#include "topics/savestate.h"

/* opentrackfile name (opcode 12019): checks the named file is a 'SONG' file, opens it with
 * OpenTrackNode and appends the 40-byte track row to gTrackTable, then renumbers every sound
 * node's track index.
 *
 * Track row layout (0x28 bytes, the record FindTrackByName copies out): +0 render node handle,
 * +4 short sound count, +6 short template count, +8 short theme-list count, +10 sound node array,
 * +14 template array, +18 theme list (each node 0x68 bytes), +0x16 Pascal name, +0x26 short
 * count of on-demand sound nodes currently loaded. */
short __cdecl ActionCmd_OpenTrackFile(int pCtx, short *pArgBlock,
                                     int *pTable, int *pSubSkip)  /* 0x0044c410 */
{
    /* the 40-byte track row, appended as one object (rep movsd, ecx = 10) */
    typedef struct { int w[10]; } Slot2Entry40;
    short        u;
    short        argResult[4];
    Slot2Entry40 entry;

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(argResult, (short *)gScratchStr2);
    if (u != 0)
        return u;
    u = ValidateDescriptorPath(gScratchStr2, 0x534f4e47);   /* 'SONG' */
    if (u != 0)
        return u;
    u = OpenTrackNode(gScratchStr2, (int *)&entry);
    if (u != 0)
        return u;

    gTrackCount++;
    ResizeResObject(gTrackTable, (unsigned int)(gTrackCount * 0x28));
    ((Slot2Entry40 *)GetResDataPtr(gTrackTable))[gTrackCount - 1] = entry;
    UnlockResObject(gTrackTable);
    AssignSoundNodeTrackIndices();
    return 0;
}


/* Opens the track file named by pDesc and fills the 40-byte track row at pOut (layout at
 * ActionCmd_OpenTrackFile). Returns 0x38 ("Must use DF 4.0 files") unless the root record's
 * version is 0x40000; any render-node failure is fatal. The root record names two children
 * (+0x1c, +0x20): the first holds the templates and the theme list (template indices, chained
 * through node +0x18, the last link going back to the loop point), the second the sounds. A
 * sound whose flag bit 0 is set is loaded on demand (kind 1 at node +0x2c), the others at once
 * (kind 2).
 *
 * Port note: written from the disassembly in its own shape (walking pointers, the 0x68-byte
 * node copied as one struct); same behaviour as the Ghidra transcription it replaced. */
short __cdecl OpenTrackNode(byte *pDesc, int *pOut)  /* 0x0044c4f0 */
{
    typedef struct { int v[0x1a]; } Node68;
    short  st;
    short  nKept;      /* local_14 */
    short  i;          /* local_12 */
    int   *row;
    int   *hRoot;      /* local_c */
    int   *hA;         /* local_8 */
    int   *hB;         /* local_4 */
    char  *p5;
    char  *w;
    char  *buf;
    char  *pd;
    int    lim;
    short  sel;
    char  *next;

    st = TakeMatchingPendingRenderNode(pDesc, pOut);
    if (st != 0)
        FatalError(st, 0x1029);
    st = AttachRenderChild((int *)*pOut, 0, &hRoot);
    if (st != 0)
        FatalError(st, 0x102a);
    MoveResObjectToHeapBottom(hRoot);
    row = (int *)GetResDataPtr(hRoot);
    if (*row != 0x40000) {
        UnlockResObject(hRoot);
        st = DetachRenderChild(*pOut, 0);
        if (st != 0)
            FatalError(st, 0x102b);
        st = DeferRenderNodeClose(*pOut);
        if (st != 0)
            FatalError(st, 0x102c);
        return 0x38;
    }
    *(short *)((char *)pOut + 0x26) = 0;
    CopyPascalString((byte *)(row + 9), (byte *)pOut + 0x16);
    st = AttachRenderChild((int *)*pOut, row[7], &hA);
    if (st != 0)
        FatalError(st, 0x102d);
    MoveResObjectToHeapBottom(hA);
    p5 = (char *)GetResDataPtr(hA);
    *(short *)((char *)pOut + 6) = (short)*(int *)(p5 + 0x10a);
    *(char **)((char *)pOut + 0xe) = (char *)AllocEngineBuffer(*(int *)(p5 + 0x10a) * 0x68);
    w = p5 + 0x10e;
    buf = *(char **)((char *)pOut + 0xe);
    i = 0;
    if (*(int *)(p5 + 0x10a) > 0) {
        do {
            InitSoundNodeEntryFromResource((unsigned int *)*pOut, *(unsigned int *)(w + 4), (byte *)w + 0xa, (unsigned int *)buf);
            w += 0x1a;
            buf += 0x68;
            i++;
        } while (i < *(int *)(p5 + 0x10a));
    }
    nKept = 0;
    *(char **)((char *)pOut + 0x12) = (char *)AllocEngineBuffer(*(short *)(p5 + 4) * 0x68);
    buf = *(char **)((char *)pOut + 0x12);
    i = 0;
    if (*(short *)(p5 + 4) > 0) {
        do {
            sel = *(short *)(p5 + i * 2 + 6);
            if (sel > 0 && sel <= *(int *)(p5 + 0x10a)) {
                next = buf + 0x68;
                if (*(short *)(p5 + 4) - i == 1) {
                    lim = *(int *)p5;
                    if (lim >= *(int *)(p5 + 0x10a))
                        lim = *(int *)(p5 + 0x10a) - 1;
                    if (lim < 0)
                        lim = 0;
                    next = *(char **)((char *)pOut + 0x12) + lim * 0x68;
                }
                *(Node68 *)buf = *(Node68 *)(*(char **)((char *)pOut + 0xe) + sel * 0x68 - 0x68);
                nKept++;
                *(char **)(buf + 0x18) = next;
                buf += 0x68;
            }
            i++;
        } while (*(short *)(p5 + 4) > i);
    }
    *(short *)(pOut + 2) = nKept;
    UnlockResObject(hA);
    st = DetachRenderChild(*pOut, row[7]);
    if (st != 0)
        FatalError(st, 0x102e);
    st = AttachRenderChild((int *)*pOut, row[8], &hB);
    if (st != 0)
        FatalError(st, 0x102f);
    MoveResObjectToHeapBottom(hB);
    p5 = (char *)GetResDataPtr(hB);
    *(short *)(pOut + 1) = (short)*(int *)(p5 + 4);
    *(char **)((char *)pOut + 10) = (char *)AllocEngineBuffer(*(int *)(p5 + 4) * 0x68);
    w = p5 + 8;
    buf = *(char **)((char *)pOut + 10);
    i = 0;
    if (*(int *)(p5 + 4) > 0) {
        do {
            if ((*w & 1) != 0) {
                InitSoundNodeEntry(*pOut, *(uint32_t *)(w + 4), (byte *)w + 10, (uint32_t *)buf);
                *(short *)(buf + 0x2c) = 1;
            } else {
                InitSoundNodeEntryFromResource((unsigned int *)*pOut, *(unsigned int *)(w + 4), (byte *)w + 10, (unsigned int *)buf);
                *(short *)(buf + 0x2c) = 2;
            }
            *(int *)buf = i + 1;
            w += 0x1a;
            buf += 0x68;
            i++;
        } while (i < *(int *)(p5 + 4));
    }
    UnlockResObject(hB);
    st = DetachRenderChild(*pOut, row[8]);
    if (st != 0)
        FatalError(st, 0x1030);
    UnlockResObject(hRoot);
    st = DetachRenderChild(*pOut, 0);
    if (st != 0)
        FatalError(st, 0x1031);
    return 0;
}


/* closetrackfile name (opcode 12020): closes the named track with CloseTrackNode and removes
 * its row from gTrackTable, moving the later rows down and shrinking the table. */
short __cdecl ActionCmd_CloseTrackFile(int pCtx, short *pArgBlock,
                                       int *pTable, int *pSubSkip)  /* 0x0044c8a0 */
{
    void *pDst;
    short  status;
    int   moveBytes;
    int   slotIdx;
    short err;
    short argResult[4];
    int   entryBuf[10];    /* the 40-byte track row from FindTrackByName */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                               (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = FindTrackByName(gScratchStr, entryBuf);
    if (status != 0)
        return status;

    err = FindTrackIndexByNode(entryBuf[0], &slotIdx);
    if (err != 0)
        return err;

    CloseTrackNode(entryBuf);
    pDst = (void *)GetResDataPtr(gTrackTable);
    moveBytes = (gTrackCount - slotIdx - 1) * 0x28;
    memmove((char *)pDst + slotIdx * 0x28, (char *)pDst + slotIdx * 0x28 + 0x28, moveBytes);
    UnlockResObject(gTrackTable);
    gTrackCount--;
    ResizeResObject(gTrackTable, (unsigned int)(gTrackCount * 0x28));
    return 0;
}


/* Silences the track's channels, tears down its loaded sound nodes and templates, frees the three
 * node arrays and closes its render node. A loaded on-demand node (kind 0) decrements the row's
 * count at +0x26; the count must reach zero or it is a fatal error. */
void __cdecl CloseTrackNode(int *pTrack)  /* 0x0044c9a0 */
{
    int   pRow;
    short rowIdx;
    short tmplIdx;
    int   pTmplRow;                        /* the second loop has its own pair: the registers differ */

    rowIdx = 0;
    SilenceSoundChannelsByHandle(*pTrack);
    pRow = *(int *)((int)pTrack + 10);
    if (0 < (short)pTrack[1]) {
        do {
            RemoveCricketEntry((byte *)(pRow + 8));
            switch (*(short *)(pRow + 0x2c)) {   /* movsx + a compare chain: a switch on the row kind */
            case 0:
                TeardownVoicePlaybackRow(pRow);
                *(short *)((int)pTrack + 0x26) = *(short *)((int)pTrack + 0x26) - 1;
                break;
            case 2:
                TeardownVoicePlaybackRow(pRow);
                break;
            }
            rowIdx++;
            pRow += 0x68;
        } while (rowIdx < (short)pTrack[1]);
    }
    if (*(short *)((int)pTrack + 0x26) != 0) {
        FatalError(0x6e, 0x1032);
    }
    tmplIdx = 0;
    pTmplRow = *(int *)((int)pTrack + 0xe);
    if (0 < *(short *)((int)pTrack + 6)) {
        do {
            tmplIdx++;
            TeardownVoicePlaybackRow(pTmplRow);
            pTmplRow += 0x68;
        } while (tmplIdx < *(short *)((int)pTrack + 6));
    }
    FreeEngineBuffer((void *)*(int *)((int)pTrack + 10));
    FreeEngineBuffer((void *)*(int *)((int)pTrack + 0xe));
    FreeEngineBuffer((void *)*(int *)((int)pTrack + 0x12));
    tmplIdx = DeferRenderNodeClose(*pTrack);
    if (tmplIdx != 0) {
        FatalError(tmplIdx, 0x1033);
    }
}


/* playtheme name (opcode 12021): plays the named track's theme list (the row's +18), if it
 * has one (count at +8).
 *
 * Port note: entryBuf was int[4] and the 40-byte row overran it, and the theme list was read
 * at +10 instead of +18, which crashed in the codec (port-bug-classes.md #3). */
short __cdecl ActionCmd_PlayTheme(int pCtx, short *pArgBlock,
                                     int *pTable, int *pSubSkip)  /* 0x0044ca80 */
{
    short   status;
    short  argResult[4];
    int    entryBuf[10];    /* the 40-byte track row written by FindTrackByName */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                               (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = FindTrackByName(gScratchStr, entryBuf);
    if (status != 0)
        return status;

    if (*(short *)((char *)entryBuf + 8) > 0)
        PlayThemeList(*(void **)((char *)entryBuf + 18));
    status = 0; /* PlayThemeList returns nothing */

    return status & 0xffff0000;
}


/* singlesound name (opcode 12022): plays the named sound node through AssignSoundChannel.
 * The four handlers that follow differ only in the channel call; each returns 0 through the
 * `status & 0xffff0000` spelling (port-bug-classes.md #18). */
short __cdecl ActionCmd_SingleSound(int pCtx, short *pArgBlock, int *pTable,
                                   int *pSubSkip)  /* 0x0044cb00 */
{
    short status;
    int  nodeCtx;

    status = ResolveNodeFromScript(pCtx, pArgBlock, pTable, pSubSkip, &nodeCtx);
    if (status != 0)
        return status;

    AssignSoundChannel((int *)nodeCtx);
    return status & 0xffff0000;
}


/* multiplesound name (opcode 12023): plays the named sound node through ReplaceSoundChannel. */
short __cdecl ActionCmd_MultipleSound(int pCtx, short *pArgBlock, int *pTable,
                                     int *pSubSkip)  /* 0x0044cb40 */
{
    short status;
    int  nodeCtx;

    status = ResolveNodeFromScript(pCtx, pArgBlock, pTable, pSubSkip, &nodeCtx);
    if (status != 0)
        return status;

    ReplaceSoundChannel((int *)nodeCtx);
    return status & 0xffff0000;
}


/* dualsound name (opcode 12024): plays the named sound node through UpgradeSoundChannels. */
short __cdecl ActionCmd_DualSound(int pCtx, short *pArgBlock, int *pTable,
                                 int *pSubSkip)  /* 0x0044cb80 */
{
    short status;
    int  nodeCtx;

    status = ResolveNodeFromScript(pCtx, pArgBlock, pTable, pSubSkip, &nodeCtx);
    if (status != 0)
        return status;

    UpgradeSoundChannels((int *)nodeCtx);
    return status & 0xffff0000;
}


/* bothsound name (opcode 12025): plays the named sound node through ForceAssignSoundChannels. */
short __cdecl ActionCmd_BothSound(int pCtx, short *pArgBlock, int *pTable,
                                 int *pSubSkip)  /* 0x0044cbc0 */
{
    short status;
    int  nodeCtx;

    status = ResolveNodeFromScript(pCtx, pArgBlock, pTable, pSubSkip, &nodeCtx);
    if (status != 0)
        return status;

    ForceAssignSoundChannels((int *)nodeCtx);
    return status & 0xffff0000;
}


/* voicesound name (opcode 12026): plays the named sound node on the primary (voice) channel
 * through AssignPrimarySoundChannel. */
short __cdecl ActionCmd_VoiceSound(int pCtx, short *pArgBlock, int *pTable,
                                  int *pSubSkip)  /* 0x0044cc00 */
{
    short status;
    int  nodeCtx;

    status = ResolveNodeFromScript(pCtx, pArgBlock, pTable, pSubSkip, &nodeCtx);
    if (status != 0)
        return status;

    AssignPrimarySoundChannel((int *)nodeCtx);
    return status & 0xffff0000;
}


/* halttheme (opcode 12029), no arguments: stops the theme channel (channel 3). StopSoundChannels'
 * four flags are channels 1, 2 (the two sound channels), 3 (theme) and 0 (voice). */
short __cdecl ActionCmd_HaltTheme(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0044cc40 */
{
    *pSubSkip = 0;                      /* the exe stores through [esp+0x10]: the 4th argument */
    StopSoundChannels(0, 0, 1, 0);
    return 0;
}


/* haltsound n (opcode 12028): stops sound channel 1, 2, or both for n = 3. Returns 0x0e ("Bad
 * argument type") when n is not an int or is out of range. */
short __cdecl ActionCmd_HaltSound(int pCtx, short *pArgBlock, int *pTable,
                                 int *pSubSkip)  /* 0x0044cc60 */
{
    short  status;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                               (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    if (argResult[0] != 4)
        return 0x0e;

    switch (*(int *)(argResult + 1)) {   /* the exe compares 1, 2, 3 first and places the bodies after */
    case 1:
        StopSoundChannels(1, 0, 0, 0);
        return 0;
    case 2:
        StopSoundChannels(0, 1, 0, 0);
        return 0;
    case 3:
        StopSoundChannels(1, 1, 0, 0);
        return 0;
    default:
        return 0x0e;
    }
}


/* haltvoice (opcode 12030), no arguments: stops the voice channel (channel 0). */
short __cdecl ActionCmd_HaltVoice(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0044cd00 */
{
    *pSubSkip = 0;                      /* as ActionCmd_HaltTheme */
    StopSoundChannels(0, 0, 0, 1);
    return 0;
}


/* soundvol name, n -- the statement half of opcode 16040: sets the named sound node's volume,
 * clamped to 0..0xff. 0x0fb4 is the comma token; a missing comma is error 0x1c ("Expected comma"),
 * a non-int n 0x0e. The skip count covers the name, the comma and n. */
short __cdecl
ActionCmd_SoundVol(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0044cd20 */
{
    typedef struct { short w[4]; } ArgSlot8;
    short  u;
    int    skip;
    short  arg2[4];
    int    nodeCtx;
    int    start;

    start = pArgBlock;
    u = ResolveNodeFromScript(pCtx, (short *)pArgBlock, pTable, &skip, &nodeCtx);
    if (u != 0)
        return u;
    pArgBlock += skip * 8;
    if (*(short *)pArgBlock != 0x0fb4)
        return 0x1c;
    pArgBlock += 8;
    u = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, (int *)arg2, &skip);
    if (u != 0)
        return u;
    pArgBlock += skip * 8;
    *pSubSkip = (pArgBlock - start) >> 3;
    if (arg2[0] != 4)
        return 0x0e;
    if (*(int *)&arg2[1] < 0)
        *(int *)&arg2[1] = 0;
    if (*(int *)&arg2[1] > 0xff)
        *(int *)&arg2[1] = 0xff;
    SetSoundNodeVolume((byte *)nodeCtx, (short)*(int *)&arg2[1]);
    return 0;
}


/* themevol track, n -- the statement half of opcode 16041: sets the volume of every template
 * and theme-list node of the named track, n clamped to 0..0xff.
 *
 * Port note: four parameters, pTable passed through to EvalTwoScriptArgs; with a literal 0 an
 * identifier argument failed with error 9 ("No such variable"), port-bug-classes.md #2. */
short __cdecl
ActionCmd_ThemeVol(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0044cdf0 */
{
    short  s1;
    short  a1[4], a2[4];
    short  i;
    int    slot2Entry[10];
    byte  *pWalk;

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    if (s1 != 0)
        return s1;
    s1 = TakeScriptString(a1, (byte *)gScratchStr);
    if (s1 != 0)
        return s1;
    s1 = FindTrackByName((byte *)gScratchStr, slot2Entry);
    if (s1 != 0)
        return s1;
    if (a2[0] != 4)
        return 0x0e;
    if (*(int *)&a2[1] < 0)
        *(int *)&a2[1] = 0;
    if (*(int *)&a2[1] > 0xff)
        *(int *)&a2[1] = 0xff;
    pWalk = (byte *)*(int *)((char *)slot2Entry + 14);
    for (i = 0; i < *(short *)((char *)slot2Entry + 6); i++) {
        SetSoundNodeVolume(pWalk, (short)*(int *)&a2[1]);
        pWalk += 0x68;
    }
    pWalk = (byte *)*(int *)((char *)slot2Entry + 18);
    for (i = 0; i < *(short *)((char *)slot2Entry + 8); i++) {
        SetSoundNodeVolume(pWalk, (short)*(int *)&a2[1]);
        pWalk += 0x68;
    }
    return 0;
}


/* themepan track, n -- the statement half of opcode 16054: ActionCmd_ThemeVol with SetSoundNodePan.
 *
 * Port note: four parameters as ActionCmd_ThemeVol; the body was re-derived from the disassembly
 * (the previous one evaluated arguments it never used), port-bug-classes.md #2 / #3. */
short __cdecl
ActionCmd_ThemePan(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0044cef0 */
{
    short  s1;
    short  a1[4], a2[4];
    short  i;
    int    slot2Entry[10];
    byte  *pWalk;

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    if (s1 != 0)
        return s1;
    s1 = TakeScriptString(a1, (byte *)gScratchStr);
    if (s1 != 0)
        return s1;
    s1 = FindTrackByName((byte *)gScratchStr, slot2Entry);
    if (s1 != 0)
        return s1;
    if (a2[0] != 4)
        return 0x0e;
    if (*(int *)&a2[1] < 0)
        *(int *)&a2[1] = 0;
    if (*(int *)&a2[1] > 0xff)
        *(int *)&a2[1] = 0xff;
    pWalk = (byte *)*(int *)((char *)slot2Entry + 14);
    for (i = 0; i < *(short *)((char *)slot2Entry + 6); i++) {
        SetSoundNodePan(pWalk, (short)*(int *)&a2[1]);
        pWalk += 0x68;
    }
    pWalk = (byte *)*(int *)((char *)slot2Entry + 18);
    for (i = 0; i < *(short *)((char *)slot2Entry + 8); i++) {
        SetSoundNodePan(pWalk, (short)*(int *)&a2[1]);
        pWalk += 0x68;
    }
    return 0;
}


/* soundpan name, n -- the statement half of opcode 16038: ActionCmd_SoundVol with SetSoundNodePan. */
short __cdecl
ActionCmd_SoundPan(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0044cff0 */
{
    typedef struct { short w[4]; } ArgSlot8;
    short  u;
    int    skip;
    short  arg2[4];
    int    nodeCtx;
    int    start;

    start = pArgBlock;
    u = ResolveNodeFromScript(pCtx, (short *)pArgBlock, pTable, &skip, &nodeCtx);
    if (u != 0)
        return u;
    pArgBlock += skip * 8;
    if (*(short *)pArgBlock != 0x0fb4)
        return 0x1c;
    pArgBlock += 8;
    u = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, (int *)arg2, &skip);
    if (u != 0)
        return u;
    pArgBlock += skip * 8;
    *pSubSkip = (pArgBlock - start) >> 3;
    if (arg2[0] != 4)
        return 0x0e;
    if (*(int *)&arg2[1] < 0)
        *(int *)&arg2[1] = 0;
    if (*(int *)&arg2[1] > 0xff)
        *(int *)&arg2[1] = 0xff;
    SetSoundNodePan((byte *)nodeCtx, (short)*(int *)&arg2[1]);
    return 0;
}


/* soundloop name, flag -- the statement half of opcode 16039: sets the named sound node's loop
 * flag; the second argument must be a short (type 2), else error 0x0e. */
short __cdecl
ActionCmd_SoundLoop(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0044d0c0 */
{
    typedef struct { short w[4]; } ArgSlot8;
    short  u;
    int    skip;
    short  arg2[4];
    int    nodeCtx;
    int    start;

    start = pArgBlock;
    u = ResolveNodeFromScript(pCtx, (short *)pArgBlock, pTable, &skip, &nodeCtx);
    if (u != 0)
        return u;
    pArgBlock += skip * 8;
    if (*(short *)pArgBlock != 0x0fb4)
        return 0x1c;
    pArgBlock += 8;
    u = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, (int *)arg2, &skip);
    if (u != 0)
        return u;
    pArgBlock += skip * 8;
    *pSubSkip = (pArgBlock - start) >> 3;
    if (arg2[0] != 2)
        return 0x0e;
    SetSoundNodeLoop(nodeCtx, arg2[1]);
    return 0;
}


/* soundrate(name) (opcode 20080): the int at +0x1c of the record the named sound node points
 * to at its own +0x1c. Query results are {short type, int value, short 0}: type 4 = int,
 * type 2 = short. */
short ScriptCmd_SoundRate(int *pCtx, short *pArgBlock, int *pTable,
                                                   short *pResult, int *pSkipCount) /* 0x0044d170 */
{
    short st;
    int   nodeEntry;

    st = ResolveNodeFromScript((int)pCtx, pArgBlock, pTable, pSkipCount, &nodeEntry);
    if (st != 0)
        return st;
    *(unsigned int *)(pResult + 1) = *(unsigned int *)(*(intptr_t *)((intptr_t)nodeEntry + 0x1c) + 0x1c);
    *pResult = 4;
    pResult[3] = 0;
    return 0;
}


/* themevol(track) (opcode 16041): the volume of the named track's first template node (the
 * array at +14), or 0 when the track has no templates (count at +6).
 *
 * Port note: nodeData was short[12] and the 40-byte row overran it (port-bug-classes.md #3). */
short ScriptCmd_ThemeVol(int *pCtx, short *pArgBlock, int *pTable,
                               short *pResult, int *pSkipCount)  /* 0x0044d1c0 */
{
    short  status;
    short argResult[4];
    short nodeData[20];  /* the 40-byte track row from FindTrackByName */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = FindTrackByName((byte *)&gScratchStr, nodeData);
    if (status != 0)
        return status;

    if (nodeData[3] > 0) {  /* template count, byte +6 */
        *(int *)(pResult + 1) = GetSoundNodeVolume(*(int *)(nodeData + 7));  /* template array, byte +14 */
    } else {
        *(int *)(pResult + 1) = 0;
    }
    pResult[3] = 0;
    *pResult = 4;   /* type: int */
    return 0;
}


/* themepan(track) (opcode 16054): ScriptCmd_ThemeVol with GetSoundNodePan.
 *
 * Port note: the original's local_28 / local_22 / local_1a are one 40-byte row; declared
 * separately the row overran a 4-byte local (port-bug-classes.md #3). */
short ScriptCmd_ThemePan(int *pCtx, short *pArgBlock, int *pTable,
                                short *pResult, int *pSkipCount)  /* 0x0044d260 */
{
    short  status;
    short argResult[4];
    int   nodeRec[10];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = FindTrackByName(&gScratchStr, nodeRec);
    if (status != 0)
        return status;

    if (*(short *)((char *)nodeRec + 6) > 0) {   /* read in place, as the exe does */
        *(int *)(pResult + 1) = GetSoundNodePan(*(int *)((char *)nodeRec + 14));
    } else {
        *(int *)(pResult + 1) = 0;
    }
    pResult[3] = 0;
    *pResult = 4;   /* type: int */
    return 0;
}


/* soundvol(name) (opcode 16040): the named sound node's volume, as an int. */
short ScriptCmd_SoundVol(int *pCtx, short *pArgBlock, int *pTable,
                           short *pResult, int *pSkipCount)  /* 0x0044d300 */
{
    short  status;
    int   nodeCtx;  /* out-param of ResolveNodeFromScript */
    int   v;

    status = ResolveNodeFromScript((int)pCtx, pArgBlock, pTable, pSkipCount, &nodeCtx);
    if (status != 0)
        return status;

    v = GetSoundNodeVolume(nodeCtx);              /* widened before the stores, as the exe does */
    *(int *)(pResult + 1) = v;
    *pResult = 4;   /* type: int */
    pResult[3] = 0;
    return 0;
}


/* soundpan(name) (opcode 16038): the named sound node's pan, as an int. */
short ScriptCmd_SoundPan(int *pCtx, short *pArgBlock, int *pTable,
                          short *pResult, int *pSkipCount)  /* 0x0044d360 */
{
    short  status;
    int   nodeCtx;  /* out-param of ResolveNodeFromScript */
    int   v;

    status = ResolveNodeFromScript((int)pCtx, pArgBlock, pTable, pSkipCount, &nodeCtx);
    if (status != 0)
        return status;

    v = GetSoundNodePan(nodeCtx);              /* widened before the stores, as the exe does */
    *(int *)(pResult + 1) = v;
    *pResult = 4;   /* type: int */
    pResult[3] = 0;
    return 0;
}


/* soundloop(name) (opcode 16039): the named sound node's loop flag, as a short. */
short ScriptCmd_SoundLoop(int *pCtx, short *pArgBlock, int *pTable,
                           short *pResult, int *pSkipCount)  /* 0x0044d3c0 */
{
    short  status;
    int   nodeCtx;  /* out-param of ResolveNodeFromScript */

    status = ResolveNodeFromScript((int)pCtx, pArgBlock, pTable, pSkipCount, &nodeCtx);
    if (status != 0)
        return status;
    *(int *)(pResult + 1) = (short)GetSoundNodeLoop(nodeCtx);
    *pResult = 2;   /* type: short */
    pResult[3] = 0;
    return 0;
}


/* sounddone (opcode 20016), no arguments: AreSoundChannelsIdle's flag, as a short. */
short ScriptCmd_SoundDone(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x0044d420 */
{
    BOOL idle;

    *pSkipCount = 0;
    idle = AreSoundChannelsIdle();
    *(int *)(pResult + 1) = (int)(short)idle;
    *pResult = 2;   /* type: short */
    pResult[3] = 0;
    return 0;
}


/* voicedone (opcode 20097), no arguments: CheckSoundChannelStopped's flag, as a short. */
short __cdecl ScriptCmd_VoiceDone(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x0044d450 */
{
    int v;

    *pSkipCount = 0;
    v = CheckSoundChannelStopped();     /* before the type store, as the exe calls it */
    *(int *)(pResult + 1) = v;
    *pResult = 2;
    pResult[3] = 0;
    return 0;
}


/* countsounds(track) (opcode 20014): the named track's sound count (row +4), as an int.
 *
 * Port note: the original's local_28 / local_24 are one 40-byte row; declared separately the
 * count read uninitialised and the row overran a 4-byte local (port-bug-classes.md #3). */
short ScriptCmd_CountSounds(int *pCtx, short *pArgBlock, int *pTable,
                                short *pResult, int *pSkipCount) /* 0x0044d480 */
{
    short  status;
    short argResult[4];
    int   nodeRec[10];
    short nodeField;

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = FindTrackByName(&gScratchStr, nodeRec);
    if (status != 0)
        return status;

    nodeField = *(short *)((char *)nodeRec + 4);

    *pResult = 4;   /* type: int */
    *(int *)(pResult + 1) = (int)nodeField;
    pResult[3] = 0;
    return 0;
}


/* indextosound(track, n) (opcode 20015): the name (node +8) of the named track's n-th sound,
 * 1-based, as a string. Error 0x0e if n is not an int, 10 ("No such object") if it is out of range.
 *
 * Port note: the port had dropped pSkipCount (error 2, "Expected paren", port-bug-classes.md #2)
 * and had the two arguments swapped, so every call failed with 0x0e. */
short ScriptCmd_IndexToSound(int *pCtx, int pArgBlock, int *pTable,
                                       short *pResult, int *pSkipCount) /* 0x0044d500 */
{
    typedef struct { byte b[0x68]; } SoundChild68;
    short trackArg[4];
    short indexArg[4];
    int   nodeRec[10];
    short status;
    uint off;
    byte *pb;

    status = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)trackArg, (int *)indexArg, pSkipCount);
    if (status != 0)
        return status;
    status = TakeScriptString(trackArg, &gScratchStr);
    if (status != 0)
        return status;
    status = FindTrackByName(&gScratchStr, nodeRec);
    if (status != 0)
        return status;
    if (indexArg[0] != 4)
        return 0x0e;
    if (*(int *)(indexArg + 1) < 1 || *(short *)((char *)nodeRec + 4) < *(int *)(indexArg + 1))
        return 10;
    /* child[index - 1] + 8, the 0x68-byte stride: offset first, then the base */
    off = *(int *)(indexArg + 1) * 0x68;
    pb = (byte *)(*(int *)((char *)nodeRec + 10) - 0x60);
    return AllocScriptString(pb + off, pResult);
}


/* counttracks (opcode 20075), no arguments: the number of open tracks, as an int. */
short ScriptCmd_CountTracks(int *pCtx, short *pArgBlock, int *pTable,
                                       short *pResult, int *pSkipCount) /* 0x0044d5b0 */
{
    (void)pCtx;
    (void)pArgBlock;
    (void)pTable;

    *pSkipCount          = 0;
    *(uint *)(pResult + 1) = (uint)gTrackCount;
    *pResult             = 4;
    pResult[3]           = 0;
    return 0;
}


/* indextotrack(n) (opcode 20076): the name of the n-th open track, 1-based, as a string.
 * Error 0x0e if n is not an int, 10 if it is out of range.
 *
 * Port note: the name is at row +0x16; the port passed +0x1e, local_12's distance from the frame
 * base rather than from the row. */
short ScriptCmd_IndexToTrack(int *pCtx, short *pArgBlock, int *pTable,
                                                    short *pResult, int *pSkipCount) /* 0x0044d5e0 */
{
    typedef struct { int d[10]; } Slot2Rec40;
    short arg[4];          /* {type, int value (unaligned at +2)}: frame +0 */
    short status;
    Slot2Rec40 rec;        /* the 40-byte track row, copied whole: frame +8 */
    char *base;

    status = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)arg, pSkipCount);
    if (status != 0)
        return status;
    if (arg[0] != 4)
        return 0xe;
    if (*(int *)(arg + 1) < 1 || *(int *)(arg + 1) > gTrackCount)
        return 10;
    base = (char *)GetResDataPtr((int *)gTrackTable) - 0x28;
    rec = *(Slot2Rec40 *)(base + *(int *)(arg + 1) * 0x28);
    UnlockResObject((int *)gTrackTable);
    return AllocScriptString((byte *)&rec + 0x16, pResult);
}


/* currentsound(n) (opcode 20077): the name of the sound playing on channel 1 or 2, or for n = 3
 * the one GetPlayingSoundNameByPriority picks, as a string. Error 0x0e for any other n. */
short ScriptCmd_CurrentSound(int *pCtx, short *pArgBlock, int *pTable,
                                                  short *pResult, int *pSkipCount) /* 0x0044d690 */
{
    short  st;
    short  typed[4];
    int    auxA;
    int    auxB;

    st = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSkipCount);
    if (st != 0)
        return st;
    if (typed[0] != 4)
        return 0xE;

    /* the two outputs are never read here */
    switch (*(int *)(typed + 1)) {
    case 1:
        GetPlayingSoundName1(gScratchStr, &auxA, &auxB);
        break;
    case 2:
        GetPlayingSoundName2(gScratchStr, &auxA, &auxB);
        break;
    case 3:
        GetPlayingSoundNameByPriority(gScratchStr, &auxA, &auxB);
        break;
    default:
        return 0xE;
    }
    return AllocScriptString(gScratchStr, pResult);
}


/* currenttheme(n) (opcode 20079): for n = 1 the name GetPlayingThemeName returns; for n = 2 the
 * name of the track it reports (CopyTrackNameByNode), or kStrNone when it reports none. Error 0x0e otherwise. */
short ScriptCmd_CurrentTheme(int *pCtx, short *pArgBlock, int *pTable,
                                               short *pResult, int *pSkipCount) /* 0x0044d750 */
{
    short  st;
    short  typed[4];
    int    rowCtx;
    int    aux;

    st = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSkipCount);
    if (st != 0)
        return st;
    if (typed[0] != 4)
        return 0xE;

    /* GetPlayingThemeName always writes both outputs */
    switch (*(int *)(typed + 1)) {
    case 1:
        GetPlayingThemeName(gScratchStr, &rowCtx, &aux);
        break;
    case 2:
        GetPlayingThemeName(gScratchStr, &rowCtx, &aux);
        if (rowCtx == 0)
            CopyPascalString((byte *)UpdatePascalLength((char *)&kStrNone), gScratchStr);   /* the lookup written inline: the other arguments are pushed before it is called */
        else
            CopyTrackNameByNode(rowCtx, gScratchStr);
        break;
    default:
        return 0xE;
    }
    return AllocScriptString(gScratchStr, pResult);
}


/* currentvoice (opcode 20078), no arguments: the name of the sound on the voice channel. */
short ScriptCmd_CurrentVoice(int *pCtx, short *pArgBlock, int *pTable,
                                                     short *pResult, int *pSkipCount) /* 0x0044d820 */
{
    int auxLo;                          /* out-parameters: the exe does not zero them */
    int auxHi;

    (void)pCtx;
    (void)pArgBlock;
    (void)pTable;

    *pSkipCount = 0;
    GetPlayingVoiceName(gScratchStr, &auxLo, &auxHi);
    return AllocScriptString(gScratchStr, pResult);
}


/* Evaluates one script argument as a sound node name and stores the node's address in
 * *pNodeOut (FindNodeByDescriptor). Shared by the sound handlers above.
 *
 * Port note: Ghidra shows it as __stdcall(void) reading its caller's stack; it is __cdecl with
 * five parameters, and the callers push all five (port-bug-classes.md #2). */
short __cdecl ResolveNodeFromScript(int pCtx, short *pArgBlock, int *pTable,
                                    int *pSkipCount, int *pNodeOut)  /* 0x0044d860 */
{
    short status;                       /* the exe tests ax and returns it untouched */
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    return FindNodeByDescriptor((byte *)&gScratchStr, (uint **)pNodeOut);
}


/* Finds the sound node named pName (node +8, case-insensitive) in any open track and stores its
 * address in *ppEntry; 10 ("No such object") if there is none. An unloaded on-demand node (kind 1
 * at +0x2c) is loaded first, keeping its volume, pan, loop flag and key; it becomes kind 0 and
 * the track's loaded-voice count (+0x26) goes up. */
short __cdecl FindNodeByDescriptor(byte *pName, uint **ppEntry)  /* 0x0044d8c0 */
{
    uint  *pEntry;   /* sound node, 0x68 bytes */
    uint  *pGroup;   /* track row, 0x28 bytes  */
    short  g, e;
    uint   ex;
    short  iv2, fv;

    pGroup = (uint *)GetResDataPtr(gTrackTable);
    for (g = 0; g < gTrackCount; g++, pGroup += 10) {
        pEntry = *(uint **)((char *)pGroup + 10);
        for (e = 0; e < *(short *)(pGroup + 1); e++, pEntry += 0x1a) {
            if (PascalStringsEqualNoCase((byte *)(pEntry + 2), pName) != 0) {
                if (*(short *)(pEntry + 0xb) == 1) {
                    /* the dead inner counter holds the saved int value */
                    e = GetSoundNodeVolume((int)pEntry);
                    iv2 = GetSoundNodePan((int)pEntry);
                    fv = (short)GetSoundNodeLoop((int)pEntry);
                    ex = GetSoundNodeKey(pEntry);
                    InitSoundNodeEntryFromResource((uint *)*pGroup, pEntry[9], (byte *)(pEntry + 2), pEntry);
                    SetSoundNodeKey(pEntry, ex);
                    SetSoundNodeLoop((int)pEntry, fv);
                    SetSoundNodeVolume((byte *)pEntry, e);
                    SetSoundNodePan((byte *)pEntry, iv2);
                    *(short *)(pEntry + 0xb) = 0;
                    (*(short *)((char *)pGroup + 0x26))++;
                }
                *ppEntry = pEntry;
                UnlockResObject(gTrackTable);
                return 0;
            }
        }
    }
    UnlockResObject(gTrackTable);
    return 10;
}


/* Copies the 40-byte gTrackTable row whose name (+0x16) matches pName, case-insensitive, to
 * pOut; 10 if no track has that name. Callers' buffers must hold all 40 bytes. */
short __cdecl FindTrackByName(byte *pName, int *pOut)  /* 0x0044d9f0 */
{
    int *pRow;
    int  i;
    typedef struct { int d[10]; } Slot2Rec40;

    pRow = (int *)GetResDataPtr((int *)gTrackTable);
    for (i = 0; i < gTrackCount; pRow += 10, i++) {
        if (PascalStringsEqualNoCase(pName, (byte *)((char *)pRow + 0x16)) != 0) {
            /* one struct copy, as the exe's rep movsd (ecx = 10) */
            *(Slot2Rec40 *)pOut = *(Slot2Rec40 *)pRow;
            UnlockResObject((int *)gTrackTable);
            return 0;
        }
    }

    UnlockResObject((int *)gTrackTable);
    return 10;
}


/* Stores in *pIdx the index of the gTrackTable row whose render node handle (+0) is entryKey;
 * 10 if none. */
short __cdecl FindTrackIndexByNode(int entryKey, int *pIdx)  /* 0x0044da70 */
{
  int *pTrack;
  short extraout_var;
  uint32_t uVar2;
  int idx;
  
  pTrack = (int *)GetResDataPtr(gTrackTable);
  idx = 0;
  if (0 < gTrackCount) {
    do {
      if (*pTrack == entryKey) {
        *pIdx = idx;
        UnlockResObject(gTrackTable);
        return 0;
      }
      pTrack = pTrack + 10;
      idx = idx + 1;
    } while (gTrackCount > idx);   /* the exe compares the global against the register */
  }
  UnlockResObject(gTrackTable);
  return 10;
}


/* Copies the name (+0x16) of the gTrackTable row whose render node handle is nodeKey to pOut;
 * 10 if none. */
short __cdecl CopyTrackNameByNode(int nodeKey, byte *pOut)  /* 0x0044dad0 */
{
  int *pTrack;
  undefined2 extraout_var;
  uint32_t uVar2;
  int idx;

  pTrack = (int *)GetResDataPtr(gTrackTable);
  idx = 0;
  if (0 < gTrackCount) {
    do {
      if (*pTrack == nodeKey) {
        CopyPascalString((byte *)((int)pTrack + 0x16), pOut);
        UnlockResObject(gTrackTable);
        return 0;
      }
      pTrack = pTrack + 10;
      idx = idx + 1;
    } while (idx < gTrackCount);
  }
  UnlockResObject(gTrackTable);
  return 10;
}


/* Writes each track's index in gTrackTable into the high 16 bits of the key of every sound node
 * it owns. Run after a track is opened and after a saved game is restored. */
void __stdcall AssignSoundNodeTrackIndices(void)  /* 0x0044db40 */
{
  short trackIdx;  /* local_2 */
  int pTrack;
  short rowIdx;
  unsigned int key;
  unsigned int *pRow;

  pTrack = GetResDataPtr(gTrackTable);
  trackIdx = 0;
  if (0 < gTrackCount) {
    do {
      rowIdx = 0;
      pRow = *(unsigned int **)(pTrack + 10);   /* loaded before the count test */
      if (0 < *(short *)(pTrack + 4)) {
        do {
          rowIdx = rowIdx + 1;
          key = GetSoundNodeKey(pRow);
          SetSoundNodeKey(pRow, (key & 0xffff) | ((unsigned int)trackIdx << 16));
          pRow = pRow + 0x1a;
        } while (rowIdx < *(short *)(pTrack + 4));
      }
      trackIdx = trackIdx + 1;
      pTrack = pTrack + 0x28;
    } while (trackIdx < gTrackCount);
  }
  UnlockResObject(gTrackTable);
}


/* Unloads every loaded on-demand sound node (kind 0) that has finished playing: tears it down,
 * marks it kind 1 again and decrements its track's count at +0x26. Called from TickScriptBindings. */
void __stdcall TeardownFinishedVoiceNodes(void)  /* 0x0044dbd0 */
{
    short j;
    int pVoice;
    short i;
    int pRow;
    short left;

    i = 0;
    pRow = GetResDataPtr((int *)gTrackTable);
    for (; i < gTrackCount; i++, pRow += 0x28) {
        if (*(short *)(pRow + 0x26) > 0) {
            j = 0;
            pVoice = *(int *)(pRow + 10);
            for (; j < *(short *)(pRow + 4); j++, pVoice += 0x68) {
                if (*(short *)(pVoice + 0x2c) == 0 && IsVoicePlaybackStillActive(pVoice) == 0) {
                    TeardownVoicePlaybackRow(pVoice);
                    *(short *)(pVoice + 0x2c) = 1;
                    *(short *)(pRow + 0x26) = left = *(short *)(pRow + 0x26) - 1;
                    if (left <= 0)
                        break;
                }
            }
        }
    }
    UnlockResObject((int *)gTrackTable);
    /* void: the exe ends in a bare RET after the call */
}
