/* render_nodes.c -- TI.EXE 0x00437490..0x004389d0: render nodes, the render scan lock and the child-entry cache. */

#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "df_protos.h"
#include "topics/renderer.h"
#include "topics/scriptcmd.h"
#include "topics/resource.h"
#include "topics/container.h"
#include "topics/scene.h"
#include "topics/anim.h"

static short __cdecl ProcessRenderNodeState(int *pNode);
static short __stdcall GetActiveListHead(void);
static int __cdecl GetChildEntry(short index);
static void __cdecl UpdateChildEntry(short index);
static short __stdcall GetScanListHead(void);
static short __stdcall GetScanEntryCount(void);
static void TeardownParentContext(int *pCtx);
static short FlushAndReleaseScanEntries(void *pNode);
static short __cdecl FindChildByID(int childId);
static short __cdecl RegisterChildEntry(int childId);
static void __cdecl PromoteChildEntry(short index);
static void __cdecl DestroyChildEntry(int *pNode, short index);
static void __cdecl FreeChildSlot(short index);

/* Layout shared by everything below. A render node is a 0x550-byte engine buffer, one per open
 * container file: +0x24 Pascal-string name, +0x530 serial (NextRenderNodeSerial), +0x538 page
 * size, +0x53c handle of the node's parent-data resource, +0x540 next node in gRenderNodeList,
 * +0x548 read-only flag, +0x54a pending-close flag, +0x54c next node in gPendingRenderNodeList.
 * Dword-indexed uses (pNode[0x14f], [0x150], [0x153]) are the +0x53c / +0x540 / +0x54c fields.
 *
 * The parent data (AllocChildTable) indexes the node's child cache: 0x18-byte slots in a second
 * resource, reached through gCurrentParentData / gCurrentChildListData once a node is bound with
 * SetRenderParentContext. A slot is: +0x00 child id, +0x04 use count, +0x05 dirty flag, +0x06
 * resource handle, +0x0a order stamp, +0x0e next in hash bucket, +0x10 / +0x12 next / previous in
 * the LRU chain, +0x14 / +0x16 previous / next in the dirty (active) list. Chain ends are -1. */

/* Writes back and checks every render node in gRenderNodeList under the scan lock; returns the
 * first non-zero error from ProcessRenderNodeState or FlushContainerNodeState, else 0. */
short __stdcall FlushAllRenderNodes(void)  /* 0x00437490 */
{
    short       err;       /* declared first: err in esi, the node in edi */
    RenderNode *pNode;

    AcquireRenderScanLock();
    for (pNode = gRenderNodeList; pNode != NULL; pNode = pNode->pNext)
    {
        if ((err = (short)ProcessRenderNodeState(pNode)) != 0 ||
            (err = (short)FlushContainerNodeState(pNode)) != 0) {
            ReleaseRenderScanLock();
            return err;
        }
    }
    ReleaseRenderScanLock();
    return 0;
}


/* Takes the render scan lock, a CRITICAL_SECTION created on first use, and sets gScanLockBusy. */
void __stdcall AcquireRenderScanLock(void)  /* 0x004374f0 */
{
    if (gScanLockInited == 0) {
        gScanLockBusy = 0;
        InitializeCriticalSection(&gScanLock);
        gScanLockInited = 1;
    }
    EnterCriticalSection(&gScanLock);
    gScanLockBusy = 1;
}


/* Clears gScanLockBusy and leaves the render scan lock. */
void __stdcall ReleaseRenderScanLock(void)  /* 0x00437530 */
{
    gScanLockBusy = 0;
    LeaveCriticalSection(&gScanLock);
}


/* Writes every dirty child (the active list) back to the node, then destroys the cache entries
 * nothing uses (use count 0). Returns the first write error, else 0.
 *
 * Port note: an error return from the write loop skips ReleaseRenderParentContext, leaving the
 * parent context bound, as in the exe. */
static short __cdecl ProcessRenderNodeState(int *pNode)  /* 0x00437550 */
{
    unsigned int *pSlot;
    int          scanCount;
    short        idx;
    short        err;
    int          pScanEntry;
    short        nextScanIdx;
    short        nextIdx;

    SetRenderParentContext((int *)pNode[0x14f]);

    idx = GetActiveListHead();
    while (idx >= 0)
    {
        pSlot = (unsigned int *)GetChildEntry(idx);
        /* The third argument is the slot's resource handle (+6), as in the other two callers
         * (0x00437583 is `MOV EAX,[EAX+6]; PUSH EAX`); the port once passed the field's address. */
        err   = WriteChildDataToNode(pNode, *pSlot, *(int **)((char *)pSlot + 6));
        if (err != 0)
            return err;
        *((char *)pSlot + 5) = 0;                         /* clean now: UpdateChildEntry unlinks it */
        nextIdx = *(short *)((char *)pSlot + 0x14);       /* prevActive, read before the unlink */
        UpdateChildEntry(idx);
        idx = nextIdx;
    }

    scanCount = (int)(short)GetScanEntryCount();
    idx       = (short)GetScanListHead();
    while (scanCount > 0)
    {
        pScanEntry  = GetChildEntry(idx);
        nextScanIdx = *(short *)(pScanEntry + 0x10);
        if (*(char *)(pScanEntry + 4) == '\0')            /* use count 0: unreferenced */
        {
            DestroyChildEntry(pNode, idx);
            scanCount--;
        }
        if (nextScanIdx < 0)
            break;
        idx = nextScanIdx;
    }

    ReleaseRenderParentContext((int)pNode[0x14f]);
    return 0;
}


/* Binds a node's parent-data resource: locks it and its child-list resource and caches the
 * pointers in gCurrentParentNode, gCurrentParentData and gCurrentChildListData, which every
 * child-entry helper below reads. Pair with ReleaseRenderParentContext. */
void __cdecl SetRenderParentContext(int *pNodeHandle)  /* 0x00437610 */
{
    gCurrentParentNode    = pNodeHandle;
    gCurrentParentData    = (int **)GetResDataPtr(pNodeHandle);
    gCurrentChildListData = (int  *)GetResDataPtr((int *)*gCurrentParentData);
}


/* Unlocks both resources SetRenderParentContext locked and clears gCurrentParentNode.
 *
 * Port note: every caller passes the node's parent-context handle (+0x53c) and cleans four
 * bytes, but the body works from the two globals. The parameter stays because it is what the
 * 1996 callers push. */
void __cdecl ReleaseRenderParentContext(int ctx)  /* 0x00437640 */
{
    (void)ctx;
    UnlockResObject((int *)*gCurrentParentData);
    UnlockResObject(gCurrentParentNode);
    gCurrentParentNode = NULL;
}


