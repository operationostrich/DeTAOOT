/* trigger_overlay_actions.c -- TI.EXE 0x00439320..0x0043e990: the set view (moves, frames, dirty
 * rects, the visual-effect transitions) and DispatchActionCommand with its ActionCmd_* handlers. */

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


/* The exe's callers here read eax after ReplaceSoundChannel (void) and keep its upper
 * half: in the original this file saw only an implicit-int declaration (K&R). */


#include "topics/savestate.h"

/* Raises gOverlayRequest to at least mode (1 = redraw the set rect, 2 = the whole scene).
 *
 * Port note: void in the exe (bare ret); the three callers that read eax after it were K&R `int`
 * reads of garbage, and return 0 explicitly in the port. */
void __cdecl BumpSceneOverlayModeFloor(short mode)  /* 0x00439320 */
{
    if (gOverlayRequest < mode) {          /* cmp cx,[mem] / jle */
        gOverlayRequest = mode;
        DF_TRACE("overlayFloor: raised to %d (1=rect 0x48ae68, 2=full scene 0x48a350)", (int)mode);
    }
}


/* Sets up the set view for a session: two working buffers (7200 bytes of calc rows, 3200 of
 * transition rects), the sine and cosine tables ('TRIG' resources 1 and 2, locked at the heap top),
 * the two set HotspotBuffers, a copy of gHotspotBuf's first 120 bytes, and the half-sizes of the set
 * viewport used to centre it. Called by BeginGameSession; undone by ShutdownSetView. */
void __stdcall InitSetView(void)  /* 0x00439340 */
{
    typedef struct { unsigned int d[30]; } HotspotState120;
    short          halfW, halfH;
    int            i;

    gCalcRows = AllocEngineBuffer(0x1c20);    /* 7200 bytes */
    gTransitionRects = AllocEngineBuffer(0xc80);     /* 3200 bytes */

    gSinTableRes = (int *)GetResourceHandle(0x54524947u, 1);   /* 'TRIG' id 1 */
    gCosTableRes = (int *)GetResourceHandle(0x54524947u, 2);   /* 'TRIG' id 2 */

    MoveResObjectToHeapTop((int *)gSinTableRes);
    gSinTable = GetResDataPtr((int *)gSinTableRes);
    MoveResObjectToHeapTop((int *)gCosTableRes);
    gCosTable = GetResDataPtr((int *)gCosTableRes);

    gTrigViewport = gSceneViewport;

    InitHotspotBuffer((short *)&gTrigBufOverlay, gOverlayW, gOverlayH);
    InitHotspotBuffer((short *)&gTrigBufScene,   gSceneW,     gSceneH);
    ZeroBlock(gTrigBufOverlay.pPx, (int)(short)gOverlayW * (int)gOverlayH);
    ZeroBlock(gTrigBufScene.pPx,   (int)(short)gSceneW     * (int)gSceneH);

    /* one struct copy, as the exe's `rep movsd` with ecx = 0x1e */
    *(HotspotState120 *)&gTrigHotspotCopy = *(HotspotState120 *)&gHotspotBuf;

    gTriggerOverlayValid = 1;
    gLastTickTime = 0;
    gActiveTriggerSel = -1;        /* no move active */
    gTriggerState = 3;

    halfW = (short)(((int)gTrigViewport.w - (int)gTrigViewport.y) / 2);
    halfH = (short)(((int)gTrigViewport.h - (int)gTrigViewport.x) / 2);
    if (halfW > halfH)
        DAT_0048a79e = halfW;          /* max(halfW, halfH) */
    else
        DAT_0048a79e = halfH;
    DAT_0048a7a0 = halfH;
    DAT_0048a7a2 = halfW;
}


/* Undoes InitSetView: frees the working buffers, unlocks and releases the two 'TRIG' tables and
 * destroys the set HotspotBuffers. Called by EndGameSession. */
void __stdcall ShutdownSetView(void)  /* 0x004394f0 */
{
    FreeEngineBuffer(gCalcRows);
    FreeEngineBuffer(gTransitionRects);

    UnlockResObject((int *)gSinTableRes);
    UnlockResObject((int *)gCosTableRes);
    ReleaseResource((void *)gSinTableRes, 0x54524947u);   /* 'TRIG' id 1 */
    ReleaseResource((void *)gCosTableRes, 0x54524947u);   /* 'TRIG' id 2 */

    DestroyHotspotBuffer((int)&gTrigBufOverlay);
    DestroyHotspotBuffer((int)&gTrigBufScene);
}


/* Called once a set is loaded or restored: places gTrigViewport from the set's origin and size (gSet
 * +0x1e..+0x24), FatalError 0x66 / 0x1083 if it does not lie inside gSceneViewport, points
 * gTrigHotspotCopy's pixels at that rect of gHotspotBuf, resets the move state and recomputes the
 * viewport's half-sizes and centre. */
void __stdcall CommitSetViewport(void) /* 0x00439570 */
{
    short               localE;
    int                 local4;
    short               s2;
    short               s3;     /* 16-bit in the exe: cmp ax,si */
    short               unionRect[4];

    gTrigViewport.x = DAT_0048a79c;
    gTrigViewport.y = DAT_0048a79a;
    gTrigViewport.h = (short)(DAT_0048a798 + DAT_0048a79c);
    gTrigViewport.w = (short)(DAT_0048a796 + DAT_0048a79a);

    UnionSceneRects((short *)&gTrigViewport, (short *)&gSceneViewport, (unsigned int *)unionRect);
    if (!AreSceneRectsIdentical(unionRect, (short *)&gSceneViewport)) {
        const short *sc = (const short *)&gSceneViewport;
        DF_TRACE("!! CommitSubsystemATriggerViewport: trig NOT inside scene rect. "
                 "src a796=%d a798=%d a79a=%d a79c=%d | trig={x=%d,y=%d,h=%d,w=%d} "
                 "scene={%d,%d,%d,%d} union={%d,%d,%d,%d}",
                 (int)DAT_0048a796, (int)DAT_0048a798,
                 (int)DAT_0048a79a, (int)DAT_0048a79c,
                 (int)gTrigViewport.x, (int)gTrigViewport.y,
                 (int)gTrigViewport.h, (int)gTrigViewport.w,
                 (int)sc[0], (int)sc[1], (int)sc[2], (int)sc[3],
                 (int)unionRect[0], (int)unionRect[1],
                 (int)unionRect[2], (int)unionRect[3]);
        FatalError(0x66, 0x1083);
    }

    ComputeAlignedRectSize((short *)&gTrigViewport, &local4, (unsigned short *)&localE);
    gTrigBufOverlay.w = localE;
    gTrigBufOverlay.h = DAT_0048a798;

    /* 0x0048ad4c and 0x0048a24c are the pPx fields of the two buffers. The port had both as
     * separate int globals nothing else read or wrote. */
    gTrigHotspotCopy.pPx = (byte *)gHotspotBuf.pPx + ((int)gHotspotBuf.w * (int)gTrigViewport.x + (int)gTrigViewport.y);
    gTrigAnimBufSize = 0;
    gLastTickTime = 0U;
    gTriggerOverlayValid = 1;
    gActiveTriggerSel = -1;
    gTriggerState = 3;

    s2 = (signed short)(((int)gTrigViewport.w - (int)gTrigViewport.y) / 2);
    s3 = (signed short)(((int)gTrigViewport.h - (int)gTrigViewport.x) / 2);
    if (s3 < s2)
        DAT_0048a79e = s2;
    else
        DAT_0048a79e = s3;
    DAT_0048a7a0 = (short)(gTrigViewport.x + s3);
    DAT_0048a7a2 = (short)(gTrigViewport.y + s2);
}


/* Clears the active move and marks the set view for a full redraw (the reset half of
 * CommitSetViewport). */
void __stdcall ResetSetMoveState(void) /* 0x004396d0 */
{
    gActiveTriggerSel = -1;
    gTriggerOverlayValid = 1;
    gTriggerState = 3;
    gTrigAnimBufSize = 0;
    gLastTickTime = 0;
}


/* The last pass of a tick, after every binding table: counts the frame in gSceneFrame, then either
 * steps a running puppet (WaitForSceneTick + ShowPuppetBasePose), steps the active set move, or
 * renders the scene frame. */
short __stdcall PostTickSceneUpdate(void)  /* 0x00439700 */
{
    gSceneFrame = gSceneFrame + 1;

#if DF_TRACE_ENABLED
    {   /* Which of the three per-frame paths runs. Logged only on change, so a
         * stuck branch shows up as one line rather than thousands. Trace-only:
         * the exe has none of this. */
        static int lastBranch = -1;
        int br = ((gPuppet.active != 0) && (DAT_0048a7fa != 0)) ? 0
               : ((gSet.active != 0) && (-1 < gActiveTriggerSel)) ? 1 : 2;
        if (br != lastBranch) {
            lastBranch = br;
            DF_TRACE("postTick: branch=%d (0=transition 1=trigger 2=scene) "
                     "a7f8=%d a7fa=%d a778=%d ad40=%d",
                     br, (int)gPuppet.active, (int)DAT_0048a7fa,
                     (int)gSet.active, (int)gActiveTriggerSel);
        }
    }
#endif

    if ((gPuppet.active != 0) && (DAT_0048a7fa != 0)) {
        WaitForSceneTick();
        ShowPuppetBasePose();
        return 0;
    }
    if ((gSet.active != 0) && (gActiveTriggerSel >= 0))
        return StepSetMove();     /* a tail jump */
    RenderSceneFrame();
    return 0;
}


/* When sound + AVI subsystems are active and trigger sentinels allow it,
 * rebuilds the trigger overlay from &gSceneName / &gViewName. */
void __stdcall ReshowSetViewIfIdle(void)  /* 0x00439750 */
{
    if (gSet.active != 0 && DAT_0048a77a != 0 && gActiveTriggerSel < 0
        && gTriggerOverlayValid == 0) {
        ShowSetView(&gSceneName, &gViewName);
    }
}


/* Starts a move in the current set. moveState 3 is the straight move (StartSetMoveStraight).
 * Otherwise the playfield row of gSceneName gives the view directory (selector at byte 10); the row
 * there named gViewName supplies the move's selector (byte 14 for state 2, byte 18 for the other
 * states), whose frame table is searched for the frame keyed by that row's index; that frame becomes
 * gTriggerFrame and is shown. Does nothing while a move is active; fatals 0x1084..0x108b on a
 * missing set or resource. */
void __cdecl StartSetMove(short moveState) /* 0x00439790 */
{
    /* The 42-byte playfield row, its selectors read at byte offsets 10 / 14 / 18. Declaration
     * order and the loop spellings are the exe's register allocation (byte match). */
    typedef struct { byte b[42]; } PfRow;
    short  st;
    int   *hChild;
    PfRow  row;
    int    found;
    int    n;
    byte  *p;
    byte  *data;
    int    i;
#define SEL(off) (*(uint *)(row.b + (off)))

    if (moveState == 3) {
        StartSetMoveStraight();
        return;
    }
    if (gActiveTriggerSel >= 0)
        return;
    if (gSet.active == 0)
        FatalError(0, 0x1084);
    st = FindSceneRowByName(&gSceneName, &row);
    if (st != 0)
        FatalError(st, 0x1085);
    st = AttachRenderChild((int *)DAT_0048a780, SEL(10), &hChild);
    if (st != 0)
        FatalError(st, 0x1086);
    data  = (byte *)GetResDataPtr(hChild);
    found = -1;
    p     = data + 0x34;
    for (i = 0; i < *(int *)(data + 0x30); i++) {
        if (PascalStringsEqualNoCase(p + 0x1e, &gViewName) != 0) {
            found = i;
            break;
        }
        p += 0x2e;
    }
    UnlockResObject(hChild);
    st = DetachRenderChild((int)DAT_0048a780, SEL(10));
    if (st != 0)
        FatalError(st, 0x1087);
    if (found < 0)
        FatalError(0, 0x1088);
    if (moveState == 2)
        gActiveTriggerSel = SEL(14);
    else
        gActiveTriggerSel = SEL(18);
    st = AttachRenderChild((int *)DAT_0048a780, (uint)gActiveTriggerSel, &hChild);
    if (st != 0)
        FatalError(st, 0x1089);
    data = (byte *)GetResDataPtr(hChild);
    p    = data + 0xc;
    for (i = 0, n = *(int *)(data + 4); i < n; i++) {
        if (*(int *)(p + 0x38) == found) {
            found = -1;
            gTriggerState = moveState;
            gTriggerFrame = i;
            ShowSetFrame((int)p, i, *(int *)(data + 4));
            break;
        }
        p += 0x3c;
    }
    UnlockResObject(hChild);
    st = DetachRenderChild((int)DAT_0048a780, (uint)gActiveTriggerSel);
    if (st != 0)
        FatalError(st, 0x108a);
    if (found >= 0)
        FatalError(0, 0x108b);
#undef SEL
}


/* Starts the straight move GetStraightMoveForCurrentView finds for the current view: sets
 * gTriggerState 3, frame 0, and shows the first frame. A move with fewer than two frames is fatal
 * (0x108d); fatals 0x108c..0x108e on resource errors. */
void __cdecl StartSetMoveStraight(void) /* 0x00439990 */
{
    int     sel;
    short   sErr;
    int    *hChild;
    byte   *data;
    byte   *frames;   /* formed before the count check, as the exe does */

    sel = (int)GetStraightMoveForCurrentView();
    if (sel <= 0)
        return;

    gActiveTriggerSel = sel;
    sErr = AttachRenderChild((int *)DAT_0048a780, sel, &hChild);
    if (sErr != 0)
        FatalError(sErr, 0x108c);

    data   = (byte *)GetResDataPtr(hChild);
    frames = data + 0xc;
    if (*(int *)(data + 4) <= 1)
        FatalError(0, 0x108d);

    gTriggerFrame = 0;
    gTriggerState = 3;
    ShowSetFrame((int)frames, 0, *(int *)(data + 4));
    UnlockResObject(hChild);
    sErr = DetachRenderChild((int)(intptr_t)DAT_0048a780, (unsigned int)gActiveTriggerSel);
    if (sErr != 0)
        FatalError(sErr, 0x108e);
}


/* Looks up the current view's straight-move selector: the view directory's row named gViewName gives
 * an index, and the frame in the move table keyed by that index supplies the selector, returned
 * (0xffffffff while a move is active). Fatals 0x108f..0x1096 on a missing set, row or resource. */
uint __cdecl GetStraightMoveForCurrentView(void) /* 0x00439a60 */
{
    int    found;
    char  *pDir;
    int    i;
    char  *p;
    short  st, stAttach, stDetach, stDetach2;
    int    n;
    uint   value;
    int   *hRes;
    /* FindSceneRowByName copies 42 bytes: parseOut was uint[5] and smashed the frame (port-bug-classes.md #3). */
    uint   parseOut[11];

    if (gActiveTriggerSel >= 0)
        return 0xffffffffU;
    if (gSet.active == 0)
        FatalError(0, 0x108f);
    st = FindSceneRowByName(&gSceneName, parseOut);
    if (st != 0)
        FatalError(st, 0x1090);
    /* The selectors are at byte offsets 10 and 14 of the row (not uint indices 2 and 3), re-read each use. */
    stAttach = AttachRenderChild((int *)DAT_0048a780, *(uint *)((byte *)parseOut + 10), (int *)&hRes);
    if (stAttach != 0)
        FatalError(stAttach, 0x1091);
    pDir = (char *)GetResDataPtr(hRes);
    found = -1;
    i = 0;
    for (p = pDir + 0x34; i < *(int *)(pDir + 0x30); p += 0x2e, i++)
        if (PascalStringsEqualNoCase((byte *)(p + 0x1e), &gViewName) != 0) {
            found = i;
            break;
        }
    UnlockResObject(hRes);
    stDetach = DetachRenderChild((int)DAT_0048a780, *(int *)((byte *)parseOut + 10));
    if (stDetach != 0)
        FatalError(stDetach, 0x1092);
    if (found < 0)
        FatalError(0, 0x1093);
    stAttach = AttachRenderChild((int *)DAT_0048a780, *(uint *)((byte *)parseOut + 14), (int *)&hRes);
    if (stAttach != 0)
        FatalError(stAttach, 0x1094);
    pDir = (char *)GetResDataPtr(hRes);
    /* No assignment on the miss path, as in the exe (Ghidra showed `value = parseOut[0]`, the shared
     * stack slot); unobservable, since a miss ends in FatalError 0x1096. */
    p = pDir + 0xc;
    for (i = 0, n = *(int *)(pDir + 4); i < n; i++) {
        if (*(int *)(p + 0x38) == found) {
            found = -1;
            value = *(uint *)(p + 0x34);
            break;
        }
        p += 0x3c;
    }
    UnlockResObject(hRes);
    stDetach2 = DetachRenderChild((int)DAT_0048a780, *(int *)((byte *)parseOut + 14));
    if (stDetach2 != 0)
        FatalError(stDetach2, 0x1095);
    if (found >= 0)
        FatalError(0, 0x1096);
    return value;
}


