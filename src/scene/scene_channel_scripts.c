/* scene_channel_scripts.c -- TI.EXE 0x004291a0..0x0042b780: the scene channel, the Boot/Post/Server
 * scripts and the method cache. */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "df_protos.h"
#include "topics/scriptcmd.h"
#include "topics/scriptquery.h"
#include "topics/scene.h"
#include "topics/multimedia.h"
#include "topics/anim.h"
#include "topics/hotspot_raster.h"

#include "topics/savestate.h"

#include "topics/scenerect.h"

/* Resets the per-session scene state: frame rate 3, the five resource tables (actor, cast, prop,
 * shop, track) emptied, the set/stage/puppet guards cleared and the text and puppet defaults set.
 * With mode != 0 (BeginGameSession passes 1) it also opens the "bootfile" container from search
 * path slot 0, reads the scene and overlay sizes from its sub-resource 0, centres the scene in
 * the scroll rect and attaches sub-resources 1-3.
 * FatalError's second argument is the "line" its message box reports: 0x10bf..0x10c6 here
 * (4287..4294), one per failing step, as everywhere in this file.
 *
 * Port note: the centring once read gScrollRect by value, which decayed to the array's address;
 * each word is read from the rect, as the exe does. */
void __cdecl InitSceneResource(short mode)  /* 0x004291a0 */
{
    short           sVar;
    byte           *pDescriptor;
    int             iData;
    int            *pSubRes;
    unsigned short  localC;
    short           localA;
    int             localB;

    gFrameRate = 3;
    gVisualEffectDepth = 0;
    gKeyAborts = 0;
    gSceneFrame = 0;
    gActionFrameFlags = 0;

    gActorTable = AllocResObject(0);
    gActorCount = 0;
    gCastTable = AllocResObject(0);
    gCastCount = 0;
    gPropTable = AllocResObject(0);
    gPropCount = 0;
    gShopTable = AllocResObject(0);
    gShopCount = 0;
    gTrackTable = AllocResObject(0);

    gStage.active = 0;
    gSet.active = 0;
    gPuppet.active = 0;
    gTrackCount = 0;

    gClutSyncFirst = 0;
    gClutSyncEnd = 0x80;
    gTextColor = 0xfa;              /* palette indices */
    gTextHighlightColor = 0xfb;
    gTextFont = 0x378;
    gTextSize = 0xc;
    gPuppetParam7 = 0;
    gPuppetNoTimeout = 0;
    gSpeakTicksPerChar = 2;
    gTextIndent = 8;
    gPuppetGrab = 0;
    /* a byte store in the exe: the length byte of the Pascal string at 0x0048a842 */
    *(unsigned char *)&gPuppetBase = 0;

    if (mode != 0)
    {
        *(short *)&gBootScriptRan = 0;   /* a word in the exe */

        GetSearchPathSlot(0, &gScratchStr);

        /* The kStr constants are stored "\0text"; UpdatePascalLength fills in the length byte
         * and returns the string. Written nested in the argument list throughout this file:
         * the exe pushes the other arguments before it calls the lookup. */
        AppendPascalString((byte *)UpdatePascalLength((char *)&kStrBootfile), &gScratchStr);

        DF_TRACE("bootpath: len=%d '%.*s'", (int)(unsigned char)gScratchStr[0],
                 (int)(unsigned char)gScratchStr[0], (const char *)&gScratchStr[1]);
        sVar = ProbeContainerFile(&gScratchStr, &localA, &localB, &localC);
        if (sVar != 0)
            FatalError(sVar, 0x10bf);
        if (localA != 0)
            FatalError(0, 0x10c0);

        sVar = OpenContainerNode(&gScratchStr, &gBootNode);
        if (sVar != 0)
            FatalError(sVar, 0x10c2);

        sVar = AttachRenderChild(gBootNode, 0, &pSubRes);
        if (sVar != 0)
            FatalError(sVar, 0x10c3);

        iData = GetResDataPtr(pSubRes);
        gSceneW     = *(short *)(iData + 0x14);   /* canvas width  */
        gSceneH     = *(short *)(iData + 0x16);   /* canvas height */
        gOverlayW = *(short *)(iData + 0x10);
        gOverlayH = *(short *)(iData + 0x12);
        UnlockResObject(pSubRes);

        /* gScrollRect is a short[4] {y1,x1,y2,x2}; the same expression as SceneChannelHandler's
         * message 0x10, each word read from the rect (`movsx edx,[0x48a2c6]`) */
        gSceneOriginX = ((short *)&gScrollRect)[1] +
                       (short)(((int)((short *)&gScrollRect)[1]
                              + ((int)gSceneW - (int)((short *)&(*(int *)&gScrollSceneRect.y2))[1]))
                              / 2);
        gSceneOriginY = gScrollSceneRect.y1 +
                       (short)(((int)gScrollSceneRect.y1
                              + ((int)gSceneH - (int)(short)(*(int *)&gScrollSceneRect.y2)))
                              / 2);
        DF_TRACE("centre: rect={%d,%d,%d,%d} sceneW=%d sceneH=%d -> 86c=%d 86a=%d",
                 (int)gScrollSceneRect.y1, (int)gScrollSceneRect.x1,
                 (int)gScrollSceneRect.y2, (int)gScrollSceneRect.x2,
                 (int)gSceneW, (int)gSceneH, (int)gSceneOriginX, (int)gSceneOriginY);

        sVar = AttachRenderChild(gBootNode, 1, &pSubRes);
        if (sVar != 0)
            FatalError(sVar, 0x10c4);

        sVar = AttachRenderChild(gBootNode, 2, &pSubRes);
        if (sVar != 0)
            FatalError(sVar, 0x10c5);

        sVar = AttachRenderChild(gBootNode, 3, &pSubRes);
        if (sVar != 0)
            FatalError(sVar, 0x10c6);
    }
}


/* Undoes InitSceneResource. A pre-shutdown pass over the cast and shop entries and the set, stage
 * and puppet subsystems, then every entry of the five resource tables is closed and the tables
 * freed: actor rows 0xa0 bytes, cast 0x1c, prop 0x9e, shop 0x1c, track 0x28. With mode != 0 it
 * also detaches bootfile sub-resources 0-3 and destroys the bootfile node (fatal 0x10c7..0x10cb). */