/* Index of the head of the active (dirty) list in the bound parent data (+0x12), the field
 * UpdateChildEntry maintains. */
static short __stdcall GetActiveListHead(void)  /* 0x00437670 */
{
    return *(short *)((char *)gCurrentParentData + 0x12);
}


/* Index of the LRU tail, where the scan chain starts, in the bound parent data (+0x0e).
 *
 * Port note: the port also carried this address as GetOrphanScanListHead, a uint32_t copy
 * nothing called (Ghidra's CONCAT22 of the pointer's upper half and the word). */
static short __stdcall GetScanListHead(void)  /* 0x00437680 */
{
    return *(short *)((char *)gCurrentParentData + 0x0e);
}


/* Address of child slot `index` (stride 0x18) in gCurrentChildListData. */
static int __cdecl GetChildEntry(short index)  /* 0x00437690 */
{
    return (int)((char *)gCurrentChildListData + index * 0x18);
}


/* Brings a slot's membership of the active (dirty) list in line with its dirty flag (+5): a
 * dirty slot is appended at the head, a clean one is unlinked. Parent data +0x12 is the head
 * index and +0x14 the count. */
static void __cdecl UpdateChildEntry(short index)  /* 0x004376b0 */
{
#define PD_HEAD  (*(short *)((char *)gCurrentParentData + 0x12))
#define PD_COUNT (*(short *)((char *)gCurrentParentData + 0x14))
#pragma pack(push, 2)
    typedef struct {
        int   key;          /* +0x00 */
        char  linkCount;    /* +0x04 */
        char  flags;        /* +0x05 */
        char  pad[14];      /* +0x06 .. +0x13 */
        short prevActive;   /* +0x14 */
        short nextActive;   /* +0x16 */
    } ActiveSlotRow;
#pragma pack(pop)
    ActiveSlotRow *e;

    e = (ActiveSlotRow *)GetChildEntry(index);
    /* the exe tests the linked case (flags != 0) first */
    if (e->flags != 0) {
        /* not yet linked: old head's next = this, this's prev = old head */
        if (PD_HEAD != index && e->prevActive < 0 && e->nextActive < 0) {
            PD_COUNT += 1;
            if (PD_HEAD >= 0) {
                ((ActiveSlotRow *)GetChildEntry(PD_HEAD))->nextActive = index;
                e->prevActive = PD_HEAD;
            }
            PD_HEAD = index;
        }
    } else if (PD_HEAD == index || e->prevActive >= 0 || e->nextActive >= 0) {
        PD_COUNT -= 1;
        if (e->nextActive >= 0)
            ((ActiveSlotRow *)GetChildEntry(e->nextActive))->prevActive = e->prevActive; /* next.prev = this.prev */
        else
            PD_HEAD = e->prevActive;                                                   /* this IS the head */
        if (e->prevActive >= 0)
            ((ActiveSlotRow *)GetChildEntry(e->prevActive))->nextActive = e->nextActive; /* prev.next = this.next */
        e->nextActive = -1;
        e->prevActive = -1;
    }
#undef PD_HEAD
#undef PD_COUNT
}


/* Number of cache entries ProcessRenderNodeState scans for unused ones: parent data +0x16
 * (live entries) minus +0x08 (the size AllocChildTable was given). */
static short __stdcall GetScanEntryCount(void)  /* 0x004377a0 */
{
    return *(short *)((char *)gCurrentParentData + 0x16)
         - *(short *)((char *)gCurrentParentData + 0x08);
}


/* Stores the head of gRenderNodeList through ppHead. */
void __cdecl GetGlobalRenderNodeListHead(void **ppHead) /* 0x004377c0 */
{
    *ppHead = (void *)gRenderNodeList;
}


/* Makes room for a `size`-byte allocation: below 400000 free bytes it evicts cached children
 * (and calls the memory-pressure hook) for size + 200000 first, then retries the allocation,
 * evicting 200000 at a time. *pSucceeded is 0 when no room could be found; the return value is
 * an error code only if an eviction fails. */
short __cdecl ReserveHeapSpace(int size, short *pSucceeded) /* 0x004377d0 */
{
    short status;
    int   local4;

    /* The hook takes the byte-count cell (push eax / call [0x45db94] / add esp,4). Nothing in
     * TI.EXE ever stores to 0x0045db94, so it is always NULL; the cast keeps the globals
     * generator's type. */
#define CALL_MEMORY_PRESSURE_HOOK(p) (((short (*)(int *))gMemoryPressureCallback)(p))

    AcquireRenderScanLock();
    if (GetHeapFreeBytes() < 400000) {
        local4 = size + 200000;
        status  = EvictCachedChildrenForSpace(&local4);
        if (status != 0)
            goto fail;
        if (gMemoryPressureCallback != NULL) {
            status = CALL_MEMORY_PRESSURE_HOOK(&local4);
            if (status != 0)
                goto fail;
        }
    }
    *pSucceeded = 1;
    do {
        /* the status is formed as (fail ? -1 : 0) & 0x66: cmp eax,1 / sbb / test al,0x66 */
        if (((TryAllocHeapSpace(size) == 0 ? -1 : 0) & 0x66) == 0)
            goto done_ok;
        local4 = 200000;
        status  = EvictCachedChildrenForSpace(&local4);
        if (status != 0)
            goto fail;
        if (gMemoryPressureCallback != NULL) {
            status = CALL_MEMORY_PRESSURE_HOOK(&local4);
            if (status != 0)
                goto fail;
        }
    } while (local4 < 0);
    if (((TryAllocHeapSpace(size) == 0 ? -1 : 0) & 0x66) != 0) {
        /* Every caller turns this into error 0x66 = 102, which the string table calls "resource
         * not found" but means "no room"; the numbers say whether the pool is exhausted or
         * fragmented. */
        DF_TRACE("!! heap: cannot reserve %d bytes -- pool=%d used=%d free=%d largestFree=%u",
                 size, g_pActiveHeapDesc[3], g_pActiveHeapDesc[4],
                 GetHeapFreeBytes(), GetHeapFreeCapacity((int)(intptr_t)g_pActiveHeapDesc));
        *pSucceeded = 0;
    }
done_ok:
    ReleaseRenderScanLock();
    return 0;
fail:
    ReleaseRenderScanLock();
    return status;
#undef CALL_MEMORY_PRESSURE_HOOK
}


