/* movie_player.c -- TI.EXE 0x004486a0..0x0044c340: the movie player (PlayMovie, frame actions,
 * hotspots, voices, timed jumps) and the frame transitions (wipes, scrolls, pushes, fades). */

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


static short WaitForMovieClick(uint32_t *pClickPos);
static short HandleMovieKeyEvent(short *pEvent);
static void UpdateMovieHoverCursor(void);
static short PollMovieTimedJump(void);
static void __stdcall PreloadNextMovieFrame(void);
static void PlayMovieVoice(byte *pVoiceName);
static short __cdecl RunMovieFrameWipe(int *pRowField);
static void ReblitSceneWindowForCLUTFade(void);
static void __cdecl ShiftMovieScreenRect(uint *pFrom, uint *pTo);


#include "topics/soundslot.h"

/* Plays the movie container ('MOVE') named pMovieName in pChannel: the playmovie script command
 * (ActionCmd_PlayMovie). A movie is a chain of blocks, header +0x2c giving the next block's child
 * base. Each block has a header (first dword 0x40000, else the movie is closed and 0x38 returned),
 * sound templates, the voice table, the frame table and the timed jumps; PlayMovie builds the
 * template and voice rows, the offscreen buffer and the movie's CLUT, then shows frames until one
 * returns 3. A frame action that switches movie (gMovieNavState 1) restarts it all on gMovieName.
 * Header flag byte +0x18: 4 detach (not only release) each frame's children, 8 leave the event
 * queue unflushed at the end, 0x10 hide the cursor. Returns 0 or the path error; fatal codes
 * 0x494..0x4aa. */
short __cdecl PlayMovie(byte *pMovieName, void *pChannel)  /* 0x004486a0 */
{
    typedef struct { unsigned char b[26]; } Rec26;      /* template row: +4 id, +10 name */
#pragma pack(push, 2)
    typedef struct { unsigned char b[16]; int slot; unsigned char rest[22]; } Rec42; /* +0 flags, +4 id, +10 name, +0x10 slot */
#pragma pack(pop)
    typedef struct { unsigned int v[26]; } Rec104;      /* voice playback row */
    typedef struct { unsigned int v[30]; } Rec120;      /* hotspot buffer head */
    typedef struct { unsigned int v[2]; } Rec8;         /* CLUT entry */
    short   err;
    short   st;
    short   rowTrack0;
    short   rowTrack1;
    int     i;
    char   *pRow;
    int     slotId;
    int     oldBase;
    char   *pLink;
    char   *pClut;
    int     n;
    short   overlayW;
    short   hsbufCreated;
    int    *hHeader;
    int    *hTemplates;
    int    *hNodes;
    int    *hTrack;
    int    *hSlot;
    int     rect[2];        /* {y1,x1,y2,x2} as shorts, passed as one rect */
    int     overlayMetric;
    Rec42   nodeRec;
    Rec42   rowRec;         /* slot id at +0x10 */
    Rec26   tmplRec;
    byte    name[256];

    DF_TRACE("probe3 PlayMovie entry: desc='%.20s' "
             "pChannel=%p chanB+0x12(before)=%p",
             (const char *)pMovieName + 1, pChannel,
             *(void **)((char *)&gChannelB + 0x12));
    gMovieEntryFrame = 0;
    gMovieSoundArmed = 1;
    gMovieNavDepth = 0;
    gMovieChannel = (int)pChannel;
    /* gMovieName is an inline Pascal buffer: the exe pushes its address */
    CopyPascalString(pMovieName, gMovieName);
    gMovieNavHistory = AllocEngineBuffer(100);

    do
    {
        gMovieFrameIndex = gMovieEntryFrame;
        gMovieTemplateCount = -1;
        gMovieChildBase = 0;
        gMovieNavState = 0;
        CopyPascalString(gMovieName, name);
        err = ValidateDescriptorPathOfType(name, 0x4d4f5645);   /* 'MOVE' */
        if (err != 0)
        {
            FreeEngineBuffer((int)gMovieNavHistory);
            return err;
        }
        DF_TRACE("ResetChannel: opening container '%.*s'",
                 (int)name[0], (char *)&name[1]);
        err = OpenContainerNode(name, (int **)&gMovieNode);
        if (err != 0)
            FatalError(err, 0x494);

        for (;;)
        {
            gMovieLastFrame = -1;
            gMovieSoundPhase = 0;
            err = AttachRenderChild((int *)gMovieNode, gMovieChildBase, (int *)&hHeader);
            if (err != 0)
                FatalError(err, 0x495);
            MoveResObjectToHeapBottom(hHeader);
            gMovieHeader = (int *)GetResDataPtr(hHeader);
            if (*gMovieHeader != 0x40000)
            {
                UnlockResObject(hHeader);
                err = DetachRenderChild(gMovieNode, gMovieChildBase);
                if (err != 0)
                    FatalError(err, 0x496);
                DrainDisplayAttachSlotsForContext(gMovieNode);
                err = DestroyRenderNode((void *)gMovieNode);
                if (err != 0)
                    FatalError(err, 0x497);
                FreeEngineBuffer((int)gMovieNavHistory);
                return 0x38;
            }
            err = AttachRenderChild((int *)gMovieNode, gMovieHeader[0x19] + gMovieChildBase,
                                    (int *)&hTemplates);
            if (err != 0)
                FatalError(err, 0x498);
            MoveResObjectToHeapBottom(hTemplates);
            gMovieTemplateData = (int *)GetResDataPtr(hTemplates);
            err = AttachRenderChild((int *)gMovieNode, gMovieHeader[0x18] + gMovieChildBase,
                                    (int *)&hNodes);
            if (err != 0)
                FatalError(err, 0x499);
            MoveResObjectToHeapBottom(hNodes);
            gMovieVoiceTable = GetResDataPtr(hNodes);
            err = AttachRenderChild((int *)gMovieNode, gMovieHeader[0x1a] + gMovieChildBase,
                                    (int *)&hTrack);
            if (err != 0)
                FatalError(err, 0x49a);
            MoveResObjectToHeapBottom(hTrack);
            gMovieTimedJumps = GetResDataPtr(hTrack);

            if (gMovieChildBase == 0 || *(int *)((char *)gMovieTemplateData + 0x10a) != 0)
            {
                /* the first block, or one with its own templates (count at +0x10a), rebuilds the
                 * template rows and the theme list linked from them */
                if (gMovieTemplateCount >= 0)
                {
                    StopSoundChannels(0, 0, 1, 0);
                    pRow = (char *)gMovieTemplateRows;
                    for (i = 0; i < gMovieTemplateCount; i++)
                    {
                        TeardownVoicePlaybackRow((int)pRow);
                        pRow += 0x68;
                    }
                    FreeEngineBuffer(gMovieTemplateRows);
                    FreeEngineBuffer(gMovieChannelRows);
                }
                gMovieTemplateCount = *(int *)((char *)gMovieTemplateData + 0x10a);
                gMovieTemplateRows = (int)AllocEngineBuffer(gMovieTemplateCount * 0x68);
                pRow = (char *)gMovieTemplateRows;
                for (i = 0; i < gMovieTemplateCount; i++)
                {
                    tmplRec = *(Rec26 *)((char *)gMovieTemplateData + i * 0x1a + 0x10e);
                    InitSoundNodeEntryFromResource((unsigned int *)gMovieNode,
                                    gMovieChildBase + *(int *)(tmplRec.b + 4),
                                    tmplRec.b + 10, (unsigned int *)pRow);
                    pRow += 0x68;
                }
                gMovieChannelRows = (int)AllocEngineBuffer(*(short *)((char *)gMovieTemplateData + 4) * 0x68);
                pRow = (char *)gMovieChannelRows;
                for (i = 0; i < *(short *)((char *)gMovieTemplateData + 4); i++)
                {
                    err = *(short *)((char *)gMovieTemplateData + i * 2 + 6);
                    if (err > 0 && err <= gMovieTemplateCount)
                    {
                        pLink = pRow + 0x68;
                        if (*(short *)((char *)gMovieTemplateData + 4) - i == 1)
                        {
                            /* the last entry links back to entry *gMovieTemplateData, the loop point */
                            n = *gMovieTemplateData;
                            if (n >= gMovieTemplateCount)
                                n = gMovieTemplateCount - 1;
                            if (n < 0)
                                n = 0;
                            pLink = (char *)gMovieChannelRows + n * 0x68;
                        }
                        *(Rec104 *)pRow = *(Rec104 *)((char *)gMovieTemplateRows + (err - 1) * 0x68);
                        *(char **)(pRow + 0x18) = pLink;
                        DF_TRACE("tmpl[%d/%d] src=%d slot=%p next=%p%s", i,
                                 (int)*(short *)((char *)gMovieTemplateData + 4), (int)err,
                                 (void *)pRow, (void *)pLink,
                                 (pLink != pRow + 0x68) ? " (wrap)" : "");
                        pRow += 0x68;
                        gMovieSoundPhase = 1;
                    }
                    else
                    {
                        DF_TRACE("tmpl[%d/%d] src=%d SKIPPED", i,
                                 (int)*(short *)((char *)gMovieTemplateData + 4), (int)err);
                    }
                }
            }

            /* preload the voices whose flag bit 0 (load on demand, see PlayMovieVoice) is clear */
            gMovieVoiceRows = (int)AllocEngineBuffer(*(int *)(gMovieVoiceTable + 4) * 0x68);
            pRow = (char *)gMovieVoiceRows;
            gMovieVoiceRowCount = 0;
            for (i = 0; i < *(int *)(gMovieVoiceTable + 4); i++)
            {
                nodeRec = *(Rec42 *)(gMovieVoiceTable + i * 0x2a + 8);
                if ((nodeRec.b[0] & 1) == 0)
                {
                    InitSoundNodeEntryFromResource((unsigned int *)gMovieNode,
                                    gMovieChildBase + *(int *)(nodeRec.b + 4),
                                    nodeRec.b + 10, (unsigned int *)pRow);
                    if ((nodeRec.b[0] & 2) != 0)
                        SetSoundNodeLoop((int)pRow, 1);
                    pRow += 0x68;
                    gMovieVoiceRowCount++;
                }
            }

            hsbufCreated = 0;
            *(Rec120 *)gMovieHotspotBuf = *(Rec120 *)&gHotspotBuf;
            *(Rec8 *)rect = *(Rec8 *)(gMovieHeader + 0x21b);
            ComputeAlignedRectSize((short *)rect, &overlayMetric, (unsigned short *)&overlayW);
            DF_TRACE("hsbuf: rect=[%d..%d] wantW=%u wantH=%d | gHotspotBuf h=%d w=%d px=%p",
                     (int)((short *)rect)[0], (int)((short *)rect)[2], (unsigned)overlayW,
                     (int)(((short *)rect)[2] - ((short *)rect)[0]),
                     (int)gHotspotBuf.h, (int)gHotspotBuf.w, gHotspotBuf.pPx);
            if (overlayW != gHotspotBuf.w ||
                ((short *)rect)[2] - ((short *)rect)[0] > gHotspotBuf.h)
            {
                InitHotspotBuffer((short *)gMovieHotspotBuf, (unsigned short)overlayW,
                                  (short)(((short *)rect)[2] - ((short *)rect)[0]));
                hsbufCreated = 1;
                DF_TRACE("hsbuf: InitHotspotBuffer -> h=%d w=%d px=%p",
                         (int)((HotspotBuf *)gMovieHotspotBuf)->h,
                         (int)((HotspotBuf *)gMovieHotspotBuf)->w,
                         ((HotspotBuf *)gMovieHotspotBuf)->pPx);
            }
            CopyHotspotBufState((unsigned int *)gMovieHotspotBuf, (unsigned int *)gMovieSavedHotspotBuf);
            gMovieClut = AllocResObject(0);
            /* InitCLUT takes the ADDRESS of the handle slot and may replace it */
            InitCLUT((int *)&gMovieClut);
            pClut = (char *)GetResDataPtr((int *)gMovieClut);
            for (i = 0; i < 0x100; i++)
                ((Rec8 *)(pClut + 8))[i] = ((Rec8 *)((char *)gMovieHeader + 0x6c))[i];
            UnlockResObject((int *)gMovieClut);
            rowTrack0 = (short)FindMovieFrameIndexByName((byte *)(gMovieHeader + 0x10));
            rowTrack1 = (short)FindMovieFrameIndexByName((byte *)(gMovieHeader + 0x14));
            gMovieClutDirty = 1;
            gMovieVoiceActive = 0;
            ScheduleMovieTimedJumps();
            FlushEventQueue();
            if ((*(byte *)(gMovieHeader + 6) & 0x10) != 0)
                QdHideCursor();
            else
                QdInitCursor();
            gMovieStartTick = GetTick60Hz();
            gMovieRenderPhase = 0;

            /* show frames until one returns 3 (stop) */
            do
            {
                rowRec = *(Rec42 *)((char *)gMovieHeader + gMovieFrameIndex * 0x2a + 0x87c);
                err = ResolveDisplayAttachSlot((void *)gMovieNode, gMovieChildBase + rowRec.slot, &hSlot);
                slotId = rowRec.slot;   /* after the call: the exe reads the slot from the copy */
                if (err != 0)
                    FatalError(err, 0x49c);
                gMovieFrame = (byte *)GetResDataPtr(hSlot);
                st = (short)ShowMovieFrame();
                if (st == 1)
                    FatalError(0, 0x49d);
                if (st != 3 && st != 2)
                {
                    PretouchMovieFrameRange();
                    NoOpDisplayAttachBeforeUserInteraction(gMovieFrameIndex);
                    if (rowTrack0 == gMovieFrameIndex)
                        OrInputFlagsForDisplayAttachTrackRow(1);
                    if (rowTrack1 == gMovieFrameIndex)
                        OrInputFlagsForDisplayAttachTrackRow(2);
                    st = (short)RunMovieFrameActions();
                    if (st == 1)
                    {
                        st = WaitForMovieFrameSound();
                        if (st != 3 && st != 2)
                        {
                            UnlockResObject(hSlot);
                            if ((*(byte *)(gMovieHeader + 6) & 4) != 0)
                            {
                                err = DetachRenderChild(gMovieNode, gMovieChildBase + slotId);
                                if (err != 0)
                                    FatalError(err, 0x49e);
                            }
                            else
                            {
                                err = ReleaseRenderChild(gMovieNode, gMovieChildBase + slotId);
                                if (err != 0)
                                    FatalError(err, 0x49f);
                            }
                            goto row_done;
                        }
                        gMovieNavState = 0;
                    }
                    else if (st != 3 && st != 2)
                    {
                        do
                        {
                            st = PumpMovieInputOnce();
                            if (st == 3 || st == 2)
                                goto release_slot;
                        } while ((int)GetTick60Hz() < gMovieFrameDueTick);
                        DF_TRACE("rowWait: row=%d held until tick %d",
                                 gMovieFrameIndex, gMovieFrameDueTick);
                        st = WaitForMovieFrameSound();
                    }
                }
            release_slot:
                UnlockResObject(hSlot);
                if ((*(byte *)(gMovieHeader + 6) & 4) != 0)
                {
                    err = DetachRenderChild(gMovieNode, gMovieChildBase + slotId);
                    if (err != 0)
                        FatalError(err, 0x4a0);
                }
                else
                {
                    err = ReleaseRenderChild(gMovieNode, gMovieChildBase + slotId);
                    if (err != 0)
                        FatalError(err, 0x4a1);
                }
            } while (st != 3);
            gMovieNavState = 2;

        row_done:
            if (hsbufCreated != 0)
                DestroyHotspotBuffer((int)gMovieHotspotBuf);
            FreeResObject((int *)gMovieClut);
            TeardownSceneRefs(gMovieSavedHotspotBuf);
            StopSoundChannels(0, 0, 0, 1);
            if ((*(byte *)(gMovieHeader + 6) & 8) == 0)
                FlushEventQueue();
            if ((*(byte *)(gMovieHeader + 6) & 0x10) != 0)
                QdShowCursor();
            if (gMovieVoiceActive != 0)
            {
                TeardownVoicePlaybackRow((int)&gMovieVoice);
                gMovieVoiceActive = 0;
            }
            pRow = (char *)gMovieVoiceRows;
            for (i = 0; i < gMovieVoiceRowCount; i++)
            {
                TeardownVoicePlaybackRow((int)pRow);
                pRow += 0x68;
            }
            FreeEngineBuffer(gMovieVoiceRows);

            /* chain to the next block unless the movie was stopped or a frame switched movie */
            oldBase = gMovieChildBase;
            gMovieChildBase = gMovieHeader[0xb];
            if (gMovieNavState != 0)
                gMovieChildBase = 0;
            if (gMovieChildBase == 0)
                break;
            UnlockResObject(hTrack);
            err = DetachRenderChild(gMovieNode, gMovieHeader[0x1a] + oldBase);
            if (err != 0)
                FatalError(err, 0x4a2);
            UnlockResObject(hNodes);
            err = DetachRenderChild(gMovieNode, gMovieHeader[0x18] + oldBase);
            if (err != 0)
                FatalError(err, 0x4a3);
            UnlockResObject(hTemplates);
            err = DetachRenderChild(gMovieNode, gMovieHeader[0x19] + oldBase);
            if (err != 0)
                FatalError(err, 0x4a4);
            UnlockResObject(hHeader);
            err = DetachRenderChild(gMovieNode, oldBase);
            if (err != 0)
                FatalError(err, 0x4a5);
            gMovieFrameIndex = 0;
        }

        if (gMovieTemplateCount != 0)
        {
            StopSoundChannels(0, 0, 1, 0);
            pRow = (char *)gMovieTemplateRows;
            for (i = 0; i < gMovieTemplateCount; i++)
            {
                TeardownVoicePlaybackRow((int)pRow);
                pRow += 0x68;
            }
        }
        FreeEngineBuffer(gMovieTemplateRows);
        FreeEngineBuffer(gMovieChannelRows);
        UnlockResObject(hTrack);
        err = DetachRenderChild(gMovieNode, gMovieHeader[0x1a] + oldBase);
        if (err != 0)
            FatalError(err, 0x4a6);
        UnlockResObject(hNodes);
        err = DetachRenderChild(gMovieNode, gMovieHeader[0x18] + oldBase);
        if (err != 0)
            FatalError(err, 0x4a7);
        UnlockResObject(hTemplates);
        err = DetachRenderChild(gMovieNode, gMovieHeader[0x19] + oldBase);
        if (err != 0)
            FatalError(err, 0x4a8);
        UnlockResObject(hHeader);
        err = DetachRenderChild(gMovieNode, oldBase);
        if (err != 0)
            FatalError(err, 0x4a9);
        DrainDisplayAttachSlotsForContext(gMovieNode);
        err = DestroyRenderNode((void *)gMovieNode);
        if (err != 0)
            FatalError(err, 0x4aa);
    } while (gMovieNavState == 1);

    FreeEngineBuffer((int)gMovieNavHistory);
    return 0;
}


