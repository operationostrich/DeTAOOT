/* script_block_parser.c -- TI.EXE 0x004058a0..0x004065f0: the script string slot table and
 * the script-block instruction parser. */

#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "df_protos.h"
#include "topics/sound.h"
#include "topics/multimedia.h"

static int DfTracePascalLooksWrong(const byte *s);


/* Allocates the script string slot table (20 slots x 256 bytes) and clears the occupied flags.
 * Counterpart of FreeScriptStringSlots; called by BeginGameSession. */
void __stdcall AllocScriptStringSlots(void)  /* 0x004058a0 */
{
    short i;

    gStringSlots = AllocEngineBuffer(0x1400);

    for (i = 0; i < 0x14; i++)
    {
        gStringSlotUsed[i] = 0;
    }
}


/* Frees the slot table that AllocScriptStringSlots made. */
void __stdcall FreeScriptStringSlots(void)  /* 0x004058d0 */
{
    FreeEngineBuffer((void *)gStringSlots);
}


/* Clears the 20 slot occupancy flags (gStringSlotUsed) and gScriptWorkQueueTop. */
void __stdcall ResetScriptStringSlots(void)  /* 0x004058e0 */
{
    short i;

    gScriptWorkQueueTop = 0;
    for (i = 0; i < 0x14; i++)
        gStringSlotUsed[i] = 0;
}


#if DF_TRACE_ENABLED
/* PORT-HELPER: trace check used by AllocScriptString. A Pascal string whose own characters are
 * unprintable is a stale or short-copied buffer, not a name a script wrote. Checking it where a
 * string ENTERS a slot / the property heap names the producer; checking it at the reader only
 * shows how far it has already spread. */
static int DfTracePascalLooksWrong(const byte *s)
{
    int i;
    for (i = 1; i <= (int)s[0]; i++)
        if (s[i] < 0x20 || s[i] > 0x7e)
            return 1;
    return 0;
}
#endif

/* Copies a Pascal string (CopyPascalString) into the first free slot of the script string table and
 * marks it occupied. Writes a type-3 (string) script value to pResult with the slot index as its
 * value. Returns 0, or 0x36 if no slot is free. */
short __cdecl AllocScriptString(byte *pDesc, short *pResult)  /* 0x00405900 */
{
    short slot;
    int   idx;

    slot = 0;
    do
    {
        if (gStringSlotUsed[slot] == 0)
        {
            idx = (int)slot;
#if DF_TRACE_ENABLED
            if (DfTracePascalLooksWrong(pDesc))
                DF_TRACE("!! allocStr: MALFORMED slot=%d len=%d '%.*s'",
                         idx, (int)pDesc[0], (int)pDesc[0], (const char *)pDesc + 1);
#endif
            CopyPascalString(pDesc, (byte *)gStringSlots + idx * 0x100);
            *pResult       = 3;     /* type: string */
            pResult[3]     = 0;     /* extra: clear */
            *(int *)(pResult + 1) = idx;  /* value: slot index */
            gStringSlotUsed[idx] = 1;
            return 0;               /* xor ax,ax; Ghidra's `uVar2 & 0xffff0000` was the copy's dead status */
        }
        slot++;
    } while (slot < 0x14);

    return 0x36;   /* no free descriptor slot (mov ax,0x36) */
}


/* Copies the 256-byte descriptor out of the slot named by pChannel[1] (0..19) into pOut and frees
 * the slot. *pChannel must be 3 (a string value), else it returns 0x0e. Fatal 0xfc6 if the slot
 * is out of range, 0xfc7 if it is empty. */
short __cdecl TakeScriptString(short *pChannel, byte *pOut)  /* 0x00405960 */
{
    short slot;

    if (*pChannel != 3)
    {
        return 0x0e;                 /* wrong channel type (mov ax,0xe) */
    }

    slot = pChannel[1];

    if ((slot < 0) || (slot >= 20))     /* cmp si,0x14 / jl */
    {
        FatalError(0, 0xfc6);        /* slot index out of range */
    }

    if (gStringSlotUsed[slot] == 0)
    {
        FatalError(0, 0xfc7);        /* slot is empty */
    }

    CopyPascalString((byte *)gStringSlots + slot * 0x100, pOut);
    gStringSlotUsed[slot] = 0;

    return 0;
}


