/* interpreter.c -- TI.EXE 0x00447240..0x00448620: the message-name table and the script
 * interpreter: method calls (EvalMethodCall, ExecuteMethodBody), the statement interpreter
 * (InterpretStatements) and the skippers that find the end of if, while, switch and for blocks. */

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

/* The message names (table at 0x0045e120). ConvertMessageNameToPascal rewrites each name into a Pascal string
 * in place. The original wrote them as literals in the initializer, which VC++ 4.2 puts in writable .data just
 * before the table (0x0045dff0..0x0045e118); GCC puts literals in read-only memory, so the port names a
 * writable array for each. It sits here, not in display_ui.c: in that file it shifted the data by 0x130 bytes. */
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char g_ich_opencast[9]     = "opencast";
char g_ich_openactor[10]   = "openactor";
char g_ich_closecast[10]   = "closecast";
char g_ich_closeactor[11]  = "closeactor";
char g_ich_openflat[9]     = "openflat";
char g_ich_openstage[10]   = "openstage";
char g_ich_closeflat[10]   = "closeflat";
char g_ich_closestage[11]  = "closestage";
char g_ich_endwalk[8]      = "endwalk";
char g_ich_endturn[8]      = "endturn";
char g_ich_openshop[9]     = "openshop";
char g_ich_openprop[9]     = "openprop";
char g_ich_closeshop[10]   = "closeshop";
char g_ich_closeprop[10]   = "closeprop";
char g_ich_openpuppet[11]  = "openpuppet";
char g_ich_closepuppet[12] = "closepuppet";
char g_ich_menustate[10]   = "menustate";
char g_ich_boot[5]         = "boot";
char g_ich_idle[5]         = "idle";
char g_ich_menuselect[11]  = "menuselect";
char g_ich_keydown[8]      = "keydown";
char g_ich_keyrepeat[10]   = "keyrepeat";
char g_ich_mousedown[10]   = "mousedown";
char g_ich_openset[8]      = "openset";
char g_ich_openscene[10]   = "openscene";
char g_ich_closeset[9]     = "closeset";
char g_ich_closescene[11]  = "closescene";
InputChannelEntry g_inputChannelTable[27] = {   /* 0x0045e120 */
    { g_ich_opencast,    0 },
    { g_ich_openactor,   0 },
    { g_ich_closecast,   0 },
    { g_ich_closeactor,  0 },
    { g_ich_openflat,    0 },
    { g_ich_openstage,   0 },
    { g_ich_closeflat,   0 },
    { g_ich_closestage,  0 },
    { g_ich_endwalk,     0 },
    { g_ich_endturn,     0 },
    { g_ich_openshop,    0 },
    { g_ich_openprop,    0 },
    { g_ich_closeshop,   0 },
    { g_ich_closeprop,   0 },
    { g_ich_openpuppet,  0 },
    { g_ich_closepuppet, 0 },
    { g_ich_menustate,   0 },
    { g_ich_boot,        0 },
    { g_ich_idle,        0 },
    { g_ich_menuselect,  0 },
    { g_ich_keydown,     0 },
    { g_ich_keyrepeat,   0 },
    { g_ich_mousedown,   0 },
    { g_ich_openset,     0 },
    { g_ich_openscene,   0 },
    { g_ich_closeset,    0 },
    { g_ich_closescene,  0 },
};
#else
InputChannelEntry g_inputChannelTable[27] = {   /* 0x0045e120 */
    { "opencast",     0 },
    { "openactor",    0 },
    { "closecast",    0 },
    { "closeactor",   0 },
    { "openflat",     0 },
    { "openstage",    0 },
    { "closeflat",    0 },
    { "closestage",   0 },
    { "endwalk",      0 },
    { "endturn",      0 },
    { "openshop",     0 },
    { "openprop",     0 },
    { "closeshop",    0 },
    { "closeprop",    0 },
    { "openpuppet",   0 },
    { "closepuppet",  0 },
    { "menustate",    0 },
    { "boot",         0 },
    { "idle",         0 },
    { "menuselect",   0 },
    { "keydown",      0 },
    { "keyrepeat",    0 },
    { "mousedown",    0 },
    { "openset",      0 },
    { "openscene",    0 },
    { "closeset",     0 },
    { "closescene",   0 },
};
#endif


#include "topics/savestate.h"

/* Converts the 27 names of g_inputChannelTable to Pascal strings in place and registers each. Called
 * once by RunGameLoop, right after ConvertKeywordTablesToPascal. */
void __stdcall ConvertMessageNamesToPascal(void)  /* 0x00447240 */
{
    short              i;        /* declared first: the exe keeps the count in si, the cursor in edi */
    InputChannelEntry *pEntry;

    pEntry = &g_inputChannelTable[0];
    for (i = 0x1b; i > 0; i--)
    {
        ConvertMessageNameToPascal(pEntry->pName);
        pEntry++;
    }
}


/* Converts the C string pName to a Pascal string in place (the same prefix as
 * ConvertCStringToPascalInPlace). */
void __cdecl ConvertMessageNameToPascal(char *pName) /* 0x00447260 */
{
    char  c;
    short s;
    s = 0;
    c = pName[0];
    while (c != '\0') {
        s = (short)(s + 1);
        c = pName[s];                   /* movsx: the index is the signed short */
    }
    memmove(pName + 1, pName, (size_t)s);
    pName[0] = (char)(unsigned char)s;
}


