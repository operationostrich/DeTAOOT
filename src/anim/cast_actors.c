/* cast_actors.c -- TI.EXE 0x0040d9a0..0x00411cb0: the cast file and its actors
 * (opencastfile, the actor* commands, closecastfile) and the render-parameter calc that follows.
 * The tables: gCastTable (7-int rows, stride 0x1c) and gActorTable
 * (0xa0-byte rows: +0 visible count, +2 cast node, +6 resource, +0x22 frame counter, +0x24
 * frame count, +0x50 name, +0x60 set name, +0x70 star name, +0x80 pose name). */

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


#include "topics/savestate.h"

/* Script command opencastfile (opcode 12013). Evaluates one argument, validates the descriptor
 * (ValidateDescriptorPath), parses it with OpenCastNode into a 7-int cast row (slotEntry[1] is the
 * source resource handle) and appends the row to gCastTable (stride 0x1c). It then expands
 * gActorTable (stride 0xa0) with one 40-int row per item of the source data's array at
 * +0x938 / +0x93c (InitActorEntry), and CompleteCastOpen finishes the open. */
short __cdecl ActionCmd_OpenCastFile(int pCtx, short *pArgBlock,  /* 0x0040d9a0 */
                                     int *pTable, int *pSubSkip)
{
    /* Both rows are copied as whole structs (rep movsd 7 / 0x28, 0x0040d9a0). */
    typedef struct { int v[7]; } Row7;
    typedef struct { int v[40]; } Row40;
    short   status;
    int    nSrcEntries;
    int    iSlot0Base;
    int    iSrcData;
    int    i;
    uint  *pSrcArray;
    int    entryBuf[40];    /* 40-int (0xA0-byte) scratch for slot 0 entries */
    short  argResult[4];
    int    slotEntry[7];    /* 7-int (0x1C-byte) slot 1 entry output;
                               slotEntry[0]=handle, slotEntry[1]=srcHandle */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, (short *)gScratchStr2);
    if (status != 0)
        return status;

    status = ValidateDescriptorPath(gScratchStr2, 0x4343444f)   /* 'CCDO' */;
    if (status != 0)
        return status;

    status = OpenCastNode(gScratchStr2, slotEntry);
    if (status != 0)
        return status;

    gCastCount++;
    ResizeResObject(gCastTable, (unsigned int)(gCastCount * 0x1c));
    ((Row7 *)GetResDataPtr(gCastTable))[gCastCount - 1] = *(Row7 *)slotEntry;

    UnlockResObject(gCastTable);

    iSrcData    = GetResDataPtr((void *)slotEntry[1]);
    nSrcEntries = *(int *)(iSrcData + 0x938);
    pSrcArray   = (uint *)(iSrcData + 0x93c);

    ResizeResObject(gActorTable, (unsigned int)((gActorCount + nSrcEntries) * 0xa0));
    iSlot0Base = GetResDataPtr(gActorTable);

    DF_TRACE("OpenCastFile: registering %d actors (table had %d)",
             nSrcEntries, (int)gActorCount);

    for (i = 0; i < nSrcEntries; i++)
    {
        InitActorEntry(slotEntry, *pSrcArray, (short *)entryBuf);
        pSrcArray += 4;   /* stride: 4 ints = 16 bytes per source array entry */

        DF_TRACE("  actor[%d] name@+80 len=%d '%.*s'",
                 (int)gActorCount,
                 (int)((byte *)entryBuf)[80], (int)((byte *)entryBuf)[80],
                 (char *)entryBuf + 81);

        *(Row40 *)(iSlot0Base + gActorCount * 0xa0) = *(Row40 *)entryBuf;

        gActorCount++;
    }

    UnlockResObject(gActorTable);
    UnlockResObject((void *)slotEntry[1]);
    /* 0x0040db37: the call's ax is the handler's result (no xor ax,ax). */
    return CompleteCastOpen(slotEntry[0]);
}


/* Opens a cast node for ActionCmd_OpenCastFile: takes the matching pending render node
 * (TakeMatchingPendingRenderNode), attaches child 0, checks the 0x40000 magic (0x38 when wrong) and
 * copies the name and selector into the 7-int cast row. The twin of OpenSetNode and OpenStageNode. */
short __cdecl OpenCastNode(byte *pDesc, int *pRow) /* 0x0040db50 */
{
    short err;
    int *pi1;
    int *piRow;
    short u;
    int local4;

    err = TakeMatchingPendingRenderNode(pDesc, pRow);
    if (err != 0)
        FatalError(err, 4000);

    pi1 = pRow + 1;
    err = AttachRenderChild((int *)*pRow, 0u, (int **)(void *)(pRow + 1));
    if (err != 0)
        FatalError(err, 0xfa1);

    piRow = (int *)GetResDataPtr((int *)*pi1);
    if (*piRow != 0x40000) {
        UnlockResObject((int *)*pi1);
        u = DetachRenderChild(*pRow, 0u);
        if (u != 0)
            FatalError(u, 0xfa2);
        err = DeferRenderNodeClose(*pRow);
        if (err != 0)
            FatalError(err, 0xfa3);
        return 0x38u;
    }

    CopyPascalString((byte *)(piRow + 0x24a), (byte *)(pRow + 3));
    pRow[2] = piRow[0x249];
    UnlockResObject((int *)pRow[1]);   /* pRow[1], not *pi1: lets the compiler hoist the load above the store */

    err = AttachRenderChild((int *)*pRow, (unsigned)pRow[2], (int **)(void *)&local4);
    if (err != 0)
        FatalError(err, 0xfa4);

    u = DetachRenderChild(*pRow, (unsigned)pRow[2]);
    if (u != 0)
        FatalError(u, 0xfa5);
    return 0;
}


/* Fills a 0xa0-byte actor row (pActor) from cast child `selector` of pNodeRef: attaches it, copies
 * its three names (+0x2a / +0x3a / +0x4a), places the actor at its waypoint
 * (FindWaypointNodePosition), reads the frame count (ReadNamedRowChildUshort) and sets the defaults
 * (scale 1000, zclip 0xffff, speed 0x10). It is void in the exe; the one caller ignores the result. */
void __cdecl InitActorEntry(int *pNodeRef, uint32_t selector, short *pActor)  /* 0x0040DC90 */
{
  undefined2 savedWord;
  uint32_t *pPosField;
  short status;
  int pRow;
  uint32_t callResult;
  byte *unusedPtr;
  byte *pStrDst;
  /* ONE 8-byte position record filled by FindWaypointNodePosition and
   * copied whole (two dword moves, POINT is just an 8-byte type); hOut is a separate AttachRenderChild
   * out-slot. See PlaceActorsAtWaypoints for what the split costs. */
  short local_c_pos[4];
  uint32_t hOut;  /* local_4 */

  /* the context is stored, then read back through the record for the attach
   * (one load into ecx in the exe) */
  *(uint32_t **)(pActor + 1) = (uint32_t *)*pNodeRef;
  status = AttachRenderChild(*(uint32_t **)(pActor + 1), selector, (uint32_t *)(pActor + 3));
  if (status != 0) {
    FatalError(status, 0xfa6);
  }
  /* statement order measured: it sets the exe's store schedule */
  *pActor = 0;
  pActor[9] = 0;
  pActor[0xb] = 0;
  *(uint32_t *)(pActor + 5) = selector;
  pActor[10] = 0;
  pActor[0xd] = 0;
  pActor[0xe] = 0;
  pActor[0xf] = 0;
  pPosField = (uint32_t *)(pActor + 0xc);
  pActor[0xc] = 0;
  pActor[0x13] = 4;
  pActor[0x10] = 0x10;
  pActor[0x14] = 0xffff;
  *(uint32_t *)(pActor + 0x15) = 1000;
  pRow = GetResDataPtr(*(int **)(pActor + 3));
  *(uint32_t *)(pActor + 7) = *(uint32_t *)(pRow + 0x26);
  callResult = NextRenderNodeSerial();
  pActor[0x26] = 0;
  *(uint32_t *)(pActor + 0x17) = callResult;
  pActor[0x27] = 0;
  *(uint32_t *)(pActor + 0x19) = 0;
  *(uint32_t *)(pActor + 0x24) = 0;
  CopyPascalString((byte *)(pRow + 0x2a), (byte *)(pActor + 0x28));
  CopyPascalString((byte *)(pRow + 0x4a), (byte *)(pActor + 0x38));
  CopyPascalString((byte *)(pRow + 0x3a), (byte *)(pActor + 0x30));
  pStrDst = (byte *)(pActor + 0x48);
  CopyPascalString((byte *)UpdatePascalLength((char *)&kStrNone), pStrDst);   /* the destination is pushed before the lookup call */
  callResult = PascalStringsEqualNoCase((byte *)(pActor + 0x30), &DAT_0048a7b4);
  if ((short)callResult != 0) {
    callResult = FindWaypointNodePosition((byte *)(pActor + 0x38), local_c_pos);
    if ((short)callResult != 0) {
      savedWord = *(undefined2 *)pPosField;
      *(POINT *)(pActor + 0xc) = *(POINT *)local_c_pos;
      pActor[9] = 1;
      *(undefined2 *)pPosField = savedWord;
    }
  }
  if (*(int *)(pRow + 0x5a) > 0) {   /* the exe takes this case first */
    pRow += 0x5e;   /* add edi,0x5e in the exe */
    CopyPascalString((byte *)(pRow + 0x10), (byte *)(pActor + 0x40));
    status = ReadNamedRowChildUshort(*(uint32_t **)(pActor + 1), *(uint32_t *)pRow);
    pActor[0x12] = status;
    status = status + -1;
  }
  else {
    pStrDst = (byte *)(pActor + 0x40);
    CopyPascalString((byte *)UpdatePascalLength((char *)&kStrNone), pStrDst);
    status = 0;
    pActor[0x12] = 0;
  }
  pActor[0x11] = status;
  UnlockResObject(*(int **)(pActor + 3));
  status = AttachRenderChild(*(uint32_t **)(pActor + 1), *(uint32_t *)(pActor + 7), &hOut);
  if (status != 0) {
    FatalError(status, 0xfa7);
  }
  status = DetachRenderChild(*(int *)(pActor + 1), *(int *)(pActor + 7));
  if (status != 0) {
    FatalError(status, 0xfa8);
  }
}


/* Script command actorinstance (opcode 12075): two arguments, an actor and an alias name (at most
 * 15 characters). If the alias is already in gActorTable it returns success; otherwise it appends a
 * new 0xa0-byte row, a copy of the actor's row with the alias in the name field (+0x50), a fresh
 * alias handle (NextRenderNodeSerial) and the parent render child (entry+2, selector entry+10)
 * attached (FatalError 0xfa9 on failure).
 *
 * Port note: this handler and the other actor handlers that call EvalTwoScriptArgs take four
 * parameters: the fourth, the skip-count out-pointer, is read at [ESP+0x10], and the handler
 * forwards its own pTable. Ghidra shows three parameters and a NULL pTable, with which an
 * identifier argument cannot resolve (err 9 "No such variable"). Class 2. */
short __cdecl ActionCmd_ActorInstance(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040de80 */
{
    short          evalStatus;
    short           status;
    short          arg1Result[4];    /* first typed result from EvalTwoScriptArgs */
    short          arg2Result[4];    /* second typed result from EvalTwoScriptArgs */
    /* ONE 0xa0-byte actor row (Ghidra local_140); Ghidra's other locals are fields of it (+6 child
     * handle, +10 selector, +0x2e alias handle, +0x32 pad, +0x50 alias name). LookupActorEntry
     * copies 0x28 ints = 160 bytes into it: as a 6-byte array that was a 154-byte stack smash on
     * every call, and as five separate locals the appended row carried none of them and childId
     * was read uninitialised (class 3, ground truth 0x0040de80). */
    byte           entryBuf[160];
#define entryHandle (*(uint32_t *)(entryBuf + 6))
#define childId     (*(uint32_t *)(entryBuf + 10))
#define aliasHandle (*(uint32_t *)(entryBuf + 0x2e))
#define aliasPad    (*(uint32_t *)(entryBuf + 0x32))
#define aliasName   (entryBuf + 0x50)
    undefined4     lookupAliasOut[40]; /* local_a0 -- LookupActorEntry output for alias */
    typedef struct { int w[40]; } Row160;   /* copied as one object (rep movsd, ecx = 0x28) */

    evalStatus = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)arg1Result, (int *)arg2Result, pSubSkip);
    if (evalStatus != 0)
        return evalStatus;

    status = TakeScriptString(arg1Result, gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry(gScratchStr, (int *)entryBuf);
    if (status != 0)
        return status;

    status = TakeScriptString(arg2Result, gScratchStr2);
    if (status != 0)
        return status;

    /* Alias name must be <= 15 characters */
    if (gScratchStr2[0] >= 0x10)   /* was (byte)gScratchStr2: the array ADDRESS truncated to 8 bits */
        return 0x1a;

    status = LookupActorEntry(gScratchStr2, lookupAliasOut);
    if (status == 0)
        return 0;   /* found -- already exists, success */

    CopyPascalString(&gScratchStr2, aliasName);
    aliasHandle = NextRenderNodeSerial();
    aliasPad    = 0;

    /* Attach the render child with pParent = the handle STORED at entry+2 (the dword at +2, as
     * in the exe) and selector = entry+10. The port passed the field's stack ADDRESS as the
     * parent node, and the first `actorinstance` ever run (a cast script, jay1b1) crashed in
     * AttachRenderChild. */
    evalStatus = AttachRenderChild(*(int **)(entryBuf + 2), childId, (int **)(entryBuf + 6));
    if (evalStatus != 0)
        FatalError(evalStatus, 0xfa9);

    gActorCount++;
    ResizeResObject(gActorTable, gActorCount * 0xa0);
    ((Row160 *)GetResDataPtr(gActorTable))[gActorCount - 1] = *(Row160 *)entryBuf;
    UnlockResObject(gActorTable);
    return 0;
}


/* Script command actordelete (opcode 12093): removes the actor named by the argument from
 * gActorTable (TeardownActorEntry, then the last row is moved over it). Returns 10 if there is no
 * such actor. */
short __cdecl
ActionCmd_ActorDelete(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040E010 */
{
    typedef struct { int w[40]; } Slot0Row;
    int      i;
    short    u;
    short    notFound;
    int      base;
    short    a[4];
    Slot0Row entry;
    int      p;

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)a, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a, gScratchStr);
    if (u != 0)
        return u;

    notFound = 1;
    base = GetResDataPtr(gActorTable);
    i = 0;
    if (0 < gActorCount) {
        p = base;
        do {
            entry = *(Slot0Row *)p;
            /* the alias name is the row's +0x50 field (0x0040e09f LEA EAX,
             * [ESP+0x74]); the port compared the row's first bytes, so
             * actordelete never found an actor */
            if (PascalStringsEqualNoCase((byte *)&entry + 0x50, gScratchStr)) {
                TeardownActorEntry((int)&entry);
                gActorCount--;
                *(Slot0Row *)(base + i * 0xa0) = *(Slot0Row *)(base + gActorCount * 0xa0);
                notFound = 0;
                break;
            }
            p += 0xa0;
            i++;
        } while (i < gActorCount);
    }
    UnlockResObject(gActorTable);
    if (notFound)
        return 10;
    ResizeResObject(gActorTable, (unsigned int)(gActorCount * 0xa0));
    return 0;
}

/* Statement half of actorset (opcode 16021): sets the set an actor lives in (row +0x60) from the
 * second argument and, when that set is the open one (DAT_0048a7b4), places the actor at its star
 * (FindWaypointNodePosition: xyz to +0x1a / +0x1c / +0x1e, is-3D at +0x12). The twin of
 * ActionCmd_ActorStar, which carries the star name. */