/* Extracts the Pascal string of a type-5 (identifier) instruction into pOut; the same as
 * ExtractInstrString, which takes opcode 3 (string literal). Returns 0, or 0x0e if the
 * instruction is not type 5. */
short __cdecl ExtractInstrIdentifier(short *pInstr, byte *pOut)  /* 0x004059e0 */
{
    byte *pName;

    if (*pInstr != 5)
    {
        return 0x0e;   /* type mismatch */
    }

    pName = (byte *)(*(int *)(pInstr + 1) + (int)pInstr);   /* formed before pOut is loaded */
    CopyPascalString(pName, pOut);
    return 0;
}


/* Processes a block of script instructions from a bytecode stream. Each 8-byte entry is parsed by
 * EvalScriptTerm and pushed on the global work queue (gScriptWorkQueue, at most 40 entries); each
 * operation takes two queue slots (the raw entry and its parsed result), so the priority sweep
 * steps by 2. Opcodes 8000-8014 chain further instructions into the block. At a terminator (any
 * other opcode) the queued operations are resolved in priority order 0-6 by ResolveQueuedOperation,
 * the first entry's result pair is copied to pResultOut and the consumed entry count stored in
 * *pCountOut.
 * pCtx is the script execution context (+0x0c current instruction index, written; +0x14 stream
 * base), pInstr the current position, pTable the lookup table passed to EvalScriptTerm.
 * Returns 0, 0x2c (insufficient stack: GetCallBudget < 0x800), 3 (work queue overflow, over 39
 * entries), or an error propagated from EvalScriptTerm or ResolveQueuedOperation. */
short __cdecl ProcessScriptBlock(char *pCtx, short *pInstr, int *pTable,  /* 0x00405a10 */
                                int *pResultOut, int *pCountOut)
{
    typedef struct { int a, b; } QEntry;
    QEntry *pFirst;
    short   status;
    int     d;
    short   priority;
    QEntry  result;
    short   savedTop[2];    /* [0] only: the exe gives it a 4-byte slot of its own (0x1c), apart from priority */
    int     skipCount;
    short  *pStart;
    short   idx;
    int     i;
    QEntry *pE;

    if (GetCallBudget() < 0x800)
        return 0x002c;
    pStart = pInstr;
    status = EvalScriptTerm(pCtx, pInstr, pTable, (short *)&result, &skipCount);
    if (status != 0)
        return status;
    /* a QEntry pointer difference; the exe reads skipCount before the store (0x00405a5f) */
    d = (QEntry *)pInstr - *(QEntry **)(pCtx + 0x14);
    pInstr += skipCount * 4;
    *(int *)(pCtx + 0x0c) = d;
    pFirst = (QEntry *)gScriptWorkQueue + gScriptWorkQueueTop;
    *pFirst = result;
    savedTop[0] = gScriptWorkQueueTop + 1;
    gScriptWorkQueueTop = savedTop[0];
    if (savedTop[0] >= 0x28)
        return 0x0003;
    while (*pInstr >= 8000 && *pInstr <= 8014) {
        ((QEntry *)gScriptWorkQueue)[gScriptWorkQueueTop++] = *(QEntry *)pInstr;
        if (gScriptWorkQueueTop >= 0x28)
            return 0x0003;
        *(int *)(pCtx + 0x0c) = (QEntry *)pInstr - *(QEntry **)(pCtx + 0x14);
        pInstr += 4;
        status = EvalScriptTerm(pCtx, pInstr, pTable, (short *)&result, &skipCount);
        if (status != 0)
            return status;
        d = (QEntry *)pInstr - *(QEntry **)(pCtx + 0x14);
        pInstr += skipCount * 4;
        *(int *)(pCtx + 0x0c) = d;
        ((QEntry *)gScriptWorkQueue)[gScriptWorkQueueTop++] = result;
        if (gScriptWorkQueueTop >= 0x28)
            return 0x0003;
    }
    for (priority = 0; priority < 7; priority++) {
        for (idx = savedTop[0]; idx < gScriptWorkQueueTop; idx += 2) {
            i = idx;
            pE = (QEntry *)gScriptWorkQueue + i;
            result = *pE;
            if (GetInstructionPriority((short)result.a) == priority) {
                status = ResolveQueuedOperation(idx, (short)result.a);
                if (status != 0)
                    return status;
                idx -= 2;
                gScriptWorkQueueTop -= 2;
                memmove(pE, (QEntry *)gScriptWorkQueue + i + 2, (gScriptWorkQueueTop - i) * 8);
            }
        }
    }
    *(QEntry *)pResultOut = *pFirst;
    *pCountOut = ((int)pInstr - (int)pStart) >> 3;
    gScriptWorkQueueTop--;
    return 0;
}


