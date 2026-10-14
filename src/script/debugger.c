/* debugger.c -- TI.EXE 0x0040bb60..0x0040d570: the script debugger window: channel
 * handler, buttons, listing, breakpoints and watches. */

#include <windows.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "df_protos.h"
#include "topics/multimedia.h"
#include "topics/dialog.h"
#include "topics/scriptcmd.h"
#include "topics/scriptquery.h"
#include "topics/scene.h"




#include "topics/scenerect.h"
/* Message handler of the type-3 channel: the script debugger's "Debug" window
 * (OpenMultimediaWindow creates it). DispatchChannelMessage calls it for channel type 3. The
 * debugger is inert in this build: none of its state is written outside this unit, so the
 * interpreter never feeds it, but the window code is linked in.
 * Messages: 0 click, 1 draw the buttons and listing, 2 key, 3 refresh (save the scene state,
 * RefreshChannelState), 4 end scene (fatal 0x1129 if not initialised), 5 / 6 init and teardown
 * reference (the first creates the 200-pixel viewport, y + 0x3d; fatal 0x112a if zero on
 * teardown), 7 set the viewport from an 8-byte rect, 9 initialise (ResetDebuggerState,
 * HideChannelWindow), 0xe get the scene info (scroll parameters and rect, y + 0x3d), 0x10 centre
 * the viewport (GetSystemMetrics(SM_CXVSCROLL) for the width). */
void __cdecl DebuggerChannelHandler(int pChannel, short msg, int *pParam)  /* 0x0040bb60 */
{
    short halfW, centerX;
    int   scrollbarW;
    short dimV, dimH;
    short rect[4];          /* one 8-byte slot, reused by cases 5, 7, 0xe and 0x10 */
    /* 8-byte rect and 20-byte scroll-info block: the exe copies both whole
     * (two dword moves; rep movsd with ecx = 5). */
    typedef struct { int a, b; } Rect8;
    typedef union { short s[10]; int i[5]; } ScrollInfo;
    ScrollInfo resp;

    switch (msg)
    {
    case 0:
        /* a click */
        HandleDebuggerClick(pChannel, *(ScenePointYX *)&pParam);
        return;

    case 1:
        /* draw the button strip and the listing */
        DrawDebuggerButtons(pChannel);
        return;

    case 2:
        /* a key */
        DispatchDebuggerKey(pChannel, (short)(int)pParam);
        return;

    case 3:
        /* Refresh -- save scene state and update */
        QdSetPort((int *)(pChannel + 0x12));
        RefreshChannelState(pChannel, pParam);
        return;

    case 4:
        /* End scene -- fatal if not initialised */
        if (gChannels[2].hResource == 0)
            FatalError(0, 0x1129);
        DestroyChannelViewport((int)&gChannelC);
        gChannels[2].hResource = 0;
        return;

    case 5:
        /* Init ref: first call sets up 200px display viewport */
        if (gDebuggerWindowRefCount == 0)
        {
            QdSetPort((int *)(pChannel + 0x12));
            GetChannelScrollPos(pChannel, &dimH, &dimV);
            QdSetOrigin(pChannel, dimH << 4, dimV << 4);
            GetSceneRect((int *)(pChannel + 0x12), rect);
            /* the top moves down 61 pixels (`add word ptr [rect],0x3d`, 0x0040bc71); the port had
             * copied rect[1] (the left edge) into rect[0] first */
            rect[0] += 0x3d;
            QdClipRect(rect);
        }
        gDebuggerWindowRefCount++;
        return;

    case 6:
        /* Teardown ref: fatal if zero (0x112a); at zero resets display */
        if (gDebuggerWindowRefCount == 0)
            FatalError(0, 0x112a);
        if (--gDebuggerWindowRefCount == 0)
        {
            QdSetPort((int *)(pChannel + 0x12));
            QdSetOrigin(pChannel, 0, 0);
            QdClipRect((short *)&gScrollRect);
        }
        return;

    case 7:
        /* Set viewport from 8-byte rect + post-set action */
        *(Rect8 *)rect = *(Rect8 *)pParam;
        ResizeChannelWindow(rect, pChannel);
        PostViewportSetAction(pChannel);
        return;

    case 9:
        /* initialise: reset the debugger state and hide the window */
        ResetDebuggerState();
        HideChannelWindow(pChannel);
        return;

    case 0xe:
    {
        /* Get scene info: computed scroll params + scene rect (Y+0x3d) */
        GetChannelWindowRect(pChannel, rect);
        resp.s[0] = 1;                                         /* scroll type */
        resp.s[1] = (short)((rect[3] - rect[1]) / 16);         /* scroll range W */
        if (resp.s[1] > 4) resp.s[1]--; else resp.s[1] = 4;    /* clamped in place */
        resp.s[2] = 0x10;                                      /* scroll step W */
        resp.s[3] = 1;                                         /* scroll type H */
        resp.s[4] = (short)((rect[2] - rect[0] - 0x3d) / 16);  /* scroll range H */
        if (resp.s[4] > 4) resp.s[4]--; else resp.s[4] = 4;
        resp.s[5] = 0x10;                                      /* scroll step H */

        GetSceneRect((int *)(pChannel + 0x12), rect);
        rect[0] += 0x3d;                                       /* Y offset +61 */
        *(Rect8 *)&resp.i[3] = *(Rect8 *)rect;
        *(ScrollInfo *)pParam = resp;
        return;
    }

    case 0x10:
    {
        /* Centre the viewport. The centre and half-width land in the rect's x words (1 and 3) and
         * the offsets from the bottom in its y words (0 and 2); the port had the two axes swapped
         * (0x0040bdf3). */
        *(int *)&rect[0] = *(int *)&gScrollSceneRect.y1;
        *(int *)&rect[2] = (*(int *)&gScrollSceneRect.y2);
        scrollbarW = GetSystemMetrics(2);    /* SM_CXVSCROLL */
        centerX = (short)((rect[1] + rect[3]) >> 1) + 100;
        halfW = (short)((short)scrollbarW + 0xcc) >> 1;
        rect[1] = centerX - halfW;        /* centreX - halfW */
        rect[3] = centerX + halfW;        /* centreX + halfW */
        rect[2] -= 100;                 /* bottom - 100 */
        rect[0] = rect[2] - 200;        /* bottom - 300 */
        DispatchChannelMessage((short *)&gChannelC, 7, (byte *)rect);
        return;
    }

    default:
        return;
    }
}