short __cdecl
ActionCmd_ActorSet(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040e150 */
{
    short  s1;
    short  u, u3;
    short  a1[4], a2[4];
    typedef struct { int w[2]; } Pos8;
    int    named[0x28];
    short  pos[4];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    u = TakeScriptString(a2, (byte *)gScratchStr2);
    if (u != 0)
        return u;
    if (gScratchStr2[0] >= 0x10)
        return 0x1a;
    RemoveWalkEntry((byte *)gScratchStr);
    /* The row is local_a0 and Ghidra's other locals are fields of it: local_40 = row+0x60 (set
     * name), local_30 = row+0x70 (node name from actorxyz), local_88/local_84 = +0x18/+0x1c
     * (xyz), local_8e = +0x12 (is-3D). Actor rows sit two bytes later than prop rows (0x5e /
     * 0x6e) because of the 0xa0 vs 0x9e layout. The port had the local names as offsets and
     * looked the node up by the set name (the same defect as ActionCmd_PropSet). */
    CopyPascalString((byte *)gScratchStr2, (byte *)named + 0x60);
    if (PascalStringsEqualNoCase((byte *)named + 0x60, (byte *)&DAT_0048a7b4) != 0
        && FindWaypointNodePosition((byte *)named + 0x70, pos) != 0) {
        /* the exe copies the whole 8-byte record over +0x18 and puts the
         * first word back: net effect +0x1a/+0x1c/+0x1e = pos[1..3] */
        short keep = *(short *)((char *)named + 0x18);
        *(Pos8 *)((char *)named + 0x18) = *(Pos8 *)pos;
        *(short *)((char *)named + 0x18) = keep;
    }
    *(short *)((char *)named + 0x12) = 1;
    WriteBackActorEntry(named);
    return 0;
}


/* Value half of actorset (opcode 16021): returns the actor's set name, the Pascal string at row
 * +0x60. */
short ScriptCmd_ActorSet(int *pCtx, short *pArgBlock, int *pTable,  /* 0x0040e280 */
                                  short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    /* One 160-byte record (LookupActorEntry copies 0x28 ints), not a 96+64 split: with separate
     * buffers GCC guarantees no adjacency, so the copy overran and the string field was read
     * uninitialised (class 3). The other ScriptCmd_Actor* readers below have the same single
     * record. */
    byte  entryRec[160];   /* string field at byte offset 96 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)entryRec);
    if (status != 0)
        return status;

    return AllocScriptString(entryRec + 96, pResult);   /* its status is the result */
}


/* Script command actorexists (opcode 20023): returns a bool, whether an actor of that name is in
 * gActorTable. */
short __cdecl ScriptCmd_ActorExists(int *pCtx, short *pArgBlock,  /* 0x0040e300 */
                                            int *pTable, short *pResult,
                                            int *pSkipCount)
{
    short  status;
    uint  lookupResult;
    short argResult[4];
    int   dataBuffer[40];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    lookupResult = LookupActorEntry((byte *)&gScratchStr, dataBuffer);
    if ((short)lookupResult != 0)       /* two branches, not a setcc; the miss branch first */
        *(int *)(pResult + 1) = 0;
    else
        *(int *)(pResult + 1) = 1;
    pResult[3] = 0;
    *pResult   = 2;   /* type: integer */
    return 0;
}


/* Statement half of actorstar (opcode 16006): sets the actor's star (node) name, row +0x70, from the
 * second argument. See ActionCmd_ActorSet. */
short __cdecl ActionCmd_ActorStar(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040E390 */
{
    short  s1;
    typedef struct { int w[2]; } Pos8;
    short  u, u3;
    short  a1[4], a2[4];
    int    named[0x28];
    short  pos[4];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    u = TakeScriptString(a2, (byte *)gScratchStr2);
    if (u != 0)
        return u;
    if (gScratchStr2[0] >= 0x10)
        return 0x1a;
    RemoveWalkEntry((byte *)gScratchStr);
    /* This is ActionCmd_ActorSet with the copy target swapped: the second argument is the STAR
     * (node) name and goes to row+0x70 (local_30); the set name at row+0x60 is left alone and
     * compared with the current scene; the node lookup uses row+0x70; xyz land at
     * +0x1a/+0x1c/+0x1e and is-3D at +0x12. The previous body copied into a scratch local, so
     * the row never changed (class 3). */
    CopyPascalString((byte *)gScratchStr2, (byte *)named + 0x70);
    /* The star name read back by the `actorstar (me)` query has come back with its LAST
     * CHARACTER replaced ("penny1" read back as "pennyà"): print the source buffer
     * and the row field; equal means the row is clobbered after this point, different means the
     * slot handed us a short string. */
    DF_TRACE("actorStar: src(len %d)='%.*s' -> row+0x70(len %d)='%.*s' actor='%.*s'",
             (int)gScratchStr2[0], (int)gScratchStr2[0], (const char *)gScratchStr2 + 1,
             (int)((byte *)named)[0x70], (int)((byte *)named)[0x70],
             (const char *)named + 0x71,
             (int)((byte *)named)[0x50], (const char *)named + 0x51);
    u3 = PascalStringsEqualNoCase((byte *)named + 0x60, (byte *)&DAT_0048a7b4);
    if (u3 != 0) {
        u3 = FindWaypointNodePosition((byte *)named + 0x70, pos);
        if (u3 != 0) {
            /* the whole 8-byte record over +0x18, then the first word put
             * back: row+0x1a/+0x1c/+0x1e = pos[1..3] */
            short keep = *(short *)((char *)named + 0x18);
            *(Pos8 *)((char *)named + 0x18) = *(Pos8 *)pos;
            *(short *)((char *)named + 0x18) = keep;
        }
    }
    *(short *)((char *)named + 0x12) = 1;
    WriteBackActorEntry(named);
    return 0;
}


/* Value half of actorstar (opcode 16006): returns the actor's star name, the Pascal string at row
 * +0x70. */
short ScriptCmd_ActorStar(int *pCtx, short *pArgBlock, int *pTable,  /* 0x0040e4d0 */
                                  short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    byte  entryRec[160];   /* string field at byte offset 112 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)entryRec);
    if (status != 0)
        return status;

    return AllocScriptString(entryRec + 112, pResult);   /* its status is the result */
}


/* Statement half of actorpose (opcode 16014): sets the actor's pose, the sprite binding named at
 * +0x80, with its frame count (+0x24, from SetActorFrameCountForPose) and frame counter (+0x22 =
 * count - 1). */
short __cdecl
ActionCmd_ActorPose(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040E550 */
{
    short  s1;
    short   u;
    int    skip;
    short  a1[4], a2[4];
    int    named[0x28];
    short  c;

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    u = TakeScriptString(a2, (byte *)gScratchStr2);
    if (u != 0)
        return u;
    if (gScratchStr2[0] >= 0x10)
        return 0x1a;
    u = SetActorFrameCountForPose(named, (byte *)gScratchStr2);
    if (u != 0)
        return 10;
    /* +0x22 = +0x24 - 1 (the view index LookupSpriteFrameGeometryByAngle reads at +0x22), and
     * the pose name goes to +0x80, which that lookup compares against each binding's name. The
     * port used Ghidra's local NAMES as offsets and copied the name to a scratch buffer, so
     * actorpose never changed the sprite; it also resolved the second argument into the first
     * name buffer. */
    c = *(short *)((char *)named + 0x24);
    *(short *)((char *)named + 0x22) = c - 1;
    CopyPascalString((byte *)gScratchStr2, (byte *)named + 0x80);
    WriteBackActorEntry(named);
    return 0;
}


/* Value half of actorpose (opcode 16014): returns the actor's pose name, the Pascal string at row
 * +0x80. */
short ScriptCmd_ActorPose(int *pCtx, short *pArgBlock, int *pTable,  /* 0x0040e650 */
                                  short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    byte  entryRec[160];   /* string field at byte offset 128 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)entryRec);
    if (status != 0)
        return status;

    return AllocScriptString(entryRec + 128, pResult);   /* its status is the result */
}


/* Statement half of actordeg (opcode 16002): sets the actor's facing angle (row +0x18, low byte).
 * The first script argument checked is the angle, which must be type 4. */
short __cdecl
ActionCmd_ActorDeg(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040E6D0 */
{
    short  s1;
    short   u;
    int    named[0x28], skip, second;
    short  a1[4], a2[4];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    /* First script arg must be type 4 (Ghidra local_b0); not named[0]. */
    if (a2[0] != 4)   /* 0x0040e6d0 tests local_b0, arg 2's tag; a1 is the descriptor and never type 4 */
        return 0x0E;
    /* local_88 = local_ae & 0xff: the actor's facing at row+0x18 (+0x88 was the local's NAME
     * used as an offset, inside the name strings) */
    *(unsigned short *)((char *)named + 0x18) = (unsigned short)(*(int *)&a2[1] & 0xFF);
    WriteBackActorEntry(named);
    return 0;
}


/* Statement half of actordist (opcode 16012): for a 2-D actor (is-3D word +0x12 zero) and a
 * negative value, stores it as the render-order key (+0x28). */
short __cdecl
ActionCmd_ActorDist(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040E770 */
{
    short  s1;
    short   u;
    int    skip, ival;
    short  a1[4], a2[4];
    int    named[0x28];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    if (a2[0] != 4)
        return 0x0e;
    /* `if ((local_8e == 0) && (local_ae < 0)) local_78 = (short)local_ae;`: both are fields of
     * the actor row at local_a0, +0x12 the is-3D flag and +0x28 the render-order key. The key
     * is at +0x28 here but +0x26 in the prop twin (ActionCmd_PropDist): actor rows sit two bytes
     * off from prop rows. The port tested the argument's high half and used local_7a's NAME as
     * the offset. */
    if (*(short *)((char *)named + 0x12) == 0 && *(int *)&a2[1] < 0)
        *(short *)((char *)named + 0x28) = (short)*(int *)&a2[1];
    WriteBackActorEntry(named);
    return 0;
}


/* Value half of actordist (opcode 16012): the actor's draw-order distance. If the is-3D word
 * (+0x12) is zero, or CalcActorRenderParams fails, it is the stored key (row +0x28); otherwise the
 * sentinel 32000 (the maximum distance). */
short ScriptCmd_ActorDist(int *pCtx, short *pArgBlock, int *pTable,  /* 0x0040e820 */
                                   short *pResult, int *pSkipCount)
{
    short   uVar;
    short  argResult[4];
    /* local_c4[12] + local_b8 run from ebp-0xc4 to local_a0 at ebp-0xa0: ONE 36-byte
     * calc-output record, local_b8 its short index 6, the render-order key / distance. As
     * calcBuf[12] + calcResult + pad[11] the 12-byte piece went to CalcActorRenderParams as its
     * 36-byte out-pointer: __stack_chk_fail on 'cash' in the lounge. */
    short  calcOut[18];    /* local_c4 -- 36 bytes; [6] is local_b8 */
    int    dataBuffer[40]; /* full 160-byte entry -- LookupActorEntry copies 0x28
                              ints; this was int[4] (16 bytes) and overran by 144.
                              Reads at data[9] (byte 18) and data[20] (byte 40)
                              were both past the old declaration. */
    /* Ghidra locals: local_8e = data[9], local_78 = data[20] */

    uVar = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                              (int *)argResult, pSkipCount);
    if (uVar != 0)
        return uVar;

    uVar = TakeScriptString(argResult, &gScratchStr);
    if (uVar != 0)
        return uVar;

    uVar = LookupActorEntry((byte *)&gScratchStr, dataBuffer);
    if (uVar != 0)
        return uVar;

    {
        short *pData = (short *)dataBuffer;

        /* 0x0040e884: the 3D case is tested first. The exe pushes only
         * (row, out): 0x00411210 reads its row at [esp+4] and its output at
         * [esp+8]. The fresh distance is read straight from the
         * output record -- the exe never writes it back into this local row. */
        if (pData[9] != 0) {
            if (CalcActorRenderParams(pData, calcOut) != 0)
                *(int *)(pResult + 1) = (int)calcOut[6];
            else
                *(int *)(pResult + 1) = 32000;
        } else {
            *(int *)(pResult + 1) = (int)pData[20];
        }
    }
    pResult[3] = 0;
    *pResult = 4;   /* type: int */
    return 0;
}


/* Value half of actordeg (opcode 16002): returns the actor's facing angle, row +0x18, as a type-4
 * int. */
short ScriptCmd_ActorDeg(int *pCtx, short *pArgBlock, int *pTable,  /* 0x0040e8e0 */
                              short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];    /* 8-byte typed result from ProcessScriptBlock */
    short dataBuffer[80];  /* 160-byte result from LookupActorEntry */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)dataBuffer);
    if (status != 0)
        return status;

    pResult[0] = 4;   /* type: int */
    *(int *)(pResult + 1) = (int)dataBuffer[12];  /* value at byte offset 24 */
    pResult[3] = 0;
    return 0;
}


/* Statement half of actorspeed (opcode 16023): stores the second argument, an int clamped to at
 * least 1, as a short in the actor row at +0x26. */
short __cdecl
ActionCmd_ActorSpeed(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040E970 */
{
    short  s1;
    short   u;
    int    skip, ival;
    short  a1[4], a2[4];
    int    named[0x28];
    short  v7a;

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    if (a2[0] != 4)
        return 0x0e;
    *(short *)((char *)named + 0x26) = (short)*(int *)&a2[1];
    if (*(short *)((char *)named + 0x26) < 1)
        *(short *)((char *)named + 0x26) = 1;   /* 0x0040e970: local_7a = ebp-0x7a = record +0x26 */
    /* ONE store, not two: Ghidra's `local_7a` is the NAME of ebp-0x7a, record offset +0x26 (the
     * store above); taking the name as a byte offset added a second store over characters 10-11
     * of the star name at +0x70 ("penny1" read back as "pennyà" in the gym). */
    WriteBackActorEntry(named);
    return 0;
}


/* Statement half of actorscale (opcode 16024): stores the second argument, an int clamped to at
 * least 0, as a dword in the actor row at +0x2a. */
short __cdecl
ActionCmd_ActorScale(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040EA20 */
{
    short  s1;
    short   u;
    int    skip, ival;
    short  a1[4], a2[4];
    int    named[0x28];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    if (a2[0] != 4)
        return 0x0e;
    *(int *)((char *)named + 0x2a) = *(int *)&a2[1];
    if (*(int *)((char *)named + 0x2a) < 0)
        *(int *)((char *)named + 0x2a) = 0;   /* 0x0040ea20: local_76 = ebp-0x76 = record +0x2a */
    /* ONE store, not two (as in ActionCmd_ActorSpeed): local_76 is the NAME of ebp-0x76, record
     * offset +0x2a; the extra store landed over characters 6-9 of the star name at +0x70. */
    WriteBackActorEntry(named);
    return 0;
}


/* Value half of actorscale (opcode 16024): returns the dword at row +0x2a as a type-4 int. */
short ScriptCmd_ActorScale(int *pCtx, short *pArgBlock, int *pTable,  /* 0x0040ead0 */
                                short *pResult, int *pSkipCount)
{
    short  status;
    int   scale;
    short argResult[4];
    short dataBuffer[80];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)dataBuffer);
    if (status != 0)
        return status;

    scale = *(int *)(dataBuffer + 21);  /* loaded before pResult in the exe */
    *(int *)(pResult + 1) = scale;
    *pResult = 4;   /* type: int */
    pResult[3] = 0;
    return 0;
}


/* Value half of actorspeed (opcode 16023): returns the short at row +0x26, sign-extended, as a
 * type-4 int. */
short ScriptCmd_ActorSpeed(int *pCtx, short *pArgBlock, int *pTable,  /* 0x0040eb60 */
                               short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    short dataBuffer[80];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)dataBuffer);
    if (status != 0)
        return status;

    *pResult = 4;   /* type: int */
    *(int *)(pResult + 1) = (int)dataBuffer[19];
    pResult[3] = 0;
    return 0;
}