/* Zeroes the for-loop and while-loop depth counters (gForDepth, gWhileDepth). */
void __stdcall ResetLoopStacks(void)  /* 0x00447290 */
{
    gForDepth = 0;
    gWhileDepth = 0;
}


/* Evaluates a method call. It finds the extent of the argument block (0xfb2 / 0xfb3 brackets) and
 * runs ExecuteMethodBody on each MethodCallFrame of the chain pCtxA (78 bytes each, walked by
 * adjacency, so callers build the frames as one array: see df_types.h) until one does not answer 4
 * (method not found). If none matches, ClassifyMethodIdentifier decides whether the name is a
 * message name. LatchFailedMethodFrame runs on success or error. *pSkipCount receives the
 * instructions consumed.
 *
 * Returns 0, 2 (malformed block), 4 (not found) or 0x2c (insufficient stack). */
short __cdecl EvalMethodCall(short *pCtxA, void *pCtxB, short *pInstr,  /* 0x004472a0 */
                             int *pTable, void *pResult, int *pSkipCount)
{
    short  depth;
    short  status;
    short  word;
    int    stackCheck;
    short *pCur;

    stackCheck = GetCallBudget();
    if (stackCheck < 0x800)
    {
        return 0x2c;
    }

    depth = 0;
    pCur = pInstr;

    for (;;)
    {
        word = *pCur;

        if (word == 0xfb2)
        {
            depth++;
        }

        if (word == 0xfb3)
        {
            depth--;
            if (depth <= 0)
            {
                *pSkipCount = ((int)(pCur + 4) - (int)pInstr) >> 3;

                for (;;)
                {
                    status = ExecuteMethodBody(pCtxA, pCtxB,
                                               pInstr, pTable, pResult);
                    if (status == 0)
                    {
                        return 0;
                    }

                    if (status == 4)
                    {
                        if (((MethodCallFrame *)pCtxA)->isLast == 0)
                            status = 0;
                        word = ClassifyMethodIdentifier(pInstr);
                        if (word != 0)
                            status = 0;
                    }

                    if (status != 0)
                        break;

                    if (((MethodCallFrame *)pCtxA)->isLast != 0)
                    {
                        return 0;
                    }

                    pCtxA += 0x27;
                }

                if (status == 4)
                {
                    ((MethodCallFrame *)pCtxB)->status   = status;
                    ((MethodCallFrame *)pCtxB)->posIndex =
                        ((int)pInstr - (int)((MethodCallFrame *)pCtxB)->pData) >> 3;
                    LatchFailedMethodFrame(pCtxB);
                    return status;
                }

                ((MethodCallFrame *)pCtxA)->status = status;
                LatchFailedMethodFrame((void *)pCtxA);
                return status;
            }
        }

        if (word == 6)
        {
            DF_TRACE("!! err2 ExecuteMethodBody: end-of-stream marker at %p", pCur);
            return 2;   /* unexpected end of stream */
        }

        if (word == 0)
        {
            DF_TRACE("!! err2 ExecuteMethodBody: null instruction at %p", pCur);
            return 2;   /* null instruction */
        }

        pCur += 4;
    }
}


/* Runs one frame's method: finds the 0xfa1 code block in the frame's bytecode (pData, +0x14; status
 * 4 if there is none), then loops BindMethodArguments over the code blocks until one matches the
 * call (skip count >= 0), and runs it with InterpretStatements in a fresh scope resource. A block
 * that does not match moves on through AdvanceToNextMethodBlock, ending with status 4. The frame's
 * position word (+0x0c) is kept current.
 *
 * Port note: the scan once started at +0x10 (the owner pointer) and the position word was +0x0c's
 * old meaning, the owner, overwritten on every iteration. */
short __cdecl ExecuteMethodBody(void *pCtx, void *pCtxB, short *pInstr,  /* 0x004473c0 */
                                int *pTable, void *pResult)
{
    int    pScope;
    short *pBlock;
    short  status;
    short  op;
    int    skipCount;
    MethodCallFrame *pFrame;

    pFrame = (MethodCallFrame *)pCtx;
    pBlock = (short *)pFrame->pData;
    for (;;) {
        op = *pBlock;                   /* one load, two compares */
        if (op == 0x0fa1)
            break;
        if (op == 0)
            return 4;
        pBlock += 4;
    }

    pScope = (int)AllocMethodExecutionScopeResource();

    /* every exit after the allocation shares one release + return */
    for (;;) {
        status = BindMethodArguments((int)pBlock, (int *)pScope, pCtxB,
                                     pInstr, pTable, &skipCount);

        /* position word (+0x0c), relative to the same pData base */
        pFrame->posIndex = ((int)pBlock - (int)pFrame->pData) >> 3;

        if (status != 0)
            break;

        if (skipCount >= 0) {
            /* skipCount is read before pResult in the exe: the advance is its own statement */
            pBlock += skipCount * 4;
            status = InterpretStatements(pCtx, pBlock, (int *)pScope, pResult);
            break;
        }

        status = AdvanceToNextMethodBlock(pBlock, &skipCount);
        if (status != 0)
            break;
        status = 4;
        if (skipCount < 0)
            break;
        pBlock += skipCount * 4;
    }
    UninstallResourcePair((void *)pScope);
    return status;
}