/* Frees cached children until *pAmount bytes have been reclaimed (it is decremented as it
 * goes). Per node it finds the least recently used child nothing is attached to, then
 * repeatedly evicts the one with the lowest order stamp across all nodes, writing it back first
 * if dirty. At most 64 nodes (FatalError 0x6e otherwise). Returns 0, or a write error.
 *
 * Port note: an earlier port started the LRU walk at the wrong list head and tested the use
 * count the wrong way round, so eviction found nothing or freed live children. */
short __cdecl EvictCachedChildrenForSpace(int *pAmount)  /* 0x004378D0 */
{
    /* Frame as the exe: the per-node counter n[0] at +0 (reused in the second loop as the
     * chosen slot), its copy n[1] at +2, then the three tables. Two separate shorts always took
     * the two slots the other way round. */
    short      n[2];
    short      sHead;
    short      auHead[64];
    uint32_t  *apNodes[64];
    int        aiPri[64];
    uint32_t  *pNode;
    ushort     uCur, uBest;
    uint32_t  *pWalk;
    int        pEntry;
    uint32_t  *pChild;
    short      uRet;
    int        iBestPri;
    short      i;
    int        pri;

    DF_TRACE("evict: want=%d listHead=%p heapFree=%d", *pAmount, (void *)gRenderNodeList,
             GetHeapFreeBytes());

    /* per node: the first evictable child and its order stamp */
    n[0] = 0;
    for (pWalk = (uint32_t *)gRenderNodeList; pWalk != NULL; pWalk = (uint32_t *)pWalk[0x150]) {
        SetRenderParentContext((int *)pWalk[0x14f]);
        apNodes[n[0]] = pWalk;

        /* Both walks start at the LRU tail (GetScanListHead) and follow +0x10 towards the MRU
         * head WHILE the use count (+4) is non-zero, so they skip every child still attached
         * and stop at the first free one, the candidate. */
        uCur = (ushort)GetScanListHead();
        while ((short)uCur >= 0) {
            pChild = (uint32_t *)GetChildEntry((short)uCur);
            if (*(char *)((int)pChild + 4) == '\0')
                break;
            uCur = *(ushort *)((int)pChild + 0x10);
        }
        auHead[n[0]] = uCur;
        if ((short)uCur >= 0)
            aiPri[n[0]] = *(int *)((int)pChild + 10);
        ReleaseRenderParentContext(pWalk[0x14f]);
        if (n[0] >= 0x40)
            FatalError(0x6e, 0x454);
        n[0]++;
    }

    n[1] = n[0];
    do {
        iBestPri = 0x7fffffff;
        for (i = 0; i < n[1]; i++) {
            sHead = auHead[i];
            if (sHead >= 0 && (pri = aiPri[i]) < iBestPri) {
                uBest    = sHead;
                iBestPri = pri;
                n[0]   = i;
                pNode    = apNodes[i];
            }
        }
        if (iBestPri == 0x7fffffff) {
            DF_TRACE("evict: no evictable child on %d node(s), still want=%d", (int)n[1], *pAmount);
            return 0;
        }
        SetRenderParentContext((int *)pNode[0x14f]);

        /* advance this node's candidate to its next evictable child */
        pEntry = GetChildEntry((short)uBest);
        uCur = *(ushort *)(pEntry + 0x10);
        while ((short)uCur >= 0) {
            pEntry = GetChildEntry(uCur);
            if (*(char *)(pEntry + 4) == '\0')
                break;
            uCur = *(ushort *)(pEntry + 0x10);
        }
        auHead[n[0]] = uCur;
        if ((short)uCur >= 0)
            aiPri[n[0]] = *(int *)(pEntry + 10);

        pChild = (uint32_t *)GetChildEntry((short)uBest);
        if (*(char *)((int)pChild + 5) != '\0') {
            uRet = WriteChildDataToNode((int *)pNode, *pChild, *(int **)((int)pChild + 6));
            if (uRet != 0)
                return uRet;
        }

#if DF_TRACE_ENABLED
        {
            /* Names every victim before it dies: a child with use count 0 whose heap descriptor
             * still holds a lock (an unmatched GetResDataPtr) makes FreeResObject fatal (0xe9). */
            int *h = *(int **)((int)pChild + 6);
            int locks = (h != NULL && *h != 0) ? *(int *)(*h - 0x30 + 0x20) : -1;
            DF_TRACE("evict: victim node=%p child=%u handle=%p size=%d locks=%d dirty=%d stamp=%d%s",
                     (void *)pNode, *pChild, (void *)h, GetResObjectSize(h), locks,
                     (int)*(char *)((int)pChild + 5), *(int *)((int)pChild + 10),
                     locks > 0 ? "  <<< STILL LOCKED" : "");
        }
#endif
        *pAmount -=GetResObjectSize(*(int **)((int)pChild + 6));
        DestroyChildEntry((int *)pNode, (short)uBest);
        ReleaseRenderParentContext(pNode[0x14f]);
    } while (*pAmount > 0);
    return 0;
}


/* Increments gRenderSlotCounter and returns it: the serial OpenContainerNode stores at node+0x530.
 * Ghidra shows it as void; the caller uses EAX, so it returns int. */
int __stdcall NextRenderNodeSerial(void)  /* 0x00437ac0 */
{
    gRenderSlotCounter = gRenderSlotCounter + 1;
    return gRenderSlotCounter;
}


/* Opens a container file (named by the Pascal-string descriptor), builds a node for it, reads
 * the container header into it and prepends it to gRenderNodeList; *ppNode receives the node.
 * Returns 0, or an error code (0x6b when the file cannot be accessed). */
short __cdecl OpenContainerNode(byte *pDescriptor, int **ppNode)  /* 0x00437ad0 */
{
    short          status;
    int           *node;
    short          openError;
    int            containerInfo;
    unsigned short isReadOnly;
    int            statusCopy;   /* unused out-param from ProbeContainerFile */  /* local_4 */

    AcquireRenderScanLock();

    /* VC++ 4.2 moves a return block reached by ONE unconditional jump to just after it; with
     * two it stays put, after the read's test, as in TI.EXE. The `statusCopy = status; status =
     * statusCopy;` pairs are those jumps at layout time and compile to nothing afterwards. */
    status = ProbeContainerFile(pDescriptor, &openError, &containerInfo, &isReadOnly);
    if (status != 0) {
        statusCopy = status;
        status = statusCopy;
        goto fail;
    }
    if (openError != 0) {
        status = 0x6b;   /* file not accessible, error 107 */
        goto fail;
    }
    {
        node = (int *)AllocEngineBuffer(0x550);

        node[0x14c] = NextRenderNodeSerial();           /* +0x530 serial */
        node[0x14d] = 0;                             /* +0x534 */
        node[0x14e] = 0x400;                         /* +0x538 page size */
        node[0x14f] = (int)AllocChildTable(0x400);   /* +0x53c parent data, sized for 0x400 */
        node[0x151] = containerInfo;      /* +0x544 */
        *(unsigned short *)((char *)node + 0x548) = isReadOnly;  /* +0x548 */
        *(unsigned short *)((char *)node + 0x54a) = 0;           /* +0x54a not pending */
        node[0x153] = 0;                  /* +0x54c */

        status = ReadContainerIntoNode(pDescriptor, node);
    }
    if (status != 0) {
        statusCopy = status;
        status = statusCopy;
fail:
        ReleaseRenderScanLock();
        return status;
    }
    node[0x150] = (int)gRenderNodeList;  /* +0x540 next */
    gRenderNodeList = node;
    *ppNode = node;
    ReleaseRenderScanLock();
    return 0;
}