/* Runs the current frame's action once it is shown. With no hotspots (count at frame +0x442; frame
 * flag 4 also means none while no click is pending) the frame's own action (frame +0) runs, else it
 * waits for a click inside one of the 64-byte hotspot records at +0x446 and runs that hotspot's
 * action after starting its voice. Actions: 1 end the block, 2 go to a frame by name, 3 go to a
 * movie, 4 call a movie (push this movie and the named frame, at most 5 deep), 5 return (pop),
 * 6 next frame, 7 previous frame. Returns 0 when it set gMovieFrameIndex, 1 to end the block (with
 * gMovieNavState 1 to switch movie), 2 or 3 from the input pump. The kStr* messages are Pascal+C
 * templates (.rdata 0x0045e1c4..0x0045e240) whose length byte is 0 until UpdatePascalLength. */
short RunMovieFrameActions(void) /* 0x00449310 */
{
    /* One entry of the 5-deep navigation history at gMovieNavHistory (stride 0x14),
     * pushed and popped whole */
    typedef struct { int rowIndex; byte name[16]; } NavEntry;
    NavEntry  navEntry;
    uint32_t  pt;           /* packed cursor point from WaitForMovieClick */
    /* 64-byte hotspot record: +0 action, +2 flags (1 track the press, 2 hover cursor),
     * +8 rect {y1,x1,y2,x2}, +0x10 voice name, +0x20 movie name, +0x30 frame name */
    byte      rec[64];
    byte      lmsg[256];
    short     st;
    int       nHot;
    int       i;

    DF_TRACE("probe4 RunMovieFrameActions entry: chanB+0x12=%p",
             *(void **)((char *)&gChannelB + 0x12));
    for (;;)
    {
        DF_TRACE("probe5 loop top: chanB+0x12=%p",
                 *(void **)((char *)&gChannelB + 0x12));
        st = PumpMovieInputOnce();
        if (st == 3)
            return 3;
        if (st == 2)
            return 2;

        nHot = *(int *)(gMovieFrame + 0x442);
        if ((gMovieFrame[6] & 4) != 0 && st == 0)
            nHot = 0;

        DF_TRACE("rowUI: row=%d nav=%d nHot=%d flags6=%02x sel=%d",
                 gMovieFrameIndex, (int)st, nHot, (unsigned)gMovieFrame[6],
                 (int)*(short *)gMovieFrame);
        if (nHot <= 0)
        {
            /* no hotspots: the frame's own action */
            switch (*(short *)gMovieFrame)
            {
            case 1:
                return 1;
            case 2:
                i = FindMovieFrameIndexByName(gMovieFrame + 0x32);
                DF_TRACE("rowUI: sel=2 goto row '%.*s' -> %d",
                         (int)gMovieFrame[0x32], (const char *)gMovieFrame + 0x33, i);
                if (i < 0)
                {
                    CopyPascalString((byte *)UpdatePascalLength(kStrJumpToFrameError), lmsg);
                    AppendPascalString(gMovieFrame + 0x32, lmsg);
                    AppendPascalString((byte *)UpdatePascalLength(kStrAposDot), lmsg);
                    ShowEngineMessageBoxFromPascalString(lmsg);
                    return 3;
                }
                gMovieFrameIndex = i;
                return 0;
            case 3:
                gMovieEntryFrame = 0;
                gMovieNavState = 1;
                CopyPascalString(gMovieFrame + 0x22, gMovieName);
                return 1;
            case 4:
                i = FindMovieFrameIndexByName(gMovieFrame + 0x32);
                if (i < 0)
                {
                    CopyPascalString((byte *)UpdatePascalLength(kStrJumpToFrameError), lmsg);
                    AppendPascalString(gMovieFrame + 0x32, lmsg);
                    AppendPascalString((byte *)UpdatePascalLength(kStrAposDot), lmsg);
                    ShowEngineMessageBoxFromPascalString(lmsg);
                    return 3;
                }
                navEntry.rowIndex = i;
                CopyPascalString(gMovieName, navEntry.name);
                if (gMovieNavDepth >= 5)
                {
                    ShowEngineMessageBoxFromPascalString((byte *)UpdatePascalLength(kStrNoPushFrame));
                    return 3;
                }
                ((NavEntry *)gMovieNavHistory)[gMovieNavDepth] = navEntry;
                gMovieNavState = 1;
                gMovieNavDepth++;
                gMovieEntryFrame = 0;
                CopyPascalString(gMovieFrame + 0x22, gMovieName);
                return 1;
            case 5:
                if (gMovieNavDepth <= 0)
                {
                    ShowEngineMessageBoxFromPascalString((byte *)UpdatePascalLength(kStrNoPopFrame));
                    return 3;
                }
                gMovieNavDepth--;
                navEntry = ((NavEntry *)gMovieNavHistory)[gMovieNavDepth];
                gMovieNavState = 1;
                gMovieEntryFrame = navEntry.rowIndex;
                CopyPascalString(navEntry.name, gMovieName);
                return 1;
            case 6:
                if (*(int *)((char *)gMovieHeader + 0x878) - 1 <= gMovieFrameIndex)
                {
                    ShowEngineMessageBoxFromPascalString((byte *)UpdatePascalLength(kStrNoNextFrame));
                    return 3;
                }
                gMovieFrameIndex++;
                return 0;
            case 7:
                if (gMovieFrameIndex <= 0)
                {
                    ShowEngineMessageBoxFromPascalString((byte *)UpdatePascalLength(kStrNoPrevFrame));
                    return 3;
                }
                gMovieFrameIndex--;
                return 0;
            default:
                FatalError(0x66, 0x4ab);
            }
        }

        do
        {
            st = WaitForMovieClick(&pt);
            if (st == 3)
                return 3;
            if (st == 2)
                return 2;
            for (i = 0; i < *(int *)(gMovieFrame + 0x442); i++)
            {
                memcpy(rec, gMovieFrame + 0x446 + i * 0x40, 64);
                DF_TRACE("click test: pt=(y=%d,x=%d) rect{%d,%d,%d,%d} "
                         "tag=%d flags=%02x -> %d",
                         (int)(short)(pt & 0xffff), (int)(short)(pt >> 16),
                         (int)((short *)(rec + 8))[0], (int)((short *)(rec + 8))[1],
                         (int)((short *)(rec + 8))[2], (int)((short *)(rec + 8))[3],
                         (int)*(short *)rec, rec[2],
                         (int)PointInSceneRect(*(ScenePointYX *)&pt, (short *)(rec + 8)));
                if (PointInSceneRect(*(ScenePointYX *)&pt, (short *)(rec + 8)) != 0)
                {
                    switch (*(short *)rec)
                    {
                    case 1:
                        if ((rec[2] & 1) == 0
                            || TrackHotspotRectPress((short *)(rec + 8)) != 0)
                        {
                            PlayMovieVoice(rec + 16);
                            NoOpAfterDisplayAttachVoice(i);
                            return 1;
                        }
                        break;
                    case 2:
                        if ((rec[2] & 1) == 0
                            || TrackHotspotRectPress((short *)(rec + 8)) != 0)
                        {
                            PlayMovieVoice(rec + 16);
                            NoOpAfterDisplayAttachVoice(i);
                            i = FindMovieFrameIndexByName(rec + 48);
                            DF_TRACE("rowUI: hit tag=2 goto row '%.*s' -> %d",
                                     (int)rec[48], (const char *)rec + 49, i);
                            if (i < 0)
                            {
                                CopyPascalString((byte *)UpdatePascalLength(kStrJumpToFrameError), lmsg);
                                AppendPascalString(rec + 48, lmsg);
                                AppendPascalString((byte *)UpdatePascalLength(kStrAposDot), lmsg);
                                ShowEngineMessageBoxFromPascalString(lmsg);
                                return 3;
                            }
                            gMovieFrameIndex = i;
                            return 0;
                        }
                        break;
                    case 3:
                        if ((rec[2] & 1) == 0
                            || TrackHotspotRectPress((short *)(rec + 8)) != 0)
                        {
                            PlayMovieVoice(rec + 16);
                            NoOpAfterDisplayAttachVoice(i);
                            gMovieEntryFrame = 0;
                            gMovieNavState = 1;
                            CopyPascalString(rec + 32, gMovieName);
                            return 1;
                        }
                        break;
                    case 4:
                        if ((rec[2] & 1) == 0
                            || TrackHotspotRectPress((short *)(rec + 8)) != 0)
                        {
                            PlayMovieVoice(rec + 16);
                            NoOpAfterDisplayAttachVoice(i);
                            i = FindMovieFrameIndexByName(rec + 48);
                            if (i < 0)
                            {
                                CopyPascalString((byte *)UpdatePascalLength(kStrJumpToFrameError), lmsg);
                                AppendPascalString(rec + 48, lmsg);
                                AppendPascalString((byte *)UpdatePascalLength(kStrAposDot), lmsg);
                                ShowEngineMessageBoxFromPascalString(lmsg);
                                return 3;
                            }
                            navEntry.rowIndex = i;
                            CopyPascalString(gMovieName, navEntry.name);
                            if (gMovieNavDepth >= 5)
                            {
                                ShowEngineMessageBoxFromPascalString((byte *)UpdatePascalLength(kStrNoPushFrame));
                                return 3;
                            }
                            ((NavEntry *)gMovieNavHistory)[gMovieNavDepth] = navEntry;
                            gMovieNavState = 1;
                            gMovieNavDepth++;
                            gMovieEntryFrame = 0;
                            CopyPascalString(rec + 32, gMovieName);
                            return 1;
                        }
                        break;
                    case 5:
                        if ((rec[2] & 1) == 0
                            || TrackHotspotRectPress((short *)(rec + 8)) != 0)
                        {
                            PlayMovieVoice(rec + 16);
                            NoOpAfterDisplayAttachVoice(i);
                            if (gMovieNavDepth <= 0)
                            {
                                ShowEngineMessageBoxFromPascalString((byte *)UpdatePascalLength(kStrNoPopFrame));
                                return 3;
                            }
                            gMovieNavDepth--;
                            navEntry = ((NavEntry *)gMovieNavHistory)[gMovieNavDepth];
                            gMovieNavState = 1;
                            gMovieEntryFrame = navEntry.rowIndex;
                            CopyPascalString(navEntry.name, gMovieName);
                            return 1;
                        }
                        break;
                    case 6:
                        if ((rec[2] & 1) == 0
                            || TrackHotspotRectPress((short *)(rec + 8)) != 0)
                        {
                            PlayMovieVoice(rec + 16);
                            NoOpAfterDisplayAttachVoice(i);
                            if (*(int *)((char *)gMovieHeader + 0x878) - 1 <= gMovieFrameIndex)
                            {
                                ShowEngineMessageBoxFromPascalString((byte *)UpdatePascalLength(kStrNoNextFrame));
                                return 3;
                            }
                            gMovieFrameIndex++;
                            return 0;
                        }
                        break;
                    case 7:
                        if ((rec[2] & 1) == 0
                            || TrackHotspotRectPress((short *)(rec + 8)) != 0)
                        {
                            PlayMovieVoice(rec + 16);
                            NoOpAfterDisplayAttachVoice(i);
                            if (gMovieFrameIndex <= 0)
                            {
                                ShowEngineMessageBoxFromPascalString((byte *)UpdatePascalLength(kStrNoPrevFrame));
                                return 3;
                            }
                            gMovieFrameIndex--;
                            return 0;
                        }
                        break;
                    default:
                        FatalError(0, 0x4ac);
                    }
                }
            }
        } while ((gMovieFrame[6] & 4) == 0);
    }
}


