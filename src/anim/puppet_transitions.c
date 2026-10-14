/* puppet_transitions.c -- TI.EXE 0x0043ea00..0x00441a80: the puppet file, Puppet* commands and track playback. */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <mmsystem.h>   /* timeGetTime */
#include "df_protos.h"
#include "topics/anim.h"
#include "topics/scriptcmd.h"
#include "topics/scriptprop.h"
#include "topics/scene.h"
#include "topics/tick.h"
#include "topics/multimedia.h"

static int RunPuppetEventModalLoop(int timeoutTicks);


/* The puppet's bevel table (the text-entry rows): BevelRow (260 bytes) and BevelTable (0x518 bytes)
 * at 0x0048af08. Storage is one region object (gBevelCount_r, short[652]); the types are views of it.
 * ScriptCmd_PuppetEvent copies all 0x518 bytes into gBevelTableSnapshot (int[326] at 0x0048b428), where
 * int[1] is rows[0].id and `selected` is the high half of int[0]. */
#pragma pack(push, 1)
typedef struct BevelRow { int id; byte name[256]; } BevelRow;   /* 260 */
typedef struct BevelTable {
    short    count;      /* +0 gBevelCount */
    short    selected;   /* +2 DAT_0048af0a (-1 none) */
    BevelRow rows[5];    /* +4 gBevelTable (int[325]); DAT_0048af10 = rows[0].name */
} BevelTable;            /* 0x518 */
#pragma pack(pop)

#include "topics/savestate.h"

/* openpuppetfile (opcode 12018): opens the 'PUPP' file named by its one argument and sends
 * the open message. Error 0x2d if a puppet is already open. Reinstalls gCursor first, and
 * reshows the set view if gPuppetGrab is set. */
short __cdecl ActionCmd_OpenPuppetFile(int pCtx, short *pArgBlock,  /* 0x0043ea00 */
                                  int *pTable, int *pSubSkip)
{
    short  status;
    short argResult[4];

    if (gPuppet.active != 0)
        return 0x2d;   /* the error is the LOW word of the status */

    InstallCursor(gCursor);
    if (gPuppetGrab != 0)
        ReshowSetViewIfIdle();

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                               (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, (short *)gScratchStr2);
    if (status != 0)
        return status;

    status = ValidateDescriptorPath(gScratchStr2, 0x50555050)   /* 'PUPP' */;
    if (status != 0)
        return status;

    status = OpenPuppetNode(gScratchStr2, (short *)&gPuppet);
    if (status != 0)
        return status;

        return SendOpenPuppetMessage();
}


/* Opens the puppet file pDesc into the guard (gPuppet): takes its pending render node and
 * attaches child 0 (DAT_0048a810, the header). Returns 0x38 if the header tag is not 0x40000
 * and 0x39 unless its rect is the scene's width and 0x78 less than its height; either way the
 * node is released again. Otherwise attaches children 1..3 and the header's own child, marks
 * the guard live, probes every child the 0x0048a808 list names, attaches the quad variant for
 * gPuppetBase's track (DAT_0048a818), clears its frame slots and resets the bevel table and
 * voice queue. A failed attach or detach is fatal (0x1058..0x1065). */
short __cdecl OpenPuppetNode(byte *pDesc, short *pSceneGuard) /* 0x0043eab0 */
{
    uint *pChildEntry;
    int iData;
    int i1;
    int *piCtx;
    int status;
    uint uVar4;
    int pChanData;
    int rowsLeft;
    uint *pTrackSrc;
    uint *puVar12;
    short *pChanCount;
    short *pFrameSlot;
    SceneViewport r;
    uint scratchChild;  /* local_13c */
    uint trackEntry[78];  /* local_138 */
    int i2;
    int kf;
    int *piRow;
    typedef struct { uint d[78]; } Row138_;

    piCtx = (int *)(pSceneGuard + 2);
    status = TakeMatchingPendingRenderNode(pDesc, piCtx);
    if (status != 0)
        FatalError(status, 0x1058);

    status = AttachRenderChild((int *)*piCtx, 0u, (int **)(void *)&DAT_0048a810);
    if (status != 0)
        FatalError(status, 0x1059);

    piRow = (int *)GetResDataPtr(DAT_0048a810);
    if (*piRow != 0x40000) {
        UnlockResObject((int *)DAT_0048a810);
        status = DetachRenderChild(*piCtx, 0u);
        if (status != 0)
            FatalError(status, 0x105a);
        status = DeferRenderNodeClose(*piCtx);
        if (status != 0)
            FatalError(status, 0x105b);
        return 0x38u;
    }

    r = *(SceneViewport *)((char *)piRow + 0x22);
    if (r.h - r.x - gSceneH != -0x78 || r.w - r.y != gSceneW) {
        UnlockResObject((int *)DAT_0048a810);
        status = DetachRenderChild(*piCtx, 0u);
        if (status != 0)
            FatalError(status, 0x105c);
        status = DeferRenderNodeClose(*piCtx);
        if (status != 0)
            FatalError(status, 0x105d);
        return 0x39u;
    }

    status = AttachRenderChild((int *)*piCtx, 1u, (int **)(void *)&DAT_0048a80c);
    if (status != 0)
        FatalError(status, 0x105e);
    status = AttachRenderChild((int *)*piCtx, 2u, (int **)(void *)&DAT_0048a808);
    if (status != 0)
        FatalError(status, 0x105f);
    status = AttachRenderChild((int *)*piCtx, 3u, (int **)(void *)&DAT_0048a814);
    if (status != 0)
        FatalError(status, 0x1060);

    *pSceneGuard = 1;
    pSceneGuard[1] = 1;
    DF_TRACE("trans: ARMED guard=%p node=%p a808=%p a80c=%p a810=%p a814=%p",
             (void *)pSceneGuard, (void *)(intptr_t)*piCtx, (void *)DAT_0048a808,
             (void *)DAT_0048a80c, (void *)DAT_0048a810, (void *)DAT_0048a814);
    *(uint *)((char *)pSceneGuard + 8) = *(uint *)((char *)piRow + 0x85a);

    status = AttachRenderChild((int *)*(int *)(pSceneGuard + 2), *(unsigned *)((char *)piRow + 0x85a),
                             (int **)((char *)pSceneGuard + 12));
    if (status != 0)
        FatalError(status, 0x1061);

    CopyPascalString((byte *)((char *)piRow + 0x85e), (byte *)((char *)pSceneGuard + 0x24));
    UnlockResObject((int *)DAT_0048a810);

    iData = GetResDataPtr(DAT_0048a808);
    pChildEntry = (uint *)(iData + 0x18);
    for (i1 = 0; i1 < *(short *)(iData + 0x16); i1++) {
            status = AttachRenderChild((int *)*piCtx, *pChildEntry, (int **)(void *)&scratchChild);
            if (status != 0)
                FatalError(status, 0x1062);
            status = DetachRenderChild(*piCtx, *pChildEntry);
            if (status != 0)
                FatalError(status, 0x1063);
            pChildEntry += 10;
    }
    UnlockResObject(DAT_0048a808);

    status = FindPuppetTrackIndexByName(&gPuppetBase);
    kf = (int)status;
    if (kf < 0)
        kf = 0;

    iData = GetResDataPtr(DAT_0048a810);
    pTrackSrc = (uint *)(iData + 0x870 + kf * 0x138);
    *(Row138_ *)trackEntry = *(Row138_ *)pTrackSrc;   /* one struct copy: rep movsd, ecx = 0x4e */
    UnlockResObject((int *)DAT_0048a810);

    pChanData = GetResDataPtr(DAT_0048a814);
    *(short *)(pChanData + 0x14) = *(short *)trackEntry;

    status = AttachRenderChild((int *)*piCtx, (uint)GetQuadVariantChildIndexOrFatal(pChanData), (int **)(void *)&DAT_0048a818);
    if (status != 0)
        FatalError(status, 0x1064);

    pChanData = GetResDataPtr(DAT_0048a818);
    rowsLeft = 11;
    pChanCount = (short *)(pChanData + 0x16);
    do {
        if (*pChanCount > 0x20)
            FatalError(0, 0x1065);
        i2 = 0;
        if (0 < *pChanCount) {
            pFrameSlot = pChanCount + 0x43;
            do {
                *(int *)pFrameSlot = 0;
                pFrameSlot += 2;
                i2++;
            } while (i2 < *pChanCount);
        }
        pChanCount += 0x83;
        rowsLeft--;
    } while (rowsLeft != 0);

    UnlockResObject(DAT_0048a818);
    UnlockResObject(DAT_0048a814);
    ((short *)gBevelTableSnapshot)[1] = -1;     /* two word stores (0x0043ee27/34), not one dword */
    ((short *)gBevelTableSnapshot)[0] = 0;
    gBevelCount = 0;
    gQueuedVoiceCount = 0;
    gTrackInterrupted = 0;
    return 0;
}


/* closepuppetfile (opcode 12042): sends the puppet its close message; if that succeeds, the
 * puppet is still open and the scene (+0x530 of DAT_0048a7fc) has not changed meanwhile,
 * closes the puppet node, reinitialises the cursor and bumps the overlay mode floor to 2.
 * Error 0x2d if no puppet is open. */
short __cdecl ActionCmd_ClosePuppetFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043EEC0 */
{
    int  iSaved;
    short status;

    (void)pCtx; (void)pArgBlock; (void)pTable;
    *pSubSkip = 0;

    if (gPuppet.active == 0)
        return 0x2d;

    iSaved = *(int *)(DAT_0048a7fc + 0x530);
    status = SendClosePuppetMessage();
    if (status == 0) {
        if (gPuppet.active == 0)
            return 0;
        if (*(int *)(DAT_0048a7fc + 0x530) != iSaved)
            return 0;
        ClosePuppetNode((short *)&gPuppet);
        QdInitCursor();
        BumpSceneOverlayModeFloor(2);
        return 0;                       /* xor ax,ax: the bump's status is dropped */
    }
    return status;
}


/* Undoes OpenPuppetNode if the guard is live: detaches the puppet frames, then the render
 * node's children in reverse order of attachment (the quad variant chosen from DAT_0048a814,
 * 3, 2, 1, the guard's own child at +8, 0), defers the node's close and clears the guard.
 * Any failure is fatal (0x1066..0x106c). */
