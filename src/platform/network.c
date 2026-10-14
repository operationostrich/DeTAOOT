/* network.c -- TI.EXE 0x0040aec0..0x0040bae0: the network script commands and the plugin
 * entry points they use. */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "df_protos.h"
#include "topics/scriptcmd.h"
#include "topics/scriptprop.h"
#include "topics/multimedia.h"


/* Script command netconnect (opcode 12095). Looks up the network plugin directory, calls its
 * connect, stores the result in gNetworkOn and, once connected, fetches our id (gNetOurId) and
 * dispatches an addplayer event for every other member. Returns 0x3a if already connected, 0
 * quietly when there is no network directory, otherwise the first failing callback status. */
short __cdecl
ActionCmd_NetConnect(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040aec0 */
{
    short status;
    int   i;
    int   count;
    uint  pluginOut;  /* local_4 */

    /* Decode fixes: the loop APPENDS the member number to the prefix (AppendPascalString; the
     * port called CopyPascalString and overwrote the prefix), and the callbacks' failure
     * statuses are returned (the port returned 0 at the end). */
    *pSubSkip = 0;                      /* the four-argument handler: skip count cleared */
    if (gNetworkOn != 0)
        return 0x3a;

    status = LookupPluginByDescriptor((byte *)UpdatePascalLength((char *)kStrNetwork), (int *)&gNetDirEntry);
    if (status != 0)
        return 0;                       /* no network directory: quietly off */
    status = NetPluginCallStr((byte *)UpdatePascalLength((char *)kStrConnect), &pluginOut);
    if (status != 0)
        return status;
    gNetworkOn = (short)pluginOut;
    if (gNetworkOn == 0)
        return 0;
    status = NetPluginCallStr((byte *)UpdatePascalLength((char *)kStrGetourid), (uint *)&gNetOurId);
    if (status != 0)
        return status;
    status = NetPluginCallStrStr((byte *)UpdatePascalLength((char *)kStrGetmembercount),
                                       (byte *)UpdatePascalLength((char *)kStrEveryone), &pluginOut);
    if (status != 0)
        return status;
    count = (int)pluginOut;
    i = 1;
    if (count >= 1) do {
        status = NetPluginCallStrInt((byte *)UpdatePascalLength((char *)kStrGetmemberid), i, &pluginOut);
        if (status != 0)
            return status;
        if (gNetOurId != (int)pluginOut) {             /* every member but ourselves */
            CopyPascalString((byte *)UpdatePascalLength((char *)&kStrAddPlayerOpen), (byte *)gScratchStr);
            FormatUintAsPascalDecimalString(pluginOut, (char *)gScratchStr2);
            AppendPascalString((byte *)gScratchStr2, (byte *)gScratchStr);
            AppendPascalString((byte *)UpdatePascalLength((char *)kStrCommaEveryoneClose), (byte *)gScratchStr);
            status = DispatchServerMessageString((byte *)gScratchStr);
            if (status != 0)
                return status;
        }
    } while (++i <= count);
    return 0;
}


/* Script command netdisconnect (opcode 12096): calls the plugin's disconnect and clears
 * gNetworkOn. Returns 0x3a if not connected. */
short __cdecl
ActionCmd_NetDisconnect(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040b060 */
{
    uint  pluginOut;  /* local_4 */
    short status;

    *pSubSkip = 0;                      /* the four-argument handler: skip count cleared first */

    if (gNetworkOn == 0)
        return 0x3a;
    status = NetPluginCallStr(
        (byte *)UpdatePascalLength((char *)kStrDisconnect), (uint *)&pluginOut);
    if (status == 0) {
        gNetworkOn = 0;
        status = 0;
    }
    return status;
}


/* Script command netbroadcast (opcode 12097): builds a message from the arguments, as a
 * parenthesised comma list in a Pascal buffer, and sends it with the plugin's broadcast command. */
short __cdecl
ActionCmd_NetBroadcast(char *pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040b0b0 */
{
    /* The argument block walks the parameter with its start kept, each
     * separator step placed after the append it guards (byte-exact,
     * 0x0040b0b0). */
    short *start;
    short   status;
    int    sub1;
    short  argDesc[4];  /* local_114 */
    short  firstArg[4];
    uint32_t pluginOut;       /* 0x0040b235: LEA EAX,[ESP+0x24] -- the out slot; never read here */
    byte   msgBuf[256];  /* local_100 */

    if (gNetworkOn == 0)
        return 0x3a;

    start = pArgBlock;
    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)firstArg, &sub1);
    if (status != 0)
        return status;
    pArgBlock += sub1 * 4;
    if (*pArgBlock != (short)0xfb4)
        return 0x1c;
    pArgBlock += 4;
    status = ExtractInstrIdentifier(pArgBlock, msgBuf);
    if (status != 0)
        return status;
    AppendPascalString((byte *)UpdatePascalLength((char *)kStrLParen), msgBuf);   /* Append (0x00435250): the port copied, overwriting the message */
    pArgBlock += 4;
    if (*pArgBlock != (short)0xfb2)
        return 2;
    pArgBlock += 4;
    while (*pArgBlock != (short)0xfb3) {
        status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argDesc, &sub1);
        if (status != 0)
            return status;
        pArgBlock += sub1 * 4;
        FormatScriptArgAsDescriptor(argDesc, gScratchStr);
        AppendPascalString(gScratchStr, msgBuf);
        if (*pArgBlock == (short)0xfb4) {
            AppendPascalString((byte *)UpdatePascalLength((char *)kStrComma), msgBuf);
            pArgBlock += 4;
        }
    }
    pArgBlock += 4;
    *pSubSkip = ((int)pArgBlock - (int)start) >> 3;
    AppendPascalString((byte *)UpdatePascalLength((char *)kStrRParen), msgBuf);
    status = TakeScriptString(firstArg, gScratchStr);
    if (status == 0)
        /* 0x0040b235-0x0040b252: four arguments are pushed --
         * UpdatePascalLength(0x45b274), gScratchStr, the accumulated message
         * buffer, and the out-parameter. The port called this with none. */
        status = NetPluginCallStrStrStr((byte *)UpdatePascalLength((char *)kStrSendmessage),
                                             gScratchStr, msgBuf, &pluginOut);
    return status;
}