/* Shows the named view of a scene: resolves pSceneDesc with FindSceneRowByName, finds the directory
 * row whose Pascal name is pTriggerName, rewires the nested render children, runs ShowSetFrame on
 * the matching frame, then resets the trigger globals (gTriggerFrame, the state words) and requests
 * an overlay redraw. */
void __cdecl ShowSetView(byte *pSceneDesc, byte *pTriggerName)  /* 0x00439c30 */
{
    short         s1;
    short         u;
    int          *hRowKids;
    int          *hNested;
    int          *hAttach;
    int           baseRows;
    int           rowIdx;
    int           bestIdx;
    int           innerIdx;
    int          *pi3;
    int          *pi5;
    int           frameIdx;
    int           i6;
    /* FindSceneRowByName writes 42 bytes. */
    byte          rowParse[44];
#define SEL(off) (*(uint *)(rowParse + (off)))

    if (gSet.active == 0)
        FatalError(0, 0x1097);

    u = FindSceneRowByName(pSceneDesc, rowParse);
    if (u != 0)
        FatalError(u, 0x1098);

    s1 = AttachRenderChild((int *)DAT_0048a780, SEL(10), &hRowKids);
    if (s1 != 0)
        FatalError(s1, 0x1099);

    s1 = AttachRenderChild((int *)DAT_0048a780, SEL(22), &hNested);
    if (s1 != 0)
        FatalError(s1, 0x109a);

    u = DetachRenderChild((int)DAT_0048a780, SEL(22));
    if (u != 0)
        FatalError(u, 0x109b);

    baseRows = GetResDataPtr(hRowKids);
    i6       = baseRows + 0x34;
    bestIdx  = -1;
    rowIdx   = 0;
    if (*(int *)(baseRows + 0x30) > 0) {
        do {
            if (PascalStringsEqualNoCase((byte *)(i6 + 0x1e), pTriggerName) != 0)
                bestIdx = rowIdx;
            if (*(unsigned int *)(i6 + 0x1a) != 0) {
                s1 = AttachRenderChild((int *)DAT_0048a780,
                                       *(unsigned int *)(i6 + 0x1a), &hNested);
                if (s1 != 0)
                    FatalError(s1, 0x109c);
                pi3      = (int *)GetResDataPtr(hNested);
                innerIdx = 0;
                pi5      = pi3 + 2;
                if (*pi3 > 0) {
                    do {
                        s1 = AttachRenderChild((int *)DAT_0048a780,
                                               (uint)pi5[4], &hAttach);
                        if (s1 != 0)
                            FatalError(s1, 0x109d);
                        u = DetachRenderChild((int)DAT_0048a780, (uint)pi5[4]);
                        if (u != 0)
                            FatalError(u, 0x109e);
                        pi5 += 9;
                        innerIdx++;
                    } while (innerIdx < *pi3);
                }
                UnlockResObject(hNested);
                u = DetachRenderChild((int)DAT_0048a780,
                                      *(unsigned int *)(i6 + 0x1a));
                if (u != 0)
                    FatalError(u, 0x109f);
            }
            i6 += 0x2e;
            rowIdx++;
        } while (rowIdx < *(int *)(baseRows + 0x30));
    }
    UnlockResObject(hRowKids);
    u = DetachRenderChild((int)DAT_0048a780, SEL(10));
    if (u != 0)
        FatalError(u, 0x10a0);

    if (bestIdx < 0)
        FatalError(0, 0x10a1);

    s1 = AttachRenderChild((int *)DAT_0048a780, SEL(14), &hRowKids);
    if (s1 != 0)
        FatalError(s1, 0x10a2);

    u = DetachRenderChild((int)DAT_0048a780, SEL(14));
    if (u != 0)
        FatalError(u, 0x10a3);

    s1 = AttachRenderChild((int *)DAT_0048a780, SEL(18), &hRowKids);
    if (s1 != 0)
        FatalError(s1, 0x10a4);

    innerIdx   = GetResDataPtr(hRowKids);
    i6  = innerIdx  + 0xc;
    for (frameIdx = 0, innerIdx = *(int *)(innerIdx  + 4); frameIdx < innerIdx; frameIdx++) {
        if (*(int *)(i6 + 0x38) == bestIdx) {
            bestIdx = -1;
            ShowSetFrame(i6, frameIdx, innerIdx);
            break;
        }
        i6 += 0x3c;
    }
    UnlockResObject(hRowKids);
    u = DetachRenderChild((int)DAT_0048a780, SEL(18));
    if (u != 0)
        FatalError(u, 0x10a5);

    if (bestIdx >= 0)
        FatalError(0, 0x10a6);

    gTriggerFrame = 0;
    gActiveTriggerSel = -1;
    gTriggerOverlayValid = 1;
    gTriggerState = 3;
    (void)BumpSceneOverlayModeFloor(1);
#undef SEL
}


/* Advances the active set move by one frame. In state 3 it hands over to StepStraightSetMove.
 * Otherwise it steps gTriggerFrame (wrapping to 0), shows that frame, and reads the frame's link
 * word (+0x38): a negative link means the move continues (redraw and return 0); otherwise the move
 * is done, gViewName becomes the view-directory row that link names, the move state is cleared and
 * SendOpenSceneMessage is called. */
short __stdcall StepSetMove(void)  /* 0x00439fc0 */
{
    short    status;
    short     result;
    int      pData;
    int      pFrame;
    int     *hChild;  /* local_30 */
    /* One contiguous record at ebp-0x2c, as in ResolveWalkPathSelectorForView: rowChildId is filled
     * through the sceneRowRec pointer (class 3). 44 bytes because FindSceneRowByName copies 42. */
    byte     rec2c[44];
#define sceneRowRec  ((undefined4 *)&rec2c[0])  /* local_2c */
#define rowChildId  (*(uint *)&rec2c[10])  /* local_22 */

    if (gTriggerState == 3) {
        result = StepStraightSetMove();
        return result;
    }
    status = AttachRenderChild((int *)DAT_0048a780, (uint)gActiveTriggerSel, &hChild);
    if (status != 0) {
        FatalError(status, 0x10a7);
    }
    pData = GetResDataPtr(hChild);
    pFrame = pData + 0xc;              /* the frame table base is formed first, as in the exe */
    gTriggerFrame = gTriggerFrame + 1;
    if (*(int *)(pData + 4) <= gTriggerFrame) {
        gTriggerFrame = 0;
    }
    pFrame += gTriggerFrame * 0x3c;
    ShowSetFrame(pFrame, gTriggerFrame, *(int *)(pData + 4));
    pData = *(int *)(pFrame + 0x38);
    UnlockResObject(hChild);
    status = DetachRenderChild((int)DAT_0048a780, (uint)gActiveTriggerSel);
    if (status != 0) {
        FatalError(status, 0x10a8);
    }
    if (pData < 0) {
        BumpSceneOverlayModeFloor(1);
        RenderSceneFrame();
        return 0;
    }
    status = FindSceneRowByName(&gSceneName, sceneRowRec);
    if (status != 0) {
        FatalError(status, 0x10a9);
    }
    status = AttachRenderChild((int *)DAT_0048a780, rowChildId, &hChild);
    if (status != 0) {
        FatalError(status, 0x10aa);
    }
    pFrame = GetResDataPtr(hChild);
    {
        int base = pFrame + 0x34;      /* the row table is formed before the count test */
        if (*(int *)(pFrame + 0x30) <= pData) {
            FatalError(0, 0x10ab);
        }
        base += pData * 0x2e;
        CopyPascalString((byte *)(base + 0x1e), &gViewName);
    }
    UnlockResObject(hChild);
    status = DetachRenderChild((int)DAT_0048a780, rowChildId);
    if (status != 0) {
        FatalError(status, 0x10ac);
    }
    gActiveTriggerSel  = 0xffffffff;
    gTriggerState  = 3;
    gTriggerOverlayValid  = 0;
    gTriggerFrame  = 0;
    result = SendOpenSceneMessage();
    if (result == 0) {
        BumpSceneOverlayModeFloor(1);
        RenderSceneFrame();
        result = 0;
    }
    return result;
}


/* Advances a straight move by one frame and shows it; fatal 0x10ae if the move runs past its last
 * frame. Until the last frame it only redraws. On the last frame it opens the destination view
 * directory (selector at data+8), copies its scene name to gSceneName, picks the row whose heading
 * (+8) is closest to gCameraAngle (ComputeAngularDistance, 1000 = none found, fatal 0x10b2) as
 * gViewName, clears the move state and calls SendOpenSceneMessage. */
short __stdcall StepStraightSetMove(void)  /* 0x0043a1c0 */
{
    short  stA, stD;
    short  isLast;
    char  *pData;
    int   *hRes;
    uint   next;      /* navigate table child selector */
    short  best, dist;
    int    i;
    char  *p;

    stA = AttachRenderChild((int *)DAT_0048a780, (uint)gActiveTriggerSel, (int *)&hRes);
    if (stA != 0)
        FatalError(stA, 0x10ad);
    pData = (char *)GetResDataPtr(hRes);
    p = pData + 0xc;
    gTriggerFrame++;
    isLast = 0;
    if (gTriggerFrame >= *(int *)(pData + 4))
        FatalError(0, 0x10ae);
    p += gTriggerFrame * 0x3c;      /* the frame pointer itself steps, as in the exe */
    ShowSetFrame((int)p, gTriggerFrame, *(int *)(pData + 4));
    if (*(int *)(pData + 4) - 1 <= gTriggerFrame)
        isLast = 1;
    next = *(uint *)(pData + 8);
    UnlockResObject(hRes);
    stD = DetachRenderChild((int)DAT_0048a780, (int)gActiveTriggerSel);
    if (stD != 0)
        FatalError(stD, 0x10af);
    if (isLast == 0) {
        BumpSceneOverlayModeFloor(1);
        RenderSceneFrame();
        return 0;
    }

    stA = AttachRenderChild((int *)DAT_0048a780, next, (int *)&hRes);
    if (stA != 0)
        FatalError(stA, 0x10b0);
    pData = (char *)GetResDataPtr(hRes);
    CopyPascalString((byte *)(pData + 0x20), &gSceneName);
    best = 1000;
    p = pData + 0x34;
    for (i = 0; i < *(int *)(pData + 0x30); p += 0x2e, i++) {
        dist = ComputeAngularDistance(*(short *)(p + 8), gCameraAngle);
        if (dist < best) {
            best = dist;
            CopyPascalString((byte *)(p + 0x1e), &gViewName);
        }
    }
    UnlockResObject(hRes);
    stD = DetachRenderChild((int)DAT_0048a780, next);
    if (stD != 0)
        FatalError(stD, 0x10b1);
    if (best == 1000)
        FatalError(0, 0x10b2);

    gActiveTriggerSel = 0xffffffff;
    gTriggerState = 3;
    gTriggerOverlayValid = 0;
    gTriggerFrame = 0;
    stA = SendOpenSceneMessage();
    if (stA == 0) {
        BumpSceneOverlayModeFloor(1);
        RenderSceneFrame();
        return 0;
    }
    return stA;
}

#undef local_2c
#undef local_22

/* Draws one scene frame from the dirty rects. Resets the row and rect counts, adds gTrigViewport or
 * gSceneViewport when gOverlayRequest is 1 or 2, rebuilds and sorts the calc rows, merges the dirty
 * rects, restores each from the backdrop, stamps the rows into the hotspot buffers, waits for the
 * frame tick, and (unless a visual effect is running, gVisualEffectDepth) copies each dirty rect
 * from the back buffer to channel B. */
void __stdcall RenderSceneFrame(void)  /* 0x0043a3e0 */
{
    short  rectsLeft;
    short *pRect;

    gCalcRowCount = 0;
    gTransitionRectCount = 0;
    if (gOverlayRequest != 0) {
        if (gOverlayRequest == 1) {
            AddDirtyRect(&gTrigViewport);
        }
        if (gOverlayRequest == 2) {
            AddDirtyRect(&gSceneViewport);
        }
        gOverlayRequest = 0;
    }
    WalkBothSlotTablesCalcRender(1);
    SortCalcScratchRowsByHotspotBand();
    MergeOverlappingDirtyRects();
    if (gTransitionRectCount != 0)
        DF_TRACE("nextScene: %d dirty rect(s), gate a716=%d rects=%p",
                 (int)gTransitionRectCount, (int)gVisualEffectDepth, (void *)gTransitionRects);
    pRect = gTransitionRects;
    rectsLeft  = gTransitionRectCount;
    if (0 < gTransitionRectCount) {
        do {
            RestoreDirtyRectFromBackdrop(pRect);
            pRect = pRect + 4;
            rectsLeft  = rectsLeft - 1;
        } while (rectsLeft != 0);
    }
    MarkScratchRowsOverlappingDirtyRects();
    StampCalcScratchRowsToHotspotBufs();
    WaitForSceneTick();
    if (gVisualEffectDepth == 0) {
        pRect = gTransitionRects;
        QdSetPort((int *)&gChannels[1].hResource);
        rectsLeft  = gTransitionRectCount;
        if (0 < gTransitionRectCount) {
            do {
                QdCopyBits((int *)&gSavedHotspotBuf, (int *)&gChannels[1].hResource,
                             pRect, pRect);
                pRect = pRect + 4;
                rectsLeft  = rectsLeft - 1;
            } while (rectsLeft != 0);
        }
    }
}


/* Busy-waits until gFrameRate 60 Hz ticks have passed since the last frame, then makes the new
 * deadline the baseline (gLastTickTime) for the next call. */
void __stdcall WaitForSceneTick(void)  /* 0x0043a4c0 */
{
    while ((int)GetTick60Hz() < gFrameRate + gLastTickTime)
        ;
    gLastTickTime = GetTick60Hz();
}

#undef local_2c
#undef local_22
#undef local_1e

/* Makes frame record pFrame current. The camera position and angle (+0x20..+0x26) go to gCameraX/Y/Z
 * and gCameraAngle, the angle's cosine and sine to gViewCos/gViewSin, and the player position is the
 * camera moved back along the heading by DAT_0048a78e. If the frame names an image child (+0x2c) it
 * is decoded into gTrigBufOverlay, the rest of its data copied to gTrigAnimBuf, and the child
 * detached (or released, when DAT_0048a77c is 0). Unless this is the last frame, the next child
 * (+0x68) is activated while the loader runs. */
void __cdecl ShowSetFrame(int pFrame, int frameIdx, int totalFrames)  /* 0x0043a4e0 */
{
    short      status;
    int        hdr;       /* size of the child's header section */
    int        size;      /* size of its data section */
    int        data;
    void      *pDst;
    int       *hChild;   /* local_4 */

    gViewCos = *(short *)((int)gCosTable + *(short *)(pFrame + 0x26) * 2);
    gViewSin = *(short *)((int)gSinTable + *(short *)(pFrame + 0x26) * 2);
    gCameraX = *(short *)(pFrame + 0x20);
    gCameraY = *(short *)(pFrame + 0x22);
    gCameraZ = *(short *)(pFrame + 0x24);
    gCameraAngle = *(short *)(pFrame + 0x26);
    gPlayerX = *(short *)(pFrame + 0x20)
                 - (short)AngleDistToDeltaX(*(unsigned short *)(pFrame + 0x26), DAT_0048a78e);
    gPlayerY = *(short *)(pFrame + 0x22)
                 - (short)AngleDistToDeltaY(*(unsigned short *)(pFrame + 0x26), DAT_0048a78e);
    gPlayerZ = *(short *)(pFrame + 0x24);
    gCurrentDeg = *(short *)(pFrame + 0x26);

    DF_TRACE("trigFrame: %d/%d child=%u alt=%u node=%p a77c=%d scene=%d",
             frameIdx, totalFrames, *(uint *)(pFrame + 0x2c), *(uint *)(pFrame + 0x68),
             (void *)DAT_0048a780, (int)DAT_0048a77c, (int)*(short *)(pFrame + 0x26));
    if (*(uint *)(pFrame + 0x2c) == 0) {
        gTrigAnimBufSize = 0;
        return;
    }

    status = ResolveDisplayAttachSlot((int)DAT_0048a780, *(uint *)(pFrame + 0x2c), &hChild);
    if (status != 0) {
        DF_TRACE("!! trigFrame: resolve child=%u on node=%p failed err=%d (frame %d/%d, "
                 "scene=%d pos=(%d,%d,%d) alt=%u)",
                 *(uint *)(pFrame + 0x2c), (void *)DAT_0048a780, (int)status,
                 frameIdx, totalFrames, (int)*(short *)(pFrame + 0x26),
                 (int)*(short *)(pFrame + 0x20), (int)*(short *)(pFrame + 0x22),
                 (int)*(short *)(pFrame + 0x24), *(uint *)(pFrame + 0x68));
        FatalError(status, 0x10b3);
    }

    if ((DAT_0048a77e != 0) && (frameIdx + 1 < totalFrames)) {
        if (IsDisplayAttachLoaderRunning((int)DAT_0048a780) != 0) {
            DrainDisplayAttachSlotsForContext((int)DAT_0048a780);
            status = ActivateDisplayAttachSlot((int)DAT_0048a780, *(uint *)(pFrame + 0x68));
            if (status != 0)
                FatalError(status, 0x10b4);
        }
    }

    hdr  = DecodePageNodeImage(hChild, &gTrigBufOverlay);
    size = GetResObjectSize(hChild) - hdr;     /* total allocation minus header */
    if (gTrigAnimBufSize < size) {
        ResizeResObject((int *)gTrigAnimBuf, size);
        gTrigAnimBufSize = size;
    }
    data = GetResDataPtr((void *)hChild);
    pDst = (void *)GetResDataPtr((void *)gTrigAnimBuf);
    memmove(pDst, (void *)(hdr + data), size);
    UnlockResObject((int *)gTrigAnimBuf);
    UnlockResObject(hChild);

    /* the exe tests the detach case first */
    if (DAT_0048a77c != 0) {
        status = DetachRenderChild((int)DAT_0048a780, *(uint *)(pFrame + 0x2c));
        if (status != 0)
            FatalError(status, 0x10b5);
    } else {
        status = ReleaseRenderChild((int)DAT_0048a780, *(uint *)(pFrame + 0x2c));
        if (status != 0)
            FatalError(status, 0x10b6);
    }
}