void __cdecl ClosePuppetNode(short *pGuard)  /* 0x0043ef40 */
{
    short deferStatus;
    int   variantChildIdx;
    short  detachStatus;

    DF_TRACE("trans: TEARDOWN guard=%p active=%d a810=%p",
             (void *)pGuard, (int)*pGuard, (void *)DAT_0048a810);

    if (*pGuard != 0) {
        DetachPuppetFrames();
        variantChildIdx = GetResDataPtr(DAT_0048a814);
        variantChildIdx = GetQuadVariantChildIndexOrFatal(variantChildIdx);
        detachStatus = DetachRenderChild(*(int *)(pGuard + 2), variantChildIdx);
        if (detachStatus != 0)
            FatalError(detachStatus, 0x1066);
        UnlockResObject(DAT_0048a814);
        detachStatus = DetachRenderChild(*(int *)(pGuard + 2), 3);
        if (detachStatus != 0)
            FatalError(detachStatus, 0x1067);
        detachStatus = DetachRenderChild(*(int *)(pGuard + 2), 2);
        if (detachStatus != 0)
            FatalError(detachStatus, 0x1068);
        detachStatus = DetachRenderChild(*(int *)(pGuard + 2), 1);
        if (detachStatus != 0)
            FatalError(detachStatus, 0x1069);
        detachStatus = DetachRenderChild(*(int *)(pGuard + 2), *(int *)(pGuard + 4));
        if (detachStatus != 0)
            FatalError(detachStatus, 0x106a);
        detachStatus = DetachRenderChild(*(int *)(pGuard + 2), 0);
        if (detachStatus != 0)
            FatalError(detachStatus, 0x106b);
        deferStatus = DeferRenderNodeClose(*(int *)(pGuard + 2));
        if (deferStatus != 0)
            FatalError(deferStatus, 0x106c);
        *pGuard = 0;
    }
}


/* Sends the open puppet's first puppet the command `"<name>", openpuppet()` through
 * RunSendToPuppetCommand. Error 0x2d if no puppet file is open. */
short __stdcall SendOpenPuppetMessage(void) /* 0x0043f070 */
{
    if (gPuppet.active == 0)
        return 0x2du;

    CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
    CopyFirstPuppetName(&gScratchStr2);
    AppendPascalString(&gScratchStr2, &gScratchStr);
    /* An Append at 0x0043f0c5, not a Copy: as a Copy the parser saw a bare quote (error 0x26). */
    AppendPascalString((byte *)UpdatePascalLength(kStrQuoteCommaOpenpuppetCall), &gScratchStr);
    return RunSendToPuppetCommand(&gScratchStr);
}


/* SendOpenPuppetMessage's twin: sends `"<name>", closepuppet()`. Error 0x2d if no puppet
 * file is open. */
short __stdcall SendClosePuppetMessage(void)  /* 0x0043f0f0 */
{

    if (gPuppet.active == 0)
        return 0x2d;

    CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
    CopyFirstPuppetName(&gScratchStr2);
    AppendPascalString(&gScratchStr2, &gScratchStr);
    AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteCommaClosepuppetCall), &gScratchStr);   /* lookup inline: the other arguments are pushed before it is called */
    return RunSendToPuppetCommand(&gScratchStr);
}


/* puppetbevel (opcode 12044): appends a bevel row {id = the int second argument, name = the
 * string first} to the puppet's bevel table. Error 0x2d if no puppet is open, 0x0e if the
 * second argument is not an int (type 4), 0x2e when all 5 rows are taken.
 * Port note: four parameters, as the disassembly reads param 4 as the skip out-pointer
 * (port-bug-classes.md #2). */
short __cdecl ActionCmd_PuppetBevel(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043f170 */
{
    short  u;
    short  arg1Result[4];
    short  arg2Result[4];   /* out2 = [ESP+8] */
    /* One row image (+0 id, +4 Pascal name), copied as one object: rep movsd, ecx = 0x41 */
    byte   entryBuf[0x104];

    if (gPuppet.active == 0)
        return 0x2d;

    u = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)arg1Result, (int *)arg2Result, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(arg1Result, gScratchStr);
    if (u != 0)
        return u;
    if (arg2Result[0] != 4)
        return 0x0e;
    if (gBevelCount >= 5)
        return 0x2e;
    CopyPascalString(gScratchStr, entryBuf + 4);
    *(int *)entryBuf = *(int *)(arg2Result + 1);
    gBevel.rows[gBevelCount] = *(BevelRow *)entryBuf;
    /* volatile: the exe does `inc word ptr [gBevelCount]` after the copy (no qualifier in the generator) */
    (*(volatile short *)&gBevelCount)++;
    return 0;
}


/* puppetevent (opcode 20028): runs the puppet's modal event loop for the int argument's tick
 * count and returns its result as an int; then snapshots the whole bevel table (0x146 dwords,
 * one rep movsd) for PollPuppetEventInput's replay and empties the voice queue. Error 0x2d
 * unless a puppet is open and DAT_0048a7fa is set, 0x0e if the argument is not an int. */
short __cdecl ScriptCmd_PuppetEvent(int *pCtx, short *pArgBlock, int *pTable,
                                                            short *pResult, int *pSkipCount) /* 0x0043F260 */
{
    short   st;
    short   argBuf[4];
    int     ret;

    if (gPuppet.active == 0 || DAT_0048a7fa == 0)
        return 0x2d;

    gTrackInterrupted = 0;
    st = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)argBuf, pSkipCount);
    if (st != 0)
        return st;
    if (argBuf[0] != 4)
        return 0x0e;

    DAT_0048af0a = -1;                  /* no row selected yet */

    ret = RunPuppetEventModalLoop(*(int *)(argBuf + 1));
    *pResult              = 4;
    pResult[3]            = 0;
    *(int *)(pResult + 1) = ret;
    *(BevelTable *)gBevelTableSnapshot = gBevel;
    gQueuedVoiceCount = 0;
    return 0;
}


/* puppetspeak (opcode 12043): plays the puppet track named by the string argument. With no
 * such track it shows the text as a subtitle instead and waits length * gSpeakTicksPerChar
 * ticks, at least 60 (one second at 60 Hz), pumping messages. A found track is queued in
 * gQueuedVoiceIds (up to 3) and played unless gTrackInterrupted is set. Error 0x2d unless
 * a puppet is open and DAT_0048a7fa is set. */
short __cdecl ActionCmd_PuppetSpeak(int pCtx, short *pArgBlock,  /* 0x0043f320 */
                                       int *pTable, int *pSubSkip)
{
    short  st;
    short  key;
    int    n;
    short  argResult[4];

    if ((gPuppet.active == 0) || (DAT_0048a7fa == 0))
        return 0x2d;

    st = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                            (int *)argResult, pSubSkip);
    if (st != 0)
        return st;
    st = TakeScriptString(argResult, gScratchStr);
    if (st != 0)
        return st;
    key = FindPuppetTrackIndexByName(gScratchStr);
    if (key < 0) {
        DrawSubtitleText(gScratchStr);
        /* one register in the exe: the duration becomes the deadline */
        n = (byte)gScratchStr[0] * gSpeakTicksPerChar;
        if (n < 0x3c)
            n = 0x3c;
        n += GetTick60Hz();
        while ((int)GetTick60Hz() < n)
            PumpMessages();
        return 0;
    }
    if (gQueuedVoiceCount < 3) {
        gQueuedVoiceIds[gQueuedVoiceCount] = key;
        gQueuedVoiceCount++;
    }
    if (gTrackInterrupted != 0)
        return 0;
    PlayPuppetTrack(key);
    return 0;
}


/* puppetclear (opcode 12041): empties the bevel table ActionCmd_PuppetBevel fills.
 * Error 0x2d if no puppet is open. */
short __cdecl ActionCmd_PuppetClear(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043F440 */
{
    (void)pCtx; (void)pArgBlock; (void)pTable;
    if (gPuppet.active == 0)
        return 0x2d;
    *pSubSkip = 0;                      /* after the check, as the exe stores it */

    gBevelCount = 0;
    return 0;
}


/* puppetgrab (opcode 12074): sets gPuppetGrab from its one boolean (type 2) argument;
 * error 0x0e for any other type.
 *
 * Port note: an invented TakeScriptString call here rejected boot()'s literal `true` and
 * aborted the boot script; the error was also in the high half of the status word. */
short __cdecl
ActionCmd_PuppetGrab(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip) /* 0x0043f470 */
{
    short   u;
    short  a[4];

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)a, pSubSkip);
    if (u != 0)
        return u;
    if (a[0] != 2)
        return 0xe;
    gPuppetGrab = (short)*(int *)(a + 1);
    return 0;
}


/* puppetsubtitle (opcode 12084): draws its string argument as the subtitle. */
short __cdecl
ActionCmd_PuppetSubtitle(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043f4c0 */
{
    short u;
    short a[4];

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)a, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a, gScratchStr);
    if (u != 0)
        return u;
    DrawSubtitleText(&gScratchStr);
    return 0;
}


/* puppetbase (opcode 16053, statement form): sets gPuppetBase, the name of the puppet's base
 * track, from its string argument; error 0x1a if it is 0x20 characters or longer. */
short __cdecl
ActionCmd_PuppetBase(int pCtx, short *pArgBlock, int *pTable,
                                              int *pSubSkip)  /* 0x0043f520 */
{
    short  u;
    short pb[4];

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)pb, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(pb, (byte *)gScratchStr);
    if (u != 0)
        return u;
    if (gScratchStr[0] >= 0x20)        /* cmp 0x20 / jb, as the exe has it */
        return 0x1a;
    CopyPascalString(&gScratchStr, &gPuppetBase);
    return 0;
}


/* puppetbase (opcode 16053, value form): returns gPuppetBase as a string. */
short ScriptCmd_PuppetBase(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x0043F590 */
{
    *pSkipCount = 0;
    return AllocScriptString(&gPuppetBase, pResult);   /* the callee's status is the result */
}


/* Runs the command text pCmd (a Pascal string) as a sendtopuppet statement, in a method
 * frame named kStrPuppetMessage with the "System:" owner. Same spine as 0x00446460. */
short __cdecl RunSendToPuppetCommand(byte *pCmd)                 /* 0x0043f5b0 */
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

    CopyPascalString((byte *)UpdatePascalLength(kStrPuppetMessage), frame.buf0);
    CopyPascalString((byte *)UpdatePascalLength(kStrPuppetMessage), frame.buf2);
    CopyPascalString((byte *)UpdatePascalLength(&kStrSystemColon), frame.buf1);

    sendStatus = (short)ActionCmd_SendToPuppet((int)&frame, rowBase, (int *)0, &skipOut);
    UnlockResObject(ph);
    FreeResObject(ph);
    return sendStatus;
}