/* Index of the frame named pName (case-insensitive) in the frame table at header +0x87c (count at
 * +0x878, 42-byte records, name at +0x1a), or -1. */
int __cdecl FindMovieFrameIndexByName(byte *pName) /* 0x00449c20 */
{
    int   i, n;
    char *p;

    if (*pName == 0)
        return -1;
    n = *(int *)((char *)gMovieHeader + 0x878);
    p = (char *)gMovieHeader + 0x87c;
    for (i = 0; i < n; p += 0x2a, i++)
        if (PascalStringsEqualNoCase(pName, (byte *)(p + 0x1a)) != 0)
            return i;
    return -1;
}


/* With frame flag 1, pumps input until the sound channel stops. Returns 2 or 3 if the pump does,
 * else 0. */
short __stdcall WaitForMovieFrameSound(void) /* 0x00449c80 */
{
    short pumpStatus;

    if ((gMovieFrame[6] & 1) != 0) {
        do {
            pumpStatus = PumpMovieInputOnce();   /* ax tested as is: the callee returns short */
            if (pumpStatus == 3)
                return 3;
            if (pumpStatus == 2)
                return 2;
        } while (CheckSoundChannelStopped() == 0);
    }
    return 0;
}


/* One pass of the movie's input pump: updates the hover cursor, returns 2 if a timed jump fired,
 * 3 if header flag 8 is set and an event of mask 10 is pending. Then handles one queued event: a
 * stop key returns 3, event 6 re-blits the picture, event 0x2a (display change) resizes the window
 * and recentres the picture. Returns 1 if an event of mask 2 is pending, else 0. */
short __stdcall PumpMovieInputOnce(void) /* 0x00449cc0 */
{
    short pollResult;
    uint  keyStatus;
    short event[10];  /* local_14 */
    short sx, sy;

    UpdateMovieHoverCursor();
    if ((short)PollMovieTimedJump() != 0)
        return 2;
    if ((*(byte *)((char *)gMovieHeader + 0x18) & 8) != 0) {
        pollResult = (short)PeekEventQueue(10u, (uint)(uintptr_t)event);
        if (pollResult != 0)
            return 3;
    }
    pollResult = PollEventQueue(0x148u, (unsigned int *)event);
    if (pollResult != 0) {
        keyStatus = HandleMovieKeyEvent(event);
        if ((short)keyStatus != 0)
            return 3;
        if (event[0] == 6) {
            SaveNavigationState((void *)gMovieChannel);
            BlitMovieRect(
                (uint32_t *)((char *)gMovieHeader + 0x86c),
                (uint32_t *)((char *)gMovieHeader + 0x86c));
            RestoreNavigationState((void *)gMovieChannel);
        }
        if (event[0] == 0x2a) {
            DF_TRACE("reactivate(0x2a): a2c0={%d,%d,%d,%d} vp{x=%d,y=%d,h=%d,w=%d} node[16]=%d node[17]=%d",
                     (int)gScrollSceneRect.y1, (int)gScrollSceneRect.x1, (int)gScrollSceneRect.y2, (int)gScrollSceneRect.x2,
                     (int)gSceneViewport.x, (int)gSceneViewport.y, (int)gSceneViewport.h, (int)gSceneViewport.w,
                     ((int *)*(int *)((char *)gMovieChannel + 0x12))[0x16], ((int *)*(int *)((char *)gMovieChannel + 0x12))[0x17]);
            ResetSceneRectToScreen((short *)&gScrollRect);
            ResizeChannelWindow((short *)&gScrollRect, (int)(intptr_t)gMovieChannel);
            /* (*(int *)&gScrollSceneRect.y2) is gScrollRect[2..3]: the exe reads the two words */
            sx = (gScrollSceneRect.x2 - gSceneViewport.w - gScrollSceneRect.x1) >> 1;
            sy = (gScrollSceneRect.y2 - gSceneViewport.h - gScrollSceneRect.y1) >> 1;
            QdSetOrigin((int)(intptr_t)gMovieChannel, -sx, -sy);
        }
        DF_TRACE("reactivate(0x2a): after -> node[16]=%d node[17]=%d",
                 ((int *)*(int *)((char *)gMovieChannel + 0x12))[0x16], ((int *)*(int *)((char *)gMovieChannel + 0x12))[0x17]);
    }
    pollResult = (short)PeekEventQueue(2u, (uint)(uintptr_t)event);
    return pollResult != 0;
}


/* Waits for a click (event 1), handling the hover cursor, timed jumps (returns 2) and the same
 * redraw and display-change events as PumpMovieInputOnce; a stop key returns 3. On a click,
 * stores the point relative to the picture in *pClickPos (packed {y, x}) and returns 1.
 *
 * Port note: MapScreenPointToNode converts the click's own point inside the event record in place;
 * declared as a separate local it was uninitialised and no button could be pressed (class 3). */