/* One script work-queue entry as ResolveQueuedOperation copies it: 8 bytes, 2-packed. */
#pragma pack(push, 2)
typedef struct {
    short type;     /* +0: 2 = bool, 3 = string descriptor, 4 = int */
    int   value;    /* +2 */
    short extra;    /* +6: passthrough */
} QOperand;
#pragma pack(pop)

/* Evaluates a binary operation on two operands of the script work queue: the operator is at queue
 * index queueIdx, the left operand at queueIdx - 1, the right at queueIdx + 1, and the result
 * overwrites the left slot. The opcodes are 0x1f41 + 0..12: ADD, SUB, MUL, DIV, AND, OR,
 * STR_CONCAT, EQ, NEQ, GT, LT, GEQ, LEQ. Returns 0, or 0x0e (type mismatch), 0x37 (divide by
 * zero), 0x1a (string too long) or an error from a helper.
 *
 * The exe copies both entries (QOperand) to the stack, works on the copies and stores the left
 * copy back at the shared exit. */
short __cdecl ResolveQueuedOperation(short queueIdx, short opcode)  /* 0x00405c50 */
{
    QOperand *pLeft;
    QOperand  a;
    QOperand  b;
    short     status;

    pLeft = (QOperand *)gScriptWorkQueue + (queueIdx - 1);
    a = *pLeft;
    b = ((QOperand *)gScriptWorkQueue)[queueIdx + 1];

    switch (opcode) {
    case 0x1f41:    /* ADD */
        if (a.type != 4) return 0x0e;
        if (b.type != 4) return 0x0e;
        a.value += b.value;
        break;

    case 0x1f42:    /* SUBTRACT */
        if (a.type != 4) return 0x0e;
        if (b.type != 4) return 0x0e;
        a.value -= b.value;
        break;

    case 0x1f43:    /* MULTIPLY */
        if (a.type != 4) return 0x0e;
        if (b.type != 4) return 0x0e;
        a.value = b.value * a.value;
        break;

    case 0x1f44:    /* DIVIDE (0x00405d6c CDQ / IDIV: signed) */
        if (a.type != 4) return 0x0e;
        if (b.type != 4) return 0x0e;
        if (b.value == 0) return 0x37;
        a.value = a.value / b.value;
        break;

    case 0x1f45:    /* AND */
        if (a.type != 2) return 0x0e;
        if (b.type != 2) return 0x0e;
        a.value &= b.value;
        break;

    case 0x1f46:    /* OR */
        if (a.type != 2) return 0x0e;
        if (b.type != 2) return 0x0e;
        a.value |= b.value;
        break;

    case 0x1f47:    /* STRING CONCAT */
        status = TakeScriptString(&a.type, gScratchStr);
        if (status != 0) return status;
        status = TakeScriptString(&b.type, gScratchStr2);
        if (status != 0) return status;
        if (gScratchStr2[0] + gScratchStr[0] > 0xff)     /* JLE: int */
            return 0x1a;
        AppendPascalString(gScratchStr2, gScratchStr);
        /* into the copy: on failure (0x36, nothing written) the queue keeps the old entry */
        status = AllocScriptString(gScratchStr, &a.type);
        if (status != 0) return status;
        break;

    case 0x1f48:    /* EQUAL */
        if (b.type != a.type) {
            /* Mixed types compare unequal. A type-3 operand's string slot is
             * NOT released on this path in TI.EXE (the Mar 1997 build adds
             * that release); the trace shows
             * whether any shipped script ever gets here with a string. */
            if (a.type == 3 || b.type == 3)
                DF_TRACE("!! ResolveQueuedOperation: EQUAL on mixed types L=%d R=%d "
                         "-- string slot not released (December behaviour)",
                         a.type, b.type);
            a.value = 0;
            a.type = 2;
        }
        else if (a.type == 4 || a.type == 2) {
            if (b.value == a.value) a.value = 1; else a.value = 0;
            a.type = 2;
        }
        else if (a.type == 3) {
            status = TakeScriptString(&a.type, gScratchStr);
            if (status != 0) return status;
            status = TakeScriptString(&b.type, gScratchStr2);
            if (status != 0) return status;
            a.value = PascalStringsEqualNoCase(gScratchStr, gScratchStr2);
            a.type = 2;
        }
        else {
            return 0x0e;
        }
        break;

    case 0x1f49:    /* NOT EQUAL */
        if (b.type != a.type) {
            if (a.type == 3 || b.type == 3)
                DF_TRACE("!! ResolveQueuedOperation: NOT-EQUAL on mixed types L=%d R=%d "
                         "-- string slot not released (December behaviour)",
                         a.type, b.type);
            a.value = 1;
            a.type = 2;
        }
        else if (a.type == 4 || a.type == 2) {
            if (b.value == a.value) a.value = 0; else a.value = 1;
            a.type = 2;
        }
        else if (a.type == 3) {
            status = TakeScriptString(&a.type, gScratchStr);
            if (status != 0) return status;
            status = TakeScriptString(&b.type, gScratchStr2);
            if (status != 0) return status;
            a.value = (PascalStringsEqualNoCase(gScratchStr, gScratchStr2) == 0);
            a.type = 2;
        }
        else {
            return 0x0e;
        }
        break;

    /* The four relational operators are SIGNED (JL, JG, JLE, JGE at 0x00406017..0x004060fb).
     * Ghidra rendered the operands as uint and the port once compared unsigned, so delt < 0 was
     * false for -3: the radio tuning knob could only turn the needle one way. */
    case 0x1f4a:    /* GREATER THAN (right < left) */
        if (b.type != a.type) return 0x0e;
        if (a.type == 4) {
            if (b.value < a.value) a.value = 1; else a.value = 0;
            a.type = 2;
        }
        else
            return 0x0e;
        break;

    case 0x1f4b:    /* LESS THAN (left < right) */
        if (b.type != a.type) return 0x0e;
        if (a.type == 4) {
            if (b.value > a.value) a.value = 1; else a.value = 0;
            a.type = 2;
        }
        else
            return 0x0e;
        break;

    case 0x1f4c:    /* GREATER OR EQUAL (right <= left) */
        if (b.type != a.type) return 0x0e;
        if (a.type == 4) {
            if (b.value <= a.value) a.value = 1; else a.value = 0;
            a.type = 2;
        }
        else
            return 0x0e;
        break;

    case 0x1f4d:    /* LESS OR EQUAL (left <= right) */
        if (b.type != a.type) return 0x0e;
        if (a.type == 4) {
            if (b.value >= a.value) a.value = 1; else a.value = 0;
            a.type = 2;
        }
        else
            return 0x0e;
        break;
    }

    *pLeft = a;
    return 0;
}