/* Script command actorwarm (opcode 12085): looks the actor up and re-attaches its render children
 * (ReattachSlot0RowRenderChildren). */
short __cdecl
ActionCmd_ActorWarm(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040ebf0 */
{
    short  u;
    short a[4];
    int   named[40];

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)a, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a, gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry(gScratchStr, named);
    if (u != 0)
        return u;
    ReattachSlot0RowRenderChildren(named);
    return 0;
}


/* Script command castwarm (opcode 12088): looks the cast up by name and re-attaches the render
 * children of every actor of that cast (rows whose node at +2 is the cast's). */
short __cdecl
ActionCmd_CastWarm(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040ec70 */
{
    short u;
    short a[4];
    int   slot1[7];
    int   iEntry;
    int  *iSlotBase;
    int  *row;

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)a, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a, gScratchStr);
    if (u != 0)
        return u;
    u = LookupCastEntryByName(gScratchStr, slot1);
    if (u != 0)
        return u;
    row = (int *)GetResDataPtr(gActorTable);
    for (iEntry = 0; iEntry < gActorCount; iEntry++) {
        if (*(int *)((char *)row + 2) == slot1[0])
            ReattachSlot0RowRenderChildren(row);
        row = (int *)((char *)row + 0xa0);
    }
    UnlockResObject(gActorTable);
    return 0;
}


/* Script command actorlock (opcode 12107): a name and a bool. True warms the actor's render
 * children (WarmActorRenderChildren), false unwarms them, and the row is written back. */
short __cdecl
ActionCmd_ActorLock(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040ED20 */
{
    short  s1;
    short   u;
    int    skip, secondVal;
    short  a1[4], a2[4];
    int    named[40];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    if (a2[0] != 2)     /* the flag's tag (0x0040ed71 CMP [ESP],2); a1 is the descriptor, type 3 */
        return 0x0E;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    if (*(int *)&a2[1] != 0)
        WarmActorRenderChildren(named);
    else
        UnwarmActorRenderChildren(named);
    WriteBackActorEntry(named);
    return 0;
}


/* Script command castlock (opcode 12110): as actorlock, for every actor of the named cast. */
short __cdecl
ActionCmd_CastLock(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040EDD0 */
{
    short  u;
    short  a1[4], a2[4];
    int    row[7];
    int    k;
    char  *p;

    u = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    if (a2[0] != 2)     /* the flag's tag (0x0040ee20); a1 is the descriptor, type 3 */
        return 0x0E;
    u = LookupCastEntryByName((byte *)gScratchStr, row);
    if (u != 0)
        return u;
    p = (char *)GetResDataPtr((void *)gActorTable);
    for (k = 0; k < gActorCount; k++) {
        /* the slot-0 row's slot-1 handle is the int at BYTE +2 (0x0040ee6a
         * CMP [ESI+0x2]); the port had read iVar3[2], byte +8 */
        if (*(int *)(p + 2) == row[0]) {
            if (*(int *)&a2[1] != 0)
                WarmActorRenderChildren((int *)p);
            else
                UnwarmActorRenderChildren((int *)p);
        }
        p += 0xA0;
    }
    UnlockResObject((int *)gActorTable);
    return 0;
}


/* Statement half of actorvisible (opcode 16001): takes an actor name and a bool and writes the
 * bool into the actor row's first word, the visible flag that CalcActorRenderParams tests
 * (a value of 0 or less is not drawn).
 *
 * Port note: four parameters. Ghidra shows two and passes NULL for EvalTwoScriptArgs's
 * out-parameters, but 0x0040eeb0 loads arg4 from [ESP+0x10] before its prologue and pushes SIX
 * arguments (ADD ESP,0x18). With NULLs ProcessScriptBlock wrote its result through a NULL pointer
 * and segfaulted the moment a cast method used opcode 16001 (class 2). */
short __cdecl ActionCmd_ActorVisible(int pCtx, int pArgBlock,  /* 0x0040EEB0 */
                                                     int *pTable, int *pSubSkip)
{
  short u;
  short a1[4];              /* arg1 result: fed to TakeScriptString */
  short a2[4];              /* arg2 result: [0]=type (2), value at +2 */
  int   named[40];          /* local_a0: 160-byte LookupActorEntry record */

  u = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
  if (u != 0)
    return u;
  u = TakeScriptString(a1, &gScratchStr);
  if (u != 0)
    return u;
  u = LookupActorEntry(&gScratchStr, named);
  if (u != 0)
    return u;
  if (a2[0] != 2)
    return 0xe;
  /* 0x0040ef2b: MOV EAX,[ESP+2] / MOV [ESP+0x10],AX -- the value's low
   * short goes into the record's first word before the release */
  *(short *)named = (short)*(int *)(a2 + 1);
  WriteBackActorEntry(named);
  return 0;
}


/* Value half of actorvisible (opcode 16001): returns a bool, whether the actor's visible word
 * (row +0) is at least 1. Returns the lookup's failure status when there is no such actor. */
short ScriptCmd_ActorVisible(int *pCtx, short *pArgBlock, int *pTable,  /* 0x0040ef50 */
                                short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];    /* 8-byte typed result from ProcessScriptBlock */
    short dataBuffer[80];  /* 160-byte result from LookupActorEntry */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)dataBuffer);
    /* 0x0040efa6: the exe returns the lookup's failure status (the port had
     * dropped this test and answered from an unfilled buffer) */
    if (status != 0)
        return status;
    if (dataBuffer[0] > 0)
        *(int *)(pResult + 1) = 1;
    else
        *(int *)(pResult + 1) = 0;
    pResult[3] = 0;
    pResult[0] = 2;   /* type: short (boolean) */
    return 0;
}


/* Script command actorhide (opcode 12104): a name and a bool; adjusts the actor's visible count
 * (row word 0): true decrements it, false increments it clamped to 1. The name "all" applies it to
 * every actor. */
short __cdecl
ActionCmd_ActorHide(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040EFF0 */
{
    short  u;
    short  a1[4], a2[4];
    short  named80[80];
    int    i;
    short *pRow0;

    u = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    if (a2[0] != 2)     /* the flag's tag (0x0040f04a); a1 is the descriptor, type 3 */
        return 0x0E;
    u = LookupActorEntry((byte *)gScratchStr, (int *)named80);
    if (u != 0) {
        /* Name not in table: only proceed if the requested name is the global wildcard. */
        if (PascalStringsEqualNoCase(
                (byte *)UpdatePascalLength((char *)&kStrAll), (byte *)gScratchStr) == 0)
            return u;
        pRow0 = (short *)GetResDataPtr((void *)gActorTable);
        for (i = 0; i < gActorCount; i++) {
            if (*(int *)&a2[1] != 0)
                pRow0[0]--;
            else if (++pRow0[0] > 0)
                pRow0[0] = 1;
            pRow0 = (short *)((char *)pRow0 + 0xA0);
        }
        UnlockResObject((int *)gActorTable);
        return 0;
    }
    if (*(int *)&a2[1] != 0)
        named80[0]--;
    else if (++named80[0] > 0)
        named80[0] = 1;
    WriteBackActorEntry((int *)named80);
    return 0;
}


/* Statement half of actorxyz (opcode 16003): an actor and three ints. Stores x, y, z as shorts at
 * +0x1a / +0x1c / +0x1e, sets the is-3D word (+0x12) to 1, drops the actor's walk entry
 * (RemoveWalkEntry) and copies the wildcard name (kStrCustom) into the row at +0x70. */
short __cdecl
ActionCmd_ActorXyz(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040F140 */
{
    /* four separate 8-byte typed values (frame: x +0, y +8, z +0x10,
     * desc +0x18, the row +0x20), each type tested with its own return */
    short  s1;
    short   u;
    short  x[4];
    short  y[4];
    short  z[4];
    short  desc[4];
    int    named[0x28];

    s1 = EvalFourScriptArgs(pCtx, pArgBlock, pTable, desc, x, y, z, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(desc, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    if (x[0] != 4)
        return 0x0E;
    if (y[0] != 4)
        return 0x0E;
    if (z[0] != 4)
        return 0x0E;
    RemoveWalkEntry((byte *)gScratchStr);
    /* 0x0040f201..0x0040f226 (row at [ESP+0x20], stores relative to it):
     *   x -> +0x1a, y -> +0x1c, z -> +0x1e (shorts), +0x12 = 1,
     *   wildcard name copied INTO the row at +0x70. */
    *(short *)((char *)named + 0x1a) = (short)*(int *)(x + 1);
    *(short *)((char *)named + 0x1c) = (short)*(int *)(y + 1);
    *(short *)((char *)named + 0x1e) = (short)*(int *)(z + 1);
    *(short *)((char *)named + 0x12) = 1;
    CopyPascalString((byte *)UpdatePascalLength((char *)&kStrCustom), (byte *)named + 0x70);
    WriteBackActorEntry(named);
    return 0;
}



/* Value half of actorxyz (opcode 16003): an actor and an index 1-4. 1, 2, 3 return x, y, z (row
 * +0x1a / +0x1c / +0x1e) sign-extended; 4 returns x and y packed into one int. Error 0x0e on a type
 * mismatch or an index out of range.
 *
 * Port note: five parameters (class 2): [ESP+0x14] is the Eval helper's skip-count out-pointer; the
 * same holds for every ScriptCmd_ handler below that takes pSkipCount after pResult. */
short ScriptCmd_ActorXyz(int *pCtx, int pArgBlock, int *pTable,
                        short *pResult, int *pSkipCount) /* 0x0040f260 */
{
    short sVar;
    short  status;
    short argResult1[4];   /* first eval: descriptor name */
    short argResult2[4];   /* second eval: field index */
    short dataBuffer[80];  /* 160-byte result from LookupActorEntry */
    short packed[2];       /* case 4: two words packed into one int */

    sVar = EvalTwoScriptArgs(pCtx, pArgBlock, pTable,
                       (undefined4 *)argResult1, (undefined4 *)argResult2, pSkipCount);
    status = (uint)(unsigned short)sVar;
    if (sVar != 0)
        return status;

    status = TakeScriptString(argResult1, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)dataBuffer);
    if (status != 0)
        return status;

    if (argResult2[0] != 4)
        return 0x0e;

    *pResult = 4;   /* type: int */
    pResult[3] = 0;
    switch (*(int *)(argResult2 + 1) - 1) {
    case 0:
        *(int *)(pResult + 1) = (int)dataBuffer[13];
        return 0;
    case 1:
        *(int *)(pResult + 1) = (int)dataBuffer[14];
        return 0;
    case 2:
        *(int *)(pResult + 1) = (int)dataBuffer[15];
        return 0;
    case 3:
        /* the exe builds the pair in a short[2] local and reads it back as a dword */
        packed[1] = dataBuffer[13];
        packed[0] = dataBuffer[14];
        *(int *)(pResult + 1) = *(int *)packed;
        return 0;
    default:
        return 0x0e;
    }
}


/* Script command pointinactor (opcode 20039): an actor and a point (type 4, packed); returns a
 * bool, whether the point hits the actor's drawn image. CalcActorRenderParams gives the rect
 * (PointInSceneRect), then the frame is attached and probed with ProbeSpriteHotspotStripHit (a 2-D
 * sprite) or ProbeScaledHotspotStripHit. */
short __cdecl ScriptCmd_PointInActor(int *pCtx, int pArgBlock, int *pTable,
                                                  short *pResult, int *pSkipCount) /* 0x0040F380 */
{
    short           sVar;
    short           a1[4], a2[4];
    short           row160[80];
    short           ccMix[18];   /* the 36-byte calc output (the exe's frame) */
    int             packedPt;
    int            *hChild;

    /* The exe passes pSkipCount straight through and fills the default
     * result (type 2, value 0) only after the argument checks; the port
     * filled it first and copied the skip count only on success. */
    sVar = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSkipCount);
    if (sVar != 0)
        return sVar;
    sVar = TakeScriptString(a1, gScratchStr);
    if (sVar != 0)
        return sVar;
    sVar = LookupActorEntry((byte *)gScratchStr, (int *)row160);
    if (sVar != 0)
        return sVar;
    if (a2[0] != 4)
        return 0xE;

    *pResult              = 2;
    packedPt              = *(int *)((char *)a2 + 2);
    *(int *)(pResult + 1) = 0;
    pResult[3]            = 0;

    if (CalcActorRenderParams(row160, ccMix) != 0
        && PointInSceneRect(*(ScenePointYX *)&packedPt, ccMix + 8)) {

    /* Everything below comes from the CALC OUTPUT (ccMix = Ghidra local_cc), not the row:
     * Attach([calc+0x18] VALUE, [calc+0x1c], &hChild) is the parent and per-angle frame
     * selector the calc just chose (the pair StampCalcScratchRows uses as row[6]/row[7]); the
     * branch is on the short at calc+0x0c (ccMix[6]: < 0 = 2-D sprite); bias = ccMix[5]; scaled
     * size = the dword at calc+0x20. The port passed the ADDRESS of row+2 as the parent (a
     * crash) and read bias, branch and size from the wrong places. */
    sVar = AttachRenderChild(*(int **)((byte *)ccMix + 0x18),
                             *(unsigned int *)((byte *)ccMix + 0x1c), (int **)&hChild);
    if (sVar != 0)
        FatalError(sVar, 0xfaa);
    if (ccMix[6] < 0)
        *(int *)(pResult + 1) = ProbeSpriteHotspotStripHit(hChild, *(int *)ccMix, ccMix[5], *(ScenePointYX *)&packedPt);
    else
        *(int *)(pResult + 1) = ProbeScaledHotspotStripHit(hChild, *(int *)ccMix, ccMix[5],
                                    *(ScenePointYX *)((byte *)ccMix + 0x20), *(ScenePointYX *)&packedPt);
    sVar = DetachRenderChild(*(int *)((byte *)ccMix + 0x18), *(unsigned int *)((byte *)ccMix + 0x1c));
    if (sVar != 0)
        FatalError(sVar, 0xfab);
    }
    return 0;
}


/* Statement half of actorxy (opcode 16004): an actor and a 2-D position. Stores x at +0x16 and y at
 * +0x14 (shorts), clears the is-3D word (+0x12), sets the short at +0x28 to -1 if it is not
 * negative, and drops the actor's walk entry. */
short __cdecl
ActionCmd_ActorXy(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040F510 */
{
    short  u;
    short  name[4];
    short  xArg[4];
    short  yArg[4];
    int    named[0x28];

    u = EvalThreeScriptArgs(pCtx, pArgBlock, pTable, (int *)name, (int *)xArg, (int *)yArg, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(name, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    if (xArg[0] != 4)
        return 0x0E;
    if (yArg[0] != 4)
        return 0x0E;
    RemoveWalkEntry((byte *)gScratchStr);
    DF_TRACE("actorxy: name='%.*s' x=%d y=%d", (int)((byte *)gScratchStr)[0],
             (const char *)gScratchStr + 1, *(int *)(xArg + 1), *(int *)(yArg + 1));
    /* 0x0040f5b5..0x0040f5d9: x -> +0x16, y -> +0x14 (shorts); if the short at
     * +0x28 is >= 0 it is set to -1; +0x12 = 0. The old `named + 0x78` test was
     * Ghidra's local_78 NAME transcribed as an offset. */
    *(short *)((char *)named + 0x16) = (short)*(int *)(xArg + 1);
    *(short *)((char *)named + 0x14) = (short)*(int *)(yArg + 1);
    if (*(short *)((char *)named + 0x28) >= 0)
        *(short *)((char *)named + 0x28) = -1;
    *(short *)((char *)named + 0x12) = 0;
    WriteBackActorEntry(named);
    return 0;
}


/* Value half of actorxy (opcode 16004): an actor and an index 1-3. 1 and 2 return the upper and
 * lower word of the position at row +0x14 (sign-extended), 3 the whole 32-bit value. Error 0x0e on
 * a type mismatch or an index out of range. */
short ScriptCmd_ActorXy(int *pCtx, int pArgBlock, int *pTable,
                       short *pResult, int *pSkipCount) /* 0x0040f600 */
{
    short sVar;
    short  status;
    short argResult1[4];   /* first eval: descriptor name */
    short argResult2[4];   /* second eval: sub-index */
    short dataBuffer[80];  /* 160-byte result from LookupActorEntry */

    sVar = EvalTwoScriptArgs(pCtx, pArgBlock, pTable,
                       (undefined4 *)argResult1, (undefined4 *)argResult2, pSkipCount);
    status = (uint)(unsigned short)sVar;
    if (sVar != 0)
        return status;

    status = TakeScriptString(argResult1, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)dataBuffer);
    if (status != 0)
        return status;

    if (argResult2[0] != 4)
        return 0x0e;

    *pResult = 4;   /* type: int */
    pResult[3] = 0;

    switch (*(int *)(argResult2 + 1)) {   /* a compare chain on one load: a switch */
    case 1:
        *(int *)(pResult + 1) = (int)dataBuffer[11];
        return 0;
    case 2:
        *(int *)(pResult + 1) = (int)dataBuffer[10];
        return 0;
    case 3:
        *(int *)(pResult + 1) = *(int *)(dataBuffer + 10);
        return 0;
    }
    return 0x0e;
}


/* Statement half of actoris3d (opcode 16005): a bool stored in the actor's is-3D word (+0x12). If
 * it is 0, the short at +0x28 is forced to -1 unless it is already negative. */
short __cdecl
ActionCmd_ActorIs3d(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x0040F6F0 */
{
    short  s1;
    short   u;
    int    named[0x28], skip, second;
    short  a1[4], a2[4];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    if (a2[0] != 2)     /* the flag's tag (0x0040f758 CMP [ESP],2); a1 is the descriptor, type 3 */
        return 0x0E;
    /* The flag is STORED at row+0x12 (0x0040f76f MOV [ESP+0x22],AX), the is-3D
     * word ActionCmd_ActorXy clears; Ghidra's decompile drops that store. Then,
     * when it is 0, the short at row+0x28 (local_78; +0x78 was the local's NAME
     * as an offset) is forced to -1 unless already negative. */
    *(short *)((char *)named + 0x12) = (short)*(int *)&a2[1];
    if (*(short *)((char *)named + 0x12) == 0 && *(short *)((char *)named + 0x28) >= 0)
        *(short *)((char *)named + 0x28) = -1;
    WriteBackActorEntry(named);
    return 0;
}


/* Value half of actoris3d (opcode 16005): returns the short at row +0x12 as a type-2 result. */
short ScriptCmd_ActorIs3d(int *pCtx, short *pArgBlock, int *pTable,  /* 0x0040f7a0 */
                                short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];    /* 8-byte typed result from ProcessScriptBlock */
    short dataBuffer[80];  /* 160-byte result from LookupActorEntry */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)dataBuffer);
    if (status != 0)
        return status;

    *pResult = 2;   /* type: short */
    *(int *)(pResult + 1) = (int)dataBuffer[9];
    pResult[3] = 0;
    return 0;
}


/* Not a handler: called only from ActionCmd_OpenCastFile. Finds the cast row for pNode, runs
 * "<cast>", opencast() as a Cast Message (RunCastMessage), then "<actor>", openactor() for every
 * actor of that cast. The twin of CompleteSetInit and CompleteStageInit. */
short __cdecl CompleteCastOpen(int pNode)  /* 0x0040F830 */
{
  typedef struct { int d[40]; } Rec160;   /* one slot-0 row, copied whole (rep movsd, ecx = 0x28) */
  int ownerKey;
  short status;
  int i;
  int rowOffset;
  /* LookupCastEntryByNode always writes the full 28-byte cast record (entry[0..2] header +
   * entry[3..6] Pascal name), as at every other entryBuf[7] / slotEntry[7] site. Split into
   * local_bc[3] + local_b0[16] the write overflowed 16 bytes into whatever came next:
   * confirmed live, it fed the following AppendPascalString a garbage source pointer and
   * crashed one frame later in RunCastMessage (class 3). */
  int slotEntry [7];
  /* One 160-byte actor row copied whole (40 DWORDs) from gActorTable: +2 owning node, +80
   * Pascal name. It was split into two locals 80 bytes apart (ebp-0xa0 / ebp-0x50), the same
   * bug as the cast record above. */
  Rec160 entryRec;

  ownerKey = *(int *)(pNode + 0x530);
  status = LookupCastEntryByNode(pNode,slotEntry);
  if (status != 0)
    return status;
  CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
  AppendPascalString((byte *)&slotEntry[3],&gScratchStr);
  AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteCommaOpencastCall), &gScratchStr);
  status = RunCastMessage(&gScratchStr);
  if (status != 0)
    return status;
  for (i = 0, rowOffset = 0; i < gActorCount; rowOffset += 0xa0, i++) {
    entryRec = *(Rec160 *)(rowOffset + GetResDataPtr(gActorTable));
    UnlockResObject(gActorTable);
    if (*(int *)((*(uint32_t *)((char *)&entryRec + 2)) + 0x530) == ownerKey) {
      CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
      AppendPascalString((byte *)&entryRec + 80,&gScratchStr);
      AppendPascalString((byte *)UpdatePascalLength(&kStrQuoteCommaOpenactorCall), &gScratchStr);
      status = RunActorMessage(&gScratchStr);
      if (status != 0)
        return status;
    }
  }
  return 0;
}