void __cdecl ShutdownSceneResource(short mode)  /* 0x004294a0 */
{
    /* Each table row is copied out whole (`rep movsd`, plus `movsw` for the
     * 0x9e-byte slot-2 rows). Declaration order sets the frame layout: the
     * two 7-int rows at +0 / +0x1c, the 10-int row at +0x38, the 40-int row at
     * +0x60, the slot-2 row at +0x100. */
    typedef struct { int v[7]; } Rec7;
    typedef struct { int v[10]; } Rec10;
    typedef struct { int v[40]; } Rec40;
    typedef struct { char b[0x9e]; } Rec9E;
    int    iSlot;
    int    j;          /* the slot-3 pass has its own counter: ebx/ebp swap roles */
    Rec7  *p7;
    Rec10 *p10;
    Rec40 *p40;
    Rec9E *p9e;
    Rec7   castRec;  /* local_1a0 */
    Rec7   shopRec;  /* local_184 */
    Rec10  trackRec;  /* local_168 */
    Rec40  actorRec;  /* local_140 */
    Rec9E  propRec;  /* local_a0 */

    for (iSlot = 0; gCastCount > iSlot; iSlot++) {
        castRec = ((Rec7 *)GetResDataPtr((int *)gCastTable))[iSlot];
        UnlockResObject((int *)gCastTable);
        (void)CastPreShutdownStep(castRec.v[0]);
    }
    for (j = 0; gShopCount > j; j++) {
        shopRec = ((Rec7 *)GetResDataPtr((int *)gShopTable))[j];
        UnlockResObject((int *)gShopTable);
        (void)ShopPreShutdownStep(shopRec.v[0]);
    }

    if (gSet.active != 0)
        SetPreShutdownStep();
    if (gStage.active != 0)
        StagePreShutdownStep();
    if (gPuppet.active != 0)
        SendClosePuppetMessage();

    p40 = (Rec40 *)GetResDataPtr((int *)gActorTable);
    iSlot = 0;
    if (gActorCount > 0)
    {
        do {
            actorRec = *p40++;
            TeardownActorEntry((int)&actorRec);
            iSlot++;
        } while (gActorCount > iSlot);
    }
    UnlockResObject((int *)gActorTable);
    FreeResObject(gActorTable);

    p7 = (Rec7 *)GetResDataPtr((int *)gCastTable);
    iSlot = 0;
    if (gCastCount > 0)
    {
        do {
            castRec = *p7++;
            CloseCastNode(castRec.v);
            iSlot++;
        } while (gCastCount > iSlot);
    }
    UnlockResObject((int *)gCastTable);
    FreeResObject(gCastTable);

    p9e = (Rec9E *)GetResDataPtr((int *)gPropTable);
    iSlot = 0;
    if (gPropCount > 0)
    {
        do {
            propRec = *p9e++;
            TeardownPropEntry((int)&propRec);
            iSlot++;
        } while (gPropCount > iSlot);
    }
    UnlockResObject((int *)gPropTable);
    FreeResObject(gPropTable);

    p7 = (Rec7 *)GetResDataPtr((int *)gShopTable);
    iSlot = 0;
    if (gShopCount > 0)
    {
        do {
            shopRec = *p7++;
            CloseShopNode(shopRec.v);
            iSlot++;
        } while (gShopCount > iSlot);
    }
    UnlockResObject((int *)gShopTable);
    FreeResObject(gShopTable);

    p10 = (Rec10 *)GetResDataPtr((int *)gTrackTable);
    iSlot = 0;
    if (gTrackCount > 0)
    {
        do {
            trackRec = *p10++;
            CloseTrackNode(trackRec.v);
            iSlot++;
        } while (gTrackCount > iSlot);
    }
    UnlockResObject((int *)gTrackTable);
    FreeResObject(gTrackTable);

    CloseSetNode((short *)&gSet);
    CloseStageNode((short *)&gStage);
    ClosePuppetNode((short *)&gPuppet);

    if (mode != 0)
    {
        short sVar;

        sVar = DetachRenderChild((int)gBootNode, 0);
        if (sVar != 0) FatalError(sVar, 0x10c7);

        sVar = DetachRenderChild((int)gBootNode, 1);
        if (sVar != 0) FatalError(sVar, 0x10c8);

        sVar = DetachRenderChild((int)gBootNode, 2);
        if (sVar != 0) FatalError(sVar, 0x10c9);

        sVar = DetachRenderChild((int)gBootNode, 3);
        if (sVar != 0) FatalError(sVar, 0x10ca);

        sVar = DestroyRenderNode(gBootNode);
        if (sVar != 0) FatalError(sVar, 0x10cb);
    }
}


/* Builds channel B (gChannelB) as a type-1 scene channel on the "Runtime" viewport, then clears
 * the render-lock count, sends it message 7 (set position) with the scroll rect and shows it.
 * The channel record is 146 bytes, the same for all three channels (0x0048a560, 0x0048a5f2,
 * 0x0048a684): short type, eight reserved shorts, the resource handle at +18, then the body
 * SetChannelWindowState fills in. */
void __stdcall InitSceneNodeChannel(void)  /* 0x00429840 */
{
    byte   nameBuf[256];
    byte  *pDescriptor;
    void  *pResource;

    short  channelState[73];   /* the 146-byte channel record */

    CopyPascalString((byte *)UpdatePascalLength((char *)&kStrRuntime), nameBuf);
    CreateSceneViewport(nameBuf, &pResource, 0);

    /* the reserved shorts are four dword stores in the exe */
    *(int *)&channelState[1] = 0;
    *(int *)&channelState[3] = 0;
    *(int *)&channelState[5] = 0;
    *(int *)&channelState[7] = 0;
    *(void **)&channelState[9] = pResource;
    channelState[0] = 1;                      /* type 1: scene */

    SetChannelWindowState(pResource, channelState);

    /* copied as one record (rep movsd 0x24 + movsw) */
    {
        typedef struct { char b[146]; } ChanImg;
        *(ChanImg *)&gChannelB = *(ChanImg *)channelState;
    }

    gSceneChannelRefCount = 0;
    DispatchChannelMessage((short *)&gChannelB, 7, (byte *)&gScrollRect);
    ShowChannelWindow((int)&gChannelB);
}


/* Copies search-path slot index (0-8) into pOut (256 bytes). gSearchPathTable holds nine
 * 256-byte Pascal strings, set up by InitScriptRuntime. Fatal 0x10cc (4300) if the table is not
 * allocated or the index is out of range. */
void __cdecl GetSearchPathSlot(short index, byte *pOut)  /* 0x004298f0 */
{
    if ((gSearchPathTable == 0) || (index < 0) || (index > 8))
    {
        DF_TRACE("!! GetSearchPathSlot: index=%d table=%p -- fatal 0x10cc",
                 (int)index, gSearchPathTable);
        FatalError(0, 0x10cc);
    }
    CopyPascalString((byte *)((int)gSearchPathTable + index * 0x100), pOut);
}


/* Stores the Pascal string pState in search-path slot index (0-8); fatal 0x10cd as
 * GetSearchPathSlot. InitScriptRuntime sets all nine to the module's boot directory. */
void __cdecl SetSearchPathSlot(short index, unsigned char *pState)  /* 0x00429940 */
{
    if (gSearchPathTable == NULL || index < 0 || index > 8)
        FatalError(0, 0x10cd);

    CopyPascalString(pState, (unsigned char *)((int)gSearchPathTable + index * 0x100));
}


/* Checks a file name a script passed (Pascal string) and, when it has a type suffix, finds the
 * file. The name must be 3-12 characters, start with a letter and hold at most one '.', with at
 * most 8 characters before it and 1-3 after (DOS 8.3). A qualified name is looked up in search
 * path slots 1-8, an already-open node first and then the disk, and pDesc is replaced by the
 * full path. Returns 0 found, 0x29 no '.' (the caller qualifies it), 0x2a in no slot, 0x31 the
 * file exists but failed to open, 0x34 malformed. Reserves 200000 bytes of heap first (fatal 0x10ce).
 * fileType is the four-character type every caller pushes ('CCDO', 'PUPP', 'SONG', 'SCDO',
 * 'PDDO', 'FLAT', 'MOVE'); this build never reads it.
 *
 * Port note: the exe increments the slot index before testing it, so slot 9 is never fetched;
 * the port once tested first and died with fatal 0x10cc instead of returning 0x2a. */
short __cdecl ValidateDescriptorPath(byte *pDesc, int fileType)  /* 0x00429990 */
{
    /* the four address-taken locals first: frame +4 containerErr, +6 bDotFound,
     * +8 containerOut, +2 isReadOnly, buffer at +0xc */
    short   iPos;              /* loop index / dot position */
    short   bDotFound;         /* 1 if descriptor has TYPE.NAME format */
    short   containerErr;      /* ProbeContainerFile open-error output */
    unsigned short isReadOnly; /* ProbeContainerFile read-only flag */
    int     containerOut;      /* ProbeContainerFile handle output */
    byte    slotPath[256];    /* working descriptor buffer */  /* local_100 */
    byte    bLen;              /* descriptor length byte */
    byte    bFirst;            /* first character of name */
    int     suffixLen;         /* characters after the '.' separator */
    short   heapErr;           /* return value of ReserveHeapSpace */

    bDotFound = 0;
    bLen = pDesc[0];

    if (bLen < 3 || bLen > 0xc)
        return 0x34;

    /* a letter: 'A'..'z' without the six characters between 'Z' and 'a' */
    bFirst = pDesc[1];
    if (bFirst < 0x41 || bFirst > 0x7a)
        return 0x34;
    if (bFirst > 0x5a && bFirst < 0x61)
        return 0x34;

    for (iPos = 1; iPos <= bLen; iPos++) {
        if (pDesc[iPos] == '.') {
            if (bDotFound != 0)
                return 0x34;    /* double dot -- invalid */
            if (iPos > 9)
                return 0x34;    /* prefix too long */
            suffixLen = bLen - iPos;
            if (suffixLen > 3)
                return 0x34;    /* suffix too long */
            if (suffixLen < 1)
                return 0x34;    /* empty suffix */
            bDotFound++;        /* `inc word ptr` in the exe */
        }
    }

    /* ReserveHeapSpace's note out-parameter is bDotFound's own slot in the exe; the callee
     * leaves it alone on success, so the flag survives the call */
    heapErr = ReserveHeapSpace(200000, &bDotFound);
    if (heapErr != 0)
        FatalError(heapErr, 0x10ce);

    if (bDotFound == 0)
        return 0x29;    /* unqualified name */

    iPos = 1;
    do {
        GetSearchPathSlot(iPos, slotPath);
        AppendPascalString(pDesc, slotPath);

        /* The (short) is for the per-function match harness, where this callee's declaration
         * sits under an #ifndef that is already defined and it would be implicit int. */
        if ((short)MatchPendingRenderNodeDescriptor(slotPath) != 0) {
            CopyPascalString(slotPath, pDesc);
            return 0;
        }

        /* the return code and the open-error out-parameter are different values */
        if (ProbeContainerFile(slotPath, &containerErr, &containerOut, &isReadOnly) == 0) {
            if (containerErr != 0)
                return 0x31;
            CopyPascalString(slotPath, pDesc);
            return 0;
        }
        iPos++;
    } while (iPos <= 8);
    return 0x2a;    /* all slots exhausted */
}