/* Runs WalkActorsCalcRender, then WalkPropsCalcRender, with the same mode. */
void __cdecl WalkBothSlotTablesCalcRender(short mode) /* 0x0043a720 -- a short argument (mov si,[esp+8]) */
{
    WalkActorsCalcRender(mode);
    WalkPropsCalcRender(mode);
}


/* Hit-tests the calc rows (rebuilt and sorted first, newest first, 9 dwords each) against packedPt,
 * which the exe biases in place as it goes: a row's rect must contain the point, then its sprite or
 * scaled hotspot strip is probed through AttachRenderChild. On a hit the row's binding key (dword 1)
 * is stored in *pBindingKeyOut and 1 is returned. */
short __cdecl FindPaintingBindingKeyAtPoint(int packedPt, int *pBindingKeyOut)  /* 0x0043A740 */
{
    uint  *pu;
    int   *hChild;
    short  attachSt;
    short  idx;
    short  detachSt;
    short  hitProbe;

    gCalcRowCount = 0;
    WalkBothSlotTablesCalcRender(0);
    SortCalcScratchRowsByHotspotBand();

    /* formed before the count test, as the exe does */
    pu = (uint *)((byte *)gCalcRows + gCalcRowCount * 0x24 - 0x24);

    for (idx = 0; idx < gCalcRowCount; idx++) {
        if (PointInSceneRect(*(ScenePointYX *)&packedPt, (short *)(pu + 4))) {
            attachSt = AttachRenderChild((int *)pu[6], pu[7], &hChild);
            if (attachSt != 0)
                FatalError(attachSt, 0x10b9);

            if (*(short *)((byte *)pu + 12) < 0) {
                hitProbe = ProbeSpriteHotspotStripHit(hChild, (int)pu[0],
                                                      *(short *)((byte *)pu + 10), *(ScenePointYX *)&packedPt);
            } else {
                hitProbe = 0;
                if (ProbeScaledHotspotStripHit(hChild, (int)pu[0],
                                               *(short *)((byte *)pu + 10), *(ScenePointYX *)&pu[8], *(ScenePointYX *)&packedPt) != 0) {
                    /* The exe biases the parameter in place (0x0043a81b), so the adjusted point is what
                     * every later row tests; the port once adjusted a copy. */
                    ((ScenePointYX *)&packedPt)->x -= DAT_0048a79a;
                    ((ScenePointYX *)&packedPt)->y -= DAT_0048a79c;
                    hitProbe =
                        ProbeTriggerOverlayHotspotAtPackedPoint((short *)&gTrigBufOverlay,
                                                                *(short *)((byte *)pu + 14),
                                                                *(ScenePointYX *)&packedPt);
                }
            }

            detachSt = (short)DetachRenderChild((int)(intptr_t)(void *)pu[6], pu[7]);
            if (detachSt != 0)
                FatalError(detachSt, 0x10ba);

            DF_TRACE("hitProbe: pt=(y=%d,x=%d) row child=%u pos=(y=%d,x=%d) rect=(%d,%d,%d,%d) sel=%d band=%d -> probe=%d",
                     (int)(short)(packedPt & 0xffff), (int)(short)(packedPt >> 16), (unsigned)pu[7],
                     (int)(short)(pu[0] & 0xffff), (int)(short)(pu[0] >> 16),
                     (int)*(short *)((byte *)pu + 16), (int)*(short *)((byte *)pu + 18),
                     (int)*(short *)((byte *)pu + 20), (int)*(short *)((byte *)pu + 22),
                     (int)*(short *)((byte *)pu + 12), (int)*(short *)((byte *)pu + 10), (int)hitProbe);
            if (hitProbe != 0) {
                *pBindingKeyOut = (int)pu[1];
                return 1;
            }
        }
        pu -= 9;
    }

    return 0;
}


/* Copies gTrigBufScene (30 dwords) to pDstBuf and gSceneViewport (2 dwords) to pDstViewport. */
void CopyTrigSceneBufferAndViewportPair(void *pDstBuf, void *pDstViewport)  /* 0x0043a8a0 */
{
    typedef struct { unsigned int v[0x1e]; } TrigBuf120;   /* rep movsd, ecx = 30 */
    typedef struct { unsigned int v[2]; } Viewport8;       /* two dword moves */

    *(TrigBuf120 *)pDstBuf = *(TrigBuf120 *)&gTrigBufScene;
    *(Viewport8 *)pDstViewport = *(Viewport8 *)&gSceneViewport;
}


/* Copies gTrigBufOverlay (30 dwords) to pDstBuf and gTrigViewport (2 dwords) to pDstViewport. */
void CopyTrigOverlayBufferAndViewportPair(void *pDstBuf, void *pDstViewport) /* 0x0043a8d0 */
{
    typedef struct { unsigned int v[0x1e]; } TrigBuf120;   /* rep movsd, ecx = 30 */
    typedef struct { unsigned int v[2]; } Viewport8;       /* two dword moves */

    *(TrigBuf120 *)pDstBuf = *(TrigBuf120 *)&gTrigBufOverlay;
    *(Viewport8 *)pDstViewport = *(Viewport8 *)&gTrigViewport;
}


/* Returns gActiveTriggerSel, the active move's selector; -1 means no move is active. */
int __stdcall GetActiveSetMove(void)  /* 0x0043a900 */
{
    return gActiveTriggerSel;
}


/* Fixed-point offset along X for a heading: gCosTable[frameSlotIndex & 0xff] * sceneId >> 14,
 * rounded toward zero as the exe does (cdq / and 0x3fff / add / sar 14). AngleDistToDeltaY is the
 * same with gSinTable. */
short __cdecl AngleDistToDeltaX(unsigned short frameSlotIndex, short sceneId) /* 0x0043a910 */
{
    int rowScale;
    int prod;

    rowScale = (int)*(short *)((int)gCosTable + (int)(frameSlotIndex & 0xffu) * 2);
    prod     = rowScale * (int)sceneId;
    return (short)(prod / 0x4000);      /* a signed division: cdq / and 0x3fff / add / sar 14 */
}


/* AngleDistToDeltaX with gSinTable. */
short __cdecl AngleDistToDeltaY(unsigned short frameSlotIndex, short sceneId) /* 0x0043a940 */
{
    int rowScale;
    int prod;

    rowScale = (int)*(short *)((int)gSinTable + (int)(frameSlotIndex & 0xffu) * 2);
    prod     = rowScale * (int)sceneId;
    return (short)(prod / 0x4000);      /* a signed division: cdq / and 0x3fff / add / sar 14 */
}


/* Maps AVI sprite axes through trigger trig basis (gViewCos/5c) + depth (gCameraHeight); writes pOutPair[0... */
short __cdecl ProjectSetPointToScreen(int pPoint, short *pScreenYX) /* 0x0043a970 */
{
    int across;
    int y;
    int x;
    int depth;
    int x2;
    int y2;
    int z;

    x      = *(short *)(pPoint + 2) - gCameraX;
    y      = *(short *)(pPoint + 4) - gCameraY;
    z      = *(short *)(pPoint + 6) - gCameraZ - gCameraHeight;
    x2 = x;
    y2 = y;
    x2 *= gViewCos;
    x *= gViewSin;
    y *= gViewCos;
    y2 *= gViewSin;
    x2 += y2;
    y -= x;
    depth  = x2 / 0x4000;
    across = y / 0x4000;
    if (depth <= 0)
        return (short)depth;
    across *= DAT_0048a79e;
    z      *= DAT_0048a79e;
    across /= depth;
    z      /= depth;
    z = -z;
    z += DAT_0048a7a0;
    across += DAT_0048a7a2;
    pScreenYX[0] = (short)z;
    pScreenYX[1] = (short)across;
    return (short)depth;
}


/* Appends the 9-dword row pRow (0x24 bytes) to gCalcRows; fatal 0x6e / 0x10bb past 199 rows. */
void AppendCalcScratchRow(unsigned int *pRow) /* 0x0043aa40 */
{
    typedef struct { unsigned int d[9]; } CalcScratchRow;   /* 36 bytes: rep movsd, ecx = 9 */

    ((CalcScratchRow *)gCalcRows)[gCalcRowCount] = *(CalcScratchRow *)pRow;
    gCalcRowCount++;
    if (gCalcRowCount >= 200)
        FatalError(0x6e, 0x10bb);
}


/* Adds pRect (4 shorts) to the dirty-rect list gTransitionRects: merged into the first rect it
 * intersects (UnionSceneRects), else appended; fatal past 399 rects. */
void AddDirtyRect(short *pRect) /* 0x0043aa90 */
{
    short  n;
    short *p;
    int    tmp[2];

    n = gTransitionRectCount;
    for (p = (short *)gTransitionRects; n > 0; n--, p += 4) {
        if (IntersectSceneRects(pRect, p, tmp) != 0) {
            UnionSceneRects(pRect, p, (unsigned int *)p);
            return;
        }
    }
    ((SceneViewport *)gTransitionRects)[gTransitionRectCount] = *(SceneViewport *)pRect;
    gTransitionRectCount++;
    if (gTransitionRectCount >= 400)
        FatalError(0x6e, 0x10bc);
}


/* Bubble-sorts the gCalcRows scratch rows ascending by the short at byte +0x0c. */
void __stdcall SortCalcScratchRowsByHotspotBand(void) /* 0x0043ab20 */
{
    typedef struct { unsigned int d[9]; } Row36;
    short i, j;
    Row36 tmp;
    Row36 *p, *q;

    /* Row i against every later row j (the port once compared a row with
     * itself: the key of the NEXT row is at +0x24+12). Rows swap as whole
     * 36-byte structs (three rep movsd, ecx = 9). */
    p = (Row36 *)gCalcRows;
    for (i = 0; i < gCalcRowCount - 1; i++, p++)
        for (j = i + 1, q = p + 1; j < gCalcRowCount; j++, q++)
            if (*(short *)((char *)p + 12) < *(short *)((char *)q + 12)) {
                tmp = *p;
                *p = *q;
                *q = tmp;
            }
}


/* Merges overlapping rects of gTransitionRects (IntersectSceneRects, then UnionSceneRects), closing
 * the gap with SlideDirtyRectSlotsDown. */
void MergeOverlappingDirtyRects(void) /* 0x0043abb0 */
{
    short i;
    short *ri;
    short *rj;
    short j;
    int tmp[2];

    ri = (short *)gTransitionRects;
    for (i = 0; i < gTransitionRectCount - 1; ri += 4) {
        j = ++i;
        for (rj = ri + 4; j < gTransitionRectCount; j++, rj += 4) {
            if (IntersectSceneRects(ri, rj, tmp) != 0) {
                UnionSceneRects(ri, rj, (unsigned int *)ri);
                SlideDirtyRectSlotsDown((unsigned int *)(rj + 4), (unsigned int *)rj,
                                             (short)(gTransitionRectCount - j - 1));
                j--;            /* re-test the rect that slid into slot j */
                rj -= 4;
                gTransitionRectCount--;
            }
        }
    }
}


/* Moves rectCount rects (two dwords each) from pFrom to pTo, lowest first, closing a slot after a
 * merge. */
void SlideDirtyRectSlotsDown(unsigned int *pFrom, unsigned int *pTo, short rectCount) /* 0x0043ac50 */
{
    /* the exe's own copy loop: one rect (two dwords) per iteration, source first;
     * the caller passes (rect j+1, rect j, rects after j) -- Ghidra 0x0043abb0 */
    unsigned int lo, hi;

    while (rectCount > 0) {
        lo = pFrom[0];
        hi = pFrom[1];
        pFrom += 2;
        pTo[0] = lo;
        pTo[1] = hi;
        pTo += 2;
        rectCount--;
    }
}


/* Sets the marker short (+8) of every gCalcRows row whose rect (+0x10) intersects some dirty rect,
 * where it was 0. */
void __stdcall MarkScratchRowsOverlappingDirtyRects(void) /* 0x0043ac80 */
{
    short         remaining;     /* kept in the frame: DEC WORD PTR [ESP+..] / JNZ */
    int           scratchPair[2];
    short         nLeft;
    short        *psWalk;
    unsigned char *row;

    remaining = gCalcRowCount;
    row       = (unsigned char *)gCalcRows;
    while (remaining > 0) {
        if (*(short *)(row + 8) == 0) {
            nLeft = gTransitionRectCount;
            psWalk = (short *)gTransitionRects;
            for (; nLeft > 0; nLeft--, psWalk += 4)
                if (IntersectSceneRects(psWalk, (short *)(row + 0x10), scratchPair) != 0) {
                    *(short *)(row + 8) = 1;
                    break;
                }
        }
        row += 0x24;
        remaining--;
    }
}


/* Stamps each committed gCalcRows row's render child into gHotspotBuf or gTrigHotspotCopy (chosen by
 * the short at +12), clipped to gSceneViewport or gTrigViewport, then detaches the child. Fatal
 * 0x10bd / 0x10be on an attach or detach error. See RasterStampChildIntoSceneHotspotBuf and
 * RasterStampChildIntoTrigSceneHotspotBuf. */
void __stdcall StampCalcScratchRowsToHotspotBufs(void) /* 0x0043ad00 */
{
    typedef struct { short s[4]; } Rect8;
    unsigned int  packedAdj;
    int           childHandle;
    unsigned int *row;
    short         remaining;
    short         att;

    DF_TRACE("stampRows: count=%d rowTable=%p", (int)gCalcRowCount, (void *)gCalcRows);

    row       = (unsigned int *)gCalcRows;
    remaining = gCalcRowCount;
    if (remaining > 0) {
      do {
        if (*(short *)((unsigned char *)row + 8) != 0) {
            att = AttachRenderChild((int *)(intptr_t)row[6], row[7], &childHandle);
            if (att != 0)
                FatalError(att, 0x10bd);
            /* row is the 0x24-byte calc scratch record; as shorts, [8..0xb] is
             * the destination rect the sprite was placed at = dwords [4] and [5]. */
            DF_TRACE("  stamp: child=%u pos=(y%d,x%d) rect=(y%d,x%d,y%d,x%d) sel=%d path=%s",
                     (unsigned)row[7],
                     (int)(short)(row[0] & 0xffffu), (int)(short)(row[0] >> 16),
                     (int)(short)(row[4] & 0xffffu), (int)(short)(row[4] >> 16),
                     (int)(short)(row[5] & 0xffffu), (int)(short)(row[5] >> 16),
                     (int)*(short *)((unsigned char *)row + 12),
                     (*(short *)((unsigned char *)row + 12) < 0) ? "scene" : "trig");

            if (*(short *)((unsigned char *)row + 12) < 0) {
                *(Rect8 *)gClipRect = *(Rect8 *)&gSceneViewport;
                /* the raster stampers take the handle (they lock it themselves), not a data pointer */
                RasterStampChildIntoSceneHotspotBuf(
                    (int *)(intptr_t)(unsigned int)childHandle,
                    row[0],
                    (int)(intptr_t)&gHotspotBuf,
                    *(short *)((unsigned char *)row + 10));
            } else {
                *(Rect8 *)gClipRect = *(Rect8 *)&gTrigViewport;
                /* the packed point is adjusted in place, half by half */
                packedAdj = row[0];
                ((short *)&packedAdj)[0] -= gTrigViewport.x;
                ((short *)&packedAdj)[1] -= gTrigViewport.y;
                OffsetSceneRect(gClipRect, (short)(-gTrigViewport.y), (short)(-gTrigViewport.x));
                RasterStampChildIntoTrigSceneHotspotBuf(
                    (int *)(intptr_t)(unsigned int)childHandle,
                    packedAdj,
                    (int)(intptr_t)&gTrigHotspotCopy,
                    *(short *)((unsigned char *)row + 10),
                    *(short *)((unsigned char *)row + 14),
                    *(ScenePointYX *)&row[8]);
            }

            att = DetachRenderChild((int)(intptr_t)row[6], row[7]);
            if (att != 0)
                FatalError(att, 0x10be);
        }
        row += 9;
      } while (--remaining != 0);
    }
}