/* Script command closecastfile (opcode 12014). Evaluates one argument, finds the cast
 * (LookupCastEntryByName), pre-shuts it down with CastPreShutdownStep, resolves its render node
 * (FindRenderNodeByContextValue; returns early if there is none) and removes every actor row whose
 * node at +2 is that cast (TeardownActorEntry, compacting gActorTable in place). Then it removes the
 * cast row by moving the last row into its position. */
short __cdecl ActionCmd_CloseCastFile(int pCtx, short *pArgBlock,  /* 0x0040f990 */
                                       int *pTable, int *pSubSkip)
{
    /* The slot-0 row (0xA0 bytes) and slot-1 row (0x1C) are copied as whole
     * structs: rep movsd with ecx = 0x28 / 7. */
    typedef struct { int d[40]; } Rec160;
    typedef struct { int d[7]; } Rec28;
    Rec160 *pRow;          /* ground truth puVar3 -- the row's own slot */
    int   channelEntry;
    int   slot1Idx;
    int   slotEntry[7];    /* 7-int slot 1 entry from LookupCastEntryByName */
    short argResult[4];
    Rec160 entryBuf;       /* copy of one slot 0 entry */
    short  status;
    Rec160 *rows;
    int   iEntry;
    int   ctxValue;
    Rec28 *p1;

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupCastEntryByName(gScratchStr, slotEntry);
    if (status != 0)
        return status;

    /* The exe reads +0x530 BEFORE the shutdown (iVar2 = *(int *)(local_c4[0] + 0x530); then
     * CastPreShutdownStep): the shutdown releases the entry, so reading through the handle
     * afterwards is a use-after-release, the lookups below run on a stale context value and
     * unbalance the pack refcount: FatalError 104, "Bad pack use count at line %d". */
    ctxValue = *(int *)(slotEntry[0] + 0x530);

    status = CastPreShutdownStep(slotEntry[0]);
    if (status != 0)
        return status;

    /* Resolve channel node; 0 = not found -- nothing to unregister */
    if (FindRenderNodeByContextValue(ctxValue, &channelEntry) == 0)
        return 0;

    status = LookupCastEntryByNode(channelEntry, slotEntry);
    if (status != 0)
        return status;

    status = FindCastEntryIndexByNode(slotEntry[0], &slot1Idx);
    if (status != 0)
        return status;

    rows = (Rec160 *)GetResDataPtr(gActorTable);
    for (iEntry = 0; iEntry < gActorCount; iEntry++)
    {
        /* pRowSlot is the exe's puVar3, computed from the ORIGINAL index and still pointing at
         * this row after the decrement below: the compaction writes into it. Recomputing the
         * destination from iEntry after `iEntry--` put the last row in the PREVIOUS slot and
         * left the removed row in place, so the iEntry--/iEntry++ pair revisited it and tore it
         * down twice (two DetachRenderChild for one attach: "Bad pack use count at line 4012",
         * seen leaving the purser corridor and in the lift). */
        pRow = &rows[iEntry];
        entryBuf = *pRow;

        /* Match: field at byte offset +2 of entry == slotEntry[0] */
        if (*(int *)((char *)&entryBuf + 2) == slotEntry[0])
        {
            TeardownActorEntry((int)&entryBuf);
            iEntry--;
            gActorCount--;

            *pRow = rows[gActorCount];
        }
    }

    UnlockResObject(gActorTable);
    ResizeResObject(gActorTable, (unsigned int)(gActorCount * 0xa0));

    CloseCastNode(slotEntry);

    gCastCount--;
    p1 = (Rec28 *)GetResDataPtr(gCastTable);
    p1[slot1Idx] = p1[gCastCount];

    UnlockResObject(gCastTable);
    ResizeResObject(gCastTable, (unsigned int)(gCastCount * 0x1c));
    return 0;
}



/* Unregisters the actor's method and property entries for its name (+0x50), unwarms its render
 * children and detaches its cast child (+2 / +10). */
void __cdecl TeardownActorEntry(int pEntry)  /* 0x0040fba0 */
{
    byte *pName = (byte *)(pEntry + 0x50);

    RemoveWalkEntry(pName);
    RemoveLoopEntry((byte *)UpdatePascalLength((char *)kStrActor), pName);
    UnwarmActorRenderChildren((int *)pEntry);
    /* "Bad pack use count" (FatalError 104 / 0xfac) means this detach took a pack use count
     * below zero: the child was detached more often than it was attached. The trace names the
     * row and the child. */
    DF_TRACE("slot0Teardown: row='%.*s' parent=%p child=%u",
             (int)*(byte *)(pEntry + 0x50), (const char *)(pEntry + 0x51),
             (void *)(intptr_t)*(int *)(pEntry + 2),
             (unsigned)*(int *)(pEntry + 10));
    {
        short u = DetachRenderChild(*(int *)(pEntry + 2),
                                         (unsigned int)*(int *)(pEntry + 10));
        if (u != 0)
            FatalError(u, 0xFAC);
    }
}


/* Detaches child 0 of the cast's render node and registers the node for asynchronous finalisation
 * (DeferRenderNodeClose). */
void __cdecl CloseCastNode(int *pNodeRef)  /* 0x0040fc00 */
{
    short  u;
    short s1;

    u = DetachRenderChild(*pNodeRef, 0u);
    if (u != 0)
        FatalError(u, 0xFAD);
    s1 = DeferRenderNodeClose(*pNodeRef);
    if (s1 != 0)
        FatalError(s1, 0xFAE);
}


/* Runs `"<actor>", closeactor()` for each actor of the cast (RunActorMessage), then `"<cast>",
 * closecast()` (RunCastMessage): the twin of SetPreShutdownStep and StagePreShutdownStep. It finds
 * the cast's actors by the +0x530 key of pNode and resolves the render node with
 * FindRenderNodeByContextValue. */
short __cdecl CastPreShutdownStep(int pNode)  /* 0x0040fc50 */
{
    /* One actor row copied whole (rep movsd, ecx = 0x28). Byte-exact with a `short` return type
     * (the exe returns ax with no widening); df_protos.h still declares it `uint`, so the
     * returns below sign-extend until that prototype changes. */
    typedef struct { int d[40]; } Rec160;
    int     matchKey;
    int     i;
    short   st;
    int     channelVal;
    int     slot1Entry[7];
    Rec160  row;

    matchKey = *(int *)(pNode + 0x530);
    for (i = 0; i < gActorCount; i++) {
        row = ((Rec160 *)GetResDataPtr((int *)gActorTable))[i];
        UnlockResObject((int *)gActorTable);
        if (*(int *)(*(int *)((byte *)&row + 2) + 0x530) == matchKey) {
            CopyPascalString((byte *)UpdatePascalLength((char *)&kStrQuote), &gScratchStr);
            AppendPascalString((byte *)&row + 0x50, &gScratchStr);
            AppendPascalString((byte *)UpdatePascalLength((char  *)kStrQuoteCommaCloseactorCall), &gScratchStr);
            st = RunActorMessage(&gScratchStr);
            if (st != 0)
                return st;
        }
    }
    st = FindRenderNodeByContextValue(matchKey, &channelVal);
    if (st == 0)
        return 0;
    st = LookupCastEntryByNode(channelVal, slot1Entry);
    if (st != 0)
        return st;
    CopyPascalString((byte *)UpdatePascalLength((char *)&kStrQuote), &gScratchStr);
    AppendPascalString((byte *)&slot1Entry[3], &gScratchStr);
    AppendPascalString((byte *)UpdatePascalLength((char *)kStrQuoteCommaClosecastCall), &gScratchStr);
    return RunCastMessage(&gScratchStr);
}


/* Script command actorscript (opcode 12015). Evaluates one argument, looks the actor up
 * (LookupActorEntry), builds "Actor Script: <name>" (kStrActorScriptColon) in gScratchStr2 and
 * finalises it with ClearScriptResult. */
short __cdecl ActionCmd_ActorScript(int pCtx, short *pArgBlock,  /* 0x0040fdd0 */
                                           int *pTable, int *pSubSkip)
{
    short   status;
    byte  *pbVar2;
    byte  *pbVar3;
    short  argResult[4];
    int    entryBuf[40];    /* 0xA0-byte scratch for entry data */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry(gScratchStr, entryBuf);
    if (status != 0)
        return status;

    CopyPascalString((byte *)UpdatePascalLength((char *)kStrActorScriptColon), gScratchStr2);
    AppendPascalString(gScratchStr, gScratchStr2);
    /* 0x0040fe5a: loads at [esp+0x12]/[esp+0x1e] with 8 bytes still pushed, so
     * frame +0xa/+0x16, and the entry sits at frame +8: entry +2 and +0xe */
    ClearScriptResult(*(int *)((char *)entryBuf + 2), *(int *)((char *)entryBuf + 0xe),
                         gScratchStr2, 0);
    return 0;
}


/* Script command castscript (opcode 12047). Evaluates one argument, checks the cast exists
 * (LookupCastEntryByName), builds "Cast Script: <name>" in gScratchStr2 and finalises it with
 * ClearScriptResult. Unlike BuildPuppetScriptPath it has no gPuppet transition guard. */
short __cdecl ActionCmd_CastScript(int pCtx, short *pArgBlock,  /* 0x0040fe80 */
                                           int *pTable, int *pSubSkip)
{
    short  status;
    int   entryBuf[7];     /* slot-1 entry output -- validated but not used further */
    short args[4];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)args, pSubSkip);
    if (status == 0) {
        status = TakeScriptString(args, gScratchStr);
        if (status == 0) {
            status = LookupCastEntryByName(gScratchStr, entryBuf);
            if (status == 0) {
                CopyPascalString((byte *)UpdatePascalLength(kStrCastScriptColon),
                                 gScratchStr2);
                AppendPascalString(gScratchStr, gScratchStr2);
                ClearScriptResult(entryBuf[0], entryBuf[2], gScratchStr2, 0);   /* 0x0040ff01 */
                return 0;
            }
        }
    }
    return status;
}