/* Returns the evaluation priority (0-6) of a script binary operator
 * opcode.  Lower numbers are evaluated first (higher precedence).
 *
 *   Priority 0: multiply (0x1f43), divide (0x1f44)
 *   Priority 1: add (0x1f41), subtract (0x1f42)
 *   Priority 2: string concat (0x1f47)
 *   Priority 3: relational  (0x1f4a--0x1f4d)
 *   Priority 4: equality    (0x1f48, 0x1f49)
 *   Priority 5: logical AND (0x1f45)
 *   Priority 6: logical OR  (0x1f46)
 *
 * Fatal error 0xfc8 if the opcode is not a known binary operator. */
short __cdecl GetInstructionPriority(short opcode)  /* 0x00406150 */
{
    switch (opcode) {
    case 0x1f41:
    case 0x1f42:
        return 1;
    case 0x1f43:
    case 0x1f44:
        return 0;
    case 0x1f45:
        return 5;
    case 0x1f46:
        return 6;
    case 0x1f47:
        return 2;
    case 0x1f48:
    case 0x1f49:
        return 4;
    case 0x1f4a:
    case 0x1f4b:
    case 0x1f4c:
    case 0x1f4d:
        return 3;
    default:
        FatalError(0, 0xfc8);
        return -1;   /* unreachable */
    }
}
/* Evaluates one script expression at pInstr into pResult and reports in
 * pSkipCount how many 8-byte entries it consumed. One switch on the opcode
 * (the exe's compare tree: pivot 0xfb2, a jump table for 0xfb5..0xfbb):
 *   0xfb2   -- sub-block (ProcessScriptBlock; must end in 0xfb3)
 *   3       -- string literal
 *   4       -- integer literal
 *   0xfb5   -- boolean TRUE
 *   0xfb6   -- boolean FALSE
 *   0xfb7   -- boolean NOT (unary)
 *   0xfba   -- context field A (pCtx + 0x1e)
 *   0xfbb   -- context field B (pCtx + 0x3e)
 *   0x1f42  -- unary minus (integer)
 *   default -- 16000..20122 is a command (DispatchScriptCommand); anything
 *              else is an identifier: a method call when a block follows,
 *              else a property of pTable, else of gGlobalsNode
 * Every skip count is spelled as the exe computes it: the byte distance
 * from pInstr to the next entry, >> 3 (SKIP_TO).
 * Returns 0 on success; 2 = block not closed, 0xe = operand type error. */