static short WaitForMovieClick(uint32_t *pClickPos) /* 0x00449e00 */
{
    /* evbuf is 20 bytes: the exe gives the 18-byte event record a 0x14-byte slot */
    short         polled;
    unsigned int  evbuf[5];
    short        *mp;
    short         sx, sy;

    for (;;) {
        do {
            UpdateMovieHoverCursor();
            if ((short)PollMovieTimedJump() != 0)
                return 2;
            polled = PollEventQueue(0x15eu, evbuf);
        } while (polled == 0);

        DF_TRACE("attach ev: type=%d", (int)((short *)evbuf)[0]);
        if (((short *)evbuf)[0] == 1)
            break;
        if (((short *)evbuf)[0] == 6) {
            SaveNavigationState((void *)gMovieChannel);
            BlitMovieRect(
                (uint32_t *)((char *)gMovieHeader + 0x86c),
                (uint32_t *)((char *)gMovieHeader + 0x86c));
            RestoreNavigationState((void *)gMovieChannel);
        }
        if (((short *)evbuf)[0] == 0x2a) {
            DF_TRACE("reactivate(0x2a): a2c0={%d,%d,%d,%d} vp{x=%d,y=%d,h=%d,w=%d} node[16]=%d node[17]=%d",
                     (int)gScrollSceneRect.y1, (int)gScrollSceneRect.x1, (int)gScrollSceneRect.y2, (int)gScrollSceneRect.x2,
                     (int)gSceneViewport.x, (int)gSceneViewport.y, (int)gSceneViewport.h, (int)gSceneViewport.w,
                     ((int *)*(int *)((char *)gMovieChannel + 0x12))[0x16], ((int *)*(int *)((char *)gMovieChannel + 0x12))[0x17]);
            ResetSceneRectToScreen((short *)&gScrollRect);
            ResizeChannelWindow((short *)&gScrollRect, (int)(intptr_t)gMovieChannel);
            /* (*(int *)&gScrollSceneRect.y2) is gScrollRect[2..3]: the exe reads the two words */
            sx = (gScrollSceneRect.x2 - gSceneViewport.w - gScrollSceneRect.x1) >> 1;
            sy = (gScrollSceneRect.y2 - gSceneViewport.h - gScrollSceneRect.y1) >> 1;
            QdSetOrigin((int)(intptr_t)gMovieChannel, -sx, -sy);
        }
        DF_TRACE("reactivate(0x2a): after -> node[16]=%d node[17]=%d",
                 ((int *)*(int *)((char *)gMovieChannel + 0x12))[0x16], ((int *)*(int *)((char *)gMovieChannel + 0x12))[0x17]);
        if ((short)HandleMovieKeyEvent((short *)evbuf) != 0)
            return 3;
    }

    /* the click's screen point, 10 bytes into the event record, converted in place (in/out) */
    mp = (short *)((char *)evbuf + 10);   /* ESP+0x16 vs ESP+0xc */
    MapScreenPointToNode(gMovieChannel, mp);
    ((short *)evbuf)[6] -= *(short *)((char *)gMovieHeader + 0x24);
    ((short *)evbuf)[5] -= *(short *)((char *)gMovieHeader + 0x26);
    *pClickPos = *(uint32_t *)mp;
    return 1;
}

/* Passes a key event to ProcessMovieKeyInput and returns its stop status. On 'T' it toggles the
 * theme music: stops the template channels if armed, else restarts the theme list (if the block
 * built one) and re-arms; a movie with no templates calls NoOpDisplayAttachAfterRowT instead.
 * gMovieSoundArmed and gMovieSoundPhase are words in the exe (cmp/mov word ptr); the globals
 * generator emits them as int, hence the casts. */
static short HandleMovieKeyEvent(short *pEvent) /* 0x00449f70 */
{
    short  st;
    short  wantPoll;

    st = ProcessMovieKeyInput(pEvent, &wantPoll);
    if (wantPoll != 0) {
        if (gMovieTemplateCount != 0) {
            if (*(short *)&gMovieSoundArmed != 0) {
                StopSoundChannels(0, 0, 1, 0);
                *(short *)&gMovieSoundArmed = 0;
                return st;
            }
            if (*(short *)&gMovieSoundPhase != 0)
                PlayThemeList(gMovieChannelRows);
            *(short *)&gMovieSoundArmed = 1;
            return st;
        }
        NoOpDisplayAttachAfterRowT();
    }
    return st;
}


/* The movie's keys, for a key event (type 3) with one of bits 0x1fa0 of event word 7 set: '.' and
 * 'Q' stop the movie when header flag 1 allows it (returns 1), '0'..'9' set the wave volume level,
 * 'T' sets *pWantPoll (the theme toggle in HandleMovieKeyEvent). Returns 0 otherwise. */
short __cdecl ProcessMovieKeyInput(short *pEvent, short *pWantPoll) /* 0x0044a000 */
{
    *pWantPoll = 0;
    if (*pEvent == 3 && (pEvent[7] & 0x1fa0) != 0) {
        /* the key code, zero-extended (xor edx,edx / mov dl) */
        switch (*(unsigned char *)(pEvent + 1)) {
        case '.':
        case 'Q':
        case 'q':
            return (*(uint32_t *)((char *)gMovieHeader + 0x18) & 1u) != 0;
        /* one case per digit, each pushing its own level: the exe's ten call blocks
         * (the port once collapsed them into one argument-less call, class 2) */
        case '0': SetWaveVolumeIfDeviceReady(0); return 0;
        case '1': SetWaveVolumeIfDeviceReady(1); return 0;
        case '2': SetWaveVolumeIfDeviceReady(2); return 0;
        case '3': SetWaveVolumeIfDeviceReady(3); return 0;
        case '4': SetWaveVolumeIfDeviceReady(4); return 0;
        case '5': SetWaveVolumeIfDeviceReady(5); return 0;
        case '6': SetWaveVolumeIfDeviceReady(6); return 0;
        case '7': SetWaveVolumeIfDeviceReady(7); return 0;
        case '8': SetWaveVolumeIfDeviceReady(8); return 0;
        case '9': SetWaveVolumeIfDeviceReady(9); return 0;
        case 'T':
        case 't':
            *pWantPoll = 1;
            break;
        }
    }
    return 0;
}


/* A lone RET in the exe; HandleMovieKeyEvent calls it on 'T' when the movie has no templates. */
void __stdcall NoOpDisplayAttachAfterRowT(void) /* 0x0044a170 */
{
}


/* Shows the alternate cursor while the pointer is over a hotspot with flag 2 (hover), the normal
 * one otherwise; does nothing when header flag 0x10 (cursor hidden) is set.
 *
 * Port note: the hotspot record is one 64-byte object (Ghidra's local_40 + local_38); declared as a
 * bare uint32_t, its whole-record copy overran the frame and destroyed the return address (class 3). */
static void UpdateMovieHoverCursor(void) /* 0x0044a180 */
{
    typedef struct { unsigned char b[0x40]; } HotRec64;
    short         hit;
    short         pt[2];       /* the cursor point, adjusted in place */
    /* one whole 64-byte hotspot record (rep movsd): +2 flags, +8 the rect (not +4);
     * the count is re-read from frame +0x442 at every iteration */
    HotRec64      rec;
    int           i;
    int           off;

    if ((*(byte *)((char *)gMovieHeader + 0x18) & 0x10) != 0)
        return;
    hit = 0;
    GetSceneCursorPos((int)(intptr_t)gMovieChannel, pt);
    pt[1] -= *(short *)((char *)gMovieHeader + 0x24);
    pt[0] -= *(short *)((char *)gMovieHeader + 0x26);
    i = 0;
    if (*(int *)(gMovieFrame + 0x442) > 0) {
        off = 0;
        do {
            rec = *(HotRec64 *)(gMovieFrame + 0x446 + off);
            if ((rec.b[2] & 2) != 0 &&
                PointInSceneRect(*(ScenePointYX *)pt, (short *)(rec.b + 8)) != 0) {
                hit = 1;
                break;
            }
            off += 0x40;
            i++;
        } while (i < *(int *)(gMovieFrame + 0x442));
    }
    if (hit != 0) {
        DF_TRACE("hover HIT: pt=(y=%d,x=%d) rect{%d,%d,%d,%d} idx=%d flags=%02x",
                 (int)pt[0], (int)pt[1],
                 (int)((short *)(rec.b + 8))[0], (int)((short *)(rec.b + 8))[1],
                 (int)((short *)(rec.b + 8))[2], (int)((short *)(rec.b + 8))[3],
                 i, rec.b[2]);
        InstallCursor((HCURSOR)(UINT_PTR)gCursorAlt);
        return;
    }
    QdInitCursor();
}


/* Arms the block's timed jumps (gMovieTimedJumps: a count, then 28-byte records {startTick, flags,
 * dueTick, frame name}, see PollMovieTimedJump). A record whose frame exists gets bit 0 (fired)
 * cleared and dueTick = startTick - (frame dword +0x16 * 60) / (rate << 10), rate being header +0x28
 * or 300; one whose frame is missing is marked fired. Also clears the voice auto-advance. */
void __stdcall ScheduleMovieTimedJumps(void) /* 0x0044a260 */
{
    /* both records copied whole (rep movsd); the frame's +0x16 is a DWORD (no movsx),
     * not the short at +40 the port once read (class 3) */
    typedef struct { int v[7]; } Rec28;
    typedef struct { unsigned char b[42]; } Rec42;
    int    rate;
    Rec28  row;
    Rec42  hs;
    int    n;
    int    off;
    int    idx;

    n = *(int *)gMovieTimedJumps;
    rate = *(int *)((char *)gMovieHeader + 0x28);
    if (rate == 0)
        rate = 300;
    rate <<= 10;
    if (n > 0) {
        off = 0;
        do {
            row = *(Rec28 *)((char *)gMovieTimedJumps + off + 4);
            idx = FindMovieFrameIndexByName((byte *)&row.v[3]);
            if (idx >= 0) {
                hs = *(Rec42 *)((char *)gMovieHeader + 0x87c + idx * 0x2a);
                row.v[1] &= ~1;
                row.v[2] = row.v[0] + (*(int *)&hs.b[22] * -0x3c) / rate;
            } else {
                row.v[1] |= 1;
            }
            *(Rec28 *)((char *)gMovieTimedJumps + off + 4) = row;
            off += 0x1c;
        } while (--n != 0);
    }
    gMovieNextFrame = -1;
    gMovieVoiceEndTick = 0;
}


/* Returns 1 after moving gMovieFrameIndex: to the frame of the first due timed jump (marking it
 * fired and setting gMovieVoiceEndTick), or, once the voice the current frame started is no longer
 * playing, to the frame its voice record names (gMovieNextFrame). Otherwise 0, also when a due
 * jump names a missing frame.
 *
 * Port note: the port had the voice test inverted, so every voice frame advanced on its first
 * poll (the bedroom close-ups "flicked back"; class 12). */
static short PollMovieTimedJump(void) /* 0x0044a340 */
{
    /* one 28-byte record (Ghidra's local_120 + local_114), copied whole (rep movsd)
     * in and out of the table at gMovieTimedJumps + 4 + 28*i (class 3) */
    typedef struct {
        int  startTick;
        int  flags;          /* bit 0: fired */
        int  dueTick;
        byte name[16];       /* Pascal name of the frame to go to */
    } DeferredRec;
    int            jumpCount, jumpIdx, byteOffset;  /* jumpCount=local_124 */
    int            frameIdx;   /* int, not uint: an int sets its stack slot (0x10) and the reload at exit */  /* local_128 */
    DeferredRec    rec;
    uint32_t       voiceAux;  /* local_104 */
    char           playingName[256];  /* local_100 */

    jumpCount = *(int *)gMovieTimedJumps;
    jumpIdx = 0;
    if (0 < jumpCount) {
        byteOffset = 0;
        do {
            rec = *(DeferredRec *)((char *)gMovieTimedJumps + byteOffset + 4);
            if ((rec.flags & 1) == 0) {
                if ((int)GetTick60Hz() >= rec.dueTick + (int)gMovieStartTick) {
                    rec.flags |= 1;
                    *(DeferredRec *)((int *)gMovieTimedJumps + 1 + jumpIdx * 7) = rec;
                    frameIdx = FindMovieFrameIndexByName(rec.name);
                    if (frameIdx < 0)
                        return 0;
                    gMovieVoiceEndTick = rec.startTick + (int)gMovieStartTick;
                    gMovieFrameIndex = frameIdx;
                    return 1;
                }
            }
            byteOffset += 0x1c;
            jumpIdx++;
        } while (jumpIdx < jumpCount);
    }
    if (gMovieNextFrame >= 0) {
        GetPlayingVoiceName((byte *)playingName, (int *)&voiceAux, &frameIdx);
        /* the test returns 1 for equal: advance once the voice playing is not this frame's */
        if (PascalStringsEqualNoCase(gMovieVoiceName, (byte *)playingName) == 0) {
            gMovieFrameIndex = gMovieNextFrame;
            gMovieNextFrame = -1;
            return 1;
        }
    }
    return 0;
}