/* Tokenizes a script string and runs it through sendtoactor in a frame named "Actor Message" /
 * "System: ": the actor flavour of the four message runners (TickMethodEntry case 1). It resolves a
 * Pascal heap path with WaitHeapThenResolveMethodPath, runs ActionCmd_SendToActor on the bytecode,
 * then finalises and uninstalls the heap handle. */
short __cdecl RunActorMessage(byte *pPascalDesc)  /* 0x0040ff30 */
{
    short           st;
    int            *h;
    int             subSkip;
    int             posErr;
    int             scriptData;
    /* One MethodCallFrame (0x4e bytes), not an opaque header: 0x0040ff30 sets isLast = 1, the
     * saved word at +2 to -1, pData (+0x14) to the script data and the three names at +0x1e /
     * +0x2e / +0x3e before passing it as the context to ActionCmd_SendToActor. The port had
     * written the -1 at +4 and left the data pointer and the names zeroed, so the callee's
     * method lookup saw empty names. */
    MethodCallFrame f;

    st = WaitHeapThenResolveMethodPath(pPascalDesc + 1,
                                       (uint)(unsigned char)pPascalDesc[0],
                                       &h, &posErr);
    if (st != 0)
        return st;

    scriptData   = GetResDataPtr(h);
    f.pData      = (void *)scriptData;
    f.isLast     = 1;
    f.saved0x14c = 0xffffffff;
    CopyPascalString((byte *)UpdatePascalLength(kStrActorMessage), f.buf0);
    CopyPascalString((byte *)UpdatePascalLength(kStrActorMessage), f.buf2);
    CopyPascalString((byte *)UpdatePascalLength(&kStrSystemColon), f.buf1);   /* "System: " */
    st = ActionCmd_SendToActor((int)&f, scriptData, NULL, &subSkip);
    UnlockResObject(h);
    FreeResObject(h);
    return st;
}


/* Script command sendtoactor (opcode 12016): a thin wrapper over ScriptCmd_SendToActorFx with a
 * NULL result pointer; returns that body's status. */
short __cdecl ActionCmd_SendToActor(int pCtx, int pArgBlock, int *pTable,  /* 0x00410010 */
                             int *pSubSkip)
{
    return ScriptCmd_SendToActorFx((int *)pCtx, pArgBlock, pTable, 0, pSubSkip);
}


/* Body of sendtoactor (opcodes 12016 / 20084): one typed argument and a 0xfb4 block, run through
 * EvalMethodCall with three method frames: the actor ("Actor Script: ", owner and selector from
 * the actor row), its cast ("Cast Script: ", selector from the cast row) and the bootfile
 * (gBootNode, selector 2). */
short __cdecl ScriptCmd_SendToActorFx(int *pCtx, int pArgBlock,  /* 0x00410030 */
                                                       int *pTable, void *pResult,
                                                       int *pSubSkip)
{
    int                start;           /* the parameter walks; this keeps its start */
    short              sVar;
    int                skipCount;       /* both evaluators' skip count (one slot) */
    short              args[4];
    /* MethodCallFrame[3], not three MethodCallContext locals: both are 0x4e bytes, but
     * MethodCallContext has dataPtr at +0x10 and pMethodBlock at +0x14 where the real layout
     * (df_types.h) has pOwner at +0x10 and pData at +0x14. ExecuteMethodBody reads +0x14 as
     * bytecode, so fed a MethodCallContext it walked string data hunting for an 0xfa1 and
     * crashed (eax=0x747363, ASCII "cst", from the gang.cst descriptor). EvalMethodCall also
     * walks the chain by raw adjacency (+0x4e): the original's frames sit at local_1a8 /
     * local_15a / local_10c, exactly 0x4e apart (class 3). The exe does not zero the frames.
     * Each frame is filled through a pointer, as in ScriptCmd_SendToBootFx (byte-exact). */
    MethodCallFrame    f[3];
    MethodCallFrame   *pf;
    int                slotRow[7];
    int                namedEntry[40];
    int               *h;

    start = pArgBlock;
    sVar = ProcessScriptBlock((int)pCtx, (short *)pArgBlock, pTable, (int *)args, &skipCount);
    if (sVar != 0)
        return sVar;

    pArgBlock += skipCount * 8;
    if (*(short *)pArgBlock != 0xfb4)
        return 0x1c;
    pArgBlock += 8;

    sVar = TakeScriptString(args, gScratchStr);
    if (sVar != 0) {
        DF_TRACE("!! SendToActorFx: TakeScriptString failed err=%d "
                 "(argtype=%d)", (int)sVar, (int)args[0]);
        return sVar;
    }

    DF_TRACE("SendToActorFx: actor name len=%d '%.*s'",
             (int)gScratchStr[0], (int)gScratchStr[0],
             (char *)&gScratchStr[1]);

    sVar = LookupActorEntry(gScratchStr, namedEntry);
    if (sVar != 0) {
        DF_TRACE("!! SendToActorFx: LookupActorEntry('%.*s') failed err=%d",
                 (int)gScratchStr[0], (char *)&gScratchStr[1],
                 (int)sVar);
        return sVar;
    }

    /* Field offsets in the two output records follow the original's frame: LookupActorEntry
     * fills local_a0 (160 bytes) and LookupCastEntryByNode local_bc (28 bytes). Ghidra's
     * `local_a0._2_4_` is the 4 bytes at offset +2, an UNALIGNED read the original relies on;
     * reading it as namedEntry[2] (offset +8) gave a non-handle (0x000205ef) and every lookup
     * failed with err=10. */
    #define ACTOR_OWNER(rec)  (*(int *)((byte *)(rec) + 2))    /* local_a0._2_4_ */
    #define ACTOR_SEL(rec)    (*(uint *)((byte *)(rec) + 14))  /* local_92 */
    #define ACTOR_NAME(rec)   ((byte *)(rec) + 80)             /* local_50 */

    sVar = LookupCastEntryByNode(ACTOR_OWNER(namedEntry), slotRow);
    if (sVar != 0) {
        DF_TRACE("!! SendToActorFx: LookupCastEntryByNode(%08x) failed err=%d",
                 ACTOR_OWNER(namedEntry), (int)sVar);
        return sVar;
    }

    /* Frame 0 -- original's local_1a8 block */
    pf = &f[0];
    pf->pOwner     = (int *)ACTOR_OWNER(namedEntry);             /* local_198 */
    pf->selector   = ACTOR_SEL(namedEntry);                      /* local_1a0 = local_92 */
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector); /* local_18e */
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];                 /* local_1a6 */
    pf->pData      = (void *)GetResDataPtr(h);                   /* local_194 */
    CopyPascalString((byte *)UpdatePascalLength(kStrActorScriptColon), pf->buf1);  /* local_17a */
    CopyPascalString(ACTOR_NAME(namedEntry), pf->buf0);          /* local_18a */
    CopyPascalString(ACTOR_NAME(namedEntry), pf->buf2);          /* local_16a */
    pf->isLast     = 0;

    /* Frame 1 -- original's local_15a block */
    pf = &f[1];
    pf->pOwner     = (int *)ACTOR_OWNER(namedEntry);             /* local_14a */
    pf->selector   = (unsigned int)slotRow[2];                   /* local_152 = local_b4 */
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector); /* local_140 */
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];                 /* local_158 */
    pf->pData      = (void *)GetResDataPtr(h);                   /* local_146 */
    CopyPascalString((byte *)UpdatePascalLength(kStrCastScriptColon), pf->buf1);  /* local_12c */
    CopyPascalString((byte *)&slotRow[3], pf->buf0);             /* local_13c <- local_b0 */
    CopyPascalString(ACTOR_NAME(namedEntry), pf->buf2);          /* local_11c */
    pf->isLast     = 0;

    /* Frame 2 -- original's local_10c block; isLast terminates the chain */
    pf = &f[2];
    pf->pOwner     = (int *)gBootNode;                        /* local_fc */
    pf->selector   = 2u;                                         /* local_104 */
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, 2u);  /* local_f2 */
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];                 /* local_10a */
    pf->pData      = (void *)GetResDataPtr(h);                   /* local_f8 */
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), pf->buf1);  /* local_de */
    CopyPascalString((byte *)UpdatePascalLength(kStrPostScript), pf->buf0);  /* local_ee */
    CopyPascalString(ACTOR_NAME(namedEntry), pf->buf2);          /* local_ce */
    pf->isLast     = 1;

    #undef ACTOR_OWNER
    #undef ACTOR_SEL
    #undef ACTOR_NAME

    sVar = EvalMethodCall((short *)f, (void *)pCtx, (short *)pArgBlock, pTable, pResult, &skipCount);

    UnlockResObject((int *)f[0].hMethod);
    ReleaseMethodHandle((int)f[0].hMethod);
    UnlockResObject((int *)f[1].hMethod);
    ReleaseMethodHandle((int)f[1].hMethod);
    UnlockResObject((int *)f[2].hMethod);
    ReleaseMethodHandle((int)f[2].hMethod);

    pArgBlock += skipCount * 8;
    *pSubSkip = (pArgBlock - start) >> 3;
    return sVar;
}


/* Same shape as RunActorMessage, with sendtocast and a frame named "Cast Message": compiles the
 * text, builds the MethodCallFrame around it and hands it to ActionCmd_SendToCast. It was the
 * model for 0x004183b0 / 0x00417f80. */
short __cdecl RunCastMessage(byte *pCmd)  /* 0x00410360 */
{
    int            *ph;
    int             skipOut;
    int             posErr;
    MethodCallFrame frame;
    short            status;
    short           sendStatus;
    int             rowBase;

    status = WaitHeapThenResolveMethodPath(pCmd + 1, (uint)*pCmd, &ph, &posErr);
    if (status != 0)
        return status;

    rowBase          = GetResDataPtr(ph);
    frame.isLast     = 1;
    frame.saved0x14c = 0xffffffffu;
    frame.pData      = (void *)rowBase;

    CopyPascalString((byte *)UpdatePascalLength(kStrCastMessage), frame.buf0);
    CopyPascalString((byte *)UpdatePascalLength(kStrCastMessage), frame.buf2);
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), frame.buf1);

    sendStatus = (short)ActionCmd_SendToCast((int)&frame, rowBase, (int *)0, &skipOut);
    UnlockResObject(ph);
    FreeResObject(ph);
    return sendStatus;
}


/* Script command sendtocast (opcode 12048): a thin wrapper over ScriptCmd_SendToCastFx with a NULL
 * result pointer. */
short __cdecl ActionCmd_SendToCast(int pCtx, int pArgBlock,  /* 0x00410440 */
                                         int *pTable, int *pSubSkip)
{
    return ScriptCmd_SendToCastFx((int *)pCtx, pArgBlock,
                                pTable, NULL, pSubSkip);
}


/* Body of sendtocast (opcodes 12048 / 20087). The same as ScriptCmd_SendToPuppetFx, but the target
 * is resolved with LookupCastEntryByName: the cast row supplies the context (entryBuf[0]) and the
 * method selector (entryBuf[2]). The script block is [args...] 0xfb4 [sub-instructions...]; it
 * runs a "Cast Script: " frame and a "System: " / "Post Script" frame through EvalMethodCall.
 * pResult goes to EvalMethodCall (the wrapper passes NULL). */
short __cdecl ScriptCmd_SendToCastFx(int *pCtx, int pArgBlock,  /* 0x00410460 */
                                    int *pTable, void *pResult,
                                    int *pSubSkip)
{
    int            start;          /* the parameter walks; this keeps its start */
    short          status;
    int            skip;           /* both evaluators' skip count (one slot) */
    short          args[4];
    int            entryBuf[7];    /* output of LookupCastEntryByName:
                                    *   entryBuf[0] = context pointer
                                    *   entryBuf[1] = ?
                                    *   entryBuf[2] = method selector
                                    *   entryBuf[3..6] = Pascal name (16 bytes) */
    /* Contiguous: EvalMethodCall walks frame[0] -> frame[1] by adding 0x4e. Built as ~14
     * separate locals the gap between the frames was up to the compiler, so the chain step and
     * every pOwner / pData read landed in unrelated stack memory, which fed ExecuteMethodBody a
     * garbage pData and crashed it on the very first opencast() click. The two frames sit at
     * ebp-0x9c and ebp-0x4e, exactly 0x4e apart; entryBuf occupies ebp-0xac..ebp-0x9c, ending
     * where frame[0] begins (class 3). */
    MethodCallFrame f[2];
    int           *h1, *h2;
    MethodCallFrame *pf;   /* frames filled through a pointer, as ScriptCmd_SendToBootFx */

    start = pArgBlock;
    status = ProcessScriptBlock((int)pCtx, (short *)pArgBlock, pTable,
                               (int *)args, &skip);
    if (status != 0)
        return status;

    pArgBlock += skip * 8;
    if (*(short *)pArgBlock != 0xfb4)   /* expect 0xfb4 separator */
        return 0x1c;
    pArgBlock += 8;                     /* advance past 8-byte separator */

    status = TakeScriptString(args, gScratchStr);
    if (status != 0)
        return status;

    status = LookupCastEntryByName(gScratchStr, entryBuf);
    if (status != 0)
        return status;

    /* Set up primary context from slot-1 entry; owner and selector are
     * passed from the frame fields (0x004104f5..0x00410507). */
    pf = &f[0];
    pf->pOwner     = (int *)entryBuf[0];
    pf->selector   = (uint32_t)entryBuf[2];
    pf->hMethod    = h1 = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h1);
    CopyPascalString((byte *)UpdatePascalLength(kStrCastScriptColon), pf->buf1); /* "Cast Script: " */
    CopyPascalString((byte *)&entryBuf[3], pf->buf0);                    /* cast entry name */
    CopyPascalString((byte *)&entryBuf[3], pf->buf2);                    /* cast entry name (dup) */
    pf->isLast = 0;

    pf = &f[1];
    pf->pOwner     = (int *)gBootNode;
    pf->selector   = 2u;
    pf->hMethod    = h2 = AcquireMethodHandle((int *)pf->pOwner, 2u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h2);
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), pf->buf1); /* "System: " */
    CopyPascalString((byte *)UpdatePascalLength(kStrPostScript), pf->buf0); /* "Post Script" */
    CopyPascalString((byte *)&entryBuf[3], pf->buf2);                    /* cast entry name */
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


