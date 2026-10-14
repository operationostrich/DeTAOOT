/* script_bindings.c -- TI.EXE 0x004420c0..0x00444c10: the three tables of timed script
 * bindings -- loops (makeloop), walks (walkto*, walkon*, turntodeg) and crickets (makecricket) --
 * with their tick, install, remove and pause functions, and the keyframe samplers they use. */

#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "df_protos.h"
#include "topics/renderer.h"
#include "topics/scriptprop.h"
#include "topics/scriptcmd.h"
#include "topics/strutil.h"

static short __cdecl BuildDeferredWalkEntry(byte *pSubA, byte *pSubB, short methodKind, int pNamed, unsigned short *pBuilt);


#include "topics/savestate.h"

/* Stores the addresses of the loop, walk and cricket tables in the three output words. */
void __cdecl GetScriptBindingTableBasePointers(uint32_t *pPropWords,  /* 0x004420C0 */
                                               uint32_t *pMethodWords,
                                               uint32_t *pObjectWords) /* 0x004420C0 */
{
    *pPropWords   = (uint32_t)(uintptr_t)&gLoopTable;
    *pMethodWords = (uint32_t)(uintptr_t)&gWalkTable;
    *pObjectWords = (uint32_t)(uintptr_t)&gCricketTable;
}


/* Ticks the three binding tables: the walk table (gWalkTable, 16 entries of 0x37 shorts:
 * TickWalkEntry), the cricket table (gCricketTable, 16 of 0x25 shorts: TickCricketEntry, then
 * AssignCricketSoundChannels if any entry queued a sound) and the loop table (gLoopTable, 32 of 0x15
 * shorts: TickLoopEntry). An entry whose tick returns a non-zero status is cleared and the status
 * returned at once; a walk entry also releases its path handle. Afterwards NetPollEvents runs and,
 * if it returns 0, PostTickSceneUpdate. */
short TickScriptBindings(void)  /* 0x004420e0 */
{
    short *pEntry;   /* declaration order sets the exe's register roles */
    short  status;
    short  i;

    i     = 0;
    pEntry = &gWalkTable;
    TeardownFinishedVoiceNodes();
    do {
        if (*pEntry != 0) {
            status = TickWalkEntry(pEntry);
            if (status != 0) {
                DF_TRACE("tickBind: METHOD slot %d -> err=%d", (int)i, (int)status);
                if (*(int **)(pEntry + 9) != NULL) {
                    FreeResObject(*(int **)(pEntry + 9));
                }
                *pEntry = 0;
                return status;
            }
        }
        i++;
        pEntry += 0x37;
    } while (i < 0x10);

    i      = 0;
    pEntry = &gCricketTable;
    /* gObjectUpdateCount is a word in the exe; every use in this file goes through the same cast. */
    *(short *)&gObjectUpdateCount = 0;
    do {
        pEntry[0x12] = -1;
        if (*pEntry != 0) {
            status = (short)TickCricketEntry(pEntry);
            if (status != 0) {
                DF_TRACE("tickBind: OBJECT slot %d -> err=%d", (int)i, (int)status);
                *pEntry = 0;
                return status;
            }
        }
        i++;
        pEntry += 0x25;
    } while (i < 0x10);

    if (*(short *)&gObjectUpdateCount > 0) {
        AssignCricketSoundChannels();
    }

    i      = 0;
    pEntry = &gLoopTable;
    do {
        if (*pEntry != 0) {
            status = TickLoopEntry(pEntry);
            if (status != 0) {
                DF_TRACE("tickBind: PROPERTY slot %d -> err=%d", (int)i, (int)status);
                *pEntry = 0;
                return status;
            }
        }
        i++;
        pEntry += 0x15;
    } while (i < 0x20);

    status = NetPollEvents();
    if (status == 0)
        status = PostTickSceneUpdate();
    DF_TRACE("tickBind: post-tick -> err=%d", (int)status);
    return status;
}


/* Removes from the walk table the entry named pNameDesc (the name is the Pascal string at byte 46),
 * or every entry when pNameDesc is the wildcard kStrAll; a removed entry's path handle is released. */
void __cdecl RemoveWalkEntry(byte *pNameDesc)  /* 0x004421d0 */
{
    byte  *pWildcard;
    short *psEntry;
    short  i;

    psEntry   = &gWalkTable;
    pWildcard = UpdatePascalLength(&kStrAll);

    if (PascalStringsEqualNoCase(pNameDesc, pWildcard) != 0) {
        i = 0x10;
        do {
            if (*psEntry != 0) {
                if (*(int **)(psEntry + 9) != (int *)0) {
                    FreeResObject(*(int **)(psEntry + 9));
                }
                *psEntry = 0;
            }
            psEntry += 0x37;     /* an entry is 0x6e bytes */
            i--;
        } while (i != 0);
        return;
    }

    i = 0;
    do {
        if (*psEntry != 0) {
            if (PascalStringsEqualNoCase(pNameDesc, (byte *)(psEntry + 0x17)) != 0) {
                if (*(int **)(psEntry + 9) != (int *)0) {
                    FreeResObject(*(int **)(psEntry + 9));
                }
                *psEntry = 0;
                return;
            }
        }
        i++;
        psEntry += 0x37;
    } while (i < 0x10);
}


/* Removes from the loop table the entries of type pTypeDesc (actor, prop, scene or flat) named
 * pNameDesc (the name is the Pascal string at byte 10), or all of that type when pNameDesc is the
 * wildcard kStrAll. */
void __cdecl RemoveLoopEntry(byte *pTypeDesc, byte *pNameDesc)  /* 0x00442270 */
{
    short   typeId;
    byte  *pWildcard;
    short  i;           /* before psEntry: the counter takes ebp, the name esi */
    short *psEntry;

    psEntry   = &gLoopTable;
    typeId = GetDescriptorTypeId(pTypeDesc);
    pWildcard = UpdatePascalLength(&kStrAll);

    if (PascalStringsEqualNoCase(pNameDesc, pWildcard) != 0) {
        i = 0x20;
        do {
            if (psEntry[2] == typeId) {
                *psEntry = 0;
            }
            psEntry += 0x15;     /* an entry is 0x2a bytes */
            i--;
        } while (i != 0);
        return;
    }

    i = 0;
    do {
        if ((*psEntry != 0) && (psEntry[2] == typeId)) {
            if (PascalStringsEqualNoCase(pNameDesc, (byte *)(psEntry + 5)) != 0) {
                *psEntry = 0;
                return;
            }
        }
        i++;
        psEntry += 0x15;
    } while (i < 0x20);
}


/* Removes from the cricket table the entry named pNameDesc (the name is the Pascal string at byte
 * 58), or every entry when pNameDesc is the wildcard kStrAll. */
void __cdecl RemoveCricketEntry(byte *pNameDesc)  /* 0x00442300 */
{
    byte  *pWildcard;
    short *psEntry;
    short  i;

    psEntry   = &gCricketTable;
    pWildcard = UpdatePascalLength(&kStrAll);

    if (PascalStringsEqualNoCase(pNameDesc, pWildcard) != 0) {
        i = 0x10;
        do {
            *psEntry = 0;
            psEntry += 0x25;     /* an entry is 0x4a bytes */
            i--;
        } while (i != 0);
        return;
    }

    i = 0;
    do {
        if (*psEntry != 0) {
            if (PascalStringsEqualNoCase(pNameDesc, (byte *)(psEntry + 0x1d)) != 0) {
                *psEntry = 0;
                return;
            }
        }
        i++;
        psEntry += 0x25;
    } while (i < 0x10);
}


/* 1 if an active walk entry is named pNameDesc (case-insensitive), else 0. */
short __cdecl IsWalkActive(byte *pNameDesc)  /* 0x00442370 */
{
    uint   equal;
    short  i;                           /* declared before the cursor: ebx holds the table, di the index */
    short *psEntry;

    psEntry = &gWalkTable;
    i       = 0;
    do {
        if (*psEntry != 0) {
            equal = PascalStringsEqualNoCase(pNameDesc, (byte *)(psEntry + 0x17));
            if ((short)equal != 0) {
                return 1;
            }
        }
        i++;
        psEntry += 0x37;
    } while (i < 0x10);
    return 0;
}


/* 1 if an active loop entry has type pTypeDesc and the name pNameDesc (case-insensitive), else 0. */
short __cdecl IsLoopActive(byte *pTypeDesc, byte *pNameDesc)  /* 0x004423c0 */
{
    short *psEntry;
    short  i;
    short  typeId;

    psEntry = &gLoopTable;
    typeId  = GetDescriptorTypeId(pTypeDesc);
    for (i = 0; i < 0x20; i++, psEntry += 0x15) {
        if (*psEntry != 0 && psEntry[2] == typeId) {
            if (PascalStringsEqualNoCase(pNameDesc, (byte *)(psEntry + 5)) != 0)
                return 1;
        }
    }
    return 0;
}


/* 1 if an active cricket entry is named pNameDesc (case-insensitive), else 0. */
short __cdecl IsCricketActive(byte *pNameDesc)  /* 0x00442420 */
{
    uint   equal;
    short  i;                           /* as IsWalkActive: index before cursor */
    short *psEntry;

    psEntry = &gCricketTable;
    i       = 0;
    do {
        if (*psEntry != 0) {
            equal = PascalStringsEqualNoCase(pNameDesc, (byte *)(psEntry + 0x1d));
            if ((short)equal != 0) {
                return 1;
            }
        }
        i++;
        psEntry += 0x25;
    } while (i < 0x10);
    return 0;
}


/* Copies to pOut458 the destination name (byte 62) of the active walk entry named pIn148, or the
 * string kStrNone if there is none. */
void __cdecl CopyWalkDestName(byte *pIn148, byte *pOut458) /* 0x00442470 */
{
    short slot;
    short *pEntry;
    byte  *pbVar2;

    slot = 0;
    pEntry = &gWalkTable;
    do {
        if (*pEntry != 0) {
            if (PascalStringsEqualNoCase(pIn148, (byte *)(pEntry + 0x17)) != 0) {
                CopyPascalString((byte *)(pEntry + 0x1f), pOut458);
                return;
            }
        }
        slot = (short)(slot + 1);
        pEntry += 0x37;
    } while (slot < 0x10);

    CopyPascalString((byte *)UpdatePascalLength(kStrNone), pOut458);   /* the destination is pushed before the lookup call */
}