/* Shows the current frame. When the frame changed and its rect is not empty, decodes its image
 * child into the offscreen buffer (preloading the next frame first while the attach loader runs).
 * Then starts its voice, sets gMovieFrameDueTick (the frame's duration at frame +2, at least the
 * header's +0x1c; frame flag 8 counts from the previous due tick instead of now), starts the theme
 * list once and runs the transition. A frame more than 6 ticks late skips its transition (returns
 * 0); skipping stops when a frame is within 4 ticks again or after seven skipped frames, and from
 * then on every frame runs its transition until one is on time. Returns the transition's status. */
short __stdcall ShowMovieFrame(void) /* 0x0044a480 */
{
    /* the frame-table record, copied whole: +4 the frame's rect (also RunMovieFrameWipe's
     * argument), +0xc its image child, resolved and detached/released alike */
    typedef struct { unsigned char b[42]; } Rec42;
    int     *hSlot;
    Rec42    row;
    short    err;
    int      tick;

    row = *(Rec42 *)((char *)gMovieHeader + 0x87c + gMovieFrameIndex * 0x2a);
    if (gMovieFrameIndex != gMovieLastFrame)
    {
        if (IsSceneRectEmpty((short *)(row.b + 4)) == 0)
        {
            err = ResolveDisplayAttachSlot((void *)gMovieNode,
                                           gMovieChildBase + *(int *)(row.b + 0xc), &hSlot);
            if (err != 0)
                FatalError(err, 0x4ad);
            if (IsDisplayAttachLoaderRunning(gMovieNode) != 0)
                PreloadNextMovieFrame();
            DecodePageNodeImage(hSlot, (HotspotBuf *)gMovieHotspotBuf);
            if ((*(byte *)((char *)gMovieHeader + 0x18) & 4) != 0)
            {
                err = DetachRenderChild(gMovieNode, gMovieChildBase + *(int *)(row.b + 0xc));
                if (err != 0)
                    FatalError(err, 0x4ae);
            }
            else
            {
                err = ReleaseRenderChild(gMovieNode, gMovieChildBase + *(int *)(row.b + 0xc));
                if (err != 0)
                    FatalError(err, 0x4af);
            }
        }
        gMovieLastFrame = gMovieFrameIndex;
    }

    PlayMovieVoice(gMovieFrame + 0x12);
    tick = GetTick60Hz();
    if (gMovieFrameIndex == 0 || (gMovieFrame[6] & 8) == 0)
        gMovieFrameDueTick = tick;
    /* hold the frame for its own duration or the movie's minimum, whichever is longer */
    if (*(int *)((char *)gMovieHeader + 0x1c) > *(int *)(gMovieFrame + 2))
        gMovieFrameDueTick += *(int *)((char *)gMovieHeader + 0x1c);
    else
        gMovieFrameDueTick += *(int *)(gMovieFrame + 2);
    DF_TRACE("rowTiming: row=%d tick=%u rowDur=%d movieMin=%d -> hold=%d state=%d flags6=%02x",
             gMovieFrameIndex, (unsigned)tick, *(int *)(gMovieFrame + 2),
             *(int *)((char *)gMovieHeader + 0x1c), gMovieFrameDueTick, (int)gMovieRenderPhase,
             (unsigned)gMovieFrame[6]);
    if (gMovieSoundPhase == 1)
    {
        PlayThemeList((void *)gMovieChannelRows);
        gMovieSoundPhase = 2;
    }

    switch (gMovieRenderPhase)
    {
    case 0:
        if (tick > gMovieFrameDueTick + 6)
        {
            gMovieRenderPhase = 1;
            gMovieStallCount = 1;
            return 0;
        }
        return RunMovieFrameWipe((int *)(row.b + 4));
    case 1:
        if (gMovieFrameDueTick + 4 > tick)
        {
            gMovieRenderPhase = 0;
            return RunMovieFrameWipe((int *)(row.b + 4));
        }
        gMovieStallCount++;
        if (gMovieStallCount > 7)
        {
            gMovieRenderPhase = 2;
            return RunMovieFrameWipe((int *)(row.b + 4));
        }
        return 0;
    case 2:
        if (gMovieFrameDueTick + 4 > tick)
            gMovieRenderPhase = 0;
        return RunMovieFrameWipe((int *)(row.b + 4));
    }
    return 0;
}


/* Drains the movie's attach slots; then, when the current frame's action is 6 (next frame) and
 * there is a next frame, activates its data child (+0x10) and, if its rect (+4) is not empty, its
 * image child (+0xc). Fatal 0x4b0 / 0x4b1 on an activation error.
 *
 * Port note: was an empty stub (class 7). */
static void __stdcall PreloadNextMovieFrame(void) /* 0x0044a6f0 */
{
    uint32_t row[11];          /* one 42-byte frame-table record, copied whole as the exe does */
    short    err;
    int      ordered;
    int      next;

    DrainDisplayAttachSlotsForContext((int)gMovieNode);
    next = gMovieFrameIndex + 1;
    if (*(short *)gMovieFrame == 6 && next < *(int *)((char *)gMovieHeader + 0x878)) {
        memcpy(row, (char *)gMovieHeader + 0x87c + next * 0x2a, 42);
        err = ActivateDisplayAttachSlot((void *)gMovieNode, (unsigned int)(gMovieChildBase + (int)row[4]));
        if (err != 0)
            FatalError(err, 0x4b0);
        ordered = IsSceneRectEmpty((const short *)((char *)row + 4));
        if ((short)ordered == 0) {
            err = ActivateDisplayAttachSlot((void *)gMovieNode, (unsigned int)(gMovieChildBase + (int)row[3]));
            if (err != 0)
                FatalError(err, 0x4b1);
        }
    }
}


/* Starts the voice named pVoiceName from the voice table (gMovieVoiceTable, the child at movie header +0x18: 42-byte
 * records, +0 flags, +4 child id, +0xa name, +0x1a the frame to go to when the voice ends). Flag
 * bit 0: load on demand into gMovieVoice, kept if it is already the one loaded (bit 1 loops it);
 * otherwise the voice's preloaded row in gMovieVoiceRows. Sets the auto-advance (gMovieNextFrame,
 * gMovieVoiceName), waits for gMovieVoiceEndTick, then makes the row the primary sound channel.
 *
 * Port note: the record is one 0x2a-byte object (four Ghidra locals); copied into a 4-byte local it
 * overran the frame and destroyed the return address (class 3). */
static void PlayMovieVoice(byte *pVoiceName) /* 0x0044a7c0 */
{
    typedef struct { byte b[0x2a]; } VoiceRow2a;
    int        rowIdx;
    uint32_t   tick;
    short      equal;
    uint32_t  *pVoiceRow;   /* the loop walker, and the row finally played */
    /* one whole 0x2a-byte voice record (local_2c..local_12): the frame lookup uses its +0x1a
     * name, not pVoiceName */
    VoiceRow2a rec;      /* copied whole: rep movsd 10 + movsw */

    rowIdx = FindMovieVoiceRowIndexByName(pVoiceName);
    if (rowIdx < 0)
        return;
    rec = *(VoiceRow2a *)(gMovieVoiceTable + 8 + rowIdx * 0x2a);
    /* gMovieVoiceActive is a WORD in the exe (cmp/mov word ptr); declared int, hence the casts */
    if ((rec.b[0] & 1) != 0) {          /* the exe takes the flagged case first */
        if (*(short *)&gMovieVoiceActive != 0) {
            equal = PascalStringsEqualNoCase(rec.b + 0x0a, gMovieVoice.name);
            if (equal != 0) {
                pVoiceRow = (uint32_t *)&gMovieVoice;
                goto lab_assign;
            }
            if (*(short *)&gMovieVoiceActive != 0) {
                StopSoundChannels(0, 0, 0, 1);
                TeardownVoicePlaybackRow((int)(intptr_t)&gMovieVoice);
            }
        }
        InitSoundNodeEntryFromResource((UINT *)gMovieNode,
                        (UINT)(unsigned)(gMovieChildBase + *(int *)(rec.b + 4)),
                        rec.b + 0x0a, (UINT *)&gMovieVoice);
        if ((rec.b[0] & 2) != 0)
            SetSoundNodeLoop((int)(intptr_t)&gMovieVoice, 1);
        *(short *)&gMovieVoiceActive = 1;
        pVoiceRow = (uint32_t *)&gMovieVoice;
    } else {
        /* rows are 0x68 bytes apart: 0x1a dwords */
        for (pVoiceRow = (uint32_t *)gMovieVoiceRows, rowIdx = 0; rowIdx < gMovieVoiceRowCount; pVoiceRow += 0x1a, rowIdx++)
        {
            equal = PascalStringsEqualNoCase((byte *)(pVoiceRow + 2), pVoiceName);
            if (equal != 0)
                break;
        }
    }
lab_assign:
    gMovieNextFrame = FindMovieFrameIndexByName(rec.b + 0x1a);
    if (gMovieNextFrame >= 0)
        CopyPascalString(rec.b + 0x0a, gMovieVoiceName);
    do {
        tick = GetTick60Hz();
    } while ((int)tick <= gMovieVoiceEndTick);
    AssignPrimarySoundChannel((int *)pVoiceRow);
}


/* Index of the voice named pName (case-insensitive) in the voice table (count at +4, 42-byte
 * records from +8, name at +0xa), or -1. */
int FindMovieVoiceRowIndexByName(byte *pName) /* 0x0044a910 */
{
    int   i, n;
    char *p;

    if (*pName == 0)
        return -1;
    n = *(int *)(gMovieVoiceTable + 4);
    p = (char *)(gMovieVoiceTable + 8);
    for (i = 0; i < n; p += 0x2a, i++)
        if (PascalStringsEqualNoCase(pName, (byte *)(p + 10)) != 0)
            return i;
    return -1;
}


/* With frame flag 2, attaches and detaches the data (+0x10) and image (+0xc) children of the
 * frames from index frame +0xe to frame +0xa, so they are loaded ahead of use. Fatal 0x4b2..0x4b5. */
void __stdcall PretouchMovieFrameRange(void) /* 0x0044a970 */
{
    /* the frame-table record is copied whole (rep movsd 10 + movsw); both child ids are DWORDS
     * (the port once read the first from the short at +40, class 3) */
#pragma pack(push, 2)
    typedef struct { int f0; int f4; int f8; int fc; int f10; unsigned char rest[22]; } Rec42;
#pragma pack(pop)
    int   *child;
    int    hi;
    Rec42  rec;
    int    i;
    int    off;
    short  s;
    int    a;
    int    b;

    if ((gMovieFrame[6] & 2) == 0)
        return;
    hi = *(short *)(gMovieFrame + 10);
    i  = *(short *)(gMovieFrame + 0xe);
    if (i > hi)
        return;
    off = i * 0x2a;
    do {
        if (off >= 0 && i < *(int *)((char *)gMovieHeader + 0x878)) {
            rec = *(Rec42 *)((char *)gMovieHeader + 0x87c + off);
            a = rec.f10;
            b = rec.fc;
            s = AttachRenderChild((int *)gMovieNode, (UINT)(gMovieChildBase + a), (int **)&child);
            if (s != 0)
                FatalError(s, 0x4b2);
            s = DetachRenderChild((int)(intptr_t)gMovieNode, gMovieChildBase + a);
            if (s != 0)
                FatalError(s, 0x4b3);
            s = AttachRenderChild((int *)gMovieNode, (UINT)(gMovieChildBase + b), (int **)&child);
            if (s != 0)
                FatalError(s, 0x4b4);
            s = DetachRenderChild((int)(intptr_t)gMovieNode, gMovieChildBase + b);
            if (s != 0)
                FatalError(s, 0x4b5);
        }
        off += 0x2a;
        i++;
    } while (i <= hi);
}