/* Script command countactors (opcode 20012): returns gActorCount as a type-4 int. */
short ScriptCmd_CountActors(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x004106A0 */
{
    *pSkipCount = 0;
    *(int *)(pResult + 1) = gActorCount;
    *pResult = 4;   /* type: int */
    pResult[3] = 0;
    return 0;
}


/* Script command countcasts (opcode 20029): returns gCastCount as a type-4 int. */
short __cdecl ScriptCmd_CountCasts(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x004106D0 */
{
    *pSkipCount = 0;
    *(int *)(pResult + 1) = gCastCount;
    *pResult = 4;
    pResult[3] = 0;
    return 0;
}


/* Script command indextoactor (opcode 20013): one int argument, a 1-based index, checked against
 * gActorCount. Copies that actor row, writes the cast descriptor into the shared result slot
 * (gScriptResult) and returns the actor's own name through AllocScriptString; the two are
 * separate answers. Error 0x0e on a type mismatch, 0x0a on a range error. */
short ScriptCmd_IndexToActor(int *pCtx, short *pArgBlock, int *pTable,
                              short *pResult, int *pSkipCount) /* 0x00410700 */
{
    /* the 160-byte slot-0 row, copied as one object (rep movsd, ecx = 0x28) */
    typedef struct { int w[0x28]; } Slot0Row;
    short    status;
    /* One typed value, the 7-int slot-1 record and the row copy -- each filled
     * as ONE region by its callee (port-bug-classes.md #3). */
    short    argResult[4];   /* [0]=type, [1..2]=value */
    int      entryBuf[7];    /* descriptor at &entryBuf[3] */
    Slot0Row entryRec;       /* owning handle at +2, name at +80 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;
    if (argResult[0] != 4)
        return 0x0e;
    if (*(int *)(argResult + 1) < 1 || gActorCount < *(int *)(argResult + 1))
        return 10;

    /* the product is a pointer before the -0xA0 is applied: the exe keeps the
     * displacement in the lea instead of folding it into the multiply */
    entryRec = *(Slot0Row *)((char *)(*(int *)(argResult + 1) * 0xA0) - 0xA0 + GetResDataPtr(gActorTable));
    UnlockResObject(gActorTable);

    DF_TRACE("IndexToActor: idx=%d/%d rec+0=%02x%02x rec+2=%08x "
             "name@+80 len=%d '%.*s'",
             *(int *)(argResult + 1), (int)gActorCount,
             ((byte *)&entryRec)[0], ((byte *)&entryRec)[1], *(int *)((byte *)&entryRec + 2),
             (int)((byte *)&entryRec)[80], (int)((byte *)&entryRec)[80], (char *)&entryRec + 81);

    status = LookupCastEntryByNode(*(int *)((byte *)&entryRec + 2), entryBuf);
    if (status != 0) {
        DF_TRACE("!! IndexToActor: LookupCastEntryByNode(%08x) failed err=%d",
                 *(int *)((byte *)&entryRec + 2), (int)status);
        return status;
    }

    DF_TRACE("IndexToActor: slot1 desc len=%d '%.*s'",
             (int)*(byte *)&entryBuf[3], (int)*(byte *)&entryBuf[3],
             (char *)&entryBuf[3] + 1);

    CopyPascalString((byte *)&entryBuf[3], gScriptResult);
    return AllocScriptString((byte *)&entryRec + 80, pResult);
}


/* Script command indextocast (opcode 20030): one int argument, a 1-based index into gCastTable;
 * returns the cast's name. */
short __cdecl ScriptCmd_IndexToCast(int *pCtx, short *pArgBlock, int *pTable,
                                               short *pResult, int *pSkipCount) /* 0x00410800 */
{
    /* the 28-byte table row, copied as one object (rep movsd, ecx = 7) */
    typedef struct { int w[7]; } Row28;
    short  st;
    short  argBuf[4];
    Row28  row;

    st = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)argBuf, pSkipCount);
    if (st != 0)
        return st;
    if (argBuf[0] != 4)
        return 0x0e;
    if (*(int *)(argBuf + 1) < 1 || gCastCount < *(int *)(argBuf + 1))
        return 10;
    /* the product is a pointer before the -0x1c: the exe keeps the displacement
     * in the lea instead of folding it into the multiply */
    row = *(Row28 *)(GetResDataPtr(gCastTable) + (char *)(*(int *)(argBuf + 1) * 0x1c) - 0x1c);
    UnlockResObject(gCastTable);
    return AllocScriptString((byte *)&row.w[3], pResult);
}


/* Statement half of actorturn (opcode 16036): stores the turn value (clamped to at least 1) in the
 * actor row's short at +0x20. */
short __cdecl
ActionCmd_ActorTurn(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x004108b0 */
{
    short  s1;
    short   u;
    int    named[0x28];
    short  a1[4], a2[4];
    int    skip;
    int    v;

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    if (a2[0] != 4)
        return 0x0e;
    if (*(int *)&a2[1] < 1)    /* clamped in place: the exe stores 1 back into the argument */
        *(int *)&a2[1] = 1;
    *(short *)((char *)named + 0x20) = (short)*(int *)&a2[1];   /* 0x004108b0: local_80 = ebp-0x80 = record +0x20 */
    /* ONE store, not two: Ghidra's `local_80` is the NAME of ebp-0x80, record offset +0x20 (the
     * store above); the extra store at +0x80 landed over the pose name's length byte and first
     * character ("penny1" read back as "pennyà" in the gym). */
    WriteBackActorEntry(named);
    return 0;
}


/* Value half of actorturn (opcode 16036): returns the short at row +0x20, sign-extended, as a
 * type-4 int. */
short ScriptCmd_ActorTurn(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00410960 */
                               short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    short dataBuffer[80];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)dataBuffer);
    if (status != 0)
        return status;

    *pResult = 4;   /* type: int */
    *(int *)(pResult + 1) = (int)dataBuffer[16];
    pResult[3] = 0;
    return 0;
}


/* Statement half of actorowner (opcode 16043): stores the second argument, a name of at most 15
 * characters (error 0x1a otherwise), as the Pascal string at row +144 (+0x90). */
