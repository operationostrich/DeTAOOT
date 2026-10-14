/* property_heap.c -- TI.EXE 0x0044dc70..0x0044e2e0, the last game code before the C runtime: property
 * tables (method scopes, the globals scope) and their string heap. */

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


/* Creates the globals scope, an empty property table, in gGlobalsNode; called from
 * BeginGameSession. TeardownGlobalsScope frees it. */
void __stdcall InitGlobalsScope(void)  /* 0x0044dc70 */
{
    gGlobalsNode = (int)AllocMethodExecutionScopeResource();
}


/* Frees the globals scope InitGlobalsScope created. */
void __stdcall TeardownGlobalsScope(void)  /* 0x0044dc80 */
{
    UninstallResourcePair((void *)gGlobalsNode);
}


/* Allocates an empty property table: a method call's scope (ExecuteMethodBody) or the
 * globals scope. The table is one resource, a 0x14-byte header and then 32-byte entries:
 *   header +0 short count, +2 short capacity (20 here, so 0x294 bytes), +4 int bytes of the
 *          string heap freed since the last compaction, +8 int heap bytes used, +0xc int heap
 *          capacity (0x800), +0x10 the string heap's own resource handle;
 *   entry  +0 short type (3 = string, its value a heap offset), +2 int value (unaligned),
 *          +0x10 the name, a Pascal string of at most 15 characters.
 * UninstallResourcePair frees it. */
int *__cdecl AllocMethodExecutionScopeResource(void) /* 0x0044dc90 */
{
    short *ps;                          /* declared before hR: ps gets esi, hR edi */
    int   *hR;
    int   *hChild;

    hR = (int *)AllocResObject(0x294);
    ps = (short *)GetResDataPtr(hR);
    ps[0] = 0;
    ps[1] = 0x14;
    *(unsigned int *)((char *)ps + 8)  = 0;   /* +8 before +4, as the exe stores */
    *(unsigned int *)((char *)ps + 4)  = 0;
    *(unsigned int *)((char *)ps + 12) = 0x800;
    hChild = (int *)AllocResObject(0x800);
    *(int **)((char *)ps + 16) = hChild;
    UnlockResObject(hR);
    return hR;
}


/* Reads property tag (an entry index) of the table pObj into the 8-byte script value
 * pResult; a string property comes back as a new script string. Returns 0, 9 when tag is out
 * of range, or AllocScriptString's error. */
short __cdecl ReadPropertyValue(int *pObj, short tag, short *pResult)  /* 0x0044dcf0 */
{
    /* One 32-byte property entry, copied whole (rep movsd, ecx = 8); its
     * type word and 2-aligned value are read as fields. */
#pragma pack(push, 2)
    typedef struct { short type; int value; byte rest[26]; } PropSlotT;
#pragma pack(pop)
    short    *pData;
    short     status;
    PropSlotT slot;

    pData = (short *)GetResDataPtr(pObj);

    if ((tag < *pData) && (tag >= 0))
    {
        slot = *(PropSlotT *)(pData + tag * 0x10 + 10);

        *pResult = slot.type;
        *(int *)(pResult + 1) = slot.value;
        pResult[3] = 0;

        DF_TRACE("prop[tag %d] '%.*s' type=%d value=%d (count=%d)", (int)tag,
                 (int)slot.rest[10], (char *)&slot.rest[11],
                 (int)slot.type, slot.value, (int)*pData);

        if (slot.type == 3)
        {
            CopyPropertyHeapStringToBuffer((int)(intptr_t)pData, slot.value,
                                          (byte *)&gScratchStr);
            DF_TRACE("prop[tag %d] heap@%d used=%d -> '%.*s'", (int)tag, slot.value,
                     *(int *)((char *)pData + 8), (int)gScratchStr[0], (const char *)&gScratchStr[1]);
            status = AllocScriptString((byte *)&gScratchStr, pResult);
            if (status != 0)
            {
                UnlockResObject(pObj);
                return status;
            }
        }

        UnlockResObject(pObj);
        return 0;
    }

    UnlockResObject(pObj);
    return 9;
}


/* Returns the NAME of property tag of the table hProps as a new script string in
 * pOutValue. Returns 0, 9 when tag is out of range, or AllocScriptString's error. */