/* Message handler of the type-1 (scene) channel, reached through DispatchChannelMessage.
 * The messages that call into the scripts are dropped while gMethodFrameLatched holds a failed
 * script frame: 0 mousedown, 2 keydown, 0xb menustate(), 0xc menuselect, 0xf keyrepeat.
 * The others: 1 repaints the four strips around the viewport, 3 reloads the CLUT, 4 destroys
 * the viewport, 5/6 enter/leave the render lock (counted: the first sets the origin and clip,
 * the last resets them; fatal 0x10cf on underflow), 7 resizes the window to the rect at
 * pParam, 8 reshows the set view, 9 is the per-frame reset (boot() once per session, then
 * idle()), 0xe writes a 20-byte scene-info reply to pParam, 0x10 recentres the scene.
 * pParam is a pointer for 7, 0xc and 0xe and a value for 0, 2, 3 and 0xf.
 *
 * Port note: message 0's pParam is a packed point passed by value; reading it as a pointer
 * crashed live. */
void __cdecl SceneChannelHandler(int pChannel, short msg, byte *pParam)  /* 0x00429b70 */
{
    /* Three 8-byte rects at frame +0 / +8 / +0x10 (whole-struct copies in the
     * exe); case 0's packed point and case 0xe's 20-byte reply are block-scoped
     * and share the slot at +0x18. Frame 0x2c. */
    typedef struct { short s[4]; } SceneRect8;
    typedef struct { short w[6]; SceneRect8 r; } SceneInfo20;
    byte *layoutPad;          /* unused, but VC++ 4.2's frame layout needs it */
    SceneRect8 rect;       /* working rect */
    SceneRect8 sceneRect;
    SceneRect8 viewRect;

    switch (msg)
    {
    case 0:
        /* mouse down: the packed point {y, x} plus the scene origin */
        if (gMethodFrameLatched == 0)
        {
            int pt;
            EnterSceneRenderLock(pChannel);
            /* the exe copies the whole value, then adds into each half */
            pt = (int)pParam;
            {
                short hi = gSceneOriginX;
                hi += ((short *)&pParam)[1];
                ((short *)&pt)[1] = hi;
            }
            ((short *)&pt)[0] += gSceneOriginY;
            DF_TRACE("sceneMsg0: pParam=(y=%d,x=%d) origin=(x=%d,y=%d) -> mousedown(y=%d,x=%d)",
                     (int)(short)(intptr_t)pParam, (int)(short)((uint32_t)(uintptr_t)pParam >> 16),
                     (int)gSceneOriginX, (int)gSceneOriginY,
                     (int)(short)pt, (int)(short)(pt >> 16));
            DispatchMouseDownScriptCall((uint)pt);
            LeaveSceneRenderLock(pChannel);
        }
        return;

    case 1:
        /* the four strips around the viewport, one working rect mutated between fills:
         * {s0,s1,v0,s3} {v0,s1,s2,v1} {v0,v3,s2,s3} {v2,v1,s2,v3} */
        viewRect = *(SceneRect8 *)&gSceneViewport;
        OffsetSceneRect(viewRect.s, -gSceneOriginX, -gSceneOriginY);
        GetSceneRect((int *)(pChannel + 0x12), sceneRect.s);
        rect = sceneRect;
        rect.s[2] = viewRect.s[0];
        QdPaintRect(rect.s);
        rect = sceneRect;
        rect.s[0] = viewRect.s[0];
        rect.s[3] = viewRect.s[1];
        QdPaintRect(rect.s);
        rect.s[1] = viewRect.s[3];
        rect.s[3] = sceneRect.s[3];
        QdPaintRect(rect.s);
        rect.s[0] = viewRect.s[2];
        rect.s[1] = viewRect.s[1];
        rect.s[3] = viewRect.s[3];
        QdPaintRect(rect.s);
        BumpSceneOverlayModeFloor(2);
        return;

    case 2:
        if (gMethodFrameLatched == 0)
        {
            EnterSceneRenderLock(pChannel);
            DispatchKeyDownScriptCall((short)(int)pParam);
            LeaveSceneRenderLock(pChannel);
        }
        return;

    case 3:
        if (pParam != NULL)
            ReloadCLUT(gActiveClut);
        return;

    case 4:
        DestroyChannelViewport((int)&gChannelB);
        gChannels[1].hResource = 0;
        return;

    case 5:
        if (gSceneChannelRefCount == 0)
        {
            QdSetOrigin(pChannel, gSceneOriginX, gSceneOriginY);
            GetSceneRect((int *)(pChannel + 0x12), rect.s);
            QdClipRect(rect.s);
        }
        gSceneChannelRefCount++;
        return;

    case 6:
        if (gSceneChannelRefCount == 0)
            FatalError(0, 0x10cf);
        gSceneChannelRefCount--;
        if (gSceneChannelRefCount == 0)
        {
            QdSetOrigin(pChannel, 0, 0);
            QdClipRect((short *)&gScrollRect);
        }
        return;

    case 7:
        rect = *(SceneRect8 *)pParam;
        ResizeChannelWindow(rect.s, pChannel);
        return;

    case 8:
        EnterSceneRenderLock(pChannel);
        ReshowSetViewIfIdle();
        LeaveSceneRenderLock(pChannel);
        return;

    case 0xb:   /* written before case 9: "menustate()" is then named before "boot()" and
                 * "idle()", which puts the literals in TI.EXE's order; same code */
        if (gMethodFrameLatched == 0)
        {
            EnterSceneRenderLock(pChannel);
            DispatchBootMessageString((byte *)UpdatePascalLength(&kStrMenustateCall));
            LeaveSceneRenderLock(pChannel);
        }
        return;

    case 9:
        ShowScriptErrorContinueDialog();
        ReapMarkedMethodCacheEntries();
        ResetScriptStringSlots();
        ResetLoopStacks();
        EnterSceneRenderLock(pChannel);
        /* gBootScriptRan is a word in the exe (`cmp word ptr` / `mov word ptr ..,1`) */
        if (*(short *)&gBootScriptRan == 0)
        {
            /* -10000..10000: paints the whole port before boot() */
            sceneRect.s[0] = (short)0xd8f0;
            sceneRect.s[1] = (short)0xd8f0;
            sceneRect.s[2] = (short)0x2710;
            sceneRect.s[3] = (short)0x2710;
            QdSetPort((int *)(pChannel + 0x12));
            QdPenNormal();
            QdPaintRect(sceneRect.s);
            DF_TRACE("probe2 before boot(): chanB+0x12=%p",
                     *(void **)((char *)&gChannelB + 0x12));
            DispatchBootMessageString((byte *)UpdatePascalLength(&kStrBootCall));
            *(short *)&gBootScriptRan = 1;
        }
        if (gMethodFrameLatched == 0)
            DispatchBootMessageString((byte *)UpdatePascalLength(&kStrIdleCall));
        LeaveSceneRenderLock(pChannel);
        return;

    case 0xc:
        if (gMethodFrameLatched == 0)
            DispatchMenuSelectScriptCall(pChannel, pParam);
        return;

    case 0xe:
    {
        /* A 20-byte reply {1, 10, 1, 1, 10, 1, scene rect}, copied out whole (`rep movsd`,
         * ecx = 5). The window rect goes into the working rect GetSceneRect then overwrites:
         * the exe has one 8-byte slot for both. */
        SceneInfo20 info;
        GetChannelWindowRect(pChannel, rect.s);
        info.w[0] = 1;
        info.w[1] = 10;
        info.w[2] = 1;
        info.w[3] = 1;
        info.w[4] = 10;
        info.w[5] = 1;
        GetSceneRect((int *)(pChannel + 0x12), rect.s);
        info.r = rect;
        *(SceneInfo20 *)pParam = info;
        return;
    }

    case 0xf:
        if (gMethodFrameLatched == 0)
        {
            EnterSceneRenderLock(pChannel);
            DispatchKeyRepeatScriptCall((short)(int)pParam);
            LeaveSceneRenderLock(pChannel);
        }
        return;

    case 0x10:
        gSceneOriginX = ((short *)&gScrollRect)[1] +
                       (short)(((int)((short *)&gScrollRect)[1]
                              + ((int)gSceneW - (int)((short *)&(*(int *)&gScrollSceneRect.y2))[1]))
                              / 2);
        gSceneOriginY = gScrollSceneRect.y1 +
                       (short)(((int)gScrollSceneRect.y1
                              + ((int)gSceneH - (int)(short)(*(int *)&gScrollSceneRect.y2)))
                              / 2);
        DispatchChannelMessage((short *)&gChannelB, 7, (byte *)&gScrollRect);
        return;

    default:
        return;
    }
}