/* Runs a debugger command, called from DebuggerChannelHandler on message 2 (the key equivalents
 * of the button strip). Saves the scene state, then dispatches on cmd: 3 jumps to the script pane;
 * 'A' clears all breakpoints and watches; 'E' runs until an error (run mode 0xffff) and 'G' just
 * goes (run mode 0), both hiding the window; 'I' steps in (target depth = depth + 1, step mode 2);
 * 'O' steps out (depth - 1, mode 3); 'S' steps (target depth = depth, mode 1). Upper and lower
 * case are the same key. */
void __cdecl DispatchDebuggerKey(int pChannel, short cmd)  /* 0x0040beb0 */
{
    short btnRect[4];  /* local_8 */

    QdSetPort((int *)(pChannel + 0x12));

    switch (cmd)
    {
    case 3:
        if (gDebuggerPane != 2) {    /* the exe lays the mode-change arm out first */
            gDebuggerPane = 2;
            RebuildDebuggerListing();
            ScrollDebuggerListingToCaret();
            gDebuggerPane = 1;
        } else {
            ScrollDebuggerListingToCaret();
        }
        InvalidateDebuggerListing(1);
        return;

    case 'A':
    case 'a':
        InvalidateDebuggerListing(0);
        ComputeButtonRect(2, 2, btnRect);
        InsetSceneRect(btnRect, 1, 1);
        QdInvertRect(btnRect);
        InvalidateSceneRect(btnRect);
        ResizeResObject(gDebuggerBreakpoints, 0);
        ResizeResObject(gDebuggerWatches, 0);
        gDebuggerWatchCount = 0;
        gDebuggerBreakpointCount = 0;
        return;

    case 'E':
    case 'e':
        ComputeButtonRect(1, 2, btnRect);
        InsetSceneRect(btnRect, 1, 1);
        QdInvertRect(btnRect);
        SleepTicks(2);
        HideChannelWindow((int)&gChannelC);
        gDebuggerStepMode = 0;
        gDebuggerRunMode = 0xffff;    /* run until error */
        return;

    case 'G':
    case 'g':
        ComputeButtonRect(0, 2, btnRect);
        InsetSceneRect(btnRect, 1, 1);
        QdInvertRect(btnRect);
        SleepTicks(2);
        HideChannelWindow((int)&gChannelC);
        gDebuggerStepMode = 0;
        gDebuggerRunMode = 0;
        return;

    case 'I':
    case 'i':
        InvalidateDebuggerListing(1);
        ComputeButtonRect(1, 1, btnRect);
        InsetSceneRect(btnRect, 1, 1);
        QdInvertRect(btnRect);
        InvalidateSceneRect(btnRect);
        gDebuggerTargetDepth = gDebuggerDepth + 1;   /* step in */
        gDebuggerStepMode = 2;
        gDebuggerRunMode = 0;
        return;

    case 'O':
    case 'o':
        InvalidateDebuggerListing(1);
        ComputeButtonRect(2, 1, btnRect);
        InsetSceneRect(btnRect, 1, 1);
        QdInvertRect(btnRect);
        InvalidateSceneRect(btnRect);
        gDebuggerTargetDepth = gDebuggerDepth - 1;   /* step out */
        gDebuggerStepMode = 3;
        gDebuggerRunMode = 0;
        return;

    case 'S':
    case 's':
        InvalidateDebuggerListing(1);
        ComputeButtonRect(0, 1, btnRect);
        InsetSceneRect(btnRect, 1, 1);
        QdInvertRect(btnRect);
        InvalidateSceneRect(btnRect);
        gDebuggerTargetDepth = gDebuggerDepth;        /* step */
        gDebuggerStepMode = 1;
        gDebuggerRunMode = 0;
    }
}


/* Handles a click in the debugger window (DebuggerChannelHandler message 0, the point packed in
 * param). Above y = 0x3d it is the 3x3 button strip, each button hit-tested with PointInSceneRect
 * and tracked with TrackButtonPress: pane select (GLOBALS / LOCALS / SCRIPTS), Step / Step In /
 * Step Out, and Go / Error / Clear All. Below it the click is on a listing line: in the GLOBALS
 * and LOCALS panes it edits the variable on that line (EditDebuggerVariable), in the SCRIPTS pane
 * it toggles a breakpoint (ClearBreakpointAtLine, else it adds a row). Adding a row hides the
 * window. */