/* Creates the parent data for a node's child cache, sized for `requestedSize`, and returns its
 * resource handle. The size class is the first entry of kReadBufSizeTable (22 shorts; entry
 * i - 1 is the slot count, entry i the size threshold, which must exceed requestedSize / 4) and
 * fixes the number of slots, which is also the number of hash buckets.
 *
 * Two resources are allocated: the parent data, (slotCount * 2 + 0x18) bytes, and the slot array,
 * slotCount * 0x18 bytes, whose handle sits at +0x00 of the parent data. Fields: +0x04 capacity
 * (slotCount), +0x06 next fresh slot (0), +0x08 requestedSize, +0x0a bucket count (slotCount),
 * +0x0c free list head, +0x0e LRU tail, +0x10 MRU head, +0x12 active list head (all -1), +0x14
 * active count (0), +0x16 live entry count (0), then short[slotCount] hash buckets from +0x18,
 * all -1.
 *
 * Port note: VC++ 4.2 aligns an array of 8+ bytes to 8, so the table starts at 0x0045dba8, not
 * 0x0045dba6. */
int * __cdecl AllocChildTable(short requestedSize)  /* 0x00437bd0 */
{
    int    *pDescHandle;
    short  *pDesc;
    short   slotCount;
    short   i;
    short   cls;

    for (cls = 1; cls < 0x16; cls++) {
        if ((short)(requestedSize / 4) < kReadBufSizeTable[cls])
            break;
    }
    slotCount = kReadBufSizeTable[cls - 1];

    pDescHandle = AllocResObject(slotCount * 2 + 0x18);
    pDesc = (short *)GetResDataPtr(pDescHandle);
    pDesc[2] = slotCount;
    pDesc[3] = 0;
    *(int *)pDesc = (int)AllocResObject(slotCount * 0x18);   /* +0x00 slot array handle */
    pDesc[4] = requestedSize;
    pDesc[5] = slotCount;
    pDesc[6] = -1;
    pDesc[7] = -1;
    pDesc[8] = -1;
    pDesc[9] = -1;
    pDesc[10] = 0;
    pDesc[11] = 0;
    for (i = 0; i < slotCount; i++)
        pDesc[i + 12] = -1;

    UnlockResObject(pDescHandle);
    return pDescHandle;
}


/* Finds the node in gRenderNodeList whose serial (+0x530) is nodeVal and which is not pending
 * close (+0x54a == 0). Stores it in *pOut and returns 1, or returns 0. */
short __cdecl FindRenderNodeByContextValue(int nodeVal, int *pOut)   /* 0x00437ca0: xor ax,ax / mov ax,1 */
{
    int *walk;

    AcquireRenderScanLock();
    for (walk = gRenderNodeList; walk != NULL; walk = *(int **)((char *)walk + 0x540)) {
        if (*(int *)((char *)walk + 0x530) == nodeVal
            && *(short *)((char *)walk + 0x54a) == 0) {
            *pOut = (int)walk;
            ReleaseRenderScanLock();
            return 1;
        }
    }
    ReleaseRenderScanLock();
    return 0;
}


/* Installs the callback DestroyRenderNode calls before tearing a node down. */
void __cdecl SetNodeDestroyHook(int pHandler)  /* 0x00437cf0 */
{
    gNodeDestroyHook = pHandler;
}


/* Closes and frees a render node: runs gNodeDestroyHook if installed, releases its child cache
 * (error 0x68 if a child is still in use), frees its parent data and storage, and unlinks it
 * from gRenderNodeList and gPendingRenderNodeList. Returns 0 or the first error. */
short __cdecl DestroyRenderNode(void *pNode)  /* 0x00437d00 */
{
    unsigned int **pPrev;
    unsigned int  *pCur;
    short          status;

    AcquireRenderScanLock();

    /* 0x00437d14 is `PUSH ESI; CALL [0x0045db98]; ADD ESP,4`: the hook takes the node. */
    if (gNodeDestroyHook == 0 || (status = ((short (__cdecl *)(void *))gNodeDestroyHook)(pNode)) == 0)
    {
        status = (short)FlushAndReleaseScanEntries(pNode);
        if (status == 0)
        {
            TeardownParentContext((int *)((unsigned int *)pNode)[0x14f]);
            status = FlushAndCloseNodeStorage((int *)pNode);
        }
    }
    if (status != 0)
    {
        ReleaseRenderScanLock();
        return status;
    }

    /* unlink from gRenderNodeList (link at +0x540) */
    pPrev = (unsigned int **)&gRenderNodeList;
    pCur  = (unsigned int *)gRenderNodeList;
    while (pCur != NULL)
    {
        if (pCur == (unsigned int *)pNode)
        {
            *pPrev = (unsigned int *)((unsigned int *)pNode)[0x150];
            break;
        }
        pPrev = (unsigned int **)&pCur[0x150];
        pCur  = (unsigned int *)*pPrev;
    }

    /* unlink from the pending list (link at +0x54c) */
    pPrev = (unsigned int **)&gPendingRenderNodeList;
    pCur  = (unsigned int *)(unsigned int)gPendingRenderNodeList;
    while (pCur != NULL)
    {
        if (pCur == (unsigned int *)pNode)
        {
            *pPrev = (unsigned int *)((unsigned int *)pNode)[0x153];
            break;
        }
        pPrev = (unsigned int **)&pCur[0x153];
        pCur  = (unsigned int *)*pPrev;
    }
    FreeEngineBuffer(pNode);
    ReleaseRenderScanLock();
    return 0;
}