/* Matches a call against one code block: compares the method name at pBlock+8 with the call's
 * identifier and, on a match, pairs the formal parameters (pBlock+0x18) with the actual arguments
 * (pInstr+8), both in 0xfb2 / 0xfb3 brackets with 0xfb4 separators. Each formal is declared in
 * pScope and given the evaluated argument (ProcessScriptBlock, WritePropertyValue); an empty formal
 * list (0xfb3 right after 0xfb2) is a call without arguments. *pSkip is the code block's skip count,
 * -1 when the names differ. Returns 0, 2 (malformed) or 0x1c (argument count mismatch). */
short __cdecl BindMethodArguments(int pBlock, int *pScope, int pCtxB,  /* 0x00447490 */
                                 short *pInstr, int *pTable, int *pSkip)
{
    /* The exe walks its pBlock parameter (and pInstr) with the start kept in a local, like the other bytecode
     * skippers; the zero-argument case jumps into the loop's end-of-list block (0x004475ec). */
    short  err;
    short  propTag;
    int    argSkip;       /* used as a dword */
    int    start;
    short  argResult[4];

    start = pBlock;

    pBlock += 8;
    err = ExtractInstrIdentifier((short *)pBlock, &gScratchStr);
    if (err != 0)
        return err;
    err = ExtractInstrIdentifier(pInstr, gScratchStr2);
    if (err != 0)
        return err;
    if (PascalStringsEqualNoCase(&gScratchStr, gScratchStr2) == 0) {
        *pSkip = -1;
        return 0;
    }

    pBlock += 8;
    if (*(short *)pBlock != 0xfb2)
        return 2;
    pBlock += 8;
    pInstr += 4;
    if (*pInstr != 0xfb2)
        return 2;
    pInstr += 4;

    if (*(short *)pBlock == 0xfb3)
        goto done;

    for (;;) {
        err = ExtractInstrIdentifier((short *)pBlock, &gScratchStr);
        if (err != 0)
            return err;
        pBlock += 8;
        err = DeclarePropertyFromBytecodeInstr(pScope, &gScratchStr, &propTag, pBlock);
        if (err != 0)
            return err;
        err = ProcessScriptBlock(pCtxB, pInstr, pTable, (int *)argResult, &argSkip);
        if (err != 0)
            return err;
        pInstr += argSkip * 4;   /* the whole dword skip count */
        err = WritePropertyValue(pScope, propTag, argResult);
        if (err != 0)
            return err;
        if (*(short *)pBlock != 0xfb4) {
            if (*(short *)pBlock != 0xfb3)
                return 2;
done:
            pBlock += 8;
            while (*(short *)pBlock == 6)
                pBlock += 8;
            if (*pInstr != 0xfb3)
                return 2;
            *pSkip = (pBlock - start) >> 3;
            return 0;
        }
        pBlock += 8;
        if (*pInstr != 0xfb4)
            break;               /* argument count mismatch */
        pInstr += 4;
    }
    return 0x1c;
}


/* The statement interpreter for a method body: loops over the bytecode from pInstr and runs each
 * statement by opcode. After each one it checks the 6 separator, skips separators, stores the
 * position in pCtx (+0x0c, relative to +0x14) and, if gKeyAborts is set, tests for the escape key.
 *
 * Opcodes: 0xfa2 / 0xfa3 formal parameters (global / local scope), 0xfa4 end of block, 0xfa5 break,
 * 0xfa6 / 0xfa7 / 0xfa8 if / endif / else, 0xfa9 / 0xfaa / 0xfab switch / endswitch / case, 0xfac /
 * 0xfad / 0xfae / 0xfaf for / to / step / next, 0xfb0 / 0xfb1 while / wend, 0xfb8 / 0xfb9 return
 * with / without a value, 0xfbc / 0xfbd declare local / global; 12000..16057 go to
 * DispatchActionCommand, anything else is an assignment or method call. For loops use a 20-entry
 * stack of 14-byte records (gForStack, gForDepth) and while loops a 20-entry stack (gWhileStack,
 * gWhileDepth); a full stack is status 3. pResult receives the value of a 0xfb8 return.
 *
 * Returns 0 or the statement's error code. */