/* makeloop (12005): makeloop(type, name, message, ticks). type is one of actor, prop, scene or flat
 * (GetDescriptorTypeId, else status 10); name and message are strings of at most 15 characters
 * (status 0x1a) and ticks an int (0x0e). Builds a loop entry whose countdown is ticks and installs
 * it (InstallLoopEntry). When the countdown expires TickLoopEntry sends `"name", message()` to the
 * object of that type. */
short __cdecl ActionCmd_MakeLoop(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x004424e0 */
{
    short  evalStatus;
    short   status;
    short   validResult;
    short  arg1Result[4];   /* result of evaluating 1st arg */
    short  arg2Result[4];   /* result of evaluating 2nd arg */
    short  arg3Result[4];   /* result of evaluating 3rd arg */
    /* EvalFourScriptArgs evaluates four arguments; the fourth (the tick count) is arg4Result (class 3). */
    short  arg4Result[4];   /* result of evaluating 4th arg; type tag at [0] */
    /* One 42-byte loop entry, not three locals: the names sit at +10 and +26 inside it (class 3). */
    short  paramStruct[21];

#define ML_NAME1 ((byte *)paramStruct + 10)
#define ML_NAME2 ((byte *)paramStruct + 26)

    evalStatus = EvalFourScriptArgs(pCtx, pArgBlock, pTable, arg1Result, arg2Result, arg3Result, arg4Result, pSubSkip);
    if (evalStatus != 0)
        return (uint)evalStatus;

    status = TakeScriptString(arg1Result, &gScratchStr);
    if (status != 0)
        return status;

    validResult = GetDescriptorTypeId(&gScratchStr);
    if (validResult == 0)
        return 10;

    status = TakeScriptString(arg2Result, &gScratchStr);
    if (status != 0)
        return status;

    /* the Pascal length byte, not the array address (class 1) */
    if (gScratchStr[0] >= 0x10)
        return 0x1a;   /* name too long */

    status = TakeScriptString(arg3Result, &gScratchStr2);
    if (status != 0)
        return status;

    if (gScratchStr2[0] >= 0x10)   /* length byte, not the array address */
        return 0x1a;   /* name too long */

    if (arg4Result[0] != 4)
        return 0x0e;

    paramStruct[0] = 1;
    paramStruct[1] = 0;
    paramStruct[2] = validResult;
    /* record+6 is the countdown that TickLoopEntry decrements; without this store a loop never fired */
    *(int *)(paramStruct + 3) = *(int *)(arg4Result + 1);
    CopyPascalString(&gScratchStr, ML_NAME1);
    CopyPascalString(&gScratchStr2, ML_NAME2);
    return InstallLoopEntry(paramStruct);
}


/* Upserts a 42-byte loop entry: removes any existing entry with the same type and name
 * (RemoveLoopEntry), then copies pEntry into the first free slot. Returns 0, or 0x23 if all 32 slots
 * are taken.
 *
 * Entry layout (shorts): [0] active, [1] pause count, [2] type id (GetDescriptorTypeId), [3..4]
 * countdown (int), [5..12] name (Pascal, byte 10), [13..20] message (Pascal, byte 26). */
short __cdecl InstallLoopEntry(short *pEntry)  /* 0x00442610 */
{
    typedef struct { short v[0x15]; } PropertyEntry42;   /* rep movsd 10 + movsw: one struct assignment */
    short  i;
    short *psSlot;

    psSlot = &gLoopTable;
    GetDescriptorById(pEntry[2], &gScratchStr);
    RemoveLoopEntry(&gScratchStr, (byte *)(pEntry + 5));
    i = 0;
    do {
        if (*psSlot == 0) {
            *(PropertyEntry42 *)psSlot = *(PropertyEntry42 *)pEntry;
            return 0;
        }
        i++;
        psSlot += 0x15;
    } while (i < 0x20);
    return 0x23;
}


/* Per-tick update of one loop entry (layout in InstallLoopEntry). While the pause count is non-zero
 * it does nothing. Otherwise the countdown (int at byte 6) is decremented; when it drops below 1 the
 * entry is deactivated and the command `"name", message()` is built in gScratchStr and sent by type:
 * 1 RunActorMessage, 2 RunPropMessage, 3 RunSceneMessage, 4 RunSendToFlatCommand. Returns that
 * call's status; for an unknown type the exe returns its uninitialised local, and so does the port. */
short TickLoopEntry(short *pEntry)  /* 0x00442670 */
{
    short  status;   /* local_2; uninitialised for an unknown type, as in the exe */

    if (pEntry[1] != 0)
        return 0;

    if (--*(int *)(pEntry + 3) > 0)
        return 0;
    *pEntry = 0;

    CopyPascalString(UpdatePascalLength(&kStrQuote), &gScratchStr);
    AppendPascalString((byte *)(pEntry + 5), &gScratchStr);
    AppendPascalString(UpdatePascalLength(&kStrQuoteCommaSpace), &gScratchStr);
    AppendPascalString((byte *)(pEntry + 0xd), &gScratchStr);
    AppendPascalString(UpdatePascalLength(&kStrEmptyCall), &gScratchStr);

    /* status-and-break, not return-per-case: the exe puts the default (the unset local) after the cases */
    switch (pEntry[2]) {
    case 1:  status = RunActorMessage(&gScratchStr); break;
    case 2:  status = RunPropMessage(&gScratchStr); break;
    case 3:  status = RunSceneMessage(&gScratchStr); break;
    case 4:  status = (short)RunSendToFlatCommand(&gScratchStr); break;
    }
    return status;
}


/* Returns the number of active loop entries of type pTypeDesc. */
short __cdecl CountLoopsByType(byte *pTypeDesc)  /* 0x00442790 */
{
    short  typeId;
    short *psEntry;
    short  count;
    short  i;

    psEntry = &gLoopTable;
    count   = 0;
    typeId  = GetDescriptorTypeId(pTypeDesc);
    i       = 0x20;
    do {
        if (*psEntry != 0 && psEntry[2] == typeId)
            count++;
        psEntry += 0x15;
        i--;
    } while (i != 0);
    return count;
}


/* Copies to pOut the name of the index-th (1-based) active loop entry of type pTypeDesc, or
 * kStrUnknown if there are fewer. */
void __cdecl GetLoopNameByIndex(short index, byte *pTypeDesc, byte *pOut)  /* 0x004427d0 */
{
    short  i;
    short *psEntry;
    short  typeId;

    psEntry = &gLoopTable;
    typeId  = GetDescriptorTypeId(pTypeDesc);
    i       = 0;
    do {
        if (*psEntry != 0 && psEntry[2] == typeId)
            index--;
        if (index <= 0) {
            CopyPascalString((byte *)(psEntry + 5), pOut);
            return;
        }
        i++;
        psEntry += 0x15;
    } while (i < 0x20);
    CopyPascalString((byte *)UpdatePascalLength(&kStrUnknown), pOut);
}


/* Returns the pause count (entry[1]) of the first active loop entry of type pTypeDesc named
 * pNameDesc (case-insensitive), or 0 if none. */
short __cdecl GetLoopPauseCount(byte *pTypeDesc, byte *pNameDesc)  /* 0x00442840 */
{
    short *psEntry;
    short  i;
    short  typeId;

    psEntry = &gLoopTable;
    typeId  = GetDescriptorTypeId(pTypeDesc);
    for (i = 0; i < 0x20; i++, psEntry += 0x15) {
        if (*psEntry != 0 && psEntry[2] == typeId) {
            if (PascalStringsEqualNoCase(pNameDesc, (byte *)(psEntry + 5)) != 0)
                return psEntry[1];
        }
    }
    return 0;
}


/* pauseloop's worker: changes the pause count (the counter at entry[1]) of the loop entries of type
 * pTypeDesc named pNameDesc, or of every slot when pNameDesc is the wildcard kStrAll (that branch
 * does not test the active flag; the by-name branch does). A non-zero resume argument adds one to
 * the pause count and zero takes one off (never below 0). It creates and names nothing. */
void __cdecl AdjustLoopPauseCount(byte *pTypeDesc, byte *pNameDesc,
                                             short resume)  /* 0x004428a0 */
{
    short *psSlot = &gLoopTable;
    short  i;
    short  typeId;

    typeId = GetDescriptorTypeId(pTypeDesc);

    if ((short)PascalStringsEqualNoCase(
            pNameDesc, (byte *)UpdatePascalLength((char *)kStrAll)) != 0) {
        for (i = 0x20; i != 0; i--) {
            if (psSlot[2] == (short)typeId) {
                if (resume != 0) {
                    psSlot[1]++;
                } else {
                    if (--psSlot[1] < 0)
                        psSlot[1] = 0;
                }
            }
            psSlot += 0x15;
        }
        return;
    }

    for (i = 0; i < 0x20; i++, psSlot += 0x15) {
        if (*psSlot != 0 && psSlot[2] == (short)typeId &&
            (short)PascalStringsEqualNoCase(pNameDesc, (byte *)(psSlot + 5)) != 0) {
            if (resume != 0) {
                psSlot[1]++;
            } else {
                if (--psSlot[1] < 0)
                    psSlot[1] = 0;
            }
            return;
        }
    }
}


/* turntodeg (12072): turntodeg(name, angle) looks up the named actor entry (LookupActorEntry),
 * builds a turn entry for the int angle (BuildTurnToDegEntry), writes the actor entry back and
 * installs the turn in the walk table (InstallWalkEntry). 0x0e if the angle is not an int. */
short __cdecl
ActionCmd_TurnToDeg(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00442970 */
{
    short  s1;
    short   u;
    short  arg1[4];
    short  arg2[4];   /* out2 = [ESP]: type checked there, payload at [ESP+2] */
    int    named[40];
    short  built[56];
    ushort nameTag;

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)arg1, (int *)arg2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(arg1, gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    if (arg2[0] != 4)
        return 0x0E;   /* the code is the low word (once put in the high half: status 0 = success) */
    nameTag = (unsigned short)*(int *)(arg2 + 1);
    u = BuildTurnToDegEntry(nameTag, (int)named, built);
    if (u != 0)
        return u;
    WriteBackActorEntry(named);   /* void: nothing to test */
    u = InstallWalkEntry((short *)built) & 0xFFFFU;
    return (u & 0xFFFFU);
}


/* Removes any walk entry with the same name as pNewEntry (the 0x6e-byte record), then copies it into
 * the first free slot of gWalkTable. If the table is full (error 7 in the low word) the entry's path
 * handle is released. */
short __cdecl InstallWalkEntry(short *pNewEntry)  /* 0x00442a30 */
{
    typedef struct { short v[0x37]; } MethodEntry110;    /* walk entry: rep movsd 0x1b + movsw, one struct assignment */
    short  i;
    short *psSlot;

    psSlot = &gWalkTable;
    RemoveWalkEntry((byte *)(pNewEntry + 0x17));
    i = 0;
    do {
        if (*psSlot == 0) {
            *(MethodEntry110 *)psSlot = *(MethodEntry110 *)pNewEntry;
            return 0;
        }
        i++;
        psSlot += 0x37;
    } while (i < 0x10);
    if (*(int **)(pNewEntry + 9) != NULL)          /* table full: release the entry's handle */
        FreeResObject(*(int **)(pNewEntry + 9));
    return 7;
}


/* walktostar (12006): walktostar(actor, star). The actor's entry is looked up and the star name
 * checked (at most 15 characters, 0x1a). If the actor's row names the same scene as DAT_0048a7b4
 * (gSet +0x3c) the walk is built at once (BuildWalkToStarEntry); otherwise a deferred entry
 * (BuildDeferredWalkEntry, kind 1) is installed, which TickWalkEntry completes when the scene
 * matches. */
short __cdecl ActionCmd_WalkToStar(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00442a90 */
{
    short  u;
    short  a1[4], a2[4];
    int    named[40];
    unsigned short built[0x38];   /* a 112-byte walk entry */

    u = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a1, gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    u = TakeScriptString(a2, gScratchStr);
    if (u != 0)
        return u;
    if (gScratchStr[0] >= 0x10)
        return 0x1A;
    gScratchStr2[0] = 0;
    /* the compared field is the row's +0x60 (the port had +0x50) */
    if (PascalStringsEqualNoCase((byte *)named + 0x60, (byte *)&DAT_0048a7b4)) {
        u = BuildWalkToStarEntry((byte *)&gScratchStr, (int)named, built);
        if (u != 0)
            return u;
    } else {
        u = BuildDeferredWalkEntry((byte *)gScratchStr, (byte *)gScratchStr2, 1, (int)named, built);
        if (u != 0)
            return u;
    }
    WriteBackActorEntry(named);
    return InstallWalkEntry((short *)built);
}


/* walktoxyz (12101): walktoxyz(actor, x, y, z) walks the actor to the point (the three ints, else
 * 0x0e): BuildWalkToXyzEntry, then InstallWalkEntry. */
short __cdecl
ActionCmd_WalkToXyz(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00442bc0 */
{
    short  s1;
    short   u;
    short  o1[4], o2[4], o3[4], o4[4];
    int    named[40];
    unsigned short built[0x38];

    s1 = EvalFourScriptArgs(pCtx, pArgBlock, pTable, o1, o2, o3, o4, pSubSkip);
    if (s1 != 0)
        return s1;
    u = TakeScriptString(o1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupActorEntry((byte *)gScratchStr, named);
    if (u != 0)
        return u;
    if (o2[0] != 4)
        return 0x0E;
    if (o3[0] != 4)
        return 0x0E;
    if (o4[0] != 4)
        return 0x0E;
    u = BuildWalkToXyzEntry(o2[1], o3[1], o4[1],
                                             (int)named, built);
    if (u != 0)
        return u;
    WriteBackActorEntry(named);
    return InstallWalkEntry((short *)built);
}


/* walkonpath (12100): walkonpath(actor, a, b): three script arguments, the first naming the actor's
 * row, the other two names of at most 15 characters (0x1a). If the row is in the scene of
 * DAT_0048a7b4 the entry is built at once (BuildWalkOnPathEntry), else deferred
 * (BuildDeferredWalkEntry, kind 3); then the row is written back and the entry installed. One body
 * per opcode, as in the exe (a shared static compiled to one). */
short __cdecl
ActionCmd_WalkOnPath(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00442cc0 */
{
    short   s1;
    short    u;
    short   t128[4], t120[4], t118[4];
    int     named[40];
    unsigned short built[56];   /* the exe's local_70 */

    s1 = EvalThreeScriptArgs(pCtx, pArgBlock, pTable, (int *)t128, (int *)t120, (int *)t118, pSubSkip);
    u  = (unsigned short)s1;
    if (s1 == 0
        && (u = TakeScriptString(t128, (byte *)gScratchStr), u == 0)
        && (u = LookupActorEntry((byte *)gScratchStr, named), u == 0)
        && (u = TakeScriptString(t120, (byte *)gScratchStr), u == 0)) {
        if (gScratchStr[0] >= 0x10)
            return CONCAT22((short)(u >> 16), 0x1A);
        u = TakeScriptString(t118, (byte *)gScratchStr2);
        if (u == 0) {
            if (gScratchStr2[0] >= 0x10)
                return CONCAT22((short)(u >> 16), 0x1A);
            if (PascalStringsEqualNoCase((byte *)named + 0x60, (byte *)&DAT_0048a7b4) != 0) {   /* the match case first */
                u = BuildWalkOnPathEntry((byte *)gScratchStr, (byte *)gScratchStr2, (int)named, built);
                if (u != 0)
                    return u;
            } else {
                u = BuildDeferredWalkEntry((byte *)gScratchStr, (byte *)gScratchStr2, 3, (int)named, built);
                if (u != 0)
                    return u;
            }
            WriteBackActorEntry(named);
            u = InstallWalkEntry((short *)built);
        }
    }
    return u;
}


/* walkonroad (12102): walkonroad(actor, a, b): three script arguments, the first naming the actor's
 * row, the other two names of at most 15 characters (0x1a). If the row is in the scene of
 * DAT_0048a7b4 the entry is built at once (BuildWalkOnRoadEntry), else deferred
 * (BuildDeferredWalkEntry, kind 4); then the row is written back and the entry installed. One body
 * per opcode, as in the exe (a shared static compiled to one). */
short __cdecl
ActionCmd_WalkOnRoad(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00442e20 */
{
    short   s1;
    short    u;
    short   t128[4], t120[4], t118[4];
    int     named[40];
    unsigned short built[56];   /* the exe's local_70 */

    s1 = EvalThreeScriptArgs(pCtx, pArgBlock, pTable, (int *)t128, (int *)t120, (int *)t118, pSubSkip);
    u  = (unsigned short)s1;
    if (s1 == 0
        && (u = TakeScriptString(t128, (byte *)gScratchStr), u == 0)
        && (u = LookupActorEntry((byte *)gScratchStr, named), u == 0)
        && (u = TakeScriptString(t120, (byte *)gScratchStr), u == 0)) {
        if (gScratchStr[0] >= 0x10)
            return CONCAT22((short)(u >> 16), 0x1A);
        u = TakeScriptString(t118, (byte *)gScratchStr2);
        if (u == 0) {
            if (gScratchStr2[0] >= 0x10)
                return CONCAT22((short)(u >> 16), 0x1A);
            if (PascalStringsEqualNoCase((byte *)named + 0x60, (byte *)&DAT_0048a7b4) != 0) {   /* the match case first */
                u = BuildWalkOnRoadEntry((byte *)gScratchStr, (byte *)gScratchStr2, (int)named, built);
                if (u != 0)
                    return u;
            } else {
                u = BuildDeferredWalkEntry((byte *)gScratchStr, (byte *)gScratchStr2, 4, (int)named, built);
                if (u != 0)
                    return u;
            }
            WriteBackActorEntry(named);
            u = InstallWalkEntry((short *)built);
        }
    }
    return u;
}


/* walkonframes (12103): walkonframes(actor, a, b): three script arguments, the first naming the
 * actor's row, the other two names of at most 15 characters (0x1a). If the row is in the scene of
 * DAT_0048a7b4 the entry is built at once (BuildWalkOnFramesEntry), else deferred
 * (BuildDeferredWalkEntry, kind 5); then the row is written back and the entry installed. One body
 * per opcode, as in the exe (a shared static compiled to one). */
short __cdecl
ActionCmd_WalkOnFrames(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00442f80 */
{
    short   s1;
    short    u;
    short   t128[4], t120[4], t118[4];
    int     named[40];
    unsigned short built[56];   /* the exe's local_70 */

    s1 = EvalThreeScriptArgs(pCtx, pArgBlock, pTable, (int *)t128, (int *)t120, (int *)t118, pSubSkip);
    u  = (unsigned short)s1;
    if (s1 == 0
        && (u = TakeScriptString(t128, (byte *)gScratchStr), u == 0)
        && (u = LookupActorEntry((byte *)gScratchStr, named), u == 0)
        && (u = TakeScriptString(t120, (byte *)gScratchStr), u == 0)) {
        if (gScratchStr[0] >= 0x10)
            return CONCAT22((short)(u >> 16), 0x1A);
        u = TakeScriptString(t118, (byte *)gScratchStr2);
        if (u == 0) {
            if (gScratchStr2[0] >= 0x10)
                return CONCAT22((short)(u >> 16), 0x1A);
            if (PascalStringsEqualNoCase((byte *)named + 0x60, (byte *)&DAT_0048a7b4) != 0) {   /* the match case first */
                u = BuildWalkOnFramesEntry((byte *)gScratchStr, (byte *)gScratchStr2, (int)named, built);
                if (u != 0)
                    return u;
            } else {
                u = BuildDeferredWalkEntry((byte *)gScratchStr, (byte *)gScratchStr2, 5, (int)named, built);
                if (u != 0)
                    return u;
            }
            WriteBackActorEntry(named);
            u = InstallWalkEntry((short *)built);
        }
    }
    return u;
}


/* Fills walk-table row pRow for a turn: active, type 0, target angle nameTag & 0xff in entry[4], the
 * actor entry's value pair (+0x18) as the current value, and its two names (+0x50, +0x70). Always
 * returns 0. */
short __cdecl
BuildTurnToDegEntry(ushort nameTag, int pNamed, short *pRow)  /* 0x004430E0 */
{
    typedef struct { int a, b; } Pair8;   /* the two dwords, copied as one 8-byte object */

    pRow[0] = 1;
    pRow[1] = 0;
    pRow[2] = 0;
    pRow[3] = 0;
    pRow[4] = nameTag & 0xff;
    *(Pair8 *)(pRow + 5) = *(Pair8 *)(pNamed + 0x18);
    *(uint32_t *)(pRow + 9) = 0;
    *(uint32_t *)(pRow + 0x15) = gNamedEntryCacheIndex;
    CopyPascalString((byte *)(pNamed + 0x50), (byte *)(pRow + 0x17));
    CopyPascalString((byte *)(pNamed + 0x70), (byte *)(pRow + 0x1f));
    return 0;
}


/* Fills a type 1 walk entry that moves the actor from its current position to the star pTargetName
 * (FindWaypointNodePosition): current and target positions, the delta vector and its length
 * (IntSqrt, at least 1). Returns 10 if the star is not found. */
short __cdecl BuildWalkToStarEntry(byte *pTargetName, int pNamed, unsigned short *pOutEntry)  /* 0x00443150 */
{
    typedef struct { int a, b; } PointPair8;   /* the 8-byte position copy (two dword movs) */
    int   dz, dx, dy;
    /* One 8-byte record: FindWaypointNodePosition writes the tag at +0 and x/y/z at +2/+4/+6, and
     * ComputeAngleBetweenPoints reads it; as separate locals the coordinates were never written (class 3). */
    short pos[4];

    if (FindWaypointNodePosition(pTargetName, pos) == 0)
        return 10;
    *pOutEntry   = 1;
    pOutEntry[1] = 0;
    pOutEntry[3] = 0;
    pOutEntry[2] = 1;
    pOutEntry[4] = ComputeAngleBetweenPoints(pNamed + 0x18, (int)(intptr_t)pos);
    *(PointPair8 *)(pOutEntry + 5) = *(PointPair8 *)(pNamed + 0x18);
    *(int *)(pOutEntry + 9)   = 0;
    *(int *)(pOutEntry + 0xb) = 0;
    dx = *(short *)(pNamed + 0x1a) - pos[1];
    dy = *(short *)(pNamed + 0x1c) - pos[2];
    dz = *(short *)(pNamed + 0x1e) - pos[3];
    *(int *)(pOutEntry + 0xd)  = dx;
    *(int *)(pOutEntry + 0xf)  = dy;
    *(int *)(pOutEntry + 0x11) = dz;
    *(int *)(pOutEntry + 0x13) = IntSqrt(dz * dz + dy * dy + dx * dx);
    if (*(int *)(pOutEntry + 0x13) <= 0)
        *(int *)(pOutEntry + 0x13) = 1;
    *(int *)(pOutEntry + 0x15) = gNamedEntryCacheIndex;
    CopyPascalString((byte *)(pNamed + 0x50), (byte *)(pOutEntry + 0x17));
    CopyPascalString(pTargetName, (byte *)(pOutEntry + 0x1f));
    CopyPascalString((byte *)UpdatePascalLength(&kStrWalktostar), (byte *)(pNamed + 0x70));
    return 0;
}


/* BuildWalkToStarEntry for an explicit point (x, y, z) instead of a named star. */
short __cdecl BuildWalkToXyzEntry(short x, short y, short z, int pNamed, undefined2 *pOutEntry)  /* 0x00443260 */
{
    /* One 8-byte point record (x/y/z at +2/+4/+6), as in BuildWalkToStarEntry; the port had three
     * separate shorts and the angle read garbage (class 3). */
    short pt[4];
    int   dz, dx, dy;
    typedef struct { int a, b; } PointPair8;   /* the 8-byte position copy (two dword movs) */

    pt[1] = x;
    pt[2] = y;
    pt[3] = z;
    *pOutEntry   = 1;
    pOutEntry[1] = 0;
    pOutEntry[3] = 0;
    pOutEntry[2] = 2;
    pOutEntry[4] = ComputeAngleBetweenPoints(pNamed + 0x18, (int)(intptr_t)pt);
    *(PointPair8 *)(pOutEntry + 5) = *(PointPair8 *)(pNamed + 0x18);
    *(int *)(pOutEntry + 9)   = 0;
    *(int *)(pOutEntry + 0xb) = 0;
    dx = *(short *)(pNamed + 0x1a) - pt[1];
    dy = *(short *)(pNamed + 0x1c) - pt[2];
    dz = *(short *)(pNamed + 0x1e) - pt[3];
    *(int *)(pOutEntry + 0xd)  = dx;
    *(int *)(pOutEntry + 0xf)  = dy;
    *(int *)(pOutEntry + 0x11) = dz;
    *(int *)(pOutEntry + 0x13) = IntSqrt(dy * dy + dx * dx + dz * dz);
    if (*(int *)(pOutEntry + 0x13) <= 0)
        *(int *)(pOutEntry + 0x13) = 1;
    *(int *)(pOutEntry + 0x15) = gNamedEntryCacheIndex;
    CopyPascalString((byte *)(pNamed + 0x50), (byte *)(pOutEntry + 0x17));
    CopyPascalString((byte *)UpdatePascalLength(&kStrCustom), (byte *)(pOutEntry + 0x1f));
    CopyPascalString((byte *)UpdatePascalLength(&kStrWalktoxyz), (byte *)(pNamed + 0x70));
    return 0;
}


/* Fills a type 3 walk entry that follows a path: the path handle comes from FindWaypointPathToNode,
 * or from FindWaypointPathBetween (two names) when pName1 is not the actor's own, and the start
 * angle from GetPathStartAngle. Returns 10 if no path is found. */
short __cdecl BuildWalkOnPathEntry(byte *pName1, byte *pName2, int pNamed,  /* 0x00443380 */
                                        unsigned short *pOutEntry)  /* 0x00443380 */
{
    typedef struct { int lo, hi; } Pair8;   /* the +0x18 point pair: one 8-byte copy */
    int        *hPath;  /* local_4 */

    if ((PascalStringsEqualNoCase(UpdatePascalLength(&kStrResume), pName1) == 0 ||
         FindWaypointPathToNode(pName2, pNamed, &hPath) == 0) &&
        FindWaypointPathBetween(pName1, pName2, &hPath) == 0)
        return 10;
    *pOutEntry            = 1;
    pOutEntry[1]          = 0;
    pOutEntry[2]          = 3;
    pOutEntry[3]          = 0;
    pOutEntry[4]          = GetPathStartAngle(hPath);
    *(Pair8 *)(pOutEntry + 5)        = *(Pair8 *)(pNamed + 0x18);
    *(undefined4 *)(pOutEntry + 0xb) = 0;
    *(int **)(pOutEntry + 9)         = hPath;
    *(undefined4 *)(pOutEntry + 0x15) = (undefined4)gNamedEntryCacheIndex;
    CopyPascalString((byte *)(pNamed + 0x50), (byte *)(pOutEntry + 0x17));
    CopyPascalString(pName2, (byte *)(pOutEntry + 0x1f));
    CopyPascalString((byte *)UpdatePascalLength(&kStrWalkonpath), (byte *)(pNamed + 0x70));
    return 0;
}


/* Fills a type 4 walk entry that follows a road: the path comes from ResolveWalkPathSelectorForView
 * and BuildAnimWaypointPath. Returns 10 if the lookup fails. */
short __cdecl BuildWalkOnRoadEntry(byte *pName1, byte *pName2, int pNamed,  /* 0x00443490 */
                                    unsigned short *pOutEntry)  /* 0x00443490 */
{
    typedef struct { int lo, hi; } Pair8;   /* the +0x18 point pair: one 8-byte copy */
    uint        selector;
    int        *hPath;  /* local_4 */

    selector = ResolveWalkPathSelectorForView(pName1, pName2);
    if ((int)selector <= 0)
        return 10;
    BuildAnimWaypointPath(selector, &hPath);
    *pOutEntry            = 1;
    pOutEntry[1]          = 0;
    pOutEntry[2]          = 4;
    pOutEntry[3]          = 0;
    pOutEntry[4]          = GetPathStartAngle(hPath);
    *(Pair8 *)(pOutEntry + 5)       = *(Pair8 *)(pNamed + 0x18);
    *(int **)(pOutEntry + 9)        = hPath;
    *(undefined4 *)(pOutEntry + 0xb) = 0;
    *(undefined4 *)(pOutEntry + 0x15) = (undefined4)gNamedEntryCacheIndex;
    CopyPascalString((byte *)(pNamed + 0x50), (byte *)(pOutEntry + 0x17));
    CopyPascalString((byte *)UpdatePascalLength(&kStrCustom), (byte *)(pOutEntry + 0x1f));
    CopyPascalString((byte *)UpdatePascalLength(&kStrWalkonroad), (byte *)(pNamed + 0x70));
    return 0;
}


/* BuildWalkOnRoadEntry for a type 5 entry, a walk through step keyframes (SampleStepKeyframe); the
 * suffix string is kStrWalkonframes. */
short __cdecl BuildWalkOnFramesEntry(byte *pName1, byte *pName2, int pNamed,  /* 0x00443570 */
                                    unsigned short *pOutEntry)  /* 0x00443570 */
{
    typedef struct { int lo, hi; } Pair8;   /* the +0x18 point pair: one 8-byte copy */
    uint        selector;
    int        *hPath;  /* local_4 */

    selector = ResolveWalkPathSelectorForView(pName1, pName2);
    if ((int)selector <= 0)
        return 10;
    BuildAnimWaypointPath(selector, &hPath);
    *pOutEntry            = 1;
    pOutEntry[1]          = 0;
    pOutEntry[2]          = 5;
    pOutEntry[3]          = 0;
    pOutEntry[4]          = GetPathStartAngle(hPath);
    *(Pair8 *)(pOutEntry + 5)       = *(Pair8 *)(pNamed + 0x18);
    *(int **)(pOutEntry + 9)        = hPath;
    *(undefined4 *)(pOutEntry + 0xb) = 0;
    *(undefined4 *)(pOutEntry + 0x15) = (undefined4)gNamedEntryCacheIndex;
    CopyPascalString((byte *)(pNamed + 0x50), (byte *)(pOutEntry + 0x17));
    CopyPascalString((byte *)UpdatePascalLength(&kStrCustom), (byte *)(pOutEntry + 0x1f));
    CopyPascalString((byte *)UpdatePascalLength(&kStrWalkonframes), (byte *)(pNamed + 0x70));
    return 0;
}


/* Fills a deferred walk entry of the given kind (1 star, 3 path, 4 road, 5 frames): deferred flag
 * set in entry[3], the actor's names, and the two walk arguments as sub-names; marks the actor's row
 * with kStrDefer. TickWalkEntry builds the real entry once the actor's row is in the current scene. */
static short __cdecl
BuildDeferredWalkEntry(byte *pSubA, byte *pSubB, short methodKind, int pNamed,  /* 0x00443650 */
                                 unsigned short *pBuilt)
{
    typedef struct { int lo, hi; } Pair8;   /* the +0x18 point pair: one 8-byte copy */

    *pBuilt       = 1;
    pBuilt[1]     = 0;
    pBuilt[2]     = (unsigned short)methodKind;
    pBuilt[3]     = 1;
    *(Pair8 *)(pBuilt + 5) = *(Pair8 *)(pNamed + 0x18);
    *(uint *)(pBuilt + 9)  = 0;
    *(int *)(pBuilt + 0x15) = gNamedEntryCacheIndex;
    CopyPascalString(pSubA, (byte *)(pBuilt + 0x27));
    CopyPascalString(pSubB, (byte *)(pBuilt + 0x2f));
    CopyPascalString((byte *)(pNamed + 0x50), (byte *)(pBuilt + 0x17));
    CopyPascalString((byte *)UpdatePascalLength(&kStrCustom), (byte *)(pBuilt + 0x1f));
    if (methodKind == 1)
        CopyPascalString(pSubA, (byte *)(pBuilt + 0x1f));
    if (methodKind == 3)
        CopyPascalString(pSubB, (byte *)(pBuilt + 0x1f));
    CopyPascalString((byte *)UpdatePascalLength(&kStrDefer), (byte *)(pNamed + 0x70));
    return 0;
}


/* Per-tick update of one walk entry (an actor's walk or turn; 0x6e bytes, one per slot of
 * gWalkTable). Fields in shorts: [0] active, [1] pause count, [2] type (0 turn, 1 to a point, 3
 * path, 4 road, 5 frames), [3] deferred flag, [4] turn target (negative = none), [5..8] the actor's
 * current value pair, [9..10] path handle, [0xb..0xc] elapsed ticks, [0xd..0x12] delta vector,
 * [0x13..0x14] duration, [0x15..0x16] LookupActorEntry cache slot, [0x17] actor name (byte 46),
 * [0x1f] completion name, [0x27] and [0x2f] sub-names.
 *
 * While paused it does nothing. A deferred entry waits until the actor's row (LookupActorEntry) is
 * in the current scene, then is built for real. A turn steps the angle toward its target and sends
 * `"name", endturn()` on arrival. Other entries advance their elapsed ticks and interpolate the
 * actor's value pair (linear for types 1 and 2, SampleCurveKeyframe for 3 and 4, SampleStepKeyframe
 * for 5); at the end the entry is cleared, its path handle released and `"name", endwalk()` sent
 * through RunActorMessage.
 *
 * Port note: the actor record LookupActorEntry fills is one 160-byte object, not scattered locals. */
short TickWalkEntry(short *pEntry)  /* 0x00443730 */
{
    typedef struct { int d[2]; } ValuePair;   /* current + target pair, 8 bytes */
    short *pValue;
    int    tickPos;
    int    duration;
    short  status;
    byte  *pName;
    /* The actor record LookupActorEntry fills (160 bytes). Offsets: +0x12 found flag, +0x18 current pair,
     * +0x1c target pair, +0x20 step argument, +0x26 step delta, +0x60 scene name, +0x70 callback name. */
    int    namedEntryData[40];

#define NM_FLAG    (*(short *)((char *)namedEntryData + 0x12))
#define NM_STEPARG (*(int   *)((char *)namedEntryData + 0x20))
#define NM_STEP    (*(short *)((char *)namedEntryData + 0x26))
#define NM_SCENE   ((byte  *)((char *)namedEntryData + 0x60))
#define NM_CBNAME  ((byte  *)((char *)namedEntryData + 0x70))
/* The current (+0x18) and target (+0x1c) value pairs, four shorts; the samplers write all 8 bytes through it. */
#define NM_PAIR    ((short *)((char *)namedEntryData + 0x18))

    if (pEntry[1] != 0) {
#if DF_TRACE_ENABLED
        /* Trace only, bounded: this fires every frame for every paused row. */
        static int nSkip;
        if (nSkip < 40) {
            const byte *nm = (const byte *)(pEntry + 0x17);
            nSkip++;
            DF_TRACE("tickMethod: SKIP paused name='%.*s' pause=%d active=%d",
                     (int)nm[0], (const char *)nm + 1,
                     (int)pEntry[1], (int)pEntry[0]);
        }
#endif
        return 0;
    }

    gNamedEntryCacheIndex            = *(int *)(pEntry + 0x15);
    pName                  = (byte *)(pEntry + 0x17);
    DF_TRACE("tickMethod: name='%.*s' entry0=%d entry1=%d entry2=%d entry3=%d entry4=%d",
             (int)pName[0], (const char *)pName + 1,
             (int)pEntry[0], (int)pEntry[1], (int)pEntry[2],
             (int)pEntry[3], (int)pEntry[4]);
    status                   = LookupActorEntry(pName, namedEntryData);
    if (status != 0) {
        DF_TRACE("tickMethod: LookupActorEntry('%.*s') -> err=%d",
                 (int)pName[0], (const char *)pName + 1, (int)status);
        return status;
    }
    NM_FLAG                 = 1;
    *(int *)(pEntry + 0x15) = gNamedEntryCacheIndex;

    if (pEntry[3] != 0) {
        *(ValuePair *)NM_PAIR = *(ValuePair *)(pEntry + 5);
        if (PascalStringsEqualNoCase(NM_SCENE, &DAT_0048a7b4) != 0) {
            switch (pEntry[2]) {
            case 1:
                status = (short)BuildWalkToStarEntry((byte *)(pEntry + 0x27),
                                                (int)namedEntryData, pEntry);
                if (status != 0) return status;
                break;
            default:
                FatalError(0x6e, 0xfca);
                break;
            case 3:
                status = BuildWalkOnPathEntry((byte *)(pEntry + 0x27),
                                     (byte *)(pEntry + 0x2f),
                                     (int)namedEntryData, pEntry);
                if (status != 0) return status;
                break;
            case 4:
                status = BuildWalkOnRoadEntry((byte *)(pEntry + 0x27),
                                     (byte *)(pEntry + 0x2f),
                                     (int)namedEntryData, pEntry);
                if (status != 0) return status;
                break;
            case 5:
                status = BuildWalkOnFramesEntry((byte *)(pEntry + 0x27),
                                     (byte *)(pEntry + 0x2f),
                                     (int)namedEntryData, pEntry);
                if (status != 0) return status;
                break;
            }
        }
        WriteBackActorEntry(namedEntryData);
        return 0;
    }

    if (pEntry[4] >= 0) {
        pValue  = pEntry + 5;
        *pValue = StepValueTowardTarget(*pValue, pEntry[4], (short)NM_STEPARG, 0xff);
        *(ValuePair *)NM_PAIR = *(ValuePair *)pValue;
        WriteBackActorEntry(namedEntryData);
        if (pEntry[4] == *pValue) {
            if (pEntry[2] == 0)
                *pEntry = 0;
            else
                pEntry[4] = (short)0xffff;   /* no turn target */
            CopyPascalString(UpdatePascalLength(&kStrQuote), &gScratchStr);
            AppendPascalString(pName, &gScratchStr);
            AppendPascalString(UpdatePascalLength(&kStrQuoteCommaEndturnCall), &gScratchStr);
            status = RunActorMessage(&gScratchStr);
            if (status != 0) return status;
        }
        return 0;
    }

    switch (pEntry[2]) {
    case 1:
    case 2:
        tickPos = NM_STEP + *(int *)(pEntry + 0xb);
        duration = *(int *)(pEntry + 0x13);
        *(int *)(pEntry + 0xb) = tickPos;
        if (duration < tickPos)
            *(int *)(pEntry + 0xb) = duration;
        tickPos = *(int *)(pEntry + 0xb);
        NM_PAIR[1] = pEntry[6] - (short)((*(int *)(pEntry + 0xd)  * tickPos) / duration);
        NM_PAIR[2] = pEntry[7] - (short)((*(int *)(pEntry + 0xf)  * tickPos) / duration);
        NM_PAIR[3] = pEntry[8] - (short)((*(int *)(pEntry + 0x11) * tickPos) / duration);
        NM_PAIR[0] = pEntry[5];
        if (tickPos < duration) {
            WriteBackActorEntry(namedEntryData);
            return 0;
        }
        break;
    case 3:
    case 4:
        tickPos = NM_STEP + *(int *)(pEntry + 0xb);
        *(int *)(pEntry + 0xb) = tickPos;
        if ((short)SampleCurveKeyframe(*(int **)(pEntry + 9), tickPos,
                                       (unsigned short *)NM_PAIR) == 0) {
            WriteBackActorEntry(namedEntryData);
            return 0;
        }
        break;
    case 5:
        tickPos = *(int *)(pEntry + 0xb) + 1;
        *(int *)(pEntry + 0xb) = tickPos;
        if ((short)SampleStepKeyframe(*(int **)(pEntry + 9), tickPos,
                                      (unsigned short *)NM_PAIR) == 0) {
            WriteBackActorEntry(namedEntryData);
            return 0;
        }
        break;
    default:
        FatalError(0x6e, 0xfcb);
    }

    CopyPascalString((byte *)(pEntry + 0x1f), NM_CBNAME);
    WriteBackActorEntry(namedEntryData);
    *pEntry = 0;
    if (*(int **)(pEntry + 9) != (int *)0) {
        FreeResObject(*(int **)(pEntry + 9));
        *(int *)(pEntry + 9) = 0;
    }
    CopyPascalString(UpdatePascalLength(&kStrQuote), &gScratchStr);
    AppendPascalString(pName, &gScratchStr);
    AppendPascalString(UpdatePascalLength(&kStrQuoteCommaEndwalkCall), &gScratchStr);
    return RunActorMessage(&gScratchStr);
}


/* Returns the number of active entries in the walk table. */
short __stdcall CountWalks(void)  /* 0x00443b40 */
{
    short  count;
    short *psEntry;
    short  i;

    count   = 0;
    psEntry = &gWalkTable;
    i       = 0x10;
    do {
        if (*psEntry != 0) {
            count++;
        }
        psEntry += 0x37;
        i--;
    } while (i != 0);
    return count;
}


/* Copies to pOutDesc the name of the oneBasedIdx-th active walk entry, or kStrUnknown if there are
 * fewer. */
void __cdecl GetWalkNameByIndex(short oneBasedIdx, byte *pOutDesc)  /* 0x00443B60 */
{
    short *psEntry;
    short  i;

    i       = 0;
    psEntry = &gWalkTable;
    do {
        if (*psEntry != 0)
            oneBasedIdx--;
        if (oneBasedIdx <= 0) {
            CopyPascalString((byte *)(psEntry + 0x17), pOutDesc);
            return;
        }
        i++;
        psEntry += 0x37;
    } while (i < 0x10);
    CopyPascalString((byte *)UpdatePascalLength(&kStrUnknown), pOutDesc);   /* the destination is pushed before the lookup call */
}


/* Returns the pause count (entry[1]) of the first active walk entry named pNameDesc (case-
 * insensitive), or 0 if none. */
short __cdecl GetWalkPauseCount(byte *pNameDesc)  /* 0x00443bc0 */
{
    short  i;                           /* declared before the pointer: i gets di, the pointer ebx */
    short *psEntry;

    psEntry = &gWalkTable;
    i       = 0;
    do {
        if (*psEntry != 0) {
            if (PascalStringsEqualNoCase(pNameDesc, (byte *)(psEntry + 0x17)) != 0) {
                return psEntry[1];
            }
        }
        i++;
        psEntry += 0x37;
    } while (i < 0x10);
    return 0;
}


/* pausewalk's worker: AdjustLoopPauseCount for the walk table, matching by name only; kStrAll
 * applies it to every slot.
 *
 * Port note: also once an invented body (classes 12 and 14); see AdjustLoopPauseCount. */
void __cdecl AdjustWalkPauseCount(byte *pNameDesc, short resume)  /* 0x00443c10 */
{
    short *psSlot = &gWalkTable;
    short  i;

    if ((short)PascalStringsEqualNoCase(
            pNameDesc, (byte *)UpdatePascalLength((char *)kStrAll)) != 0) {
        for (i = 0x10; i != 0; i--) {
            if (resume != 0) {          /* the resume case first, re-read each pass (byte match) */
                psSlot[1]++;
            } else {
                if (--psSlot[1] < 0)
                    psSlot[1] = 0;
            }
            psSlot += 0x37;
        }
        return;
    }

    for (i = 0; i < 0x10; i++, psSlot += 0x37) {
        if (*psSlot != 0 &&
            (short)PascalStringsEqualNoCase(pNameDesc, (byte *)(psSlot + 0x17)) != 0) {
            if (resume != 0) {
                psSlot[1]++;
            } else {
                if (--psSlot[1] < 0)
                    psSlot[1] = 0;
            }
            return;
        }
    }
}


/* makecricket (12007): makecricket(node, x, y, range, basePan, delta) in two argument groups around
 * an 0xfb4 separator: the node name and the position (ints), then the range (at least 1), the base
 * pan (at least 0) and the delta (at least -1, where -1 means no randomised pan). Builds a
 * CricketEntry for the object (the node found by FindNodeByDescriptor, the direction angle from the
 * player's position) and installs it (InstallCricketEntry).
 *
 * Port note: the two groups use different result locals; the earlier port read the wrong ones and
 * failed every call with 0x0e. */
short __cdecl
ActionCmd_MakeCricket(char *pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00443cc0 */
{
    short   st;
    int     nodeHandle;             /* local_4 */
    int     skipC;                  /* local_84 -- both passes write it */
    /* group 1 outputs: descriptor, X, Y, skip (0x00443cea) */
    short   aDesc[4];               /* local_c  */
    short   aX[4];                  /* local_68 */
    short   aY[4];                  /* local_60 */
    /* group 2 outputs: range, basePan, delta in that order (0x00443daf) */
    short   aRange[4];              /* local_70 */
    short   aBasePan[4];            /* local_78 */
    short   aDelta[4];              /* local_80 */
    CricketEntry entry;             /* local_58 */
    int     dy, dx;
    int     start;                  /* the parameter walks; its start is kept */

    if (gSet.active == 0)
        return 0x28;
    start = pArgBlock;

    st = EvalThreeScriptArgs(pCtx, pArgBlock, pTable, (int *)aDesc, (int *)aX, (int *)aY, &skipC);
    if (st != 0)
        return st;
    st = TakeScriptString(aDesc, gScratchStr2);
    if (st != 0)
        return st;
    st = FindNodeByDescriptor(gScratchStr2, (uint **)&nodeHandle);
    if (st != 0)
        return st;
    if (aX[0] != 4)
        return 0x0e;
    if (aY[0] != 4)
        return 0x0e;
    pArgBlock += skipC * 8;
    if (*(short *)pArgBlock != 0x0fb4)
        return 0x1c;
    pArgBlock += 8;

    st = EvalThreeScriptArgs(pCtx, pArgBlock, pTable, (int *)aRange, (int *)aBasePan, (int *)aDelta, &skipC);
    if (st != 0)
        return st;
    if (aRange[0] != 4)
        return 0x0e;
    if (aBasePan[0] != 4)
        return 0x0e;
    if (aDelta[0] != 4)
        return 0x0e;
    if (*(int *)&aRange[1] < 1)
        return 0x0e;
    if (*(int *)&aBasePan[1] < 0)
        return 0x0e;
    if (*(int *)&aDelta[1] < -1)
        return 0x0e;

    entry.type = 1;
    entry.pad0 = 0;
    *pSubSkip = ((pArgBlock += skipC * 8) - start) >> 3;
    entry.objX = (short)*(int *)&aX[1];
    entry.objY    = (short)*(int *)&aY[1];
    entry.range   = *(int *)&aRange[1];
    entry.basePan = *(int *)&aBasePan[1];
    entry.delta   = *(int *)&aDelta[1];
    if (entry.delta < 0)
        entry.targetPan = *(int *)&aBasePan[1];   /* delta -1: no randomisation */
    else
        entry.targetPan = RandomUpTo(entry.delta) + entry.basePan;
    entry.playerX = gPlayerX;
    entry.playerY = gPlayerY;
    dx = entry.objX - entry.playerX;
    dy = entry.objY - entry.playerY;
    entry.dist     = IntSqrt(dx * dx + dy * dy);
    entry.dirAngle = ComputeDirectionAngle((short)dy, (short)dx);
    entry.pad2     = 0;
    CopyPascalString(&DAT_0048a7b4, entry.ctxDesc);
    CopyPascalString(gScratchStr2, entry.nodeDesc);
    return InstallCricketEntry((short *)&entry);
}


/* Removes any cricket entry with the same name (byte 58) as pNewEntry, then copies the 37-short
 * record into the first free slot of gCricketTable. Returns 8 in the low word if the table is full. */
short __cdecl InstallCricketEntry(short *pNewEntry)  /* 0x00443f70 */
{
    typedef struct { short v[0x25]; } SpatialEntry74;   /* rep movsd 0x12 + movsw: one struct assignment */
    short  i;
    short *psSlot;

    psSlot = &gCricketTable;
    RemoveCricketEntry((byte *)(pNewEntry + 0x1d));
    i = 0;
    do {
        if (*psSlot == 0) {
            *(SpatialEntry74 *)psSlot = *(SpatialEntry74 *)pNewEntry;
            return 0;
        }
        i++;
        psSlot += 0x25;
    } while (i < 0x10);
    return 8;                           /* table full */
}


#undef NM_FLAG
#undef NM_STEPARG
#undef NM_STEP
#undef NM_SCENE
#undef NM_CBNAME
#undef NM_PAIR

/* Per-tick update of one cricket entry, a positional ambient sound (0x4a bytes, 37 shorts): [0]
 * active, [1] pause count, [2] x, [3] y, ints at [4] range, [6] base pan period, [8] deadline mode,
 * [0xa] deadline countdown, [0xc] cached listener x and [0xd] y, [0xe] cached distance, [0x10]
 * cached direction angle, [0x11] in-range flag, [0x12] pending volume (-1 none), [0x13] pending
 * node, [0x15] scene name (byte 42), [0x1d] node name (byte 58).
 *
 * It acts only while the entry's scene is the current one. When the listener has moved it recomputes
 * the distance (IntSqrt) and direction. Inside the range it either queues the node for a sound
 * channel with its volume (counting it in gObjectUpdateCount for AssignCricketSoundChannels) or
 * counts down its deadline; leaving the range deactivates the node. */
short TickCricketEntry(short *pEntry)  /* 0x00443fc0 */
{
    /* Returns short: every exit but a failed node lookup is `xor ax,ax`. */
    short  x;
    short  y;
    int    dy;
    int    dx;
    int    dist;
    int    range;
    short  vol;
    short  pan;
    short  st;
    int   *node;

    if (pEntry[1] != 0)
        return 0;
    if (gSet.active == 0)
        return 0;
    if (PascalStringsEqualNoCase(&DAT_0048a7b4, (byte *)(pEntry + 0x15)) == 0)
        return 0;
    x = gPlayerX;
    if (pEntry[0xc] != x || pEntry[0xd] != gPlayerY) {
        pEntry[0xc] = gPlayerX;
        y = gPlayerY;
        pEntry[0xd] = y;
        dx = pEntry[2] - x;
        dy = pEntry[3] - y;
        *(int *)(pEntry + 0xe) = IntSqrt(dx * dx + dy * dy);
        pEntry[0x10] = ComputeDirectionAngle((short)dy, (short)dx);
    }
    if (*(int *)(pEntry + 8) < 0) {
        if (--*(int *)(pEntry + 10) > 0)
            return 0;
        range = *(int *)(pEntry + 4);
        dist  = *(int *)(pEntry + 0xe);
        if (range > dist) {
            vol = -(dist * 0xff) / range + 0xff;
            pan = ComputeStereoPanning(gCurrentDeg, pEntry[0x10]);
            st = FindNodeByDescriptor((byte *)(pEntry + 0x1d), (uint **)&node);
            if (st != 0)
                return st;
            SetSoundNodeVolume((byte *)node, vol);
            SetSoundNodePan((byte *)node, pan);
            ReplaceSoundChannel_KR(node);
        }
        *pEntry = 0;
        return 0;
    }
    range = *(int *)(pEntry + 4);
    dist  = *(int *)(pEntry + 0xe);
    if (range > dist) {
        vol = -(dist * 0xff) / range + 0xff;
        pan = ComputeStereoPanning(gCurrentDeg, pEntry[0x10]);
        st = FindNodeByDescriptor((byte *)(pEntry + 0x1d), (uint **)&node);
        if (st != 0)
            return st;
        SetSoundNodeVolume((byte *)node, vol);
        SetSoundNodePan((byte *)node, pan);
        if ((short)GetSoundNodeLoop((int)node) != 0) {
            pEntry[0x12] = vol;
            *(int **)(pEntry + 0x13) = node;
            pEntry[0x11] = 1;
            (*(short *)&gObjectUpdateCount)++;
            return 0;
        }
        if (pEntry[0x11] != 0) {
            if (--*(int *)(pEntry + 10) <= 0) {
                *(int *)(pEntry + 10) = *(int *)(pEntry + 6) + RandomUpTo(*(uint *)(pEntry + 8));
                ReplaceSoundChannel_KR(node);
                return 0;
            }
        } else {
            pEntry[0x11] = 1;
            *(int *)(pEntry + 10) = *(int *)(pEntry + 6) + RandomUpTo(*(uint *)(pEntry + 8));
            return 0;
        }
    } else if (pEntry[0x11] != 0) {
        pEntry[0x11] = 0;
        st = FindNodeByDescriptor((byte *)(pEntry + 0x1d), (uint **)&node);
        if (st != 0)
            return st;
        DeactivateSoundNodeEntry((int)node);
    }
    return 0;
}


/* Runs after the cricket pass when entries queued a sound (gObjectUpdateCount > 0): picks the two
 * pending entries with the highest volume, silences every other pending node
 * (DeactivateSoundNodeEntry) and gives the two a sound channel (AssignSoundChannel). Only the two
 * loudest crickets are ever heard. */
void __stdcall AssignCricketSoundChannels(void)  /* 0x00444260 */
{
    /* declaration order and the separate counter m set the exe's registers */
    short *first;
    short  n;
    short  v;
    short  max;
    short  m;
    short *p;
    short *second;

    max    = -1;
    n      = 0x10;
    second = NULL;
    first  = NULL;
    p      = &gCricketTable;
    do {
        v = p[0x12];
        if (max < v) {
            max   = v;
            first = p;
        }
        p += 0x25;
        n--;
    } while (n != 0);
    (*(short *)&gObjectUpdateCount)--;
    if (*(short *)&gObjectUpdateCount > 0) {
        n   = 0x10;
        max = -1;
        first[0x12] = max;
        p   = &gCricketTable;
        do {
            v = p[0x12];
            if (max < v) {
                max    = v;
                second = p;
            }
            p += 0x25;
            n--;
        } while (n != 0);
    }
    (*(short *)&gObjectUpdateCount)--;
    if (*(short *)&gObjectUpdateCount > 0) {
        m = 0x10;
        p = &gCricketTable;
        second[0x12] = -1;
        do {
            if (p[0x12] >= 0)
                DeactivateSoundNodeEntry(*(int *)(p + 0x13));
            p += 0x25;
            m--;
        } while (m != 0);
    }
    AssignSoundChannel(*(int **)(first + 0x13));
    if (second != NULL)
        AssignSoundChannel(*(int **)(second + 0x13));
}


/* Turns the direction of a sound (sourceAngle, 0..255) and the listener's heading into a pan value
 * 0..255: the source's offset from 0x80 is folded onto the heading, mirrored into one half, doubled
 * around the centre 0x80 and clamped. */
short __cdecl ComputeStereoPanning(short sourceAngle, short listenerAngle)  /* 0x00444320 */
{
    short offset;
    short pan;

    offset = ComputeAngularDistance(sourceAngle, 0x80);
    if (sourceAngle > 0x80)
        pan = listenerAngle - offset;
    else
        pan = listenerAngle + offset;
    pan &= 0xff;
    if ((byte)pan > 0xc0)
        pan = 0x180 - pan;
    if (pan < 0x40)
        pan = 0x80 - pan;
    pan = pan * 2 - 0x80;
    if (pan < 0)
        pan = 0;
    if (pan > 0xff)
        pan = 0xff;
    return pan;
}


/* Returns the number of active entries in the cricket table. */
short __stdcall CountCrickets(void)  /* 0x00444390 */
{
    short  count;
    short *psEntry;
    short  i;

    count   = 0;
    psEntry = &gCricketTable;
    i       = 0x10;
    do {
        if (*psEntry != 0) {
            count++;
        }
        psEntry += 0x25;
        i--;
    } while (i != 0);
    return count;
}


/* Copies to pOut the name of the index-th (1-based) active cricket entry, or kStrUnknown if there
 * are fewer. */
void __cdecl GetCricketNameByIndex(short index, byte *pOut)  /* 0x004443b0 */
{
    short *psEntry;
    short  i;

    i       = 0;
    psEntry = &gCricketTable;
    do {
        if (*psEntry != 0)
            index--;
        if (index <= 0) {
            CopyPascalString((byte *)(psEntry + 0x1d), pOut);
            return;
        }
        i++;
        psEntry += 0x25;
    } while (i < 0x10);
    CopyPascalString((byte *)UpdatePascalLength(&kStrUnknown), pOut);   /* the lookup written inline: the other arguments are pushed before it is called */
}


/* Returns the pause count (entry[1]) of the first active cricket entry named pNameDesc (case-
 * insensitive), or 0 if none. */
short __cdecl GetCricketPauseCount(byte *pNameDesc)  /* 0x00444410 */
{
    short  i;                           /* declared before the pointer: i gets di, the pointer ebx */
    short *psEntry;

    psEntry = &gCricketTable;
    i       = 0;
    do {
        if (*psEntry != 0) {
            if (PascalStringsEqualNoCase(pNameDesc, (byte *)(psEntry + 0x1d)) != 0) {
                return psEntry[1];
            }
        }
        i++;
        psEntry += 0x25;
    } while (i < 0x10);
    return 0;
}


/* pausecricket's worker: AdjustLoopPauseCount for the cricket table, matching by name only; kStrAll
 * applies it to every slot.
 *
 * Port note: also once an invented body (classes 12 and 14); see AdjustLoopPauseCount. */
void __cdecl AdjustCricketPauseCount(byte *pNameDesc, short resume)  /* 0x00444460 */
{
    short *psSlot = &gCricketTable;
    short  i;

    if ((short)PascalStringsEqualNoCase(
            pNameDesc, (byte *)UpdatePascalLength((char *)kStrAll)) != 0) {
        for (i = 0; i < 0x10; i++) {
            if (resume != 0) {          /* the resume case first, re-read each pass (byte match) */
                psSlot[1]++;
            } else {
                if (--psSlot[1] < 0)
                    psSlot[1] = 0;
            }
            psSlot += 0x25;
        }
        return;
    }

    for (i = 0; i < 0x10; i++, psSlot += 0x25) {
        if (*psSlot != 0 &&
            (short)PascalStringsEqualNoCase(pNameDesc, (byte *)(psSlot + 0x1d)) != 0) {
            if (resume != 0) {
                psSlot[1]++;
            } else {
                if (--psSlot[1] < 0)
                    psSlot[1] = 0;
            }
            return;
        }
    }
}


/* Direction angle of a path's first segment: the angle from the first point (data +0x14) to the
 * second (+0x1c) of the path resource pHandle (ComputeAngleBetweenPoints). */
short __cdecl GetPathStartAngle(int *pHandle)  /* 0x00444510 */
{
    int   dataPtr;
    /* four 8-byte locals in the exe's frame order; the points are copied as whole 8-byte path entries
     * (WayPt, waypoint_paths.c) and VC++ 4.2 keeps only the dword then read */
    short a[4];
    short b[4];
    short ptA[4];   /* [1] = x at +2, [2] = y at +4 */
    short ptB[4];

    dataPtr = GetResDataPtr(pHandle);
    *(POINT *)a = *(POINT *)(dataPtr + 0x14);
    *(POINT *)b = *(POINT *)(dataPtr + 0x1c);
    UnlockResObject(pHandle);
    ptA[1] = a[0];
    ptA[2] = a[1];
    ptB[1] = b[0];
    ptB[2] = b[1];
    return ComputeAngleBetweenPoints((int)ptA, (int)ptB);
}


/* Returns the object type a name denotes, case-insensitively: 1 actor, 2 prop, 3 scene, 4 flat, 0
 * for anything else. Loops are made for these four types. */
short GetDescriptorTypeId(byte *pDesc)  /* 0x00444580 */
{
    byte *pbComp;
    short cmp;

    pbComp = UpdatePascalLength(&kStrActor);
    cmp    = PascalStringsEqualNoCase(pDesc, pbComp);
    if (cmp != 0) return 1;

    pbComp = UpdatePascalLength(&kStrProp);
    cmp    = PascalStringsEqualNoCase(pDesc, pbComp);
    if (cmp != 0) return 2;

    pbComp = UpdatePascalLength(&kStrScene);
    cmp    = PascalStringsEqualNoCase(pDesc, pbComp);
    if (cmp != 0) return 3;

    pbComp = UpdatePascalLength(&kStrFlat);
    cmp    = PascalStringsEqualNoCase(pDesc, pbComp);
    return (cmp == 0) ? 0 : 4;
}


#undef ISA_RESHANDLE
#undef ISA_PARAM
#undef ISA_VIEWNAME

/* The inverse of GetDescriptorTypeId: copies the type name for typeId (1 actor, 2 prop, 3 scene, 4
 * flat; kStrUnknown otherwise) to pOutDesc. */
void __cdecl GetDescriptorById(short typeId, byte *pOutDesc)  /* 0x00444620 */
{
  byte *pbVar1;

  switch(typeId) {
  case 1:
    CopyPascalString((byte *)UpdatePascalLength(&kStrActor), pOutDesc);   /* the destination is pushed before the lookup call (the same in the arms below) */
    return;
  case 2:
    CopyPascalString((byte *)UpdatePascalLength(&kStrProp), pOutDesc);
    return;
  case 3:
    CopyPascalString((byte *)UpdatePascalLength(&kStrScene), pOutDesc);
    return;
  case 4:
    CopyPascalString((byte *)UpdatePascalLength(&kStrFlat), pOutDesc);
    return;
  default:
    CopyPascalString((byte *)UpdatePascalLength(&kStrUnknown), pOutDesc);   /* the destination is pushed before the lookup call */
    return;
  }
}


/* Direction angle (0..255, 256 = a full turn) from point pA to point pB, each a record with a short
 * x at +2 and y at +4. Returns short: all ten exe call sites store ax unextended, so an angle of
 * 128..255 must stay positive. */
short __cdecl ComputeAngleBetweenPoints(int pA, int pB)  /* 0x004446d0 */
{
    short dx, dy;

    dx = *(short *)(pB + 2) - *(short *)(pA + 2);
    dy = *(short *)(pB + 4) - *(short *)(pA + 4);
    return ComputeDirectionAngle(dy, dx);
}


/* Maps the vector (y, x) to the engine's direction angle, 0..255 for a full turn: the exact
 * multiples are returned directly, otherwise the arctangent comes from the table
 * kDirectionAngleTable (indexed by the smaller over the larger component, scaled by 512) and is
 * placed by octant. ComputeAngleBetweenPoints calls it as (dy, dx). */
short __cdecl ComputeDirectionAngle(short x, short y)  /* 0x00444700 */
{
    /* short: every path leaves the angle in ax, masked to a byte */
    short swap;
    short t;
    short ratio;
    short angle;

    if (x == 0) {
        if (y < 0)
            return 0x80;
        return 0;
    } else if (y == 0) {
        if (x < 0)
            return 0xc0;
        return 0x40;
    } else {
        swap = 0;
        if (x < 0) {
            x = -x;
            if (y < 0) {
                y = -y;
                if (y == x)
                    return 0xa0;
                if (y < x) {
                    t = y; y = x; x = t;
                    swap = 1;
                }
                ratio = ((int)x << 9) / y;
                if (swap)
                    angle = 0xc0 - kDirectionAngleTable[ratio];
                else
                    angle = kDirectionAngleTable[ratio] + 0x80;
            } else {
                if (y == x)
                    return 0xe0;
                if (y < x) {
                    t = y; y = x; x = t;
                    swap = 1;
                }
                ratio = ((int)x << 9) / y;
                if (swap)
                    angle = kDirectionAngleTable[ratio] + 0xc0;
                else
                    angle = 0x100 - kDirectionAngleTable[ratio];
            }
        } else if (y < 0) {
            y = -y;
            if (y == x)
                return 0x60;
            if (y < x) {
                t = y; y = x; x = t;
                swap = 1;
            }
            ratio = ((int)x << 9) / y;
            if (swap)
                angle = kDirectionAngleTable[ratio] + 0x40;
            else
                angle = 0x80 - kDirectionAngleTable[ratio];
        } else {
            if (y == x)
                return 0x20;
            if (y < x) {
                t = y; y = x; x = t;
                swap = 1;
            }
            ratio = ((int)x << 9) / y;
            if (swap)
                angle = 0x40 - kDirectionAngleTable[ratio];
            else
                angle = kDirectionAngleTable[ratio];
        }
    }
    return angle & 0xff;
}


/* The angular distance between two direction angles (0..255, 256 = a full turn), 0..128. */
short __cdecl ComputeAngularDistance(short a, short b) /* 0x004448d0 */
{
    short t, direct, wrap;

    if (b < a) { t = a; a = b; b = t; }  /* the parameters are swapped in place */
    direct = b - a;
    wrap   = (a - b) + 0x100;
    if (wrap > direct)
        return direct;
    return wrap;
}


/* -----------------------------------------------------------------------
 * SampleCurveKeyframe                                 Address: 0x00444900
 *
 * Evaluates a parametric curve at position tickPos and writes the
 * interpolated value to pOut.  Uses GetResDataPtr to lock the curve
 * resource then binary-searches the keyframe table for the enclosing
 * segment.  Values are linearly interpolated and packed via ComputeAngleBetweenPoints.
 *
 * hCurve   -- curve resource handle
 * tickPos   -- position along the curve (ticks/units)
 * pOut   -- output buffer: [0]=packed word, [1][2][3]=interpolated values
 *
 * Returns 0 (success, still within curve) or 1 (past end of curve / done).
 * ----------------------------------------------------------------------- */
typedef struct { short x, y, z, w; } Keyframe;
/* Evaluates a curve resource at tickPos and writes the interpolated value pair to pOut (4 shorts:
 * the packed word, then three values). It finds the segment holding tickPos in the keyframe table
 * and interpolates linearly over the end keyframe's duration. Returns 0 while inside the curve and 1
 * once tickPos is at or past its end, when the last keyframe's values are written. */
short __cdecl SampleCurveKeyframe(int *hCurve, int tickPos, unsigned short *pOut)  /* 0x00444900 */
{
    /* The records as in SampleStepKeyframe (class 3): the segment's end keyframe and start, and two
     * point records; the end keyframe's w is the segment's duration. */
    char    *base;       /* declaration order sets the register roles */
    int      dur;
    Keyframe *p;
    int      n;
    int      i;
    Keyframe cur;
    Keyframe prev;
    short    ptA[4];
    short    ptB[4];

    base = (char *)GetResDataPtr(hCurve);
    n    = *(int *)(base + 8);
    if (tickPos >= *(int *)base) {
        prev = *(Keyframe *)(base + 4 + n * 8);
        cur  = *(Keyframe *)(base + 0xc + n * 8);
        pOut[1] = cur.x;
        pOut[2] = cur.y;
        pOut[3] = cur.z;
        ptB[1] = prev.x;
        ptB[2] = prev.y;
        ptA[1] = cur.x;
        ptA[2] = cur.y;
        pOut[0] = (unsigned short)ComputeAngleBetweenPoints((int)(intptr_t)ptB, (int)(intptr_t)ptA);
        UnlockResObject(hCurve);
        return 1;
    }
    i = 1;
    if (i < n) {
    p = (Keyframe *)(base + 0x1c);
    do {
        cur = *p;
        if (tickPos <= cur.w) {
            prev = *(Keyframe *)(base + 0xc + i * 8);
            dur  = cur.w;
            pOut[1] = (short)(((cur.x - prev.x) * tickPos) / dur) + prev.x;
            pOut[2] = (short)(((cur.y - prev.y) * tickPos) / dur) + prev.y;
            pOut[3] = prev.z + (short)(((cur.z - prev.z) * tickPos) / dur);
            ptB[1] = prev.x;
            ptB[2] = prev.y;
            ptA[1] = cur.x;
            ptA[2] = cur.y;
            pOut[0] = (unsigned short)ComputeAngleBetweenPoints((int)(intptr_t)ptB, (int)(intptr_t)ptA);
            UnlockResObject(hCurve);
            return 0;
        }
        tickPos -= cur.w;
        p++;
        i++;
    } while (i < n);
    }
    FatalError(0, 0xfcc);
    return 1;
}


/* Like SampleCurveKeyframe for a step table (8-byte steps, no interpolation): writes the value pair
 * of step stepIdx (1-based) to pOut. Returns 0 inside the table and 1 at or past its end, with the
 * last step's values. A stepIdx below 1 is fatal (0xfcd). */
short __cdecl SampleStepKeyframe(int *hSteps, int stepIdx, unsigned short *pOut)  /* 0x00444ac0 */
{
    /* Two 8-byte keyframe records copied whole, and two point records {pad, x, y, pad} for
     * ComputeAngleBetweenPoints; Ghidra split each point, and as separate locals the helper read garbage (class 3). */
    int      n;
    char    *base;
    Keyframe cur;
    Keyframe prev;
    short    ptA[4];
    short    ptB[4];

    base = (char *)GetResDataPtr(hSteps);
    n    = *(int *)(base + 8);
    if (stepIdx <= 0)
        FatalError(0, 0xfcd);
    if (stepIdx >= n) {
        prev = *(Keyframe *)(base + 4 + n * 8);
        cur  = *(Keyframe *)(base + 0xc + n * 8);
        pOut[1] = cur.x;
        pOut[2] = cur.y;
        pOut[3] = cur.z;
        ptB[1] = prev.x;
        ptB[2] = prev.y;
        ptA[1] = cur.x;
        ptA[2] = cur.y;
        pOut[0] = (unsigned short)ComputeAngleBetweenPoints((int)(intptr_t)ptB, (int)(intptr_t)ptA);
        UnlockResObject(hSteps);
        return 1;
    }
    prev = *(Keyframe *)(base + 0xc + stepIdx * 8);
    cur  = *(Keyframe *)(base + 0x14 + stepIdx * 8);
    pOut[1] = cur.x;
    pOut[2] = cur.y;
    pOut[3] = cur.z;
    ptB[1] = prev.x;
    ptB[2] = prev.y;
    ptA[1] = cur.x;
    ptA[2] = cur.y;
    pOut[0] = (unsigned short)ComputeAngleBetweenPoints((int)(intptr_t)ptB, (int)(intptr_t)ptA);
    UnlockResObject(hSteps);
    return 0;
}


/* Moves a value on the wrapped range 0..max toward target by delta along the shorter way round,
 * never overshooting; the walk entries use it for turns, with max 0xff. */
short __cdecl StepValueTowardTarget(short current, short target, short delta, short max)  /* 0x00444c10 */
{
    if (current < target) {
        if ((short)(current - target + max) > (short)(target - current)) {
            current += delta;
            if (current > target)
                current = target;
        } else {
            current -= delta;
            if (current < 0) {
                current += max;
                if (current < target)
                    current = target;
            }
        }
    } else {
        if ((short)(target - current + max) > (short)(current - target)) {
            current -= delta;
            if (current < target)
                current = target;
        } else {
            current += delta;
            if (current >= max) {
                current -= max;
                if (current > target)
                    current = target;
            }
        }
    }
    return current;
}