void __cdecl HandleDebuggerClick(int pChannel, ScenePointYX param)  /* 0x0040c240: the point by value, as PointInSceneRect takes it */
{
    int   *pEntry;
    int    lineIdx;
    short  i, row;   /* i: loop counter, and the "entry added" flag */
    short  scrollH, scrollV;
    short  btnRect[4];  /* local_8 */

    QdSetPort((int *)(pChannel + 0x12));

    /* The exe lays the button hit-test out first (cmp 0x3d / jge). */
    if (param.y < 0x3d) {
    /* the top row: pane select (GLOBALS / LOCALS / SCRIPTS) */
    i = 0;
    do {
        ComputeButtonRect(i, 0, btnRect);
        InsetSceneRect(btnRect, 1, 1);
        if (PointInSceneRect(param, btnRect) != 0 &&
            (short)TrackButtonPress(btnRect) != 0) {  /* 0x0040d4e0 */
            gDebuggerPane = i;
            RebuildDebuggerListing();
            ScrollDebuggerListingToCaret();
            GetSceneRect(&gChannels[2].hResource, btnRect);  /* scene rect for mode select scroll update */
            InvalidateSceneRect(btnRect);
            return;
        }
        i++;
    } while (i < 3);

    /* the middle row: Step / Step In / Step Out */
    i = 0;
    do {
        ComputeButtonRect(i, 1, btnRect);
        InsetSceneRect(btnRect, 1, 1);
        if (PointInSceneRect(param, btnRect) != 0 &&
            (short)TrackButtonPress(btnRect) != 0) {
            i++;
            InvalidateDebuggerListing(1);
            gDebuggerStepMode = i;
            /* The exe tests the counter once, then re-reads the global for the
             * other two (cmp word ptr [0x00460dc0]) -- a volatile read. */
            if (i == 1) gDebuggerTargetDepth = gDebuggerDepth;
            if (*(volatile short *)&gDebuggerStepMode == 2) gDebuggerTargetDepth = gDebuggerDepth + 1;
            if (*(volatile short *)&gDebuggerStepMode == 3) gDebuggerTargetDepth = gDebuggerDepth - 1;
            gDebuggerRunMode = 0;
            return;
        }
        i++;
    } while (i < 3);

    /* the bottom row: Go / Error / Clear All */
    i = 0;
    do {
        ComputeButtonRect(i, 2, btnRect);
        InsetSceneRect(btnRect, 1, 1);
        if (PointInSceneRect(param, btnRect) != 0 &&
            (short)TrackButtonPress(btnRect) != 0) {
            if (i == 0) {
                HideChannelWindow(pChannel);
                gDebuggerStepMode = 0;
                gDebuggerRunMode = 0;
                return;
            }
            if (i == 1) {
                HideChannelWindow(pChannel);
                gDebuggerStepMode = 0;
                gDebuggerRunMode = 0xffff;
                return;
            }
            if (i == 2) {
                ResizeResObject(gDebuggerBreakpoints, 0);
                ResizeResObject(gDebuggerWatches, 0);
                gDebuggerBreakpointCount = 0;
                gDebuggerWatchCount = 0;
                InvalidateDebuggerListing(0);
                return;
            }
        }
        i++;
    } while (i < 3);
    } else {
        /* a click on a listing line: the line index from the scroll position and the click offset */
        i = 0;
        GetChannelScrollPos(pChannel, &scrollH, &scrollV);
        row = scrollV + (short)((param.y - 0x41) / 16);
        lineIdx = (int)row;
        if (CountDebuggerListingLines() > lineIdx) {
            /* the row's Y is row * 16 + 0x4d, the render lock is taken here (the port called only
             * the Leave below), and the rect runs from Y - 0xc to Y + 4 (the port had dropped the
             * +0x4d from the top) */
            scrollV = row * 0x10 + 0x4d;
            EnterSceneRenderLock(pChannel);
            btnRect[0] = scrollV - 0xc;
            btnRect[3] = 20000;
            btnRect[1] = 0;
            btnRect[2] = btnRect[0] + 0x10;
            if ((short)TrackButtonPress(btnRect) != 0) {
                switch (gDebuggerPane) {
                case 0:
                    EditDebuggerVariable(gGlobalsNode, lineIdx);
                    break;
                case 1:
                    EditDebuggerVariable(gDebuggerLocalsNode, lineIdx);
                    break;
                case 2:
                    if ((short)ClearBreakpointAtLine(lineIdx) == 0) {
                        /* add a breakpoint: an 18-byte row */
                        ResizeResObject(gDebuggerBreakpoints, (gDebuggerBreakpointCount * 2 + 2) * 9);
                        /* 18 bytes per entry, a byte offset: the port stepped `0x12 / 4` = 4
                         * ints (16 bytes), overlapping every entry after the first */
                        pEntry = (int *)((char *)GetResDataPtr(gDebuggerBreakpoints) + gDebuggerBreakpointCount * 0x12);
                        *pEntry       = *(int *)(gDebuggerFrame + 2);   /* frame title ref   */
                        pEntry[1]     = *(int *)(gDebuggerFrame + 8);   /* frame path ref    */
                        pEntry[2]     = GetDebuggerListingLineOffset(lineIdx);    /* listing byte offset   */
                        pEntry[3]     = GetScriptTokenIndexForLine(lineIdx);       /* token index     */
                        i         = (short)IsMultimediaSequentialModifierKeyDown();  /* modifier key, the row's flag word */
                        *(short *)(pEntry + 4) = i;
                        UnlockResObject(gDebuggerBreakpoints);
                        gDebuggerBreakpointCount++;
                    }
                }
            }
            LeaveSceneRenderLock(pChannel);
            if (gDebuggerPane != 2) {
                RebuildDebuggerListing();   /* the new pane's listing */
            }
            if (i != 0) {
                HideChannelWindow(pChannel);
                gDebuggerStepMode = 0;
                gDebuggerRunMode = 0;
            }
        }
    }
}


/* Draws the nine buttons (GLOBALS / LOCALS / SCRIPTS with the current pane's label highlighted,
 * Step / Step In / Step Out, Go / Error / Clear All), each rect from ComputeButtonRect and drawn
 * by DrawButton, and then the listing (DrawDebuggerListing). Called from DebuggerChannelHandler on
 * message 1: it reloads the CLUT, saves the scene state, clips to the 61-pixel (0x3d) button strip
 * and draws the listing under the render lock. */
void __cdecl DrawDebuggerButtons(int pChannel)  /* 0x0040c660 */
{
    int   *pPort;
    short  rect[4];     /* rect buffer {top, left, bottom, right} */  /* local_8 */

    InitCLUT(&gActiveClut);
    ReloadCLUT(gActiveClut);
    pPort = (int *)(pChannel + 0x12);
    QdSetPort(pPort);
    QdTextFont(0x58);
    QdTextSize(9);
    FlushChannelDisplay(pChannel);
    GetSceneRect(pPort, rect);
    /* `mov word ptr [esp+0xc],0x3d` (0x0040c6bf) is rect[2], the bottom: the viewport is the
     * 61-pixel button strip across the top. The port overrode rect[1] (the left edge). */
    rect[2] = 0x3d;
    QdClipRect(rect);
    QdEraseRect(rect);
    QdMoveTo(rect[1], 0x3c);
    QdLineTo(rect[3], 0x3c);

    /* top row: GLOBALS / LOCALS / SCRIPTS, the current pane's label highlighted */
    ComputeButtonRect(0, 0, rect);
    if (gDebuggerPane == 0)      /* the whole call twice, tail-merged in the exe */
        DrawButton(UpdatePascalLength(&kStrGLOBALS), rect);
    else
        DrawButton(UpdatePascalLength(&kStrGlobals), rect);

    ComputeButtonRect(1, 0, rect);
    if (gDebuggerPane == 1)
        DrawButton(UpdatePascalLength(&kStrLOCALS), rect);
    else
        DrawButton(UpdatePascalLength(&kStrLocals), rect);

    ComputeButtonRect(2, 0, rect);
    if (gDebuggerPane == 2)
        DrawButton(UpdatePascalLength(&kStrSCRIPTS), rect);
    else
        DrawButton(UpdatePascalLength(&kStrScripts), rect);

    /* middle row: Step / Step In / Step Out */
    ComputeButtonRect(0, 1, rect);
    DrawButton(UpdatePascalLength(&kStrStep), rect);

    ComputeButtonRect(1, 1, rect);
    DrawButton(UpdatePascalLength(&kStrStepIn), rect);

    ComputeButtonRect(2, 1, rect);
    DrawButton(UpdatePascalLength(&kStrStepOut), rect);

    /* bottom row: Go / Error / Clear All */
    ComputeButtonRect(0, 2, rect);
    DrawButton(UpdatePascalLength(&kStrGo), rect);

    ComputeButtonRect(1, 2, rect);
    DrawButton(UpdatePascalLength(&kStrError), rect);

    ComputeButtonRect(2, 2, rect);
    DrawButton(UpdatePascalLength(&kStrClearAll), rect);

    /* the listing is drawn under the render lock */
    EnterSceneRenderLock(pChannel);
    GetSceneRect(pPort, rect);
    QdEraseRect(rect);
    QdTextFont(0);
    QdTextSize(0xc);
    DrawDebuggerListing();
    LeaveSceneRenderLock(pChannel);
}