short __cdecl InterpretStatements(void *pCtx, short *pInstr, int *pTable,  /* 0x00447640 */
                                  void *pResult)
{
#pragma pack(push, 2)
    struct ForLoopEntry
    {
        short  tag;         /* property tag of the loop variable */
        int    limit;
        int    step;
        short *loopback;    /* first statement of the body */
    };
    struct WhileLoopEntry
    {
        short *cond;        /* the condition expression */
    };
#pragma pack(pop)
    /* pCtx is a pointer parameter, not an int: a parameter's pointer-ness ranks it in VC++ 4.2's register
     * allocation (esi/ebx here); no count, order or spelling moved this. */
    short  opcode;
    short  err;
    int    stepVal;
    short *pCur;
    short  ifDepth;
    short  forDepth;
    short  whileDepth;
    short  switchDepth;
    int    skipCount;
    int    initVal;         /* declared here and stmtResult last: sets the operand order of the four limit compares */
    struct ForLoopEntry rec;
    struct WhileLoopEntry wl;
    short  tempTag;
    short  stmtResult[4];   /* {type, int value}: value at +2 */

    ifDepth     = 0;
    forDepth    = 0;
    whileDepth  = 0;
    switchDepth = 0;

    for (;;)
    {
        opcode = *pInstr;

        DF_TRACE("stmt op=%d (0x%x) @pc=%d", (int)opcode, (int)(unsigned short)opcode,
                 (*(int *)((int)pCtx + 0x14) != 0)
                     ? (int)(((int)pInstr - *(int *)((int)pCtx + 0x14)) >> 3) : -1);

        switch (opcode)
        {
        default:
            if (opcode >= 12000 && opcode <= 0x3eb9)
            {
                err = DispatchActionCommand(pCtx, pInstr, pTable, &skipCount);
                if (err != 0)
                {
                    DF_TRACE("ABORT: action cmd %d failed, err=%d (0x%x)",
                             (int)opcode, (int)err, (int)err);
                    return err;
                }
                pInstr += skipCount * 4;
                break;
            }

            err = ExtractInstrIdentifier(pInstr, gScratchStr);
            if (err != 0)
            {
                DF_TRACE("ABORT: ExtractInstrIdentifier failed, err=%d", (int)err);
                return err;
            }

            pCur = pInstr + 4;
            if (*pCur == 0xfb2)
            {
                DF_TRACE("call '%.*s'", (int)gScratchStr[0], (char *)&gScratchStr[1]);
                err = EvalMethodCall((short *)pCtx, (void *)pCtx, pInstr, pTable,
                                     NULL, &skipCount);
                if (err != 0)
                {
                    DF_TRACE("ABORT: '%.*s' (%s) failed, err=%d",
                             (int)gScratchStr[0], (char *)&gScratchStr[1],
                             "method call", (int)err);
                    return err;
                }
                pInstr += skipCount * 4;
                break;
            }

            /* property assignment: local scope first, then global */
            err = LookupPropertyByName(pTable, gScratchStr, &tempTag, (int)pInstr);
            if (err != 0)
            {
                err = LookupPropertyByName((int *)gGlobalsNode, gScratchStr,
                                           &tempTag, (int)pInstr);
                if (err != 0)
                {
                    DF_TRACE("ABORT: property '%.*s' not found in local or global scope, err=%d",
                             (int)gScratchStr[0], (char *)&gScratchStr[1], (int)err);
                    return err;
                }
                if (*pCur != 0x1f48)
                    return 0x10;
                pInstr = pCur + 4;
                err = ProcessScriptBlock(pCtx, pInstr, pTable, (int *)stmtResult, &skipCount);
                if (err != 0)
                    return err;
                err = WritePropertyValue((int *)gGlobalsNode, tempTag, stmtResult);
                if (err != 0)
                {
                    DF_TRACE("ABORT: '%.*s' (%s) failed, err=%d",
                             (int)gScratchStr[0], (char *)&gScratchStr[1],
                             "assignment", (int)err);
                    return err;
                }
                pInstr += skipCount * 4;
                break;
            }
            if (*pCur != 0x1f48)
                return 0x10;
            pInstr = pCur + 4;
            err = ProcessScriptBlock(pCtx, pInstr, pTable, (int *)stmtResult, &skipCount);
            if (err != 0)
                return err;
            err = WritePropertyValue(pTable, tempTag, stmtResult);
            if (err != 0)
            {
                DF_TRACE("ABORT: '%.*s' (%s) failed, err=%d",
                         (int)gScratchStr[0], (char *)&gScratchStr[1],
                         "assignment", (int)err);
                return err;
            }
            pInstr += skipCount * 4;
            break;

        case 0xfa2:
            err = DeclareVariableList(pInstr, (int *)gGlobalsNode, &skipCount);
            if (err != 0)
                return err;
            pInstr += skipCount * 4;
            break;

        case 0xfa3:
            err = DeclareVariableList(pInstr, pTable, &skipCount);
            if (err != 0)
                return err;
            pInstr += skipCount * 4;
            break;

        case 0xfa4:
            if (ifDepth != 0)
                return 0x1d;
            if (forDepth != 0)
                return 0x1e;
            if (whileDepth != 0)
                return 0x25;
            if (switchDepth != 0)
                return 0x1f;
            /* 0 if no result is expected, 6 if the caller expected a return value */
            return (pResult == NULL) ? 0 : 6;

        case 0xfa5:
            gForDepth = gForDepth - forDepth;
            gWhileDepth = gWhileDepth - whileDepth;
            return (pResult == NULL) ? 0 : 6;

        case 0xfa6:
            pInstr += 4;
            err = ProcessScriptBlock(pCtx, pInstr, pTable, (int *)stmtResult, &skipCount);
            if (err != 0)
                return err;
            if (stmtResult[0] != 2)
            {
                DF_TRACE("ABORT: `if` condition is type %d, not bool (2); "
                         "value=%d", (int)stmtResult[0], *(int *)(stmtResult + 1));
                return 0x0e;
            }

            DF_TRACE("  if -> %s", (*(int *)(stmtResult + 1) == 0) ? "FALSE" : "TRUE");

            pInstr += skipCount * 4;
            if (*(int *)(stmtResult + 1) != 0)
            {
                ifDepth++;
                break;
            }

            err = FindElseBranchOrMarkNoElse(pInstr, &skipCount);
            if (err != 0)
                return err;
            if (skipCount >= 0)
            {
                pInstr += skipCount * 4;
                ifDepth++;
                break;
            }
            err = SkipIfFalseBranchToEndif(pInstr, &skipCount);
            if (err != 0)
                return err;
            pInstr += skipCount * 4;
            break;

        case 0xfa7:
            pInstr += 4;
            if (ifDepth <= 0)
                return 0x1d;
            ifDepth--;
            break;

        case 0xfa8:
            pInstr += 4;
            if (ifDepth <= 0)
                return 0x1d;
            ifDepth--;
            err = SkipIfFalseBranchToEndif(pInstr, &skipCount);
            if (err != 0)
                return err;
            pInstr += skipCount * 4;
            break;

        case 0xfa9:
            pInstr += 4;
            err = ProcessScriptBlock(pCtx, pInstr, pTable, (int *)stmtResult, &skipCount);
            if (err != 0)
                return err;
            if (stmtResult[0] != 4 && stmtResult[0] != 3)
                return 0x0e;
            pInstr += skipCount * 4;

            err = EvaluateSwitchFirstMatchingCase(pCtx, pInstr, pTable, stmtResult, &skipCount);
            if (err != 0)
                return err;
            if (skipCount < 0)
            {
                err = SkipSwitchBodyToEndswitch(pInstr, &skipCount);
                if (err != 0)
                    return err;
                pInstr += skipCount * 4;
                break;
            }
            pInstr += skipCount * 4;
            switchDepth++;
            break;

        case 0xfaa:
            pInstr += 4;
            if (switchDepth <= 0)
                return 0x1f;
            switchDepth--;
            break;

        case 0xfab:
            if (switchDepth <= 0)
                return 0x1f;
            switchDepth--;
            err = SkipSwitchBodyToEndswitch(pInstr, &skipCount);
            if (err != 0)
                return err;
            pInstr += skipCount * 4;
            break;

        case 0xfac:
            /* for <var> = <init> to <limit> [step <n>] */
            pInstr += 4;
            err = ExtractInstrIdentifier(pInstr, gScratchStr);
            if (err != 0)
                return err;
            err = DeclarePropertyFromBytecodeInstr(pTable, gScratchStr, &tempTag, (int)pInstr);
            if (err != 0)
                return err;
            rec.tag = tempTag;

            pInstr += 4;
            if (*pInstr != 0x1f48)
                return 0x10;

            pInstr += 4;
            err = ProcessScriptBlock(pCtx, pInstr, pTable, (int *)stmtResult, &skipCount);
            if (err != 0)
                return err;
            if (stmtResult[0] != 4)
                return 0x0e;
            pInstr += skipCount * 4;
            initVal = *(int *)(stmtResult + 1);
            err = WritePropertyValue(pTable, tempTag, stmtResult);
            if (err != 0)
                return err;

            if (*pInstr != 0xfad)
                return 0x20;
            pInstr += 4;
            err = ProcessScriptBlock(pCtx, pInstr, pTable, (int *)stmtResult, &skipCount);
            if (err != 0)
                return err;
            if (stmtResult[0] != 4)
                return 0x0e;
            pInstr += skipCount * 4;
            rec.limit = *(int *)(stmtResult + 1);

            stepVal = 1;
            if (*pInstr == 0xfae)
            {
                pInstr += 4;
                err = ProcessScriptBlock(pCtx, pInstr, pTable, (int *)stmtResult, &skipCount);
                if (err != 0)
                    return err;
                if (stmtResult[0] != 4)
                    return 0x0e;
                pInstr += skipCount * 4;
                stepVal = *(int *)(stmtResult + 1);
            }
            rec.loopback = pInstr;

            /* a loop whose initial value is already past the limit is skipped */
            if (stepVal > 0 && rec.limit < initVal)
            {
                err = SkipForBodyToNextStatement(pInstr, &skipCount);
                if (err != 0)
                    return err;
                pInstr += skipCount * 4;
                break;
            }
            if (stepVal < 0 && rec.limit > initVal)
            {
                err = SkipForBodyToNextStatement(pInstr, &skipCount);
                if (err != 0)
                    return err;
                pInstr += skipCount * 4;
                break;
            }

            forDepth++;
            rec.step = stepVal;
            ((struct ForLoopEntry *)gForStack)[gForDepth++] = rec;
            if (gForDepth >= 0x14)
                return 3;   /* loop stack full */
            break;

        case 0xfaf:
            pInstr += 4;
            if (forDepth <= 0)
                return 0x1e;
            if (gForDepth <= 0)
                return 0x1e;
            rec = ((struct ForLoopEntry *)gForStack)[gForDepth - 1];

            err = ReadPropertyValue(pTable, rec.tag, stmtResult);
            if (err != 0)
                return err;
            if (stmtResult[0] != 4)
                return 0x0e;
            *(int *)(stmtResult + 1) += rec.step;
            err = WritePropertyValue(pTable, rec.tag, stmtResult);
            if (err != 0)
                return err;

            /* if/else, not `?:`: the exe's blocks at 0x00447c44 are mov eax,0 / cmp / jg A / jmp B,
             * mov eax,0 / cmp / jl A, B: mov eax,1 */
            if (rec.step >= 0)
            {
                if (*(int *)(stmtResult + 1) > rec.limit)
                    initVal = 0;
                else
                    initVal = 1;
            }
            else
            {
                if (*(int *)(stmtResult + 1) < rec.limit)
                    initVal = 0;
                else
                    initVal = 1;
            }
            if (initVal != 0)
            {
                pInstr = rec.loopback;
            }
            else
            {
                forDepth--;
                gForDepth--;
            }
            break;

        case 0xfb0:
            pInstr += 4;
            wl.cond = pInstr;
            err = ProcessScriptBlock(pCtx, pInstr, pTable, (int *)stmtResult, &skipCount);
            if (err != 0)
                return err;
            if (stmtResult[0] != 2)
                return 0x0e;
            if (*(int *)(stmtResult + 1) != 0)
            {
                whileDepth++;
                ((struct WhileLoopEntry *)gWhileStack)[gWhileDepth++] = wl;
                if (gWhileDepth >= 0x14)
                    return 3;   /* loop stack full */
                pInstr += skipCount * 4;
                break;
            }
            err = SkipWhileBodyToWend(pInstr, &skipCount);
            if (err != 0)
                return err;
            pInstr += skipCount * 4;
            break;

        case 0xfb1:
            /* wend: re-evaluate the condition from the saved position */
            pInstr += 4;
            if (whileDepth <= 0)
                return 0x25;
            if (gWhileDepth <= 0)
                return 0x25;
            wl = ((struct WhileLoopEntry *)gWhileStack)[gWhileDepth - 1];
            err = ProcessScriptBlock(pCtx, wl.cond, pTable, (int *)stmtResult, &skipCount);
            if (err != 0)
                return err;
            if (stmtResult[0] != 2)
                return 0x0e;
            if (*(int *)(stmtResult + 1) != 0)
            {
                pInstr = wl.cond + skipCount * 4;
            }
            else
            {
                whileDepth--;
                gWhileDepth--;
            }
            break;

        case 0xfb8:
            pInstr += 4;
            if (pResult == NULL)
                return 5;
            err = ProcessScriptBlock(pCtx, pInstr, pTable, (int *)pResult, &skipCount);
            if (err != 0)
                return err;
            gForDepth = gForDepth - forDepth;
            gWhileDepth = gWhileDepth - whileDepth;
            return 0;

        case 0xfb9:
            gForDepth = gForDepth - forDepth;
            gWhileDepth = gWhileDepth - whileDepth;
            return (pResult == NULL) ? 4 : 6;

        case 0xfbc:
            err = UndeclareVariableList(pInstr, pTable, &skipCount);
            if (err != 0)
                return err;
            pInstr += skipCount * 4;
            break;

        case 0xfbd:
            err = UndeclareVariableList(pInstr, (int *)gGlobalsNode, &skipCount);
            if (err != 0)
                return err;
            pInstr += skipCount * 4;
            break;
        }

        if (*pInstr != 6)
        {
            DF_TRACE("ABORT: missing statement separator after op=%d, got %d",
                     (int)opcode, (int)*pInstr);
            return 0x1b;
        }

        while (*pInstr == 6)
            pInstr += 4;

        *(int *)((int)pCtx + 0x0c) = ((int)pInstr - *(int *)((int)pCtx + 0x14)) >> 3;

        if (gKeyAborts != 0 && IsEscapeKeyDown() != 0)
            return 0x35;
    }
}