short __cdecl
ActionCmd_ActorOwner(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x004109F0 */
{
    short  s1;
    short   u;
    short  a1[4], a2[4];
    int    skip;
    int    named[0x28];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    u = TakeScriptString(a2, (byte *)gScratchStr2);
    if (u != 0)
        return u;
    if (gScratchStr2[0] >= 0x10)
        return 0x1a;
    /* 16-byte Pascal tail at byte offset 144 (same tail as ScriptCmd_ActorOwner). */
    CopyPascalString((byte *)gScratchStr2, (byte *)named + 144);
    WriteBackActorEntry(named);
    return 0;
}


/* Statement half of actorvalue (opcode 16044): stores the second argument, an int, in the actor
 * record at +0x48 (a 32-bit store; ActorZclip's is a 16-bit store at +0x4c). */
short __cdecl
ActionCmd_ActorValue(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00410ab0 */
{
    short s1;
    short  u;
    short a1[4], a2[4];
    int   named[0x28];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    /* the type checked is the SECOND argument's (local_b0 at ESP'+0; 0x00410b18 CMP word ptr
     * [ESP],0x4), not the record's first field: checking named[0] failed 0x0e on every call,
     * since the record's first word is an actor field, never 4 */
    if (a2[0] != 4)
        return 0x0E;
    /* store the second argument's VALUE into the actor record at +0x48 (0x00410b2b..0x00410b33):
     * this is what "actorvalue" does, and it was missing entirely */
    *(int *)((byte *)named + 0x48) = *(int *)(a2 + 1);
    WriteBackActorEntry(named);
    return 0;
}


/* Statement half of actorzclip (opcode 16051): stores the second argument, an int, as a 16-bit
 * value in the actor record at +0x4c. Same shape as ActionCmd_ActorValue, but the store is 16-bit at
 * 0x00410bd3 (`MOV word ptr [ESP+0x5c],AX`) where ActorValue's is 32-bit; do not copy one onto the
 * other. */
short __cdecl ActionCmd_ActorZclip(int pCtx, int pArgBlock, int *pTable,
                                  int *pSubSkip)  /* 0x00410B50 */
{
  short u;
  short a1[4];              /* out1: local_a8 (ESP'+8) -- resolved to descriptor */
  short a2[4];              /* out2: local_b0 (ESP'+0) -- must be type 4 */
  int   named[40];          /* local_a0: 160-byte LookupActorEntry record */

  u = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
  if (u != 0)
    return u;
  u = TakeScriptString(a1, &gScratchStr);
  if (u != 0)
    return u;
  u = LookupActorEntry(&gScratchStr, named);
  if (u != 0)
    return u;
  if (a2[0] != 4)
    return 0xe;
  *(short *)((byte *)named + 0x4c) = (short)*(int *)(a2 + 1);
  WriteBackActorEntry(named);
  return 0;
}


/* Value half of actorowner (opcode 16043): returns the owner name, the Pascal string at row +144. */
short ScriptCmd_ActorOwner(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00410bf0 */
                                  short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    byte  entryRec[160];   /* Pascal string at byte offset 144 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)entryRec);
    if (status != 0)
        return status;

    return AllocScriptString(entryRec + 144, pResult);   /* its status is the result */
}


/* Value half of actorvalue (opcode 16044): returns the dword at row +72 as a type-4 int. */
short ScriptCmd_ActorValue(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00410c70 */
                                 short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    byte  entryRec[160];
    int   dwordField;      /* dword at byte offset 72 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)entryRec);
    if (status != 0)
        return status;

    dwordField = *(int *)(entryRec + 72);

    *(int *)(pResult + 1) = dwordField;
    *pResult = 4;   /* type: int */
    pResult[3] = 0;
    return 0;
}


/* Value half of actorzclip (opcode 16051): returns the short at row +76 as a type-4 int. */
short ScriptCmd_ActorZclip(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00410d00 */
                               short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    byte  entryRec[160];
    short intField;        /* short at byte offset 76 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupActorEntry((byte *)&gScratchStr, (int *)entryRec);
    if (status != 0)
        return status;

    intField = *(short *)(entryRec + 76);

    *pResult = 4;   /* type: int */
    *(int *)(pResult + 1) = (int)intField;
    pResult[3] = 0;
    return 0;
}


/* Finds the actor row named pName in gActorTable (name at +0x50) and copies its 160 bytes to
 * pDataOut, trying the last matching index (gNamedEntryCacheIndex) first. Returns 0, or 10 if there
 * is no such actor. Every actor handler starts with it. */
short LookupActorEntry(byte *pName, int *pDataOut)  /* 0x00410d90 */
{
    typedef struct { int d[40]; } NamedEntryRec;   /* 160 bytes: one rep movsd */
    char *pTable;
    char *pSlot;
    int   i;

    pTable = (char *)GetResDataPtr(gActorTable);

    /* Fast path: try last-matched index first */
    /* the exe re-reads the index after the compare: a volatile read */
    if (*(volatile int *)&gNamedEntryCacheIndex < gActorCount) {
        pSlot = pTable + gNamedEntryCacheIndex * 0xa0;
        if (PascalStringsEqualNoCase(pName, (byte *)(pSlot + 0x50)) != 0) {
            *(NamedEntryRec *)pDataOut = *(NamedEntryRec *)pSlot;
            UnlockResObject(gActorTable);
            return 0;
        }
    }

    i = 0;
    if (i < gActorCount) {
        do {
            if (PascalStringsEqualNoCase(pName, (byte *)(pTable + 0x50)) != 0) {
                gNamedEntryCacheIndex = i;
                *(NamedEntryRec *)pDataOut = *(NamedEntryRec *)pTable;
                UnlockResObject(gActorTable);
                return 0;
            }
            pTable += 0xa0;
            i++;
        } while (i < gActorCount);
    }

    UnlockResObject(gActorTable);
    return 10;   /* not found */
}


/* Writes the edited 160-byte copy back to the row cached by LookupActorEntry
 * (gNamedEntryCacheIndex), after checking the binding key (+0x2e) matches. Fatal 0xfaf if the
 * cached index is out of range, 0xfb0 on a key mismatch. */
void __cdecl WriteBackActorEntry(int *pEntryData)  /* 0x00410e70 */
{
    int  *pSlot;
    typedef struct { int d[40]; } NamedEntryRec;   /* 160 bytes: one rep movsd */

    /* one pointer: the table base is stepped in place (add edi,eax) */
    pSlot = (int *)GetResDataPtr(gActorTable);
    if (gNamedEntryCacheIndex >= gActorCount) {
        FatalError(0, 0xfaf);
    }
    pSlot += gNamedEntryCacheIndex * 0x28;
    if (*(int *)((char *)pSlot + 0x2e) != *(int *)((char *)pEntryData + 0x2e)) {
        FatalError(0, 0xfb0);
    }
    *(NamedEntryRec *)pSlot = *(NamedEntryRec *)pEntryData;
    UnlockResObject(gActorTable);
}


/* Finds the cast row in gCastTable whose name (entry[3], byte +12) equals pName, ignoring case, and
 * copies its 7 ints to pOut. The by-name counterpart of LookupCastEntryByNode. Returns 0, or 10 if
 * there is none. */
short __cdecl LookupCastEntryByName(byte *pName, int *pOut)  /* 0x00410ee0 */
{
    /* the 7-int entry as one object: `rep movsd`, ecx = 7, is a struct assignment.
     * Local on purpose: a file-scope typedef enters every later test file's symbol
     * table and moves the 1996 compiler's register allocation there. */
    typedef struct { int w[7]; } Slot3EntryRec;
    int *piEntry;
    int  i;

    piEntry = (int *)GetResDataPtr(gCastTable);
    for (i = 0; i < gCastCount; piEntry += 7, i++) {   /* the slot-3 lookup's shape */
        if (PascalStringsEqualNoCase(pName, (byte *)(piEntry + 3)) != 0) {
            *(Slot3EntryRec *)pOut = *(Slot3EntryRec *)piEntry;   /* the same 7-int record: rep movsd */
            UnlockResObject(gCastTable);
            return 0;
        }
    }
    UnlockResObject(gCastTable);
    return 10;   /* not found */
}


#undef aliasHandle
#undef aliasName
#undef aliasPad
#undef childId
#undef entryHandle

/* Linear scan of gCastTable (stride 7 ints) for the row whose node (entry[0]) is `handle`; the
 * index goes to *pIdxOut. */
short __cdecl FindCastEntryIndexByNode(int handle, int *pIdxOut) /* 0x00410f60 */
{
    int *pEnt;
    int i;

    pEnt = (int *)GetResDataPtr(gCastTable);
    i = 0;
    if (0 < gCastCount) {
        do {
            if (*pEnt == handle) {
                *pIdxOut = i;
                UnlockResObject(gCastTable);
                return 0;
            }
            pEnt += 7;
            i++;
        } while (i < gCastCount);
    }
    UnlockResObject(gCastTable);
    return 10u;
}


/* Finds the cast row whose first field (the cast's render node) equals entryValue and copies its 7
 * ints to pOut. Returns 0, or 10 if there is none. */
short __cdecl LookupCastEntryByNode(int entryValue, int *pOut)  /* 0x00410fc0 */
{
    typedef struct { int w[7]; } Slot1EntryRec;   /* copied whole: rep movsd, ecx = 7 */
    int *piEntry;
    int  i;

    piEntry = (int *)GetResDataPtr(gCastTable);
    for (i = 0; i < gCastCount; piEntry += 7, i++) {
        if (*piEntry == entryValue) {
            *(Slot1EntryRec *)pOut = *(Slot1EntryRec *)piEntry;
            UnlockResObject(gCastTable);
            return 0;
        }
    }
    UnlockResObject(gCastTable);
    return 10;
}


/* For every actor with a frame count whose set name (+0x60) is the current set and whose waypoint
 * name (+0x70) is in the set's waypoint table, copies the waypoint's xyz into the row
 * (+0x1a..+0x1e). Called after OpenSetNode so actors land in the new set. */
void __stdcall PlaceActorsAtWaypoints(void)  /* 0x00411030 */
{
  /* the 8-byte position record FindWaypointNodePosition (0x00409f70) fills --
   * [+0] tag, three shorts at +2/+4/+6 -- is ONE object: split, its +4/+6
   * halves once landed on the row pointer's slot (the stairs crash). It is
   * copied over row+0x18 whole and the first word put back. */
  typedef struct { int w[2]; } Pos8;
  int    i;
  byte  *row;
  short  keep;
  short  pos[4];

  row = (byte *)GetResDataPtr(gActorTable);
  DF_TRACE("slot0Rows: handle=%p data=%p rows=%d%s",
           (void *)gActorTable, (void *)row, (int)gActorCount,
           (((intptr_t)row & 1) || row == 0) ? "  <<< BAD DATA POINTER" : "");
  for (i = 0; i < gActorCount; row += 0xa0, i++) {
    if (*(short *)(row + 0x12) != 0
        && PascalStringsEqualNoCase(row + 0x60, &DAT_0048a7b4) != 0
        && FindWaypointNodePosition(row + 0x70, pos) != 0) {
      short *p18 = (short *)(row + 0x18);
      keep = *p18;
      *(Pos8 *)p18 = *(Pos8 *)pos;
      *p18 = keep;
    }
  }
  UnlockResObject(gActorTable);
}


/* Locks gActorTable and walks its gActorCount rows (0xa0 bytes each), calling
 * CalcActorRenderParams on each. Mode 0 snapshots the row; otherwise it advances the row's frame
 * counter, computes live, appends the 9 dwords to the scratch rows (AppendCalcScratchRow),
 * adds a dirty rect (AddDirtyRect) and commits the rects to the row (shorts +0x19..+0x23). */
void __cdecl WalkActorsCalcRender(short mode)  /* 0x004110c0 */
{
    typedef struct { short s[80]; } Slot0Row160;   /* copied whole: `rep movsd` 0x28 */
    typedef struct { short s[4]; } Rect8;
    int           idx;
    short        *psRow;
    short         u;
    Slot0Row160   rowCopy;
    /* calcOut is Ghidra's local_c4 (ebp-0xc4), 0x24 bytes (the frame is 0xc4 = 0x24 + the 0xa0
     * row copy); AppendCalcScratchRow takes its 9 dwords. Every other "local" in the ground
     * truth is a field of it: calcDirty = [4] (set by the calc when anything changed), calcAux
     * = [5], calcRectTL = [8..9], calcRectBR = [10..11], calcGeomWord = [14..15], calcSizePair
     * = [16..17]. The port had calcDirty as its own variable, so the dirty gate never fired
     * after the first frame and no changed sprite produced a transition rect (nothing
     * re-blitted) (class 3). */
    short         calcOut[18];
#define calcDirty (calcOut[4])
#define calcAux (calcOut[5])
#define calcRectTL (*(int *)(calcOut + 8))
#define calcRectBR (*(int *)(calcOut + 10))
#define calcGeomWord (*(int *)(calcOut + 14))
#define calcSizePair (*(int *)(calcOut + 16))

    psRow = (short *)(intptr_t)GetResDataPtr(gActorTable);
    DF_TRACE("walkSlot0: mode=%d rows=%d table=%p", (int)mode,
             (int)gActorCount, (void *)psRow);

    /* no NULL test on the table in the exe (the port had added one) */
    {
        for (idx = 0; idx < gActorCount; idx++) {
            if (mode != 0) {
                psRow[0x11]++;
                if (psRow[0x12] <= psRow[0x11])
                    psRow[0x11] = 0;

                u = CalcActorRenderParams(psRow, calcOut);
                DF_TRACE_RENDER("  slot0row[%d]: calc=%d", idx, (int)(short)u);
#if DF_TRACE_ENABLED
                /* the actor twin of the prop-row trace below: buttons live here (the hit-test
                 * names them, e.g. 'exit' over the enigma's OK box), and a button drawn by the
                 * original but missing here shows up as a stuck calc=0. Name at +0x50, aux at
                 * +0x60. Change-only, as above. */
                {
                    static signed char sPrev0[64];
                    int state0 = ((short)u != 0 ? 2 : 0) | (calcOut[4] != 0 ? 1 : 0);
                    if (idx < 64 && sPrev0[idx] != (signed char)state0) {
                        const byte *r0 = (const byte *)psRow;
                        sPrev0[idx] = (signed char)state0;
                        DF_TRACE("slot0row[%d] '%.*s' aux='%.*s' calc=%d dirty=%d vis=%d",
                                 idx, r0[0x50], (const char *)r0 + 0x51,
                                 r0[0x60], (const char *)r0 + 0x61,
                                 (int)(short)u, (int)calcOut[4], (int)psRow[0]);
                    }
                }
#endif
                if (u != 0) {
                    AppendCalcScratchRow((unsigned int *)calcOut);
                    if (calcDirty != 0) {
                        if (*(int *)(psRow + 0x19) != 0)
                            AddDirtyRect(psRow + 0x1b);
                        AddDirtyRect(calcOut + 8);
                        DF_TRACE("  dirtyRect: row %d rect=(%d,%d,%d,%d)", idx,
                                 (int)calcOut[8], (int)calcOut[9], (int)calcOut[10], (int)calcOut[11]);
                        *(Rect8 *)(psRow + 0x1b) = *(Rect8 *)(calcOut + 8);   /* calcRectTL/calcRectBR */
                        *(int *)(psRow + 0x19) = calcGeomWord;
                        *(int *)(psRow + 0x1f) = *(int *)(calcOut + 0);
                        *(int *)(psRow + 0x21) = calcSizePair;
                        psRow[0x23]            = calcAux;
                    }
                } else {
                    if (*(int *)(psRow + 0x19) != 0)
                        AddDirtyRect(psRow + 0x1b);
                    /* Ground truth clears shorts [0x19] and [0x1a] with ONE dword store (`mov
                     * dword ptr [ebp+0x32],0`); two 4-byte stores ran on into short [0x1b], the
                     * first word of the saved rect this row hands to AddDirtyRect next pass. */
                    *(int *)(psRow + 0x19) = 0;
                }
            } else {
                rowCopy = *(Slot0Row160 *)psRow;
                u = CalcActorRenderParams(rowCopy.s, calcOut);
                if (u != 0) {
                    calcDirty = 1;
                    AppendCalcScratchRow((unsigned int *)calcOut);
                }
            }
            psRow += 0x50;
        }
    }

    UnlockResObject(gActorTable);
#undef calcDirty
#undef calcAux
#undef calcRectTL
#undef calcRectBR
#undef calcGeomWord
#undef calcSizePair
}


/* Computes the render parameters of an actor row (pActor) into pCalc: the dirty flag (pCalc[4], 1 =
 * changed since the last call), the screen rect (pCalc[8..0xb] = y1, x1, y2, x2) and the frame
 * geometry. Two paths by pActor[9]: 0 is a sprite, which asks LookupSpriteFrameGeometryByAngle for
 * the frame geometry; non-zero is video, which matches the node name to the active AVI clip
 * (PascalStringsEqualNoCase), derives the frame from its playback position and calls the same
 * lookup with that frame. Returns 1 on success, 0 on failure.
 *
 * Port note: two arguments, (row, out): the exe reads them at [esp+4] and [esp+8] (0x00411218,
 * 0x00411376); Ghidra's three leading parameters were phantoms. frameGeom is ONE 44-byte record
 * (class 3): LookupSpriteFrameGeometryByAngle copies eleven dwords into it, so a 16-byte int[4]
 * overran and every rect was garbage. Shaped to the exe (the AVI path first, the cull
 * as one test, the point pair and size pair as the exe stores them). */
short CalcActorRenderParams(short *pActor, short *pCalc)  /* 0x00411210 */
{
    short         sVar4;
    short         dist;
    short         sVar11;
    short         extentA;
    short         extentB;
    short         relDist;  /* local_bc */
    short         angle;
    int           w;
    int           serial;
    short        *psVar1;
    short         camAngle;
    short         rowAngle;
    short         bearing;
    short         pt[2];
    int           h;
    short         clipRect[4];  /* local_ac */
    short         frameGeom[22];          /* 44 bytes @ ebp-0xa4, one geometry record */
    short         vp[60];                 /* 120 bytes @ ebp-0x78: viewport pair, then the size pair */
#define frameGeomHead  ((int *)frameGeom)      /* +0x00 */
#define frameMinA  (frameGeom[9])          /* +0x12 */
#define frameMinB  (frameGeom[10])         /* +0x14 */
#define frameMaxA  (frameGeom[11])         /* +0x16 */
#define frameMaxB  (frameGeom[12])         /* +0x18 */
#define frameOriginA  (frameGeom[13])         /* +0x1a */
#define frameOriginB  (frameGeom[14])         /* +0x1c */
#define frameScale  (frameGeom[21])         /* +0x2a */

    if (*pActor <= 0) {
        DF_TRACE_RENDER("    calcRes: bail row[0]=%d (<1, inactive)", (int)*pActor);
        return 0;
    }

    if (pActor[9] != 0) {
        /* AVI / video path */
        DF_TRACE("    calcRes: AVI path (row[9]=%d)", (int)pActor[9]);
        if ((gSet.active == 0) || (DAT_0048a77a == 0))
            return 0;
        if (PascalStringsEqualNoCase((byte *)(pActor + 0x30), &DAT_0048a7b4) == 0)
            return 0;

        camAngle = gCameraAngle;
        psVar1 = pActor + 0xc;
        rowAngle = *psVar1 - gCurrentDeg;
        /* 0x48a7ac is a field of the camera record (gSet[0x34]); passing the literal
         * dereferenced a 1996 address that is unmapped in this process. */
        bearing = camAngle - ComputeAngleBetweenPoints((int)psVar1, (int)(intptr_t)&gCameraAngle);
        angle  = (rowAngle + bearing) & 0xff;
        dist  = ProjectSetPointToScreen((int)psVar1, pt);
        relDist = dist - pActor[0x26];
        if (relDist < 0)
            relDist = 0;

        /* Near/far cull. An actor close to the camera has the SMALLEST dist, so a
         * wrong near limit drops exactly the foreground figures while distant ones
         * keep drawing -- the grand-staircase steward missing while someone at the
         * end of the corridor renders. */
        if (dist <= (short)(DAT_0048a78c / 4) || relDist > DAT_0048a790) {
            DF_TRACE("actorCull: CULL '%.*s' dist=%d near=%d d=%d far=%d (a78c=%d selfNear=%d)",
                     (int)*((byte *)pActor + 0x50), (const char *)pActor + 0x51,
                     (int)dist, (int)(short)(DAT_0048a78c / 4), (int)relDist,
                     (int)DAT_0048a790, (int)DAT_0048a78c, (int)pActor[0x26]);
            return 0;
        }
        DF_TRACE("actorCull: draw '%.*s' dist=%d near=%d far=%d d=%d",
                 (int)*((byte *)pActor + 0x50), (const char *)pActor + 0x51,
                 (int)dist, (int)(short)(DAT_0048a78c / 4), (int)DAT_0048a790, (int)relDist);

        DF_TRACE("    calcRes: AVI lookupGeom angle=%d (rowAngle=%d camAngle=%d bearing=%d)",
                 (int)angle, (int)rowAngle, (int)camAngle, (int)bearing);
        if (!LookupSpriteFrameGeometryByAngle((int)pActor, angle, frameGeomHead))
            return 0;

        /* The divisors below are the record's own extents. The exe divides by them
         * with no guard, so a degenerate frame is an integer divide-by-zero, not a
         * wrong picture. */
        DF_TRACE("    calcRes: geom dx=%d dy=%d dist=%d scale=%d%s",
                 (int)(short)(frameMaxB - frameMinB), (int)(short)(frameMaxA - frameMinA),
                 (int)dist, (int)frameScale,
                 ((short)(frameMaxA - frameMinA) == 0 || (short)(frameMaxB - frameMinB) == 0
                  || dist == 0) ? "  <<< ZERO DIVISOR" : "");

        angle = frameMaxA - frameMinA;
        sVar11 = (short)((*(int *)(pActor + 0x15) * (int)frameScale) / 1000);
        sVar4 = frameMaxB - frameMinB;
        extentB = (short)((sVar11 * sVar4) / (int)dist);
        h      = angle;
        extentA = (short)((sVar11 * angle) / (int)dist);
        sVar4 = (short)(((int)extentB * (int)frameOriginB) / sVar4);
        psVar1 = pCalc + 8;
        sVar11 = pt[0] - (short)(((int)extentA * (int)frameOriginA) / h);
        sVar4  = pt[1] - sVar4;
        psVar1[0]   = sVar11;
        psVar1[1]   = sVar4;
        psVar1[2]  = sVar11 + extentA;
        psVar1[3] = sVar4 + extentB;
        CopyTrigOverlayBufferAndViewportPair(vp, clipRect);
        if (IntersectSceneRects(clipRect, pCalc + 8, (int *)(pCalc + 8)) == 0)
            return 0;
        pCalc[6]      = dist;
        pCalc[7]      = relDist / DAT_0048a78c;
        *(int *)pCalc = *(int *)pt;
        pActor[0x14]   = dist;
    } else {
        /* Sprite/animation path */
        if (!LookupSpriteFrameGeometryByAngle((int)pActor, pActor[0xc], frameGeomHead)) {
            DF_TRACE("    calcRes: bail lookupGeom failed (angle=%d)", (int)pActor[0xc]);
            return 0;
        }
        extentA = frameMaxA - frameMinA;
        extentB = frameMaxB - frameMinB;
        sVar11 = pActor[10] - frameOriginA;
        pCalc[8]   = sVar11;
        sVar4  = pActor[0xb] - frameOriginB;
        pCalc[9]   = sVar4;
        pCalc[10]  = sVar11 + extentA;
        pCalc[0xb] = sVar4 + extentB;
        CopyTrigSceneBufferAndViewportPair(vp, clipRect);
        DF_TRACE("    calcRes: rect=(%d,%d,%d,%d) clip=(%d,%d,%d,%d)",
                 (int)pCalc[8], (int)pCalc[9], (int)pCalc[10], (int)pCalc[0xb],
                 (int)clipRect[0], (int)clipRect[1], (int)clipRect[2], (int)clipRect[3]);
        if (IntersectSceneRects(clipRect, pCalc + 8, (int *)(pCalc + 8)) == 0) {
            DF_TRACE("    calcRes: bail rect/clip do not intersect");
            return 0;
        }
        pCalc[6]      = pActor[0x14];
        *(int *)pCalc = *(int *)(pActor + 10);
    }

    /* The size pair goes out through the first dword of the viewport blob. */
    psVar1 = vp;
    vp[1] = extentB;
    vp[0] = extentA;
    *(int *)(pCalc + 0xc) = *(int *)(pActor + 1);       /* int at byte offset +2 */
    *(int *)(pCalc + 0xe) = frameGeomHead[0];
    serial = *(int *)(pActor + 0x17);
    pCalc[4] = 0;
    pCalc[5] = 0;
    *(int *)(pCalc + 2) = serial;
    *(int *)(pCalc + 0x10) = *(int *)psVar1;

    /* Dirty check: set pCalc[4]=1 if any cached value changed */
    if (*(int *)(pActor + 0x19) != frameGeomHead[0] ||     /* frame data at byte +0x32 */
        pCalc[1]    != pActor[0x20] ||
        pActor[0x1f] != *pCalc       ||
        pCalc[0x11] != pActor[0x22] ||
        pActor[0x21] != pCalc[0x10]       ||
        pActor[0x23] != 0) {
        pCalc[4] = 1;
    }
    return 1;
}

/* Finds, in the binding named by the actor's view (+0x80), the frame of the current pose (+0x22)
 * whose angle is nearest the requested one, and copies its 44-byte frame record to pFrameOut. The
 * twin of LookupStateSpriteFrameGeometryByAngle. Returns a short: nonzero when no frame was within
 * range (`(short)(best - 1000) != 0`). */
short __cdecl LookupSpriteFrameGeometryByAngle(int pActor, short angle, int *pFrameOut) /* 0x00411570 */
{
    /* short: the exe returns `(short)(best - 1000) != 0` in ax. Frame: the
     * target angle +0, best +2, childBase +4, rowWalk +8, childHandle +0xc;
     * the 44-byte frame record is copied out whole (`rep movsd`, ecx = 0xb). */
    typedef struct { int v[11]; } Frame44;
    short         target;
    short         bestDelta;
    int           childBase;
    int           rowWalk;
    int           childHandle;
    int           parentBase;
    unsigned int *pb;
    Frame44      *pf;
    short         d;
    short         err;
    int           bindIdx;

    bestDelta  = 1000;
    parentBase = GetResDataPtr(*(void **)(pActor + 6));
    bindIdx    = 0;
    pb         = (unsigned int *)(parentBase + 0x5e);
    if (*(int *)(parentBase + 0x5a) > 0) {
        do {
            if (PascalStringsEqualNoCase((byte *)(pb + 4), (byte *)(pActor + 0x80)) != 0) {
                err = AttachRenderChild(*(int **)(pActor + 2), *pb, &childHandle);
                if (err != 0)
                    FatalError(err, 0xfb1);
                childBase = GetResDataPtr((void *)(intptr_t)childHandle);
                rowWalk = 0;
                target = *(short *)(childBase + 0x2e + *(short *)(pActor + 0x22) * 2) - 1;
                pf = (Frame44 *)(childBase + 0x76);
                if (*(int *)(childBase + 0x72) > 0) {
                    do {
                        if (*(short *)((char *)pf + 8) == target) {
                            d = ComputeAngularDistance(*(short *)((char *)pf + 0x28), angle);
                            if (d < bestDelta) {
                                bestDelta = d;
                                *(Frame44 *)pFrameOut = *pf;
                                if (d <= 0)
                                    break;
                            }
                        }
                        pf++;
                        rowWalk++;
                    } while (rowWalk < *(int *)(childBase + 0x72));
                }
                UnlockResObject((void *)(intptr_t)childHandle);
                err = DetachRenderChild(*(int *)(pActor + 2), *pb);
                if (err != 0)
                    FatalError(err, 0xfb2);
                /* Ground truth (0x00411570 / 0x004190f0) leaves the binding loop as soon as a
                 * name matches: the match path jumps to the frame loop and then to the epilogue.
                 * Falling through let a LATER binding with the same name overwrite pFrameOut, so
                 * the sprite drew with another frame's hotspot in the wrong place. */
                break;
            }
            pb += 8;
            bindIdx++;
        } while (bindIdx < *(int *)(parentBase + 0x5a));
    }
    UnlockResObject(*(void **)(pActor + 6));
    return (short)(bestDelta - 1000) != 0;
}


/* Reads the ushort at +0x70 of the child resource childId (the frame count of a pose). The twin
 * ReadChildUshortAt0x70 has the same body with different FatalError ids: two copies in the exe, so
 * two copies here. */
unsigned short __cdecl ReadNamedRowChildUshort(int *pCtx, unsigned int childId)  /* 0x004116E0 */
{
    /* Same body as ReadChildUshortAt0x70 (0x00419260), other FatalError ids:
     * two copies in the exe, so two copies here. */
    short   err;
    int    *hChild;
    int     data;
    short    d2;
    unsigned short u;

    err  = AttachRenderChild(pCtx, childId, (int **)&hChild);
    if (err != 0)
        FatalError(err, 0xFB3);
    data = GetResDataPtr(hChild);
    u    = *(unsigned short *)(data + 0x70);
    UnlockResObject(hChild);
    d2 = DetachRenderChild((int)pCtx, childId);
    if (d2 != 0)
        FatalError(d2, 0xFB4);
    return u;
}


/* Finds the cast binding row whose name (+0x10) is the pose name pSecond and writes that pose's
 * frame count (ReadNamedRowChildUshort) into the actor's +0x24, the count the frame counter wraps
 * at. The one caller is ActionCmd_ActorPose. */
short __cdecl SetActorFrameCountForPose(int *pNamed, byte *pSecond) /* 0x00411760 */
{
    /* Ground truth (0x00411760): handle at BYTE +6, context at BYTE +2 -- the
     * twin of SetPropFrameCountForPose */
    int     i;
    byte   *p;
    int     base;

    base = GetResDataPtr(*(int **)((char *)pNamed + 6));
    p    = (byte *)(base + 0x5e);
    for (i = 0; i < *(int *)(base + 0x5a); p += 0x20, i++) {
        if (PascalStringsEqualNoCase(p + 0x10, pSecond) != 0) {
            *(ushort *)((char *)pNamed + 36) = ReadNamedRowChildUshort(*(int **)((char *)pNamed + 2), *(uint *)p);
            UnlockResObject(*(int **)((char *)pNamed + 6));
            return 0;
        }
    }
    UnlockResObject(*(int **)((char *)pNamed + 6));
    return 1;
}


/* Scans gActorTable for the row whose binding key (+0x2e) equals bindingKey and copies its name to
 * pDescOut. Returns 0 on a hit, 10 otherwise (a short, as the exe returns ax). */
short __cdecl GetActorNameByBindingKey(int bindingKey, byte *pDescOut) /* 0x004117F0 */
{
    /* The exe returns ax (`mov ax,0xa` / `xor ax,ax`): `short`, which its
     * df_protos.h prototype does not say yet. */
    byte  *pRow;
    int    i;

    pRow = (byte *)GetResDataPtr(gActorTable);
    for (i = 0; i < gActorCount; pRow += 0xa0, i++) {
        if (*(int *)(pRow + 0x2e) == bindingKey) {
            CopyPascalString(pRow + 0x50, pDescOut);
            UnlockResObject((int *)gActorTable);
            return 0;
        }
    }
    UnlockResObject((int *)gActorTable);
    return 10u;
}


/* Re-attaches the render children of an actor row: the parent node handle (+2) is used for every
 * attach and detach, with the scope handle at +6 and the child selector at +0xe. The twin of
 * ReattachStateRowRenderChildren. */
void __cdecl
ReattachSlot0RowRenderChildren(int *pRow)  /* 0x00411860 */
{
    /* Declared in this order for the exe's register allocation (byte-exact). */
    void  *scopeBase;
    int    hFrameChild;  /* local_4 */
    int    bindIdx;  /* local_8 */
    uint  *pBinding;  /* local_10 */
    int   *hViewChild;  /* local_14 */
    uint  *pFrame;
    int    viewData;
    int    frameIdx;
    short   detachStatus;
    short  attachStatus;

    /* Ground truth (0x00411860 / 0x004193e0) takes the row as a BYTE address: the parent node
     * handle at +2 (every attach and detach), the scope handle at +6, the child selector at
     * +0xe. The port declared the row `int *` and kept `pRow + 6` / `pRow + 2` bare, which
     * scaled them to bytes +24 and +8: the scope read at +24 was NULL in the gym (fatal 264 from
     * GetResDataPtr(0)) and every attach / detach used the wrong parent (class 3,
     * audit_mixed_ptr_arith.py). */
    scopeBase = GetResDataPtr((void *)*(int **)((char *)pRow + 6));
    attachStatus = AttachRenderChild(*(int **)((char *)pRow + 2), *(unsigned int *)((char *)pRow + 0x0E),
                             (int **)&hViewChild);
    if (attachStatus != 0)
        FatalError(attachStatus, (short)0xFB5);

    detachStatus = DetachRenderChild(*(int *)((char *)pRow + 2),
                                   *(unsigned int *)((char *)pRow + 0x0E));
    if (detachStatus != 0)
        FatalError(detachStatus, (short)0xFB6);

    bindIdx = 0;
    pBinding = (uint *)((char *)scopeBase + 0x5E);
    while (bindIdx < *(int *)((char *)scopeBase + 0x5A)) {
        attachStatus = AttachRenderChild(*(int **)((char *)pRow + 2), *pBinding, (int **)&hViewChild);
        if (attachStatus != 0)
            FatalError(attachStatus, (short)0xFB7);

        viewData = GetResDataPtr((void *)hViewChild);
        pFrame = (uint *)((char *)viewData + 0x76);
        for (frameIdx = 0; frameIdx < *(int *)((char *)viewData + 0x72); frameIdx++) {
            attachStatus = AttachRenderChild(*(int **)((char *)pRow + 2), *pFrame, (int **)&hFrameChild);
            if (attachStatus != 0)
                FatalError(attachStatus, (short)0xFB8);

            detachStatus = DetachRenderChild(*(int *)((char *)pRow + 2), *pFrame);
            if (detachStatus != 0)
                FatalError(detachStatus, (short)0xFB9);

            pFrame += 0xB;
        }
        UnlockResObject((int *)hViewChild);

        detachStatus = DetachRenderChild(*(int *)((char *)pRow + 2), *pBinding);
        if (detachStatus != 0)
            FatalError(detachStatus, (short)0xFBA);

        bindIdx++;
        pBinding += 8;
    }
    UnlockResObject(*(int **)((char *)pRow + 6));
}


/* Attaches every binding child and every frame child of the actor's cast resource and sets the
 * +0x4e "warm" flag. Called by actorlock / castlock. */
void __cdecl
WarmActorRenderChildren(int *pRow)  /* 0x004119e0 */
{
    int     local4, local10, local8;
    int     *local14, *pR = pRow;
    uint   *p12;
    uint   *pu4;
    int     viewData;
    int     frameIdx;
    short   s1, local16;

    if (*(short *)((char *)pR + 0x4E) != 0)
        return;
    s1 = ReserveHeapSpace(200000, &local16);
    if (s1 != 0)
        FatalError(s1, 0x0FBB);
    if (local16 == 0)
        return;

    /* the row is a BYTE address (parent at +2, scope at +6, selector at +0xe, flag at +0x4e),
     * as in ReattachSlot0RowRenderChildren */
    local10 = GetResDataPtr((void *)*(int **)((char *)pR + 6));
    s1     = AttachRenderChild(*(int **)((char *)pR + 2), *(uint *)((char *)pR + 0x0E), (int **)(void *)&local14);
    if (s1 != 0)
        FatalError(s1, 0x0FBC);
    local8 = 0;
    p12    = (uint *)((char *)local10 + 0x5E);
    while (local8 < *(int *)((char *)local10 + 0x5A)) {
        s1 = AttachRenderChild(*(int **)((char *)pR + 2), *p12, (int **)(void *)&local14);
        if (s1 != 0)
            FatalError(s1, 0x0FBD);
        viewData = GetResDataPtr((void *)local14);
        for (frameIdx = 0, pu4 = (uint *)((char *)viewData + 0x76);
             frameIdx < *(int *)((char *)viewData + 0x72);
             pu4 += 0xB, frameIdx++) {
            s1 = AttachRenderChild(*(int *)((char *)pR + 2), *pu4, (int)(void *)&local4); /* use temp int slot */
            if (s1 != 0)
                FatalError(s1, 0x0FBE);
        }
        UnlockResObject((int *)local14);
        local8++;
        p12 += 8;
    }
    UnlockResObject((int *)*(int **)((char *)pR + 6));
    *(short *)((char *)pR + 0x4E) = 1;
}


/* The inverse of WarmActorRenderChildren: detaches them all and clears +0x4e. Called by the lock
 * commands' off path and by TeardownActorEntry. */
void __cdecl
UnwarmActorRenderChildren(int *pRow) /* 0x00411B40 */
{
    int    scopeData;
    int    local4, local8;
    uint  *p10;
    int    frameIdx;
    uint  *pu5;
    int    viewData;
    short  u3;
    short  s1;

    if (*(short *)((char *)pRow + 0x4E) == 0)
        return;
    /* the row is a BYTE address, as in the Reattach / Warm twins (parent +2, scope +6, selector
     * +0xe, flag +0x4e) */
    scopeData = GetResDataPtr((void *)*(int **)((char *)pRow + 6));
    u3    = DetachRenderChild(*(int *)((char *)pRow + 2), *(uint *)((char *)pRow + 0x0E));
    if (u3 != 0)
        FatalError(u3, 0x0FBF);

    local4 = 0;
    p10    = (uint *)((char *)scopeData + 0x5E);
    while (local4 < *(int *)((char *)scopeData + 0x5A)) {
        s1 = AttachRenderChild(*(int **)((char *)pRow + 2), *p10, (int **)(void *)&local8);
        if (s1 != 0)
            FatalError(s1, 0x0FC0);
        viewData = GetResDataPtr((void *)local8);
        for (frameIdx = 0, pu5 = (uint *)((char *)viewData + 0x76);
             frameIdx < *(int *)((char *)viewData + 0x72);
             pu5 += 0xB, frameIdx++) {   /* 0x2C-byte stride, uint* steps of 0xB */
            u3 = DetachRenderChild(*(int *)((char *)pRow + 2), *pu5);
            if (u3 != 0)
                FatalError(u3, 0x0FC1);
        }
        UnlockResObject((int *)local8);
        u3 = DetachRenderChild(*(int *)((char *)pRow + 2), *p10);
        if (u3 != 0)
            FatalError(u3, 0x0FC2);
        u3 = DetachRenderChild(*(int *)((char *)pRow + 2), *p10);
        if (u3 != 0)
            FatalError(u3, 0x0FC3);
        local4++;
        p10 += 8;
    }
    UnlockResObject((int *)*(int **)((char *)pRow + 6));
    *(short *)((char *)pRow + 0x4E) = 0;
}


/* Looks the actor up, sets its view name (+0x80) and pose, finds the frame nearest the angle
 * (LookupSpriteFrameGeometryByAngle) and returns a CLONE of that frame's resource (AttachRenderChild,
 * InstallCLUT, detach). The caller hands the clone to a hotspot / image slot. */
uint32_t __cdecl CloneActorFrameImage(byte *pActorName, byte *pViewName, short angle, undefined2 animIdx)  /* 0x00411CB0 */
{
  short attachStatus;
  short status;
  int scopeData;
  uint32_t hClone;  /* local_d0 */
  /* The actor record. LookupActorEntry copies 0x28 ints = 160 bytes into it. Ground truth
   * 0x00411cb0 shows local_cc[6] / local_c6 / local_aa / local_4c[32], with offsets 0xcc minus
   * the local number: +0x06 the resource handle (it was read UNINITIALISED into GetResDataPtr),
   * +0x22 the caller's parameter (written to a dead local), +0x80 the view name, the field
   * LookupSpriteFrameGeometryByAngle compares against each binding (written to a scratch buffer,
   * so the requested view never took effect: the propview defect) (class 3). */
  undefined1 entryRec [160];

#define QSA_RESHANDLE  (*(int **)(entryRec + 0x06))
#define QSA_PARAM      (*(undefined2 *)(entryRec + 0x22))
#define QSA_VIEWNAME   ((byte *)(entryRec + 0x80))
  uint32_t frameGeom [11];  /* local_2c */

  status = LookupActorEntry(pActorName, (uint32_t *)entryRec);
  if (status != 0) {
    return 0;
  }
  if (*pViewName >= 0x10) {
    return 0;
  }
  CopyPascalString(pViewName, QSA_VIEWNAME);
  if (QSA_VIEWNAME[0] == 0) {
    scopeData = GetResDataPtr(QSA_RESHANDLE);
    if (*(int *)(scopeData + 0x5a) <= 0) {
      UnlockResObject(QSA_RESHANDLE);
      return 0;
    }
    CopyPascalString((byte *)(scopeData + 0x6e), QSA_VIEWNAME);
    UnlockResObject(QSA_RESHANDLE);
  }
  QSA_PARAM = animIdx;
  if ((short)LookupSpriteFrameGeometryByAngle((int)entryRec, angle, frameGeom) == 0) {
    return 0;
  }
  attachStatus = AttachRenderChild((uint32_t *)(*(uint32_t *)((char *)&entryRec + 2)), frameGeom[0], &hClone);
  if (attachStatus != 0) {
    FatalError(attachStatus, 0xfc4);
  }
  CloneResource(&hClone);
  status = DetachRenderChild((*(uint32_t *)((char *)&entryRec + 2)), frameGeom[0]);
  if (status != 0) {
    FatalError(status, 0xfc5);
  }
  return hClone;
}

#undef QSA_RESHANDLE
#undef QSA_PARAM
#undef QSA_VIEWNAME