/* Invalidates the listing area of channel C's window, below the 61-pixel (0x3d) button strip.
 * With param != 0 and the pane not SCRIPTS it instead switches to the SCRIPTS pane and
 * invalidates the whole window. */
void __cdecl InvalidateDebuggerListing(short param)  /* 0x0040c910 */
{
    short rect[4];  /* local_8 */

    GetSceneRect((int *)&gChannels[2].hResource, rect);
    if ((gDebuggerPane == 2) || (param == 0)) {
        rect[0] = rect[0] + 0x3d;
    } else {
        gDebuggerPane = 2;
    }
    InvalidateSceneRect(rect);
}


/* Draws the listing text (gDebuggerListing): CR starts the next line (Y += 0x10, X back to 0xf),
 * TAB indents by 0x20, other characters are drawn as a run with QdMoveTo + QdDrawString. A line
 * that IsDebuggerLineMarked (a watch or breakpoint) is highlighted with a pattern-brush rect
 * (InsetSceneRect, QdPaintRect), and DrawPlaybackPositionMarker draws the chevron at the caret
 * offset (gDebuggerCaretOffset). */
void __stdcall DrawDebuggerListing(void)  /* 0x0040c960 */
{
    char  *_Src, *pRunEnd, *pEnd;
    char  *pStart;         /* pointer to res_obj_1 data */  /* local_104 */
    short  y;         /* current Y position (starts at 0x4d) */  /* local_112 */
    /* Two Ghidra-split stack objects (class 3): local_10c.. is ONE 8-byte rect, whose address
     * goes to InsetSceneRect and InvalidateSceneRect, and local_100 + local_ff[255] ONE 256-byte
     * Pascal string (QdDrawString takes the length from [0] and the body from +1). */
    short  hiRect[4];         /* local_10c: {y1, x1, y2, x2} */
    short  x;             /* current X position (starts at 0xf) */
    short  isMarked;
    int    dataSize;
    int    _Size;             /* signed: the exe clamps with cmp / jle */
    byte   pascalStr[256];    /* local_100 (length) + local_ff[255] (body) */

    y = 0x4d;   /* initial Y = 77 */
    x     = 0xf;    /* initial X = 15 */
    dataSize     = (int)GetResObjectSize(gDebuggerListing);
    /* the pointer the call returns is the walker; the start is its copy */
    _Src      = (char *)GetResDataPtr(gDebuggerListing);
    pStart = _Src;
    pEnd    = _Src + dataSize;

    while (_Src < pEnd) {
        if (x == 0xf) {
            isMarked = IsDebuggerLineMarked((int)_Src - (int)pStart);
            if (isMarked != 0) {
                hiRect[0] = y - 4;
                hiRect[1] = 7;
                hiRect[2] = hiRect[0];
                hiRect[3] = 7;
                InsetSceneRect(hiRect, -3, -3);   /* expand rect by 3 */
                QdPaintRect(hiRect);   /* draw the highlight: 0x004202c0 in the exe (the port only invalidated) */
            }
        }
        if (_Src - gDebuggerCaretOffset == pStart) {
            DrawPlaybackPositionMarker(y - 4);
        }
        if (*_Src == '\r') {
            x     = 0xf;
            y = y + 0x10;
            _Src++;
        } else {
            if (*_Src == '\t') {
                x = x + 0x20;   /* tab: advance X indent */
                _Src++;
            } else {
                for (pRunEnd = _Src; pRunEnd < pEnd && *pRunEnd != '\r'; pRunEnd++);
                _Size = (int)pRunEnd - (int)_Src;
                if (_Size > 0xff) _Size = 0xff;
                memmove(pascalStr + 1, _Src, _Size);   /* a call to 0x0044e440 in the exe */
                pascalStr[0] = (byte)_Size;
                QdMoveTo(x, y);
                QdDrawString(pascalStr);
                _Src = pRunEnd;
            }
        }
    }
    UnlockResObject(gDebuggerListing);
}


/* Draws one button: frames pRect, centres the Pascal string pLabel in it and draws the right and
 * bottom border lines. pRect is {y1, x1, y2, x2} as shorts. */
void __cdecl DrawButton(byte *pLabel, short *pRect)  /* 0x0040cac0 */
{
    short x;

    QdFrameRect(pRect); /* hotspot frame / clip rect */
    /* Centre text: x = midpoint - half_width (the midpoint is formed before
     * the width call), y = y2 - 4 */
    x = ((pRect[1] + pRect[3]) >> 1) - (QdStringWidth(pLabel) >> 1);
    QdMoveTo(x, pRect[2] - 4);
    QdDrawString(pLabel);
    /* Draw bottom border line from (x1+1, y2) to (x2, y2) */
    QdMoveTo(pRect[1] + 1, pRect[2]);
    QdLineTo(pRect[3], pRect[2]);
    /* Draw right border line from (x2, y2) to (x2, y1+1) */
    QdLineTo(pRect[3], *pRect + 1);
}


/* Computes the rect {y1, x1, y2, x2} (shorts, into pRect) of a button in the 3x3 grid, origin at
 * the top left of the control panel: row (mode) pitch 19 px with a 3 px top margin, column
 * (phase) pitch 66 px with a 6 px left margin, each button 15 px high and 60 px wide. */