/* Walks a list of formal parameters or declarations in the bytecode (8-byte entries, up to the 0xfb4
 * terminator): for each, ExtractInstrIdentifier puts the name in gScratchStr and
 * DeclarePropertyFromBytecodeInstr declares it in pScope. *pSkipOut receives the entries consumed.
 * Returns 0 or the failing status. */
short __cdecl DeclareVariableList(short *pFormalWalk, int *pScope, int *pSkipOut)  /* 0x004480a0 */
{
    short   st;
    short  *start;
    short   scratchTag;

    /* the parameter walks, one entry's argument field at a time (+8 bytes
     * past each header); the start is kept in a local, as in the skippers */
    start = pFormalWalk;
    pFormalWalk += 4;
    for (;;) {
        st = ExtractInstrIdentifier(pFormalWalk, (byte *)&gScratchStr);
        if (st != 0)
            return st;

        st = DeclarePropertyFromBytecodeInstr(pScope, (byte *)&gScratchStr,
                                              &scratchTag, (int)pFormalWalk);
        if (st != 0)
            return st;

        pFormalWalk += 4;
        if (*pFormalWalk != (short)0xfb4)
            break;
        pFormalWalk += 4;
    }
    *pSkipOut = ((int)(intptr_t)pFormalWalk - (int)(intptr_t)start) >> 3;
    return 0;
}