short __cdecl ReadPropertyNameByTag(int *hProps, short tag, short *pOutValue)  /* 0x0044ddb0 */
{
    typedef struct { byte b[32]; } PropSlot;   /* copied whole: rep movsd, ecx = 8 */
    short   *pTable;
    short    st;
    /* ONE 32-byte entry at ebp-0x20 (Ghidra's local_20[4] and propName[16]); as two arrays
     * the copy overran and the name was stale (port-bug-classes.md #3). */
    PropSlot slot;
#define propName (slot.b + 0x10)  /* local_10 */

    pTable = (short *)GetResDataPtr(hProps);

    if ((*pTable <= tag) || (tag < 0)) {
        UnlockResObject(hProps);
        return 9;
    }

    slot = *(PropSlot *)(pTable + tag * 0x10 + 10);

    DF_TRACE("entryKey: tag=%d name len=%d '%.*s'", (int)tag,
             (int)propName[0], (int)propName[0], (const char *)propName + 1);

    st = AllocScriptString(propName, pOutValue);
    if (st != 0) {
        UnlockResObject(hProps);
        return st;
    }

    UnlockResObject(hProps);
    return 0;
#undef propName
}


/* Stores the script value pValue in property tag of the table hProps. A string value is
 * interned in the table's string heap; an old string's bytes are counted as freed, and once
 * 0x800 bytes are freed the heap is compacted. Returns 0, or 9 when tag is out of range. */
short __cdecl WritePropertyValue(int *hProps, short tag, short *pValue)  /* 0x0044de40 */
{
    /* The 32-byte entry is copied in and out whole (`rep movsd`, ecx = 8): type is Ghidra's
     * local_20, value its local_1e. As two loose locals the copy overflowed the stack (#3). */
#pragma pack(push, 2)
    typedef struct { short type; int value; short rest[13]; } PropSlot;   /* 32 bytes */
#pragma pack(pop)
    short    *pTable;
    PropSlot *pSlot;
    PropSlot  slot;

    pTable = (short *)GetResDataPtr(hProps);
    DF_TRACE("WriteProp obj=%p data=%p tag=%d newtype=%d | count=%d refs=%d "
             "poolUsed=%d poolCap=%d heapH=%p",
             (void *)hProps, (void *)pTable, (int)tag, (int)*pValue,
             (int)pTable[0], *(int *)((char *)pTable + 4),
             *(int *)((char *)pTable + 8), *(int *)((char *)pTable + 0xc),
             *(void **)((char *)pTable + 0x10));
    if (tag < *pTable && tag >= 0) {
        pSlot = (PropSlot *)((char *)pTable + tag * 0x20 + 0x14);
        slot = *pSlot;
        if (slot.type == 3) {
            CopyPropertyHeapStringToBuffer((int)(intptr_t)pTable, slot.value, &gScratchStr);
            *(int *)(pTable + 2) += gScratchStr[0] + 1;   /* length byte, not the array address */
        }
        slot.type  = *pValue;
        slot.value = *(int *)(pValue + 1);
        if (slot.type == 3) {
            TakeScriptString(pValue, &gScratchStr);
            slot.value = InternPascalStringInPropertyHeap((int)(intptr_t)pTable, &gScratchStr);
        }
        *pSlot = slot;
        if (*(int *)(pTable + 2) >= 0x800) {
            CompactPropertyStringHeap(pTable);
        }
        UnlockResObject(hProps);
        return 0;
    }
    UnlockResObject(hProps);
    return 9;
}


/* Declares the property pName on pScope unless it exists, returning its tag in pTagOut and
 * caching it in the instruction (instrAddr + 6, the cache LookupPropertyByName reads). A new
 * property is appended as type 4 with value 0, the table growing by 20 entries when full.
 * Returns 0, 1 when the name is longer than 15 characters, 9 at 32000 properties. */
short __cdecl DeclarePropertyFromBytecodeInstr(int *pScope, byte *pName, short *pTagOut,  /* 0x0044df40 */
                                              int instrAddr)
{
    /* The new 32-byte record is built in one local and copied whole
     * (`rep movsd`, ecx = 8): type word 4 at +0, a zero dword at +2, the name
     * at +0x10. The exe writes nothing else into it -- bytes 6..15 are whatever
     * the stack held (the port had zeroed all sixteen). */
    typedef struct { unsigned char b[32]; } PropRec32;
    short     *psData;
    short      oldCount;
    short      capShort;
    PropRec32  rec;

    if (*pName >= 0x10)
        return 1;
    if (LookupPropertyByName(pScope, pName, pTagOut, instrAddr) == 0)
        return 0;

    psData = (short *)GetResDataPtr(pScope);
    oldCount = psData[0];
    capShort = psData[1];
    UnlockResObject(pScope);
    if (oldCount >= 32000)
        return 9;

    *(short *)(instrAddr + 6) = oldCount;
    *pTagOut = oldCount;
    oldCount++;                        /* now the new count */
    if (capShort < oldCount) {
        capShort += 0x14;
        ResizeResObject(pScope, (int)capShort * 0x20 + 0x14);
    }

    psData = (short *)GetResDataPtr(pScope);
    psData[0] = oldCount;
    psData[1] = capShort;
    *(int *)&rec.b[2] = 0;
    *(short *)&rec.b[0] = 4;
    CopyPascalString(pName, &rec.b[16]);
    *(PropRec32 *)((char *)psData + oldCount * 0x20 - 0xc) = rec;
    UnlockResObject(pScope);
    return 0;
}