/* Script command netjoingroup (opcode 12098): one string argument, the group, passed to the
 * plugin's joingroup. Returns 0x3a if not connected. */
short __cdecl
ActionCmd_NetJoinGroup(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040B270 */
{
    short   u;
    uint   pluginOut;  /* local_4 */
    short  a[4];

    if (gNetworkOn == 0)
        return (uint)(unsigned short)0x3a;
    u = (uint)ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)a, pSubSkip);
    if (u == 0) {
        u = TakeScriptString(a, gScratchStr);
        if (u == 0) {
            u = (uint)(unsigned short)NetPluginCallStrStr(
                (byte *)UpdatePascalLength((char *)kStrJoingroup), gScratchStr, &pluginOut);
        }
    }
    return u;
}


/* Script command netleavegroup (opcode 12099): as ActionCmd_NetJoinGroup, with the plugin's
 * leavegroup. */
short __cdecl
ActionCmd_NetLeaveGroup(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040b2f0 */
{
    short  u;
    uint   pluginOut;  /* local_4 */
    short  a[4];

    if (gNetworkOn == 0)
        return (uint)(unsigned short)0x3a;
    u = (uint)ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)a, pSubSkip);
    if (u == 0) {
        u = TakeScriptString(a, gScratchStr);
        if (u == 0) {
            u = (uint)(unsigned short)NetPluginCallStrStr(
                (byte *)UpdatePascalLength((char *)kStrLeavegroup), gScratchStr, &pluginOut);
        }
    }
    return u;
}


/* Called at the end of TickScriptBindings. When gNetworkOn is set, runs NetPollNotification and,
 * if that succeeds, NetPollMessage. */
short NetPollEvents(void)  /* 0x0040b370 */
{
    short status;

    if (gNetworkOn == 0)
        return 0;

    status = (short)NetPollNotification();
    if (status == 0)
        status = NetPollMessage();   /* a tail jump in the exe */
    return status;
}


/* Called by NetPollEvents while gNetworkOn is set. Polls the plugin's "getnotify" command for a
 * pending network notification. "disconnected" dispatches the disconnected() event through
 * DispatchServerMessageString; otherwise a "player <id> joined / left <name>" notification is
 * parsed (the id with ParseLeadingIntFromPascalString) and dispatched as addplayer(<id>,<name>)
 * or delplayer(<id>,<name>). Returns 0, or 0x3a if the notification cannot be parsed. */