void __cdecl ComputeButtonRect(short mode, short phase, short *pRect)  /* 0x0040cb50 */
{
    /* built in a local and copied out whole (two dwords), each far edge
     * derived from its near edge */
    typedef struct { short s[4]; } ButtonRect;
    ButtonRect r;

    r.s[0] = mode * 0x13 + 3;        /* y1 = mode*19 + 3  */
    r.s[2] = r.s[0] + 0xf;           /* y2 = mode*19 + 18 */
    r.s[1] = phase * 0x42 + 6;       /* x1 = phase*66 + 6 */
    r.s[3] = r.s[1] + 0x3c;          /* x2 = phase*66 + 66 */
    *(ButtonRect *)pRect = r;
}


/* Frees the three debugger res-objects (listing, breakpoints, watches) that InitDebugger
 * allocated. Called by EndGameSession as the last subsystem shutdown before the cursor release. */
void __stdcall ShutdownDebugger(void)  /* 0x0040cba0 */
{
    FreeResObject(gDebuggerListing);
    FreeResObject(gDebuggerBreakpoints);
    FreeResObject(gDebuggerWatches);
}


/* Called by BeginGameSession: allocates the three debugger res-objects, opens the "Debug" window
 * (OpenMultimediaWindow) as channel C, type 3, and positions it. */
void __stdcall InitDebugger(void)  /* 0x0040cbd0 */
{
    /* The viewport rect {y1, x1, y2, x2} starts as a whole copy of the scene
     * rect at 0x0048a2c0 and is adjusted in place; the channel state is one
     * 146-byte ChannelState, copied whole into channel C (rep movsd + movsw). */
    typedef struct { short v[4]; } Rect8;
    void        *pResource;
    Rect8        rect;
    ChannelState state;
    short        centreX;
    short        halfW;

    gDebuggerBreakpointCount = 0;
    gDebuggerWatchCount = 0;
    gDebuggerListing = AllocResObject(0);
    gDebuggerBreakpoints = AllocResObject(0);
    gDebuggerWatches = AllocResObject(0);
    ResetDebuggerState();
    halfW = (short)(GetSystemMetrics(2) + 0xcc);   /* SM_CXVSCROLL + 204 */
    if (gChannels[2].hResource != 0)
        FatalError(0, 0x112b);
    gDebuggerWindowRefCount = 0;
    OpenMultimediaWindow((int *)&pResource);
    ((int *)&rect)[0] = ((int *)gScrollRect)[0];
    ((int *)&rect)[1] = ((int *)gScrollRect)[1];
    centreX = (short)(((int)gScrollSceneRect.x1 + (int)gScrollSceneRect.x2) >> 1) + 100;
    halfW >>= 1;
    rect.v[1] = centreX - halfW;            /* X min */
    rect.v[3] = centreX + halfW;            /* X max */
    rect.v[2] -= 100;                       /* Y max */
    rect.v[0] = rect.v[2] - 200;            /* Y min */
    state.type = 3;
    *(int *)((byte *)&state + 2)   = 0;
    *(int *)((byte *)&state + 6)   = 0;
    *(int *)((byte *)&state + 0xe) = 0;
    *(int *)((byte *)&state + 0xa) = -1;    /* shorts 5 and 6: sentinels */
    state.hResource = pResource;            /* offset 18: resource handle */
    SetChannelWindowState(pResource, (short *)&state);
    gChannelC = state;
    DispatchChannelMessage((short *)&state, 7, (byte *)&rect);
}


/* Resets the debugger: no caret, depth 0, run mode go, step mode none, the SCRIPTS pane, and an
 * empty listing. Called by InitDebugger and by DebuggerChannelHandler on message 9. */
void __stdcall ResetDebuggerState(void)  /* 0x0040cd10 */
{
    gDebuggerCaretOffset = -1;      /* no caret */
    gDebuggerDepth = 0;       /* call depth */
    gDebuggerRunMode = 0;       /* run mode: go */
    gDebuggerPane = 2;       /* the SCRIPTS pane */
    gDebuggerStepMode = 0;       /* step mode: none */
    ResizeResObject(gDebuggerListing, 0);
}


/* Rebuilds the listing for the current pane: GLOBALS lists gGlobalsNode's properties, LOCALS those
 * of gDebuggerLocalsNode, SCRIPTS the current script's source (ListScriptSourceToDebugger). */
void __stdcall RebuildDebuggerListing(void)  /* 0x0040cd50 */
{
    switch (gDebuggerPane) {             /* movsx + a compare chain: a switch on the mode word */
    case 0:
        ListNodePropertiesToDebugger(gGlobalsNode);
        break;
    case 1:
        ListNodePropertiesToDebugger(gDebuggerLocalsNode);
        break;
    case 2:
        ListScriptSourceToDebugger();
        break;
    }
}


/* Resets channel C's scroll, sets its vertical range to the listing's line count and scrolls to
 * the caret's line minus 2. */
void __stdcall ScrollDebuggerListingToCaret(void)  /* 0x0040cd90 */
{
    int lineNo;

    SetChannelScrollMin((int)&gChannelC, 0, 0);
    lineNo = CountDebuggerListingLines();
    SetChannelScrollMax((int)&gChannelC, 100, (short)lineNo);
    lineNo = GetDebuggerListingLineAtOffset(gDebuggerCaretOffset);
    SetChannelScrollPos((int)&gChannelC, 0, (short)lineNo - 2);
}


/* Counts the CR characters in the listing (gDebuggerListing): the number of lines. */
int __cdecl CountDebuggerListingLines(void)  /* 0x0040cde0 */
{
    int   cbData;
    char *pBase, *p, *pEnd;   /* declaration order fixes the compare's operand order */
    int   count = 0;

    cbData = (int)GetResObjectSize(gDebuggerListing);
    pBase  = (char *)GetResDataPtr(gDebuggerListing);
    pEnd   = pBase + cbData;
    for (p = pBase; p < pEnd; p++) {
        if (*p == '\r') count++;
    }
    UnlockResObject(gDebuggerListing);
    return count;
}


/* Converts a byte offset in the listing to a line index: the number of CRs before it. Returns 0
 * for a negative offset. */