/* Runs the boot-script call menuselect("<pParam>") inside the render lock; pParam is the menu
 * item's Pascal name. */
void __cdecl DispatchMenuSelectScriptCall(int pChannel, byte *pParam)  /* 0x0042a0c0 */
{
    byte *pbVar1;
    byte *pCallText;
    byte  callText[256];  /* local_100 */

    EnterSceneRenderLock(pChannel);
    pCallText = callText;
    CopyPascalString((byte *)UpdatePascalLength(&kStrMenuSelectOpenQuote), pCallText);
    AppendPascalString(pParam, callText);
    pCallText = callText;
    AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteRParen), pCallText);
    DispatchBootMessageString(callText);
    LeaveSceneRenderLock(pChannel);
}


/* Runs the boot-script call keydown("<key>"): the key's character, or uparrow / downarrow /
 * leftarrow / rightarrow for 0x1e / 0x1f / 0x1c / 0x1d.
 *
 * Port note: kStrQuoteRParen is the string buffer itself; passed without & it was read as a
 * NULL pointer (a live SIGSEGV in UpdatePascalLength). */
void __cdecl DispatchKeyDownScriptCall(short param)  /* 0x0042a140 */
{
    byte suffixed[256];   /* the exe's frame is two 256-byte buffers (sub esp,0x200) */
    byte work[256];

    CopyPascalString((byte *)UpdatePascalLength((char *)kStrKeyDownOpenQuote), work);

    /* the one-character default is written first, then four independent
     * tests overwrite it (the exe tests every value, no else chain) */
    suffixed[0] = 1;
    suffixed[1] = (byte)param;
    if (param == 0x1e)
        CopyPascalString((byte *)UpdatePascalLength((char *)kStrUparrow), suffixed);
    if (param == 0x1f)
        CopyPascalString((byte *)UpdatePascalLength((char *)kStrDownarrow), suffixed);
    if (param == 0x1c)
        CopyPascalString((byte *)UpdatePascalLength((char *)kStrLeftarrow), suffixed);
    if (param == 0x1d)
        CopyPascalString((byte *)UpdatePascalLength((char *)kStrRightarrow), suffixed);
    AppendPascalString(suffixed, work);
    AppendPascalString((byte *)UpdatePascalLength((char *)&kStrQuoteRParen), work);
    DispatchBootMessageString(work);
}


/* Runs the boot-script call keyrepeat("<key>"), the key named as DispatchKeyDownScriptCall does.
 *
 * Port note: the suffix is one 256-byte buffer below callText (the exe's frame is 0x200); as
 * two 1-byte locals the arrow names overran it on every held arrow key (stack adjacency). */
void __cdecl DispatchKeyRepeatScriptCall(short keyCode)  /* 0x0042a250 */
{
    byte  *pbVar1;
    byte  *pbVar2;
    byte   suffix[256];
    byte   callText[256];  /* local_100 */

    CopyPascalString((byte *)UpdatePascalLength(&kStrKeyRepeatOpenQuote), callText);
    suffix[1] = (byte)keyCode;
    suffix[0] = 1;
    if (keyCode == 0x1e)
        CopyPascalString((byte *)UpdatePascalLength(&kStrUparrow), suffix);
    if (keyCode == 0x1f)
        CopyPascalString((byte *)UpdatePascalLength(&kStrDownarrow), suffix);
    if (keyCode == 0x1c)
        CopyPascalString((byte *)UpdatePascalLength(&kStrLeftarrow), suffix);
    if (keyCode == 0x1d)
        CopyPascalString((byte *)UpdatePascalLength(&kStrRightarrow), suffix);
    AppendPascalString(suffix, callText);
    AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteRParen), callText);
    DispatchBootMessageString(callText);
}


/* Runs the boot-script call mousedown(<packedXY>), the packed point as an unsigned decimal. */
void __cdecl DispatchMouseDownScriptCall(uint packedXY)  /* 0x0042a360 */
{
    byte scratch[256];
    byte decAsc[256];

    CopyPascalString((byte *)UpdatePascalLength((char *)kStrMouseDownOpen), scratch);
    FormatUintAsPascalDecimalString(packedXY, (char *)decAsc);
    AppendPascalString(decAsc, scratch);
    AppendPascalString((byte *)UpdatePascalLength((char *)kStrRParen), scratch);
    DispatchBootMessageString(scratch);
}


/* Script statement bootscript (opcode 12031): takes no arguments and only empties result().
 * ClearScriptResult ignores the boot node, selector 1 and name it is passed; the post and server
 * twins below differ only in those. */
short __cdecl ActionCmd_BootScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0042a3e0 */
{
    *pSubSkip = 0;
    ClearScriptResult(gBootNode, 1, (byte *)UpdatePascalLength(kStrBootScript), 0);
    return 0;
}


/* Script statement postscript (opcode 12089): as bootscript (selector 2). */
short __cdecl ActionCmd_PostScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0042a410 */
{
    *pSubSkip = 0;
    ClearScriptResult(gBootNode, 2, (byte *)UpdatePascalLength((char *)&kStrPostScript), 0);
    return 0;
}


/* Script statement serverscript (opcode 12091): as bootscript (selector 3). */
short __cdecl ActionCmd_ServerScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0042a440 */
{
    *pSubSkip = 0;
    ClearScriptResult(gBootNode, 3, (byte *)UpdatePascalLength((char *)&kStrServerScript), 0);
    return 0;
}


/* Runs one line of script text, pName (Pascal), as a "Boot Message" sent to the boot script:
 * how the scene channel delivers boot(), idle(), keydown(...) and the rest. The text is compiled
 * by WaitHeapThenResolveMethodPath into a temporary handle and handed to ActionCmd_SendToBoot in
 * a MethodCallFrame {isLast 1, saved0x14c -1, names "Boot Message" / "System: "}. Returns the
 * compile status, else the call's.
 * MethodCallFrame is one 78-byte stack object (EvalMethodCall reads pData at +0x14 and writes
 * status and posIndex at +0x18 / +0x0c through it); every frame in this file is one.
 *
 * Port note: as loose locals the frame's fields landed in unrelated stack slots (stack
 * adjacency). The zeroing under DF_FIX_ZERO_INTERACTION_FRAME is port-added; the exe leaves
 * the fields it does not set as stack garbage. */
short __cdecl DispatchBootMessageString(byte *pName)  /* 0x0042a470 */
{
    short     result;
    short    *pData;
    int      *hEntry;
    int       subSkip;  /* local_58 */
    int       entryIdx;
    /* the zeroing is port-added (the exe leaves unset fields as stack garbage) */
#if DF_FIX_ZERO_INTERACTION_FRAME
    MethodCallFrame frame = {0};
#else
    MethodCallFrame frame;
#endif

    DF_TRACE(">> DispatchBootMessageString('%.*s')", (int)*pName, (char *)(pName + 1));
    result = WaitHeapThenResolveMethodPath(pName + 1, (uint)*pName, &hEntry, &entryIdx);
    if (result != 0) {   /* the exe returns the lookup's status straight from ax */
        DF_TRACE("!! DispatchBootMessageString('%.*s'): method path unresolved, err=%d",
                 (int)*pName, (char *)(pName + 1), (int)result);
        return result;
    }
    {
        pData = (short *)GetResDataPtr(hEntry);
        frame.pData      = pData;    /* stored before the two header fields */
        frame.isLast     = 1;
        frame.saved0x14c = 0xffffffffu;
        CopyPascalString((byte *)UpdatePascalLength(kStrBootMessage), frame.buf0);
        CopyPascalString((byte *)UpdatePascalLength(kStrBootMessage), frame.buf2);
        CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), frame.buf1);
        DF_TRACE_ENTER();
        result = ActionCmd_SendToBoot((int *)&frame, pData, (int *)0, &subSkip);
        DF_TRACE_LEAVE();
        DF_TRACE("<< DispatchBootMessageString('%.*s') -> %d",
                 (int)*pName, (char *)(pName + 1), (int)result);
        UnlockResObject(hEntry);
        FreeResObject(hEntry);
    }
    return result;
}


