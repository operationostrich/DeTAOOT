/* stage.c -- TI.EXE 0x00444ca0..0x004471b0: the stage file (STG): opening and closing it,
 * its flat pages and their buttons, page navigation and the stage script commands. */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "df_protos.h"
#include "topics/scriptcmd.h"


#include "topics/savestate.h"

/* openstagefile (12060): opens the 'FLAT' stage file named by its one argument:
 * ValidateDescriptorPath, OpenStageNode, NavigateToStagePage(page 0 as DAT_0048a758) and
 * CompleteStageInit. Status 0x0b if a stage is already open. */
short __cdecl ActionCmd_OpenStageFile(int pCtx, short *pArgBlock,  /* 0x00444ca0 */
                                 int *pTable, int *pSubSkip)
{
    short  status;
    short argResult[4];

    if (gStage.active != 0)
        return 0x0b;   /* already active -- error 11 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                               (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, (short *)gScratchStr2);
    if (status != 0)
        return status;

    status = ValidateDescriptorPath(gScratchStr2, 0x464c4154)   /* 'FLAT' */;
    if (status != 0)
        return status;

    status = OpenStageNode(gScratchStr2, (short *)&gStage);
    if (status != 0)
        return status;

    NavigateToStagePage(DAT_0048a758);   /* set initial stage page position */
    status = CompleteStageInit();
    return status;
}


/* Opens the stage file pDesc into the guard record pHandle (gStage): takes its pending render node,
 * attaches child 0 (the header) and checks it. Returns 0x38 if the header tag is wrong and 0x39 if
 * its dimensions are not the scene canvas's (the node is released again either way). Otherwise sets
 * the flags, the current page 0 and previous page -1, the page selector from header word 11 and the
 * stage name from header +0x838. Attach and detach failures are fatal (0x1035..0x103c).
 *
 * Handle layout: +0 guard (gStage), +2/+4/+6 flags (DAT_0048a74e/50/52), +8 stage context
 * (DAT_0048a754), +12 current page (DAT_0048a758), +16 previous page (DAT_0048a75c), +20 header
 * child (DAT_0048a760), +24 page selector, +28 stage name. */
short __cdecl OpenStageNode(byte *pDesc, short *pHandle)  /* 0x00444d40 */
{
    int   *pStageCtx;
    int   *pChildHandle;
    short  status;
    int   *pHeader;
    short   detachStatus;
    short   detachStatus2;
    int    hPageChild;  /* local_4 */

    pStageCtx = (int *)(pHandle + 4);
    status  = TakeMatchingPendingRenderNode(pDesc, pStageCtx);
    if (status != 0)
        FatalError(status, 0x1035);

    pChildHandle = (int *)(pHandle + 10);
    status  = AttachRenderChild((int *)*pStageCtx, 0, (int **)pChildHandle);
    if (status != 0)
        FatalError(status, 0x1036);

    MoveResObjectToHeapTop((int *)*pChildHandle);
    pHeader = (int *)GetResDataPtr((void *)*pChildHandle);

    if (*pHeader != 0x40000) {
        UnlockResObject((void *)*pChildHandle);
        detachStatus = DetachRenderChild(*pStageCtx, 0);
        if (detachStatus != 0)
            FatalError(detachStatus, 0x1037);
        status = DeferRenderNodeClose(*pStageCtx);
        if (status != 0)
            FatalError(status, 0x1038);
        return 0x38;
    }

    if (((short)pHeader[10] == gSceneW) && (*(short *)((byte *)pHeader + 0x2a) == gSceneH)) {
        *(int *)(pHandle + 6)  = 0;
        *(int *)(pHandle + 8)  = 0xffffffff;
        pHandle[1] = 1;
        pHandle[0] = 1;
        pHandle[2] = 1;
        pHandle[3] = 1;
        *(int *)(pHandle + 12) = pHeader[11];
        CopyPascalString((byte *)(pHeader + 0x20e), (byte *)(pHandle + 14)); /* stage name */
        UnlockResObject((void *)*pChildHandle);
        status = AttachRenderChild((int *)*pStageCtx, *(uint *)(pHandle + 12), (int **)&hPageChild);
        if (status != 0)
            FatalError(status, 0x103b);
        detachStatus2 = DetachRenderChild(*pStageCtx, *(int *)(pHandle + 12));
        if (detachStatus2 != 0)
            FatalError(detachStatus2, 0x103c);
        return 0;
    }

    UnlockResObject((void *)*pChildHandle);
    detachStatus = DetachRenderChild(*pStageCtx, 0);
    if (detachStatus != 0)
        FatalError(detachStatus, 0x1039);
    status = DeferRenderNodeClose(*pStageCtx);
    if (status != 0)
        FatalError(status, 0x103a);
    return 0x39;
}


/* closestagefile (12061): closes the stage. Status 0x0b if none is open. Runs StagePreShutdownStep;
 * if that closed the stage itself, or the stage's scene changed meanwhile (the context's +0x530
 * word), it returns. Otherwise it closes the node (CloseStageNode), blanks the trigger surface
 * (CopyTrigSceneBufferAndViewportPair, ZeroBlock) and requests a whole-scene redraw
 * (BumpSceneOverlayModeFloor(2)). */
short __cdecl ActionCmd_CloseStageFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x00444f20 */
{
    int      sceneHandle;
    short    status;
    /* local_78..: ONE 120-byte buffer descriptor that CopyTrigSceneBufferAndViewportPair fills, not
     * separate locals : +0 width, +2 height, +4 pixels. */
    unsigned int trigBuf[30];   /* local_78 */
    int      dims[2];           /* local_80 -- viewport pair */


    *pSubSkip = 0;
    if (gStage.active == 0)
        return 0x0b;

    sceneHandle = *(int *)((char *)DAT_0048a754 + 0x530);

    status = StagePreShutdownStep();
    if (status != 0)
        return status;

    if (gStage.active == 0)
        return 0;   /* the stage closed itself during the pre-shutdown step */

    if (*(int *)((char *)DAT_0048a754 + 0x530) != sceneHandle)
        return 0;   /* the stage's scene changed */

    CloseStageNode((short *)&gStage);
    CopyTrigSceneBufferAndViewportPair(trigBuf, dims);
    ZeroBlock(*(void **)((char *)trigBuf + 4),
              (int)*(short *)((char *)trigBuf + 2) * (int)*(short *)trigBuf);
    BumpSceneOverlayModeFloor(2);
    return 0;
}


/* Shuts down the stage node pHandle if its guard (+0) is set: detaches child 0 of the stage context
 * (+8; fatal 0x103d), drains its display slots (DrainDisplayAttachSlotsForContext), defers the
 * render node's close (DeferRenderNodeClose; fatal 0x103e) and clears the guard. */
void __cdecl CloseStageNode(short *pHandle)  /* 0x00444fe0 */
{
    short deferStatus;
    short  detachStatus;

    if (*pHandle != 0) {
        detachStatus = DetachRenderChild(*(int *)(pHandle + 4), 0);
        if (detachStatus != 0)
            FatalError(detachStatus, 0x103d);
        DrainDisplayAttachSlotsForContext(*(int *)(pHandle + 4));
        deferStatus = DeferRenderNodeClose(*(int *)(pHandle + 4));
        if (deferStatus != 0)
            FatalError(deferStatus, 0x103e);
        *pHandle = 0;
    }
}