int __cdecl GetDebuggerListingLineAtOffset(int byteOffset)  /* 0x0040ce30 */
{
    int   cbData;
    char *pBase;
    char *p;
    char *end;
    int   rowCount;

    rowCount = 0;
    if (byteOffset < 0) return 0;
    cbData = GetResObjectSize(gDebuggerListing);
    /* the pointer the call returns is the walker; the start is its copy */
    p = (char *)GetResDataPtr(gDebuggerListing);
    pBase = p;
    end = pBase + cbData;
    for (; p < end && (p - pBase) != byteOffset; p++) {
        if (*p == '\r') rowCount++;
    }
    UnlockResObject(gDebuggerListing);
    return rowCount;
}


/* Returns the byte offset at which listing line lineIdx (0-based) begins; the inverse of
 * GetDebuggerListingLineAtOffset. Returns 0 for a negative index. */
int __cdecl GetDebuggerListingLineOffset(int lineIdx)  /* 0x0040cea0 */
{
    int   linesSeen;
    int   cbData;
    char *p;
    char *pBase;

    /* the shape of GetDebuggerListingLineAtOffset: the pointer the call returns is the
     * walker and the start its copy (spilled to the frame in the exe) */
    linesSeen = 0;
    if (lineIdx < 0) return 0;
    cbData = (int)GetResObjectSize(gDebuggerListing);
    p      = (char *)GetResDataPtr(gDebuggerListing);
    pBase  = p;
    for (; p < pBase + cbData && linesSeen != lineIdx; p++) {
        if (*p == '\r') linesSeen++;
    }
    UnlockResObject(gDebuggerListing);
    return (int)p - (int)pBase;
}


/* Walks the current script's 8-byte token records (gDebuggerFrame + 0x14, type 0 ends the list) and
 * returns the index of the token that starts source line lineIdx, counting the newline tokens
 * (type 6). Returns 0 for a negative index. */
int __cdecl GetScriptTokenIndexForLine(int lineIdx)  /* 0x0040cf20 */
{
    int    linesSeen = 0;   /* declared first: it takes esi (cmp esi,edx) */
    short  tokenType;
    short *pTokens, *pCur;

    if (lineIdx < 0) return 0;
    pCur = *(short **)(gDebuggerFrame + 0x14);
    tokenType  = *pCur;
    pTokens = pCur;
    while (tokenType != 0 && lineIdx != linesSeen) {   /* cmp esi,edx: the parameter on the left */
        if (*pCur == 6) linesSeen++;
        pCur += 4;       /* stride 8 bytes = 2 shorts */
        tokenType   = *pCur;
    }
    return ((int)pCur - (int)pTokens) >> 3;
}


/* Emits one line per property of the node pHandle: the name, " = ", then the value as True /
 * False (type 2), a quoted string (type 3) or a decimal number (type 4); anything else is fatal.
 * Used for the GLOBALS and LOCALS panes. Resets gDebuggerCaretOffset to -1. */
void __cdecl ListNodePropertiesToDebugger(int *pHandle)  /* 0x0040cf60 */
{
    short propBuf[3];      /* [0]=type/key, [1..2]=uint value (6-byte struct) */
    byte  szDesc[256];
    int   i;         /* declared before count: sets the esi/ebp choice */
    int   count;
    short err;
    short *p;

    ResizeResObject(gDebuggerListing, 0);
    p = (short *)GetResDataPtr(pHandle);
    count = *p;
    UnlockResObject(pHandle);
    /* a for loop: the exe steps i after the closing separator's push */
    for (i = 0; i < count; i++) {
        /* the key read's status is tested (test ax,ax at 0x0040cfa8); the port dropped it and
         * tested an uninitialised local */
        err = ReadPropertyNameByTag(pHandle, (short)i, propBuf);
        if (err != 0)
            FatalError(err, 0x112f);
        err = TakeScriptString(propBuf, szDesc);
        if (err != 0)
            FatalError(err, 0x1130);
        AppendToDebuggerListing(szDesc);
        AppendToDebuggerListing(UpdatePascalLength(&kStrEquals));
        err = ReadPropertyValue(pHandle, (short)i, propBuf);
        if (err != 0)
            FatalError(err, 0x1131);
        switch (propBuf[0]) {
        case 2:
            if (*(unsigned int *)&propBuf[1] != 0)
                AppendToDebuggerListing(UpdatePascalLength(&kStrTrue));  /* "True" */
            else
                AppendToDebuggerListing(UpdatePascalLength(&kStrFalse));  /* "False" */
            break;
        case 3:
            AppendToDebuggerListing(UpdatePascalLength(&kStrQuote));
            err = TakeScriptString(propBuf, szDesc);
            if (err != 0)
                FatalError(err, 0x1132);
            AppendToDebuggerListing(szDesc);
            AppendToDebuggerListing(UpdatePascalLength(&kStrQuote));
            break;
        case 4:
            FormatUintAsPascalDecimalString(*(unsigned int *)&propBuf[1], (char *)szDesc);
            AppendToDebuggerListing(szDesc);
            break;
        default:
            FatalError(0, 0x1133);
        }
        AppendToDebuggerListing(UpdatePascalLength(&kStrCR));
    }
    gDebuggerCaretOffset = 0xffffffff;
}


/* Fills the listing with the current script's source: copies the token list of gDebuggerFrame to a
 * temporary resource and formats it as text (FormatScriptTokensAsText), setting the caret to the
 * current token (gDebuggerToken minus the token base, / 8). */
void __stdcall ListScriptSourceToDebugger(void)  /* 0x0040d130 */
{
    int  *piEntry;
    uint  cbEntry;
    int  *piTemp;
    void *pSrc;
    void *pDst;

    FreeResObject(gDebuggerListing);
    piEntry = *(int **)(gDebuggerFrame + 0x1a);
    cbEntry = GetResObjectSize(piEntry);
    piTemp  = (int *)AllocResObject(cbEntry);
    pSrc = (void *)GetLockedResDataPtr(piEntry);
    pDst = (void *)GetResDataPtr(piTemp);
    memmove(pDst, pSrc, cbEntry);
    UnlockResObject(piTemp);
    gDebuggerCaretOffset = FormatScriptTokensAsText(piTemp, (int **)&gDebuggerListing,
                                gDebuggerToken - *(int *)(gDebuggerFrame + 0x14) >> 3);
    FreeResObject(piTemp);
}