/* Script statement sendtoboot (opcode 12081): ScriptCmd_SendToBootFx with no result slot. */
short __cdecl
ActionCmd_SendToBoot(int *pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0042a550 */
{
    return ScriptCmd_SendToBootFx(pCtx, pArgBlock, pTable, 0, pSubSkip);
}


/* Script query sendtobootfx (opcode 20095): evaluates the method call at pArgBlock against the
 * boot node's two method scopes, selector 1 (Boot Script) and then selector 2 (Post Script).
 * *pSubSkip gets the number of 8-byte argument slots consumed; returns EvalMethodCall's status.
 * The frame's saved0x14c is the owner's int index 0x14c (byte +0x530), read back through the
 * frame's owner field in every frame below.
 *
 * Port note: the two frames must be one array, EvalMethodCall steps from f[0] to f[1] by the
 * frame size; as two locals the second scope was never searched (stack adjacency). */
short __cdecl
ScriptCmd_SendToBootFx(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSubSkip)  /* 0x0042a570 */
{
    int             start;          /* the parameter walks; this keeps its start */
    short           status;
    int             skipCount;  /* local_a0 */
    MethodCallFrame f[2];      /* contiguous: EvalMethodCall steps from f[0] to f[1] */
    int            *h1, *h2;
    MethodCallFrame *pf;

    start = (int)pArgBlock;

    /* Each frame is filled through a pointer: that keeps the owner reload after the handle
     * store and the isLast store ahead of the stack pop, as in the exe. The owner is passed
     * from the frame field, not the global. */
    pf = &f[0];
    pf->pOwner   = (int *)gBootNode;
    pf->selector = 1u;
    pf->hMethod  = (int *)AcquireMethodHandle((int *)pf->pOwner, 1u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData    = (void *)GetResDataPtr(pf->hMethod);
    CopyPascalString((byte *)UpdatePascalLength(kStrBootScript), pf->buf0);
    CopyPascalString((byte *)UpdatePascalLength(kStrBootScript), pf->buf2);
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), pf->buf1);
    pf->isLast = 0;

    pf = &f[1];
    pf->pOwner   = (int *)gBootNode;
    pf->selector = 2u;
    pf->hMethod  = h2 = (int *)AcquireMethodHandle((int *)pf->pOwner, 2u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData    = (void *)GetResDataPtr(h2);
    CopyPascalString((byte *)UpdatePascalLength(kStrPostScript), pf->buf0);
    CopyPascalString((byte *)UpdatePascalLength(kStrBootScript), pf->buf2);
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), pf->buf1);
    pf->isLast = 1;

    status = EvalMethodCall((short *)&f[0], (short *)pCtx, pArgBlock, pTable, pResult, &skipCount);

    UnlockResObject(f[0].hMethod);
    ReleaseMethodHandle((int)f[0].hMethod);
    UnlockResObject(f[1].hMethod);
    ReleaseMethodHandle((int)f[1].hMethod);
    pArgBlock += skipCount * 4;
    *pSubSkip = ((int)pArgBlock - start) >> 3;
    return status;
}


/* Script statement sendtopost (opcode 12090): ScriptCmd_SendToPostFx with a literal NULL result
 * slot; the same thunk as ActionCmd_SendToSet. */
short __cdecl ActionCmd_SendToPost(int pCtx, short *pArgBlock, int *pTable, int *pSkipOut)  /* 0x0042a740 */
{
    return (uint)(unsigned short)ScriptCmd_SendToPostFx((void *)(intptr_t)pCtx, pArgBlock,
                                                        pTable, NULL, pSkipOut);
}


/* Script query sendtopostfx (opcode 20021): as ScriptCmd_SendToBootFx with the single Post
 * Script scope (selector 2). */
short __cdecl ScriptCmd_SendToPostFx(void *pCtxB, short *pInstr, int *pTable,
                                     void *pResult, int *pSkipOut)  /* 0x0042a760 */
{
    int            start;               /* the parameter walks; this keeps its start */
    short          status;
    int            skipCount;  /* local_54 */
    MethodCallFrame f;
    MethodCallFrame *pf = &f;  /* filled through a pointer, as ScriptCmd_SendToBootFx */
    int           *h1;

    start        = (int)pInstr;
    pf->pOwner   = (int *)gBootNode;
    pf->selector = 2u;
    pf->hMethod  = h1 = AcquireMethodHandle((int *)pf->pOwner, 2u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData    = (void *)GetResDataPtr(h1);

    CopyPascalString((byte *)UpdatePascalLength(&kStrPostScript), f.buf0);
    CopyPascalString((byte *)UpdatePascalLength(&kStrPostScript), f.buf2);
    CopyPascalString((byte *)UpdatePascalLength(&kStrSystemColon), f.buf1);
    f.isLast = 1;

    status = EvalMethodCall((short *)&f, pCtxB, pInstr, pTable, pResult, &skipCount);

    UnlockResObject(f.hMethod);
    ReleaseMethodHandle((int)f.hMethod);

    pInstr += skipCount * 4;
    *pSkipOut = ((int)pInstr - start) >> 3;
    return status;
}


/* As DispatchBootMessageString for the server script: the frame is named "Server Message" and
 * runs through ActionCmd_SendToServer. */
short __cdecl DispatchServerMessageString(byte *pStr)  /* 0x0042a860 */
{
    short   st;
    void   *pData;
    int    *ph;
    int     skipOut;   /* the exe does not initialise it; the callee writes it */
    int     posErr;  /* local_54 */
    MethodCallFrame frame;

    st = WaitHeapThenResolveMethodPath(pStr + 1, (uint)*pStr, &ph, &posErr);
    if (st != 0)
        return st;

    pData            = (void *)GetResDataPtr(ph);
    frame.pData      = pData;
    frame.isLast     = 1;
    frame.saved0x14c = 0xffffffffu;

    CopyPascalString((byte *)UpdatePascalLength(&kStrServerMessage), frame.buf0);
    CopyPascalString((byte *)UpdatePascalLength(&kStrServerMessage), frame.buf2);
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), frame.buf1);

    st = ActionCmd_SendToServer((int)(intptr_t)&frame,
                                (short *)pData, NULL, &skipOut);
    UnlockResObject(ph);
    FreeResObject(ph);
    return st;
}


/* Script statement sendtoserver (opcode 12092): ScriptCmd_SendToServerFx with a literal NULL
 * result slot; the same thunk as ActionCmd_SendToSet. */
short __cdecl ActionCmd_SendToServer(int pCtx, short *pArgBlock, int *pTable,
                                    int *pSkipOut)  /* 0x0042A940 */
{
    return (uint)(unsigned short)ScriptCmd_SendToServerFx((void *)(intptr_t)pCtx, pArgBlock,
                                                          pTable, NULL, pSkipOut);
}


/* Script query sendtoserverfx (opcode 20100): as ScriptCmd_SendToBootFx with the scopes
 * selector 3 (Server Script) and then selector 2 (Post Script). */
