/* attach_slots.c -- TI.EXE 0x00422080..0x00422260: the display-attach slot ring.
 * The ring functions come right after the slot init/shutdown in the March 1997 build, and the
 * cut before BootstrapContainerFile (container_store.c) is the file boundary. */

#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include "df_protos.h"
#include "topics/renderer.h"


/* One display-attach slot: 0x3e bytes, four of them at gAttachSlots (0x00489c78), the ring ending
 * at gAttachWaitCount (0x00489d70). state 0 = busy, 1 = done/free. pack(1) is required (pCtx is at +6).
 * Port note: replaces two identical function-local typedefs; the symbol count is fitted to this. */
#pragma pack(push, 1)
typedef struct AttachSlot {
    short         state;     /* +0x00 */
    unsigned int  chanId;    /* +0x02 */
    void         *pCtx;      /* +0x06 */
    unsigned char rec[0x34]; /* +0x0a record handed to PollDisplayAttachSlotCompletion */
} AttachSlot;                /* 0x3e */
#pragma pack(pop)
#define gAttachSlotTbl (*(AttachSlot (*)[4])gAttachSlots)

/* Called by BeginGameSession before the channel inits. Empty in this build (a plain return).
 * The March 1997 build marks the four slots free (FatalError(0x6e, 1000) on a second init),
 * starts the loader thread (StartDisplayAttachLoaderThread, container_store.c) and sets
 * gDisplayAttachLoaderRunning. */
void __stdcall InitDisplayAttachSlots(void)  /* 0x00422080 */
{
    return;
}

/* EndGameSession's last statement (`jmp 0x00422090`). Empty in this build: a bare RET, absent from
 * the Ghidra export because only that tail jump reaches it. The March 1997 build cancels every
 * busy slot, waits for each, stops the loader thread (StopDisplayAttachLoaderThread, container_store.c)
 * and clears gDisplayAttachLoaderRunning. */
void __stdcall ShutdownDisplayAttachSlots(void)  /* 0x00422090 */
{
}

/* Attaches the render child for (pCtx, chanId). If a busy slot is queued for that pair, waits for
 * its load to complete and registers the loaded handle (*ppHandleOut); otherwise, or if
 * registering fails, falls through to a plain AttachRenderChild. Returns 0 or an error status. */
short __cdecl ResolveDisplayAttachSlot(void *pCtx, unsigned int chanId,
                                      int **ppHandleOut)                      /* 0x004220a0 */
{
    AttachSlot *p;
    int    i;
    short  st;
    short  done;     /* the poll writes all three outputs on every path */
    short  aux;
    int   *hRes;

    if (gDisplayAttachLoaderRunning != 0) {
        i = 0;
        p = gAttachSlotTbl;
        do {
            if (p->state == 0 && p->pCtx == pCtx && p->chanId == chanId) {
                do {
                    PumpMessages();
                    st = PollDisplayAttachSlotCompletion(gAttachSlotTbl[i].rec,
                                                         &done, &aux, &hRes);
                    if (st != 0)
                        break;
                } while (done == 0);
                gAttachSlotTbl[i].state = 1;
                if (st != 0)
                    return st;
                if (aux != 0)
                    return aux;
                *ppHandleOut = hRes;
                if (AttachRenderChildHandle((int)pCtx, chanId, hRes) == 0)
                    return 0;
                /* registering failed: free the handle and fall through to the plain attach
                 * below (0x00422171 jmp 0x0042217f); the port used to return the status */
                FreeResObject(hRes);
                break;
            }
            p++;
            i++;
        } while (p < (gAttachSlotTbl + 4));
    }
    return AttachRenderChild((int *)pCtx, chanId, (int *)ppHandleOut);
}


/* Queues a display attach for (pCtx, chanId): claims the first free slot in the ring and fills its
 * record via SetupDisplayAttachSlotRecord. Does nothing when the loader is not running, the
 * pair already has a render child or a busy slot, or the ring is full. Returns 0 or an error status. */
short __cdecl ActivateDisplayAttachSlot(void *pCtx, unsigned int chanId)       /* 0x004221a0 */
{
    /* Do-while walks with no entry test, as in the exe. The end is formed from the base: the
     * port's globals are separate symbols, so &gAttachWaitCount is not a usable end. */
    AttachSlot *p;
    int i;
    short st;

    if (gDisplayAttachLoaderRunning != 0 && HasRenderChild((int *)pCtx, chanId) == 0) {
        p = gAttachSlotTbl;
        do {
            if (p->state == 0 && p->pCtx == pCtx && p->chanId == chanId)
                return 0;
            p++;
        } while (p < (gAttachSlotTbl + 4));
        i = 0;
        p = gAttachSlotTbl;
        do {
            if (p->state != 0)
                break;
            p++;
            i++;
        } while (p < (gAttachSlotTbl + 4));
        if (i >= 4)
            return 0;
        st = SetupDisplayAttachSlotRecord(pCtx, chanId, gAttachSlotTbl[i].rec);
        if (st != 0)
            return st;
        gAttachSlotTbl[i].chanId = chanId;
        gAttachSlotTbl[i].pCtx = pCtx;
        gAttachSlotTbl[i].state = 0;
        return 0;
    }
    return 0;
}


/* Cancels (UnregisterDisplayAttachWaitRecord) every busy slot queued for pCtx, then waits for each
 * to complete and marks it done. Failures are fatal: the 0x3ee / 0x3ef passed to FatalError
 * identify the failing step. */
void __cdecl DrainDisplayAttachSlotsForContext(int pCtx)                      /* 0x00422260 */
{
    short         status;
    short       *pSlot;
    short        completed;  /* local_2 */

    if (gDisplayAttachLoaderRunning == 0)
        return;

    pSlot = gAttachSlots;
    while (pSlot < DISPLAY_ATTACH_RING_END_SHORTS) {
        if (*pSlot == 0 && *(int *)(pSlot + 3) == pCtx) {
            status = UnregisterDisplayAttachWaitRecord((int *)(pSlot + 5));
            if (status != 0)
                FatalError(status, 0x3ee);
        }
        pSlot += 0x1f;    /* next slot: 0x3e bytes = 0x1f shorts */
    }

    pSlot = gAttachSlots;
    while (pSlot < DISPLAY_ATTACH_RING_END_SHORTS) {
        if (*pSlot == 0 && *(int *)(pSlot + 3) == pCtx) {
            do {
                PumpMessages();
                status = PollDisplayAttachSlotCompletion((void *)(pSlot + 5), &completed,
                                                         (short *)0, (int **)0);
                if (status != 0)
                    FatalError(status, 0x3ef);
            } while (completed == 0);
            *pSlot = 1;
        }
        pSlot += 0x1f;
    }
}