/* Restores one dirty rect of RenderSceneFrame's list from the backdrop buffers: gTrigBufScene,
 * gHotspotBuf, gTrigBufOverlay and gTrigHotspotCopy, chosen by the AVI flags and by how the rect
 * meets gTrigViewport. */
void __cdecl RestoreDirtyRectFromBackdrop(short *transitionRect) /* 0x0043ae30 */
{
    /* One 8-byte working rect (the intersection, then each strip, each a whole
     * copy of the transition rect with one edge replaced and the other two
     * clamped in place) and a saved copy of the intersection. */
    typedef struct { short s[4]; } Rect8;
    Rect8 piece;
    Rect8 clipSave;

    if (gSet.active != 0 && DAT_0048a77a != 0 &&
        IntersectSceneRects(transitionRect, (short *)&gTrigViewport, (int *)&piece) != 0) {
        clipSave = piece;
        OffsetSceneRect(piece.s, (short)-gTrigViewport.y, (short)-gTrigViewport.x);
        CopyHotspotRectBetweenBuffers(&gTrigBufOverlay, &gTrigHotspotCopy, piece.s);
        /* (short): the exe tests ax */
        if ((short)AreSceneRectsIdentical(clipSave.s, transitionRect) == 0) {
            piece = *(Rect8 *)transitionRect;
            piece.s[2] = gTrigViewport.x;
            CopyHotspotRectBetweenBuffers(&gTrigBufScene, &gHotspotBuf, piece.s);
            piece = *(Rect8 *)transitionRect;
            piece.s[0] = gTrigViewport.h;
            CopyHotspotRectBetweenBuffers(&gTrigBufScene, &gHotspotBuf, piece.s);
            piece = *(Rect8 *)transitionRect;
            piece.s[1] = gTrigViewport.w;
            if (piece.s[0] < gTrigViewport.x)
                piece.s[0] = gTrigViewport.x;
            if (piece.s[2] > gTrigViewport.h)
                piece.s[2] = gTrigViewport.h;
            CopyHotspotRectBetweenBuffers(&gTrigBufScene, &gHotspotBuf, piece.s);
            piece = *(Rect8 *)transitionRect;
            piece.s[3] = gTrigViewport.y;
            if (piece.s[0] < gTrigViewport.x)
                piece.s[0] = gTrigViewport.x;
            if (piece.s[2] > gTrigViewport.h)
                piece.s[2] = gTrigViewport.h;
            CopyHotspotRectBetweenBuffers(&gTrigBufScene, &gHotspotBuf, piece.s);
        }
        return;
    }
    CopyHotspotRectBetweenBuffers(&gTrigBufScene, &gHotspotBuf, transitionRect);
}


/* Runs the transition named by cueId (0x5dc1..0x5dd5; there is no 0x5dd3) on the rect pDesc over
 * value ticks. Every transition copies the new picture from the back buffer (gSavedHotspotBuf) onto
 * channel B in steps; the Turn pair also shifts the old picture across. Called by
 * ActionCmd_VisualEffect with the scene viewport. */
void __cdecl DispatchVisualEffect(short *pDesc, short value, short cueId)  /* 0x0043B000 */
{
  switch (cueId) {
  case 0x5dc1:
    VisualEffectBarnDoorClose((uint32_t *)pDesc, value);
    return;
  case 0x5dc2:
    VisualEffectBarnDoorOpen((uint32_t *)pDesc, value);
    return;
  case 0x5dc3:
    VisualEffectIrisClose((uint32_t *)pDesc, value);
    return;
  case 0x5dc4:
    VisualEffectIrisOpen((uint32_t *)pDesc, value);
    return;
  case 0x5dc5:
    VisualEffectScrollDown((uint32_t *)pDesc, value);
    return;
  case 0x5dc6:
    VisualEffectScrollUp((uint32_t *)pDesc, value);
    return;
  case 0x5dc7:
    VisualEffectScrollRight((uint32_t *)pDesc, value);
    return;
  case 0x5dc8:
    VisualEffectScrollLeft((uint32_t *)pDesc, value);
    return;
  case 0x5dc9:
    VisualEffectVenetian(pDesc, value);
    return;
  case 0x5dca:
    VisualEffectWipeDown((uint32_t *)pDesc, value);
    return;
  case 0x5dcb:
    VisualEffectWipeUp((uint32_t *)pDesc, value);
    return;
  case 0x5dcc:
    VisualEffectWipeRight((uint32_t *)pDesc, value);
    return;
  case 0x5dcd:
    VisualEffectWipeLeft((uint32_t *)pDesc, value);
    return;
  case 0x5dce:
    VisualEffectPlain((uint32_t *)pDesc, value);
    return;
  case 0x5dcf:
    VisualEffectTurnRight((uint32_t *)pDesc, value);
    return;
  case 0x5dd0:
    VisualEffectTurnLeft((uint32_t *)pDesc, value);
    return;
  case 0x5dd1:
    VisualEffectTurnUp((uint32_t *)pDesc, value);
    return;
  case 0x5dd2:
    VisualEffectTurnDown((uint32_t *)pDesc, value);
    return;
  case 0x5dd4:
    VisualEffectTurnHalfLeft((uint32_t *)pDesc, value);
    return;
  case 0x5dd5:
    VisualEffectTurnHalfRight((uint32_t *)pDesc, value);
    return;
  default:
    return;
  }
}