short __cdecl ScriptCmd_SendToServerFx(void *pCtxB, short *pInstr, int *pTable,
                                       void *pResult, int *pSkipOut)  /* 0x0042a960 */
{
    int            start;               /* the parameter walks; this keeps its start */
    short          status;
    int            skipCount;  /* local_a0 */
    MethodCallFrame f[2];
    int           *h1, *h2;
    MethodCallFrame *pf;   /* frames filled through a pointer, as ScriptCmd_SendToBootFx */

    start = (int)pInstr;

    pf = &f[0];
    pf->pOwner     = (int *)gBootNode;
    pf->selector   = 3u;
    pf->hMethod    = h1 = AcquireMethodHandle((int *)pf->pOwner, 3u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h1);
    CopyPascalString((byte *)UpdatePascalLength(&kStrServerScript), pf->buf0);
    CopyPascalString((byte *)UpdatePascalLength(&kStrServerScript), pf->buf2);
    CopyPascalString((byte *)UpdatePascalLength(&kStrSystemColon), pf->buf1);
    pf->isLast = 0;

    pf = &f[1];
    pf->pOwner     = (int *)gBootNode;
    pf->selector   = 2u;
    pf->hMethod    = h2 = AcquireMethodHandle((int *)pf->pOwner, 2u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h2);
    CopyPascalString((byte *)UpdatePascalLength(&kStrPostScript), pf->buf0);
    CopyPascalString((byte *)UpdatePascalLength(&kStrServerScript), pf->buf2);
    CopyPascalString((byte *)UpdatePascalLength(&kStrSystemColon), pf->buf1);
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


/* EvalMethodCall's failure hook: if the frame's status is positive and no frame is latched yet,
 * copies the whole frame to gLatchedMethodFrame and sets gMethodFrameLatched. That mutes the
 * scene channel's script messages until ShowScriptErrorContinueDialog reports the error.
 *
 * Port note: the status is the short at +0x18; the port read byte +6 (an int-pointer offset
 * taken as bytes), so failures were latched only by accident. */
void __cdecl LatchFailedMethodFrame(void *pCtx)  /* 0x0042ab20 */
{
    if (((MethodCallFrame *)pCtx)->status > 0 && gMethodFrameLatched == 0) {
        *(MethodCallFrame *)gLatchedMethodFrame = *(MethodCallFrame *)pCtx;   /* rep movsd 0x13 + movsw */
        gMethodFrameLatched = 1;
    }
}



/* Reports a latched script failure (scene channel message 9): clears the latch and asks, in a
 * yes/no box, string 1160 ("A scripting error has occured. Choose "Yes" to try ignoring the
 * error, or "No" to quit. [") followed by string 1100 + status and "]". A result of 0 (No) sets
 * gGameLoopState = 1, which quits. */
void __stdcall ShowScriptErrorContinueDialog(void)  /* 0x0042ab60 */
{
    byte  msgText[256];  /* local_100 */

    if (gMethodFrameLatched != 0) {
        gMethodFrameLatched = 0;
        LoadStringResourceAsPascal(0x44c, 0x3c, (char *)msgText);   /* 0x44c + n = string id 1100 + n */
        /* the latched frame's 16-bit status word (mov ax,[0x0048a0f8]) */
        LoadStringResourceAsPascal(0x44c, ((MethodCallFrame *)gLatchedMethodFrame)->status, &gScratchStr);
        AppendPascalString(&gScratchStr, msgText);
        AppendPascalString((byte *)UpdatePascalLength(&kStrRBracket), msgText);
        if (RunDescriptorYesNoDialog(msgText) == 0) {
            gGameLoopState = 1;
        }
    }
}


/* Sets up the script runtime at session start (BeginGameSession): the method cache (10 entries
 * of 0x20 bytes, growing by 10; layout at AcquireMethodHandle), the node-destroy hook that
 * retires a destroyed node's cached methods, the nine search-path slots all set to the module's
 * boot directory, and the 0x64000-byte (400 KB) script heap. Leaves the primary heap active. */
void __stdcall InitScriptRuntime(void)  /* 0x0042ac00 */
{
    short i;

    gMethodCacheCapacity = 10;
    gMethodCacheCount = 0;
    gMethodCache = AllocResObject(0x140);                /* 10 x 0x20 */
    SetNodeDestroyHook((int)InvalidateCachedMethodsForNode);
    ComposeModuleDirBootPath((char *)&gScratchStr);
    gSearchPathTable = AllocEngineBuffer(0x900);         /* 9 x 256 */

    for (i = 0; i <= 8; i++)            /* cmp 8 / jle, as the exe loops */
    {
        SetSearchPathSlot(i, &gScratchStr);
    }

    AllocSecondaryHeap((int *)&gScriptHeap, 0x64000);
    RestorePrimaryHeap();
}


/* Undoes InitScriptRuntime at session end (EndGameSession): frees every cached method's bytecode
 * (fatal 0x10d0 if one is still referenced), the script heap, the cache and the search-path table. */
void __stdcall ShutdownScriptRuntime(void)  /* 0x0042ac80 */
{
    int pData;
    int iSlot;

    pData = GetResDataPtr(gMethodCache);
    iSlot = 0;

    if (gMethodCacheCount > 0)
    {
        do {
            if (*(short *)(pData + 0x10) != 0)
                FatalError(0, 0x10d0);

            FreeResObject(*(void **)(pData + 0x0c));
            pData += 0x20;
            iSlot++;
        } while (iSlot < gMethodCacheCount);
    }

    FreeSecondaryHeap((int *)&gScriptHeap);
    UnlockResObject(gMethodCache);
    FreeResObject(gMethodCache);
    FreeEngineBuffer(gSearchPathTable);
    gSearchPathTable = 0;
}


/* Compiles script text (pData, len) into a bytecode handle on the script heap with
 * TokenizeScriptSource, first evicting least-recently-used cached methods until the largest free
 * block is at least 20000 bytes. *pPosErr is the tokenizer's position out-parameter; returns its
 * status. */
short __cdecl WaitHeapThenResolveMethodPath(byte *pData, uint len, int **ppHandleOut, int *pPosErr)  /* 0x0042ad10 */
{
    int     heap;
    short   r;                          /* mov si,ax: the status is a short */

    SetActiveHeap(&gScriptHeap);
    heap = GetLargestFreeBlockSize();
    while (heap < 20000) {
        EvictLRUMethodCacheEntry();
        heap = GetLargestFreeBlockSize();
    }
    r = TokenizeScriptSource(pData, (int)len, ppHandleOut, pPosErr);
    RestorePrimaryHeap();
    return r;
}


/* Per-frame method-cache housekeeping (scene channel message 9): clears gVisualEffectDepth,
 * evicts the first entry InvalidateCachedMethodsForNode marked, and evicts the least recently
 * used one while more than 0x40 (64) are cached. */
void __stdcall ReapMarkedMethodCacheEntries(void)  /* 0x0042ad70 */
{
    int   entryIdx;  /* local_4 */

    gVisualEffectDepth = 0;
    if ((short)FindInvalidatedMethodCacheEntry(&entryIdx) != 0) {
        EvictMethodCacheEntry(entryIdx);
    }
    if (0x40 < gMethodCacheCount) {
        EvictLRUMethodCacheEntry();
    }
}


/* Returns a counted handle to the compiled bytecode of method (pCtx, selector). A cached method
 * gets its count and LRU stamp bumped; otherwise the bytecode is attached from the node, copied
 * into a new buffer on the script heap (evicting until there is 10000 bytes to spare), detached
 * again and entered in the cache (fatal 0x10d1 / 0x10d2 if the attach or detach fails).
 * Method cache entry (gMethodCache, 0x20 bytes): +0x00 owner node, +0x04 selector, +0x08 LRU
 * stamp, +0x0c bytecode handle, +0x10 reference count (short), +0x12 retired flag (short, set by
 * InvalidateCachedMethodsForNode), +0x14..+0x1f unused. */
int * __cdecl AcquireMethodHandle(int *pCtx, unsigned int selector)  /* 0x0042adb0 */
{
    /* The exe builds the new 32-byte entry on the stack and copies it into
     * the table whole (rep movsd, ecx = 8); +0x14..+0x1f are never set. */
    typedef struct { int *ctx; unsigned int sel; int ts; int *h; short ref; short pad; int rest[3]; } CacheEnt;
    short        err;
    int          cacheIdx;
    int         *hData;
    unsigned int dataSize;
    int         *hBuf;
    CacheEnt     ent;
    CacheEnt    *pe;
    void        *src;
    void        *dst;

    if ((short)FindMethodCacheEntryByKey((int)pCtx, selector, &cacheIdx) != 0) {
        pe = (CacheEnt *)(GetResDataPtr(gMethodCache) + cacheIdx * 0x20);
        pe->ref++;
        pe->ts = gMethodCacheClock;
        gMethodCacheClock++;
        hData = pe->h;
        UnlockResObject(gMethodCache);
        return hData;
    }

    err = (short)AttachRenderChild(pCtx, selector, &hData);
    if (err != 0)
        FatalError(err, 0x10d1);

    dataSize = GetResObjectSize(hData);
    SetActiveHeap(&gScriptHeap);
    while ((int)GetLargestFreeBlockSize() < (int)(dataSize + 10000))
        EvictLRUMethodCacheEntry();
    ent.ctx = pCtx;
    ent.sel = selector;
    ent.ts  = gMethodCacheClock;
    ent.ref = 1;
    gMethodCacheClock++;
    ent.pad = 0;
    hBuf = AllocResObject(dataSize);
    RestorePrimaryHeap();

    src = (void *)GetResDataPtr(hData);
    dst = (void *)GetResDataPtr(hBuf);
    _memcpy(dst, src, dataSize);         /* the exe's own CRT copy (0x0044e440) */
    UnlockResObject(hData);
    UnlockResObject(hBuf);

    err = (short)DetachRenderChild((int)pCtx, selector);
    if (err != 0)
        FatalError(err, 0x10d2);

    gMethodCacheCount++;
    if (gMethodCacheCount > gMethodCacheCapacity) {
        gMethodCacheCapacity += 10;
        ResizeResObject(gMethodCache, (unsigned int)(gMethodCacheCapacity * 0x20));
    }
    ent.h = hBuf;
    ((CacheEnt *)GetResDataPtr(gMethodCache))[gMethodCacheCount - 1] = ent;
    UnlockResObject(gMethodCache);
    return hBuf;
}


/* Drops one reference to a cached method's bytecode handle; the entry stays cached at count 0
 * until evicted. FatalError(0x6e, 0x10d4) if the handle is not cached, (0x6e, 0x10d3) if the
 * count goes negative. */
void __cdecl ReleaseMethodHandle(int hHandle) /* 0x0042AF80 */
{
    int   iEntry;             /* declaration order decides which register holds what */
    short refcount;
    int   cacheIdx;

    if ((short)FindMethodCacheEntry(hHandle, &cacheIdx) != 0) {
        iEntry = GetResDataPtr(gMethodCache) + cacheIdx * 0x20;
        refcount = *(short *)(iEntry + 0x10) - 1;
        *(short *)(iEntry + 0x10) = refcount;
        if (refcount < 0)
            FatalError(0x6e, 0x10d3);
        UnlockResObject(gMethodCache);
        return;
    }
    FatalError(0x6e, 0x10d4);
}

/* Empties result(): copies the empty Pascal string kStrEmptyPascal to gScriptResult, the slot
 * ScriptCmd_Result reads. Every caller pushes four arguments (owner, kind, a name, 0); none is read. */
void __cdecl ClearScriptResult(int owner, int kind, byte *pName, int flags)   /* 0x0042aff0 */
{
    (void)owner; (void)kind; (void)pName; (void)flags;
    CopyPascalString((byte *)UpdatePascalLength(kStrEmptyPascal), gScriptResult);
}


/* Evicts the unreferenced method-cache entry with the oldest LRU stamp; fatal 0x10d8 if every
 * entry is still referenced.
 * History: was EvictLRUAnimDescriptorSlot; the table is the method cache, which the animation
 * code only allocated during init. */
void __stdcall EvictLRUMethodCacheEntry(void)  /* 0x0042B010 */
{
  uint32_t oldestIdx;  /* local_4 */
  int pEntry;
  int oldestStamp;
  int i;

  oldestStamp = 0x7fffffff;   /* INT_MAX: no candidate yet */
  pEntry = GetResDataPtr(gMethodCache);
  i = 0;
  if (0 < gMethodCacheCount) {
    do {
      if ((*(short *)(pEntry + 0x10) == 0) && (*(int *)(pEntry + 8) < oldestStamp)) {
        oldestStamp = *(int *)(pEntry + 8);
        oldestIdx = (uint32_t)i;
      }
      pEntry = pEntry + 0x20;
      i = i + 1;
    } while (i < gMethodCacheCount);
  }
  UnlockResObject(gMethodCache);
  if (oldestStamp == 0x7fffffff) {
    FatalError(0, 0x10d8);
  }
  EvictMethodCacheEntry((int)oldestIdx);
  return;
}


/* Finds the method-cache entry for (owner keyA, selector keyB); returns 1 with its index in
 * *pIdxOut, else 0. */
short __cdecl FindMethodCacheEntryByKey(int keyA, int keyB, int *pIdxOut)  /* 0x0042B090 */
{
  int *pEntry;
  int i;

  pEntry = (int *)GetResDataPtr(gMethodCache);
  i = 0;
  if (0 < gMethodCacheCount) {
    do {
      if ((pEntry[1] == keyB) && (*pEntry == keyA)) {
        UnlockResObject(gMethodCache);
        *pIdxOut = i;
        return 1;
      }
      pEntry = pEntry + 8;
      i = i + 1;
    } while (i < gMethodCacheCount);
  }
  UnlockResObject(gMethodCache);
  return 0;
}



/* Finds the method-cache entry holding bytecode handle key; returns 1 with its index in
 * *pIdxOut, else 0. */
short __cdecl FindMethodCacheEntry(int key, int *pIdxOut)  /* 0x0042B100 */
{
  int pEntry;
  int i;

  pEntry = GetResDataPtr(gMethodCache);
  i = 0;
  if (0 < gMethodCacheCount) {
    do {
      if (*(int *)(pEntry + 0xc) == key) {
        UnlockResObject(gMethodCache);
        *pIdxOut = i;
        return 1;
      }
      pEntry = pEntry + 0x20;
      i = i + 1;
    } while (i < gMethodCacheCount);
  }
  UnlockResObject(gMethodCache);
  return 0;
}


/* Frees entry slotIdx's bytecode and moves the last entry into its place. */
void __cdecl EvictMethodCacheEntry(int slotIdx)  /* 0x0042B160 */
{
    typedef struct { uint32_t v[8]; } MethodCacheEntry32;   /* rep movsd, ecx = 8: a struct assignment */
    int iBase;

    gMethodCacheCount = gMethodCacheCount - 1;
    iBase = GetResDataPtr(gMethodCache);
    FreeResObject(*(int **)(slotIdx * 0x20 + 0xc + iBase));
    *(MethodCacheEntry32 *)(slotIdx * 0x20 + iBase) = *(MethodCacheEntry32 *)(iBase + gMethodCacheCount * 0x20);
    UnlockResObject(gMethodCache);
}


/* Finds the first method-cache entry InvalidateCachedMethodsForNode retired (+0x12 set);
 * returns 1 with its index in *pOut, else 0. Fatal 0x10d9 if it passes an entry that is still
 * referenced (+0x10). */
short __cdecl FindInvalidatedMethodCacheEntry(int *pOut) /* 0x0042B1C0 */
{
    int  iSlot;
    int  iBase;

    iBase = (int)GetResDataPtr(gMethodCache);
    iSlot = 0;
    if (gMethodCacheCount > 0) {
        do {
            if (*(short *)(iBase + 0x10) != 0)
                FatalError(0, 0x10d9);
            if (*(short *)(iBase + 0x12) != 0) {
                UnlockResObject(gMethodCache);
                *pOut = iSlot;
                return 1;
            }
            iBase += 0x20;
            iSlot++;
        } while (iSlot < gMethodCacheCount);
    }
    UnlockResObject(gMethodCache);
    return 0;
}


/* DestroyRenderNode's pre-destroy hook, installed by InitScriptRuntime through SetNodeDestroyHook:
 * retires every method-cache entry owned by pNode. It sets the retired flag (+0x12) and the
 * selector to -1, which is never a real selector, so FindMethodCacheEntryByKey can never hand
 * the entry out again. Returns 0.
 * History: was MarkAnimationDescriptorActive(int selector), wrong on the argument, the table and
 * the effect; the port called it with no argument, so it never did anything (class 14). */
short __cdecl InvalidateCachedMethodsForNode(void *pNode) /* 0x0042B240 */
{
    int   *row;
    int    i;

    row = (int *)GetResDataPtr(gMethodCache);
    for (i = 0; i < gMethodCacheCount; i++) {
        if (*row == (int)(intptr_t)pNode) {
            *(short *)((char *)row + 0x12) = 1;
            row[1] = -1;
        }
        row += 8;
    }
    UnlockResObject(gMethodCache);
    return 0;
}


/* Script statement reboot (opcode 12106): sets gGameLoopState = 2, so RunGameLoop ends the
 * session and starts a new one. No arguments. */
short __cdecl
ActionCmd_Reboot(int pCtx, int pArgBlock, int *pTable, int *pSubSkip) /* 0x0042B2A0 */
{
    (void)pCtx;
    (void)pArgBlock;
    (void)pTable;
    *pSubSkip = 0;
    gGameLoopState = 2;
    return 0U;
}


/* Script statement currentcd "name" (opcode 16034): switches to the named CD volume after
 * ValidateCurrentCdName accepts it; returns its error otherwise. */
short __cdecl
ActionCmd_CurrentCd(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip) /* 0x0042B2C0 */
{
    short  u;
    short res[4];
    byte  descBuf[256];

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)res, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(res, descBuf);
    if (u != 0)
        return u;
    u = ValidateCurrentCdName(descBuf);
    if (u != 0)
        return u;
    SetCurrentCdName(descBuf);
    return 0;
}


