/* messages_channel.c -- TI.EXE 0x00438a10..0x004390e0: the messages channel (type 2), a typed command line. */

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


#include "topics/scenerect.h"

/* The handler for type-2 (messages) channels, reached through DispatchChannelMessage.
 * 0 hit (nothing), 1 redraw, 2 key (pParam is the key code; run under channel B's render lock),
 * 3 reload the CLUT when pParam is non-null, 4 end scene (fatal 0x10dc if the channel has no
 * window), 5/6 enter/leave render lock (a counted clip to the channel's scene rect; fatal 0x10dd
 * on an unbalanced leave), 7 move the window to the rect in pParam, 0xe report the scene rect,
 * 0x10 recentre the strip as InitMessagesChannel places it. */
void __cdecl MessagesChannelHandler(int pChannel, short msg, int *pParam)  /* 0x00438a10 */
{
    /* the 0xe reply: six zeroed control words, then the scene rect; the exe copies
     * it out as one 20-byte struct (rep movsd) */
    typedef struct { short ctl[6]; short rect[4]; } AnimChannelReply;
    short            rect[4];
    AnimChannelReply resp;
    short            mid;

    switch (msg)
    {
    case 0:
        HandleMessagesHit(pParam);
        return;

    case 1:
        DrawMessagesWindow();
        return;

    case 2:
        EnterSceneRenderLock((int)&gChannelB);
        HandleMessagesKey((short)(int)pParam);
        LeaveSceneRenderLock((int)&gChannelB);
        return;

    case 3:
        if (*(int *)&pParam != 0)
        {
            ReloadCLUT(gActiveClut);
        }
        return;

    case 4:
        if (gChannels[0].hResource == 0)
            FatalError(0, 0x10dc);
        DestroyChannelViewport((int)&gChannels);
        gChannels[0].hResource = 0;
        return;

    case 5:
        if (gAnimChannelRefCount == 0)
        {
            QdSetPort((int *)(pChannel + 0x12));
            GetSceneRect((int *)(pChannel + 0x12), rect);
            QdClipRect(rect);
        }
        gAnimChannelRefCount++;
        return;

    case 6:
        if (gAnimChannelRefCount == 0)
            FatalError(0, 0x10dd);
        gAnimChannelRefCount--;
        if (gAnimChannelRefCount == 0)
        {
            QdSetPort((int *)(pChannel + 0x12));
            QdClipRect((short *)&gScrollRect);
        }
        return;

    case 7:
        *(SceneViewport *)rect = *(SceneViewport *)pParam;
        ResizeChannelWindow(rect, pChannel);
        return;

    /* listed empty in the original: the exe's jump table is one-level (17 dword
     * entries), which VC++ 4.2 builds only when the gaps are explicit cases */
    case 8: case 9: case 10: case 11: case 12: case 13:
        return;

    case 0xe:
        /* The window rect lands in the local the scene rect then overwrites
         * (the exe has one 8-byte rect for both). */
        GetChannelWindowRect(pChannel, rect);
        resp.ctl[0] = 0; resp.ctl[1] = 0; resp.ctl[2] = 0;
        resp.ctl[3] = 0; resp.ctl[4] = 0; resp.ctl[5] = 0;
        GetSceneRect((int *)(pChannel + 0x12), rect);
        *(SceneViewport *)resp.rect = *(SceneViewport *)rect;
        *(AnimChannelReply *)pParam = resp;
        return;

    case 0x10:
        /* the strip rect of InitMessagesChannel, built in place from the canvas rect */
        *(__int64 *)rect = *(__int64 *)gScrollRect;
        mid =(gScrollSceneRect.x1 + gScrollSceneRect.x2) >> 1;
        rect[2] -= 7;                /* y2: viewH - 7  */
        rect[1] = mid - 0xda;        /* x1: midX - 218 */
        rect[3] = mid + 0xda;        /* x2: midX + 218 */
        rect[0] = rect[2] - 0x25;    /* y1: viewH - 44 */
        DispatchChannelMessage((short *)&gChannels, 7, (byte *)rect);
        return;

    default:
        return;
    }
}


/* One key typed into the messages window's input line (gTextEntryBuf, a Pascal string):
 * Enter (0x0d) runs the line through RunMessagesInputLine, shows its result in gMessageStr and
 * clears the line; Backspace (8) drops the last character; anything else is appended while
 * the line is under 255 characters. Every path ends by redrawing the window. */
void __cdecl HandleMessagesKey(short key)  /* 0x00438c90 */
{
    /* cdecl (the caller pushes and pops the key); the closing DrawMessagesWindow is a tail jmp */
    short len;

    QdSetPort((int *)&gChannels[0].hResource);
    if (key == 0xd) {
        RunMessagesInputLine(gTextEntryBuf);
        CopyPascalString(gTextEntryBuf, gMessageStr);
        gTextEntryBuf[0] = 0;
    }
    else {
        len = gTextEntryBuf[0];
        if (key == 8) {
            if (len > 0)
                gTextEntryBuf[0] = len - 1;
        }
        else if (len < 0xff) {
            gTextEntryBuf[0] = len + 1;
            (&gTextEntryBuf[1])[len] = (char)key;
        }
    }
    DrawMessagesWindow();
}