/* Reveals the new picture in two strips that grow from the left and right edges toward the middle. */
void __cdecl VisualEffectBarnDoorClose(uint32_t *pRect, short stepCount) /* 0x0043B1F0 */
{
    uint32_t tick;
    short step;
    short i;
    short a[4];
    short b[4];

    /* Rects are {y1, x1, y2, x2} arrays: Ghidra showed loose dword/short locals, and as separate C
     * locals GCC did not keep each rect contiguous, so every step used garbage (port-bug-classes.md
     * #3). The effects below repeat this; copy order = the exe's stack layout. */
    tick = GetTick60Hz();
    *(SceneViewport *)b = *(SceneViewport *)pRect;
    *(SceneViewport *)a = *(SceneViewport *)pRect;
    step = (a[3] - a[1]) / (short)(stepCount * 2) + 1;
    a[3] = a[1] + step;
    b[1] = b[3] - step;
    for (i = 0; i < stepCount; i++) {
        BlitBackBufferRect((uint32_t *)a, (uint32_t *)a);
        BlitBackBufferRect((uint32_t *)b, (uint32_t *)b);
        a[1] += step;
        a[3] += step;
        b[1] -= step;
        b[3] -= step;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* Reveals the new picture in two strips that grow from the horizontal midline outward. */
void __cdecl VisualEffectBarnDoorOpen(uint32_t *pRect, short stepCount) /* 0x0043B2D0 */
{
    uint32_t tick;
    short step;
    short i;
    short a[4];
    short b[4];

    /* rects as in BarnDoorClose; copy order = the exe's stack layout */
    tick = GetTick60Hz();
    *(SceneViewport *)b = *(SceneViewport *)pRect;
    *(SceneViewport *)a = *(SceneViewport *)pRect;
    step = (a[3] - a[1]) / (short)(stepCount * 2) + 1;
    a[3] = (a[3] + a[1]) >> 1;
    b[1] = a[3];
    a[1] = a[3] - step;
    b[3] = a[3] + step;
    for (i = 0; i < stepCount; i++) {
        BlitBackBufferRect((uint32_t *)a, (uint32_t *)a);
        BlitBackBufferRect((uint32_t *)b, (uint32_t *)b);
        a[1] -= step;
        a[3] -= step;
        b[1] += step;
        b[3] += step;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* Reveals the new picture in four strips that grow inward from the four edges. */
void __cdecl VisualEffectIrisClose(uint32_t *pRect, short stepCount) /* 0x0043B3B0 */
{
    uint32_t tick;
    short i;
    short r0[4];
    short r1[4];
    short r2[4];
    short r3[4];
    short dy;
    short dx;

    /* rects as in BarnDoorClose; copy order = the exe's stack layout */
    tick = GetTick60Hz();
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
        BlitBackBufferRect((uint32_t *)r1, (uint32_t *)r1);
        BlitBackBufferRect((uint32_t *)r0, (uint32_t *)r0);
        BlitBackBufferRect((uint32_t *)r3, (uint32_t *)r3);
        BlitBackBufferRect((uint32_t *)r2, (uint32_t *)r2);
        r2[1] += dx;
        r2[3] += dx;
        r1[0] += dy;
        r1[2] += dy;
        r3[1] -= dx;
        r3[3] -= dx;
        r0[0] -= dy;
        r0[2] -= dy;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* Reveals the new picture in a rect that grows outward from the centre. */
void __cdecl VisualEffectIrisOpen(uint32_t *pRect, short stepCount) /* 0x0043B500 */
{
    short i;
    uint32_t tick;
    short r[4];
    short dy;
    short dx;
    short midY;
    short midX;
    int x2;
    short n2;

    /* One struct copy, then x2 read after midX: the exe's schedule loads r[3] last (MovieWipeBoxOut's twin). */
    tick = GetTick60Hz();
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
        BlitBackBufferRect((uint32_t *)r, (uint32_t *)r);
        r[0] -= dy;
        r[1] -= dx;
        r[2] += dy;
        r[3] += dx;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* Slides the new picture in downward from the top edge. */
void __cdecl VisualEffectScrollDown(uint32_t *pRect, short stepCount) /* 0x0043B5F0 */
{
    uint32_t tick;
    short step;
    short i;
    short p[4];
    short q[4];

    /* rects as in BarnDoorClose; copy order = the exe's stack layout */
    tick = GetTick60Hz();
    *(SceneViewport *)p = *(SceneViewport *)pRect;
    *(SceneViewport *)q = *(SceneViewport *)pRect;
    step = (p[2] - p[0]) / stepCount + 1;
    p[0] = p[2] - step;
    q[2] = q[0] + step;
    for (i = 0; i < stepCount; i++) {
        BlitBackBufferRect((uint32_t *)p, (uint32_t *)q);
        p[0] -= step;
        q[2] += step;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* Slides the new picture in upward from the bottom edge. */
void __cdecl VisualEffectScrollUp(uint32_t *pRect, short stepCount) /* 0x0043B6A0 */
{
    uint32_t tick;
    short step;
    short i;
    short p[4];
    short q[4];

    /* rects as in BarnDoorClose; copy order = the exe's stack layout */
    tick = GetTick60Hz();
    *(SceneViewport *)p = *(SceneViewport *)pRect;
    *(SceneViewport *)q = *(SceneViewport *)pRect;
    step = (p[2] - p[0]) / stepCount + 1;
    p[2] = p[0] + step;
    q[0] = q[2] - step;
    for (i = 0; i < stepCount; i++) {
        BlitBackBufferRect((uint32_t *)p, (uint32_t *)q);
        p[2] += step;
        q[0] -= step;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* Slides the new picture in rightward from the left edge. */
void __cdecl VisualEffectScrollRight(uint32_t *pRect, short stepCount) /* 0x0043B750 */
{
    uint32_t tick;
    short step;
    short i;
    short p[4];
    short q[4];

    /* rects as in BarnDoorClose; copy order = the exe's stack layout */
    tick = GetTick60Hz();
    *(SceneViewport *)p = *(SceneViewport *)pRect;
    *(SceneViewport *)q = *(SceneViewport *)pRect;
    step = (p[3] - p[1]) / stepCount + 1;
    p[1] = p[3] - step;
    q[3] = q[1] + step;
    for (i = 0; i < stepCount; i++) {
        BlitBackBufferRect((uint32_t *)p, (uint32_t *)q);
        p[1] -= step;
        q[3] += step;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* Slides the new picture in leftward from the right edge. */
void __cdecl VisualEffectScrollLeft(uint32_t *pRect, short stepCount) /* 0x0043B800 */
{
    uint32_t tick;
    short step;
    short i;
    short p[4];
    short q[4];

    /* rects as in BarnDoorClose; copy order = the exe's stack layout */
    tick = GetTick60Hz();
    *(SceneViewport *)p = *(SceneViewport *)pRect;
    *(SceneViewport *)q = *(SceneViewport *)pRect;
    step = (p[3] - p[1]) / stepCount + 1;
    p[3] = p[1] + step;
    q[1] = q[3] - step;
    for (i = 0; i < stepCount; i++) {
        BlitBackBufferRect((uint32_t *)p, (uint32_t *)q);
        p[3] += step;
        q[1] -= step;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* Wipes the new picture in as a band that sweeps from top to bottom; the picture does not move. */
void __cdecl VisualEffectWipeDown(uint32_t *pRect, short stepCount) /* 0x0043B8B0 */
{
    uint32_t tick;
    short step;
    short i;
    short r[4];     /* ONE 8-byte rect {y1,x1,y2,x2}: the callee reads both dwords */

    tick = GetTick60Hz();
    *(SceneViewport *)r = *(SceneViewport *)pRect;
    step = (r[2] - r[0]) / stepCount + 1;
    r[2] = r[0] + step;
    for (i = 0; i < stepCount; i++) {
        BlitBackBufferRect((uint32_t *)r, (uint32_t *)r);
        r[0] += step;
        r[2] += step;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* Wipes the new picture in as a band that sweeps from bottom to top. */
void __cdecl VisualEffectWipeUp(uint32_t *pRect, short stepCount) /* 0x0043B940 */
{
    short i;
    uint32_t tick;
    short step;
    short r[4];     /* ONE 8-byte rect {y1,x1,y2,x2}: the callee reads both dwords */

    tick = GetTick60Hz();
    *(SceneViewport *)r = *(SceneViewport *)pRect;
    step = (r[2] - r[0]) / stepCount + 1;
    r[0] = r[2] - step;
    for (i = 0; i < stepCount; i++) {
        BlitBackBufferRect((uint32_t *)r, (uint32_t *)r);
        r[0] -= step;
        r[2] -= step;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* Wipes the new picture in as a band that sweeps from left to right. */
void __cdecl VisualEffectWipeRight(uint32_t *pRect, short stepCount) /* 0x0043B9D0 */
{
    uint32_t tick;
    short step;
    short r[4];     /* ONE 8-byte rect {y1,x1,y2,x2}: the callee reads both dwords */
    short i;

    tick = GetTick60Hz();
    *(SceneViewport *)r = *(SceneViewport *)pRect;
    step = (r[3] - r[1]) / stepCount + 1;
    r[3] = r[1] + step;
    for (i = 0; i < stepCount; i++) {
        BlitBackBufferRect((uint32_t *)r, (uint32_t *)r);
        r[1] += step;
        r[3] += step;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* Wipes the new picture in as a band that sweeps from right to left. */
void __cdecl VisualEffectWipeLeft(uint32_t *pRect, short stepCount) /* 0x0043BA70 */
{
    uint32_t tick;
    short step;
    short i;
    short r[4];     /* ONE 8-byte rect {y1,x1,y2,x2}: the callee reads both dwords */

    tick = GetTick60Hz();
    *(SceneViewport *)r = *(SceneViewport *)pRect;
    step = (r[3] - r[1]) / stepCount + 1;
    r[1] = r[3] - step;
    for (i = 0; i < stepCount; i++) {
        BlitBackBufferRect((uint32_t *)r, (uint32_t *)r);
        r[1] -= step;
        r[3] -= step;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* Horizontal blinds: every stepCount + 1 rows a strip is revealed, one row taller on each tick. */
void __cdecl VisualEffectVenetian(short *pRect, short stepCount) /* 0x0043BB10 */
{
    uint32_t tick;
    short w;
    short y;
    short j;
    short r[4];
    short i;

    tick = GetTick60Hz();
    *(SceneViewport *)r = *(SceneViewport *)pRect;
    w = r[2] - r[0];
    for (i = 0; i < stepCount; i++) {
        j = 0;
        if (0 < w) {
            do {
                y = pRect[0] + j;
                r[0] = y;
                r[2] = y + i + 1;
                BlitBackBufferRect((uint32_t *)r, (uint32_t *)r);
                j = stepCount + 1 + j;
            } while (j < w);
        }
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect((uint32_t *)pRect, (uint32_t *)pRect);
}


/* Shows the new picture at once. Takes the tick count the other effects take (the dispatcher pushes
 * both) and ignores it. */
void __cdecl VisualEffectPlain(uint32_t *pRect, short stepCount) /* 0x0043BBB0 */
{
    BlitBackBufferRect(pRect, pRect);
}


/* Pushes the old picture off to the left (BlitScreenRect) while the new one enters from the right
 * (BlitBackBufferRect), in steps that sum to the rect's width. */
void __cdecl VisualEffectTurnRight(uint32_t *pRect, short stepCount) /* 0x0043BBC0 */
{
    short    step;
    short    i;
    short    rem;
    uint32_t tick;
    short    ra[4];
    short    rb[4];
    short    rc[4];
    short    rd[4];

    /* four rects, all copies of *pRect (class 3, as BarnDoorClose); copy order = the exe's stack layout */
    tick = GetTick60Hz();
    *(SceneViewport *)ra = *(SceneViewport *)pRect;
    *(SceneViewport *)rd = *(SceneViewport *)pRect;
    *(SceneViewport *)rc = *(SceneViewport *)pRect;
    *(SceneViewport *)rb = *(SceneViewport *)pRect;
    rem = ra[3] - ra[1];
    step = rem / stepCount + 1;
    for (i = 0; i < stepCount; i++) {
        if (rem < step)
            step = rem;
        rem -= step;
        rb[3] = ((short *)pRect)[3] - step;
        rc[1] = step;
        BlitScreenRect((uint32_t *)rc, (uint32_t *)rb);
        ra[3] = ra[1] + step;
        rd[1] = ((short *)pRect)[3] - step;
        BlitBackBufferRect((uint32_t *)ra, (uint32_t *)rd);
        ra[1] += step;
        WaitUntilTick(tick, i);
    }
}


/* TurnRight mirrored: the old picture is pushed off to the right and the new one enters from the
 * left. */
void __cdecl VisualEffectTurnLeft(uint32_t *pRect, short stepCount) /* 0x0043BCA0 */
{
    uint32_t tick;
    short    step;
    short    i;
    short    rem;
    short    ra[4];
    short    rb[4];
    short    rc[4];
    short    rd[4];

    /* four rects, all copies of *pRect (class 3, as BarnDoorClose); copy order = the exe's stack layout */
    tick = GetTick60Hz();
    *(SceneViewport *)ra = *(SceneViewport *)pRect;
    *(SceneViewport *)rd = *(SceneViewport *)pRect;
    *(SceneViewport *)rc = *(SceneViewport *)pRect;
    *(SceneViewport *)rb = *(SceneViewport *)pRect;
    rem = ra[3] - ra[1];
    step = rem / stepCount + 1;
    for (i = 0; i < stepCount; i++) {
        if (rem < step)
            step = rem;
        rem -= step;
        rc[3] = ((short *)pRect)[3] - step;
        rb[1] = step;
        BlitScreenRect((uint32_t *)rc, (uint32_t *)rb);
        rd[3] = step;
        ra[1] = ra[3] - step;
        BlitBackBufferRect((uint32_t *)ra, (uint32_t *)rd);
        ra[3] -= step;
        WaitUntilTick(tick, i);
    }
}


/* Pushes the old picture down while the new one enters from the top; ends with one full copy of the
 * new picture. */
void __cdecl VisualEffectTurnUp(uint32_t *pRect, short stepCount) /* 0x0043BD70 */
{
    short    step;
    short    r2[4];
    uint32_t tick;
    short    i;
    short    r3[4];
    short    r4[4];
    short    r1[4];

    /* four rects, all copies of *pRect (class 3, as BarnDoorClose); copy order = the exe's stack layout */
    tick = GetTick60Hz();
    *(SceneViewport *)r1 = *(SceneViewport *)pRect;
    *(SceneViewport *)r4 = *(SceneViewport *)pRect;
    *(SceneViewport *)r3 = *(SceneViewport *)pRect;
    *(SceneViewport *)r2 = *(SceneViewport *)pRect;
    step = (r1[2] - r1[0]) / stepCount + 1;
    r1[0] = r1[2] - step;
    r4[2] = r4[0] + step;
    r2[0] += step;
    r3[2] -= step;
    for (i = 0; i < stepCount; i++) {
        QdSetPort(&gChannels[1].hResource);
        BlitScreenRect((uint32_t *)r3, (uint32_t *)r2);
        BlitBackBufferRect((uint32_t *)r1, (uint32_t *)r4);
        r1[0] -= step;
        r4[2] += step;
        *(SceneViewport *)r3 = *(SceneViewport *)r2;
        r2[0] += step;
        r3[2] -= step;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* TurnUp mirrored: the old picture is pushed up and the new one enters from the bottom. */
void __cdecl VisualEffectTurnDown(uint32_t *pRect, short stepCount) /* 0x0043BE70 */
{
    uint32_t tick;
    short    step;
    short    i;
    short    r1[4];
    short    r2[4];
    short    r3[4];
    short    r4[4];

    /* four rects, all copies of *pRect (class 3, as BarnDoorClose); copy order = the exe's stack layout */
    tick = GetTick60Hz();
    *(SceneViewport *)r1 = *(SceneViewport *)pRect;
    *(SceneViewport *)r4 = *(SceneViewport *)pRect;
    *(SceneViewport *)r3 = *(SceneViewport *)pRect;
    *(SceneViewport *)r2 = *(SceneViewport *)pRect;
    step = (r1[2] - r1[0]) / stepCount + 1;
    r1[2] = r1[0] + step;
    r4[0] = r4[2] - step;
    r2[2] -= step;
    r3[0] += step;
    for (i = 0; i < stepCount; i++) {
        QdSetPort(&gChannels[1].hResource);
        BlitScreenRect((uint32_t *)r3, (uint32_t *)r2);
        BlitBackBufferRect((uint32_t *)r1, (uint32_t *)r4);
        r1[2] += step;
        r4[0] -= step;
        *(SceneViewport *)r3 = *(SceneViewport *)r2;
        r2[2] -= step;
        r3[0] += step;
        WaitUntilTick(tick, i);
    }
    BlitBackBufferRect(pRect, pRect);
}


/* TurnRight over half the width: the old picture moves left by half a rect while a middle slice of
 * the new one enters from the right. */
void __cdecl VisualEffectTurnHalfRight(uint32_t *pRect, short stepCount) /* 0x0043BF70 */
{
    uint32_t tick;
    short    step;
    short    i;
    short    rem;
    short    ra[4];
    short    rb[4];
    short    rc[4];
    short    rd[4];

    /* four rects, all copies of *pRect (class 3, as BarnDoorClose); copy order = the exe's stack layout */
    tick = GetTick60Hz();
    *(SceneViewport *)ra = *(SceneViewport *)pRect;
    *(SceneViewport *)rb = *(SceneViewport *)pRect;
    *(SceneViewport *)rc = *(SceneViewport *)pRect;
    *(SceneViewport *)rd = *(SceneViewport *)pRect;
    rem = (ra[3] - ra[1]) / 2;
    step = rem / stepCount + 1;
    ra[1] += rem / 2;
    ra[3] = ra[1];
    rb[1] = rb[3];
    for (i = 0; i < stepCount; i++) {
        if (rem < step)
            step = rem;
        rem -= step;
        rd[3] = ((short *)pRect)[3] - step;
        rc[1] = step;
        BlitScreenRect((uint32_t *)rc, (uint32_t *)rd);
        ra[3] = ra[1] + step;
        rb[1] = rb[3] - step;
        BlitBackBufferRect((uint32_t *)ra, (uint32_t *)rb);
        ra[1] += step;
        WaitUntilTick(tick, i);
    }
}


/* TurnLeft over half the width. */
void __cdecl VisualEffectTurnHalfLeft(uint32_t *pRect, short stepCount) /* 0x0043C080 */
{
    short    i;
    short    ra[4];
    uint32_t tick;
    short    rd[4];
    short    rb[4];
    short    step;
    short    rc[4];
    short    rem;

    /* four rects, all copies of *pRect (class 3, as BarnDoorClose); copy order = the exe's stack layout */
    tick = GetTick60Hz();
    *(SceneViewport *)ra = *(SceneViewport *)pRect;
    *(SceneViewport *)rb = *(SceneViewport *)pRect;
    *(SceneViewport *)rc = *(SceneViewport *)pRect;
    *(SceneViewport *)rd = *(SceneViewport *)pRect;
    rem = (ra[3] - ra[1]) / 2;
    step = rem / stepCount + 1;
    ra[3] -= rem / 2;
    ra[1] = ra[3];
    rb[3] = rb[1];
    for (i = 0; i < stepCount; i++) {
        if (rem < step)
            step = rem;
        rem -= step;
        rd[1] = step;
        rc[3] = ((short *)pRect)[3] - step;
        BlitScreenRect((uint32_t *)rc, (uint32_t *)rd);
        rb[3] = step;
        ra[1] = ra[3] - step;
        BlitBackBufferRect((uint32_t *)ra, (uint32_t *)rb);
        ra[3] -= step;
        WaitUntilTick(tick, i);
    }
}


/* Pumps messages until GetTick60Hz() reaches baseTick + extraTicks. */
void __cdecl WaitUntilTick(int baseTick, short extraTicks)  /* 0x0043c180 */
{
    do {
        PumpMessages();
    } while ((int)GetTick60Hz() < baseTick + (int)extraTicks);
}


/* Copies the back buffer (gSavedHotspotBuf) to channel B: the source rect pCoord1 and destination
 * rect pCoord2 are each clipped to gSceneViewport and nothing is copied if either is empty. */
void __cdecl BlitBackBufferRect(int *pCoord1, int *pCoord2)  /* 0x0043c1a0 */
{
    int pair1[2], pair2[2];

    pair1[0] = pCoord1[0];
    pair1[1] = pCoord1[1];
    pair2[0] = pCoord2[0];
    pair2[1] = pCoord2[1];

    if ((short)IntersectSceneRects(
            (short *)&gSceneViewport,
            (short *)pair1, pair1) != 0) {
        if ((short)IntersectSceneRects(
                (short *)&gSceneViewport,
                (short *)pair2, pair2) != 0) {
            QdSetPort(&gChannels[1].hResource);
            QdCopyBits(
                (int *)&gSavedHotspotBuf, &gChannels[1].hResource,
                (short *)pair1, (short *)pair2);
        }
    }
}


/* BlitBackBufferRect with channel B as both source and destination: copies a clipped rect of the
 * screen onto another. */
void __cdecl BlitScreenRect(uint32_t *pCoord1, uint32_t *pCoord2)  /* 0x0043c230 */
{
    uint32_t pair1[2], pair2[2];

    pair1[0] = pCoord1[0];  pair1[1] = pCoord1[1];
    pair2[0] = pCoord2[0];  pair2[1] = pCoord2[1];

    if ((short)IntersectSceneRects(
            (short *)&gSceneViewport,
            (short *)pair1, (int *)pair1) != 0) {
        if ((short)IntersectSceneRects(
                (short *)&gSceneViewport,
                (short *)pair2, (int *)pair2) != 0) {
            QdSetPort(&gChannels[1].hResource);
            QdCopyBits(
                &gChannels[1].hResource, &gChannels[1].hResource,
                (short *)pair1, (short *)pair2);
        }
    }
}

#undef entryHandle
#undef childId
#undef aliasHandle
#undef aliasPad
#undef aliasName

/* Runs the action command at the head of pArgBlock: 12001-12110 and 16001-16056 are the ActionCmd_*
 * handlers (the script keywords' opcodes). The block is the opcode entry, an 0xfb2 opener, the
 * handler's arguments and an 0xfb3 closer; the result is 0 or the handler's status, 1 for an unknown
 * opcode, 2 for a missing opener or closer. *pSkipCount receives the 8-byte entries consumed. Called
 * by InterpretStatements.
 *
 * Every handler takes (pCtx, pArgBlock, pTable, pSubSkip). Ghidra showed some with three parameters
 * and NULLs for the Eval helpers' out-pointers; the exe reads the fourth from [ESP+0x10] and pushes
 * pTable through (port-bug-classes.md #2). The status words the handlers share: 0x0e = an argument
 * of the wrong type (script value types: 2 bool, 3 string, 4 int), 0x1c = a missing 0xfb4 argument
 * separator. */
short __cdecl DispatchActionCommand(int pCtx, short *pArgBlock, int *pTable,  /* 0x0043c2c0 */
                                   int *pSkipCount)
{
    /* One switch over the opcodes in numeric order, the parameter walking past the opcode entry and the
     * opener with its start kept, every status through one test (byte match). The skip count is not
     * pre-set: every handler writes it, except ActionCmd_ExportClut (`xor ax,ax; ret`). */
    short  op;
    short *start;
    short  status;
    int    subSkip;        /* instructions consumed within the arg block */

    op = *pArgBlock;
    start = pArgBlock;
    pArgBlock += 4;
    if (*pArgBlock != 0xfb2)
    {
        DF_TRACE("!! err2 DispatchActionCommand: opcode %d has no arg-block opener (got %d)",
                 (int)op, (int)*pArgBlock);
        return 2;
    }
    pArgBlock += 4;

    DF_TRACE("action %d", (int)op);

    switch (op)
    {
        case 0x2EE1: status = ActionCmd_Message(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12001 message */
        case 0x2EE2: status = ActionCmd_HideCursor(pCtx, pArgBlock, pTable, &subSkip); break;                                             /* 12002 hidecursor */
        case 0x2EE3: status = ActionCmd_ShowCursor(pCtx, pArgBlock, pTable, &subSkip); break;                                    /* 12003 showcursor */
        case 0x2EE4: status = ActionCmd_Delay(pCtx, pArgBlock, pTable, &subSkip); break;       /* 12004 delay */
        case 0x2EE5: status = ActionCmd_MakeLoop(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 12005 makeloop */
        case 0x2EE6: status = ActionCmd_WalkToStar(pCtx, (int)pArgBlock, pTable, &subSkip); break;                 /* 12006 walktostar */
        case 0x2EE7: status = ActionCmd_MakeCricket(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 12007 makecricket */
        case 0x2EE8: status = ActionCmd_ExportClut(pCtx, pArgBlock, pTable, &subSkip); break;               /* 12008 exportclut */
        case 0x2EE9: status = ActionCmd_VisualEffect(pCtx, pArgBlock, pTable, &subSkip); break;       /* 12009 visualeffect */
        case 0x2EEA: status = ActionCmd_StopWalk(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12010 stopwalk */
        case 0x2EEB: status = ActionCmd_StopLoop((int *)pCtx, (int)pArgBlock, pTable, &subSkip); break;          /* 12011 stoploop */
        case 0x2EEC: status = ActionCmd_StopCricket(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12012 stopcricket */
        case 0x2EED: status = ActionCmd_OpenCastFile(pCtx, pArgBlock, pTable, &subSkip); break;     /* 12013 opencastfile */
        case 0x2EEE: status = ActionCmd_CloseCastFile(pCtx, pArgBlock, pTable, &subSkip); break;   /* 12014 closecastfile */
        case 0x2EEF: status = ActionCmd_ActorScript(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12015 actorscript */
        case 0x2EF0: status = ActionCmd_SendToActor(pCtx, (int)pArgBlock, pTable, &subSkip); break;                 /* 12016 sendtoactor */
        case 0x2EF1: status = ActionCmd_PlayMovie(pCtx, pArgBlock, pTable, &subSkip); break;      /* 12017 playmovie */
        case 0x2EF2: status = ActionCmd_OpenPuppetFile(pCtx, pArgBlock, pTable, &subSkip); break;        /* 12018 openpuppetfile */
        case 0x2EF3: status = ActionCmd_OpenTrackFile(pCtx, pArgBlock, pTable, &subSkip); break;     /* 12019 opentrackfile */
        case 0x2EF4: status = ActionCmd_CloseTrackFile(pCtx, pArgBlock, pTable, &subSkip); break;   /* 12020 closetrackfile */
        case 0x2EF5: status = ActionCmd_PlayTheme(pCtx, pArgBlock, pTable, &subSkip); break;     /* 12021 playtheme */
        case 0x2EF6: status = ActionCmd_SingleSound(pCtx, pArgBlock, pTable, &subSkip); break;      /* 12022 singlesound */
        case 0x2EF7: status = ActionCmd_MultipleSound(pCtx, pArgBlock, pTable, &subSkip); break;    /* 12023 multiplesound */
        case 0x2EF8: status = ActionCmd_DualSound(pCtx, pArgBlock, pTable, &subSkip); break;        /* 12024 dualsound */
        case 0x2EF9: status = ActionCmd_BothSound(pCtx, pArgBlock, pTable, &subSkip); break;        /* 12025 bothsound */
        case 0x2EFA: status = ActionCmd_VoiceSound(pCtx, pArgBlock, pTable, &subSkip); break;       /* 12026 voicesound */
        case 0x2EFB: status = ActionCmd_Plugin(pCtx, (int)pArgBlock, pTable, &subSkip); break;   /* 12027 plugin */
        case 0x2EFC: status = ActionCmd_HaltSound(pCtx, pArgBlock, pTable, &subSkip); break;        /* 12028 haltsound */
        case 0x2EFD: status = ActionCmd_HaltTheme(pCtx, pArgBlock, pTable, &subSkip); break;            /* 12029 halttheme */
        case 0x2EFE: status = ActionCmd_HaltVoice(pCtx, pArgBlock, pTable, &subSkip); break;            /* 12030 haltvoice */
        case 0x2EFF: status = ActionCmd_BootScript(pCtx, pArgBlock, pTable, &subSkip); break;           /* 12031 bootscript */
        case 0x2F00: status = ActionCmd_OpenSetFile(pCtx, pArgBlock, pTable, &subSkip); break;           /* 12032 opensetfile */
        case 0x2F01: status = ActionCmd_CloseSetFile(pCtx, pArgBlock, pTable, &subSkip); break;                                             /* 12033 closesetfile */
        case 0x2F02: status = ActionCmd_SendToScene(pCtx, (int)pArgBlock, pTable, &subSkip); break;      /* 12034 sendtoscene */
        case 0x2F03: status = ActionCmd_SetScript(pCtx, pArgBlock, pTable, &subSkip); break;                                             /* 12035 setscript */
        case 0x2F04: status = ActionCmd_SceneScript(pCtx, pArgBlock, pTable, &subSkip); break;      /* 12036 scenescript */
        case 0x2F05: status = ActionCmd_PaintingScript(pCtx, (int)pArgBlock, pTable, &subSkip); break;                         /* 12037 paintingscript */
        case 0x2F06: status = ActionCmd_Clut(pCtx, pArgBlock, pTable, &subSkip); break;         /* 12038 clut */
        case 0x2F07: status = ActionCmd_Cursor(pCtx, pArgBlock, pTable, &subSkip); break;          /* 12039 cursor */
        case 0x2F08: status = ActionCmd_Debugger(pCtx, pArgBlock, pTable, &subSkip); break;                                               /* 12040 debugger */
        case 0x2F09: status = ActionCmd_PuppetClear(pCtx, pArgBlock, pTable, &subSkip); break;                                     /* 12041 puppetclear */
        case 0x2F0A: status = ActionCmd_ClosePuppetFile(pCtx, pArgBlock, pTable, &subSkip); break;                                        /* 12042 closepuppetfile */
        case 0x2F0B: status = ActionCmd_PuppetSpeak(pCtx, pArgBlock, pTable, &subSkip); break;    /* 12043 puppetspeak */
        case 0x2F0C: status = ActionCmd_PuppetBevel(pCtx, (int)pArgBlock, pTable, &subSkip); break;                    /* 12044 puppetbevel */
        case 0x2F0D: status = ActionCmd_SendToPuppet(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 12045 sendtopuppet */
        case 0x2F0E: status = ActionCmd_PuppetScript(pCtx, pArgBlock, pTable, &subSkip); break; /* 12046 puppetscript */
        case 0x2F0F: status = ActionCmd_CastScript(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12047 castscript */
        case 0x2F10: status = ActionCmd_SendToCast(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 12048 sendtocast */
        case 0x2F11: status = ActionCmd_BlackToScreen(pCtx, (int)pArgBlock, pTable, &subSkip); break;                          /* 12049 blacktoscreen */
        case 0x2F12: status = ActionCmd_ScreenToBlack(pCtx, (int)pArgBlock, pTable, &subSkip); break;                          /* 12050 screentoblack */
        case 0x2F13: status = ActionCmd_BlackScreen(pCtx, pArgBlock, pTable, &subSkip); break;                                              /* 12051 blackscreen */
        case 0x2F14: status = ActionCmd_ForceUpdate(pCtx, pArgBlock, pTable, &subSkip); break;                                              /* 12052 forceupdate */
        case 0x2F15: status = ActionCmd_Error(pCtx, pArgBlock, pTable, &subSkip); break;                                              /* 12053 error */
        case 0x2F16: status = ActionCmd_PropScript(pCtx, pArgBlock, pTable, &subSkip); break; /* 12054 propscript */
        case 0x2F17: status = ActionCmd_SendToProp(pCtx, (int)pArgBlock, pTable, &subSkip); break;   /* 12055 sendtoprop */
        case 0x2F18: status = ActionCmd_OpenShopFile(pCtx, pArgBlock, pTable, &subSkip); break;     /* 12056 openshopfile */
        case 0x2F19: status = ActionCmd_CloseShopFile(pCtx, pArgBlock, pTable, &subSkip); break;   /* 12057 closeshopfile */
        case 0x2F1A: status = ActionCmd_ShopScript(pCtx, pArgBlock, pTable, &subSkip); break;   /* 12058 shopscript */
        case 0x2F1B: status = ActionCmd_SendToShop(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 12059 sendtoshop */
        case 0x2F1C: status = ActionCmd_OpenStageFile(pCtx, pArgBlock, pTable, &subSkip); break;          /* 12060 openstagefile */
        case 0x2F1D: status = ActionCmd_CloseStageFile(pCtx, pArgBlock, pTable, &subSkip); break;                                               /* 12061 closestagefile */
        case 0x2F1E: status = ActionCmd_GotoFlat(pCtx, pArgBlock, pTable, &subSkip); break;     /* 12062 gotoflat */
        case 0x2F1F: status = ActionCmd_StageScript(pCtx, pArgBlock, pTable, &subSkip); break;                               /* 12063 stagescript */
        case 0x2F20: status = ActionCmd_FlatScript(pCtx, pArgBlock, pTable, &subSkip); break; /* 12064 flatscript */
        case 0x2F21: status = ActionCmd_ButtonScript(pCtx, (int)pArgBlock, pTable, &subSkip); break;          /* 12065 buttonscript */
        case 0x2F22: status = ActionCmd_SendToPainting(pCtx, (int)pArgBlock, pTable, &subSkip); break;    /* 12066 sendtopainting */
        case 0x2F23: status = ActionCmd_SendToSet(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12067 sendtoset */
        case 0x2F24: status = ActionCmd_SendToButton(pCtx, (int)pArgBlock, pTable, &subSkip); break;   /* 12068 sendtobutton */
        case 0x2F25: status = ActionCmd_SendToFlat(pCtx, (int)pArgBlock, pTable, &subSkip); break;  /* 12069 sendtoflat */
        case 0x2F26: status = ActionCmd_SendToStage(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 12070 sendtostage */
        case 0x2F27: status = ActionCmd_Quit(pCtx, pArgBlock, pTable, &subSkip); break;                                     /* 12071 quit */
        case 0x2F28: status = ActionCmd_TurnToDeg(pCtx, (int)pArgBlock, pTable, &subSkip); break;  /* 12072 turntodeg */
        case 0x2F29: status = ActionCmd_FlushEvents(pCtx, pArgBlock, pTable, &subSkip); break;                                  /* 12073 flushevents */
        case 0x2F2A: status = ActionCmd_PuppetGrab(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12074 puppetgrab */
        case 0x2F2B: status = ActionCmd_ActorInstance(pCtx, (int)pArgBlock, pTable, &subSkip); break;            /* 12075 actorinstance */
        case 0x2F2C: status = ActionCmd_PropInstance(pCtx, (int)pArgBlock, pTable, &subSkip); break;    /* 12076 propinstance */
        case 0x2F2D: status = ActionCmd_SaveGame(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12077 savegame */
        case 0x2F2E: status = ActionCmd_OpenGame(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12078 opengame */
        case 0x2F2F: status = ActionCmd_NoteDialog(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12079 notedialog */
        case 0x2F30: status = ActionCmd_DrawString(pCtx, (int)pArgBlock, pTable, &subSkip); break;  /* 12080 drawstring */
        case 0x2F31: status = ActionCmd_SendToBoot((int *)pCtx, pArgBlock, pTable, &subSkip); break;  /* 12081 sendtoboot */
        case 0x2F32: status = ActionCmd_MixClut(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 12082 mixclut 0x0043dbc0 */
        case 0x2F33: status = ActionCmd_PuppetScramble(pCtx, pArgBlock, pTable, &subSkip); break;                              /* 12083 puppetscramble */
        case 0x2F34: status = ActionCmd_PuppetSubtitle(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12084 puppetsubtitle */
        case 0x2F35: status = ActionCmd_ActorWarm(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12085 actorwarm */
        case 0x2F36: status = ActionCmd_PropWarm(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12086 propwarm */
        case 0x2F37: status = ActionCmd_ShopWarm(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12087 shopwarm */
        case 0x2F38: status = ActionCmd_CastWarm(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12088 castwarm */
        case 0x2F39: status = ActionCmd_PostScript(pCtx, pArgBlock, pTable, &subSkip); break;   /* 12089 postscript */
        case 0x2F3A: status = ActionCmd_SendToPost(pCtx, pArgBlock, pTable, &subSkip); break; /* 12090 sendtopost */
        case 0x2F3B: status = ActionCmd_ServerScript(pCtx, pArgBlock, pTable, &subSkip); break; /* 12091 serverscript */
        case 0x2F3C: status = ActionCmd_SendToServer(pCtx, pArgBlock, pTable, &subSkip); break; /* 12092 sendtoserver */
        case 0x2F3D: status = ActionCmd_ActorDelete(pCtx, pArgBlock, pTable, &subSkip); break; /* 12093 actordelete */
        case 0x2F3E: status = ActionCmd_PropDelete(pCtx, pArgBlock, pTable, &subSkip); break; /* 12094 propdelete */
        case 0x2F3F: status = ActionCmd_NetConnect(pCtx, pArgBlock, pTable, &subSkip); break;                        /* 12095 netconnect */
        case 0x2F40: status = ActionCmd_NetDisconnect(pCtx, pArgBlock, pTable, &subSkip); break;  /* 12096 netdisconnect */
        case 0x2F41: status = ActionCmd_NetBroadcast(pCtx, pArgBlock, pTable, &subSkip); break; /* 12097 netbroadcast */
        case 0x2F42: status = ActionCmd_NetJoinGroup(pCtx, pArgBlock, pTable, &subSkip); break; /* 12098 netjoingroup */
        case 0x2F43: status = ActionCmd_NetLeaveGroup(pCtx, pArgBlock, pTable, &subSkip); break; /* 12099 netleavegroup */
        case 0x2F44: status = ActionCmd_WalkOnPath(pCtx, (int)pArgBlock, pTable, &subSkip); break;         /* 12100 walkonpath */
        case 0x2F45: status = ActionCmd_WalkToXyz(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 12101 walktoxyz */
        case 0x2F46: status = ActionCmd_WalkOnRoad(pCtx, (int)pArgBlock, pTable, &subSkip); break;            /* 12102 walkonroad */
        case 0x2F47: status = ActionCmd_WalkOnFrames(pCtx, (int)pArgBlock, pTable, &subSkip); break;            /* 12103 walkonframes */
        case 0x2F48: status = ActionCmd_ActorHide(pCtx, (int)pArgBlock, pTable, &subSkip); break;  /* 12104 actorhide */
        case 0x2F49: status = ActionCmd_PropHide(pCtx, (int)pArgBlock, pTable, &subSkip); break;  /* 12105 prophide */
        case 0x2F4A: status = ActionCmd_Reboot(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 12106 reboot */
        case 0x2F4B: status = ActionCmd_ActorLock(pCtx, (int)pArgBlock, pTable, &subSkip); break;  /* 12107 actorlock */
        case 0x2F4C: status = ActionCmd_PropLock(pCtx, (int)pArgBlock, pTable, &subSkip); break;                            /* 12108 proplock */
        case 0x2F4D: status = ActionCmd_ShopLock(pCtx, (int)pArgBlock, pTable, &subSkip); break;                            /* 12109 shoplock */
        case 0x2F4E: status = ActionCmd_CastLock(pCtx, (int)pArgBlock, pTable, &subSkip); break;                            /* 12110 castlock */

        /* 16001: special action command. &subSkip, like every other handler
         * here -- the handler reports how many entries it consumed INSIDE
         * the arg block; the dispatcher then locates the 0xfb3 closer and
         * writes the real *pSkipCount itself. Passing pSkipCount left
         * subSkip at 0 and the closer check landed on 0xfbb, giving err=2
         * "Expected paren". */
        case 0x3E81: status = ActionCmd_ActorVisible(pCtx, (int)pArgBlock,
                                                                      pTable, &subSkip); break;

        case 0x3E82: status = ActionCmd_ActorDeg(pCtx, (int)pArgBlock, pTable, &subSkip); break;                            /* 16002 actordeg */
        case 0x3E83: status = ActionCmd_ActorXyz(pCtx, (int)pArgBlock, pTable, &subSkip); break;                            /* 16003 actorxyz */
        case 0x3E84: status = ActionCmd_ActorXy(pCtx, (int)pArgBlock, pTable, &subSkip); break;                            /* 16004 actorxy */
        case 0x3E85: status = ActionCmd_ActorIs3d(pCtx, (int)pArgBlock, pTable, &subSkip); break;                            /* 16005 actoris3d */
        case 0x3E86: status = ActionCmd_ActorStar(pCtx, (int)pArgBlock, pTable, &subSkip); break;  /* 16006 actorstar */
        case 0x3E87: status = ActionCmd_SetVisible(pCtx, pArgBlock, pTable, &subSkip); break;  /* 16007 setvisible */
        case 0x3E88: status = ActionCmd_StageVisible(pCtx, pArgBlock, pTable, &subSkip); break; /* 16008 stagevisible */
        case 0x3E89: status = ActionCmd_Path(pCtx, (int)pArgBlock, pTable, &subSkip); break;          /* 16009 path */
        case 0x3E8A: status = ActionCmd_Result(pCtx, pArgBlock, pTable, &subSkip); break; /* 16010 result */
        case 0x3E8B: status = ActionCmd_CurrentView(pCtx, pArgBlock, pTable, &subSkip); break; /* 16011 currentview */
        case 0x3E8C: status = ActionCmd_ActorDist(pCtx, (int)pArgBlock, pTable, &subSkip); break;      /* 16012 actordist */
        case 0x3E8D: status = ActionCmd_PropDist(pCtx, (int)pArgBlock, pTable, &subSkip); break;      /* 16013 propdist */
        case 0x3E8E: status = ActionCmd_ActorPose(pCtx, (int)pArgBlock, pTable, &subSkip); break;      /* 16014 actorpose */
        case 0x3E8F: status = ActionCmd_PropVisible(pCtx, (int)pArgBlock, pTable, &subSkip); break;   /* 16015 propvisible */
        case 0x3E90: status = ActionCmd_PropDeg(pCtx, (int)pArgBlock, pTable, &subSkip); break;    /* 16016 propdeg */
        case 0x3E91: status = ActionCmd_PropXyz(pCtx, (int)pArgBlock, pTable, &subSkip); break;      /* 16017 propxyz */
        case 0x3E92: status = ActionCmd_PropXy(pCtx, (int)pArgBlock, pTable, &subSkip); break;            /* 16018 propxy */
        case 0x3E93: status = ActionCmd_PropIs3d(pCtx, (int)pArgBlock, pTable, &subSkip); break;    /* 16019 propis3d */
        case 0x3E94: status = ActionCmd_PropStar(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16020 propstar */
        case 0x3E95: status = ActionCmd_ActorSet(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16021 actorset -- same logic as 16006, own body (0x0040e150) */
        case 0x3E96: status = ActionCmd_Framerate(pCtx, pArgBlock, pTable, &subSkip); break; /* 16022 framerate */
        case 0x3E97: status = ActionCmd_ActorSpeed(pCtx, (int)pArgBlock, pTable, &subSkip); break;   /* 16023 actorspeed */
        case 0x3E98: status = ActionCmd_ActorScale(pCtx, (int)pArgBlock, pTable, &subSkip); break;          /* 16024 actorscale */
        case 0x3E99: status = ActionCmd_PropView(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16025 propview */
        case 0x3E9A: status = ActionCmd_PropSpeed(pCtx, (int)pArgBlock, pTable, &subSkip); break;   /* 16026 propspeed */
        case 0x3E9B: status = ActionCmd_PropSet(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16027 propset -- same as 16020, own body (0x00415e20) */
        case 0x3E9C: status = ActionCmd_PropScale(pCtx, (int)pArgBlock, pTable, &subSkip); break;   /* 16028 propscale */
        case 0x3E9D: status = ActionCmd_CurrentScene(pCtx, pArgBlock, pTable, &subSkip); break; /* 16029 currentscene */
        case 0x3E9E: status = ActionCmd_Variable(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16030 variable */
        case 0x3E9F: status = ActionCmd_CurrentDeg(pCtx, pArgBlock, pTable, &subSkip); break; /* 16031 currentdeg */
        case 0x3EA0: status = ActionCmd_PropOwner(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16032 propowner */
        case 0x3EA1: status = ActionCmd_WaveVolume(pCtx, pArgBlock, pTable, &subSkip); break; /* 16033 wavevolume */
        case 0x3EA2: status = ActionCmd_CurrentCd(pCtx, pArgBlock, pTable, &subSkip); break;       /* 16034 currentcd */
        case 0x3EA3: status = ActionCmd_CameraHi(pCtx, pArgBlock, pTable, &subSkip); break;         /* 16035 camerahi */
        case 0x3EA4: status = ActionCmd_ActorTurn(pCtx, (int)pArgBlock, pTable, &subSkip); break;       /* 16036 actorturn */
        case 0x3EA5: status = ActionCmd_MenuVisible(pCtx, pArgBlock, pTable, &subSkip); break;         /* 16037 menuvisible */
        case 0x3EA6: status = ActionCmd_SoundPan(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16038 soundpan */
        case 0x3EA7: status = ActionCmd_SoundLoop(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16039 soundloop */
        case 0x3EA8: status = ActionCmd_SoundVol(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16040 soundvol */
        case 0x3EA9: status = ActionCmd_ThemeVol(pCtx, (int)pArgBlock, pTable, &subSkip); break;              /* 16041 themevol */
        case 0x3EAA: status = ActionCmd_PropValue(pCtx, (int)pArgBlock, pTable, &subSkip); break;            /* 16042 propvalue */
        case 0x3EAB: status = ActionCmd_ActorOwner(pCtx, (int)pArgBlock, pTable, &subSkip); break;              /* 16043 actorowner */
        case 0x3EAC: status = ActionCmd_ActorValue(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16044 actorvalue (0x00410ab0) */
        case 0x3EAD: status = ActionCmd_KeyAborts(pCtx, pArgBlock, pTable, &subSkip); break; /* 16045 keyaborts */
        case 0x3EAE: status = ActionCmd_PauseLoop(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16046 pauseloop */
        case 0x3EAF: status = ActionCmd_PauseCricket(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16047 pausecricket */
        case 0x3EB0: status = ActionCmd_PauseWalk(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16048 pausewalk */
        case 0x3EB1: status = (uint)(unsigned short)ActionCmd_PuppetParam(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16049 puppetparam */
        case 0x3EB2: status = ActionCmd_PuppetVisible(pCtx, pArgBlock, pTable, &subSkip); break; /* 16050 puppetvisible */
        case 0x3EB3: status = ActionCmd_ActorZclip(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16051 actorzclip */
        case 0x3EB4: status = ActionCmd_PropZclip(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16052 propzclip */
        case 0x3EB5: status = ActionCmd_PuppetBase(pCtx, pArgBlock, pTable, &subSkip); break; /* 16053 puppetbase */
        case 0x3EB6: status = ActionCmd_ThemePan(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16054 themepan */
        case 0x3EB7: status = (uint)(unsigned short)ActionCmd_SetParam(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16055 setparam */
        case 0x3EB8: status = (uint)(unsigned short)ActionCmd_StageParam(pCtx, (int)pArgBlock, pTable, &subSkip); break; /* 16056 stageparam */
        default:
            DF_TRACE("!! DispatchActionCommand: NO HANDLER for opcode %d", (int)op);
            status = 1;   /* unknown command */
            break;
    }

    if (status != 0) {
        DF_TRACE_TOKENS("   failing args:", pArgBlock, 20);
        return status;
    }

    pArgBlock += subSkip * 4;
    if (*pArgBlock != 0xfb3)
    {
        DF_TRACE("!! DispatchActionCommand: opcode %d missing block closer "
                 "(subSkip=%d, got %d)", (int)op, subSkip, (int)*pArgBlock);
        return 2;   /* missing block closer */
    }
    pArgBlock += 4;
    *pSkipCount = ((int)pArgBlock - (int)start) >> 3;
    return 0;
}


/* menuvisible (16037): shows the menu bar for a true bool argument (RestoreMenuBar), hides it for
 * false (HideMenuBar). */
short __cdecl
ActionCmd_MenuVisible(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043D6C0 */
{
    short  u;
    short res[4];

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)res, pSubSkip);
    if (u != 0)
        return u;
    if (res[0] != 2)
        return 0x0e;
    if (*(int *)(res + 1) != 0) {
        RestoreMenuBar();
        return 0;
    }
    HideMenuBar();
    return 0;
}


/* drawstring (12080): draws a string on channel B. Arguments: the text, the pen point (x in the high
 * word), the font and the size; the pen is reset to normal and the scene colour index to 0 first. */
short __cdecl ActionCmd_DrawString(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043d720 */
{
    short  s1;
    short  o1[4], o2[4], o3[4], o4[4];
    short  pen[2];

    s1 = EvalFourScriptArgs(pCtx, pArgBlock, pTable, o1, o2, o3, o4, pSubSkip);
    if (s1 != 0)
        return s1;
    s1 = TakeScriptString(o1, gScratchStr);
    if (s1 != 0)
        return s1;
    /* arguments 2-4: the pen point (x in the high word), the column width and
     * the row count -- drawstring (text, point, 0, 12) */
    *(int *)pen = *(int *)(o2 + 1);
    QdSetPort((int *)&gChannels[1].hResource); /* channel B hResource -- same as ChannelState+1 */
    QdMoveTo(pen[1], pen[0]);
    QdTextFont((short)*(int *)(o3 + 1));
    QdTextSize((short)*(int *)(o4 + 1));
    QdPenNormal();
    SetSceneNodeColorFromPalette(0);
    QdDrawString(&gScratchStr);
    return 0;
}


/* camerahi (16035): sets gCameraHeight from one int argument. */
short __cdecl
ActionCmd_CameraHi(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043D7E0 */
{
    short  u;
    short res[4];
    int   idWord;

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)res, pSubSkip);
    if (u != 0)
        return u;
    if (res[0] != 4)
        return 0x0e;
    idWord = *(int *)&res[1];
    gCameraHeight = (short)idWord;
    return 0;
}


/* note (12079): shows the string argument in a task-modal message box
 * (ShowEngineMessageBoxFromPascalString).
 *
 * Port note: a stub once shadowed that function and the box never appeared. */
short __cdecl
ActionCmd_NoteDialog(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip) /* 0x0043d830 */
{
    short u;
    short a[4];

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)a, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a, gScratchStr);
    if (u != 0)
        return u;
    ShowEngineMessageBoxFromPascalString(&gScratchStr[0]);
    return 0;
}


/* message (12001): evaluates one argument and stores it as a string in gMessageStr. */
short __cdecl ActionCmd_Message(int pCtx, short *pArgBlock,  /* 0x0043d890 */
                                         int *pTable, int *pSubSkip)
{
    short   status;
    short  argResult[4];   /* 8-byte typed result */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, gMessageStr);   /* byte[256] */
    if (status != 0)
        return status;

    return 0;
}


/* path (16009): path(slot, directory) stores the string in search-path slot 1..8
 * (SetSearchPathSlot); status 10 if the slot is out of range.
 *
 * Port note: the two arguments were once swapped. Which result buffer holds which argument differs
 * between two-argument handlers (the order the exe LEAs its locals) and has to be read off the
 * disassembly. */
short __cdecl
ActionCmd_Path(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043D8F0 */
{
    short  u;
    short  a1[4], a2[4];

    u = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    if (u != 0)
        return u;
    if (a1[0] != 4)
        return 0x0e;
    if (*(int *)&a1[1] < 1 || *(int *)&a1[1] > 8)
        return 10;
    u = TakeScriptString(a2, (byte *)gScratchStr);
    if (u != 0)
        return u;
    SetSearchPathSlot((short)*(int *)&a1[1], (unsigned char *)gScratchStr);
    return 0;
}


/* wavevolume (16033): sets the wave volume, an int 0..9 (SetWaveVolumeIfDeviceReady); status 10
 * outside that range. */
short __cdecl
ActionCmd_WaveVolume(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043D980 */
{
    short   u;
    short  res[4];

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)res, pSubSkip);
    if (u != 0) {
        DF_TRACE("vol: wavevolume SET -- arg block failed, err=%u", (unsigned)(u & 0xffff));
        return u;
    }
    if (res[0] != 4) {
        DF_TRACE("vol: wavevolume SET -- arg type %d, wanted 4 (int) -> err 0x0e", (int)res[0]);
        return 0x0e;
    }
    DF_TRACE("vol: wavevolume SET level=%d", *(int *)&res[1]);
    if (*(int *)&res[1] >= 0 && *(int *)&res[1] <= 9) {   /* cmp [esp+2],0 / jl; cmp [esp+2],9 / jg */
        /* the level is the argument; the port once validated it and dropped it (class 2) */
        SetWaveVolumeIfDeviceReady((short)*(int *)&res[1]);
        return 0U;
    }
    DF_TRACE("vol: wavevolume SET level=%d OUT OF RANGE (0..9) -> err 10", *(int *)&res[1]);
    return 10;
}


/* result (16010): stores its string argument in gScriptResult and returns that conversion's status. */
short __cdecl
ActionCmd_Result(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043D9F0 */
{
    short  typed[4];
    short  u;

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSubSkip);
    if (u != 0)
        return u;
    return TakeScriptString(typed, (byte *)gScriptResult);   /* the exe returns the status as is */
}


/* hidecursor (12002): hides the cursor (QdHideCursor). Takes no arguments. */
short ActionCmd_HideCursor(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043da30 */
{
    *pSubSkip = 0;
    QdHideCursor();
    return 0;
}


#undef ML_NAME1
#undef ML_NAME2

/* visualeffect (12009): visualeffect(cue, ticks). The first entry is the effect's cue id
 * (0x5dc1..0x5dd5, else status 0x0e), then an 0xfb4 separator (else 0x1c) and an int tick count,
 * clamped to 1..1000. Requests a whole-scene redraw, ticks the script bindings once with
 * gVisualEffectDepth raised so RenderSceneFrame does not copy to the screen, then runs the
 * transition over the scene viewport (DispatchVisualEffect). */
short __cdecl ActionCmd_VisualEffect(int pCtx, short *pArgBlock,  /* 0x0043da50 */
                                    int *pTable, int *pSubSkip)
{
    typedef struct { short w[4]; } ArgEnt;   /* one 8-byte argument entry */
    short   status;
    ArgEnt *base;
    short   cueId;
    short   argResult[4];  /* evaluated second argument result */
    int     skipPos;
    ArgEnt  first;         /* the inline first entry, copied whole (only its first dword is live) */

    base = (ArgEnt *)pArgBlock;
    first = *(ArgEnt *)pArgBlock;
    pArgBlock += 4;
    cueId = first.w[0];
    if (cueId <= 0x5DC0 || cueId >= 0x5DD6)
        return 0x0e;
    if (*pArgBlock != 0x0FB4)
        return 0x1c;
    pArgBlock += 4;
    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, &skipPos);
    if (status != 0)
        return status;
    pArgBlock += skipPos * 4;
    *pSubSkip = (ArgEnt *)pArgBlock - base;
    if (argResult[0] != 4)
        return 0x0e;
    if (*(int *)(argResult + 1) < 1)
        *(int *)(argResult + 1) = 1;
    if (*(int *)(argResult + 1) > 1000)
        *(int *)(argResult + 1) = 1000;
    BumpSceneOverlayModeFloor(2);
    gVisualEffectDepth++;
    status = TickScriptBindings();
    gVisualEffectDepth--;
    if (status != 0)
        return status;
    DispatchVisualEffect((short *)&gSceneViewport, (short)*(int *)(argResult + 1), cueId);  /* 0x0043B000 */
    return 0;
}


/* clut (12038): resolves its argument to a palette (an id, a built-in name or a named resource:
 * ResolveClutFromScriptValue), loads it with ReloadCLUT and frees it. */
short __cdecl ActionCmd_Clut(int pCtx, short *pArgBlock,  /* 0x0043db50 */
                                  int *pTable, int *pSubSkip)
{
    short   status;
    int   *pHandle;
    short  argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                               (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    status = ResolveClutFromScriptValue(argResult, &pHandle);
    if (status != 0)
        return status;

    ReloadCLUT(pHandle);
    FreeResObject(pHandle);
    return 0;
}


/* mixclut (12082): mixclut(clutA, clutB, start, end, mix). For colour entries start..end (each
 * clamped to 0..255) the three 16-bit channels of the 6-byte colour at +0xa + 8*i of each palette's
 * data are blended as a + (b - a) * mix / 255 (mix clamped to 0..255), into a clone of clutA, which
 * is then loaded with ReloadCLUT. The script arguments are a pair, an 0xfb4 separator and a triple;
 * the skip count is the sum the evaluators report. */
short __cdecl
ActionCmd_MixClut(char *pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043dbc0 */
{
    typedef struct { unsigned short r, g, b; } RGB6;
    short  s;
    int   *hA;
    int    k;
    int   *hB;
    int   *hW;
    int    baseA;
    int    start;       /* the parameter walks; this keeps its start */
    int    skip;        /* both evaluators' skip count (one slot) */
    short  a1[4];
    short  a2[4];
    RGB6   ca;
    RGB6   cb;
    RGB6   out;
    short  a3[4];
    int    cnt;
    int    v;
    int    i0, i1;
    int    baseW, baseB;
    char  *pA, *pB, *pOut;

    start = pArgBlock;
    s = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, &skip);
    if (s != 0)
        return s;

    s = (short)ResolveClutFromScriptValue(a1, &hA);
    if (s != 0)
        return s;
    s = (short)ResolveClutFromScriptValue(a2, &hB);
    if (s != 0) {
        FreeResObject(hA);
        return s;
    }

    pArgBlock += skip * 8;
    if (*(short *)pArgBlock != (short)0x0fb4) {
        FreeResObject(hB);
        FreeResObject(hA);
        return 0x1c;
    }
    pArgBlock += 8;

    s = EvalThreeScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, (int *)a3, &skip);
    if (s != 0) {
        FreeResObject(hB);
        FreeResObject(hA);
        return s;
    }
    if (a1[0] != 4 || a2[0] != 4 || a3[0] != 4) {
        FreeResObject(hB);
        FreeResObject(hA);
        return 0x0e;
    }
    pArgBlock += skip * 8;
    *pSubSkip = (pArgBlock - start) >> 3;

    i0 = *(int *)&a1[1];
    if (i0 < 0) i0 = 0;
    if (i0 > 0xff) i0 = 0xff;
    i1 = *(int *)&a2[1];
    if (i1 < 0) i1 = 0;
    if (i1 > 0xff) i1 = 0xff;
    k = *(int *)&a3[1];
    if (k < 0) k = 0;
    if (k > 0xff) k = 0xff;

    hW = hA;
    CloneResource((void **)&hW);
    baseA = GetResDataPtr(hA);
    baseW = GetResDataPtr(hW);
    baseB = GetResDataPtr(hB);
    if (i0 <= i1) {
        pOut = (char *)baseW + i0 * 8 + 0xa;
        pB   = (char *)baseB + i0 * 8 + 0xa;
        pA   = (char *)baseA + i0 * 8 + 0xa;
        cnt  = i1 - i0 + 1;
        do {
            ca = *(RGB6 *)pA;
            cb = *(RGB6 *)pB;
            v = ca.r; v = (cb.r - v) * k / 0xff + v; out.r = v;
            v = ca.g; v = (cb.g - v) * k / 0xff + v; out.g = v;
            v = ca.b; v = (cb.b - v) * k / 0xff + v; out.b = v;
            *(RGB6 *)pOut = out;
            pOut += 8;
            pB += 8;
            pA += 8;
        } while (--cnt != 0);
    }
    UnlockResObject(hA);
    UnlockResObject(hW);
    UnlockResObject(hB);
    ReloadCLUT(hW);
    FreeResObject(hW);
    FreeResObject(hB);
    FreeResObject(hA);
    return 0;
}


/* framerate (16022): sets gFrameRate (ticks of 60 Hz per frame), an int clamped to 0..0x3c. */
short __cdecl
ActionCmd_Framerate(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043DEF0 */
{
    short  typed[4];
    short   u;

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)typed, pSubSkip);
    if (u != 0)
        return u;
    if (typed[0] != 4)
        return 0x0e;
    gFrameRate = *(int *)(typed + 1);
    if (gFrameRate < 0)
        gFrameRate = 0;
    if (gFrameRate > 0x3c)
        gFrameRate = 0x3c;
    return 0;
}


/* forceupdate (12052): runs TickScriptBindings over the three binding tables. Takes no arguments. */
short __cdecl ActionCmd_ForceUpdate(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043df60 */
{
    (void)pCtx; (void)pArgBlock; (void)pTable;
    *pSubSkip = 0;
    return TickScriptBindings();
}


/* cursor (12039): sets the cursor. An int loads the CURS resource with that id; the string "arrow"
 * installs the system arrow (QdInitCursor), "watch" restores the game cursor (gCursor), any other
 * string loads the CURS resource of that name. Status 0x2b if the resource is missing. */
short __cdecl ActionCmd_Cursor(int pCtx, short *pArgBlock,  /* 0x0043df70 */
                                 int *pTable, int *pSubSkip)
{
    short   u;
    HCURSOR hCursor;
    short   argResult[4];   /* [0]=type, [1..2]=value */

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSubSkip);
    if (u != 0)
        return u;

    if (argResult[0] != 4) {
        u = TakeScriptString(argResult, gScratchStr);
        if (u != 0)
            return u;
        if (PascalStringsEqualNoCase((byte *)UpdatePascalLength(kStrArrow), gScratchStr)) {  /* "arrow" */
            QdInitCursor();
            return 0;
        }
        if (PascalStringsEqualNoCase((byte *)UpdatePascalLength(kStrWatch), gScratchStr)) {  /* "watch" */
            InstallCursor(gCursor);
            return 0;
        }
        hCursor = (HCURSOR)LoadResourceByTag(0x43555253u, gScratchStr);
        if (hCursor == (HCURSOR)0)
            return 0x2b;
    } else {
        /* the whole int id is pushed */
        hCursor = (HCURSOR)GetResourceHandle(0x43555253u, *(int *)(argResult + 1));
        if (hCursor == (HCURSOR)0)
            return 0x2b;
    }

    InstallCursor(hCursor);
    ReleaseResource(hCursor, 0x43555253u);
    return 0;
}


/* showcursor (12003): shows the cursor (QdShowCursor). Takes no arguments. */
short ActionCmd_ShowCursor(int pCtx, short *pArgBlock, int *pTable,  /* 0x0043e090 */
                              int *pSubSkip)
{
    *pSubSkip = 0;
    QdShowCursor();
    return 0;
}


/* debugger (12040): does nothing in this build. */
short __cdecl ActionCmd_Debugger(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043e0b0 */
{
    (void)pCtx; (void)pArgBlock; (void)pTable;
    *pSubSkip = 0;
    return 0;
}


/* error (12053): returns status 0x2f, the script error. Takes no arguments. */
short __cdecl ActionCmd_Error(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043e0c0 */
{
    (void)pCtx; (void)pArgBlock; (void)pTable;
    *pSubSkip = 0;
    return 0x2f;
}


/* screentoblack (12050): screentoblack(clut, ticks): ActionCmd_BlackToScreen's twin, fading the
 * palette to black (FadeCLUTToBlack). */
short __cdecl ActionCmd_ScreenToBlack(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043e0d0 */
{
    short  evalStatus;
    short   status;
    int   *pHandle;
    short  result2[4];
    short  result1[4];

    evalStatus = EvalTwoScriptArgs(pCtx, pArgBlock, pTable,
                        (int *)result1, (int *)result2, pSubSkip);
    status = (unsigned short)evalStatus;
    if (evalStatus != 0)
        return status;

    status = ResolveClutFromScriptValue(result1, &pHandle);
    if (status != 0)
        return status;

    if (result2[0] != 4)
        return 0xe;

    /* the full dword value, as in BlackToScreen */
    FadeCLUTToBlack(pHandle, *(int *)&result2[1]);
    FreeResObject(pHandle);
    return 0;
}


/* blacktoscreen (12049): blacktoscreen(clut, ticks) fades the palette in from black
 * (FadeCLUTFromBlack) over ticks; the second argument must be an int (0x0e). */
short __cdecl ActionCmd_BlackToScreen(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043e150 */
{
    short  evalStatus;
    short   status;
    int   *pHandle;
    short  result2[4];   /* second typed result from EvalTwoScriptArgs (type at [0], value at [1..2]) */
    short  result1[4];   /* first typed result from EvalTwoScriptArgs */

    evalStatus = EvalTwoScriptArgs(pCtx, pArgBlock, pTable,
                        (int *)result1, (int *)result2, pSubSkip);
    status = (unsigned short)evalStatus;
    if (evalStatus != 0)
        return status;

    status = ResolveClutFromScriptValue(result1, &pHandle);
    if (status != 0)
        return status;

    if (result2[0] != 4)   /* second arg must be type: int */
        return 0xe;

    /* the full dword value is passed, not its low short */
    FadeCLUTFromBlack(pHandle, *(int *)&result2[1]);
    FreeResObject(pHandle);
    return 0;
}


/* blackscreen (12051): paints the scene node's rect on channel B black. Takes no arguments. */
short __cdecl ActionCmd_BlackScreen(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043e1d0 */
{
    short local_rect[4];

    *pSubSkip = 0;                      /* the exe reads its fourth argument first (in_stack_00000010) */
    QdSetPort((int *)&gChannels[1].hResource);
    GetSceneRect((int *)&gChannels[1].hResource, local_rect);
    QdPenNormal();
    SetSceneNodeColorFromPalette(0);
    QdPaintRect(local_rect);
    return 0;
}


/* delay (12004): sleeps for the int argument's number of ticks (SleepTicks). */
short __cdecl ActionCmd_Delay(int pCtx, short *pArgBlock,  /* 0x0043e220 */
                                  int *pTable, int *pSubSkip)
{
    short   status;
    short  argResult[4];   /* 8-byte typed result: [0]=type, [1..2]=value */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    if (argResult[0] != 4)   /* require type: int */
        return 0x0e;

    SleepTicks(*(int *)(argResult + 1));
    return 0;
}


/* keyaborts (16045): sets gKeyAborts from a bool argument (0x0e if it is not one). */
short __cdecl
ActionCmd_KeyAborts(int pCtx, short *pArgBlock, int *pTable,
                                                   int *pSubSkip)  /* 0x0043e270 */
{
    short  u;
    short out[4];

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)out, pSubSkip);
    if (u != 0)
        return u;
    if (out[0] != 2)
        return 0x0E;
    gKeyAborts = (short)*(int *)(out + 1);   /* the exe loads the int payload, stores a word */
    return 0;
}


/* quit (12071): re-applies gCursor to every window and sets gGameLoopState to 1, which ends the game
 * loop. Takes no arguments. */
short __cdecl ActionCmd_Quit(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043e2c0 */
{
    *pSubSkip = 0;                      /* the exe reads its fourth argument first */
    InstallCursor(gCursor);
    gGameLoopState = 1;
    return 0;   /* success */
}


/* flushevents (12073): pumps messages, discards the pending click and posts a type 0x2a event
 * (FlushEventQueue). Takes no arguments. */
short __cdecl ActionCmd_FlushEvents(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043e2f0 */
{
    (void)pCtx; (void)pArgBlock; (void)pTable;
    *pSubSkip = 0;
    FlushEventQueue();
    return 0;   /* success */
}


/* stopwalk (12010): resolves its argument to a name in gScratchStr and removes that walk
 * (RemoveWalkEntry). */
short __cdecl ActionCmd_StopWalk(int pCtx, short *pArgBlock,  /* 0x0043e310 */
                             int *pTable, int *pSubSkip)
{
    short  status;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    RemoveWalkEntry(gScratchStr);
    return 0;
}


/* stoploop (12011): stoploop(type, name) removes the loop binding of that type and name
 * (RemoveLoopEntry). */
short __cdecl ActionCmd_StopLoop(int *pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043e370 */
{
    short evalStatus;
    short  status;
    short argResult1[4];
    short argResult2[4];

    evalStatus = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable,
                       (int *)argResult1, (int *)argResult2, pSubSkip);
    status = (uint)(unsigned short)evalStatus;
    if (evalStatus != 0)
        return status;

    status = TakeScriptString(argResult1, &gScratchStr);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult2, &gScratchStr2);
    if (status != 0)
        return status;

    RemoveLoopEntry(gScratchStr, gScratchStr2);
    return 0;
}


/* stopcricket (12012): resolves its argument to a name in gScratchStr and removes that cricket
 * (RemoveCricketEntry). */
short __cdecl ActionCmd_StopCricket(int pCtx, short *pArgBlock,  /* 0x0043e3f0 */
                             int *pTable, int *pSubSkip)
{
    short  status;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    RemoveCricketEntry(gScratchStr);
    return 0;
}


/* pauseloop (16046): pauseloop(type, name, flag): sets the pause count of that loop
 * (AdjustLoopPauseCount); the flag must be a bool. PauseWalk (16048) and PauseCricket (16047) are
 * its one-name twins.
 *
 * Port note: the first two result buffers were once swapped; the push order at 0x0043e450 puts the
 * first argument in gScratchStr. The symptom was a binding named "all" instead of "prop" and script
 * error 10, "No such object", on the menu and radio OK buttons. */
short __cdecl
ActionCmd_PauseLoop(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043e450 */
{
    short s1;
    short  u;
    short a[4];                         /* three result blocks: separate locals, not one array */
    short b[4];
    short c[4];

    s1 = EvalThreeScriptArgs(pCtx, pArgBlock, pTable,
                             (int *)a, (int *)b, (int *)c, pSubSkip);
    if (s1 != 0)
        return s1;
    u = TakeScriptString(a, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = TakeScriptString(b, (byte *)gScratchStr2);
    if (u != 0)
        return u;
    if (c[0] != 2)
        return 0x0E;
    AdjustLoopPauseCount(&gScratchStr, &gScratchStr2, (short)*(int *)(c + 1));
    return 0;
}


/* pausewalk (16048): pausewalk(name, flag) adjusts the walk's pause count (AdjustWalkPauseCount). */
short __cdecl
ActionCmd_PauseWalk(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043e4e0 */
{
    short s1;
    short  u;
    short a1[4], a2[4];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    if (a2[0] != 2)
        return 0x0E;
    AdjustWalkPauseCount(&gScratchStr, (short)*(int *)(a2 + 1));
    return 0;
}


/* pausecricket (16047): pausecricket(name, flag) adjusts the cricket's pause count
 * (AdjustCricketPauseCount). */
short __cdecl
ActionCmd_PauseCricket(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043e550 */
{
    short s1;
    short  u;
    short a1[4], a2[4];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    if (a2[0] != 2)
        return 0x0E;
    AdjustCricketPauseCount(&gScratchStr, (short)*(int *)(a2 + 1));
    return 0;
}


/* playmovie (12017): evaluates one argument to a name, clears gActionFrameFlags and plays that movie
 * on channel B (PlayMovie). Returns PlayMovie's status, or the evaluator's on the early exits.
 *
 * Port note: declared void in the exe, but the dispatcher reads eax as the status, so the status is
 * whatever the last call left. Returning it explicitly matters: left void, the dispatcher's status
 * was garbage once a movie ran to completion (0x8262378 aborted boot). */
short __cdecl ActionCmd_PlayMovie(int pCtx, short *pArgBlock,  /* 0x0043e5c0 */
                                    int *pTable, int *pSubSkip)
{
    short  status;
    short argResult[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                               (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    gActionFrameFlags = 0;
    return (uint)(unsigned short)
        PlayMovie(gScratchStr, &gChannelB);
}


/* variable (16030): writes a value to a variable, in the local scope first and then the global
 * table; each write's own status is the result. */
short __cdecl
ActionCmd_Variable(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0043e620 */
{
    short  u;
    short  a1[4], a2[4];
    short  tag;

    u = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    /* the port once returned 0 after the write instead of its status */
    if (LookupPropertyByName(pTable, (byte *)gScratchStr, &tag, pArgBlock) != 0) {
        u = LookupPropertyByName((int *)gGlobalsNode, (byte *)gScratchStr, &tag, pArgBlock);
        if (u != 0)
            return u;
        u = WritePropertyValue((int *)gGlobalsNode, tag, a2);
        if (u != 0)
            return u;
        return 0;
    }
    return WritePropertyValue(pTable, tag, a2);
}


/* Resolves a script value to a palette handle in *ppHandle. An int is a 'clut' resource id; a string
 * is one of six names (the active, primary, secondary, script, stage or scene palette, each wrapped
 * by BuildCLUTFromPaletteData) or else a 'clut' resource by that name. Status 0x2b if no palette is
 * found. */
short __cdecl ResolveClutFromScriptValue(short *pScriptValue, int **ppHandle)  /* 0x0043e6f0 */
{
    /* Returns short (every caller tests ax). The string case comes first and each name test is one
     * condition with the lookup inline (byte match). */
    short  s;
    int   *piHandle;

    if (*pScriptValue != 4) {
        s = TakeScriptString(pScriptValue, &gScratchStr);
        if (s != 0)
            return s;

        if (PascalStringsEqualNoCase(&gScratchStr, (byte *)UpdatePascalLength(&kStrCurrent)) != 0) {
            *ppHandle = BuildCLUTFromPaletteData(GetResDataPtr(gActiveClut) + 8);
            UnlockResObject(gActiveClut);
            return 0;
        }
        if (PascalStringsEqualNoCase(&gScratchStr, (byte *)UpdatePascalLength(&kStrSystemLower)) != 0) {
            *ppHandle = BuildCLUTFromPaletteData(GetResDataPtr(gPrimaryClut) + 8);
            UnlockResObject(gPrimaryClut);
            return 0;
        }
        if (PascalStringsEqualNoCase(&gScratchStr, (byte *)UpdatePascalLength(&kStrBlackLower)) != 0) {
            *ppHandle = BuildCLUTFromPaletteData(GetResDataPtr(gSecondaryClut) + 8);
            UnlockResObject(gSecondaryClut);
            return 0;
        }
        if (gSet.active != 0 &&
            PascalStringsEqualNoCase(&gScratchStr, (byte *)UpdatePascalLength(&kStrSet)) != 0) {
            *ppHandle = BuildCLUTFromPaletteData(GetResDataPtr(DAT_0048a784) + 0xf2);
            UnlockResObject(DAT_0048a784);
            return 0;
        }
        if (gStage.active != 0 &&
            PascalStringsEqualNoCase(&gScratchStr, (byte *)UpdatePascalLength(&kStrStage)) != 0) {
            *ppHandle = BuildCLUTFromPaletteData(GetResDataPtr(DAT_0048a760) + 0x38);
            UnlockResObject(DAT_0048a760);
            return 0;
        }
        if (gPuppet.active != 0 &&
            PascalStringsEqualNoCase(&gScratchStr, (byte *)UpdatePascalLength(&kStrPuppet)) != 0) {
            *ppHandle = BuildCLUTFromPaletteData(GetResDataPtr(DAT_0048a810) + 0x3a);
            UnlockResObject(DAT_0048a810);
            return 0;
        }
        piHandle = (int *)LoadResourceByTag(0x636c7574, &gScratchStr);
        if (piHandle == (int *)0)
            return 0x2b;
    } else {
        /* the whole value dword is pushed (the callee takes a short) */
        piHandle = (int *)GetResourceHandle(0x636c7574, *(int *)((byte *)pScriptValue + 2));
        if (piHandle == (int *)0)
            return 0x2b;
    }
    *ppHandle = piHandle;
    CloneResource((void **)ppHandle);
    ReleaseResource(piHandle, 0x636c7574);
    return 0;
}


/* Allocates a palette resource, copies 256 eight-byte entries from pData into it after the 8-byte
 * header, and returns the finished handle. */
int * __cdecl BuildCLUTFromPaletteData(int pData)  /* 0x0043e990 */
{
    typedef struct { uint32_t w[2]; } ClutEntry8;
    int       iDst;
    short     i;
    int      *hNew;

    hNew = AllocResObject(0);
    InitCLUT(&hNew);
    iDst = GetResDataPtr(hNew);
    i    = 0;
    do {
        ((ClutEntry8 *)iDst)[i + 1] = *(ClutEntry8 *)(i * 8 + pData);   /* entry 0 is the 8-byte header */
        i++;
    } while (i < 0x100);
    UnlockResObject(hNew);
    return hNew;
}