/* Checks a currentcd volume name. Empty or the current name passes; otherwise 1-11 letters and
 * digits (else 0x3b). Returns 0x31 if the current CD is the volume of search-path slot 0, or
 * while an open cast, shop, track, set, stage or puppet lives on the current CD (its path then
 * goes to gScriptResult): the CD cannot be changed under them. */
short __cdecl ValidateCurrentCdName(byte *pName) /* 0x0042b340 */
{
    byte  nameLen;
    int   i;
    short ok;
    int  *pEntry;

    nameLen = *pName;
    if (nameLen == 0)
        return 0;
    if (0x0b < nameLen)
        return 0x3b;
    for (i = 1; i <= nameLen; i++) {
        ok = 0;
        if (pName[i] >= 0x41 && pName[i] <= 0x5a)
            ok = 1;
        if (pName[i] >= 0x61 && pName[i] <= 0x7a)
            ok = 1;
        if (pName[i] >= 0x30 && pName[i] <= 0x39)
            ok = 1;
        if (ok == 0)
            return 0x3b;
    }
    if (PascalStringsEqualNoCase(&gCurrentCdName, pName) != 0)
        return 0;
    GetSearchPathSlot(0, &gScratchStr);
    TruncatePathToVolume(&gScratchStr);
    if (PascalStringsEqualNoCase(&gCurrentCdName, &gScratchStr) != 0)
        return 0x31;
    pEntry = (int *)GetResDataPtr(gCastTable);
    i = 0;
    if (0 < gCastCount) {
        do {
            if (IsSlotEntryOnCurrentVolume(*pEntry) != 0) {
                CopyPascalString((byte *)(pEntry + 3), gScriptResult);
                UnlockResObject(gCastTable);
                return 0x31;
            }
            pEntry += 7;
            i++;
        } while (i < gCastCount);
    }
    UnlockResObject(gCastTable);
    pEntry = (int *)GetResDataPtr(gShopTable);
    i = 0;
    if (0 < gShopCount) {
        do {
            if (IsSlotEntryOnCurrentVolume(*pEntry) != 0) {
                CopyPascalString((byte *)(pEntry + 3), gScriptResult);
                UnlockResObject(gShopTable);
                return 0x31;
            }
            pEntry += 7;
            i++;
        } while (i < gShopCount);
    }
    UnlockResObject(gShopTable);
    pEntry = (int *)GetResDataPtr(gTrackTable);
    i = 0;
    if (0 < gTrackCount) {
        do {
            if (IsSlotEntryOnCurrentVolume(*pEntry) != 0) {
                CopyPascalString((byte *)((char *)pEntry + 0x16), gScriptResult);
                UnlockResObject(gTrackTable);
                return 0x31;
            }
            pEntry += 10;
            i++;
        } while (i < gTrackCount);
    }
    UnlockResObject(gTrackTable);
    if (gSet.active != 0) {
        if (IsSlotEntryOnCurrentVolume((int)DAT_0048a780) != 0) {
            CopyPascalString(&DAT_0048a7b4, gScriptResult);
            return 0x31;
        }
    }
    if (gStage.active != 0) {
        if (IsSlotEntryOnCurrentVolume((int)DAT_0048a754) != 0) {
            CopyPascalString(&DAT_0048a768, gScriptResult);
            return 0x31;
        }
    }
    if (gPuppet.active != 0) {
        if (IsSlotEntryOnCurrentVolume((int)DAT_0048a7fc) != 0) {
            CopyPascalString(&DAT_0048a81c, gScriptResult);
            return 0x31;
        }
    }
    return 0;
}