/* Empty (a bare ret), but cdecl with one parameter: MessagesChannelHandler pushes its pParam
 * and pops it. */
void __cdecl HandleMessagesHit(int *pParam)  /* 0x00438d10 */
{
    (void)pParam;
}


/* Repaints the messages window: erases it, draws the input line (gTextEntryBuf) in 12-point
 * text, a stippled rule under it, and the last result (gMessageStr) in 9-point text. */
void __stdcall DrawMessagesWindow(void)  /* 0x00438d20 */
{
    short inputRect[4];  /* local_8 */

    QdSetPort((int *)&gChannels[0].hResource);
    QdEraseRect((short *)&gScrollRect);
    QdTextSize(0xc);
    QdMoveTo(0xd, 0xf);
    QdDrawString(gTextEntryBuf);   /* byte[256] Pascal string */

    inputRect[0] = 0x15;
    inputRect[1] = 0xc;
    inputRect[2] = 0x16;
    inputRect[3] = 0x1a8;
    QdPenSize(1);
    QdFrameRectStippled(inputRect);

    QdTextSize(9);
    QdMoveTo(0xd, 0x21);
    QdDrawString(gMessageStr);   /* byte[256] Pascal string */
}


/* Formats a typed script value as a Pascal string: type 2 (bool) as True / False, type 3
 * (string) between quotes, type 4 as an unsigned decimal, anything else as the OK string.
 * The destination is pushed before each string-lookup call, hence the nested spelling. */
void __cdecl FormatScriptArgAsDescriptor(short *pArg, byte *pOut)  /* 0x00438DC0 */
{
  byte stringBuf [256];  /* local_100 */

  /* movsx + compare chain in the exe: a switch on the type word */
  switch (*pArg) {
  case 2:
    if (*(int *)(pArg + 1) != 0) {
      CopyPascalString((byte *)UpdatePascalLength(&kStrTrue), pOut);
      return;
    }
    CopyPascalString((byte *)UpdatePascalLength(&kStrFalse), pOut);
    return;
  case 3:
    CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), pOut);
    TakeScriptString(pArg, stringBuf);
    AppendPascalString(stringBuf, pOut);
    AppendPascalString((byte *)UpdatePascalLength(&kStrQuote), pOut);
    return;
  case 4:
    FormatUintAsPascalDecimalString(*(uint32_t *)(pArg + 1), (char *)pOut);
    return;
  default:
    CopyPascalString((byte *)UpdatePascalLength(&kStrOK), pOut);
    return;
  }
}


/* Sets up channel A (gChannels[0]) as the type-2 messages channel; called from
 * BeginGameSession. Fatal 0x10de if the channel already has a window. The window is a strip
 * near the bottom of the canvas: from y2 - 44 to y2 - 7 (37 pixels high) and 436 pixels wide,
 * centred on the canvas. The ChannelState header gets type 2, the dword at +10 = -1, the
 * window handle at +18 (0x12). */
void __stdcall InitMessagesChannel(void)  /* 0x00438ed0 */
{
    /* Frame as the exe: rect at +0, pResource at +8, the 146-byte channel
     * struct at +0xc (copied out whole: `rep movsd` 0x24 + `movsw`). */
    typedef struct { short s[4]; } Rect8;   /* {y1, x1, y2, x2} */
    Rect8        rect;
    void        *pResource;
    ChannelState state;
    short        midX;

    if (gChannels[0].hResource != 0)
        FatalError(0, 0x10de);

    gAnimChannelRefCount = 0;

    OpenAnimWindow((int *)&pResource);

    /* the canvas rect copied as two dwords, then adjusted in place */
    *(int *)&rect.s[0] = *(int *)&gScrollSceneRect.y1;
    *(int *)&rect.s[2] = *(int *)&gScrollSceneRect.y2;
    midX = (short)(((int)gScrollSceneRect.x1 + (int)gScrollSceneRect.x2) >> 1);
    rect.s[2] -= 7;
    rect.s[1] = midX - 0xda;
    rect.s[3] = midX + 0xda;
    rect.s[0] = rect.s[2] - 0x25;

    state.type = 2;
    *(int *)((char *)&state + 2) = 0;
    *(int *)((char *)&state + 6) = 0;
    *(int *)((char *)&state + 14) = 0;
    *(int *)((char *)&state + 10) = -1;
    state.hResource = pResource;

    SetChannelWindowState(pResource, (short *)&state);

    gChannels[0] = state;

    /* message 7 (set position) through the local copy */
    DispatchChannelMessage((short *)&state, 7, (byte *)&rect);
}