/* sendtopuppet (opcode 12045): the statement form of sendtopuppetfx, with no result. */
short __cdecl ActionCmd_SendToPuppet(int pCtx, int pArgBlock,
                                          int *pTable, int *pSubSkip)  /* 0x0043f690 */
{
    return ScriptCmd_SendToPuppetFx((int *)pCtx, pArgBlock,
                                  pTable, NULL, pSubSkip);
}


/* sendtopuppetfx (opcode 20086): `"<puppet>", method(args)`. Resolves the puppet name to its
 * selector, fetches that puppet's method code from the puppet node (DAT_0048a7fc) and the
 * boot node's (gBootNode, selector 2), and runs the method in a two-frame chain ("Puppet
 * Script", then "Post Script"/"System"). pResult goes to EvalMethodCall; the statement form
 * passes NULL. Error 0x2d if no puppet is open. */
short __cdecl ScriptCmd_SendToPuppetFx(int *pCtx, int pArgBlock,  /* 0x0043f6b0 */
                                    int *pTable, void *pResult,
                                    int *pSubSkip)
{
    short          status;
    int            skip;           /* both evaluators' skip count (one slot) */
    short          args[4];
    unsigned int   selector;
    int            start;          /* the parameter walks; this keeps its start */
    /* One array, as in SendToCastFx: frames at ebp-0x9c and ebp-0x4e. Port note: once ~14
     * separate locals, which broke EvalMethodCall's frame[0]->frame[1] walk (class 3). */
    MethodCallFrame f[2];
    int           *h1, *h2;
    MethodCallFrame *pf;

    if (gPuppet.active == 0)
        return 0x2d;

    start = pArgBlock;
    status = ProcessScriptBlock((int)pCtx, (short *)pArgBlock, pTable,
                               (int *)args, &skip);
    if (status != 0)
        return status;

    pArgBlock += skip * 8;
    if (*(short *)pArgBlock != 0xfb4)   /* the 0xfb4 separator token, 8 bytes */
        return 0x1c;
    pArgBlock += 8;

    status = TakeScriptString(args, gScratchStr);
    if (status != 0)
        return status;

    status = LookupPuppetSelectorByName(gScratchStr, &selector);
    if (status != 0)
        return status;

    pf = &f[0];
    pf->pOwner     = DAT_0048a7fc;
    pf->selector   = selector;
    pf->hMethod    = h1 = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h1);
    CopyPascalString((byte *)UpdatePascalLength(kStrPuppetScriptColon), pf->buf1); /* "Puppet Script: " */
    CopyPascalString(gScratchStr, pf->buf0);
    CopyPascalString(gScratchStr, pf->buf2);
    pf->isLast = 0;

    pf = &f[1];
    pf->pOwner     = (int *)gBootNode;
    pf->selector   = 2u;
    pf->hMethod    = h2 = AcquireMethodHandle((int *)pf->pOwner, 2u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h2);
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), pf->buf1); /* "System: " */
    CopyPascalString((byte *)UpdatePascalLength(kStrPostScript), pf->buf0); /* "Post Script" */
    CopyPascalString(gScratchStr, pf->buf2);
    pf->isLast = 1;

    status = EvalMethodCall((short *)&f[0], (short *)pCtx, (short *)pArgBlock,
                           pTable, pResult, &skip);

    UnlockResObject(f[0].hMethod);
    ReleaseMethodHandle((int)f[0].hMethod);
    UnlockResObject(f[1].hMethod);
    ReleaseMethodHandle((int)f[1].hMethod);

    pArgBlock += skip * 8;
    *pSubSkip = (pArgBlock - start) >> 3;
    return status;
}


/* puppetscript (opcode 12046): resolves the named puppet's selector and hands
 * ClearScriptResult the puppet node, that selector and the label "Puppet Script: <name>".
 * Error 0x2d if no puppet is open. */
short __cdecl ActionCmd_PuppetScript(int pCtx, short *pArgBlock,  /* 0x0043f900 */
                                             int *pTable, int *pSubSkip)
{
    short         status;
    unsigned int selector;    /* validated but unused beyond the check */
    short        args[4];

    if (gPuppet.active == 0)
        return 0x2d;

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)args, pSubSkip);
    if (status == 0) {
        status = TakeScriptString(args, gScratchStr);
        if (status == 0) {
            status = LookupPuppetSelectorByName(gScratchStr, &selector);
            if (status == 0) {
                CopyPascalString((byte *)UpdatePascalLength(kStrPuppetScriptColon),
                                 gScratchStr2);
                AppendPascalString(gScratchStr, gScratchStr2);
                ClearScriptResult(DAT_0048a7fc, (int)selector, gScratchStr2, 0);   /* frame +0 is the selector */
                return 0;
            }
        }
    }
    return status;
}


/* countpuppets (opcode 20046): the number of puppets in the open file, the short at byte 22
 * of the 0x0048a808 list's 24-byte header. Error 0x2d if no puppet is open. */
short __cdecl ScriptCmd_CountPuppets(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x0043F9C0 */
{
    uint hdr[6];

    *pSkipCount = 0;                    /* skip count cleared before the check */
    if (gPuppet.active == 0)
        return 0x2d;

    memcpy(hdr, (void *)GetResDataPtr(DAT_0048a808), sizeof(hdr));
    UnlockResObject(DAT_0048a808);

    *pResult              = 4;
    /* the short at byte 22 (MOVSX in the exe), not the low half of dword 5 */
    *(int *)(pResult + 1) = (int)*(short *)((char *)hdr + 22);
    pResult[3]            = 0;
    return 0;
}


/* indextopuppet (opcode 20047): the name of puppet n (1-based) in the open file: the list's
 * 40-byte rows start at +0x18 and hold the name at +8. Error 0x2d if no puppet is open, 0x0e
 * if the argument is not an int, 10 if it is out of range. */
short __cdecl ScriptCmd_IndexToPuppet(int *pCtx, int pArgBlock, int *pTable,
                                                        short *pResult, int *pSubSkip) /* 0x0043FA30 */
{
    short   status;
    short  argBuf[4];
    int    rowIdx;
    int   *pData;
    uint   hdr[6];
    short  rowCount;
    uint   row[10];
    byte  *pRowBytes;

    if (gPuppet.active == 0)
        return CONCAT22(0, 0x2d);

    status = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable, (int *)argBuf, pSubSkip);
    if (status != 0)
        return status;

    pData = (int *)GetResDataPtr(DAT_0048a808);
    memcpy(hdr, pData, sizeof(hdr));
    UnlockResObject(DAT_0048a808);

    if (argBuf[0] != 4)
        return (status & 0xFFFF0000U) | 0xEU;

    /* +22, see CountPuppets */
    if (*(int *)((char *)argBuf + 2) <= 0 || *(int *)((char *)argBuf + 2) > *(short *)((char *)hdr + 22))
        return 10;

    pData = (int *)GetResDataPtr(DAT_0048a808);
    pRowBytes = (byte *)pData + *(int *)((char *)argBuf + 2) * 0x28 - 0x10;
    memcpy(row, pRowBytes, sizeof(row));
    UnlockResObject(DAT_0048a808);

    return AllocScriptString((byte *)&row[2], pResult);
}


/* currentpuppet (opcode 20049): the name OpenPuppetNode copied from the puppet header
 * (gPuppet +0x24, DAT_0048a81c), or "none" when no puppet is open. Also leaves the leaf of
 * the puppet node's path (+0x24 of DAT_0048a7fc) in gScriptResult.
 *
 * Port note: DAT_0048a81c is passed by address; passed by value (an int symbol) it handed
 * the name's first four bytes to AllocScriptString as a pointer. */
short __cdecl ScriptCmd_CurrentPuppet(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x0043FB20 */
{
    uint status;

    *pSkipCount = 0;

    if (gPuppet.active != 0) {
        status = AllocScriptString(&DAT_0048a81c, pResult);
        CopyPascalString((byte *)((char *)DAT_0048a7fc + 0x24), gScriptResult);
        TruncatePathToLeaf(gScriptResult);
        return status;
    }
    status = AllocScriptString((byte *)UpdatePascalLength(&kStrNone), pResult);
    return status;
}


/* puppetvisible (opcode 16050, statement form): sets gPuppet's visible flag (DAT_0048a7fa)
 * from its boolean argument. Error 0x2d if no puppet is open, 0x0e if not a boolean. */
short __cdecl
ActionCmd_PuppetVisible(int pCtx, short *pArgBlock, int *pTable,
                                                int *pSubSkip)  /* 0x0043fb90 */
{
    short  u;
    short out[4];

    if (gPuppet.active == 0)
        return 0x2D;
    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)out, pSubSkip);
    if (u != 0)
        return u;
    if (out[0] != 2)
        return 0x0E;
    DAT_0048a7fa = (short)*(int *)(out + 1);   /* the exe loads the int payload, stores a word */
    return 0;
}


/* puppetvisible (opcode 16050, value form): the visible flag as a boolean (type 2); false
 * when no puppet is open. */
short ScriptCmd_PuppetVisible(int *pCtx, short *pArgBlock, int *pTable,
                              short *pResult, int *pSkipCount) /* 0x0043FBF0 */
{
    *pSkipCount = 0;
    if (gPuppet.active == 0) {
        *(int *)(pResult + 1) = 0;
    } else {
        *(int *)(pResult + 1) = (int)DAT_0048a7fa;
    }
    pResult[3] = 0;
    *pResult = 2;
    return 0;
}


/* puppetparam (opcode 16049, statement form): `<selector>, <value>`, both ints. Selector 1..10
 * stores the value in one of the ten puppet parameters, gClutSyncFirst..gTextIndent;
 * error 0x0e for a non-int or another selector. Argument 1 is the selector (the disassembly
 * settles the order; Ghidra's decompile does not).
 *
 * Port note: once decoded with four arguments, which failed every call with 0x1c and aborted
 * boot(). pTable/pSubSkip are explicit; the original borrows them from the dispatcher's frame. */