static int DfTracePascalLooksWrong(const byte *s);
#undef local_138
#undef local_12c
#undef local_124


#if DF_TRACE_ENABLED
/* PORT-HELPER: trace check used by InternPascalStringInPropertyHeap. A Pascal string with
 * unprintable characters is a stale or short-copied buffer; checking where a string enters
 * the heap names its producer, which a check at the reader cannot. */
static int DfTracePascalLooksWrong(const byte *s)
{
    int i;
    for (i = 1; i <= (int)s[0]; i++)
        if (s[i] < 0x20 || s[i] > 0x7e)
            return 1;
    return 0;
}
#endif


/* Removes the property pName from pScope (UndeclareVariableList): the last entry is moved
 * into its slot and the count drops; a string value's bytes are counted as freed, compacting
 * the heap at 0x800. Returns 0 (also when there is no such property), 1 for a name longer
 * than 15 characters. */
short __cdecl UndeclarePropertyForInstr(int *pScope, byte *pName, int instrAddr)  /* 0x0044e040 */
{
    typedef struct { int d[8]; } PropSlotRec;   /* one 32-byte property slot, copied whole (rep movsd) */
    short *psData;
    short  tag;
    /* ONE 32-byte slot at ebp-0x20 of which Ghidra names only slotType and slotValue; as two
     * locals the 8-dword copy overran them (port-bug-classes.md #3). */
    PropSlotRec slot;
#define slotType (*(short *)&slot)  /* local_20 */
#define slotValue (*(int *)((char *)&slot + 2))  /* local_1e */
    short  oldCount;

    if (*pName >= 0x10)
        return 1;

    if (LookupPropertyByName(pScope, pName, &tag, instrAddr) != 0)
        return 0;

    psData = (short *)GetResDataPtr(pScope);
    slot   = *(PropSlotRec *)(psData + tag * 0x10 + 10);

    if (slotType == 3) {
        CopyPropertyHeapStringToBuffer((int)(intptr_t)psData, slotValue, (byte *)&gScratchStr);
        /* the length byte, not the array's address truncated (port-bug-classes.md #17) */
        *(int *)(psData + 2) += (int)gScratchStr[0] + 1;
    }

    oldCount = *psData;
    *psData  = (short)(oldCount - 1);
    *(PropSlotRec *)(psData + tag * 0x10 + 10) =
        *(PropSlotRec *)(psData + (short)(oldCount - 1) * 0x10 + 10);

    if (*(int *)(psData + 2) >= 0x800)
        CompactPropertyStringHeap(psData);

    UnlockResObject(pScope);
    return 0;
}


/* Finds the property named pName (case-insensitive) in the table pObj and returns its tag in
 * pTagOut. The short at instrAddr + 6 caches the tag per instruction: tried first (a cached
 * 0 is never trusted), rewritten after a scan. Returns 0, or 9 when not found or pObj is NULL. */
short __cdecl LookupPropertyByName(int *pObj, byte *pName, short *pTagOut, int instrAddr)  /* 0x0044e120 */
{
    /* One variable carries the cached tag and the scan counter: the exe keeps
     * both in si. */
    short  tag;
    short *pData;
    short *pEntry;

    if (pObj == NULL)
        return 9;
    pData = (short *)GetResDataPtr(pObj);
    pEntry = pData + 10;   /* the entries, after the 0x14-byte header */
    tag = *(short *)(instrAddr + 6);
    if (tag > 0 && tag < *pData) {
        if (PascalStringsEqualNoCase((byte *)(pEntry + tag * 0x10 + 8), pName) != 0) {
            UnlockResObject(pObj);
            *pTagOut = tag;
            return 0;
        }
    }
    for (tag = 0; tag < *pData; tag++, pEntry += 0x10) {
        if (PascalStringsEqualNoCase((byte *)(pEntry + 8), pName) != 0) {
            UnlockResObject(pObj);
            *(short *)(instrAddr + 6) = tag;
            *pTagOut = tag;
            return 0;
        }
    }
    UnlockResObject(pObj);
    return 9;
}