/* Runs the current frame's transition (kind at frame +0xc) over the picture rect (header +0x86c)
 * in as many steps as ticks remain before gMovieFrameDueTick (at least 1); frame +0x10 is the
 * random-block cell size. Reloads the movie CLUT first when it is dirty, except before the fades
 * (0x11, 0x12); the fades and the cut (0x10) get pRowField, the frame's own rect. Returns the
 * transition's status: 0, or 2 / 3 if input interrupted it.
 *
 * Port note: the picture rect is ONE 8-byte local (Ghidra's sceneRect + local_4); as two C locals
 * every transition got a garbage half and nothing was ever blitted, the black screen (class 3). */
static short __cdecl RunMovieFrameWipe(int *pRowField) /* 0x0044aab0 */
{
    short  steps;
    short  result;
    int    rowKindInt;
    int    ticksLeft;
    uint   uVar4;
    uint   sceneRect[2];  /* local_8 */
    short  rowKind;

    (void)pRowField;
    gMoviePollTick = (int)GetTick60Hz();
    ticksLeft        = gMovieFrameDueTick - gMoviePollTick;
    gMovieCellSize = *(unsigned short *)(gMovieFrame + 0x10);
    rowKind      = *(short *)(gMovieFrame + 0xc);
    *(SceneViewport *)sceneRect = *(SceneViewport *)((char *)gMovieHeader + 0x86c);
    if (ticksLeft <= 0)
        ticksLeft = 1;
    result = 0;
    if (rowKind != 0x11 && rowKind != 0x12 && gMovieClutDirty != 0) {
        ReloadCLUT((int *)gMovieClut);
        gMovieClutDirty = 0;
    }
    rowKindInt = (int)rowKind;
    steps = (short)ticksLeft;
    DF_TRACE("rowPoll: rowKind=%d steps=%d sceneW={%d,%d,%d,%d}",
             (int)rowKind, (int)steps,
             (int)((short *)sceneRect)[0], (int)((short *)sceneRect)[1],
             (int)((short *)sceneRect)[2], (int)((short *)sceneRect)[3]);
    switch (rowKindInt) {
    case 0:
        result = MovieWipeSidesIn(sceneRect, steps);
        break;
    case 1:
        result = MovieWipeSidesOut(sceneRect, steps);
        break;
    case 2:
        result = MovieWipeBoxIn(sceneRect, steps);
        break;
    case 3:
        result = MovieWipeBoxOut(sceneRect, steps);
        break;
    case 4:
        result = MovieScrollDown(sceneRect, steps);
        break;
    case 5:
        result = MovieScrollUp(sceneRect, steps);
        break;
    case 6:
        result = MovieScrollRight(sceneRect, steps);
        break;
    case 7:
        result = MovieScrollLeft(sceneRect, steps);
        break;
    case 8:
        result = MovieWipeDown(sceneRect, steps);
        break;
    case 9:
        result = MovieWipeUp(sceneRect, steps);
        break;
    case 10:
        result = MovieWipeRight(sceneRect, steps);
        break;
    case 0xb:
        result = MovieWipeLeft(sceneRect, steps);
        break;
    case 0xc:
        result = MovieWipeVenetian((short *)sceneRect, steps);
        break;
    case 0xd:
        result = MovieWipeRandomBlocks((short *)sceneRect, steps);
        break;
    case 0xe:
        result = MovieWipeRandomRows(sceneRect, steps);
        break;
    case 0xf:
        result = MovieWipeRandomColumns(sceneRect, steps);
        break;
    case 0x10:
        result = MovieWipePlain(pRowField, steps);
        break;
    case 0x11:
        result = MovieFadeOut(pRowField, steps);
        break;
    case 0x12:
        result = MovieFadeIn(pRowField, steps);
        break;
    case 0x13:
        result = MoviePushLeft(sceneRect, steps);
        break;
    case 0x14:
        result = MoviePushRight(sceneRect, steps);
        break;
    case 0x15:
        result = MoviePushDown(sceneRect, steps);
        break;
    case 0x16:
        result = MoviePushUp(sceneRect, steps);
        break;
    default:
        break;
    }
    return result;
}


/* Transition 0: two vertical strips close in from the left and right edges to the middle.
 *
 * All the transitions share one contract. pRect is the picture rect {y1,x1,y2,x2}; each step
 * BlitMovieRect copies part of the new frame from the offscreen buffer to the window, and
 * WaitMovieWipeStep(i) holds step i until tick i of the transition; a 2 or 3 from the input pump
 * stops early. The whole rect is blitted at the end and the status returned. Each rect is a
 * short[4], because BlitMovieRect reads both dwords: Ghidra shows loose dword/short locals, and as
 * separate C locals GCC scattered them, so every step drew a garbage rect (class 3). */