/* Frees a node's parent data and the slot-array resource it points to. */
static void TeardownParentContext(int *pCtx)  /* 0x00437dd0 */
{
    int **ppData;

    ppData = (int **)GetResDataPtr(pCtx);
    FreeResObject((int *)*ppData);
    UnlockResObject(pCtx);
    FreeResObject(pCtx);
}


/* Walks a node's child cache from the LRU tail, writing back dirty children and freeing their
 * resources. Returns 0x68 if any child is still in use (use count non-zero), or a write error. */
static short FlushAndReleaseScanEntries(void *pNode)  /* 0x00437e00 */
{
    unsigned short  idx;
    short           status;
    unsigned int   *pEntry;

    SetRenderParentContext((int *)((unsigned int *)pNode)[0x14f]);
    idx = (unsigned short)GetScanListHead();

    while ((short)idx >= 0)
    {
        pEntry = (unsigned int *)GetChildEntry((short)idx);

        if ((char)pEntry[1] != '\0')
            return 0x68;

        if (*(char *)((char *)pEntry + 5) != '\0')
        {
            status = (short)WriteChildDataToNode((int *)pNode, pEntry[0],
                                               *(int **)((char *)pEntry + 6));
            if (status != 0)
                return status;
        }

        FreeResObject(*(int **)((char *)pEntry + 6));
        idx = (unsigned short)pEntry[4];   /* next in the LRU chain, entry+0x10 */
    }

    ReleaseRenderParentContext((int)((unsigned int *)pNode)[0x14f]);
    return 0;
}


/* Returns 1 if child chanId is in the node's cache, else 0. FatalError 0x455 on a pending node. */
short __cdecl HasRenderChild(int *pCtx, unsigned int chanId) /* 0x00437e80 */
{
    short idx;

    if (*(short *)((char *)pCtx + 0x54a) != 0)
        FatalError(0, 0x455);

    AcquireRenderScanLock();
    SetRenderParentContext(*(int **)((char *)pCtx + 0x53c));
    idx = FindChildByID((int)chanId);
    ReleaseRenderParentContext(*(int *)((char *)pCtx + 0x53c));
    ReleaseRenderScanLock();
    return (idx >= 0) ? 1 : 0;
}


/* Looks a child id up in the bound parent data's hash table (bucket = id % bucket count).
 * Returns the slot index, or -1. */
static short __cdecl FindChildByID(int childId)  /* 0x00437ef0 */
{
    short  idx;
    int   *pEntry;

    idx = *(short *)((char *)gCurrentParentData + 0x18
                     + (short)(childId % (int)*(short *)((char *)gCurrentParentData + 0x0a)) * 2);
    while (idx >= 0)
    {
        pEntry = (int *)GetChildEntry(idx);
        if (*pEntry == childId)
            break;
        idx = *(short *)((int)pEntry + 0x0e);
    }
    return idx;
}


/* Registers an already-loaded resource pRes as child chanId with use count 1. Returns 0x66 if
 * the id is already present. FatalError 0x456 on a pending node. */
short __cdecl AttachRenderChildHandle(int *pCtx, unsigned int chanId,
                                                      int *pRes) /* 0x00437f40 */
{
    short idx;

    if (*(short *)((char *)pCtx + 0x54a) != 0)
        FatalError(0, 0x456);

    AcquireRenderScanLock();
    SetRenderParentContext(*(int **)((char *)pCtx + 0x53c));

    idx = FindChildByID((int)chanId);
    if (idx >= 0) {
        ReleaseRenderParentContext(*(int *)((char *)pCtx + 0x53c));
        ReleaseRenderScanLock();
        return 0x66;
    }

    idx = RegisterChildEntry((int)chanId);
    {
        int pSlot = GetChildEntry(idx);

        *(unsigned char *)(pSlot + 5) = 0;
        *(unsigned char *)(pSlot + 4) = 1;
        *(int *)(pSlot + 6) = (int)pRes;
    }
    UpdateChildEntry(idx);
    ReleaseRenderParentContext(*(int *)((char *)pCtx + 0x53c));
    ReleaseRenderScanLock();
    return 0;
}


/* Allocates a cache slot for childId, from the free list (parent data +0x0c) or the next fresh
 * index (+0x06), growing the slot array by a sixteenth when full. The slot starts with use
 * count 0, clean, no handle and the next order stamp; it is linked into its hash bucket and
 * made the MRU head. Returns the slot index. Parent data layout: see AllocChildTable. */
static short __cdecl RegisterChildEntry(int childId)  /* 0x00437ff0 */
{
    short  idx;
    int   *pSlot;
    short  oldHead;
    short  bucket;

    idx = *(short *)((char *)gCurrentParentData + 0x0c);
    if (idx >= 0)
    {
        /* pop from the free list (chained through slot+0x0e) */
        pSlot = (int *)GetChildEntry(idx);
        *(short *)((char *)gCurrentParentData + 0x0c) = *(short *)((int)pSlot + 0x0e);
    }
    else
    {
        if (*(short *)((char *)gCurrentParentData + 0x04) ==
            *(short *)((char *)gCurrentParentData + 0x06))
        {
            /* capacity += capacity >> 4 */
            UnlockResObject(*gCurrentParentData);   /* drop the lock so the resource can move */
            *(short *)((char *)gCurrentParentData + 0x04) +=
                *(short *)((char *)gCurrentParentData + 0x04) >> 4;
            ResizeResObject(*gCurrentParentData,
                *(short *)((char *)gCurrentParentData + 0x04) * 0x18);
            gCurrentChildListData = (int *)GetResDataPtr(*gCurrentParentData);
        }
        idx   = *(short *)((char *)gCurrentParentData + 0x06);
        *(short *)((char *)gCurrentParentData + 0x06) = idx + 1;
        pSlot = (int *)GetChildEntry(idx);
    }

    *pSlot                       = childId;
    ((char *)pSlot)[4]           = 0;                     /* use count */
    ((char *)pSlot)[5]           = 0;                     /* dirty flag */
    *(int  *)((char *)pSlot + 6)    = 0;                  /* handle */
    *(int  *)((char *)pSlot + 0x0a) = gChildSerialCounter++;  /* order stamp */

    /* head of the hash bucket chain */
    bucket = (short)(childId % (int)*(short *)((char *)gCurrentParentData + 0x0a));
    *(short *)((int)pSlot + 0x0e) =
        *(short *)((char *)gCurrentParentData + 0x18 + bucket * 2); /* old bucket head */
    *(short *)((char *)gCurrentParentData + 0x18 + bucket * 2) = idx;

    /* new MRU head of the LRU chain */
    *(short *)((int)pSlot + 0x10) = -1;                   /* nothing towards the MRU end */
    oldHead = *(short *)((char *)gCurrentParentData + 0x10);
    *(short *)((int)pSlot + 0x12) = oldHead;              /* prev = old MRU head */
    if (oldHead >= 0)
        *(short *)(GetChildEntry(oldHead) + 0x10) = idx;  /* old_mru.next = this */
    *(short *)((char *)gCurrentParentData + 0x10) = idx; /* MRU head = this */
    if (*(short *)((char *)gCurrentParentData + 0x0e) < 0)
        *(short *)((char *)gCurrentParentData + 0x0e) = idx; /* init LRU tail if chain empty */
    *(short *)((char *)gCurrentParentData + 0x16) += 1;  /* live entry count */

    *(short *)((int)pSlot + 0x14) = -1;                   /* not in the active list */
    *(short *)((int)pSlot + 0x16) = -1;

    return idx;
}