#undef local_20
#undef local_1e

/* DeclareVariableList's twin for opcodes 0xfbc / 0xfbd: walks the same list but binds each name
 * through UndeclarePropertyForInstr. */
short __cdecl UndeclareVariableList(short *pWalk, int *pScope, int *pSkipOut)  /* 0x00448110 */
{
    short  *start;              /* declared before the status: the walker gets ebx, the start edi */
    short   st;

    /* the parameter walks, as in DeclareVariableList */
    start = pWalk;
    pWalk += 4;
    for (;;) {
        st = ExtractInstrIdentifier(pWalk, (byte *)&gScratchStr);
        if (st != 0)
            return st;
        st = (short)UndeclarePropertyForInstr(pScope, (byte *)&gScratchStr, (int)pWalk);
        if (st != 0)
            return st;
        pWalk += 4;
        if (*pWalk != (short)0xfb4)
            break;
        pWalk += 4;
    }
    *pSkipOut = ((int)(intptr_t)pWalk - (int)(intptr_t)start) >> 3;
    return 0;
}


/* Scans from pInstr (8-byte entries), tracking if / endif (0xfa6 / 0xfa7) nesting; at depth 0 the
 * endif gives *pSkipOut, the entries from pInstr to it. Errors: 0x1b unexpected end, 0x1d a block
 * end (0xfa4) without its if. */