short __stdcall NetPollNotification(void)  /* 0x0040b390 */
{
    short err;
    short n;
    short kind;              /* -1 none, 1 first pattern, 0 alternate */
    uint  id;
    byte *p;
    byte *end;
    byte  buf[256];          /* Pascal result buffer */

    err = NetPluginCallStrGetStr(UpdatePascalLength(kStrGetnotify), buf);   /* "getnotify" */
    if (err != 0)
        return err;
    if (buf[0] == 0)
        return 0;

    if (PascalStringsEqualNoCase(UpdatePascalLength(kStrDisconnected), buf) != 0)      /* "disconnected" */
        return DispatchServerMessageString(UpdatePascalLength(kStrDisconnectedCall));       /* "disconnected()" */

    end = buf + 1 + buf[0];
    CopyPascalString(UpdatePascalLength(kStrPlayerSpace), gScratchStr);            /* "player " */
    if (CompareRawByteSlicesCaseFolded(gScratchStr + 1, buf + 1, (unsigned short)gScratchStr[0]) == 0)
        return 0x3a;

    n = 0;
    p = buf + 1 + gScratchStr[0];
    while (*p != 0x20) {
        n++;
        gScratchStr[n] = *p++;
    }
    kind = -1;
    gScratchStr[0] = (byte)n;
    id = ParseLeadingIntFromPascalString(gScratchStr);
    p++;

    CopyPascalString(UpdatePascalLength(kStrLeftSpace), gScratchStr);
    if (CompareRawByteSlicesCaseFolded(gScratchStr + 1, p, (unsigned short)gScratchStr[0]) != 0) {
        p += gScratchStr[0];
        kind = 1;
    }
    else {
        CopyPascalString(UpdatePascalLength(kStrJoinedSpace), gScratchStr);
        if (CompareRawByteSlicesCaseFolded(gScratchStr + 1, p, (unsigned short)gScratchStr[0]) != 0) {
            p += gScratchStr[0];
            kind = 0;
        }
    }
    if (kind < 0)
        return 0x3a;

    n = 0;
    while (p < end) {
        n++;
        gScratchStr[n] = *p++;
    }
    gScratchStr[0] = (byte)n;

    if (kind != 0)
        CopyPascalString(UpdatePascalLength(kStrDelPlayerOpen), buf);   /* "delplayer(" */
    else
        CopyPascalString(UpdatePascalLength(kStrAddPlayerOpen), buf);   /* "addplayer(" */
    FormatUintAsPascalDecimalString(id, (char *)gScratchStr2);
    AppendPascalString(gScratchStr2, buf);
    AppendPascalString(UpdatePascalLength(kStrCommaQuote), buf);     /* "," */
    AppendPascalString(gScratchStr, buf);
    AppendPascalString(UpdatePascalLength(kStrQuoteRParen), buf);     /* ")" */
    return DispatchServerMessageString(buf);
}


/* Called by NetPollEvents after NetPollNotification succeeds. Polls the plugin's "getmessage"
 * command for a pending message and, if it is not empty, dispatches it through
 * DispatchServerMessageString. */
short __stdcall NetPollMessage(void)  /* 0x0040b610 */
{
    short  status;
    byte   msgBuf[256];   /* Pascal string result buffer */

    /* returns the dispatch's own status (ax), the lookup inline */
    status = NetPluginCallStrGetStr(UpdatePascalLength(&kStrGetmessage), msgBuf);   /* "getmessage" */
    if (status == 0) {
        if (msgBuf[0] == 0)
            return 0;
        status = DispatchServerMessageString(msgBuf);
    }
    return status;
}


/* Script command networkon (opcode 20110): returns gNetworkOn. */
short __cdecl ScriptCmd_NetworkOn(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x0040B660 */
{
    *pSkipCount = 0;
    *(int *)(pResult + 1) = gNetworkOn;   /* value first, then the type word */
    *pResult = 2;
    pResult[3] = 0;
    return 0;
}


/* Script command netcountmembers (opcode 20111): one string argument, the group; returns the
 * plugin's getmembercount for it as an int. Returns 0x3a if not connected. */
short __cdecl ScriptCmd_NetCountMembers(int *pCtx, short *pArgBlock, int *pTable,
                                                            short *pResult, int *pSkipCount) /* 0x0040B690 */
{
    short pluginStatus;
    short  status;
    uint  memberCount;  /* local_c */
    short groupArg[4];  /* local_8 */

    if (gNetworkOn == 0)
        return 0x3Au;

    status = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)groupArg, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(groupArg, gScratchStr);
    if (status != 0)
        return status;

    pluginStatus = NetPluginCallStrStr((byte *)UpdatePascalLength((char *)&kStrGetmembercount), gScratchStr, &memberCount);   /* the lookup written inline */
    if (pluginStatus != 0)
        return pluginStatus;

    *(uint *)(pResult + 1) = memberCount;
    *pResult               = 4;
    pResult[3]             = 0;
    return 0;
}