short __cdecl EvalScriptTerm(char *pCtx, short *pInstr, int *pTable,  /* 0x004061e0 */
                                    short *pResult, int *pSkipCount)
{
    short  status;
    short *pNext;
    short  opcode;
    int    subSkip;
    short  localTag;
    short *pAfter;
    int    tmp;
#define SKIP_TO(p) ((int)(p) - (int)pInstr) >> 3

    opcode = *pInstr;
    switch (opcode) {
    default:
        if (opcode >= 16000 && opcode <= 0x4e9a) {
            status = DispatchScriptCommand((int *)pCtx, pInstr, pTable, pResult, &subSkip);
            if (status != 0)
                return status;
            *pSkipCount = SKIP_TO(pInstr + subSkip * 4);
            return 0;
        }
        status = ExtractInstrIdentifier(pInstr, gScratchStr);
        if (status != 0) {
            DF_TRACE("!! EvalScriptTerm: token %d is not a usable "
                     "identifier, err=%d", (int)opcode, (int)status);
            return status;
        }
        DF_TRACE("expr id '%.*s'%s", (int)gScratchStr[0], (char *)&gScratchStr[1],
                 (pInstr[4] == (short)0xfb2) ? " (call)" : "");
        pNext = pInstr + 4;
        if (*pNext == 0xfb2) {
            /* two loads of pCtx, not one shared value: the exe reads it after
             * the other arguments (0x004062ae); `*(short **)&pCtx` keeps the
             * compiler from hoisting a common load */
            status = EvalMethodCall(*(short **)&pCtx, (void *)pCtx, pInstr, pTable, (void *)pResult, &subSkip);
            if (status != 0)
                return status;
            *pSkipCount = SKIP_TO(pInstr + subSkip * 4);
            return 0;
        }
        tmp = LookupPropertyByName(pTable, gScratchStr, &localTag, (int)pInstr);
        if ((short)tmp != 0) {
            /* not a property of the table: the subsystem context */
            status = LookupPropertyByName(gGlobalsNode, gScratchStr, &localTag, (int)pInstr);
            if (status != 0)
                return status;
            status = ReadPropertyValue(gGlobalsNode, localTag, pResult);
            if (status != 0)
                return status;
            *pSkipCount = SKIP_TO(pNext);
            return 0;
        }
        status = ReadPropertyValue(pTable, localTag, pResult);
        if (status != 0)
            return status;
        *pSkipCount = SKIP_TO(pNext);
        return 0;
    case 3:                                 /* string literal */
        status = ExtractInstrString(pInstr, gScratchStr);
        if (status != 0)
            return status;
        status = AllocScriptString(gScratchStr, pResult);
        if (status != 0)
            return status;
        *pSkipCount = SKIP_TO(pInstr + 4);
        return 0;
    case 4:                                 /* integer literal */
        tmp = *(int *)(pInstr + 1);
        *pResult = 4;
        *(int *)(pResult + 1) = tmp;
        *pSkipCount = SKIP_TO(pInstr + 4);
        return 0;
    case 0xfb2:                             /* sub-block */
        pNext = pInstr + 4;
        status = ProcessScriptBlock(pCtx, pNext, pTable, (int *)pResult, &subSkip);
        if (status != 0)
            return status;
        pAfter = pNext + subSkip * 4;
        if (*pAfter != 0xfb3) {
            DF_TRACE("!! err2 EvalScriptTerm: sub-block not closed "
                     "(subSkip=%d, got %d at %p)", subSkip, (int)*pAfter, pAfter);
            return 2;
        }
        *pSkipCount = SKIP_TO(pAfter + 4);
        return 0;
    case 0xfb5:                             /* TRUE */
        *pResult = 2;
        *(int *)(pResult + 1) = 1;
        *pSkipCount = SKIP_TO(pInstr + 4);
        return 0;
    case 0xfb6:                             /* FALSE */
        *pResult = 2;
        *(int *)(pResult + 1) = 0;
        *pSkipCount = SKIP_TO(pInstr + 4);
        return 0;
    case 0xfb7:                             /* NOT */
        pNext = pInstr + 4;
        status = EvalScriptTerm(pCtx, pNext, pTable, pResult, &subSkip);
        if (status != 0)
            return status;
        if (*pResult != 2)
            return 0xe;                     /* type error: NOT wants a bool */
        *(unsigned int *)(pResult + 1) ^= 1;
        *pSkipCount = SKIP_TO(pNext + subSkip * 4);
        return 0;
    case 0xfba:                             /* context field A */
        status = AllocScriptString((byte *)(pCtx + 0x1e), pResult);
        if (status != 0)
            return status;
        *pSkipCount = SKIP_TO(pInstr + 4);
        return 0;
    case 0xfbb:                             /* context field B */
        status = AllocScriptString((byte *)(pCtx + 0x3e), pResult);
        if (status != 0)
            return status;
        *pSkipCount = SKIP_TO(pInstr + 4);
        return 0;
    case 0x1f42:                            /* unary minus */
        pNext = pInstr + 4;
        status = EvalScriptTerm(pCtx, pNext, pTable, pResult, &subSkip);
        if (status != 0)
            return status;
        if (*pResult != 4)
            return 0xe;                     /* type error: minus wants an int */
        *(int *)(pResult + 1) = -*(int *)(pResult + 1);
        *pSkipCount = SKIP_TO(pNext + subSkip * 4);
        return 0;
    }
#undef SKIP_TO
}