/* Takes a use on child childId of a node and returns its resource handle in *pHandleOut. A
 * cached child just gains a use; otherwise the child's size is queried, heap space reserved
 * (twice the size plus 20000 bytes, else the size plus 20000) and the child loaded with
 * OpenChildNode. Returns 0, 0x66 when there is no room, or the error from a callee.
 *
 * Port note: DF_TRACE_PACKREF logs attach / detach / release so a "Bad pack use count"
 * (FatalError 104, a use count taken below zero) can be paired to its culprit. */
short __cdecl AttachRenderChild(int *pParent, unsigned int childId, int *pHandleOut)  /* 0x00438160 */
{
    short  idx;
    int    iEntry;
    short  status;
    int    allocSize;
    short  allocResult;

    DF_TRACE_PACKREF("packRef: ATTACH parent=%p child=%u",
                     (void *)pParent, (unsigned)childId);

    if (*(short *)((int)pParent + 0x54a) != 0)
    {
        FatalError(0, 0x457);
    }

    AcquireRenderScanLock();
    SetRenderParentContext((int *)pParent[0x14f]);

    idx = FindChildByID(childId);
    DF_TRACE_RENDER("AttachRenderChild: parent=%08x child=%u idx=%d (%s)",
             pParent, childId, (int)idx,
             (idx >= 0) ? "already present" : "creating");
    if (idx >= 0)
    {
        iEntry = GetChildEntry(idx);
        *(char *)(iEntry + 4) = *(char *)(iEntry + 4) + 1;
        *pHandleOut = *(int *)(iEntry + 6);
        UpdateChildEntry(idx);
        PromoteChildEntry(idx);
        ReleaseRenderParentContext(pParent[0x14f]);
        ReleaseRenderScanLock();
        return 0;
    }

    status = QueryChildAllocSize(pParent, childId, &allocSize);
    if (status != 0)
    {
        /* The exe drops only the scan lock here and leaves the parent context bound (callers
         * turn a non-zero return into a FatalError); it shares OpenChildNode's failure tail. */
        goto fail;
    }

    /* unbound while ReserveHeapSpace evicts */
    ReleaseRenderParentContext(pParent[0x14f]);
    ReleaseRenderScanLock();

    status = ReserveHeapSpace(allocSize * 2 + 20000, &allocResult);
    if (status != 0)
        return status;

    if (allocResult == 0)
    {
        status = ReserveHeapSpace(allocSize + 20000, &allocResult);
        if (status != 0)
            return status;
        if (allocResult == 0)
            return 0x66;   /* out of memory */
    }

    AcquireRenderScanLock();
    SetRenderParentContext((int *)pParent[0x14f]);

    status = (short)OpenChildNode(pParent, childId, pHandleOut);
    if (status != 0)
    {
fail:
        iEntry = 0;
        if (iEntry == 0) {
            ReleaseRenderScanLock();
            return status;
        }
    }

    idx     = RegisterChildEntry(childId);
    iEntry  = GetChildEntry(idx);
    *(unsigned char *)(iEntry + 5) = 0;        /* clean */
    *(unsigned char *)(iEntry + 4) = 1;        /* one use */
    *(int *)(iEntry + 6)          = *pHandleOut;

    UpdateChildEntry(idx);
    ReleaseRenderParentContext(pParent[0x14f]);
    ReleaseRenderScanLock();
    return 0;
}


/* Marks a slot most recently used: unlinks it from the LRU chain, relinks it at the MRU head
 * and gives it a fresh order stamp. */
static void __cdecl PromoteChildEntry(short index)  /* 0x00438310 */
{
    int pSlot;
    int pOther;

    if (*(short *)((char *)gCurrentParentData + 0x10) == index)
        return;   /* already the MRU head */
    pSlot = GetChildEntry(index);
    if (*(short *)(pSlot + 0x12) >= 0) {   /* test / jl: the linked case first */
        pOther = GetChildEntry(*(short *)(pSlot + 0x12));         /* prev->next = next */
        *(short *)(pOther + 0x10) = *(short *)(pSlot + 0x10);
    } else {
        *(short *)((char *)gCurrentParentData + 0x0e) = *(short *)(pSlot + 0x10);   /* head = next */
    }
    pOther = GetChildEntry(*(short *)(pSlot + 0x10));             /* next->prev = prev */
    *(short *)(pOther + 0x12) = *(short *)(pSlot + 0x12);
    pOther = GetChildEntry(*(short *)((char *)gCurrentParentData + 0x10));   /* old tail->next = index */
    *(short *)(pOther + 0x10) = index;
    *(short *)(pSlot + 0x12) = *(short *)((char *)gCurrentParentData + 0x10);  /* prev = old tail */
    *(short *)(pSlot + 0x10) = -1;                                             /* next = none */
    *(int *)(pSlot + 10) = gChildSerialCounter;
    gChildSerialCounter++;
    *(short *)((char *)gCurrentParentData + 0x10) = index;                    /* tail = index */
}


/* Drops one use of a child (entry+4); the entry stays cached. Returns 0, 0x67 if the child is
 * not attached, 0x68 if its use count would go negative. FatalError 0x458 on a pending node. */