short __cdecl
ActionCmd_PuppetParam(int pCtx, int pArgBlock,
                                        int *pTable, int *pSubSkip)  /* 0x0043fc30 */
{
    short s1;
    short oSel[4], oVal[4];
    int   slot;

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)oSel, (int *)oVal, pSubSkip);
    if (s1 != 0)
        return s1;
    if (oSel[0] != 4)
        return 0x0E;
    if (oVal[0] != 4)
        return 0x0E;
    slot = *(int *)&oSel[1];
    switch (slot) {
    case 1:  gClutSyncFirst = (short)*(int *)&oVal[1]; return 0;
    case 2:  gClutSyncEnd = (short)*(int *)&oVal[1]; return 0;
    case 3:  gTextColor = (short)*(int *)&oVal[1]; return 0;
    case 4:  gTextHighlightColor = (short)*(int *)&oVal[1]; return 0;
    case 5:  gTextFont = (short)*(int *)&oVal[1]; return 0;
    case 6:  gTextSize = (short)*(int *)&oVal[1]; return 0;
    case 7:  gPuppetParam7 = (short)*(int *)&oVal[1]; return 0;
    case 8:  gPuppetNoTimeout = (short)*(int *)&oVal[1]; return 0;
    case 9:  gSpeakTicksPerChar = (short)*(int *)&oVal[1]; return 0;
    case 10: gTextIndent = (short)*(int *)&oVal[1]; return 0;
    default: return 0x0E;
    }
}


/* puppetparam (opcode 16049, value form): puppet parameter n (1..10) as an int; error 0x0e
 * for a non-int or out-of-range argument. */
short ScriptCmd_PuppetParam(int *pCtx, short *pArgBlock, int *pTable,
                              short *pResult, int *pSkipCount) /* 0x0043FD70 */
{
    short  status;
    int   value;
    /* Port note: one typed result, once two separate locals (type, value) that only MSVC's
     * stack layout made adjacent (class 3). */
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    if (argResult[0] != 4)
        return 0x0e;

    switch (*(int *)&argResult[1]) {
    case 1:  value = (int)gClutSyncFirst; break;
    case 2:  value = (int)gClutSyncEnd; break;
    case 3:  value = (int)gTextColor; break;
    case 4:  value = (int)gTextHighlightColor; break;
    case 5:  value = (int)gTextFont; break;
    case 6:  value = (int)gTextSize; break;
    case 7:  value = (int)gPuppetParam7; break;
    case 8:  value = (int)gPuppetNoTimeout; break;
    case 9:  value = (int)gSpeakTicksPerChar; break;
    case 10: value = (int)gTextIndent; break;
    default: return 0x0e;
    }
    *pResult = 4;
    *(int *)(pResult + 1) = value;
    pResult[3] = 0;
    return 0;
}


/* puppetscramble (opcode 12083): shuffles the bevel rows with 5 * count random pairwise swaps.
 * Error 0x2d if no puppet is open. */