/* Appends the body of a Pascal string to the listing res-object (gDebuggerListing), growing it by
 * the string's length. */
void __cdecl AppendToDebuggerListing(byte *pStr)   /* 0x0040d1c0 */
{
    unsigned int  cbOld;
    int           pData;

    cbOld = GetResObjectSize(gDebuggerListing);
    ResizeResObject(gDebuggerListing, (unsigned int)*pStr + cbOld);
    pData = GetResDataPtr(gDebuggerListing);
    memmove((void *)(pData + (int)cbOld), pStr + 1, (unsigned int)*pStr);
    UnlockResObject(gDebuggerListing);
}


/* Returns 1 when the listing line at rowOffset carries a mark. SCRIPTS pane: a breakpoint exists
 * whose token offset equals the line's. GLOBALS / LOCALS: the variable named at the start of the
 * line is in the watch table (gDebuggerWatches) for this node. */
short __cdecl IsDebuggerLineMarked(int rowOffset)  /* 0x0040D220 */
{
  /* the mode-2 scan is the block at the end (the other modes are the fall-through), the result
   * lives in di, the name is one 256-byte Pascal buffer (frame 0x100), and the copy is a call
   * to memmove (0x0044e440), not an inlined memcpy */
  char *_Src;
  short marked;
  char *pNameEnd;
  int scopeNode;
  int pEntry;
  int _Size;
  int i;
  byte name[256];
  short nameEqual;

  marked = 0;
  if (gDebuggerPane != 2) {
    if (gDebuggerPane == 1)
      scopeNode = gDebuggerLocalsNode;
    else
      scopeNode = (int)gGlobalsNode;
    _Src = (char *)(rowOffset + (int)GetLockedResDataPtr(gDebuggerListing));
    if (*_Src == '\r')
      return 0;
    for (pNameEnd = _Src; *pNameEnd != ' '; pNameEnd++)
      ;
    _Size = pNameEnd - _Src;
    if (_Size > 0xff)
      FatalError(0,0x1134);
    memmove(name + 1, _Src, _Size);
    name[0] = (byte)_Size;
    pEntry = GetResDataPtr(gDebuggerWatches);
    for (i = 0; i < gDebuggerWatchCount; pEntry += 0x11a, i++) {
      if (*(int *)(pEntry + 0x12) == scopeNode &&
          (nameEqual = PascalStringsEqualNoCase((byte *)(pEntry + 2), name)) != 0) {
        marked = 1;
        break;
      }
    }
    UnlockResObject(gDebuggerWatches);
    return marked;
  }
  scopeNode = GetResDataPtr(gDebuggerBreakpoints);
  for (pEntry = 0; pEntry < gDebuggerBreakpointCount; scopeNode += 0x12, pEntry++) {
    if (*(int *)(scopeNode + 8) == rowOffset) {
      marked = 1;
      break;
    }
  }
  UnlockResObject(gDebuggerBreakpoints);
  return marked;
}


/* Finds the breakpoint for source line lineIdx of the current script (FindBreakpoint on the
 * line's token) and removes it, returning 1; returns 0 when there is none, in which case
 * HandleDebuggerClick adds one. The pair is the breakpoint toggle; the name says a query but the
 * function mutates. */
short __cdecl ClearBreakpointAtLine(int lineIdx)  /* 0x0040d380 */
{
    uint     bpIdx;
    ushort   bpFlags;  /* local_2 */

    /* the end-index call is written inline: &bpFlags is pushed before it runs */
    bpIdx = FindBreakpoint(gDebuggerFrame,
                          GetScriptTokenIndexForLine(lineIdx) * 8 + *(int *)(gDebuggerFrame + 0x14),
                          &bpFlags);
    if ((int)bpIdx < 0)
        return 0;
    RemoveBreakpoint(bpIdx);
    return 1;
}


/* Deletes breakpoint row bpIdx by copying the last 18-byte row over it and shrinking the table. */
void __cdecl RemoveBreakpoint(int bpIdx)  /* 0x0040d3d0 */
{
#pragma pack(push, 1)
    typedef struct { int w[4]; short s; } PlaylistEntry;
#pragma pack(pop)
    PlaylistEntry *pBase;

    /* The breakpoint count (0x00460dd4) is a word in the exe (`dec word ptr`, `movsx`); the
     * generated global is still an int, hence the casts. The last 18-byte row moves into the
     * freed slot as one struct copy. */

    (*(short *)&gDebuggerBreakpointCount)--;
    pBase = (PlaylistEntry *)GetResDataPtr(gDebuggerBreakpoints);
    pBase[bpIdx] = pBase[*(short *)&gDebuggerBreakpointCount];
    UnlockResObject(gDebuggerBreakpoints);
    ResizeResObject(gDebuggerBreakpoints, *(short *)&gDebuggerBreakpointCount * 0x12);
}


/* Scans the breakpoint table (gDebuggerBreakpoints) for the row matching the script frame's title
 * and path refs and the token index. Writes the row's flag word to *pOut and returns the row
 * index, or -1. */
int __cdecl FindBreakpoint(int pDataBase, int pSlotByte, ushort *pOut)  /* 0x0040D450 */
{
  int *pBp;
  int i;

  pBp = (int *)GetResDataPtr(gDebuggerBreakpoints);
  i = 0;
  if (0 < gDebuggerBreakpointCount) {
    do {
      if (((*pBp == *(int *)(pDataBase + 2)) && (pBp[1] == *(int *)(pDataBase + 8))) &&
         ((pSlotByte - *(int *)(pDataBase + 0x14)) >> 3 == pBp[3])) {
        *pOut = *(ushort *)(pBp + 4);   /* the word at +0x10 */
        UnlockResObject(gDebuggerBreakpoints);
        return i;
      }
      pBp = (int *)((int)pBp + 0x12);
      i = i + 1;
    } while (i < gDebuggerBreakpointCount);
  }
  UnlockResObject(gDebuggerBreakpoints);
  return -1;
}


/* Classic Mac button tracking: inverts the button rect (QdInvertRect), follows the cursor while the
 * mouse is held, inverting again as it enters or leaves the rect, waits out 10 ticks and returns 1
 * if the cursor ended inside the rect. HandleDebuggerClick uses it as the click feedback. */