/* gotoflat (12062): goes to a flat page of the open stage, named by a string or given as a 1-based
 * int index (status 0x0b if no stage is open, 10 if the name is unknown or the index outside 1..max,
 * max being the header word +0x848). Already being on the page is a no-op. Otherwise it sends the
 * old page its closeflat message (SendCloseFlatMessage), moves to the page (NavigateToStagePage) and
 * sends the new page its openflat message (SendOpenFlatMessage); it stops quietly if the stage
 * closed or changed scene meanwhile. */
short __cdecl ActionCmd_GotoFlat(int pCtx, short *pArgBlock,  /* 0x00445040 */
                                     int *pTable, int *pSubSkip)
{
    short   status;
    int    savedSceneHandle;
    /* local_8 + local_6 are ONE 8-byte typed value filled by ProcessScriptBlock; local_6 is the value slot
     * throughout and holds the 1-based page index (class 3). */
    short  argResult[4];  /* [0] type (4 = int), [1..2] value */
    int    dataPtr;
    int    idx;
#define pageIdx (*(int *)(argResult + 1))

    if (gStage.active == 0)
        return 0x0b;

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                               (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    if (argResult[0] != 4) {
        status = TakeScriptString(argResult, gScratchStr);
        if (status != 0)
            return status;
        idx = FlatPageNameToIndex(gScratchStr);
        DF_TRACE("gotoflat: name='%.*s' -> page %d (cur=%d)",
                 (int)gScratchStr[0], (char *)&gScratchStr[1],
                 idx, (int)DAT_0048a758);
        if (idx < 0)
            return 0x0a;
        pageIdx = idx + 1;   /* FlatPageNameToIndex is 0-based */
    } else {
        DF_TRACE("gotoflat: index=%d (cur=%d)", pageIdx, (int)DAT_0048a758);
    }

    dataPtr = GetResDataPtr(DAT_0048a760);
    if ((pageIdx <= 0) || (*(int *)(dataPtr + 0x848) < pageIdx)) {
        UnlockResObject(DAT_0048a760);
        return 0x0a;
    }
    UnlockResObject(DAT_0048a760);

    if (pageIdx - DAT_0048a758 == 1) {
        DF_TRACE("gotoflat: already on page %d -- no-op", pageIdx - 1);
        return 0;
    }

    savedSceneHandle = *(int *)((char *)DAT_0048a754 + 0x530);
    status = SendCloseFlatMessage();
    if (status != 0)
        return status;

    if (gStage.active == 0)
        return 0;

    if (*(int *)((char *)DAT_0048a754 + 0x530) != savedSceneHandle)
        return 0;

    NavigateToStagePage(pageIdx - 1);
    return SendOpenFlatMessage();
#undef pageIdx
}


/* Sends the current flat page its open message: reads the page row (DAT_0048a758 of the table at
 * +0x84c, 0x2e bytes) and runs `"<page name>", openflat()` through RunSendToFlatCommand. Status 0x0b
 * if no stage is open. */
short __stdcall SendOpenFlatMessage(void)  /* 0x00445180 */
{
    /* ONE 46-byte flat-page row, copied whole (rep movsd 11 + movsw); its Pascal name is at +0x1e (class 3) */
    typedef struct { short w[23]; } FlatRow46;
    FlatRow46 pageRow;

    if (gStage.active == 0)
        return 0xb;

    pageRow = *(FlatRow46 *)(DAT_0048a758 * 0x2e + 0x84c + GetResDataPtr(DAT_0048a760));
    UnlockResObject(DAT_0048a760);

    CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
    AppendPascalString((byte *)&pageRow + 0x1e, &gScratchStr);
    AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteCommaOpenflatCall), &gScratchStr);
    DF_TRACE("SendOpenFlatMessage: page=%d '%.*s'", (int)DAT_0048a758,
             (int)gScratchStr[0], (char *)&gScratchStr[1]);
    return RunSendToFlatCommand(&gScratchStr);
}

#undef local_30
#undef local_12

/* SendOpenFlatMessage's twin: runs `"<page name>", closeflat()` for the current page. */
short __stdcall SendCloseFlatMessage(void)  /* 0x00445240 */
{
    /* ONE 46-byte flat-page row, copied whole (rep movsd 11 + movsw); its Pascal name is at +0x1e (class 3) */
    typedef struct { short w[23]; } FlatRow46;
    FlatRow46 pageRow;

    if (gStage.active == 0)
        return 0xb;

    pageRow = *(FlatRow46 *)(DAT_0048a758 * 0x2e + 0x84c + GetResDataPtr(DAT_0048a760));
    UnlockResObject(DAT_0048a760);

    CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
    AppendPascalString((byte *)&pageRow + 0x1e, &gScratchStr);
    AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteCommaCloseflatCall), &gScratchStr);
    DF_TRACE("SendCloseFlatMessage: page=%d '%.*s'", (int)DAT_0048a758,
             (int)gScratchStr[0], (char *)&gScratchStr[1]);
    return RunSendToFlatCommand(&gScratchStr);
}


/* Finishes opening the stage: runs the stage's openstage command (RunSendToStageCommand with
 * kStrOpenstageCall) and, if that succeeds and the stage is still the same, the current page's
 * `"<page name>", openflat()` (RunSendToFlatCommand). The page name is the Pascal string at row byte
 * +0x1e. Status 0x0b if no stage is open. */
short __stdcall CompleteStageInit(void)  /* 0x00445300 */
{
    /* the 46-byte page entry, copied as one object (rep movsd 0xb + movsw); name at +0x1e */
    typedef struct { byte b[0x2e]; } PageRow;
    short    st;
    int      saved;
    int      page;
    PageRow  row;

    if (gStage.active == 0)
        return 0xb;

    saved = *(int *)((char *)DAT_0048a754 + 0x530);
    page  = DAT_0048a758;
    row   = *(PageRow *)((char *)GetResDataPtr(DAT_0048a760) + 0x84c + DAT_0048a758 * 0x2e);
    UnlockResObject(DAT_0048a760);

    st = RunSendToStageCommand((byte *)UpdatePascalLength(&kStrOpenstageCall));
    if (st != 0)
        return st;
    if (gStage.active == 0)
        return 0;
    if (*(int *)((char *)DAT_0048a754 + 0x530) != saved)
        return 0;
    if (page != DAT_0048a758)
        return 0;

    CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
    AppendPascalString(row.b + 0x1e, &gScratchStr);
    AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteCommaOpenflatCall), &gScratchStr);
    return RunSendToFlatCommand(&gScratchStr);
}


/* The stage's pre-shutdown step: sends the current page `"<page name>", closeflat()`
 * (RunSendToFlatCommand) and, if that succeeds and the stage is still the same, the stage its
 * closestage command (RunSendToStageCommand with kStrClosestageCall). Status 0x0b if no stage is
 * open. Returns short, as the exe does. */