/* Switches the current CD: destroys every open render node (link at +0x540, those with the word
 * at +0x54a set) whose path (+0x24) is on the old volume (fatal 0x10da), prompts until the new
 * volume is present (fatal 0x10db) and records it. If the prompt comes back with flag 0 (no
 * volume), the name is emptied and gGameLoopState = 1, which quits. */
void __cdecl SetCurrentCdName(byte *pCdName) /* 0x0042b610 */
{
    void  *head;
    short  flag;
    byte  *p;        /* the walk: a register copy of the out-param head */
    short  st;

    GetGlobalRenderNodeListHead(&head);
    for (p = (byte *)head; p != NULL; p = *(byte **)(p + 0x540)) {
        if (*(short *)(p + 0x54a) != 0) {
            CopyPascalString(p + 0x24, &gScratchStr);
            TruncatePathToVolume(&gScratchStr);
            if (PascalStringsEqualNoCase(&gScratchStr, &gCurrentCdName) != 0) {
                st = DestroyRenderNode(p);
                if (st != 0)
                    FatalError(st, 0x10da);
            }
        }
    }
    st = PromptUntilDescriptorVolumePresent(pCdName, &flag);
    if (st != 0)
        FatalError(st, 0x10db);
    CopyPascalString(pCdName, &gCurrentCdName);
    if (flag == 0) {
        gCurrentCdName[0] = 0;   /* clear the Pascal length byte = empty string */
        gGameLoopState = 1;
    }
}


/* Returns 1 when the node's path (+0x24) is on the volume gCurrentCdName names, compared without
 * case, else 0. */
short __cdecl IsSlotEntryOnCurrentVolume(int pSlotEntry) /* 0x0042b6f0 */
{
    CopyPascalString((byte *)(pSlotEntry + 0x24), (byte *)&gScratchStr);
    TruncatePathToVolume((byte *)&gScratchStr);
    return PascalStringsEqualNoCase((byte *)&gScratchStr, (byte *)gCurrentCdName) != 0;
}


/* Cuts a Mac-style Pascal path ("Volume:dir:file") at its first ':' in place, leaving the
 * volume name; a path with no ':' is left alone. */
void __cdecl TruncatePathToVolume(byte *pDesc) /* 0x0042b730 */
{
    short i;

    for (i = 1; i <= pDesc[0]; i++) {
        if (pDesc[i] == ':') {
            pDesc[0] = (byte)(i - 1);
            return;
        }
    }
}


/* Script query currentcd (opcode 16034): returns gCurrentCdName as a script string; no
 * arguments. Returns AllocScriptString's status. */
short ScriptCmd_CurrentCd(int *pCtx, short *pArgBlock, int *pTable,
                               short *pResult, int *pSkipCount) /* 0x0042B760 */
{
    *pSkipCount = 0;
    return AllocScriptString(&gCurrentCdName, pResult);
}


/* Reduces a Mac-style Pascal path ("Volume:dir:c73.set") to its leaf, the text after the last
 * ':', in place.
 *
 * Port note: it had been written as a call to TruncatePathToVolume, which keeps the volume
 * instead; loading a saved game then failed with fatal 42/0x1124 (class 14). */
void __cdecl TruncatePathToLeaf(byte *pBuf) /* 0x0042b780 */
{
    unsigned short i   = pBuf[0];           /* cx: the running index */
    byte           c   = pBuf[(short)i];    /* movsx eax,cx before the copy to bx (0x0042b78a) */
    unsigned short len = i;                 /* bx: the length kept for n */
    short          n;

    while (c != ':' && 0 < (short)i) {
        i--;
        c = pBuf[(short)i];
    }
    n = (short)(len - i);
    /* The original's memcpy overlaps (dst < src); memmove is the defined form. */
    memmove(pBuf + 1, pBuf + (short)(i + 1), (size_t)n);
    pBuf[0] = (byte)n;
}