short __cdecl
ActionCmd_PuppetScramble(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043FE60 */
{
    /* rows are swapped as 260-byte objects: three rep movsd, ecx = 0x41 */
    short    k;
    short    a, b;
    BevelRow tmp;

    (void)pCtx; (void)pArgBlock; (void)pTable;
    *pSubSkip = 0;

    if (gPuppet.active == 0)
        return 0x2d;
    if (gBevelCount < 2)
        return 0;
    /* count >= 2, so both draws are 1..count: no range check in the exe */
    for (k = 0; k < gBevelCount * 5; k++) {
        a = (short)RandomUpTo(gBevelCount) - 1;
        b = (short)RandomUpTo(gBevelCount) - 1;
        tmp = gBevel.rows[a];
        gBevel.rows[a] = gBevel.rows[b];
        gBevel.rows[b] = tmp;
    }
    return 0;
}


/* countbevels (opcode 20107): the number of bevel rows. Error 0x2d if no puppet is open. */
short __cdecl ScriptCmd_CountBevels(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x0043FF40 */
{
    *pSkipCount = 0;
    if (gPuppet.active == 0)
        return 0x2Du;

    *(int *)(pResult + 1) = gBevelCount;   /* value first, then the type word: the exe's register split */
    *pResult               = 4;
    pResult[3]             = 0;
    return 0;
}


/* Copies the name of the first puppet in the open file (the 0x0048a808 list, child 2 of a
 * .pup: count at +0x16, 40-byte entries from +0x18, name at +8 of each) to pDest. An empty
 * list is FatalError(0, 0x106d), the "Fatal error at line 4205" box. */
void __cdecl CopyFirstPuppetName(byte *pDest)                  /* 0x0043ff80 */
{
    int    *pHeader;
    byte    hdrCopy[24];
    byte    tailCopy[40];

    pHeader = (int *)GetResDataPtr(DAT_0048a808);
    memcpy(hdrCopy, pHeader, sizeof(hdrCopy));
    if (*(short *)(hdrCopy + 22) <= 0)   /* cmp 0 / jg, as the exe compares */
        FatalError(0, 0x106d);

    memcpy(tailCopy, pHeader + 6, sizeof(tailCopy));
    CopyPascalString(tailCopy + 8, pDest);
    UnlockResObject(DAT_0048a808);
}


/* Finds the puppet named pName (case-insensitive) in the 0x0048a808 list and stores its
 * selector, the entry's first dword, in *pSelectorOut. Returns 10 (no such object) if absent.
 * The exe copies the header and each entry to the stack whole (rep movsd 6 / 0xa).
 *
 * Port note: the count was once read at +20, not +22; it did not cover the named puppets and
 * `sendtopuppet ("before", ...)` failed with status 10 on the way to the purser. */
short __cdecl LookupPuppetSelectorByName(byte *pName, unsigned int *pSelectorOut)  /* 0x0043fff0 */
{
    typedef struct { byte b[24]; } Hdr24;
    typedef struct { int w[10]; } Entry40;
    short    i;
    byte    *pData;
    Hdr24    hdr;
    Entry40  ent;

    pData = (byte *)GetResDataPtr(DAT_0048a808);
    hdr   = *(Hdr24 *)pData;
    for (i = 0; i < *(short *)(hdr.b + 22); i++) {
        ent = *(Entry40 *)(pData + i * 40 + 24);
        /* ent.w[0] = selector, ent.w[2..9] = Pascal name (32 bytes) */
        if (PascalStringsEqualNoCase((byte *)&ent.w[2], pName) != 0) {
            *pSelectorOut = ent.w[0];
            UnlockResObject(DAT_0048a808);
            return 0;
        }
    }
    UnlockResObject(DAT_0048a808);
    return 10;
}


/* Shows the first frame of the base track (gPuppetBase, else track 0) and draws the bevel
 * table. A track entry is 0x138 bytes in the header (DAT_0048a810) from +0x870; its frames
 * are the render child named at +0xc. Fatal if no puppet is open (0x106e) or the child
 * cannot be attached/detached (0x106f/0x1070). void: the exe has no xor eax. */
void __stdcall ShowPuppetBasePose(void)  /* 0x004400a0 */
{
    /* ONE track entry, copied as one struct (rep movsd 0x4e). Port note: as three locals
     * the copy overran and the selector at +0xc read garbage, fatal 0x106f (class 3). */
    typedef struct { byte b[0x138]; } AudioEntry;
    short       trackIdx;
    AudioEntry  entry;
#define pEntryBuf ((short *)&entry)  /* local_138 */
#define trackChildSel (*(uint *)((byte *)&entry + 0x0c))  /* local_12c */
#define hFrameList (*(int **)((byte *)&entry + 0x14))  /* local_124 */

    if (gPuppet.active == 0) {
        FatalError(0, 0x106e);
    }
    trackIdx = FindPuppetTrackIndexByName(&gPuppetBase);
    if (trackIdx < 0) {
        trackIdx = 0;
    }
    DF_TRACE("trans: ShowPuppetBasePose a7f8=%d a7fa=%d key=%d a810=%p",
             (int)gPuppet.active, (int)DAT_0048a7fa, (int)trackIdx, (void *)DAT_0048a810);
    entry = *(AudioEntry *)(GetResDataPtr((void *)DAT_0048a810) + 0x870 + trackIdx * 0x138);
    UnlockResObject((int *)DAT_0048a810);
    trackIdx = AttachRenderChild((int *)DAT_0048a7fc, trackChildSel, &hFrameList);
    if (trackIdx != 0) {
        FatalError(trackIdx, 0x106f);
    }
    SwitchPuppetVariantForTrack(pEntryBuf);
    AdvanceAnimPlaybackFrame(hFrameList, -1, 0, 0);
    trackIdx = DetachRenderChild((int)DAT_0048a7fc, trackChildSel);
    if (trackIdx != 0) {
        FatalError(trackIdx, 0x1070);
    }
    DrawBevelTable((short *)&gBevelCount);
}


/* Plays one puppet track: its voice sample with its frames in step, one frame per 2 ticks
 * (30 per second) from the moment the voice starts, and its text as a subtitle when
 * TrackHasSubtitleText says so. Ends when the sound stops, or on a key ProcessEventKeyInput
 * takes, which stops the channels and sets gTrackInterrupted. Track entries as in
 * ShowPuppetBasePose: +0 flags, +6 frame count, +8 voice id, +12 frame child, +24 text. */
void __cdecl PlayPuppetTrack(short trackId)  /* 0x004401a0 */
{
    typedef struct { byte b[0x138]; } TrackEntry;   /* copied whole: rep movsd, ecx = 0x4e */
    uint     start;       /* tick at the first frame */
    short    frame, prev;
    short    hasText;
    short    err;
    short    eventResult;
    short    eventBuf[10];
    /* ONE track entry. Port note: once six separate locals the copy ran over (class 3). */
    byte     entry[0x138];
#define trackFlags    (*(uint *)(entry + 0))       /* flags / tick seed */
#define trackLen      (*(short *)(entry + 6))      /* frame count */
#define voiceChanId   (*(uint *)(entry + 8))       /* voice channel ID */
#define renderChildId (*(uint *)(entry + 12))      /* render child selector */
#define pNodeEntry    (*(int **)(entry + 20))      /* render node entry ptr (out: AttachRenderChild) */
#define trackText     (entry + 24)                 /* text, Pascal (256); the name follows at +0x118 */
    uint     voiceChan[26];   /* voice channel state buffer (out: InitSoundNodeEntryFromResource) */

    hasText = 0;
    *(TrackEntry *)entry = *(TrackEntry *)(GetResDataPtr(DAT_0048a810) + 0x870 + trackId * 0x138);
    UnlockResObject(DAT_0048a810);

    DF_TRACE("track: id=%d flags=%08x len=%d voiceChan=%u renderChild=%u '%.*s'",
             (int)trackId, trackFlags, (int)trackLen, voiceChanId, renderChildId,
             (int)trackText[0], (const char *)trackText + 1);
    InitSoundNodeEntryFromResource(DAT_0048a7fc, voiceChanId, UpdatePascalLength(&kStrPuppetVoice), voiceChan);  /* "Puppet Voice" */

    err = AttachRenderChild((int *)DAT_0048a7fc, renderChildId, &pNodeEntry);
    if (err != 0)
        FatalError(err, 0x1071);

    SwitchPuppetVariantForTrack((short *)entry);
    if (TrackHasSubtitleText((int)entry) != 0) {
        hasText = 1;
        DrawSubtitleText(trackText);
    }

    frame = 0;
    prev  = -1;
    start = trackFlags;

    for (;;) {
        if (prev != frame) {
            AdvanceAnimPlaybackFrame(pNodeEntry, prev, frame, hasText);
            if (prev < 0) {
                start = GetTick60Hz();
                AssignPrimarySoundChannel((int *)voiceChan);
            }
            prev = frame;
        }

        frame = (short)((int)(GetTick60Hz() - start) / 2);
        if (frame >= trackLen)
            frame = trackLen - 1;

        if (PollEventQueue(8, (unsigned int *)eventBuf) != 0 &&
            ProcessEventKeyInput(eventBuf, &eventResult) != 0) {
            DF_TRACE("track: KEY INTERRUPT key=%d frame=%d/%d", (int)eventBuf[1],
                     (int)prev, (int)trackLen);
            StopSoundChannels(0, 0, 0, 1);
            DF_TRACE("track: channels stopped");
            gTrackInterrupted = 1;
            break;
        }

        if (CheckSoundChannelStopped() != 0)
            break;
    }

    if (prev != (short)(trackLen - 1))
        AdvanceAnimPlaybackFrame(pNodeEntry, prev, trackLen - 1, hasText);

    err = DetachRenderChild((int)DAT_0048a7fc, renderChildId);
    if (err != 0)
        FatalError(err, 0x1072);

    DF_TRACE("track: detached, tearing down voice row");
    TeardownVoicePlaybackRow((int)voiceChan);
    DF_TRACE("track: done");
}


/* 1 when a track's text (+0x18) should be shown as a subtitle: puppet parameter 7 is set,
 * the text is not empty, blank or starting with '*', and the track's name (+0x118) is none of
 * the four idle tracks (kStrIdle1..4). */
short __cdecl TrackHasSubtitleText(int pTrackEntry)  /* 0x00440390 */
{
  short i;
  short len;
  byte *pName;

  if (gPuppetParam7 == 0)
    return 0;
  len = *(byte *)(pTrackEntry + 0x18);
  if (len <= 0)
    return 0;
  if (*(char *)(pTrackEntry + 0x19) == '*')
    return 0;
  pName = (byte *)(pTrackEntry + 0x118);
  if (PascalStringsEqualNoCase(pName, (byte *)UpdatePascalLength(&kStrIdle1)) != 0)
    return 0;
  if (PascalStringsEqualNoCase(pName, (byte *)UpdatePascalLength(&kStrIdle2)) != 0)
    return 0;
  if (PascalStringsEqualNoCase(pName, (byte *)UpdatePascalLength(&kStrIdle3)) != 0)
    return 0;
  if (PascalStringsEqualNoCase(pName, (byte *)UpdatePascalLength(&kStrIdle4)) != 0)
    return 0;
  for (i = 1; i <= len; i++)
    if (*(char *)(pTrackEntry + 0x18 + i) != ' ')
      return 1;
  return 0;
}


/* Moves the puppet from frame prevFrame to frameIdx of the frame list hAnimList: clips to the
 * union of the bounding rects of the frames in between (prevFrame < 0: the whole puppet area,
 * gSceneH - 0x78 tall, 0x28 less with the text strip), then draws each of the 11 channels'
 * frames (channel 0 is the grabbed backdrop when gPuppetGrab is set) and presents the rect.
 * A frame entry is 0x52 bytes: +0 header, +8 bounding rect, +0x10 eleven 6-byte channel
 * rows {frame, y, x}. */
void __cdecl AdvanceAnimPlaybackFrame(int *hAnimList, short prevFrame, short frameIdx, short textStrip)  /* 0x00440490 */
{
#pragma pack(push, 2)
  typedef struct { byte b[0x52]; } AnimEntry;   /* copied whole: rep movsd 0x14 + movsw */
  typedef struct { int a, b; } Rect8;
  typedef struct { short frame, y, x; } ChanRow;
#pragma pack(pop)
  /* Declaration order is load-bearing: it sets which of esi/edi the channel loop gets. */
  AnimEntry entryB;
  ChanRow row;
  short clip[4];
  int chanBase;
  short first;
  int res;
  AnimEntry entryA;
  short ch;
  /* Port note: entryA/entryB (local_a0/9c, local_44/40) and clip are whole objects Ghidra
   * split (class 3); as a 16-byte array the copy overran and a NULL child was fatal. */

  res = GetResDataPtr(hAnimList);
  entryA = *(AnimEntry *)(res + frameIdx * 0x52);
  clip[0] = 0;
  clip[1] = 0;
  clip[2] = gSceneH - 0x78;
  clip[3] = gSceneW;
  if (textStrip != 0)
    clip[2] -= 0x28;                /* text strip: gSceneH - 0xa0 */
  if (prevFrame < 0) {
    *(Rect8 *)gClipRect = *(Rect8 *)clip;
  }
  else {
    first = 1;
    gClipRect[0] = 0;
    gClipRect[1] = 0;
    gClipRect[2] = 0;
    gClipRect[3] = 0;
    for (prevFrame++; prevFrame <= frameIdx; prevFrame++) {
      entryB = *(AnimEntry *)(res + prevFrame * 0x52);
      if (IsSceneRectEmpty((short *)(entryB.b + 8)) == 0) {
        if (first) {
          first = 0;
          *(Rect8 *)gClipRect = *(Rect8 *)(entryB.b + 8);
        }
        else {
          UnionSceneRects((short *)(entryB.b + 8), gClipRect, (uint32_t *)gClipRect);
        }
      }
    }
    IntersectSceneRects(clip, gClipRect, (uint32_t *)gClipRect);
    if (IsSceneRectEmpty(gClipRect) != 0) {
      UnlockResObject(hAnimList);
      return;
    }
  }
  UnlockResObject(hAnimList);
  chanBase = GetResDataPtr(DAT_0048a818);
  ch = 0;
  do {
    if (ch == 0 && gPuppetGrab != 0) {
      GrabBackdropIntoChannel0();
    }
    else {
      row = ((ChanRow *)(entryA.b + 0x10))[ch];
      RenderAnimFrameForChannel((char *)chanBase, ch, row.frame, *(uint32_t *)&row.y);
    }
    ch++;
  } while (ch < 0xb);
  PresentBackBufferRect(gClipRect);
  UnlockResObject(DAT_0048a818);
}


/* Stamps frame frameIdx of channel chanIdx (0..10) into gHotspotBuf at packedPos. Channels
 * are 0x106 bytes from pChanBase: frame count at +0x16 (at most 0x20), frame handles from
 * +0x9c. With no such frame, channel 0 clears the buffer and the others draw nothing. A bad
 * channel or count is FatalError(0x6e, 0x1073/0x1074). */
void __cdecl RenderAnimFrameForChannel(char *pChanBase, short chanIdx, short frameIdx, uint32_t packedPos)  /* 0x004406C0 */
{
  short frameCount;
  int *pFrame;

  if ((chanIdx < 0) || (chanIdx >= 0xb)) {        /* cmp 0xb / jl */
    FatalError(0x6e, 0x1073);
  }
  frameCount = *(short *)((char *)pChanBase + chanIdx * 0x106 + 0x16);
  if ((frameCount < 0) || (frameCount > 0x20)) {
    FatalError(0x6e, 0x1074);
  }
  if ((frameIdx >= 0) && (frameIdx < frameCount)) {
    pFrame = *(int **)((char *)pChanBase + chanIdx * 0x106 + frameIdx * 4 + 0x9c);
    RasterStampChildIntoSceneHotspotBuf(
        pFrame,
        packedPos, (int)(intptr_t)&gHotspotBuf, 0); /* was a literal exe address (class 5) */
    return;
  }
  if (chanIdx == 0) {
    ZeroBlock((void *)gHotspotBuf.pPx,
              (size_t)((int)gHotspotBuf.w * (int)gHotspotBuf.h));
  }
  return;
}


/* Channel 0 under puppetgrab: the set's (or stage's) current picture is the puppet's
 * backdrop. When its viewport is the scene's width and at least gSceneH - 0x78 tall, copies
 * the clip rect of it into gHotspotBuf and the set's CLUT entries gClutSyncFirst..End-1
 * (DAT_0048a784 +0xf2) into the puppet's (DAT_0048a810 +0x3a), so the room keeps its colours. */
void __stdcall GrabBackdropIntoChannel0(void)  /* 0x00440780 */
{
  short clutIdx;
  typedef struct { uint32_t d[2]; } ClutEntry8;
  int pSetClutData;
  int pPuppetClutData;
  /* ONE rect {top, left, bottom, right} (local_80.._7a). Port note: as four locals the test
   * read garbage, the CLUT copy was skipped and rooms showed hatching (class 3). */
  short vpRect[4];
  uint32_t bufCopy [30];  /* local_78 */

  if (gSet.active != 0) {      /* the exe takes the overlay case first */
    CopyTrigOverlayBufferAndViewportPair(bufCopy, (uint32_t *)vpRect);
  }
  else {
    if (gStage.active == 0) {
      return;
    }
    CopyTrigSceneBufferAndViewportPair(bufCopy, (uint32_t *)vpRect);
  }
  DF_TRACE("grabChan0: vp=(%d,%d,%d,%d) scene=%dx%d -> %s clutSync=[%d,%d)",
           (int)vpRect[0], (int)vpRect[1], (int)vpRect[2], (int)vpRect[3],
           (int)gSceneW, (int)gSceneH,
           ((int)vpRect[3] - (int)vpRect[1] == (int)gSceneW &&
            gSceneH - 0x78 <= (int)vpRect[2] - (int)vpRect[0]) ? "GRAB" : "skip",
           (int)gClutSyncFirst, (int)gClutSyncEnd);
  if (((int)vpRect[3] - (int)vpRect[1] == (int)gSceneW) &&
     (gSceneH + -0x78 <= (int)vpRect[2] - (int)vpRect[0])) {
    CopyHotspotRectBetweenBuffers(bufCopy, &gHotspotBuf, gClipRect); /* was a literal exe address (class 5) */
    pSetClutData = GetResDataPtr(DAT_0048a784);
    pPuppetClutData = GetResDataPtr(DAT_0048a810);
    /* each 8-byte CLUT entry is one struct copy (two dword moves) */
    for (clutIdx = gClutSyncFirst; clutIdx < gClutSyncEnd; clutIdx++)
      *(ClutEntry8 *)(clutIdx * 8 + pPuppetClutData + 0x3a) = *(ClutEntry8 *)(clutIdx * 8 + pSetClutData + 0xf2);
    UnlockResObject(DAT_0048a784);
    UnlockResObject(DAT_0048a810);
  }
  return;
}


/* Copies the rect pScrollRects of the back buffer (gSavedHotspotBuf) to channel B's port,
 * the screen, unless a visual effect is running (gVisualEffectDepth != 0). */
void __cdecl PresentBackBufferRect(short *pScrollRects) /* 0x004408a0 */
{
    DF_TRACE("saveScrollB: gate=%d rect=(%d,%d,%d,%d) a880=(%d,%d,%d,%d) "
             "sceneVP=(%d,%d,%d,%d) trigVP=(%d,%d,%d,%d)",
             (int)gVisualEffectDepth,
             (int)pScrollRects[0], (int)pScrollRects[1],
             (int)pScrollRects[2], (int)pScrollRects[3],
             (int)gClipRect[0], (int)gClipRect[1],
             (int)gClipRect[2], (int)gClipRect[3],
             (int)gSceneViewport.x, (int)gSceneViewport.y,
             (int)gSceneViewport.h, (int)gSceneViewport.w,
             (int)gTrigViewport.x, (int)gTrigViewport.y,
             (int)gTrigViewport.h, (int)gTrigViewport.w);
    if (gVisualEffectDepth == 0) {
        QdSetPort((int *)&gChannels[1].hResource);
        QdCopyBits((int *)&gSavedHotspotBuf, (int *)&gChannels[1].hResource,
                                      pScrollRects, pScrollRects);
    }
}


/* The index of the puppet track whose name (+0x118) or text (+0x18) is pDesc, case-
 * insensitive, or -1 (also for an empty pDesc). The header (DAT_0048a810) holds the track
 * count at +0x86e and the 0x138-byte tracks from +0x870. */
short __cdecl FindPuppetTrackIndexByName(byte *pDesc)  /* 0x004408d0 */
{
    int   iBase;
    short i;
    byte  rowBuf[0x138];

    if (*pDesc == 0)
        return -1;

    i     = 0;
    iBase = GetResDataPtr((void *)DAT_0048a810);
    if (0 < *(short *)(iBase + 0x86e)) {
        do {
            memcpy(rowBuf, (void *)(iBase + 0x870 + i * 0x138), 0x138);
            if (PascalStringsEqualNoCase(&rowBuf[0x118], pDesc) != 0 ||
                PascalStringsEqualNoCase(&rowBuf[0x18],  pDesc) != 0) {
                UnlockResObject((int *)DAT_0048a810);
                return i;
            }
            i++;
        } while (i < *(short *)(iBase + 0x86e));
    }
    UnlockResObject((int *)DAT_0048a810);
    return -1;
}


/* Draws the bevel table pTable (count, then 260-byte rows with the name at +4) into the
 * 0x78-pixel strip at the bottom of the scene, five 24-pixel rows: stamps the header's own
 * child (DAT_0048a804) centred in the strip, writes each row's name in the puppet text font, size and colour, and presents the strip. */
void __cdecl DrawBevelTable(short *pTable)  /* 0x004409b0 */
{
    short  i;
    int    iEntry;
    /* Two whole objects Ghidra split (class 3): coords = local_8/local_4 (GetBevelRowRect
     * writes 8 bytes), view = local_10/local_c (read as one rect). Port note: as separate
     * locals the strip was presented zero-width and the bevel text never reached the screen. */
    short  coords[4];      /* {y, x} as shorts: the exe stores the halves as words */
    short  view[4];        /* {sceneH - 0x78, 0, sceneH, sceneW} */

    i = 0;
    coords[1] = gSceneW / 2;
    coords[0] = gSceneH - 0x3c;
    view[0] = gSceneH - 0x78;
    view[1] = 0;
    view[2] = gSceneH;
    view[3] = gSceneW;
    /* gClipRect = view as one 8-byte copy. coords is read through a pointer so C2's scheduler
     * keeps its load after the stores while the DAT_0048a804 load passes them, as TI.EXE does. */
    {
        uint32_t *pc = (uint32_t *)coords;

        *(SceneViewport *)gClipRect = *(SceneViewport *)view;
        RasterStampChildIntoSceneHotspotBuf((int *)DAT_0048a804, *pc,
                                            (int)(intptr_t)&gHotspotBuf, 0); /* was a literal exe address (class 5) */
    }
    QdSetPort((int *)&gSavedHotspotBuf);
    QdTextFont(gTextFont);
    QdTextSize(gTextSize);
    SetSceneNodeColorFromPalette(gTextColor);

    if (0 < *pTable) {
        do {
            GetBevelRowRect(i, (uint32_t *)coords);
            QdMoveTo(coords[1] + gTextIndent, coords[0] + 0x10);
            QdDrawString((byte *)(pTable + i * 0x82 + 4));
            i++;
        } while (i < *pTable);
    }
    SetSceneNodeColorFromPalette(0);
    PresentBackBufferRect(view);
}


/* The rect {top, 0, top + 24, gSceneW} of bevel row rowIdx, top = gSceneH - (5 - rowIdx) * 24:
 * the five rows fill the bottom 0x78 pixels of the scene. */
void __cdecl GetBevelRowRect(short rowIdx, uint32_t *pOutRect)  /* 0x00440ae0 */
{
  typedef struct { short v[4]; } Rect8;   /* two dword moves: the rect is copied as one struct */
  short rect[4];

  rect[1] = 0;                            /* left, stored first */
  rect[0] = (rowIdx + -5) * 0x18 + gSceneH;
  rect[2] = rect[0] + 0x18;
  rect[3] = gSceneW;
  *(Rect8 *)pOutRect = *(Rect8 *)rect;
  return;
}


/* Makes the puppet variant (0..3) a track entry names (its first short) the current one in
 * DAT_0048a814 +0x14: swaps the quad-variant child DAT_0048a818 and clears its frame
 * handles, then attaches the track's frames. If the variant changed and no visual effect is
 * running, plays the stamp list DAT_0048a814 holds for that pair of variants (backwards when
 * going down), one stamp per 2 ticks over channel 0, in the area above the bevel strip.
 * An impossible pair is fatal (0x1078..0x107b). */
void __cdecl SwitchPuppetVariantForTrack(short *pTrackEntry) /* 0x00440B30 */
{
#pragma pack(push, 2)
    typedef struct { byte b[0x106]; } StampRec;       /* copied whole: rep movsd 0x41 + movsw */
    typedef struct { short frame, y, x; } ChanRow;
#pragma pack(pop)
    int       pData;
    short     newIdx, oldIdx, tmp;
    short     err;
    short    *pCount;
    int       ch, i;
    short     reversed;
    ChanRow  *pRow;
    ChanRow   row;
    int       chanBase;
    int      *child;
    uint32_t  t, sel;
    int       until;
    /* ONE stamp-list record (local_108 + local_106[65] + 2 bytes; rep movsd 0x41 + movsw):
     * stamp[0] count, stamp[1..2] packed top-left, selectors as dwords from stamp[3].
     * Port note: as two locals the copy overwrote its own pointers, a SIGSEGV on the lounge
     * steward (class 3). */
    short     stamp[0x83];

    pData  = GetResDataPtr(DAT_0048a814);
    oldIdx = *(short *)(pData + 0x14);
    newIdx = *pTrackEntry;
    if (newIdx != oldIdx) {
        DetachPuppetFrames();
        err = DetachRenderChild((int)DAT_0048a7fc, GetQuadVariantChildIndexOrFatal(pData));
        if (err != 0)
            FatalError(err, 0x1075);
        *(short *)(pData + 0x14) = newIdx;
        err = AttachRenderChild((int *)DAT_0048a7fc, GetQuadVariantChildIndexOrFatal(pData), (int **)&DAT_0048a818);
        if (err != 0)
            FatalError(err, 0x1076);
        pCount = (short *)(GetResDataPtr(DAT_0048a818) + 0x16);
        ch = 0xb;
        do {
            if (*pCount > 0x20)
                FatalError(0, 0x1077);
            for (i = 0; i < *pCount; i++)
                ((int *)(pCount + 0x43))[i] = 0;       /* per-frame handles at +0x9c */
            pCount += 0x83;                            /* 0x106 bytes */
        } while (--ch != 0);
        UnlockResObject(DAT_0048a818);
    }
    UnlockResObject(DAT_0048a814);

    AttachPuppetTrackFrames(*(int **)(pTrackEntry + 10), pTrackEntry[3]);

    reversed = 0;
    if (gVisualEffectDepth != 0)
        return;
    if (newIdx == oldIdx)
        return;

    QdSetPort((int *)&gChannels[1].hResource);
    /* four short stores: {0, 0, H-0x78, W}, the area above the bevel strip */
    gClipRect[0] = reversed;
    gClipRect[1]    = reversed;
    gClipRect[2]    = gSceneH - 0x78;
    gClipRect[3]    = gSceneW;

    pRow     = (ChanRow *)GetResDataPtr(*(void **)(pTrackEntry + 10));
    chanBase = GetResDataPtr(DAT_0048a818);
    row = *(ChanRow *)((char *)pRow + 0x10);
    if (newIdx < oldIdx) {      /* order the pair: oldIdx = low, newIdx = high */
        tmp = oldIdx;
        reversed = 1;
        oldIdx = newIdx;
        newIdx = tmp;
    }

    switch (oldIdx) {
    case 0:
        switch (newIdx) {
        case 1: pData += 0x26; break;
        case 2: pData += 0x12c; break;
        case 3: pData += 0x232; break;
        default: FatalError(0, 0x1078); goto copied;
        }
        break;
    case 1:
        switch (newIdx) {
        case 2: pData += 0x338; break;
        case 3: pData += 0x43e; break;
        default: FatalError(0, 0x1079); goto copied;
        }
        break;
    case 2:
        switch (newIdx) {
        case 3: pData += 0x544; break;
        default: FatalError(0, 0x107a); goto copied;
        }
        break;
    default:
        FatalError(0, 0x107b);
        goto copied;
    }
    *(StampRec *)stamp = *(StampRec *)pData;
copied:
    if (reversed) {
        for (i = 0; i < (short)(stamp[0] / 2); i++) {
            t = *(uint32_t *)&stamp[1 + (i + 1) * 2];
            *(uint32_t *)&stamp[1 + (i + 1) * 2] = *(uint32_t *)&stamp[1 + (stamp[0] - i) * 2];
            *(uint32_t *)&stamp[1 + (stamp[0] - i) * 2] = t;
        }
    }

    for (i = 0; i < stamp[0]; i++) {
        until = GetTick60Hz() + 2;
        if (gPuppetGrab != 0)
            GrabBackdropIntoChannel0();
        else
            RenderAnimFrameForChannel((char *)chanBase, 0, row.frame, *(uint32_t *)&row.y);
        sel = *(uint32_t *)&stamp[3 + i * 2];
        err = AttachRenderChild((int *)DAT_0048a7fc, sel, &child);
        if (err != 0)
            FatalError(err, 0x107c);
        RasterStampChildIntoSceneHotspotBuf(child, *(uint32_t *)&stamp[1],
                                            (int)(intptr_t)&gHotspotBuf, 0); /* was a literal exe address (class 5) */
        err = DetachRenderChild((int)DAT_0048a7fc, sel);
        if (err != 0)
            FatalError(err, 0x107d);
        PresentBackBufferRect(gClipRect);
        while ((int)GetTick60Hz() < until)
            ;
    }
    UnlockResObject(*(void **)(pTrackEntry + 10));
    UnlockResObject(DAT_0048a818);
}


/* The child index of the current puppet variant: pData +0x14 selects one of the four dwords
 * at +0x16/+0x1a/+0x1e/+0x22. Fatal (0x107e) unless it is positive. */
int __cdecl GetQuadVariantChildIndexOrFatal(int pData)                       /* 0x00440f00 */
{
    int val = 0;

    switch (*(short *)(pData + 0x14)) {   /* movsx in the exe */
    case 0: val = *(int *)(pData + 0x16); break;
    case 1: val = *(int *)(pData + 0x1a); break;
    case 2: val = *(int *)(pData + 0x1e); break;
    case 3: val = *(int *)(pData + 0x22); break;
    default: break;
    }
    if (val <= 0)
        FatalError(0, 0x107e);
    return val;
}


/* Attaches the frames a track shows (FindRowWithMatchingShortField decides) in the 11
 * channels of DAT_0048a818 and stores their handles, which OpenPuppetNode and a variant
 * switch zero. Channels are 0x106 bytes: frame count at +0x16, selectors from +0x1c, handles
 * from +0x9c. packedPhase is the track's frame count (entry +6), a short: the caller loads
 * it with `mov ax` and pushes eax.
 *
 * Port note: once an empty stub (class 7); the handles stayed zero and the first
 * openpuppetfile was fatal (999/0x108). */
void AttachPuppetTrackFrames(int *pTrackSlot, short packedPhase) /* 0x00440F50 */
{
    int    trackData;
    short *pChan;
    short  ch, frame;
    int    handle;
    short  st;

    trackData = GetResDataPtr(pTrackSlot);
    pChan     = (short *)(GetResDataPtr(DAT_0048a818) + 0x16);

    for (ch = 0; ch < 0xb; ch++) {
        if (*pChan > 0x20)
            FatalError(0, 0x107f);

        for (frame = 0; frame < *pChan; frame++) {
            handle = *(int *)(pChan + frame * 2 + 0x43);   /* +0x86 from +0x16 = +0x9c */
            if (handle != 0)
                continue;                                   /* already attached */

            if (FindRowWithMatchingShortField(trackData, packedPhase, ch, frame) == 0)
                continue;                                   /* this frame is never shown */

            st = AttachRenderChild((int *)DAT_0048a7fc,
                                   *(uint32_t *)(pChan + frame * 2 + 3),  /* selector, +6 */
                                   (int **)&handle);
            if (st != 0)
                FatalError(st, 0x1080);
            *(int *)(pChan + frame * 2 + 0x43) = handle;
        }
        pChan += 0x83;                                      /* 0x106 bytes */
    }

    UnlockResObject(DAT_0048a818);
    UnlockResObject(pTrackSlot);
}


/* 1 if any of the count 0x52-byte frame entries from pTable shows frame key in channel
 * fieldIdx (the frame short of its channel row at +0x10 + fieldIdx * 6), else 0.
 * Four parameters: Ghidra reported seven, but the caller pushes four and cleans 0x10. */
short __cdecl FindRowWithMatchingShortField(int pTable, short count,
                                            short fieldIdx, short key)  /* 0x00441040 */
{
  short i;

  for (i = 0; i < count; i++) {
    if (*(short *)(pTable + fieldIdx * 6 + 0x10) == key)
      return 1;
    pTable += 0x52;
  }
  return 0;
}


/* Detaches every attached frame of the 11 channels of DAT_0048a818 and zeroes its handle.
 * It tests the handle (+0x9c) and detaches the selector (+0x1c).
 * Port note: testing the selector, never zero, detached unattached frames: fatal 103/0x1082
 * on closepuppetfile. */
void __stdcall DetachPuppetFrames(void)                      /* 0x00441090 */
{
    short  row;
    short *ps;
    short  idx;
    short  det;
    byte  *pe;     /* the idx'th dword column: +6 selector, +0x86 handle */

    ps = (short *)(GetResDataPtr((int *)DAT_0048a818) + 0x16);
    for (row = 11; row != 0; row--) {
        if (*ps > 0x20)
            FatalError(0, 0x1081);
        for (idx = 0; idx < *ps; idx++) {
            pe = (byte *)ps + idx * 4;
            if (*(int *)(pe + 0x86) != 0) {
                det = DetachRenderChild((int)DAT_0048a7fc, *(unsigned int *)(pe + 6));
                if (det != 0)
                    FatalError(det, 0x1082);
                *(int *)(pe + 0x86) = 0;
            }
        }
        ps += 0x83;
    }

    UnlockResObject((int *)DAT_0048a818);
}


/* The puppet dialogue-choice loop behind puppetevent: draws the bevel table and blocks until
 * the player picks a row, an idle track is interrupted, or timeoutTicks pass (< 0: never).
 * Meanwhile it replays the "idle 1".."idle 4" tracks, each on its own random interval from
 * the {min, max} pairs at +0x83a/+0x84a of the header; a missing idle track, or puppetparam 8
 * (gPuppetNoTimeout), gets INT_MAX and never fires. Returns the picked row's id (the second
 * argument of its puppetbevel), -1 if an idle track was interrupted, -2 on timeout.
 *
 * Port note: once a stub whose caller hard-coded 0, so every conversation choice returned at
 * once without showing the choices (class 7). */
static int RunPuppetEventModalLoop(int timeoutTicks) /* 0x00441130 */
{
    /* The exe is fully unrolled: four idle tracks as separate variables (Ghidra's
     * iVar1..iVar9, uVar13..16, local_34/2c/24/1c, local_18/10/c/8), the parameter
     * itself turned into the deadline, and `now` a SOFTWARE counter that starts at
     * the tick and advances by one per observed tick change. */
    uint   range3;
    int    iData;
    uint   t;
    uint   range2;
    uint   range1;
    uint   now;
    int    max0;
    int    max1;
    int    max2;
    int    max3;
    uint   picked;         /* local_4 -- filled by PollPuppetEventInput */
    uint   last3;          /* local_8 */
    uint   last2;          /* local_c */
    uint   last1;          /* local_10 */
    uint   range0;
    uint   last0;          /* local_18 */
    int    interval3;      /* local_1c */
    int    min3;
    int    interval2;      /* local_24 */
    int    min2;
    int    interval1;      /* local_2c */
    int    min1;
    int    interval0;      /* local_34 */
    int    min0;
    short  idle0;          /* local_44 / sStack_42 / local_40 / sStack_3e: four shorts, */
    short  idle1;          /* not a short[4] -- as an array VC++ 4.2 lays the frame out */
    short  idle2;          /* with savedTick below it, the exe has it above */
    short  idle3;
    uint   savedTick;      /* local_3c + uStack_3a -- ONE 32-bit tick, split by Ghidra */

    QdInitCursor();
    FlushEventQueue();
    DrawBevelTable((short *)&gBevelCount);

    /* The four names sit at DESCENDING addresses: 0x45dcb8 is "idle 1". */
    idle0 = FindPuppetTrackIndexByName(UpdatePascalLength(&kStrIdle1));
    idle1 = FindPuppetTrackIndexByName(UpdatePascalLength(&kStrIdle2));
    idle2 = FindPuppetTrackIndexByName(UpdatePascalLength(&kStrIdle3));
    idle3 = FindPuppetTrackIndexByName(UpdatePascalLength(&kStrIdle4));

    if (gPuppetNoTimeout != 0) {
        idle0 = -1;
        idle1 = -1;
        idle2 = -1;
        idle3 = -1;
    }

    iData = GetResDataPtr(DAT_0048a810);
    min0 = *(int *)(iData + 0x83a);
    min1 = *(int *)(iData + 0x83e);
    min2 = *(int *)(iData + 0x842);
    max0 = *(int *)(iData + 0x84a);
    min3 = *(int *)(iData + 0x846);
    max1 = *(int *)(iData + 0x84e);
    max2 = *(int *)(iData + 0x852);
    max3 = *(int *)(iData + 0x856);
    UnlockResObject((int *)DAT_0048a810);

    range0    = (uint)(max0 - min0);
    interval0 = (int)RandomUpTo(range0) + min0;
    range1    = (uint)(max1 - min1);
    interval1 = (int)RandomUpTo(range1) + min1;
    range2    = (uint)(max2 - min2);
    interval2 = (int)RandomUpTo(range2) + min2;
    range3    = (uint)(max3 - min3);
    interval3 = (int)RandomUpTo(range3) + min3;
    if (idle0 < 0)
        interval0 = 0x7fffffff;
    if (idle1 < 0)
        interval1 = 0x7fffffff;
    if (idle2 < 0)
        interval2 = 0x7fffffff;
    if (idle3 < 0)
        interval3 = 0x7fffffff;

    savedTick = GetTick60Hz();
    now       = savedTick;
    if (timeoutTicks < 0)
        timeoutTicks = 0x7fffffff;
    else
        timeoutTicks = (int)(timeoutTicks + savedTick);
    last0 = savedTick;
    last1 = savedTick;
    last2 = savedTick;
    last3 = savedTick;

    for (;;) {
        if (interval0 <= (int)(now - last0)) {
            last0 = now;
            interval0 = (int)RandomUpTo(range0) + min0;
            PlayPuppetTrack(idle0);
            if (gTrackInterrupted != 0)
                return -1;
        }
        if (interval1 <= (int)(now - last1)) {
            last1 = now;
            interval1 = (int)RandomUpTo(range1) + min1;
            PlayPuppetTrack(idle1);
            if (gTrackInterrupted != 0)
                return -1;
        }
        if (interval2 <= (int)(now - last2)) {
            last2 = now;
            interval2 = (int)RandomUpTo(range2) + min2;
            PlayPuppetTrack(idle2);
            if (gTrackInterrupted != 0)
                return -1;
        }
        if (interval3 <= (int)(now - last3)) {
            last3 = now;
            interval3 = (int)RandomUpTo(range3) + min3;
            PlayPuppetTrack(idle3);
            if (gTrackInterrupted != 0)
                return -1;
        }

        t = GetTick60Hz();
        if (t != savedTick) {
            now++;
            savedTick = GetTick60Hz();
        }

        if ((short)PollPuppetEventInput(&picked) != 0)
            break;

        if (timeoutTicks < (int)now)
            return -2;
    }

    return (int)picked;
}


/* One poll of RunPuppetEventModalLoop (event mask 0xe). A click on a bevel row the player
 * keeps pressed picks it: frames it in the highlight colour, plays the player's voice
 * sample of that name, stores the row id in *pEntryIdOut and returns 1. A click in the
 * puppet area with voices queued replays the previous exchange (the snapshot's selected
 * row and the queued puppet tracks) instead. A key ProcessEventKeyInput takes stores -1
 * and returns 1. Otherwise 0. */
short __cdecl PollPuppetEventInput(uint32_t *pEntryIdOut)  /* 0x00441480 */
{
    short     sRow;
    short     i;
    /* ONE short in the exe (ESP+0xa): the voice row index and the key flag share the slot. */
    short     idx;
    short     rect[4];
    /* ONE 18-byte record (local_14[5] + local_a[2]; ev.pNode is local_a). Port note: split,
     * the poll overran a 10-byte buffer and the click point was uninitialised (class 3). */
    GameEvent ev;

    /* One shared `xor ax,ax` exit: the whole body sits under the poll test. */
    if (PollEventQueue(0xe, (unsigned int *)&ev) != 0) {
        if (ev.eventType == 1) {
            QdSetPort((int *)&gChannels[1].hResource);
            MapScreenPointToNode((void *)&gChannelB, (short *)&ev.pNode);
            rect[0] = 0;
            rect[1] = 0;
            rect[2] = gSceneH - 0x78;
            rect[3] = gSceneW;
            if (PointInSceneRect(*(ScenePointYX *)&ev.pNode, rect) != 0 && gQueuedVoiceCount > 0) {
                gTrackInterrupted = 0;
                InstallCursor(gCursor);
                DrawBevelTable(gBevelTableSnapshot);
                /* the snapshot's selected row: the short at +2, not the dword at +4 */
                sRow = *(short *)((char *)gBevelTableSnapshot + 2);
                if (sRow >= 0) {
                    GetBevelRowRect(sRow, (uint32_t *)rect);
                    QdPenSize(3);
                    SetSceneNodeColorFromPalette(gTextHighlightColor);
                    QdFrameRect(rect);
                    SetSceneNodeColorFromPalette(0);
                    QdPenNormal();
                    idx = FindPlayerVoiceRowIndexByName(DAT_0048af10 + sRow * 0x104);
                    if (idx >= 0)
                        RunVoiceSamplePlaybackModal(idx);
                }
                for (i = 0; i < gQueuedVoiceCount; i++) {
                    PlayPuppetTrack(gQueuedVoiceIds[i]);
                    if (gTrackInterrupted != 0)
                        break;
                }
                QdInitCursor();
                DrawBevelTable(&gBevelCount);
                gTrackInterrupted = 0;
            } else {
                for (sRow = 0; sRow < gBevelCount; sRow++) {
                    GetBevelRowRect(sRow, (uint32_t *)rect);
                    if (PointInSceneRect(*(ScenePointYX *)&ev.pNode, rect) != 0 &&
                        TrackHotspotRectPress(rect) != 0) {
                        DAT_0048af0a = sRow;
                        *pEntryIdOut = gBevel.rows[sRow].id;
                        InstallCursor(gCursor);
                        QdPenSize(3);
                        SetSceneNodeColorFromPalette(gTextHighlightColor);
                        QdFrameRect(rect);
                        SetSceneNodeColorFromPalette(0);
                        QdPenNormal();
                        idx = FindPlayerVoiceRowIndexByName(DAT_0048af10 + sRow * 0x104);
                        if (idx >= 0)
                            RunVoiceSamplePlaybackModal(idx);
                        return 1;
                    }
                }
            }
        }
        if (ProcessEventKeyInput((short *)&ev, &idx) != 0) {
            *pEntryIdOut = 0xffffffff;
            return 1;
        }
    }
    return 0;
}


/* Plays voice row playlistRowIdx of the 0x0048a80c list ("Player Voice", rows of 0x128 bytes
 * from +0x18, sound id at +0) and waits until it ends or a key ProcessEventKeyInput takes
 * stops it. Does nothing unless the header's short at +0x20 is set. */
void __cdecl RunVoiceSamplePlaybackModal(short playlistRowIdx) /* 0x00441730 */
{
    typedef struct { uint32_t d[0x4a]; } PlaylistRow;   /* 0x128 bytes, copied whole */
    short        gotEvent;
    int          iTab;
    short        keyFlag;  /* local_1a6 */
    short        event[10];  /* local_1a4 */
    uint32_t     voiceEntry[26];  /* local_190 */
    PlaylistRow  row;
#define rowWords (row.d)  /* local_128 */

    iTab        = GetResDataPtr(DAT_0048a810);
    keyFlag   = *(short *)(iTab + 0x20);
    UnlockResObject(DAT_0048a810);
    if (keyFlag == 0)
        return;

    row = ((PlaylistRow *)(GetResDataPtr(DAT_0048a80c) + 0x18))[playlistRowIdx];
    UnlockResObject(DAT_0048a80c);
    InitSoundNodeEntryFromResource(DAT_0048a7fc, rowWords[0], (byte *)UpdatePascalLength((char *)&kStrPlayerVoice), voiceEntry);   /* lookup inline: the other arguments are pushed before it is called */
    AssignPrimarySoundChannel(voiceEntry);

    do {
        gotEvent = PollEventQueue(8, (unsigned int *)event);
        if (gotEvent != 0) {
            uint32_t pk = ProcessEventKeyInput(event, &keyFlag);
            if ((short)pk != 0) {
                StopSoundChannels(0, 0, 0, 1);
                break;
            }
        }
    } while (CheckSoundChannelStopped() == 0);

    TeardownVoicePlaybackRow((int)voiceEntry);
#undef rowWords
}


/* The index of the voice row (0x0048a80c list, count at +0x16) whose name at +0x108 or +8
 * is pSearchName, case-insensitive, or -1 (also for an empty name).
 * Port note: the names were once read at +4 and +0x84, so no row ever matched. */
short __cdecl FindPlayerVoiceRowIndexByName(byte *pSearchName) /* 0x00441840 */
{
    typedef struct { byte b[0x128]; } PlaylistRow;   /* copied whole: rep movsd 0x4a */
    int         iTab;
    short       row;
    PlaylistRow rowBuf;

    if (*pSearchName == 0)
        return -1;
    row = 0;
    iTab = GetResDataPtr(DAT_0048a80c);
    if (row < *(short *)(iTab + 0x16)) {
        do {
            rowBuf = ((PlaylistRow *)(iTab + 0x18))[row];
            if (PascalStringsEqualNoCase(rowBuf.b + 0x108, pSearchName) != 0 ||
                PascalStringsEqualNoCase(rowBuf.b + 8, pSearchName) != 0) {
                UnlockResObject(DAT_0048a80c);
                return row;
            }
            row++;
        } while (row < *(short *)(iTab + 0x16));
    }
    UnlockResObject(DAT_0048a80c);
    return -1;
}


/* The keys a puppet scene takes, for a key event (type 3) with any of the 0x1fa0 bits of
 * word 7 set: '.', 'Q' or 'q' return 1 (stop); '0'..'9' set the wave volume to that level;
 * 'T'/'t' set *pToggleFlag. Everything else, and every non-stop key, returns 0. */
short __cdecl ProcessEventKeyInput(short *pEvent, undefined2 *pToggleFlag)  /* 0x00441910 */
{
  *pToggleFlag = 0;
  if ((*pEvent == 3) && ((pEvent[7] & 0x1fa0U) != 0)) {
    switch (*(byte *)(pEvent + 1)) {      /* the key byte, unsigned: xor edx / mov dl */
    case '.':
    case 'Q':
    case 'q':
      return 1;
    /* Ten arms, each pushing its own level: Ghidra dropped the argument (class 2). */
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
      *pToggleFlag = 1;
    }
  }
  return 0;
}


/* Draws pText as a subtitle straight to the screen (channel B), in the 0x28-pixel band just
 * above the bevel strip (gSceneH - 160 .. gSceneH - 120), which it erases first. Text
 * narrower than gSceneW - 16 is one line at x 8; longer text is split at the last space
 * that makes the first part fit and drawn as two lines 16 pixels apart. Splitting rewrites
 * pText's length byte (and the length at the split) in place. */
void __cdecl DrawSubtitleText(byte *pText)  /* 0x00441a80 */
{
    short  i;
    short  len;
    /* ONE rect {y1, x1, y2, x2}, contiguous in the exe's frame (class 3: once four locals). */
    short  rect[4];

    QdSetPort((int *)&gChannels[1].hResource);
    QdTextFont(gTextFont);
    QdTextSize(gTextSize);
    SetSceneNodeColorFromPalette(0);

    rect[1] = 0;
    rect[2] = gSceneH - 0x78;
    rect[0] = rect[2] - 0x28;
    rect[3] = gSceneW;
    QdEraseRect(rect);

    if ((short)QdStringWidth(pText) < gSceneW - 0x10) {
        QdMoveTo(rect[1] + 8, rect[0] + 0x10);
        QdDrawString(pText);
        return;
    }

    len = *pText;
    i = len;
    if (i > 0) {
        do {
            if (pText[i] == 0x20) {
                *pText = (byte)(i - 1);
                if ((short)QdStringWidth(pText) < gSceneW - 0x10) {
                    pText[i] = (byte)len - (byte)i;         /* the space becomes the tail's length byte */
                    CopyPascalString(pText + i, &gScratchStr2);
                    QdMoveTo(rect[1] + 8, rect[0] + 0x10);
                    QdDrawString(pText);
                    QdMoveTo(rect[1] + 8, rect[0] + 0x20);
                    QdDrawString(&gScratchStr2);
                    return;
                }
            }
            i--;
        } while (i > 0);
    }
}