short __cdecl DetachRenderChild(int pParent, int childId)  /* 0x004383c0 */
{
    short  idx;
    int    iEntry;
    char   linkCount;

    DF_TRACE_PACKREF("packRef: DETACH parent=%p child=%u",
                     (void *)(intptr_t)pParent, (unsigned)childId);

    if (*(short *)(pParent + 0x54a) != 0)
    {
        FatalError(0, 0x458);
    }

    AcquireRenderScanLock();
    SetRenderParentContext(*(int **)(pParent + 0x53c));

    /* the found path comes first, as in ReleaseRenderChild */
    idx = FindChildByID(childId);
    if (idx >= 0) {
        DF_TRACE_RENDER("DetachRenderChild: parent=%08x child=%d idx=%d", pParent, childId, (int)idx);

        iEntry    = GetChildEntry(idx);
        linkCount = *(char *)(iEntry + 4) - 1;
        *(char *)(iEntry + 4) = linkCount;

        if (linkCount < 0)
        {
            ReleaseRenderScanLock();
            return 0x68;
        }

        UpdateChildEntry(idx);
        ReleaseRenderParentContext(*(int *)(pParent + 0x53c));
        ReleaseRenderScanLock();
        return 0;
    }
    DF_TRACE("!! DetachRenderChild: child %d NOT attached to parent %08x "
             "(list=%08x)", childId, pParent,
             *(int *)(pParent + 0x53c));
    ReleaseRenderParentContext(*(int *)(pParent + 0x53c));
    ReleaseRenderScanLock();
    return 0x67;
}


/* Drops one use of a child and destroys its cache entry when the count reaches 0 (the name
 * says release, the effect is a decrement). Returns 0, 0x67 if the child is not attached, 0x68
 * on underflow. FatalError 0x459 on a pending node. */
short __cdecl ReleaseRenderChild(int pCtx, int childId)  /* 0x00438470 */
{
    short  childIdx;
    int   *pEntry;
    char   useCount;

    DF_TRACE_PACKREF("packRef: RELEASE parent=%p child=%u",
                     (void *)(intptr_t)pCtx, (unsigned)childId);

    if (*(short *)(pCtx + 0x54a) != 0)
        FatalError(0, 0x459);

    AcquireRenderScanLock();
    SetRenderParentContext(*(int **)(pCtx + 0x53c));
    childIdx = FindChildByID(childId);
    if (childIdx >= 0) {                   /* test si,si / jl */
        pEntry = GetChildEntry(childIdx);
        useCount = *((char *)pEntry + 4) - 1;
        *((char *)pEntry + 4) = useCount;
        if (useCount < '\0') {
            ReleaseRenderScanLock();
            return 0x68;
        }
        if (useCount == '\0')
            DestroyChildEntry((int *)pCtx, childIdx);
        else
            UpdateChildEntry(childIdx);
        ReleaseRenderParentContext(*(int *)(pCtx + 0x53c));
        ReleaseRenderScanLock();
        return 0;
    }
    ReleaseRenderParentContext(*(int *)(pCtx + 0x53c));
    ReleaseRenderScanLock();
    return 0x67;
}


/* Stores a new resource handle in an in-use child, marks it dirty unless the node is read-only
 * (+0x548) and makes it most recently used. Returns 0, 0x67 if absent, 0x68 if its use count is
 * not positive. FatalError 0x45a on a pending node. */
short __cdecl SetChildRenderData(int pCtx, int childId, uint32_t renderData)  /* 0x00438530 */
{
  short idx;
  int e;

  if (*(short *)(pCtx + 0x54a) != 0) {
    FatalError(0, 0x45a);
  }
  AcquireRenderScanLock();
  SetRenderParentContext(*(int **)(pCtx + 0x53c));
  idx = FindChildByID(childId);
  if (idx >= 0) {
    e = GetChildEntry(idx);
    if (*(char *)(e + 4) <= 0) {
      ReleaseRenderScanLock();
      return 0x68;
    }
    *(uint32_t *)(e + 6) = renderData;
    if (*(short *)(pCtx + 0x548) == 0)
      *(char *)(e + 5) = 1;
    UpdateChildEntry(idx);
    PromoteChildEntry(idx);
    ReleaseRenderParentContext(*(int *)(pCtx + 0x53c));
    ReleaseRenderScanLock();
    return 0;
  }
  ReleaseRenderParentContext(*(int *)(pCtx + 0x53c));
  ReleaseRenderScanLock();
  return 0x67;
}


/* Destroys an unused child's cache entry (0x68 if it is in use), then deletes the child from
 * the node's container with DeleteChildFromNode. FatalError 0x45c on a pending node. */
short __cdecl DestroyChildRenderEntry(uint32_t *pCtx, uint32_t childId)  /* 0x004385F0 */
{
  short idx;
  short st;

  if (*(short *)((int)pCtx + 0x54a) != 0)
    FatalError(0, 0x45c);
  AcquireRenderScanLock();
  SetRenderParentContext((int *)pCtx[0x14f]);
  idx = FindChildByID(childId);
  if (idx >= 0) {
    if (*(char *)(GetChildEntry(idx) + 4) > 0) {
      ReleaseRenderScanLock();
      return 0x68;
    }
    DestroyChildEntry(pCtx, idx);
  }
  ReleaseRenderParentContext((int)pCtx[0x14f]);
  st = (short)DeleteChildFromNode(pCtx, childId);
  ReleaseRenderScanLock();
  return st;
}


/* Adds the resource hResource to the node as a new child, pChildId, under the scan lock, and
 * frees hResource on success. FatalError 0x45d on a pending node. */
short __cdecl SetupRenderObjectResource(int *pNode, uint32_t *pChildId, int *hResource)  /* 0x00438690 */
{
    short err;
    if (*(short *)((char *)pNode + 0x54a) != 0)
        FatalError(0, 0x45d);
    AcquireRenderScanLock();
    err = (short)AddChildResourceToNode(pNode, pChildId, hResource);
    if (err != 0) {
        ReleaseRenderScanLock();
        return err;
    }
    FreeResObject(hResource);
    ReleaseRenderScanLock();
    return 0;
}


/* Frees the slot's resource (+6) and recycles the slot with FreeChildSlot. */
static void __cdecl DestroyChildEntry(int *pNode, short index)  /* 0x004386f0 */
{
    int pSlot;

    pSlot = GetChildEntry(index);
    FreeResObject((int *)*(int *)(pSlot + 6));
    FreeChildSlot(index);
}


/* Unlinks a slot from its hash bucket, the LRU chain and the active list, clears it and pushes
 * it on the free list (parent data +0x0c, chained through hashNext). */