/* Extracts the Pascal string of a string-literal instruction (opcode 3) into pOut. The 4-byte
 * payload is the byte offset from the instruction to the string data. Returns 0, or 0x0e if the
 * instruction is not type 3. */
short __cdecl ExtractInstrString(short *pInstr, byte *pOut)  /* 0x004065f0 */
{
    int rawOffset;
    byte *pSrcString;

    if (*pInstr != 3)
    {
        return 0x0e;   /* type mismatch */
    }

    rawOffset = *(int *)(pInstr + 1);
    pSrcString = (byte *)(rawOffset + (int)pInstr);

    /* A literal that still contains a quote character is the "captured raw source text" defect
     * (a port-bug story): flagged loudly so it is greppable. */
#if DF_TRACE_ENABLED
    {
        int  n = (int)pSrcString[0];
        int  i;
        int  hasQuote = 0;
        for (i = 1; i <= n; i++)
            if (pSrcString[i] == '"') { hasQuote = 1; break; }

        DF_TRACE("%sExtractInstrString: pInstr=%p ofs=0x%x src=%p len=%d '%.*s'",
                 hasQuote ? "!! MALFORMED " : "", pInstr, rawOffset, pSrcString,
                 n, n, (char *)&pSrcString[1]);
    }
#endif

    CopyPascalString(pSrcString, pOut);
    return 0;
}