ushort __cdecl TrackButtonPress(short *pRect)  /* 0x0040d4e0 */
{
    int    deadline;   /* declared first: edi, the flag bx */
    short  inside;
    int    mousePos;  /* local_4 */

    inside = 1;
    deadline = GetTick60Hz() + 10;   /* the exe forms the deadline up front */
    QdInvertRect(pRect);
    while (IsMouseStillDown() != 0) {
        GetSceneCursorPos(0, (short *)&mousePos);
        if (PointInSceneRect(*(ScenePointYX *)&mousePos, pRect) != inside) {
            QdInvertRect(pRect);
            inside ^= 1;
        }
    }
    while ((int)GetTick60Hz() < deadline)
        ;
    if (inside != 0)
        InvalidateSceneRect(pRect);
    QdPenNormal();
    return inside;
}


/* Edits the variable named at the start of listing line lineIdx of the node pScopeNode: reads its
 * value, opens the EDITVAR dialog (ShowEditVariableDialog), parses the answer as True / False, a
 * number or a string, writes the property back, and adds, updates or removes the watch row
 * (gDebuggerWatches, stride 0x11a: +0 active flag, +2 Pascal name, +0x12 the owning node). */
void __cdecl EditDebuggerVariable(int *pScopeNode, int lineIdx)  /* 0x0040d570 */
{
#pragma pack(push, 2)
    typedef struct { byte b[0x11a]; } PlEntry;   /* copied whole: rep movsd 0x46 + movsw */
#pragma pack(pop)
    char  *src, *end;
    int    len;
    short  err;
    short  tag;          /* property tag from LookupPropertyByName */
    /* The value block {type, dword value, short} that ReadPropertyValue fills and
     * WritePropertyValue consumes. Ghidra rendered the type stores as `_local_326 =
     * CONCAT22(2, local_326)` (tag and type are adjacent shorts); ported as a separate int the
     * type was never written, so a typed True / False / number was written back with the
     * property's OLD type (class 3). */
    short  value[4];
    byte   name[256];    /* Pascal name */
    byte   entry[0x11a]; /* {short active; name[16] at +2; ctx at +0x12; +0x16; +0x1a ...} */
    byte   numStr[256];  /* formatted decimal string */
    short  found;
    short  i;
    byte  *p;
    uint   n;
    int    base;

    src = (char *)GetDebuggerListingLineOffset(lineIdx);
    src += GetResDataPtr(gDebuggerListing);
    for (end = src; *end != ' '; end++)
        ;
    len = end - src;
    if (len > 0xff)
        FatalError(0, 0x1135);
    memmove(name + 1, src, len);   /* the exe CALLs the CRT copy (0x0044e440); /O2 would inline memcpy */
    name[0] = (byte)len;
    UnlockResObject(gDebuggerListing);

    err = LookupPropertyByName(pScopeNode, name, &tag, (int)value);
    if (err != 0)
        FatalError(err, 0x1136);
    err = ReadPropertyValue(pScopeNode, tag, value);
    if (err != 0)
        FatalError(err, 0x1137);

    found = -1;
    i = 0;
    *(short *)entry = i;
    CopyPascalString(name, entry + 2);
    *(int **)(entry + 0x12) = pScopeNode;
    *(int *)(entry + 0x16) = i;
    entry[0x1a] = (byte)i;
    p = (byte *)GetResDataPtr(gDebuggerWatches);
    for (; i < gDebuggerWatchCount; i++, p += 0x11a) {     /* stride 0x11a bytes */
        if (*(int **)(p + 0x12) == pScopeNode && PascalStringsEqualNoCase(p + 2, name) != 0) {
            *(PlEntry *)entry = *(PlEntry *)p;      /* matching entry: copy it, record index */
            found = i;
            break;
        }
    }
    UnlockResObject(gDebuggerWatches);

    if ((short)ShowEditVariableDialog((short *)entry, value, name) != 0)  /* 0x00432570: the dialog's EndDialog value, nonzero = cancelled */
        return;

    if (PascalStringsEqualNoCase(name, UpdatePascalLength(kStrTrueLower)) != 0) {
        /* Value == "True": type 2 / 1 */
        *(int *)&value[1] = 1;
        value[0] = 2;
        value[3] = 0;
    } else if (PascalStringsEqualNoCase(name, UpdatePascalLength(kStrFalseLower)) != 0) {
        /* Value == "False": type 2 / 0 */
        value[0] = 2;
        value[3] = 0;
        *(int *)&value[1] = 0;
    } else {
        /* Not a bool literal: try numeric type */
        n = ParseLeadingIntFromPascalString(name);
        FormatUintAsPascalDecimalString(n, (char *)numStr);
        if (PascalStringsEqualNoCase(name, numStr) != 0) {
            /* Matches formatted numeric: type 4 / numeric value */
            value[0] = 4;
            value[3] = 0;
            *(int *)&value[1] = n;
        } else {
            /* String value: allocate script string */
            err = AllocScriptString(name, value);
            if (err != 0)
                FatalError(err, 0x1138);
        }
    }

    err = WritePropertyValue(pScopeNode, tag, value);
    if (err != 0)
        FatalError(err, 0x1139);

    /* Update the watch table */
    if (found >= 0 && *(short *)entry == 0) {        /* now inactive: remove by compaction */
        gDebuggerWatchCount--;
        base = GetResDataPtr(gDebuggerWatches);
        *(PlEntry *)(base + found * 0x11a) = *(PlEntry *)(base + gDebuggerWatchCount * 0x11a);  /* last entry into the vacated slot */
        UnlockResObject(gDebuggerWatches);
        ResizeResObject(gDebuggerWatches, gDebuggerWatchCount * 0x11a);
        return;
    }
    if (found < 0 && *(short *)entry != 0) {         /* new active entry: append */
        ResizeResObject(gDebuggerWatches, (gDebuggerWatchCount + 1) * 0x11a);
        base = GetResDataPtr(gDebuggerWatches);
        base += gDebuggerWatchCount * 0x11a;   /* the slot offset is added to base before the copy */
        *(PlEntry *)base = *(PlEntry *)entry;
        UnlockResObject(gDebuggerWatches);
        gDebuggerWatchCount++;
        return;
    }
    if (found >= 0) {                                /* existing entry still active: update in place */
        base = GetResDataPtr(gDebuggerWatches);
        *(PlEntry *)(base + found * 0x11a) = *(PlEntry *)entry;
        UnlockResObject(gDebuggerWatches);
    }
}