/* Frees a property table and its string heap (the handle at +0x10). */
void __cdecl UninstallResourcePair(void *pHandle)  /* 0x0044e1f0 */
{
    int pData;

    pData = GetResDataPtr(pHandle);
    FreeResObject(*(void **)(pData + 0x10));
    UnlockResObject(pHandle);
    FreeResObject(pHandle);
}

/* Copies the Pascal string at heapByteOffset in the string heap of the (locked) table data
 * into pDst. Fatal 0x1034 when the offset is not below the heap's used size. */
void __cdecl CopyPropertyHeapStringToBuffer(int propTableDataAsInt, int heapByteOffset,
                                            byte *pDst)  /* 0x0044e220 */
{
    unsigned char *base = (unsigned char *)(intptr_t)propTableDataAsInt;
    int            dataPtr;

    if (*(int *)(base + 8) <= heapByteOffset)
        FatalError(0, 0x1034);

    dataPtr = GetResDataPtr(*(void **)(base + 0x10));
    CopyPascalString((byte *)(dataPtr + heapByteOffset), pDst);
    UnlockResObject(*(int **)(base + 0x10));
}

/* Appends pSrc to the string heap of the (locked) table data and returns its offset, growing
 * the heap by the string plus 0x800 bytes when it does not fit. */
int __cdecl InternPascalStringInPropertyHeap(int propTableDataAsInt, byte *pSrc)  /* 0x0044e270 */
{
    int            poolUsed;
    int            dataPtr;
    int            cap;
    int            byteLen;
    unsigned char *base = (unsigned char *)(intptr_t)propTableDataAsInt;
    poolUsed = *(int *)(base + 8);
    byteLen = (short)(*pSrc + 1);
    if (poolUsed + byteLen >= *(int *)(base + 0xc)) {
        cap = byteLen + *(int *)(base + 0xc) + 0x800;
        dataPtr = *(int *)(base + 0x10);
        *(int *)(base + 0xc) = cap;
        ResizeResObject((int *)dataPtr, cap);
    }

#if DF_TRACE_ENABLED
    if (DfTracePascalLooksWrong(pSrc))
        DF_TRACE("!! internProp: MALFORMED ofs=%d len=%d '%.*s'",
                 poolUsed, (int)pSrc[0], (int)pSrc[0], (const char *)pSrc + 1);
#endif
    dataPtr = GetResDataPtr(*(void **)(base + 0x10));
    CopyPascalString(pSrc, (byte *)(dataPtr + poolUsed));
    UnlockResObject(*(int **)(base + 0x10));
    *(int *)(base + 8) += byteLen;
    return poolUsed;
}


/* Rebuilds the string heap of the (locked) table data: a new 0x800-byte heap, every string
 * property copied into it from the old one, the old freed and the freed-bytes count reset. */
void __cdecl CompactPropertyStringHeap(short *pTable)  /* 0x0044e2e0 */
{
    /* Byte match: fields through pTable itself keep their own alias classes, so the loop's
     * two header stores pair. tab only feeds the callees' int argument and is set after
     * newUsed = 0 so the shared zero register stays newUsed's (ebx). */
    short         i;
    int          *newHeap;
    int          *oldHeap;
    short        *p;
    uint          newUsed;
    int           tab;
    uint          oldUsed;

    oldUsed = *(uint *)(pTable + 4);
    oldHeap = *(int **)(pTable + 8);
    newUsed = 0;
    *(int *)(pTable + 6) = 0x800;
    newHeap = (int *)AllocResObject(0x800u);
    p = pTable + 10;
    tab = (int)(intptr_t)pTable;
    for (i = 0; i < *pTable; i++) {
        if (*p == 3) {
            *(int **)(pTable + 8) = oldHeap;
            *(uint *)(pTable + 4) = oldUsed;
            CopyPropertyHeapStringToBuffer(tab, *(int *)(p + 1), &gScratchStr);
            *(int **)(pTable + 8) = newHeap;
            *(uint *)(pTable + 4) = newUsed;
            *(int *)(p + 1) = InternPascalStringInPropertyHeap(tab, &gScratchStr);
            newUsed = *(uint *)(pTable + 4);
        }
        p += 0x10;
    }
    FreeResObject(oldHeap);
    *(uint *)(pTable + 4) = newUsed;
    *(int **)(pTable + 8) = newHeap;
    *(int *)(pTable + 2) = 0;
}