short __stdcall StagePreShutdownStep(void)  /* 0x00445430 */
{
    /* as CompleteStageInit: the 46-byte page entry copied as one object, indexed by a fresh read of DAT_0048a758 */
    typedef struct { byte b[0x2e]; } PageRow;
    short    st;
    int      saved;
    int      page;
    PageRow  row;

    if (gStage.active == 0)
        return 0xb;
    saved = *(int *)((char *)DAT_0048a754 + 0x530);
    page  = DAT_0048a758;
    row   = *(PageRow *)((char *)GetResDataPtr(DAT_0048a760) + 0x84c + DAT_0048a758 * 0x2e);
    UnlockResObject(DAT_0048a760);

    CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
    AppendPascalString(row.b + 0x1e, &gScratchStr);
    AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteCommaCloseflatCall), &gScratchStr);
    st = RunSendToFlatCommand(&gScratchStr);
    if (st != 0)
        return st;
    if (gStage.active == 0)
        return 0;
    if (*(int *)((char *)DAT_0048a754 + 0x530) != saved)
        return 0;
    if (DAT_0048a758 != page)
        return 0;
    return RunSendToStageCommand((byte *)UpdatePascalLength(&kStrClosestageCall));
}


/* countflats (20035): the number of flat pages of the open stage, the header dword +0x848, as an
 * int. */
short __cdecl ScriptCmd_CountFlats(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount)  /* 0x00445560 */
{
    int   base;
    uint  countVal;

    *pSkipCount = 0;
    if (gStage.active == 0)
        return 0x0b;

    base     = GetResDataPtr(DAT_0048a760);
    countVal = (uint)*(int *)(base + 0x848);   /* read, released, then stored */
    UnlockResObject(DAT_0048a760);
    *(uint *)(pResult + 1) = countVal;
    *pResult               = 4;
    pResult[3]             = 0;
    return 0;
}


/* indextoflat (20036): the name of flat page n (1-based) as a string. Status 0x0b if no stage is
 * open, 0x0e if n is not an int, 10 if it is outside 1..count. */
short __cdecl ScriptCmd_IndexToFlat(int *pCtx, short *pArgBlock, int *pTable,
                                                           short *pResult, int *pSkipCount) /* 0x004455C0 */
{
    /* the 46-byte flat-page row, copied as one object (rep movsd 11 + movsw) */
    typedef struct { short w[23]; } FlatRow46;
    short     u;
    int       base;
    short     typed[4];
    FlatRow46 row;

    /* an inactive stage is status 0xb here (the port returned an empty string); the empty-string branch below is the exe's unreachable second test */
    if (gStage.active == 0)
        return 0xb;

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSkipCount);
    if (u != 0)
        return u;
    if (typed[0] != 4)
        return 0xE;
    if (gStage.active == 0)
        return AllocScriptString((byte *)UpdatePascalLength(&kStrNone), pResult);

    base = GetResDataPtr(DAT_0048a760);
    if (*(int *)(typed + 1) <= 0 || *(int *)(base + 0x848) < *(int *)(typed + 1)) {
        UnlockResObject(DAT_0048a760);
        return 10;
    }
    row = *(FlatRow46 *)(*(int *)(typed + 1) * 0x2e + 0x81e + base);
    UnlockResObject(DAT_0048a760);
    return AllocScriptString((byte *)&row + 0x1e, pResult);
}


/* countbuttons (20044): the number of buttons of a flat page, the dword at +0x404 of the page's
 * button child. */
short __cdecl ScriptCmd_CountButtons(int *pCtx, int pArgBlock, int *pTable,
                                                      short *pResult, int *pSubSkip) /* 0x004456C0 */
{
    typedef struct { short w[23]; } FlatRow46;
    short  st;
    short  typed[4];
    int   *hChild;
    FlatRow46 flat;

    if (gStage.active == 0)
        return 0x0b;

    st = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, (int *)typed, pSubSkip);
    if (st != 0)
        return st;
    st = TakeScriptString(typed, gScratchStr);
    if (st != 0)
        return st;
    st = LookupFlatEntry(gScratchStr, (uint *)&flat);
    if (st != 0)
        return st;
    /* the flat row's child selector: the unaligned uint at byte +14 */
    st = AttachRenderChild((int *)DAT_0048a754, *(uint *)((byte *)&flat + 14), &hChild);
    if (st != 0)
        FatalError(st, 0x103f);
    *(int *)(pResult + 1) = *(int *)(GetResDataPtr(hChild) + 0x404);
    UnlockResObject(hChild);
    st = DetachRenderChild((int)DAT_0048a754, *(uint *)((byte *)&flat + 14));
    if (st != 0)
        FatalError(st, 0x1040);
    pResult[3] = 0;
    *pResult   = 4;
    return 0;
}


/* indextobutton (20045): the name of button n (1-based) of a flat page: rows of 0x20 bytes from
 * +0x408, the name at +0x10 of each. */
short __cdecl ScriptCmd_IndexToButton(int *pCtx, int pArgBlock, int *pTable,
                                                     short *pResult, int *pSkipCount) /* 0x004457D0 */
{
    typedef struct { short w[23]; } FlatRow46;
    typedef struct { uint w[8]; } ButtonRow32;
    short st;
    short arg1[4], arg2[4];
    int  *hChild;
    int   base;
    FlatRow46 flat;
    ButtonRow32 row;

    if (gStage.active == 0)
        return 0x0b;

    st = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)arg1, (int *)arg2, pSkipCount);
    if (st != 0)
        return st;
    st = TakeScriptString(arg1, gScratchStr);
    if (st != 0)
        return st;
    st = LookupFlatEntry(gScratchStr, (uint *)&flat);
    if (st != 0)
        return st;
    if (arg2[0] != 4)
        return 0xE;

    /* the flat row's child selector: the unaligned uint at byte +14 */
    st = AttachRenderChild((int *)DAT_0048a754, *(uint *)((byte *)&flat + 14), &hChild);
    if (st != 0)
        FatalError(st, 0x1041);
    base = GetResDataPtr(hChild);
    if (*(int *)(arg2 + 1) > 0 && *(int *)(base + 0x404) >= *(int *)(arg2 + 1)) {
        /* 1-based: row n is at +0x408 + (n-1)*0x20; the port had read row n+1 */
        row = *(ButtonRow32 *)(base + 0x3e8 + *(int *)(arg2 + 1) * 0x20);
        UnlockResObject(hChild);
        st = DetachRenderChild((int)DAT_0048a754, *(uint *)((byte *)&flat + 14));
        if (st != 0)
            FatalError(st, 0x1043);
        return AllocScriptString((byte *)&row + 0x10, pResult);
    }
    UnlockResObject(hChild);
    st = DetachRenderChild((int)DAT_0048a754, *(uint *)((byte *)&flat + 14));
    if (st != 0)
        FatalError(st, 0x1042);
    return 10;
}


/* flattoindex (20037): the 1-based index of the flat page with the given name (FlatPageNameToIndex +
 * 1). */
short __cdecl ScriptCmd_FlatToIndex(int *pCtx, short *pArgBlock, int *pTable,
                                                                   short *pResult, int *pSkipCount) /* 0x00445950 */
{
    short  u;
    short typed[4];
    int   idx;

    if (gStage.active == 0)
        return 0x0b;

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSkipCount);
    if (u != 0)
        return u;

    u = TakeScriptString(typed, gScratchStr);
    if (u != 0)
        return u;

    idx = FlatPageNameToIndex((byte *)gScratchStr);
    *pResult              = 4;
    *(int *)(pResult + 1) = idx + 1;
    pResult[3]            = 0;
    return 0;
}


/* stagevisible (16008, value form): the stage's visible flag (DAT_0048a74e) as a bool, false when no
 * stage is open. */