static void __cdecl FreeChildSlot(short index)  /* 0x00438720 */
{
#pragma pack(push, 2)
    typedef struct {
        int   key;          /* +0x00 childId */
        char  linkCount;    /* +0x04 */
        char  flags;        /* +0x05 */
        int   handle;       /* +0x06 */
        int   stamp;        /* +0x0a order stamp */
        short hashNext;     /* +0x0e */
        short scanNext;     /* +0x10 */
        short scanPrev;     /* +0x12 */
        short prevActive;   /* +0x14 */
        short nextActive;   /* +0x16 */
    } FreeSlotRow;
#pragma pack(pop)
    short        bucket;
    short        sHead;
    FreeSlotRow *pSlot;         /* declared before pOther: pSlot in esi */
    FreeSlotRow *pOther;

    pSlot = (FreeSlotRow *)GetChildEntry(index);

    /* hash bucket chain */
    bucket = (short)(pSlot->key % (int)*(short *)((char *)gCurrentParentData + 0x0a));
    if (*(short *)((char *)gCurrentParentData + 0x18 + bucket * 2) == index)
    {
        *(short *)((char *)gCurrentParentData + 0x18 + bucket * 2) = pSlot->hashNext;
    }
    else
    {
        sHead = *(short *)((char *)gCurrentParentData + 0x18 + bucket * 2);
        pOther = (FreeSlotRow *)GetChildEntry(sHead);
        while (pOther->hashNext != index)
            pOther = (FreeSlotRow *)GetChildEntry(pOther->hashNext);
        pOther->hashNext = pSlot->hashNext;
    }

    /* LRU chain */
    if (pSlot->scanPrev >= 0)
        ((FreeSlotRow *)GetChildEntry(pSlot->scanPrev))->scanNext = pSlot->scanNext;
    else
        *(short *)((char *)gCurrentParentData + 0x0e) = pSlot->scanNext;   /* LRU tail */
    if (pSlot->scanNext >= 0)
        ((FreeSlotRow *)GetChildEntry(pSlot->scanNext))->scanPrev = pSlot->scanPrev;
    else
        *(short *)((char *)gCurrentParentData + 0x10) = pSlot->scanPrev;   /* MRU head */
    *(short *)((char *)gCurrentParentData + 0x16) -= 1;  /* live entry count */

    /* active list, if present */
    if ((*(short *)((char *)gCurrentParentData + 0x12) == index) ||
        (pSlot->prevActive >= 0) ||
        (pSlot->nextActive >= 0))
    {
        if (pSlot->nextActive >= 0)
            ((FreeSlotRow *)GetChildEntry(pSlot->nextActive))->prevActive = pSlot->prevActive;
        else
            *(short *)((char *)gCurrentParentData + 0x12) = pSlot->prevActive;
        if (pSlot->prevActive >= 0)
            ((FreeSlotRow *)GetChildEntry(pSlot->prevActive))->nextActive = pSlot->nextActive;
        *(short *)((char *)gCurrentParentData + 0x14) -= 1;  /* active count-- */
    }

    /* clear and push on the free list */
    pSlot->key        = -1;
    pSlot->linkCount  = 0;
    pSlot->flags      = 0;
    pSlot->handle     = 0;
    pSlot->stamp      = 0;
    pSlot->hashNext   = *(short *)((char *)gCurrentParentData + 0x0c); /* chain to old free head */
    pSlot->scanNext   = -1;
    pSlot->scanPrev   = -1;
    pSlot->prevActive = -1;
    pSlot->nextActive = -1;
    *(short *)((char *)gCurrentParentData + 0x0c) = index;  /* free list head = this */
}


/* Returns 1 if a node on gPendingRenderNodeList has the Pascal-string name pDescriptor
 * (compared without case), else 0. */
short MatchPendingRenderNodeDescriptor(byte *pDescriptor) /* 0x004388b0 */
{
    int           node;
    unsigned int  cmpResult = 0;

    node = gPendingRenderNodeList;
    while (node != 0) {
        cmpResult = (unsigned int)(unsigned short)PascalStringsEqualNoCase((byte *)(node + 0x24), pDescriptor);
        if ((short)cmpResult != 0)
            return 1;
        node = *(int *)(node + 0x54c);
    }
    return 0;
}


/* Reopens a container: takes the pending-close node named pDesc off gPendingRenderNodeList and
 * returns it in *pCtxOut, or opens it afresh with OpenContainerNode. */
short __cdecl TakeMatchingPendingRenderNode(byte *pDesc, int *pCtxOut)        /* 0x004388f0 */
{
    int  *piPred;
    int   cur;

    piPred = (int *)&gPendingRenderNodeList;
    cur    = gPendingRenderNodeList;
    while (cur != 0) {
        if (PascalStringsEqualNoCase((byte *)(cur + 0x24), pDesc) != 0) {
            /* found: unlink it from the pending list (the exe does this inside the walk) */
            *piPred = *(int *)(cur + 0x54c);
            *(int *)(cur + 0x54c) = 0;
            *(short *)(cur + 0x54a) = 0;
            *pCtxOut = cur;
            return 0;
        }
        piPred = (int *)(cur + 0x54c);
        cur    = *piPred;
    }
    return OpenContainerNode(pDesc, (int **)pCtxOut);
}


/* Closes a node lazily: marks it pending (+0x54a) and pushes it on gPendingRenderNodeList so a
 * quick reopen is cheap. If the list already holds four nodes, the fourth is destroyed first.
 * FatalError 0x45f if the node is already pending. */
short __cdecl DeferRenderNodeClose(int pCtx)                     /* 0x00438960 */
{
    unsigned int *pPending;
    short         walked;

    if (*(short *)(pCtx + 0x54a) != 0)
        FatalError(0, 0x45f);

    pPending = (unsigned int *)(uintptr_t)gPendingRenderNodeList;
    walked  = 0;
    for (;;) {
        if (pPending == NULL)
            goto register_head;

        walked++;
        if (walked >= 4) {               /* cmp cx,4 / jge, as in the exe */
            walked = DestroyRenderNode(pPending);
            if (walked != 0)
                return walked;
            goto register_head;
        }
        pPending = (unsigned int *)pPending[0x153];
    }

register_head:
    *(short *)(pCtx + 0x54a)       = 1;
    *(unsigned int *)(pCtx + 0x54c) = (unsigned int)(uintptr_t)gPendingRenderNodeList;
    gPendingRenderNodeList                   = pCtx;
    return 0;
}


/* Destroys every node in gRenderNodeList (DestroyRenderNode unlinks each as it goes). Returns
 * 0, or the first error. FatalError 0x460 if gPendingRenderNodeList is still non-empty after,
 * since pending nodes stay on the main list too. */
short __stdcall DestroyAllRenderNodes(void)  /* 0x004389d0 */
{
    short status;

    while (gRenderNodeList != NULL) {
        status = DestroyRenderNode(gRenderNodeList);
        if (status != 0)
            return status;
    }
    if (gPendingRenderNodeList != 0)
        FatalError(0, 0x460);
    return 0;
}