/* Runs one typed line (a Pascal string) as a script statement and writes the outcome back
 * into pLine: the formatted result, the tokenization-error string when the line does not
 * compile, or string 0x44c + status when the statement fails. */
void __cdecl RunMessagesInputLine(byte *pLine) /* 0x00438FC0 */
{
    short    status;
    short          *pMethodData;
    int            *hMethod;  /* local_60 */
    short           result[4];  /* local_5c */
    int             resolveAux;  /* local_54 */
    /* Ghidra's local_50 / local_4e / local_3c / local_32 / local_22 / local_12 are one
     * MethodCallFrame at ebp-0x50, as DispatchServerMessageString builds it (port-bug-classes.md #3). */
    MethodCallFrame frame;

    result[0] = -1;
    status = WaitHeapThenResolveMethodPath(pLine + 1, (unsigned int)*pLine, &hMethod, &resolveAux);
    if (status != 0) {
        CopyPascalString((byte *)UpdatePascalLength((char *)&kStrTokenizationError), pLine);   /* nested: the exe pushes pLine before the lookup call */
        return;
    }
    pMethodData = (short *)GetResDataPtr(hMethod);
    frame.isLast     = 1;
    frame.saved0x14c = 0xffffffffu;
    frame.pData      = pMethodData;
    CopyPascalString((byte *)UpdatePascalLength((char *)&kStrMessageBox), frame.buf0);
    CopyPascalString((byte *)UpdatePascalLength((char *)&kStrMessageBox), frame.buf2);
    CopyPascalString((byte *)UpdatePascalLength((char *)&kStrSystemColon), frame.buf1);
    status = InvokeMessagesMethodDispatchKernel((int)(intptr_t)&frame, pMethodData, (int *)0, result);
    UnlockResObject(hMethod);
    FreeResObject(hMethod);
    if (status != 0) {
        LoadStringResourceAsPascal(0x44c, (int)status, (char *)pLine);
        return;
    }
    FormatScriptArgAsDescriptor(result, pLine);
}


/* Runs one compiled statement typed into the messages window, against the globals scope
 * gGlobalsNode. Action opcodes (12000..12111) go to DispatchActionCommand, script opcodes
 * (20000..20122) to DispatchScriptCommand; 16000..16057 is tried as a script command first
 * and, when that fails, its result is formatted into gScratchStr and the statement re-run as
 * an action command. 0xfa2 / 0xfbd declare / undeclare a variable list. Anything else is
 * `identifier = value` (token 0x1f48 after the name), the value evaluated and written to the
 * property, or else a plain script block.
 *
 * Port note: this was a `return 0` stub for a long time, so no typed line ever ran. */
short __cdecl InvokeMessagesMethodDispatchKernel(int pCtx, short *pInstr, int *pTable,  /* 0x004390E0 */
                                             short *pResult)
{
    /* short: every path hands its callee's `ax` straight back (no movsx) */
    short op;
    short u;
    short u3;
    short tag;          /* local_10e */
    int   skip;         /* iStack_10c */
    short val[4];       /* local_108 */
    byte  name[256];    /* local_100 */

    op = *pInstr;
    if (op >= 12000 && op <= 0x2f4f)        /* the exe's bounds: jl 12000 / jg 12111 */
        return DispatchActionCommand(pCtx, pInstr, pTable, &skip);
    if (op >= 20000 && op <= 0x4e9a)
        return DispatchScriptCommand((int *)pCtx, pInstr, pTable, pResult, &skip);
    if (op >= 16000 && op <= 0x3eb9) {
        u = DispatchScriptCommand((int *)pCtx, pInstr, pTable, pResult, &skip);
        if ((short)u == 0)
            return 0;
        FormatScriptArgAsDescriptor(pResult, gScratchStr);
        *pResult = -1;
        return DispatchActionCommand(pCtx, pInstr, pTable, &skip);
    }
    if (op == 0xfa2)
        return DeclareVariableList(pInstr, (int *)gGlobalsNode, &skip);
    if (op == 0xfbd)
        return UndeclareVariableList(pInstr, (int *)gGlobalsNode, &skip);
    u3 = ExtractInstrIdentifier(pInstr, name);
    if ((short)u3 != 0)
        return ProcessScriptBlock(pCtx, pInstr, pTable, (int *)pResult, &skip);
    u3 = LookupPropertyByName((int *)gGlobalsNode, name, &tag, (int)pInstr);
    if ((short)u3 == 0 && pInstr[4] == 0x1f48) {
        u = ProcessScriptBlock(pCtx, pInstr + 8, pTable, (int *)val, &skip);
        if (u != 0)
            return u;
        return WritePropertyValue((int *)gGlobalsNode, tag, val);
    }
    return ProcessScriptBlock(pCtx, pInstr, pTable, (int *)pResult, &skip);
}