short ScriptCmd_StageVisible(int *pCtx, short *pArgBlock, int *pTable,  /* 0x004459d0 */
                            short *pResult, int *pSkipCount) /* 0x004459D0 */
{
    *pSkipCount = 0;
    if (gStage.active == 0) {
        *(int *)(pResult + 1) = 0;
    } else {
        *(int *)(pResult + 1) = (int)DAT_0048a74e;
    }
    pResult[3] = 0;
    *pResult = 2;
    return 0;
}


/* stagevisible (16008, statement form): sets the stage's visible flag (DAT_0048a74e) from a bool
 * argument; no change is a no-op. Showing re-navigates to the current page (NavigateToStagePage);
 * hiding blanks the trigger surface (ZeroBlock), resets the previous page to -1 and requests a
 * whole-scene redraw. Status 0x0b if no stage is open, 0x0e for a non-bool. */
short __cdecl
ActionCmd_StageVisible(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x00445A10 */
{
    /* the visibility word is the LOW half of the bool's value (the port took the high half); the 120-byte
     * trigger buffer is filled by the callee and ZeroBlock clears its surface */
    short  u;
    short  typed[4];
    short  rect[4];
    short  work[60];
    unsigned short w;

    if (gStage.active == 0)
        return 0x0b;
    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSubSkip);
    if (u != 0)
        return u;
    if (typed[0] != 2)
        return 0x0e;
    w = typed[1];
    if ((unsigned short)gStage.flag1 == w)   /* the exe's flag word is unsigned */
        return 0;
    DAT_0048a74e = w;
    if (w != 0) {
        /* NavigateToStagePage reads exactly one parameter; the original ignores any second argument */
        NavigateToStagePage(DAT_0048a758);
        return 0;
    }
    CopyTrigSceneBufferAndViewportPair(work, (int *)rect);
    ZeroBlock(*(void **)(work + 2), work[1] * work[0]);
    DAT_0048a75c = -1;
    BumpSceneOverlayModeFloor(2);
    return 0;
}


/* currentflat (20038): the name of the current flat page as a string. */
short __cdecl ScriptCmd_CurrentFlat(int *pCtx, short *pArgBlock, int *pTable,
                                                         short *pResult, int *pSkipCount) /* 0x00445B00 */
{
    /* the 46-byte flat-page row, copied as one object (rep movsd 11 + movsw) */
    typedef struct { short w[23]; } FlatRow46;
    FlatRow46 row;

    (void)pCtx;
    (void)pArgBlock;
    (void)pTable;

    *pSkipCount = 0;

    if (gStage.active == 0)
        return AllocScriptString((byte *)UpdatePascalLength(&kStrNone), pResult);

    row = *(FlatRow46 *)(GetResDataPtr(DAT_0048a760) + DAT_0048a758 * 0x2e + 0x84c);
    UnlockResObject(DAT_0048a760);
    /* AllocScriptString's status is the result (the port had returned 0) */
    return AllocScriptString((byte *)&row + 0x1e, pResult);
}


/* currentstage (20048): the open stage's name (DAT_0048a768) as a string, or kStrNone when no stage
 * is open; also leaves the leaf of the stage node's path in gScriptResult. */
short __cdecl ScriptCmd_CurrentStage(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x00445BA0 */
{
    uint status;

    *pSkipCount = 0;                    /* the five-argument handler: skip count cleared first */

    if (gStage.active != 0) {
        status = AllocScriptString(&DAT_0048a768, pResult);
        CopyPascalString((byte *)((char *)DAT_0048a754 + 0x24), gScriptResult);
        TruncatePathToLeaf(gScriptResult);
        return status;
    }
    status = AllocScriptString((byte *)UpdatePascalLength(&kStrNone), pResult);
    return status;
}


/* pointinstage (20042): a bool, true if the point argument lies in the stage's viewport rect. */
short __cdecl ScriptCmd_PointInStage(int *pCtx, short *pArgBlock, int *pTable,
                                                     short *pResult, int *pSkipCount) /* 0x00445C10 */
{
    short u;
    short typed[4];
    short rect[4];       /* the viewport pair: 8 bytes (0x00445c84) */
    int   work[30];      /* the 120-byte trigger buffer */

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSkipCount);
    if (u != 0)
        return u;
    if (typed[0] != 4)
        return 0x0e;

    pResult[3]            = 0;
    *(int *)(pResult + 1) = 0;
    *pResult              = 2;
    if (gStage.active != 0 && DAT_0048a74e != 0) {
        CopyTrigSceneBufferAndViewportPair((short *)work, (int *)rect);
        *(int *)(pResult + 1) = (short)PointInSceneRect(*(ScenePointYX *)(typed + 1), rect);
        return 0;
    }
    return 0;
}


/* pointinbutton (20043): pointinbutton(flat, button, point): a bool, true if the point lies in that
 * button's rect. */
short __cdecl ScriptCmd_PointInButton(int *pCtx, int pArgBlock, int *pTable,
                                                         short *pResult, int *pSkipCount) /* 0x00445CD0 */
{
    typedef struct { short w[23]; } FlatRow46;
    short st;
    short arg1[4], arg2[4], arg3[4];
    uint  buttonBuf[8];
    FlatRow46 flat;

    if (gStage.active == 0)
        return 0x0b;

    st = EvalThreeScriptArgs((int)pCtx, pArgBlock, pTable,
                             (int *)arg1, (int *)arg2, (int *)arg3, pSkipCount);
    if (st != 0)
        return st;
    st = TakeScriptString(arg1, gScratchStr);
    if (st != 0)
        return st;
    st = LookupFlatEntry(gScratchStr, (uint *)&flat);
    if (st != 0)
        return st;
    st = TakeScriptString(arg2, gScratchStr);
    if (st != 0)
        return st;
    /* the flat row's child selector: the unaligned uint at byte +14 */
    st = LookupButtonEntry(*(uint *)((byte *)&flat + 14), gScratchStr, buttonBuf);
    if (st != 0)
        return st;
    if (arg3[0] != 4)
        return 0xE;

    *pResult              = 2;
    *(int *)(pResult + 1) = 0;
    pResult[3]            = 0;
    /* the rect is at buttonBuf + 4: {+0 ?, +4 rect[4], +0xc selector, +0x10 name}; reading it 4 bytes early
     * made every button answer false */
    *(int *)(pResult + 1) = (short)PointInSceneRect(*(ScenePointYX *)(arg3 + 1),
                                                    (short *)((byte *)buttonBuf + 4));
    return 0;
}


/* stagescript (12063): runs the stage's script through ClearScriptResult under the label "Stage
 * Script: <path>" (DAT_0048a768). Status 0x0b if no stage is open. */
short __cdecl ActionCmd_StageScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x00445de0 */
{
    (void)pCtx; (void)pArgBlock; (void)pTable;
    *pSubSkip = 0;
    if (gStage.active == 0)
        return 0x0b;

    CopyPascalString((byte *)UpdatePascalLength(kStrStageScriptColon), gScratchStr2);
    AppendPascalString(&DAT_0048a768, gScratchStr2);
    ClearScriptResult(DAT_0048a754, DAT_0048a764, gScratchStr2, 0);
    return 0;
}


/* flatscript (12064): runs a flat page's script through ClearScriptResult under the label "Flat
 * Script: <page name>". The page is looked up by name (LookupFlatEntry). Status 0x0b if no stage is
 * open. */