/* Script command netindextomember (opcode 20112): one int argument, a member index; returns the
 * plugin's getmemberid for it as an int. Returns 0x3a if not connected, 10 if the argument is not
 * an int. */
short __cdecl ScriptCmd_NetIndexToMember(int *pCtx, short *pArgBlock, int *pTable,
                                                                 short *pResult, int *pSkipCount) /* 0x0040B730 */
{
    short pluginStatus;
    short  status;
    uint  memberId;  /* local_c */
    short typed[4];
    int   idxArg;

    if (gNetworkOn == 0)
        return 0x3Au;

    status = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)typed, pSkipCount);
    pluginStatus = status;
    if (pluginStatus != 0)
        return status;

    if (typed[0] != 4)
        return 10u;

    idxArg = *(int *)((char *)typed + 2);

    pluginStatus = NetPluginCallStrInt((byte *)UpdatePascalLength((char *)&kStrGetmemberid), idxArg, &memberId);
    if (pluginStatus == 0) {
        *(uint *)(pResult + 1) = memberId;
        *pResult               = 4;
        pResult[3]             = 0;
        return 0;
    }
    return pluginStatus;
}


/* Script command netourid (opcode 20113): returns gNetOurId. */
short __cdecl ScriptCmd_NetOurId(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x0040B7C0 */
{
    *pSkipCount = 0;
    if (gNetworkOn == 0)
        return 0x3A;

    *(uint *)(pResult + 1) = gNetOurId;   /* value first, then the type word */
    *pResult               = 4;
    pResult[3]             = 0;
    return 0;
}


/* Script command netmessagesender (opcode 20114): returns the sender id of the pending message
 * (the plugin's getmessagesender) as an int. Returns 0x3a if not connected. */
short __cdecl ScriptCmd_NetMessageSender(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x0040B800 */
{
    short status;
    uint  senderId;  /* local_4 */

    *pSkipCount = 0;
    if (gNetworkOn == 0)
        return 0x3Au;

    status = NetPluginCallStr((byte *)UpdatePascalLength((char *)&kStrGetmessagesender), &senderId);
    if (status == 0) {
        *(uint *)(pResult + 1) = senderId;   /* value first, then the type word */
        *pResult               = 4;
        pResult[3]             = 0;
        return 0;
    }
    return status;
}


/* Script command netmessagegroup (opcode 20115): returns the group name of the pending message
 * (the plugin's getmessagegroup) as a string. Returns 0x3a if not connected. */
short __cdecl ScriptCmd_NetMessageGroup(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x0040B860 */
{
    short status;
    byte   groupName[256];  /* local_100 */

    *pSkipCount = 0;
    if (gNetworkOn == 0)
        return 0x3Au;

    status = NetPluginCallStrGetStr((byte *)UpdatePascalLength((char *)&kStrGetmessagegroup), groupName);
    if (status == 0)
        return AllocScriptString(groupName, pResult);
    return status;
}


/* The plugin entry points (this and the four below) build a DfPluginMsg with verb 2 ("invoke"),
 * dispatch it through the main container entry gNetDirEntry and read the answer out of the result
 * slot the callee writes. This one sends one string and reads an int into *pOut. Return: 0 with
 * the out-parameter written, 5 if the callee produced no result of the expected kind, or the
 * callee's own non-zero status. */
short __cdecl
NetPluginCallStr(byte *pStr, uint *pOut) /* 0x0040b8d0 */
{
    DfPluginMsg msg;
    uint        st;

    msg.verb           = 2;
    msg.arg[0].code    = DF_PLUGIN_STR;   msg.arg[0].data = pStr;
    msg.arg[1].code    = DF_PLUGIN_NONE;
    msg.arg[2].code    = DF_PLUGIN_NONE;
    msg.arg[3].code    = DF_PLUGIN_NONE;
    msg.result.code    = DF_PLUGIN_NONE;

    st = InvokeEntryCallback(&gNetDirEntry, &msg);
    if ((short)st != 0)
        return (short)st;
    if (msg.result.code != DF_PLUGIN_INT)
        return 5;
    *pOut = (uint)(uintptr_t)msg.result.data;
    return 0;
}



/* Sends one command string and copies a STRING answer back into pResultBuf. Returns 0, the
 * callback's non-zero status, or 5 if it produced no string.
 *
 * Port note: Ghidra prints "Removing unreachable block (ram,0x0040b9b2)" over the copy, because
 * it cannot see the callee write through the message pointer. The port inherited the deletion,
 * copied the command into an unused local and returned 5 every time (class 2). */
short __cdecl NetPluginCallStrGetStr(byte *pCmd, byte *pResultBuf)  /* 0x0040B940 */
{
  DfPluginMsg msg;
  uint        st;
  byte        cmdBuf[256];

  CopyPascalString(pCmd, cmdBuf);

  msg.verb        = 2;
  msg.arg[0].code = DF_PLUGIN_STR;   msg.arg[0].data = cmdBuf;
  msg.arg[1].code = DF_PLUGIN_NONE;
  msg.arg[2].code = DF_PLUGIN_NONE;
  msg.arg[3].code = DF_PLUGIN_NONE;
  msg.result.code = DF_PLUGIN_NONE;

  st = InvokeEntryCallback(&gNetDirEntry, &msg);
  if ((short)st != 0)
    return (short)st;
  if (msg.result.code != DF_PLUGIN_STR)
    return 5;
  CopyPascalString((byte *)msg.result.data, pResultBuf);
  return 0;
}


/* Sends one string and one integer argument and reads an int into *pOut. The port had it as
 * `*pOut = 0; return 0;`, a stub reporting success (class 7). */
short __cdecl
NetPluginCallStrInt(byte *pStr, int idx, uint *pOut)  /* 0x0040b9e0 */
{
    DfPluginMsg msg;
    uint        st;

    msg.verb        = 2;
    msg.arg[0].code = DF_PLUGIN_STR;   msg.arg[0].data = pStr;
    msg.arg[1].code = DF_PLUGIN_INT;   msg.arg[1].data = (void *)(intptr_t)idx;
    msg.arg[2].code = DF_PLUGIN_NONE;
    msg.arg[3].code = DF_PLUGIN_NONE;
    msg.result.code = DF_PLUGIN_NONE;

    st = InvokeEntryCallback(&gNetDirEntry, &msg);
    if ((short)st != 0)
        return (short)st;
    if (msg.result.code != DF_PLUGIN_INT)
        return 5;
    *pOut = (uint)(uintptr_t)msg.result.data;
    return 0;
}


/* Sends two strings and reads an int into *pOut. */
short __cdecl NetPluginCallStrStr(byte *pNameA, byte *pNameB, uint32_t *pOut)  /* 0x0040ba60 */
{
    DfPluginMsg msg;
    uint        st;

    msg.verb        = 2;
    msg.arg[0].code = DF_PLUGIN_STR;   msg.arg[0].data = pNameA;
    msg.arg[1].code = DF_PLUGIN_STR;   msg.arg[1].data = pNameB;
    msg.arg[2].code = DF_PLUGIN_NONE;
    msg.arg[3].code = DF_PLUGIN_NONE;
    msg.result.code = DF_PLUGIN_NONE;

    st = InvokeEntryCallback(&gNetDirEntry, &msg);
    if ((short)st != 0)
        return (short)st;
    if (msg.result.code != DF_PLUGIN_INT)
        return 5;
    *pOut = (uint32_t)(uintptr_t)msg.result.data;
    return 0;
}


/* Sends three strings and reads an int into *pOut. The port had `return 0;`, a no-parameter stub
 * reporting success (class 7), while the binary takes four parameters and writes the last one. */
short __cdecl NetPluginCallStrStrStr(byte *pNameA, byte *pNameB, byte *pNameC,
                                           uint32_t *pOut)  /* 0x0040BAE0 */
{
    DfPluginMsg msg;
    uint        st;

    msg.verb        = 2;
    msg.arg[0].code = DF_PLUGIN_STR;   msg.arg[0].data = pNameA;
    msg.arg[1].code = DF_PLUGIN_STR;   msg.arg[1].data = pNameB;
    msg.arg[2].code = DF_PLUGIN_STR;   msg.arg[2].data = pNameC;
    msg.arg[3].code = DF_PLUGIN_NONE;
    msg.result.code = DF_PLUGIN_NONE;

    st = InvokeEntryCallback(&gNetDirEntry, &msg);
    if ((short)st != 0)
        return (short)st;
    if (msg.result.code != DF_PLUGIN_INT)
        return 5;
    *pOut = (uint32_t)(uintptr_t)msg.result.data;
    return 0;
}