short __cdecl SkipIfFalseBranchToEndif(short *pInstr, int *pSkipOut)  /* 0x00448170 */
{
    short  depth = 0;                   /* depth first here: it gets ax, the start pointer ecx */
    short *start = pInstr;              /* the parameter itself walks; its start is the local */

    for (;;) {
        short op = *pInstr;
        if (op == 0)
            return 0x1b;
        if (op == 0xfa4)
            return 0x1d;
        if (op == 0xfa6) {
            depth++;
            pInstr += 4;
        } else {
            if (op == 0xfa7) {
                if (depth == 0) {
                    *pSkipOut = (int)(pInstr + 4) - (int)start >> 3;
                    return 0;
                }
                depth--;
            }
            pInstr += 4;
        }
    }
}


/* Scans from pInstr, tracking while / wend (0xfb0 / 0xfb1) nesting; at depth 0 the wend gives
 * *pSkipOut. Errors: 0x1b, 0x25 for a stray 0xfa4. */
short __cdecl SkipWhileBodyToWend(short *pInstr, int *pSkipOut)  /* 0x004481d0 */
{
    short  depth = 0;
    short *start = pInstr;              /* the parameter itself walks; its start is the local */

    for (;;) {
        short op = *pInstr;
        if (op == 0)
            return 0x1b;
        if (op == 0xfa4)
            return 0x25;
        if (op == 0xfb0) {
            depth++;
            pInstr += 4;
        } else {
            if (op == 0xfb1) {
                if (depth == 0) {
                    *pSkipOut = (int)(pInstr + 4) - (int)start >> 3;
                    return 0;
                }
                depth--;
            }
            pInstr += 4;
        }
    }
}


/* Scans from pInstr, tracking switch / endswitch (0xfa9 / 0xfaa) nesting; at depth 0 the endswitch
 * gives *pSkipOut. Errors: 0x1b, 0x1f for a stray 0xfa4. */
short __cdecl SkipSwitchBodyToEndswitch(short *pInstr, int *pSkipOut)  /* 0x00448230 */
{
    short *start = pInstr;              /* the parameter itself walks; its start is the local */
    short  depth = 0;

    for (;;) {
        short op = *pInstr;
        if (op == 0)
            return 0x1b;
        if (op == 0xfa4)
            return 0x1f;
        if (op == 0xfa9) {
            depth++;
            pInstr += 4;
        } else {
            if (op == 0xfaa) {
                if (depth == 0) {
                    *pSkipOut = (int)(pInstr + 4) - (int)start >> 3;
                    return 0;
                }
                depth--;
            }
            pInstr += 4;
        }
    }
}


/* Scans from pInstr, tracking for / next (0xfac / 0xfaf) nesting, for a loop that runs zero times;
 * at depth 0 the next gives *pSkipOut. */
short __cdecl SkipForBodyToNextStatement(short *pInstr, int *pSkipOut)  /* 0x00448290 */
{
    short *start = pInstr;
    short  depth = 0;

    for (;;) {
        short op = *pInstr;
        if (op == 0)
            return 0x1b;
        if (op == 0xfa4)
            return 0x1f;
        if (op == 0xfac) {
            depth++;
            pInstr += 4;
        } else {
            if (op == 0xfaf) {
                if (depth == 0) {
                    *pSkipOut = (int)(pInstr + 4) - (int)start >> 3;
                    return 0;
                }
                depth--;
            }
            pInstr += 4;
        }
    }
}


/* After a false if condition: scans for the else (0xfa8) at depth 0 and gives *pSkipOut to its body,
 * or, on reaching the endif (0xfa7) at depth 0 first, sets *pSkipOut to -1 (no else branch). */
short __cdecl FindElseBranchOrMarkNoElse(short *pInstr, int *pSkipOut)  /* 0x004482f0 */
{
    short  depth = 0;                   /* depth first: it gets ax, the start pointer ecx */
    short *start = pInstr;              /* the parameter itself walks; its start is the local */

    for (;;) {
        short op = *pInstr;
        if (op == 0)
            return 0x1b;
        if (op == 0xfa4)
            return 0x1d;
        if (op == 0xfa6) {
            depth++;
            pInstr += 4;
        } else if (op == 0xfa7) {
            if (depth == 0) {
                *pSkipOut = -1;
                return 0;
            }
            depth--;
            pInstr += 4;
        } else {
            if (op == 0xfa8 && depth == 0) {
                *pSkipOut = (int)(pInstr + 4) - (int)start >> 3;
                return 0;
            }
            pInstr += 4;
        }
    }
}


/* Scans the cases of a switch from pInstr for the first 0xfab whose expression equals pSwitchKey (a
 * string, type 3, or an int, type 4) and gives *pSkipOut, the entries from pInstr to the end of that
 * case's expression. *pSkipOut is -1 when the endswitch (0xfaa, depth 0) is reached with no match. */