short __cdecl ActionCmd_FlatScript(int pCtx, short *pArgBlock,  /* 0x00445e50 */
                                           int *pTable, int *pSubSkip)
{
    short   status;
    short  args[4];
    /* LookupFlatEntry copies 46 bytes into one 48-byte local; the page name is the entry's bytes from +30 on (class 3) */
    int    flatEntry[12];

    if (gStage.active == 0)
        return 0x0b;

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)args, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(args, gScratchStr);
    if (status != 0)
        return status;

    status = LookupFlatEntry(gScratchStr, flatEntry);
    if (status != 0)
        return status;

    CopyPascalString((byte *)UpdatePascalLength(kStrFlatScriptColon), gScratchStr2);
    AppendPascalString((byte *)flatEntry + 30, gScratchStr2);
    ClearScriptResult(DAT_0048a754, *(int *)((char *)flatEntry + 6), gScratchStr2, 0);
    return 0;
}


/* buttonscript (12065): buttonscript(flat, button) runs a button's script through ClearScriptResult
 * under the label "Button Script: <button name>": the flat page by LookupFlatEntry, then the button
 * in it by LookupButtonEntry (its index is the page row's dword +14). Status 0x0b if no stage is
 * open. */
short __cdecl ActionCmd_ButtonScript(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00445f10 */
{
    short  evalStatus;
    short   status;
    short  arg1Result[4];   /* first typed result from EvalTwoScriptArgs */
    short  arg2Result[4];   /* second typed result from EvalTwoScriptArgs */
    /* LookupButtonEntry copies 32 bytes and LookupFlatEntry 46: the buffers were int[3] and int[4] (class 3) */
    int    buttonEntry[8];  /* local_50 + local_40 */
    int    flatEntry[12];    /* local_30 + local_22 */

    if (gStage.active == 0)
        return 0x0b;

    evalStatus = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)arg1Result, (int *)arg2Result, pSubSkip);
    status = (unsigned short)evalStatus;
    if (evalStatus != 0)
        return status;

    status = TakeScriptString(arg1Result, gScratchStr);
    if (status != 0)
        return status;

    status = LookupFlatEntry(gScratchStr, flatEntry);
    if (status != 0)
        return status;

    status = TakeScriptString(arg2Result, gScratchStr);
    if (status != 0)
        return status;

    /* the index is the dword at flatEntry+14, and the name appended is buttonEntry+16 */
    status = LookupButtonEntry(*(unsigned int *)((byte *)flatEntry + 14), gScratchStr, buttonEntry);
    if (status != 0)
        return status;

    CopyPascalString((byte *)UpdatePascalLength(kStrButtonScriptColon), gScratchStr2);
    AppendPascalString((byte *)buttonEntry + 16, gScratchStr2);
    ClearScriptResult(DAT_0048a754, *(int *)((char *)buttonEntry + 0xc), gScratchStr2, 0);
    return 0;
}


/* sendtobutton (12068): ScriptCmd_SendToButtonFx without a result. */
short __cdecl ActionCmd_SendToButton(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00446010 */
{
    return ScriptCmd_SendToButtonFx((int *)pCtx, pArgBlock, pTable, 0, pSubSkip);
}


/* sendtobuttonfx (20092): sendtobutton(flat, button), method(args). Looks up the flat page and the
 * button in it, then runs the method in a chain of four frames: the button's script ("Button Script:
 * <button>"), the flat page's ("Flat Script: <page>"), the stage's ("Stage Script: <stage path>")
 * and the boot file's selector 2 ("System: ", "Post Script"). The arguments are followed by an 0xfb4
 * separator, and the method call must start past it (else script error 14). */
short __cdecl ScriptCmd_SendToButtonFx(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSubSkip)  /* 0x00446030 */
{
    /* the 46-byte flat-page row LookupFlatEntry copies out */
    typedef struct { short w[23]; } FlatRow46;
    int             start;      /* the parameter walks; this keeps its start */
    short           status;
    int             skipC;      /* skip count -- written by EvalTwoScriptArgs */
    short           a1[4];      /* arg1 result (written by EvalTwoScriptArgs) */
    short           a2[4];      /* arg2 result */
    uint            buttonBuf[8];  /* button entry: selector at [3], name at [4] */
    /* MUST be MethodCallFrame[4]: EvalMethodCall walks the frames by +0x4e, so they are contiguous in the
     * exe (class 3). Each frame is filled through a pointer, as in ScriptCmd_SendToBootFx (byte match). */
    MethodCallFrame    f[4];
    MethodCallFrame   *pf;
    FlatRow46       flat;       /* selector at +6, child id at +0xe, page name at +0x1e */
    int            *h;

    if (gStage.active == 0)
        return 0x0b;

    start = pArgBlock;
    status = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, &skipC);
    if (status != 0)
        return status;

    pArgBlock += skipC * 8;

    /* the 0xfb4 separator must follow the arguments and EvalMethodCall must start past it; omitting that
     * was script error 14 whenever the life ring's menu buttons ran sendtobutton */
    if (*(short *)pArgBlock != 0xfb4)
        return 0x1c;
    pArgBlock += 8;

    status = TakeScriptString(a1, gScratchStr);
    if (status != 0) return status;

    status = LookupFlatEntry(gScratchStr, (uint *)&flat);
    if (status != 0) return status;

    status = TakeScriptString(a2, gScratchStr);
    if (status != 0) return status;

    status = LookupButtonEntry(*(uint *)((char *)&flat + 0xe), gScratchStr, buttonBuf);
    if (status != 0) return status;

    pf = &f[0];
    pf->pOwner     = (void *)DAT_0048a754;
    pf->selector   = buttonBuf[3];                       /* button_entry+0x0c */
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(kStrButtonScriptColon), pf->buf1);   /* "Button Script: " */
    CopyPascalString((byte *)&buttonBuf[4], pf->buf0);   /* button name, button_entry+0x10 */
    CopyPascalString((byte *)&buttonBuf[4], pf->buf2);
    pf->isLast     = 0;

    pf = &f[1];
    pf->pOwner     = (void *)DAT_0048a754;
    pf->selector   = *(uint *)((char *)&flat + 6);       /* flat_entry+6 */
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(kStrFlatScriptColon), pf->buf1);   /* "Flat Script: " */
    CopyPascalString((byte *)&flat + 0x1e, pf->buf0);    /* page name, flat_entry+0x1e */
    CopyPascalString((byte *)&buttonBuf[4], pf->buf2);
    pf->isLast     = 0;

    pf = &f[2];
    pf->pOwner     = (void *)DAT_0048a754;
    pf->selector   = DAT_0048a764;
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(kStrStageScriptColon), pf->buf1);   /* "Stage Script: " */
    CopyPascalString(&DAT_0048a768, pf->buf0);
    CopyPascalString((byte *)&buttonBuf[4], pf->buf2);
    pf->isLast     = 0;

    pf = &f[3];
    pf->pOwner     = (int *)gBootNode;
    pf->selector   = 2u;
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, 2u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), pf->buf1);   /* "System: " */
    CopyPascalString((byte *)UpdatePascalLength(kStrPostScript), pf->buf0);   /* "Post Script" */
    CopyPascalString((byte *)&buttonBuf[4], pf->buf2);
    pf->isLast     = 1;

    status = EvalMethodCall((short *)f, (void *)pCtx, (short *)pArgBlock,
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
    *pSubSkip = (pArgBlock - start) >> 3;
    return status;
}