short __cdecl MovieWipeSidesIn(uint *pRect, short stepCount) /* 0x0044ae10 */
{
    short step;
    short i;
    short st;
    short a[4];
    short b[4];

    st = 0;
    *(SceneViewport *)b = *(SceneViewport *)pRect;
    *(SceneViewport *)a = *(SceneViewport *)pRect;
    step = (a[3] - a[1]) / (short)(stepCount * 2) + 1;
    a[3] = a[1] + step;
    b[1] = b[3] - step;
    for (i = 0; i < stepCount; i++) {
        BlitMovieRect((uint *)a, (uint *)a);
        BlitMovieRect((uint *)b, (uint *)b);
        a[1] += step;
        a[3] += step;
        b[1] -= step;
        b[3] -= step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 1: two vertical strips open from the middle out to the left and right edges. */
short __cdecl MovieWipeSidesOut(uint32_t *pRect, short stepCount)  /* 0x0044AEF0 */
{
    short step;
    short i;
    short st;
    short a[4];
    short b[4];

    /* the copy order (b first) is the exe's stack layout */
    st = 0;
    *(SceneViewport *)b = *(SceneViewport *)pRect;
    *(SceneViewport *)a = *(SceneViewport *)pRect;
    step = (a[3] - a[1]) / (short)(stepCount * 2) + 1;
    a[3] = (a[3] + a[1]) >> 1;
    b[1] = a[3];
    a[1] = a[3] - step;
    b[3] = a[3] + step;
    for (i = 0; i < stepCount; i++) {
        BlitMovieRect((uint32_t *)a, (uint32_t *)a);
        BlitMovieRect((uint32_t *)b, (uint32_t *)b);
        a[1] -= step;
        a[3] -= step;
        b[1] += step;
        b[3] += step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 2: four strips close in from all four edges. */
short __cdecl MovieWipeBoxIn(uint32_t *pRect, short stepCount)  /* 0x0044AFE0 */
{
    short dy;
    short dx;
    short st;
    short i;
    short r0[4];
    short r1[4];
    short r2[4];
    short r3[4];

    /* the copy order is the exe's stack layout */
    st = 0;
    *(SceneViewport *)r3 = *(SceneViewport *)pRect;
    *(SceneViewport *)r2 = *(SceneViewport *)pRect;
    *(SceneViewport *)r1 = *(SceneViewport *)pRect;
    *(SceneViewport *)r0 = *(SceneViewport *)pRect;
    dy = (r1[2] - r1[0]) / (short)(stepCount * 2) + 1;
    dx = (r1[3] - r1[1]) / (short)(stepCount * 2) + 1;
    r2[3] = r2[1] + dx;
    r1[2] = r1[0] + dy;
    r3[1] = r3[3] - dx;
    r0[0] = r0[2] - dy;
    for (i = 0; i < stepCount; i++) {
        BlitMovieRect((uint32_t *)r1, (uint32_t *)r1);
        BlitMovieRect((uint32_t *)r0, (uint32_t *)r0);
        BlitMovieRect((uint32_t *)r3, (uint32_t *)r3);
        BlitMovieRect((uint32_t *)r2, (uint32_t *)r2);
        r2[1] += dx;
        r2[3] += dx;
        r1[0] += dy;
        r1[2] += dy;
        r3[1] -= dx;
        r3[3] -= dx;
        r0[0] -= dy;
        r0[2] -= dy;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 3: a box grows from the centre out to the edges. */
short __cdecl MovieWipeBoxOut(uint32_t *pRect, short stepCount)  /* 0x0044B130 */
{
    short r[4];     /* ONE 8-byte rect {y1,x1,y2,x2}: BlitMovieRect reads both dwords */
    short dy;
    short dx;
    short midY;
    short midX;
    int x2;
    short n2;
    short i;
    short st;

    /* one struct copy, then x2 read after midX: the exe's schedule loads r[3] last */
    st = 0;
    *(SceneViewport *)r = *(SceneViewport *)pRect;
    n2 = stepCount * 2;
    midY = (r[0] + r[2]) >> 1;
    midX = (r[1] + r[3]) >> 1;
    x2 = r[3];
    dy = (r[2] - r[0]) / n2 + 1;
    dx = (x2 - r[1]) / n2 + 1;
    r[0] = midY - dy;
    r[1] = midX - dx;
    r[2] = midY + dy;
    r[3] = midX + dx;
    for (i = 0; i < stepCount; i++) {
        BlitMovieRect((uint32_t *)r, (uint32_t *)r);
        r[0] -= dy;
        r[1] -= dx;
        r[2] += dy;
        r[3] += dx;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 4: the new frame slides in downward from the top edge, its bottom rows first; unlike
 * a push, what is on screen does not move. */
short __cdecl MovieScrollDown(uint32_t *pRect, short stepCount)  /* 0x0044B220 */
{
    short step;
    short i;
    short st;
    short a[4];
    short b[4];

    st = 0;
    *(SceneViewport *)b = *(SceneViewport *)pRect;
    *(SceneViewport *)a = *(SceneViewport *)pRect;
    step = (b[2] - b[0]) / stepCount + 1;
    b[0] = b[2] - step;
    a[2] = a[0] + step;
    for (i = 0; i < stepCount; i++) {
        BlitMovieRect((uint32_t *)b, (uint32_t *)a);
        b[0] -= step;
        a[2] += step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 5: the new frame slides in upward from the bottom edge, its top rows first. */
short __cdecl MovieScrollUp(uint32_t *pRect, short stepCount)  /* 0x0044B2E0 */
{
    short step;
    short i;
    short st;
    short a[4];
    short b[4];

    st = 0;
    *(SceneViewport *)a = *(SceneViewport *)pRect;
    *(SceneViewport *)b = *(SceneViewport *)pRect;
    step = (a[2] - a[0]) / stepCount + 1;
    a[2] = a[0] + step;
    b[0] = b[2] - step;
    for (i = 0; i < stepCount; i++) {
        BlitMovieRect((uint32_t *)a, (uint32_t *)b);
        a[2] += step;
        b[0] -= step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 6: the new frame slides in rightward from the left edge, its right columns first. */
short __cdecl MovieScrollRight(uint32_t *pRect, short stepCount)  /* 0x0044B3A0 */
{
    short step;
    short i;
    short st;
    short a[4];
    short b[4];

    st = 0;
    *(SceneViewport *)a = *(SceneViewport *)pRect;
    *(SceneViewport *)b = *(SceneViewport *)pRect;
    step = (a[3] - a[1]) / stepCount + 1;
    a[1] = a[3] - step;
    b[3] = b[1] + step;
    for (i = 0; i < stepCount; i++) {
        BlitMovieRect((uint32_t *)a, (uint32_t *)b);
        a[1] -= step;
        b[3] += step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 7: the new frame slides in leftward from the right edge, its left columns first. */
short __cdecl MovieScrollLeft(uint32_t *pRect, short stepCount)  /* 0x0044B460 */
{
    short step;
    short i;
    short st;
    short a[4];
    short b[4];

    st = 0;
    *(SceneViewport *)a = *(SceneViewport *)pRect;
    *(SceneViewport *)b = *(SceneViewport *)pRect;
    step = (a[3] - a[1]) / stepCount + 1;
    a[3] = a[1] + step;
    b[1] = b[3] - step;
    for (i = 0; i < stepCount; i++) {
        BlitMovieRect((uint32_t *)a, (uint32_t *)b);
        a[3] += step;
        b[1] -= step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 8: reveals the new frame from top to bottom, one band per step. */
short __cdecl MovieWipeDown(uint32_t *pRect, short stepCount)  /* 0x0044B520 */
{
    short step;
    short i;
    short st;
    short r[4];     /* ONE 8-byte rect {y1,x1,y2,x2}: BlitMovieRect reads both dwords */

    *(SceneViewport *)r = *(SceneViewport *)pRect;
    st = 0;
    step = (r[2] - r[0]) / stepCount + 1;
    r[2] = r[0] + step;
    for (i = 0; i < stepCount; i++) {
        BlitMovieRect((uint32_t *)r, (uint32_t *)r);
        r[0] += step;
        r[2] += step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 9: reveals the new frame from bottom to top, one band per step. */
short __cdecl MovieWipeUp(uint32_t *pRect, short stepCount)  /* 0x0044B5C0 */
{
    short step;
    short i;
    short st;
    short r[4];     /* ONE 8-byte rect {y1,x1,y2,x2}: BlitMovieRect reads both dwords */

    *(SceneViewport *)r = *(SceneViewport *)pRect;
    st = 0;
    step = (r[2] - r[0]) / stepCount + 1;
    r[0] = r[2] - step;
    for (i = 0; i < stepCount; i++) {
        BlitMovieRect((uint32_t *)r, (uint32_t *)r);
        r[0] -= step;
        r[2] -= step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 0xa: reveals the new frame from left to right, one band per step. */
short __cdecl MovieWipeRight(uint32_t *pRect, short stepCount)  /* 0x0044B660 */
{
    short step;
    short i;
    short st;
    short r[4];     /* ONE 8-byte rect {y1,x1,y2,x2}: BlitMovieRect reads both dwords */

    *(SceneViewport *)r = *(SceneViewport *)pRect;
    st = 0;
    step = (r[3] - r[1]) / stepCount + 1;
    r[3] = r[1] + step;
    for (i = 0; i < stepCount; i++) {
        BlitMovieRect((uint32_t *)r, (uint32_t *)r);
        r[1] += step;
        r[3] += step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 0xb: reveals the new frame from right to left, one band per step. */
short __cdecl MovieWipeLeft(uint32_t *pRect, short stepCount)  /* 0x0044B700 */
{
    short step;
    short i;
    short st;
    short r[4];     /* ONE 8-byte rect {y1,x1,y2,x2}: BlitMovieRect reads both dwords */

    *(SceneViewport *)r = *(SceneViewport *)pRect;
    st = 0;
    step = (r[3] - r[1]) / stepCount + 1;
    r[1] = r[3] - step;
    for (i = 0; i < stepCount; i++) {
        BlitMovieRect((uint32_t *)r, (uint32_t *)r);
        r[1] -= step;
        r[3] -= step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 0xc: venetian blinds -- horizontal slats stepCount + 1 rows apart, each growing by
 * one row per step. Takes short * where the others take uint32_t *. */
short __cdecl MovieWipeVenetian(short *pRect, short stepCount)  /* 0x0044B7A0 */
{
    short i;        /* declaration order sets the register choice */
    short st;
    short w;
    short j;
    short r[4];     /* ONE 8-byte rect {y1,x1,y2,x2}: BlitMovieRect reads both dwords */

    st = 0;
    *(SceneViewport *)r = *(SceneViewport *)pRect;
    w = r[2] - r[0];
    for (i = 0; i < stepCount; i++) {
        for (j = 0; j < w; j += stepCount + 1) {
            r[0] = pRect[0] + j;
            r[2] = r[0] + i + 1;
            BlitMovieRect((uint32_t *)r, (uint32_t *)r);
        }
        st = WaitMovieWipeStep(i);
        if (st != 0)
            break;
    }
    BlitMovieRect((uint32_t *)pRect, (uint32_t *)pRect);
    return st;
}


/* Transition 0xd: reveals square cells of gMovieCellSize pixels in random order. A byte per cell
 * records which are shown; each of the scale steps reveals one unshown cell from every run of
 * scale + 1 cells (MovieWipeRandomBlocksStep). */
short __cdecl MovieWipeRandomBlocks(short *pCtx, short scale)  /* 0x0044B840 */
{
  short status;  /* local_8 */
  short cellsAcross;  /* local_6 */
  short h;
  short w;
  uint32_t *pShown;
  short cellsDown;
  uint32_t cellCount;
  short i;

  status = 0;
  h = pCtx[3] - pCtx[1];
  w = pCtx[2] - pCtx[0];
  cellsAcross = h / gMovieCellSize;
  if (cellsAcross * gMovieCellSize < h)
    cellsAcross = cellsAcross + 1;
  cellsDown = w / gMovieCellSize;
  if (cellsDown * gMovieCellSize < w)
    cellsDown = cellsDown + 1;
  cellCount = cellsDown * cellsAcross;
  pShown = (uint32_t *)AllocEngineBuffer(cellCount);
  ZeroBlock(pShown, cellCount);
  for (i = 0; i < scale; i++) {
    MovieWipeRandomBlocksStep((byte *)pShown, (int)cellCount, scale, cellsAcross, pCtx);
    status = WaitMovieWipeStep(i);
    if (status != 0) break;
  }
  FreeEngineBuffer((int)pShown);
  BlitMovieRect((uint32_t *)pCtx, (uint32_t *)pCtx);
  return status;
}


/* One step of MovieWipeRandomBlocks: takes pGrid (gridBytes cells, 0 = not shown yet) in runs of
 * rowWidthMinus1 + 1 cells and, in each run that has one, marks a random unshown cell and blits it.
 * Cell k lies at row k / colCount, column k % colCount of gMovieCellSize cells from pOrigin. */
void __cdecl MovieWipeRandomBlocksStep(byte *pGrid, int gridBytes, short rowWidthMinus1,
                                         short colCount, short *pOrigin)  /* 0x0044B930 */
{
    /* r is ONE 8-byte rect {y1,x1,y2,x2}: BlitMovieRect reads all eight bytes */
    int   row;
    short r[4];
    int   n;
    short sel;
    short k;
    unsigned short cnt;
    short w;

    w = rowWidthMinus1 + 1;
    n = w;
    /* the exe's spilled 16-bit row base and loop-test counter are what C2 makes of this
     * loop: k + row * w is the old running base (n == w) */
    for (row = 0; row < gridBytes / n; row++) {
        for (cnt = 0, k = 0; k < w; k++)
            if (pGrid[k] == 0)
                cnt++;
        if ((short)cnt > 0) {
            sel = (short)RandomUpTo(cnt);
            for (k = 0; k < w; k++) {
                if (pGrid[k] == 0)
                    sel--;
                if (sel <= 0)
                    break;
            }
            pGrid[k] = 1;
            k += row * w;
            r[0] = gMovieCellSize * (k / colCount) + pOrigin[0];
            r[1] = (k - colCount * (k / colCount)) * gMovieCellSize + pOrigin[1];
            r[2] = gMovieCellSize + r[0];
            r[3] = gMovieCellSize + r[1];
            BlitMovieRect((uint *)r, (uint *)r);
        }
        pGrid += n;
    }
}


/* Transition 0xe: reveals stepCount + 1 horizontal strips in random order, one per step.
 * MapRectangleLinearShort maps strip k of a (stepCount + 1)-strip space onto the rect. */
short __cdecl MovieWipeRandomRows(uint *pRect, short stepCount) /* 0x0044ba50 */
{
    /* MovieWipeRandomColumns' twin on the Y axis. mid.s[2] is stepCount + 1 (`inc ax`), not
     * the rect's own s[2] as the port had it; both rects start as whole copies of *pRect. */
    typedef struct { short s[4]; } Rect8;
    short  result;
    Rect8  out;
    Rect8  mid;
    char  *buf;
    unsigned short cnt;
    int    n;
    short  i;
    short  r;
    short  k;

    result = 0;
    n = stepCount + 1;
    buf = (char *)AllocEngineBuffer(n);
    ZeroBlock(buf, n);
    i = 0;
    if (stepCount > 0) {
        do {
            cnt = 0;
            for (k = 0; k < n; k++)
                if (buf[k] == 0)
                    cnt++;
            if ((short)cnt > 0) {
                /* pick the r-th strip not yet shown */
                r = (short)RandomUpTo((unsigned short)cnt);
                for (k = 0; k < n; k++) {
                    if (buf[k] == 0)
                        r--;
                    if (r <= 0)
                        break;
                }
                buf[k] = 1;
                mid = *(Rect8 *)pRect;
                out = *(Rect8 *)pRect;
                mid.s[0] = 0;
                out.s[0] = k;
                out.s[2] = k + 1;
                mid.s[2] = stepCount + 1;
                MapRectangleLinearShort(out.s, mid.s, (short *)pRect);
                BlitMovieRect((uint *)&out, (uint *)&out);
            }
            result = WaitMovieWipeStep(i);
        } while (result == 0 && ++i < stepCount);
    }
    FreeEngineBuffer(buf);
    BlitMovieRect(pRect, pRect);
    return result;
}


/* Transition 0xf: MovieWipeRandomRows with vertical strips. */
short __cdecl MovieWipeRandomColumns(uint *pRect, short stepCount) /* 0x0044bb80 */
{
    /* out = strip k of the rect, mid = the (stepCount + 1)-strip space it is mapped from:
     * mid.s[3] is stepCount + 1 (`inc ax`), not the rect's own s[3] as the port had it */
    typedef struct { short s[4]; } Rect8;
    short  result;
    Rect8  out;
    Rect8  mid;
    char  *buf;
    short  cnt;
    short  i;
    int    n;
    short  r;
    short  k;

    result = 0;
    n = stepCount + 1;
    buf = (char *)AllocEngineBuffer(n);
    ZeroBlock(buf, n);
    i = 0;
    if (stepCount > 0) {
        do {
            cnt = 0;
            for (k = 0; k < n; k++)
                if (buf[k] == 0)
                    cnt++;
            if (cnt > 0) {
                /* pick the r-th strip not yet shown */
                r = (short)RandomUpTo(cnt);
                for (k = 0; k < n; k++) {
                    if (buf[k] == 0)
                        r--;
                    if (r <= 0)
                        break;
                }
                buf[k] = 1;
                mid = *(Rect8 *)pRect;
                out = *(Rect8 *)pRect;
                mid.s[1] = 0;
                out.s[1] = k;
                out.s[3] = k + 1;
                mid.s[3] = stepCount + 1;
                MapRectangleLinearShort(out.s, mid.s, (short *)pRect);
                BlitMovieRect((uint *)&out, (uint *)&out);
            }
            result = WaitMovieWipeStep(i);
        } while (result == 0 && ++i < stepCount);
    }
    FreeEngineBuffer(buf);
    BlitMovieRect(pRect, pRect);
    return result;
}


/* Transition 0x10: a cut. Blits the whole rect and pumps input once; returns 3 or 2 from the pump,
 * else 0. stepCount is unused, but the callers push it. */
short __cdecl MovieWipePlain(uint32_t *pRect, short stepCount)  /* 0x0044BCB0 */
{
  short s;

  BlitMovieRect(pRect, pRect);
  s = PumpMovieInputOnce();
  if (s == 3)
    return 3;
  return (s == 2) ? 2 : 0;
}


/* Transition 0x11: blits the rect, then fades the palette from the movie CLUT to gSecondaryClut in
 * stepCount steps and installs gSecondaryClut. The DF_FIX_CLUT_FADE_REBLIT sites: see MovieFadeIn. */
short __cdecl MovieFadeOut(uint32_t *pRect, short stepCount)  /* 0x0044BCE0 */
{
  short step;          /* declared first: the exe keeps the step in edi, the status in ebx */
  short status;

  status = 0;
  step = 0;
  BlitMovieRect(pRect, pRect);
  if (0 < stepCount) {
    do {
      InterpolateCLUT(gMovieClut, gSecondaryClut, (int)step, stepCount + -1);
#if DF_FIX_CLUT_FADE_REBLIT
      ReblitSceneWindowForCLUTFade();
#endif
      status = WaitMovieWipeStep(step);
      if (status != 0) break;
      step = step + 1;
    } while (step < stepCount);
  }
  ReloadCLUT(gSecondaryClut);
#if DF_FIX_CLUT_FADE_REBLIT
  ReblitSceneWindowForCLUTFade();
#endif
  return status;
}


#if DF_FIX_CLUT_FADE_REBLIT
/* Port-only (no original address): re-blits the whole picture rect for one CLUT fade step.
 * See the port note on MovieFadeIn. */
static void ReblitSceneWindowForCLUTFade(void)
{
    uint sceneW[2];

    sceneW[0] = *(uint *)((char *)gMovieHeader + 0x86c);
    sceneW[1] = *(uint *)((char *)gMovieHeader + 0x870);
    BlitMovieRect(sceneW, sceneW);
}
#endif

/* Transition 0x12: blits the rect, then fades the palette from gSecondaryClut to the movie CLUT in
 * stepCount steps and installs the movie CLUT.
 *
 * Port note: the original only animates the hardware palette, which recoloured an 8-bit screen; on a
 * true-colour desktop that changes nothing already drawn, so DF_FIX_CLUT_FADE_REBLIT re-blits the
 * whole picture (not just pRect) after each step and at the end (port-bug-classes.md section 8). */
short __cdecl MovieFadeIn(uint32_t *pRect, short stepCount)  /* 0x0044BD60 */
{
  short status;          /* the status (edi) */
  short step;          /* the step (ebx) */

  status = 0;
  BlitMovieRect(pRect, pRect);
  for (step = 0; step < stepCount; step++) {
    InterpolateCLUT(gSecondaryClut, gMovieClut, (int)step, stepCount + -1);
#if DF_FIX_CLUT_FADE_REBLIT
    ReblitSceneWindowForCLUTFade();
#endif
    status = WaitMovieWipeStep(step);
    if (status != 0) break;
  }
  ReloadCLUT(gMovieClut);
#if DF_FIX_CLUT_FADE_REBLIT
  ReblitSceneWindowForCLUTFade();
#endif
  return status;
}


/* Transition 0x16: the picture on screen is pushed up while the new frame enters from the bottom.
 * Each step ShiftMovieScreenRect moves what is on screen (r2 -> r0) up by one step and
 * BlitMovieRect draws the new frame's top rows (r1) in the strip freed at the bottom (r3); the
 * other pushes are the same on other edges.
 *
 * Port note: was a one-line alias of MoviePushLeft, so it played as the other push (class 12). */
short __cdecl MoviePushUp(uint32_t *pRect, short stepCount)  /* 0x0044BDE0 */
{
    short st;
    short step;
    short r0[4];
    short r1[4];
    short r2[4];
    short r3[4];
    short i;

    /* four rects, all copies of *pRect; the copy order is the exe's stack layout */
    st = 0;
    *(SceneViewport *)r1 = *(SceneViewport *)pRect;
    *(SceneViewport *)r3 = *(SceneViewport *)pRect;
    *(SceneViewport *)r2 = *(SceneViewport *)pRect;
    *(SceneViewport *)r0 = *(SceneViewport *)pRect;
    step = (r1[2] - r1[0]) / stepCount + 1;
    r1[2] = r1[0] + step;
    r3[0] = r3[2] - step;
    r0[2] -= step;
    r2[0] += step;
    for (i = 0; i < stepCount; i++) {
        QdSetPort((int *)(gMovieChannel + 0x12));
        ShiftMovieScreenRect((uint32_t *)r2, (uint32_t *)r0);
        BlitMovieRect((uint32_t *)r1, (uint32_t *)r3);
        r1[2] += step;
        r3[0] -= step;
        *(SceneViewport *)r2 = *(SceneViewport *)r0;
        r0[2] -= step;
        r2[0] += step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 0x15: the picture on screen is pushed down while the new frame enters from the top. */
short __cdecl MoviePushDown(uint32_t *pRect, short stepCount)  /* 0x0044BEF0 */
{
    short step;
    short i;
    short st;
    short r0[4];
    short r1[4];
    short r2[4];
    short r3[4];

    /* four rects, all copies of *pRect; the copy order is the exe's stack layout */
    st = 0;
    *(SceneViewport *)r1 = *(SceneViewport *)pRect;
    *(SceneViewport *)r3 = *(SceneViewport *)pRect;
    *(SceneViewport *)r2 = *(SceneViewport *)pRect;
    *(SceneViewport *)r0 = *(SceneViewport *)pRect;
    step = (r1[2] - r1[0]) / stepCount + 1;
    r1[0] = r1[2] - step;
    r3[2] = r3[0] + step;
    r0[0] += step;
    r2[2] -= step;
    for (i = 0; i < stepCount; i++) {
        QdSetPort((int *)(gMovieChannel + 0x12));
        ShiftMovieScreenRect((uint32_t *)r2, (uint32_t *)r0);
        BlitMovieRect((uint32_t *)r1, (uint32_t *)r3);
        r1[0] -= step;
        r3[2] += step;
        *(SceneViewport *)r2 = *(SceneViewport *)r0;
        r0[0] += step;
        r2[2] -= step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 0x13: the picture on screen is pushed left while the new frame enters from the right. */
short __cdecl MoviePushLeft(uint32_t *pRect, short stepCount)  /* 0x0044C000 */
{
    short step;
    short i;
    short st;
    short r0[4];
    short r1[4];
    short r2[4];
    short r3[4];

    /* four rects, all copies of *pRect; the copy order is the exe's stack layout */
    st = 0;
    *(SceneViewport *)r1 = *(SceneViewport *)pRect;
    *(SceneViewport *)r3 = *(SceneViewport *)pRect;
    *(SceneViewport *)r2 = *(SceneViewport *)pRect;
    *(SceneViewport *)r0 = *(SceneViewport *)pRect;
    step = (r1[3] - r1[1]) / stepCount + 1;
    r1[3] = r1[1] + step;
    r3[1] = r3[3] - step;
    r0[3] -= step;
    r2[1] += step;
    for (i = 0; i < stepCount; i++) {
        QdSetPort((int *)(gMovieChannel + 0x12));
        ShiftMovieScreenRect((uint32_t *)r2, (uint32_t *)r0);
        BlitMovieRect((uint32_t *)r1, (uint32_t *)r3);
        r1[3] += step;
        r3[1] -= step;
        *(SceneViewport *)r2 = *(SceneViewport *)r0;
        r0[3] -= step;
        r2[1] += step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Transition 0x14: the picture on screen is pushed right while the new frame enters from the left. */
short __cdecl MoviePushRight(uint *pRect, short stepCount) /* 0x0044c110 */
{
    short step;
    short i;
    short st;
    short r0[4];
    short r1[4];
    short r2[4];
    short r3[4];

    /* four rects, all copies of *pRect; the copy order is the exe's stack layout */
    st = 0;
    *(SceneViewport *)r1 = *(SceneViewport *)pRect;
    *(SceneViewport *)r3 = *(SceneViewport *)pRect;
    *(SceneViewport *)r2 = *(SceneViewport *)pRect;
    *(SceneViewport *)r0 = *(SceneViewport *)pRect;
    step = (r1[3] - r1[1]) / stepCount + 1;
    r1[1] = r1[3] - step;
    r3[3] = r3[1] + step;
    r0[1] += step;
    r2[3] -= step;
    for (i = 0; i < stepCount; i++) {
        QdSetPort((int *)(gMovieChannel + 0x12));
        ShiftMovieScreenRect((uint *)r2, (uint *)r0);
        BlitMovieRect((uint *)r1, (uint *)r3);
        r1[1] -= step;
        r3[3] += step;
        *(SceneViewport *)r2 = *(SceneViewport *)r0;
        r0[1] += step;
        r2[3] -= step;
        st = WaitMovieWipeStep(i);
        if (st != 0) break;
    }
    BlitMovieRect(pRect, pRect);
    return st;
}


/* Pumps input until tick gMoviePollTick + timeoutTicks and returns 0, or returns 2 / 3 as soon as
 * the pump does. The transitions pass the step index, so step i is held until tick i. */
short __cdecl WaitMovieWipeStep(short timeoutTicks)  /* 0x0044c220 */
{
    short    status;

    for (;;) {
        status = PumpMovieInputOnce();
        if (status == 3) return 3;
        if (status == 2) return 2;
        /* the global is read before the tick call (MOV EDI before CALL) */
        if (gMoviePollTick + timeoutTicks <= (int)GetTick60Hz())
            return 0;
    }
}


/* Copies rect pPacked of the movie's offscreen buffer (gMovieSavedHotspotBuf) to rect pPacked2 of
 * the movie window. Both are clipped to the picture rect (header +0x86c), and the destination is
 * offset by the picture's position in the window (header +0x24 x, +0x26 y). */
void __cdecl BlitMovieRect(uint *pPacked, uint *pPacked2) /* 0x0044c260 */
{
    uint rectA[2];    /* coord pair A -- local copy */
    uint rectB[2];    /* coord pair B -- local copy */
    uint sceneW[2];   /* the picture rect (2 dwords = 4 shorts) from the header */

    rectA[0] = pPacked[0];   rectA[1] = pPacked[1];
    rectB[0] = pPacked2[0];  rectB[1] = pPacked2[1];

    /* one 8-byte struct copy: the exe loads both dwords before storing either */
    *(SceneViewport *)sceneW = *(SceneViewport *)((char *)gMovieHeader + 0x86c);

    DF_TRACE("stepPair: A={%d,%d,%d,%d} B={%d,%d,%d,%d} W={%d,%d,%d,%d}",
             (int)((short *)rectA)[0], (int)((short *)rectA)[1],
             (int)((short *)rectA)[2], (int)((short *)rectA)[3],
             (int)((short *)rectB)[0], (int)((short *)rectB)[1],
             (int)((short *)rectB)[2], (int)((short *)rectB)[3],
             (int)((short *)sceneW)[0], (int)((short *)sceneW)[1],
             (int)((short *)sceneW)[2], (int)((short *)sceneW)[3]);
    if ((short)IntersectSceneRects((short *)sceneW, (short *)rectA, rectA) != 0) {
        if ((short)IntersectSceneRects((short *)sceneW, (short *)rectB, rectB) != 0) {
            OffsetSceneRect((short *)rectB,
                            *(short *)((char *)gMovieHeader + 0x24),
                            *(short *)((char *)gMovieHeader + 0x26));
            QdSetPort((int *)((char *)gMovieChannel + 0x12));
            QdCopyBits((int *)gMovieSavedHotspotBuf,
                                          (int *)((char *)gMovieChannel + 0x12),
                                          (short *)rectA,
                                          (short *)rectB);
        }
    }
}


/* BlitMovieRect from window to window: moves what is already on screen from pFrom to pTo, with the
 * same clip and offset. The pushes use it. */
static void __cdecl ShiftMovieScreenRect(uint *pFrom, uint *pTo)  /* 0x0044c340 */
{
    uint from[2];
    uint to[2];
    uint clip[2];

    from[0] = pFrom[0];  from[1] = pFrom[1];
    to[0]   = pTo[0];    to[1]   = pTo[1];
    /* one 8-byte struct copy: the exe loads both dwords before storing either */
    *(SceneViewport *)clip = *(SceneViewport *)((char *)gMovieHeader + 0x86c);
    if ((short)IntersectSceneRects((short *)clip, (short *)from, (int *)from) != 0) {
        if ((short)IntersectSceneRects((short *)clip, (short *)to, (int *)to) != 0) {
            OffsetSceneRect((short *)to,
                            *(short *)((char *)gMovieHeader + 0x24),
                            *(short *)((char *)gMovieHeader + 0x26));
            QdSetPort((int *)((char *)gMovieChannel + 0x12));
            QdCopyBits((int *)((char *)gMovieChannel + 0x12),
                                          (int *)((char *)gMovieChannel + 0x12),
                                          (short *)from, (short *)to);
        }
    }
}