short __cdecl EvaluateSwitchFirstMatchingCase(int pCtx, short *pInstr, int *pTable,
                                              short *pSwitchKey, int *pSkipOut)  /* 0x00448370 */
{
    short   err;
    short   switchNest;
    short  *pSwitchStart;
    byte    strKeyBuf[256];
    /* One 8-byte typed script value: short type at +0, 32-bit payload at +2. Comparing a whole int, or reading the
     * payload at +4, made integer switches never match and leaked a string slot per string case (class 3). */
    short   caseResult[4];
#define caseType   (caseResult[0])
#define caseValue  (*(unsigned int *)(caseResult + 1))
    int     slots;

    switchNest   = 0;
    pSwitchStart = pInstr;
    if (pSwitchKey[0] == 3)
    {
        err = TakeScriptString(pSwitchKey, strKeyBuf);
        if (err != 0)
            return err;
    }

    for (;;)
    {
        if (*pInstr == 0)
            return 0x1b;
        if (*pInstr == 0xfa4)
            return 0x1f;
        if (*pInstr == 0xfa9)
        {
            switchNest++;
            pInstr += 4;
            continue;
        }
        if (*pInstr == 0xfaa)
        {
            if (switchNest == 0)
            {
                *pSkipOut = -1;
                return 0;
            }
            switchNest--;
            pInstr += 4;
            continue;
        }
        if (*pInstr == 0xfab && switchNest == 0)
        {
            pInstr += 4;
            err = ProcessScriptBlock(pCtx, pInstr, pTable, (int *)caseResult, &slots);
            if (err != 0)
                return err;
            if (pSwitchKey[0] != caseType)
                return 0x0e;
            pInstr += slots * 4;
            if (caseType == 4 && caseValue == *(unsigned int *)(pSwitchKey + 1))
            {
                err = SkipFallThroughCaseLabels(pInstr, &slots);
                if (err != 0)
                    return err;
                pInstr += slots * 4;    /* advanced in place: the exe's value lands in eax */
                *pSkipOut = ((int)pInstr - (int)pSwitchStart) >> 3;
                return 0;
            }
            if (caseType == 3)
            {
                err = TakeScriptString(caseResult, gScratchStr);
                if (err != 0)
                    return err;
                if (PascalStringsEqualNoCase(gScratchStr, strKeyBuf) != 0)
                {
                    err = SkipFallThroughCaseLabels(pInstr, &slots);
                    if (err != 0)
                        return err;
                    pInstr += slots * 4;
                    *pSkipOut = ((int)pInstr - (int)pSwitchStart) >> 3;
                    return 0;
                }
            }
        }
        pInstr += 4;
    }
#undef caseType
#undef caseValue
}


/* After a matched case, skips the fall-through case labels so execution starts at the arm's body.
 * pAfterCaseExpr is the separator (6) closing the matched case expression; from there separators are
 * skipped and, while the next opcode is another 0xfab, that case's expression too. Returns the
 * offset in 8-byte entries of the last separator reached: InterpretStatements lands on it, sees the
 * 6 it requires after a statement and runs the body, so `case a: case b: body` runs body for a.
 *
 * Port note: the first body was invented (class 12) and jumped to the next case; story in bug-
 * instances.md. */
short __cdecl SkipFallThroughCaseLabels(short *pAfterCaseExpr, int *pWordCountOut)  /* 0x00448550 */
{
    short *pStart;
    short *p;
    short  op;

    pStart = pAfterCaseExpr;
    p = pAfterCaseExpr;
    for (;;) {
        if (*p != 6)
            return 0x1b;
        while (*p == 6)
            p += 4;
        if (*p != 0xfab)
            break;
        /* Another case label: skip its expression to the next separator */
        for (;;) {
            op = p[4];
            p += 4;
            if (op == 0)
                return 0x1b;
            if (op == 6)
                break;
        }
        pAfterCaseExpr = p;     /* the last separator reached */
    }
    *pWordCountOut = ((int)pAfterCaseExpr - (int)pStart) >> 3;
    return 0;
}


/* Finds the distance to the next 0xfa1 code block after pBlock, in 8-byte entries, caching it in
 * pBlock[1]: *pSkip is the cached value if set, else the walk forward to an 0xfa1 or the end (opcode
 * 0, stored as -1). */
short __cdecl AdvanceToNextMethodBlock(short *pBlock, int *pSkip)  /* 0x004485b0 */
{
    int   *pCached;
    short *pStart;

    if (*pBlock != 0x0fa1)
        FatalError(0, 0x0fc9);
    pCached = (int *)(pBlock + 1);
    if (*pCached != 0) {
        *pSkip = *pCached;
        return 0;
    }
    pStart = pBlock;
    for (pBlock += 4; ; pBlock += 4) {
        if (*pBlock == 0) {
            *pCached = -1;
            *pSkip = -1;
            return 0;
        }
        if (*pBlock == 0x0fa1) {
            *pCached = ((int)pBlock - (int)pStart) >> 3;
            *pSkip = *pCached;
            return 0;
        }
    }
}


/* Classifies the call's identifier against the message names (g_inputChannelTable, 27 entries) and
 * caches the answer in pInstr[3]: 0 unclassified, 1 not a message name, 2 a message name. Returns 1
 * for a message name, else 0. */
short __cdecl ClassifyMethodIdentifier(short *pInstr)  /* 0x00448620 */
{
    uint   result;
    InputChannelEntry *pEntry;
    short  count;

    result = ExtractInstrIdentifier(pInstr, &gScratchStr);
    if ((short)result != 0)
    {
        pInstr[3] = 1;
        return 0;
    }

    if (pInstr[3] != 0)
        return pInstr[3] - 1;

    /* The exe walks a pointer over the packed 6-byte entries with a down-counter. Step with `pEntry++`, never a
     * literal 6: the GCC build once padded the entry to 8 and a literal landed mid-entry. */
    for (pEntry = g_inputChannelTable, count = 0x1b; count > 0; count--, pEntry++)
    {
        result = PascalStringsEqualNoCase((byte *)pEntry->pName, (byte *)&gScratchStr);
        if ((short)result != 0)
        {
            pInstr[3] = 2;
            return 1;
        }
    }

    pInstr[3] = 1;
    return 0;
}