/* Runs the command text pCmd (a Pascal string) as a sendtoflat statement, in a method frame named
 * kStrFlatMessage with the "System:" owner; the flat counterpart of RunSendToPuppetCommand. */
short __cdecl RunSendToFlatCommand(byte *pCmd)                   /* 0x00446460 */
{
    int            *ph;
    int             skipOut;
    int             posErr;
    MethodCallFrame frame;
    short            resolveStatus;
    short           sendStatus;
    int             rowBase;

    resolveStatus = WaitHeapThenResolveMethodPath(pCmd + 1, (uint)*pCmd, &ph, &posErr);
    if (resolveStatus != 0)
        return resolveStatus;

    rowBase          = GetResDataPtr(ph);
    frame.isLast     = 1;
    frame.saved0x14c = 0xffffffffu;
    frame.pData      = (void *)rowBase;

    CopyPascalString((byte *)UpdatePascalLength(kStrFlatMessage), frame.buf0);
    CopyPascalString((byte *)UpdatePascalLength(kStrFlatMessage), frame.buf2);
    CopyPascalString((byte *)UpdatePascalLength(&kStrSystemColon), frame.buf1);

    sendStatus = (short)ActionCmd_SendToFlat((int)&frame, rowBase, (int *)0, &skipOut);
    UnlockResObject(ph);
    FreeResObject(ph);
    return sendStatus;
}


/* sendtoflat (12069): ScriptCmd_SendToFlatFx without a result. */
short __cdecl ActionCmd_SendToFlat(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00446540 */
{
    return ScriptCmd_SendToFlatFx((int *)pCtx, pArgBlock, pTable, 0, pSubSkip);
}


/* sendtoflatfx (20093): sendtoflat(page), method(args). Runs the method in a chain of three frames:
 * the flat page's script, the stage's and the boot file's (selector 2, "Post Script"). The names are
 * the page row's own stored name, not the string the script passed. */
short __cdecl
ScriptCmd_SendToFlatFx(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSubSkip)  /* 0x00446560 */
{
    /* the 46-byte flat-page row LookupFlatEntry copies out */
    typedef struct { short w[23]; } FlatRow46;
    int             start;           /* the parameter walks; this keeps its start */
    short           status;
    int             skipC;
    short           a4[4];
    /* MUST be MethodCallFrame[3]: the frames are contiguous at stride 0x4e (class 3); each is filled through a pointer */
    MethodCallFrame f[3];
    MethodCallFrame *pf;
    FlatRow46       flat;            /* selector at +6, page name at +0x1e */
    int            *h;

    if (gStage.active == 0)
        return 0x0b;

    start = pArgBlock;
    status = ProcessScriptBlock((int)pCtx, (short *)pArgBlock, pTable, (int *)a4, &skipC);
    if (status != 0)
        return status;

    pArgBlock += skipC * 8;
    if (*(short *)pArgBlock != 0xfb4)
        return 0x1c;
    pArgBlock += 8;

    status = TakeScriptString(a4, gScratchStr);
    if (status != 0)
        return status;

    status = LookupFlatEntry(gScratchStr, (uint *)&flat);
    if (status != 0)
        return status;

    /* all four names are the flat entry's own page name (entry +0x1e), not gScratchStr (class 3) */

    pf = &f[0];
    pf->pOwner     = (void *)DAT_0048a754;
    pf->selector   = *(uint *)((char *)&flat + 6);
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(&kStrFlatScriptColon), pf->buf1);   /* the destination is pushed before the lookup call */
    CopyPascalString((byte *)&flat + 0x1e, pf->buf0);
    CopyPascalString((byte *)&flat + 0x1e, pf->buf2);
    pf->isLast     = 0;

    pf = &f[1];
    pf->pOwner     = (void *)DAT_0048a754;
    pf->selector   = DAT_0048a764;
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(&kStrStageScriptColon), pf->buf1);
    CopyPascalString(&DAT_0048a768, pf->buf0);
    CopyPascalString((byte *)&flat + 0x1e, pf->buf2);
    pf->isLast     = 0;

    pf = &f[2];
    pf->pOwner     = (int *)gBootNode;
    pf->selector   = 2u;
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, 2u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(&kStrSystemColon), pf->buf1);
    CopyPascalString((byte *)UpdatePascalLength(&kStrPostScript), pf->buf0);
    CopyPascalString((byte *)&flat + 0x1e, pf->buf2);
    pf->isLast     = 1;

    status = EvalMethodCall((short *)f, (void *)pCtx, (short *)pArgBlock, pTable, pResult, &skipC);

    UnlockResObject((int *)f[0].hMethod);
    ReleaseMethodHandle((int)f[0].hMethod);
    UnlockResObject((int *)f[1].hMethod);
    ReleaseMethodHandle((int)f[1].hMethod);
    UnlockResObject((int *)f[2].hMethod);
    ReleaseMethodHandle((int)f[2].hMethod);

    pArgBlock += skipC * 8;
    *pSubSkip = (pArgBlock - start) >> 3;
    return status;
}

#undef local_30
#undef local_12

/* Runs the stage action named by the Pascal string pMethod: resolves it with
 * WaitHeapThenResolveMethodPath, builds a method frame (kStrStageMessage names, kStrSystemColon
 * owner) and calls ActionCmd_SendToStage on the resolved code, then releases the node. */
short __cdecl RunSendToStageCommand(byte *pMethod)  /* 0x00446880 */
{
    short       st;
    void       *pData;
    int        *hMethod;  /* local_5c */
    int         skipOut;   /* local_58; the exe does not initialise it */
    int         posErr;  /* local_54 */
    /* the local_50 block is a MethodCallFrame (class 3): it reaches EvalMethodCall, which walks frames by adjacency */
    MethodCallFrame frame;

    st = WaitHeapThenResolveMethodPath(pMethod + 1, (uint)*pMethod, &hMethod, &posErr);
    if (st != 0)
        return st;
    /* no zeroing: the exe sets only the fields below */
    pData            = (void *)GetResDataPtr(hMethod);
    frame.pData      = pData;                                /* local_3c  */
    frame.isLast     = 1;                                    /* local_50  */
    frame.saved0x14c = 0xffffffff;                           /* local_4e  */
    CopyPascalString((byte *)UpdatePascalLength(&kStrStageMessage), frame.buf0);
    CopyPascalString((byte *)UpdatePascalLength(&kStrStageMessage), frame.buf2);
    CopyPascalString((byte *)UpdatePascalLength(&kStrSystemColon), frame.buf1);
    st = ActionCmd_SendToStage((int)&frame, (int)pData, 0, &skipOut);
    UnlockResObject(hMethod);
    FreeResObject(hMethod);
    return st;
}


/* sendtostage (12070): ScriptCmd_SendToStageFx without a result. Four parameters, like
 * ActionCmd_SendToFlat. */
short __cdecl ActionCmd_SendToStage(int pCtx, int pArgBlock, int *pTable,
                                   int *pSubSkip)  /* 0x00446960 */
{
    return (uint)(unsigned short)
        ScriptCmd_SendToStageFx((int *)pCtx, (short *)pArgBlock, pTable, 0, pSubSkip);
}


/* sendtostagefx (20094): sendtostage, method(args). Runs the method in a chain of two frames: the
 * stage's script ("Stage Script: ") and the boot file's selector 2 ("System: ", "Post Script"). Five
 * cdecl parameters.
 *
 * Port note: Ghidra showed it as __stdcall void and the writes through the fifth argument landed on
 * the caller's stack: openstage() crashed (class 2); the two frames must also be one
 * MethodCallFrame[2] (class 3). */
unsigned short __cdecl
ScriptCmd_SendToStageFx(int *pCtx, short *pArgBlock, int *pTable,
                        void *pResult, int *pSubSkip)  /* 0x00446980 */
{
    int            start;               /* the parameter walks; this keeps its start */
    short          status;
    int            skipOut;  /* local_a0 */
    /* MUST be MethodCallFrame[2]: EvalMethodCall walks the frames by +0x4e (class 3) */
    MethodCallFrame f[2];
    int           *h1, *h2;
    MethodCallFrame *pf;   /* frames filled through a pointer, as ScriptCmd_SendToBootFx (byte match) */

    if (gStage.active == 0)
        return 0x000B;

    start = (int)pArgBlock;

    /* Same shape as ScriptCmd_SendToSetFx: owner and selector passed from the
     * frame fields, the saved word read back through the owner field. */
    pf = &f[0];
    pf->pOwner   = (void *)DAT_0048a754;
    pf->selector = DAT_0048a764;
    pf->hMethod  = h1 = (int *)AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData    = (void *)GetResDataPtr(h1);
    CopyPascalString((byte *)UpdatePascalLength(&kStrStageScriptColon), pf->buf1);
    CopyPascalString(&DAT_0048a768, pf->buf0);
    CopyPascalString(&DAT_0048a768, pf->buf2);
    pf->isLast = 0;

    pf = &f[1];
    pf->pOwner   = (int *)gBootNode;
    pf->selector = 2u;
    pf->hMethod  = h2 = (int *)AcquireMethodHandle((int *)pf->pOwner, 2u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData    = (void *)GetResDataPtr(h2);
    CopyPascalString((byte *)UpdatePascalLength(&kStrSystemColon), pf->buf1);
    CopyPascalString((byte *)UpdatePascalLength(&kStrPostScript), pf->buf0);
    CopyPascalString(&DAT_0048a768, pf->buf2);
    pf->isLast   = 1;

    status = EvalMethodCall((short *)&f[0], pCtx, pArgBlock,
                          pTable, pResult, &skipOut);

    UnlockResObject(f[0].hMethod);
    ReleaseMethodHandle((int)f[0].hMethod);
    UnlockResObject(f[1].hMethod);
    ReleaseMethodHandle((int)f[1].hMethod);
    pArgBlock += skipOut * 4;
    *pSubSkip = ((int)pArgBlock - start) >> 3;
    return (unsigned short)status;
}


/* Finds the button of the current flat page that contains packedPt (rows of 0x20 bytes from the end
 * of the list backwards, the rect at +4) and copies its name (+0x10) to pDescOut. Returns 1 on a
 * hit, else 0. Fatal 0x1044 / 0x1045 on attach and detach errors. */
ushort __cdecl FindButtonNameAtPoint(int packedPt, byte *pDescOut) /* 0x00446B40 */
{
    typedef struct { short w[23]; } FlatRow46;
    short  u;
    int   *hChild;
    FlatRow46 row;
    int    i;
    ushort hit;
    int    btnCount;
    byte  *pWalk;
    int    base;

    hit = 0;
    row = *(FlatRow46 *)(GetResDataPtr(DAT_0048a760) + DAT_0048a758 * 0x2e + 0x84c);
    UnlockResObject(DAT_0048a760);

    /* the flat row's child selector: the unaligned uint at byte +14 */
    u = AttachRenderChild((int *)DAT_0048a754, *(uint *)((byte *)&row + 14), &hChild);
    if (u != 0)
        FatalError(u, 0x1044);

    base     = GetResDataPtr(hChild);
    btnCount = *(int *)(base + 0x404);
    pWalk    = (byte *)base + 1000 + btnCount * 0x20;
    for (i = 0; i < btnCount; i++) {
        if (PointInSceneRect(*(ScenePointYX *)&packedPt, (short *)(pWalk + 4))) {
            hit = 1;
            CopyPascalString(pWalk + 0x10, pDescOut);
            break;
        }
        pWalk -= 0x20;
    }
    UnlockResObject(hChild);
    u = DetachRenderChild((int)DAT_0048a754, *(uint *)((byte *)&row + 14));
    if (u != 0)
        FatalError(u, 0x1045);
    return hit;
}


/* stageparam (16056, statement form): stageparam(selector, value) sets DAT_0048a750 (selector 1) or
 * DAT_0048a752 (selector 2); 0x0e for any other selector. */
short __cdecl
ActionCmd_StageParam(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00446c50 */
{
    short s1;
    short o1[4], o2[4];

    if (gStage.active == 0)
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
        DAT_0048a750 = (short)*(int *)(o2 + 1);
        return 0;
    case 2:
        DAT_0048a752 = (short)*(int *)(o2 + 1);
        return 0;
    }
    return 0x0E;
}


/* stageparam (16056, value form): the value of DAT_0048a750 (selector 1) or DAT_0048a752 (selector
 * 2) as an int. Status 0x0b if no stage is open, 0x0e for a non-int or other selector. */
short ScriptCmd_StageParam(int *pCtx, short *pArgBlock, int *pTable,
                              short *pResult, int *pSkipCount) /* 0x00446CF0 */
{
    short  status;
    /* local_8 + local_6 are ONE 8-byte typed value (class 3) */
    short argResult[4];   /* [0]=type, [1..2]=value */

    if (gStage.active == 0)
        return 0x0b;

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    if (argResult[0] != 4)
        return 0x0e;

    switch (*(int *)(argResult + 1)) {
    case 1:  *(int *)(pResult + 1) = DAT_0048a750; break;   /* value first, then the type */
    case 2:  *(int *)(pResult + 1) = DAT_0048a752; break;
    default: return 0x0e;
    }
    *pResult = 4;
    pResult[3] = 0;
    return 0;
}


/* Brings the stage's displayed page to fromPage (it takes exactly one parameter, used both as the
 * scan origin and the loop bound). Does nothing unless the stage is visible (DAT_0048a74e) and the
 * previous page (DAT_0048a75c) differs. Otherwise it scans back from fromPage for a page whose flag
 * bit 0 marks a starting point, or the previous page (fatal 0x1047 if none), then walks forward to
 * fromPage decoding each page's image (ResolveDisplayAttachSlot, DecodePageNodeImage; 0x1048) and
 * detaching or releasing its child (0x104a / 0x104b), switching the display channel when
 * DAT_0048a752 is set. DAT_0048a758 always ends as fromPage, and a redraw is requested. FatalError
 * 0x1046 if no stage is open.
 *
 * Page rows are 0x2e bytes from data+0x84c (count at +0x848): +0 flags, +10 channel/child id, +14
 * child id, +0x1e name.
 *
 * Port note: the child id of the detach is the same +10 field as the attach, not Ghidra's separate
 * +14; the wrong offsets fataled 0x104a on the first navigation (class 3; story in bug-
 * instances.md). */
void __cdecl NavigateToStagePage(int fromPage)  /* 0x00446d80 */
{
    typedef struct { byte b[0x2e]; } PageRow;   /* copied whole: rep movsd 0xb + movsw */
    short    notFound;
    short    st;
    byte    *base;
    int     *pNodeEntry;
    PageRow  entry;
    int      viewRect[2];   /* local_b0 */
    PageRow  next;
    /* trigWork is the 120-byte decode target and viewRect the viewport pair; the port had dropped both (class 3) */
    short    trigWork[60];  /* local_78 */
    int      i;
    byte    *p;

    /* chanId and childId are the same field, at +10: the attach and the detach are one balanced pair around the
     * codec call. Ghidra's decompile shows a separate +14 for the detach, which is wrong (class 3). */
#define entryFlags   (*(uint *)entry.b)
#define entryChanId  (*(uint *)(entry.b + 10))
#define entryChildId entryChanId
#define nextChanId   (*(uint *)(next.b + 10))

    if (gStage.active == 0)
        FatalError(0, 0x1046);

    if ((DAT_0048a74e != 0) && (DAT_0048a75c != fromPage)) {
        notFound = 1;
        CopyTrigSceneBufferAndViewportPair(trigWork, viewRect);
        base = (byte *)GetResDataPtr(DAT_0048a760);
        i = fromPage;

        if (i >= 0) {
            p = base + 0x84c + i * 0x2e;
            do {
                entry = *(PageRow *)p;
                if ((entryFlags & 1) != 0) {
                    notFound = 0;
                    break;
                }
                if (DAT_0048a75c == i) {
                    i++;
                    notFound = 0;
                    break;
                }
                p -= 0x2e;
                i--;
            } while (i >= 0);
        }

        if (notFound != 0)
            FatalError(0, 0x1047);

        if (i <= fromPage) {
            p = base + 0x84c + i * 0x2e;
            do {
                entry = *(PageRow *)p;
                st = ResolveDisplayAttachSlot(DAT_0048a754, entryChanId, &pNodeEntry);
                if (st != 0)
                    FatalError(st, 0x1048);

                if (DAT_0048a752 != 0 && i + 1 < *(int *)(base + 0x848)
                    && IsDisplayAttachLoaderRunning((int)DAT_0048a754) != 0) {
                    DrainDisplayAttachSlotsForContext((int)DAT_0048a754);
                    next = *(PageRow *)(p + 0x2e);
                    st = ActivateDisplayAttachSlot(DAT_0048a754, nextChanId);
                    if (st != 0)
                        FatalError(st, 0x1049);
                }

                DecodePageNodeImage(pNodeEntry, (HotspotBuf *)trigWork);

                DF_TRACE("NavigateToStagePage: page=%d pageIndex=%d "
                         "ctx=%08x entryChanId=%d flag2(a750)=%d",
                         i, fromPage, DAT_0048a754, (int)entryChanId, (int)DAT_0048a750);

                if (DAT_0048a750 != 0) {
                    st = DetachRenderChild((int)DAT_0048a754, entryChildId);
                    if (st != 0)
                        FatalError(st, 0x104a);
                } else {
                    st = ReleaseRenderChild((int)DAT_0048a754, entryChildId);
                    if (st != 0)
                        FatalError(st, 0x104b);
                }

                p += 0x2e;
                i++;
            } while (i <= fromPage);
        }

        UnlockResObject(DAT_0048a760);
        DAT_0048a75c = fromPage;
        BumpSceneOverlayModeFloor(2);
    }

    DAT_0048a758 = fromPage;

#undef entryFlags
#undef entryChanId
#undef entryChildId
#undef nextChanId
}


/* Copies the flat page row named pName (the 0x2e-byte record, found by FlatPageNameToIndex) into
 * pOut. Returns 10 if there is no such page. */
short __cdecl LookupFlatEntry(byte *pName, uint *pOut)  /* 0x00446fd0 */
{
    /* the 46-byte flat-page row, copied as one object (rep movsd 11 + movsw) */
    typedef struct { short w[23]; } FlatRow46;
    int idx;

    idx = FlatPageNameToIndex(pName);
    if (idx < 0)
        return 10;
    *(FlatRow46 *)pOut = *(FlatRow46 *)(idx * 0x2e + 0x84c + GetResDataPtr(DAT_0048a760));
    UnlockResObject(DAT_0048a760);
    return 0;
}


/* The 0-based index of the flat page named pName (case-insensitive: the table at DAT_0048a760
 * +0x84c, rows of 0x2e bytes, the name at +0x1e), or -1 if there is none or no stage is open. */
int __cdecl FlatPageNameToIndex(byte *pName)  /* 0x00447030 */
{
    int   result;
    int   base;
    byte *p;
    int   i;
    int   count;

    result = -1;
    if (gStage.active == 0)
        return -1;
    base  = GetResDataPtr(DAT_0048a760);
    count = *(int *)(base + 0x848);
    p     = (byte *)(base + 0x84c);
    for (i = 0; i < count; p += 0x2e, i++) {
        if (PascalStringsEqualNoCase(p + 0x1e, pName) != 0) {
            result = i;
            break;
        }
    }
    UnlockResObject(DAT_0048a760);
    return result;
}


/* Finds the button pName in the stage child childId (FindButtonIndexByName) and copies its 0x20-byte
 * row to pOut. Status 0x0b if no stage is open, 10 if the button is not found; fatal on attach and
 * detach errors. */
short __cdecl LookupButtonEntry(uint childId, byte *pName, uint *pOut)  /* 0x004470c0 */
{
    /* the 32-byte button row, copied as one object (rep movsd, ecx = 8) */
    typedef struct { uint w[8]; } ButtonRow32;
    short  st;
    int    idx;
    int   *hButtonList;  /* local_4 */

    if (gStage.active == 0)
        return 0xb;

    st = AttachRenderChild((int *)DAT_0048a754, childId, &hButtonList);
    if (st != 0)
        FatalError(st, 0x104c);

    idx = FindButtonIndexByName(hButtonList, pName);
    if (idx < 0) {
        st = DetachRenderChild((int)DAT_0048a754, childId);
        if (st != 0)
            FatalError(st, 0x104d);
        return 10;
    }

    *(ButtonRow32 *)pOut = *(ButtonRow32 *)(idx * 0x20 + 0x408 + GetResDataPtr(hButtonList));
    UnlockResObject(hButtonList);

    st = DetachRenderChild((int)DAT_0048a754, childId);
    if (st != 0)
        FatalError(st, 0x104e);
    return 0;
}


/* The index of the button named pName (case-insensitive) in the button list of the node pNodeHandle
 * (rows of 0x20 bytes from +0x408, the name at +0x10), or -1. Returns 0xb, as the exe does, if no
 * stage is open. */
int __cdecl FindButtonIndexByName(int *pNodeHandle, byte *pName)  /* 0x004471B0 */
{
    int   result;
    int   base;
    byte *p;
    int   i;
    int   count;

    result = -1;
    if (gStage.active == 0)
        return 0xb;
    base  = GetResDataPtr(pNodeHandle);
    count = *(int *)(base + 0x404);
    p     = (byte *)(base + 0x408);
    for (i = 0; i < count; p += 0x20, i++) {
        if (PascalStringsEqualNoCase(p + 0x10, pName) != 0) {
            result = i;
            break;
        }
    }
    UnlockResObject(pNodeHandle);
    return result;
}

