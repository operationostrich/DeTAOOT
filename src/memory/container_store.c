/* container_store.c -- TI.EXE 0x00422310..0x00424100: container files, the node block store and
 * the display-attach wait list.
 * Both ends are file boundaries: the Mac and March 1997 links cut before BootstrapContainerFile,
 * and .data shows a second file's globals from 0x0045d6b8 (the sound functions from 0x004241e0 are
 * the next file, sound_channels.c). The loader thread after IsDisplayAttachLoaderRunning is
 * original source TI.EXE's link discarded. */

#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <malloc.h>
#include <stdint.h>
#include <mmsystem.h>
#include "df_protos.h"
#include "topics/renderer.h"
#include "topics/container.h"
#include "topics/nodestore.h"
#include "topics/multimedia.h"
#include "topics/sound.h"


/* Creates (or truncates) a container file and writes its empty image: the 1024-byte FileHeader
 * (magic 0x10000, file size, capacity, the two type tags -- 'ODTR'/'TRFD' for a save game) and an
 * offset table with room for childCount children (its capacity, rounded up to a multiple of 128). Returns the first failing
 * status (open, write), else CloseContainerFile's. */
short __cdecl BootstrapContainerFile(byte *pDescriptor,
                                            int childCount,
                                            unsigned int tagA,
                                            unsigned int tagB) /* 0x00422310 */
{
    short                     status;
    short                     wret;
    ContainerFileHandle       handleMem;
    unsigned char            *blob;
    DWORD                     cbWrite;
    unsigned int              chunks;
#if !DF_FIX_BOOTSTRAP_HEADER_BUFFER
    unsigned int              hdr[0x100];
    short                     i;
#endif

    status = OpenContainerFileCreateAlways(pDescriptor, tagA, tagB, &handleMem);
    if (status != 0)
        return status;

    if (childCount <= 0)
        childCount = 1;

    chunks  = ((unsigned int)childCount + 0x7FU) >> 7;
    cbWrite = (chunks + 2U) * 0x200U;

    /* Header words: magic, file size, two zero words, capacity (chunks << 7), type 0,
     * gapWhere 0, zero, then the two tags.
     * DF_FIX_BOOTSTRAP_HEADER_BUFFER: one zeroed buffer of the whole write size (the original
     * over-reads its stack header, #else). Port note: the port once wrote only the magic, and
     * the empty header failed with 101 the moment a save file was chosen. */
#if DF_FIX_BOOTSTRAP_HEADER_BUFFER
    blob = (unsigned char *)alloca(cbWrite);
    memset(blob, 0, (size_t)cbWrite);
    ((unsigned int *)blob)[0] = 0x10000;
    ((unsigned int *)blob)[1] = cbWrite;
    ((unsigned int *)blob)[4] = chunks << 7;
    ((unsigned int *)blob)[8] = tagA;
    ((unsigned int *)blob)[9] = tagB;

    wret = WriteToFileWithRetry((int *)&handleMem, blob, cbWrite);
#else
    {
        /* The exe's header is 0x400 bytes with only 0x80 dwords set; the
         * write of cbWrite (>= 0x600) bytes reads the rest of it and past it. */
        hdr[0] = 0x10000;
        hdr[1] = cbWrite;
        hdr[2] = 0;
        hdr[3] = 0;
        hdr[4] = chunks << 7;
        hdr[5] = 0;
        hdr[6] = 0;
        hdr[7] = 0;
        hdr[8] = tagA;
        hdr[9] = tagB;
        for (i = 0; i < 0x76; i++)
            hdr[10 + i] = 0;
        wret = WriteToFileWithRetry((int *)&handleMem, hdr, hdr[1]);
    }
#endif
    if (wret != 0) {
        CloseContainerFile(&handleMem);
        return wret;
    }
    return CloseContainerFile(&handleMem);
}


/* Deletes the file pName names; TryDeleteResourceFile's status, passed through in ax
 * (its one caller, DispatchPluginEngineRequest, returns it). */
short __cdecl DeleteResourceFile(byte *pName)  /* 0x004223F0 */
{
  return TryDeleteResourceFile(pName);
}


/* Opens the container pDescriptor names into the caller's node: validates its FileHeader and
 * preloads the offset-page cache. The node layout, used by every function below:
 *   +0x000  ContainerFileHandle (36 bytes)      +0x024  descriptor copy (256 bytes)
 *   +0x124  FileHeader (1024 bytes): +0x124 magic 0x10000, +0x128 file size, +0x12c total free
 *           bytes, +0x130 free-entry count, +0x134 offset-table capacity, +0x138 child count,
 *           +0x13c free-id count, +0x140 free-id list head, +0x324 free-block index
 *   +0x524  int LRU clock      +0x528 short cache slot count      +0x52a slot array
 *   +0x52e  short dirty flag (header needs writing back)
 * A cache slot (0x20a bytes) holds one page of 128 offsets: +0 first child index, +4 LRU
 * stamp, +8 short dirty flag, +0xa int[128]. One slot per page, at most 32.
 * Returns 0, 0x6a bad magic, 0x65 stored size differs from the file's, or an I/O status;
 * on failure the file is closed. */
short __cdecl ReadContainerIntoNode(byte *pDescriptor, int *pNode)  /* 0x00422400 */
{
    short  sErr;
    int   *pSlot;
    int    slotCount;
    DWORD  actualSize;
    ContainerFileHandle handleBuf;
    int    cacheSlots;

    sErr = OpenContainerFile(pDescriptor, &handleBuf);
    if (sErr != 0)
        return sErr;

    /* a struct copy: the exe's rep movsd, ecx = 9 */
    *(ContainerFileHandle *)pNode = handleBuf;

    /* the exe CALLs the CRT copy, which only memmove compiles to under /Oi */
    memmove(pNode + 9, pDescriptor, 0x100);

    pNode[0x149] = 0;
    *(short *)((char *)pNode + 0x528) = 0;

    /* The two straight-line checks sit in one-pass loops (perhaps the original's error-check
     * macro): a bare `if (e) goto fail;` makes VC++ 4.2 lay the shared exit right after the
     * fail block, and the early return then cross-jumps there, short of TI.EXE. */
    for (;;) {
        sErr = ReadFileBlock(&handleBuf, pNode + 0x49, 0x400);
        if (sErr != 0)
            goto fail;
        break;
    }

    if (pNode[0x49] != 0x10000) {
        sErr = 0x6a;   /* not a container file */
        goto fail;
    }

    for (;;) {
        sErr = GetActualFileSize(&handleBuf, &actualSize);
        if (sErr != 0)
            goto fail;
        break;
    }
    if ((DWORD)pNode[0x4a] != actualSize) {
        sErr = 0x65;
        goto fail;
    }

    cacheSlots = (unsigned int)pNode[0x4d] >> 7;
    if (cacheSlots > 0x20) cacheSlots = 0x20;

    pSlot = (int *)AllocEngineBuffer(cacheSlots * 0x20a);
    *(int **)((char *)pNode + 0x52a) = pSlot;

    for (slotCount = 0; slotCount < cacheSlots; slotCount++) {
        pSlot[0] = slotCount * 0x80;        /* first child index */
        pSlot[1] = pNode[0x149];            /* LRU stamp */
        pNode[0x149]++;
        *(short *)((char *)pSlot + 8) = 0;  /* clean */

        sErr = LoadOffsetPage(pNode, pSlot);
        if (sErr != 0)
            goto fail;

        pSlot      = (int *)((char *)pSlot + 0x20a);
    }

    *(short *)((char *)pNode + 0x52e) = 0;
    *(short *)((char *)pNode + 0x528) = (short)cacheSlots;
    return 0;

fail:
    CloseContainerFile(&handleBuf);
    return sErr;
}


/* Closes a node opened by ReadContainerIntoNode: writes back its dirty pages and header, frees
 * the slot array and closes the file. Returns the first failing status. */
short FlushAndCloseNodeStorage(int *pNode)  /* 0x00422580 */
{
    short err;

    err = FlushContainerNodeState(pNode);
    if (err != 0)
        return err;
    FreeEngineBuffer(*(int **)((char *)pNode + 0x52a));
    return CloseContainerFile(pNode);   /* the close's own status is returned (no xor ax,ax after it) */
}


/* Writes a dirty node back to its file: every dirty offset page, then the 1024-byte header at
 * offset 0, then FlushFileBuffers; clears the node's dirty flag only when all of it succeeded.
 * A clean node returns 0 at once. */
short __cdecl FlushContainerNodeState(int *pNode)  /* 0x004225b0 */
{
    short          i;
    int           *pEntry;
    short          err;

    if (*(short *)((char *)pNode + 0x52e) == 0)
        return 0;

    i      = 0;
    pEntry = *(int **)((char *)pNode + 0x52a);
    if (*(short *)((char *)pNode + 0x528) > 0)
    {
        do {
            if ((short)pEntry[2] != 0)    /* the page is dirty */
            {
                err = FlushOffsetPage(pNode, pEntry);
                if (err != 0)
                    return err;
                *(short *)(pEntry + 2) = 0;
            }
            i++;
            pEntry = (int *)((char *)pEntry + 0x20a);
        } while (i < *(short *)((char *)pNode + 0x528));
    }

    err = SeekContainerFile((int *)pNode, 0);
    if (err != 0)
        return err;

    err = WriteToFileWithRetry(pNode, (int *)((char *)pNode + 0x124), 0x400);
    if (err != 0)
        return err;

    err = FlushNodeFileBuffers(pNode);
    if (err != 0)
        return err;

    *(short *)((char *)pNode + 0x52e) = 0;
    return 0;
}


/* Reads a child's payload size from its block header. A block is an 8-byte header {child id,
 * payload size} followed by the payload, padded to 64 bytes (ComputeAlignedBlockSize).
 * Returns 0, 100 when the block at the child's offset carries another id, or an I/O status. */
short QueryChildAllocSize(int *pNode, unsigned int childId, int *pSizeOut)  /* 0x00422650 */
{
    short          err;
    unsigned int   fileOffset;
    int            header[2];   /* {stored id, payload size} */

    err = GetChildBlockOffset(pNode, childId, (int *)&fileOffset);
    if (err != 0)
        return err;

    err = (short)ReadBlockHeader(pNode, fileOffset, header);
    if (err != 0)
        return err;

    if (childId != (unsigned int)header[0])
    {
        DF_TRACE("!! QueryChildAllocSize: child %u at offset 0x%x has stored id "
                 "%u (size %d) -- mismatch", childId, fileOffset,
                 (unsigned)header[0], header[1]);
        return 100;
    }

    *pSizeOut = header[1];
    return 0;
}


/* Loads a child's payload into a new resource object, its handle in *pHandleOut.
 * Returns 0, 100 on an id mismatch, or an I/O status. */
short OpenChildNode(int *pNode, unsigned int childId, int **pHandleOut)  /* 0x004226b0 */
{
    short          err;
    int            header[2];   /* {stored id, payload size} */
    unsigned int   fileOffset;
    void          *pBuf;

    err = GetChildBlockOffset(pNode, childId, (int *)&fileOffset);
    if (err != 0)
        return err;

    err = ReadBlockHeader(pNode, fileOffset, header);
    if (err != 0)
        return err;

    if ((unsigned int)header[0] != childId)
    {
        DF_TRACE("!! OpenChildNode: child %u at offset 0x%x has stored id %u "
                 "(size %d) -- mismatch", childId, fileOffset,
                 (unsigned)header[0], header[1]);
        return 100;
    }

    /* the size is re-read from header[1] for each use, as the exe does */
    pBuf = (void *)GetResDataPtr(*pHandleOut = (int *)AllocResObject(header[1]));
    err = ReadFileBlock(pNode, pBuf, header[1]);
    if (err != 0)
        return err;
    UnlockResObject(*pHandleOut);
    return 0;
}


/* Replaces an existing child's payload with pHandle's data: in place when the size is unchanged,
 * else in a newly allocated block (the old one freed, the TOC updated). Marks the node dirty and
 * compacts the free index first. Returns 0, 100 on an id mismatch, or an I/O status. */
short WriteChildDataToNode(int *pNode, unsigned int childId, int *pHandle)  /* 0x00422740 */
{
    short          err;
    unsigned int   fileOffset;
    unsigned int   hdr[2];      /* ReadBlockHeader fills both: [0] id, [1] size */
    unsigned int   newSize;
    unsigned int   padSize;
    void          *pZeroBuf;
    const void    *pData;

    *(short *)((char *)pNode + 0x52e) = 1;

    err = CompactNodeBlockStore(pNode);
    if (err != 0)
        return err;

    err = GetChildBlockOffset(pNode, childId, (int *)&fileOffset);
    if (err != 0)
        return err;
    err = ReadBlockHeader(pNode, fileOffset, (int *)hdr);
    if (err != 0)
        return err;

    if (childId != hdr[0])
        return 100;

    newSize = GetResObjectSize(pHandle);
    if (newSize == hdr[1]) {
        pData = (const void *)GetResDataPtr(pHandle);
        err = WriteToFileWithRetry(pNode, pData, newSize);
        if (err != 0)
            return err;
        UnlockResObject(pHandle);
        return 0;
    }

    /* the aligned sizes are computed where they are used, as in the exe */
    FreeNodeBlock(pNode, fileOffset, ComputeAlignedBlockSize(hdr[1]));
    hdr[1] = newSize;
    AllocNodeBlock(pNode, ComputeAlignedBlockSize(newSize), (int *)&fileOffset);
    err = WriteBlockHeader(pNode, fileOffset, (const void *)hdr);
    if (err != 0)     /* the exe tests the header write's status */
        return err;

    padSize = ComputeAlignedBlockSize(hdr[1]) - 8;
    pData = (const void *)GetResDataPtr(pHandle);
    padSize -= hdr[1];
    err = WriteToFileWithRetry(pNode, pData, hdr[1]);
    UnlockResObject(pHandle);
    if (err != 0)
        return err;

    if (padSize != 0) {
        pZeroBuf = AllocEngineBuffer(padSize);
        ZeroBlock(pZeroBuf, padSize);
        err = WriteToFileWithRetry(pNode, pZeroBuf, padSize);
        FreeEngineBuffer((int)pZeroBuf);
        if (err != 0)
            return err;
    }

    return UpdateChildBlockOffset(pNode, childId, (int)fileOffset);
}


/* Stores hResource's data as a new child: allocates a child id (*pChildId) and a block, writes
 * header, payload and zero padding, and records the block's offset in the TOC. Marks the node
 * dirty. Returns 0 or the first failing status. */
short __cdecl AddChildResourceToNode(int *pNode, uint32_t *pChildId, int *hResource) /* 0x004228F0 */
{
    short    status;
    int      pad;               /* int: the exe's `lea ebp,[eax-8]` / `test ebp,ebp` */
    uint32_t *pPad;
    /* Port note: ONE 8-byte header {childId, dataSize} handed to WriteBlockHeader by address;
     * ported as two locals, the size on disk was stack garbage (port-bug-classes.md #3). */
    uint32_t hdr[2];
    uint32_t blockOffset;  /* local_4 */
    LPCVOID  pv;
    int      iv;

    *(short *)((char *)pNode + 0x52e) = 1;
    status = CompactNodeBlockStore((uint32_t *)pNode);
    if (status != 0)
        return status;
    status = AllocateChildId(pNode, pChildId);
    if (status != 0)
        return status;
    hdr[0] = *pChildId;
    hdr[1] = GetResObjectSize(hResource);
    AllocNodeBlock((int)pNode, ComputeAlignedBlockSize(hdr[1]), (int *)&blockOffset);
    status = WriteBlockHeader((int *)pNode, blockOffset, hdr);   /* tested: test ax,ax / jnz */
    if (status != 0)
        return status;
    iv  = ComputeAlignedBlockSize(hdr[1]) - 8;   /* `lea ebp,[eax-8]` before the lookup */
    pv  = (LPCVOID)GetResDataPtr(hResource);
    pad = iv - hdr[1];
    status = WriteToFileWithRetry((uint32_t *)pNode, pv, hdr[1]);
    UnlockResObject(hResource);
    if (status != 0)
        return status;
    if (pad != 0) {
        pPad = (uint32_t *)AllocEngineBuffer(pad);
        ZeroBlock(pPad, pad);
        status = WriteToFileWithRetry((uint32_t *)pNode, pPad, pad);
        FreeEngineBuffer((int)pPad);
        if (status != 0)
            return status;
    }
    return UpdateChildBlockOffset((uint32_t *)pNode, *pChildId, blockOffset);
}


/* Deletes a child: frees its block and pushes its id on the free-id list (the id's TOC slot
 * takes the old head, node[0x50]; node[0x4f] counts them). Marks the node dirty.
 * Returns 0, 100 on an id mismatch, or an I/O status. */
short __cdecl DeleteChildFromNode(uint32_t *pNode, uint32_t childId)  /* 0x00422A20 */
{
  short err;
  uint32_t fileOffset;
  uint32_t header[2];   /* ReadBlockHeader fills both: [0] id, [1] size */

  *(short *)((char *)pNode + 0x52e) = 1;
  err = CompactNodeBlockStore((int *)pNode);
  if (err != 0)
    return err;
  err = GetChildBlockOffset((int *)pNode, childId, (int *)&fileOffset);
  if (err != 0)
    return err;
  err = ReadBlockHeader((int *)pNode, fileOffset, (int *)header);
  if (err != 0)
    return err;
  if (header[0] != childId)
    return 100;
  err = UpdateChildBlockOffset((int *)pNode, childId, pNode[0x50]);
  if (err != 0)
    return err;
  pNode[0x50] = childId;
  pNode[0x4f]++;
  FreeNodeBlock((int *)pNode, fileOffset, ComputeAlignedBlockSize(header[1]));
  return 0;
}


/* Reads the 8-byte block header at fileOffset into pOut: {the child id it was written for,
 * payload size}. Returns the seek's status, else the read's. */
short ReadBlockHeader(int *pNode, unsigned int fileOffset, int *pOut)  /* 0x00422ad0 */
{
    short err;

    err = SeekContainerFile(pNode, fileOffset);
    if (err != 0)
        return err;
    return ReadFileBlock(pNode, pOut, 8);   /* the read's own status is returned */
}


/* Writes the 8-byte block header {child id, payload size} at fileOffset; ReadBlockHeader's
 * counterpart. Returns the seek's status, else the write's (both callers test it). */
short WriteBlockHeader(int *pNode, unsigned int fileOffset, const void *pHeader)  /* 0x00422b00 */
{
    short err;
    err = SeekContainerFile(pNode, fileOffset);
    if (err == 0)
        err = WriteToFileWithRetry(pNode, pHeader, 8);
    return err;
}


/* Hands out the child id for a block about to be written: a recycled id from the free list
 * (head node[0x50], count node[0x4f]; a freed child's offset slot links to the next free id),
 * else the next fresh id node[0x4e]++ after EnsureChildTableCapacity has made room for it. */
short __cdecl AllocateChildId(int *pNode, unsigned int *pIdOut)  /* 0x00422b30 */
{
    unsigned int id;
    int next;
    short sErr;

    /* the free list first (the exe tests != 0 and falls into it) */
    if (pNode[0x4f] != 0) {
        id = (unsigned int)pNode[0x50];
        *pIdOut = id;
        sErr = GetChildBlockOffset(pNode, id, &next);
        if (sErr != 0)
            return sErr;
        pNode[0x4f]--;
        pNode[0x50] = next;
        return 0;
    }
    sErr = (short)EnsureChildTableCapacity(pNode);
    if (sErr != 0)
        return sErr;
    *pIdOut = (unsigned int)pNode[0x4e];
    pNode[0x4e]++;
    return 0;
}


/* Called before a new child id is handed out. When the child count (node[0x4e]) has reached
 * the offset-table capacity (node[0x4d]), grows the table by 128 entries (0x200 bytes): the
 * blocks right after the table are absorbed (a free block) or moved elsewhere (a live one) until
 * 0x200 bytes are clear; any excess goes back to the free index. Returns 0 or an I/O status.
 *
 * Port note: a guessed body (a copy of the offset lookup) once failed every new container with
 * "child 0 out of range", so a fresh save file could not take its first block. */
short __cdecl EnsureChildTableCapacity(int *pNode)  /* 0x00422bb0 */
{
    /* The exe's shape: the free-index entry copied out whole (its size also read through the
     * pointer first), a `needMove` flag, and the aligned size recomputed at each use. */
    typedef struct { int pos; int size; } FreeEnt;
    int          hdr[2];          /* {childId, blockSize} */
    int          newPos;
    FreeEnt      ent;
    unsigned int moved;
    int          cursor;
    short        err;
    FreeEnt     *pe;
    short        needMove;
    int          size;

    moved = 0;
    if (pNode[0x4e] < pNode[0x4d])
        return 0;
    cursor = pNode[0x4d] * 4 + 0x400;   /* first byte after the offset table */
    do {
        needMove = 1;
        if (pNode[0x4c] != 0) {
            /* A free block begins at or before the cursor: absorb it. */
            pe = (FreeEnt *)&pNode[0xc9];
            size = pe->size;
            ent = *pe;
            if (cursor >= ent.pos) {
                RemoveFreeBlockEntry(pNode, 0);
                cursor += size;
                moved  += size;
                needMove = 0;
            }
        }
        if (needMove != 0) {
            /* A live block sits at the cursor: move it out of the way. */
            err = ReadBlockHeader(pNode, cursor, hdr);
            if (err != 0)
                return err;
            AllocNodeBlock(pNode, ComputeAlignedBlockSize(hdr[1]), &newPos);
            err = UpdateChildBlockOffset(pNode, hdr[0], newPos);
            if (err != 0)
                return err;
            err = ShiftFileRegionToHigherOffset((uint32_t *)pNode, cursor, newPos, ComputeAlignedBlockSize(hdr[1]));
            if (err != 0)
                return err;
            cursor += ComputeAlignedBlockSize(hdr[1]);
            moved  += ComputeAlignedBlockSize(hdr[1]);
        }
    } while (moved < 0x200);
    pNode[0x4d] += 0x80;
    moved -= 0x200;
    if ((int)moved > 0) {
        cursor -= moved;
        FreeNodeBlock(pNode, cursor, moved);
    }
    return 0;
}


/* Allocates alignedSize bytes of the node's file, its position in *pFilePosOut; FreeNodeBlock's
 * counterpart. Best fit over the free index: the smallest free block that is large enough is
 * taken whole or shrunk from the front; with none, the file grows (node[0x4a], the file size).
 *
 * Returns the position on the end-of-file path only; the other paths fall off the end and every
 * caller ignores the result. That return is a byte-match spelling: the web C2 coalesces into eax
 * gives the exe's ecx/edx choice for pNode/alignedSize. The volatile re-read stops C2 CSE-ing it
 * with the stored value (it becomes mov eax,eax). */
int AllocNodeBlock(int *pNode, int alignedSize, int *pFilePosOut)  /* 0x00422cf0 */
{
    /* The free index at node +0x324: {filePos, size} pairs sorted by position, copied whole.
     * bestIdx is uninitialised in the exe too: it is read only once best.pos has been set. */
    typedef struct { int pos; int size; } FreeEnt;
    FreeEnt e;
    FreeEnt best;
    short   i;
    short   bestIdx;
    int     pos;

    best.size = 0x7fffffff;
    best.pos  = 0;
    for (i = 0; i < pNode[0x4c]; i++) {
        e = ((FreeEnt *)(pNode + 0xc9))[i];
        if (e.size >= alignedSize && best.size > e.size) {
            best    = e;
            bestIdx = i;
        }
    }
    if (best.pos != 0) {
        *pFilePosOut = best.pos;
        if (best.size <= alignedSize) {
            RemoveFreeBlockEntry(pNode, bestIdx);
        } else {
            /* shrink the entry from the front */
            best.pos  += alignedSize;
            best.size -= alignedSize;
            pNode[0x4b] -= alignedSize;
            ((FreeEnt *)(pNode + 0xc9))[bestIdx] = best;
        }
    } else {
        *pFilePosOut = pNode[0x4a];
        pos = *(volatile int *)pFilePosOut;
        pNode[0x4a] += alignedSize;
        return pos;
    }
}


/* Copies alignedSize bytes of the node's file from srcOff down to dstOff, front to back, through
 * a buffer of at most GetLargestFreeBlockSize bytes; CompactNodeBlockStore slides live blocks
 * down with it. dstOff must be below srcOff (FatalError(0x66, 0x43b) otherwise).
 * Returns 0 or the first I/O status (the buffer is then left allocated, as in the exe). */
short MoveNodeBlock(int *pNode, unsigned int srcOff, unsigned int dstOff, unsigned int alignedSize)  /* 0x00422dd0 */
{
    int            xferSize;
    void          *pTempHandle;
    short          err;
    int            chunkSize;
    void          *pBuf;

    if ((int)dstOff >= (int)srcOff)
        FatalError(0x66, 0x43b);

    xferSize = GetLargestFreeBlockSize();
    if (xferSize > (int)alignedSize)
        xferSize = alignedSize;

    pTempHandle = AllocResObject(xferSize);
    pBuf        = (void *)GetResDataPtr(pTempHandle);

    do {
        chunkSize = alignedSize;
        if (xferSize < (int)alignedSize)
            chunkSize = xferSize;

        err = SeekContainerFile(pNode, srcOff);
        if (err != 0)
            return err;
        err = ReadFileBlock(pNode, pBuf, chunkSize);
        if (err != 0)
            return err;

        err = SeekContainerFile(pNode, dstOff);
        if (err != 0)
            return err;
        err = WriteToFileWithRetry(pNode, pBuf, chunkSize);
        if (err != 0)
            return err;

        alignedSize -= chunkSize;
        srcOff      += chunkSize;
        dstOff      += chunkSize;
    } while ((int)alignedSize > 0);

    UnlockResObject(pTempHandle);
    FreeResObject(pTempHandle);
    return 0;
}



/* MoveNodeBlock's mirror: moves length bytes up from srcOffset to dstOffset, which must be above
 * it (FatalError(0x66, 0x43c) otherwise), copying from the end back so an overlapping
 * region stays intact. Returns 0 or the first I/O status. */
short __cdecl ShiftFileRegionToHigherOffset(uint32_t *pHandle, int srcOffset,
                                           uint dstOffset, uint length)  /* 0x00422EB0 */
{
  int    chunkMax;
  void  *piRes;
  void  *pBuf;
  short  err;
  int    uChunk;

  if ((int)dstOffset <= srcOffset)
    FatalError(0x66, 0x43c);
  chunkMax = GetLargestFreeBlockSize();
  if ((int)length < chunkMax)
    chunkMax = length;
  piRes = AllocResObject(chunkMax);
  pBuf = (void *)GetResDataPtr(piRes);
  /* both offsets walk down from the region's end, in place */
  srcOffset += length;
  dstOffset += length;
  do {
    uChunk = length;
    if ((int)length > chunkMax)
      uChunk = chunkMax;
    srcOffset -= uChunk;
    dstOffset -= uChunk;
    err = SeekContainerFile((ContainerFileHandle *)pHandle, srcOffset);
    if (err != 0)
      return err;
    err = ReadFileBlock((ContainerFileHandle *)pHandle, pBuf, uChunk);
    if (err != 0)
      return err;
    err = SeekContainerFile((ContainerFileHandle *)pHandle, dstOffset);
    if (err != 0)
      return err;
    err = WriteToFileWithRetry((int *)pHandle, pBuf, uChunk);
    if (err != 0)
      return err;
    length -= uChunk;
  } while ((int)length > 0);
  UnlockResObject(piRes);
  FreeResObject(piRes);
  return 0;
}


/* Once the free index holds 64 entries or more, closes one gap: the live blocks after it slide
 * down over it and it merges into the next free block. Of the first 63 gaps it picks the one
 * followed by the least live data (the cheapest move). Run before every write and delete.
 * Returns 0 or an I/O status. */
short __cdecl CompactNodeBlockStore(int *pNode)  /* 0x00422fa0 */
{
    typedef struct { int pos; int size; } FreeEnt;
    short   err;
    short   idx;
    int     live;
    int     minLive;
    short   i;
    FreeEnt cur;
    FreeEnt next;
    int     dst;
    int     src;
    int     hdr[2];

    if ((unsigned int)pNode[0x4c] < 0x40)
        return 0;
    minLive = 0x7fffffff;
    for (i = 0; i < 0x3fU; i++) {
        cur  = ((FreeEnt *)(pNode + 0xc9))[i];
        next = ((FreeEnt *)(pNode + 0xc9))[i + 1];
        live = next.pos - cur.size - cur.pos;
        if (minLive > live) {
            idx     = i;
            minLive = live;
        }
    }
    cur  = ((FreeEnt *)(pNode + 0xc9))[idx];
    next = ((FreeEnt *)(pNode + 0xc9))[idx + 1];
    dst = cur.pos;
    src = cur.pos + cur.size;
    do {
        err = ReadBlockHeader(pNode, src, hdr);
        if (err != 0)
            return err;
        err = UpdateChildBlockOffset(pNode, hdr[0], dst);
        if (err != 0)
            return err;
        err = MoveNodeBlock(pNode, src, dst, ComputeAlignedBlockSize(hdr[1]));
        if (err != 0)
            return err;
        dst += ComputeAlignedBlockSize(hdr[1]);
        src += ComputeAlignedBlockSize(hdr[1]);
    } while (src < next.pos);
    /* the gap moves up into the next free entry */
    next.pos  -= cur.size;
    next.size += cur.size;
    pNode[0x4b] += cur.size;
    ((FreeEnt *)(pNode + 0xc9))[idx + 1] = next;
    RemoveFreeBlockEntry(pNode, idx);
    return 0;
}


/* Returns a block to the free index: inserted in position order, counted into the total free
 * bytes (node[0x4b]), then merged with its neighbours by CoalesceFreeBlocks. */
void FreeNodeBlock(int *pNode, unsigned int filePos, unsigned int alignedSize)  /* 0x004230f0 */
{
    /* entries are copied whole through a stack temporary, as the exe does */
    typedef struct { int pos; int size; } FreeEnt;
    short   i;
    FreeEnt e;
    short   slot;
    int     count;

    slot  = 0;
    count = pNode[0x4c];
    /* insertion point: first entry past filePos (a signed compare, `jg`) */
    if (count > 0) {
        do {
            e = ((FreeEnt *)(pNode + 0xc9))[slot];
            if (e.pos > (int)filePos)
                break;
            slot++;
        } while (slot < count);
    }
    for (i = (short)(count - 1); slot <= i; i--) {
        e = ((FreeEnt *)(pNode + 0xc9))[i];
        ((FreeEnt *)(pNode + 0xc9))[i + 1] = e;
    }
    e.pos  = (int)filePos;
    e.size = (int)alignedSize;
    ((FreeEnt *)(pNode + 0xc9))[slot] = e;
    pNode[0x4c]++;
    pNode[0x4b] += (int)alignedSize;
    CoalesceFreeBlocks(pNode);
}


/* Merges each free-index entry that reaches the next one's start into it. The removed entry's
 * size is added to the total here because RemoveFreeBlockEntry subtracts it. */
void CoalesceFreeBlocks(int *pNode)  /* 0x004231c0 */
{
    typedef struct { int pos; int size; } FreeEnt;
    FreeEnt cur;
    FreeEnt next;
    short   i;

    for (i = 0; i < pNode[0x4c] - 1; i++) {
        cur  = ((FreeEnt *)(pNode + 0xc9))[i];
        next = ((FreeEnt *)(pNode + 0xc9))[i + 1];
        if (cur.size + cur.pos >= next.pos) {
            cur.size += next.size;
            ((FreeEnt *)(pNode + 0xc9))[i] = cur;
            pNode[0x4b] += next.size;
            RemoveFreeBlockEntry(pNode, (short)(i + 1));
            i--;
        }
    }
}


/* Removes entry `index` from the free index, subtracting its size from the total free bytes. */
void RemoveFreeBlockEntry(int *pNode, short index)  /* 0x00423250 */
{
    typedef struct { int pos; int size; } FreeEnt;
    short i;
    FreeEnt e;

    /* copied whole before its size is subtracted, as in the exe */
    e = ((FreeEnt *)(pNode + 0xc9))[index];
    pNode[0x4b] -= e.size;
    for (i = index + 1; i < pNode[0x4c]; i++)
        ((FreeEnt *)(pNode + 0xc9))[i - 1] = ((FreeEnt *)(pNode + 0xc9))[i];
    pNode[0x4c]--;
}


/* Looks up a child's block offset (the TOC entry) through the offset-page cache. On a miss the
 * slot array grows by one while it has fewer than 32 slots; after that the least recently used
 * slot is reused, written back first if dirty. Every access stamps the slot from the node's LRU
 * clock. Returns 0, 100 for a child index outside the child count, or an I/O status. */
short __cdecl GetChildBlockOffset(int *pNode, unsigned int childIdx, int *pFileOffsetOut)  /* 0x004232b0 */
{
    /* The exe's shape: the array base doubles as the chosen slot (ebp), the
     * scan walks a separate pointer, the grow path re-reads the node's array
     * and count (the CRT copy, 0x0044e440, is memmove), and each path loads
     * its page itself. evictIdx is not initialised by the exe either: every
     * stamp is below 0x7fffffff, so a full table always sets it. */
    unsigned int *pSlot;
    unsigned int page;       /* aligned page base: childIdx & ~0x7f */
    unsigned int *p;         /* walking pointer */
    short n;
    short err;
    int lruStamp;            /* lowest stamp seen so far (for LRU eviction) */
    short evictIdx;
    /* No slot-count local: the slot count is read from the node field each
     * time; the compiler's own temp for it is the exe's extra frame word. */

    if ((int)childIdx < 0)
        return 100;
    if ((int)childIdx >= pNode[0x4e]) {
        DF_TRACE("!! GetChildBlockOffset: child %d out of range (node child "
                 "count = %d)", (int)childIdx, pNode[0x4e]);
        return 100;
    }
    n        = 0;
    pSlot    = *(unsigned int **)((char *)pNode + 0x52a);
    p        = pSlot;
    lruStamp = 0x7fffffff;
    page     = childIdx & 0xffffff80u;
    for (; n < *(short *)((char *)pNode + 0x528); n++, p = (unsigned int *)((char *)p + 0x20a)) {
        if (*p == page) {
            p[1] = (unsigned int)pNode[0x149];
            pNode[0x149]++;
            *pFileOffsetOut = *(int *)((char *)p + (childIdx - page) * 4 + 10);
            return 0;
        }
        if (lruStamp > (int)p[1]) {
            evictIdx = n;
            lruStamp = p[1];
        }
    }
    if (*(short *)((char *)pNode + 0x528) < 0x20) {
        *(unsigned int **)((char *)pNode + 0x52a) =
            (unsigned int *)AllocEngineBuffer((*(short *)((char *)pNode + 0x528) + 1) * 0x20a);
        memmove(*(unsigned int **)((char *)pNode + 0x52a), pSlot,
                *(short *)((char *)pNode + 0x528) * 0x20a);
        FreeEngineBuffer((int)pSlot);
        pSlot = (unsigned int *)(*(char **)((char *)pNode + 0x52a) +
                                 (*(short *)((char *)pNode + 0x528))++ * 0x20a);
        *pSlot   = page;
        pSlot[1] = (unsigned int)pNode[0x149];
        pNode[0x149]++;
        *(short *)(pSlot + 2) = 0;
        err = LoadOffsetPage(pNode, (int *)pSlot);
        if (err != 0)
            return err;
        *pFileOffsetOut = *(int *)((char *)pSlot + (childIdx - page) * 4 + 10);
        return 0;
    }
    pSlot = (unsigned int *)((char *)pSlot + evictIdx * 0x20a);
    if (*(short *)(pSlot + 2) != 0) {
        err = FlushOffsetPage(pNode, (int *)pSlot);
        if (err != 0)
            return err;
    }
    *pSlot   = page;
    pSlot[1] = (unsigned int)pNode[0x149];
    pNode[0x149]++;
    *(short *)(pSlot + 2) = 0;
    err = LoadOffsetPage(pNode, (int *)pSlot);
    if (err != 0)
        return err;
    *pFileOffsetOut = *(int *)((char *)pSlot + (childIdx - page) * 4 + 10);
    return 0;
}


/* GetChildBlockOffset's write counterpart: stores fileOffset as the child's TOC entry in the
 * cached page (loaded as there on a miss) and marks the slot dirty. A free id's entry holds the
 * next free id instead. Returns 0, 100 for a child index outside the child count, or an I/O
 * status. */
short UpdateChildBlockOffset(int *pNode, unsigned int childIdx, int fileOffset)  /* 0x00423490 */
{
    /* GetChildBlockOffset's shape (see there); the slot is marked dirty */
    unsigned int *pSlot;
    unsigned int page;
    unsigned int *p;
    short n;
    short err;
    int lruStamp;
    int stamp;
    short evictIdx;

    if ((int)childIdx < 0)
        return 100;
    if ((int)childIdx >= pNode[0x4e])
        return 100;
    n        = 0;
    pSlot    = *(unsigned int **)((char *)pNode + 0x52a);
    p        = pSlot;
    lruStamp = 0x7fffffff;
    page     = childIdx & 0xffffff80u;
    for (; n < *(short *)((char *)pNode + 0x528); n++, p = (unsigned int *)((char *)p + 0x20a)) {
        if (*p == page) {
            p[1] = (unsigned int)pNode[0x149];
            pNode[0x149]++;
            *(short *)(p + 2) = 1;
            *(int *)((char *)p + (childIdx - page) * 4 + 10) = fileOffset;
            return 0;
        }
        stamp = p[1];
        if (stamp < lruStamp) {
            evictIdx = n;
            lruStamp = stamp;
        }
    }
    if (*(short *)((char *)pNode + 0x528) < 0x20) {
        *(unsigned int **)((char *)pNode + 0x52a) =
            (unsigned int *)AllocEngineBuffer((*(short *)((char *)pNode + 0x528) + 1) * 0x20a);
        memmove(*(unsigned int **)((char *)pNode + 0x52a), pSlot,
                *(short *)((char *)pNode + 0x528) * 0x20a);
        FreeEngineBuffer((int)pSlot);
        pSlot = (unsigned int *)(*(char **)((char *)pNode + 0x52a) +
                                 (*(short *)((char *)pNode + 0x528))++ * 0x20a);
        *pSlot   = page;
        pSlot[1] = (unsigned int)pNode[0x149];
        pNode[0x149]++;
        *(short *)(pSlot + 2) = 1;
        err = LoadOffsetPage(pNode, (int *)pSlot);
        if (err != 0)
            return err;
        *(int *)((char *)pSlot + (childIdx - page) * 4 + 10) = fileOffset;
        return 0;
    }
    pSlot = (unsigned int *)((char *)pSlot + evictIdx * 0x20a);
    if (*(short *)(pSlot + 2) != 0) {
        err = FlushOffsetPage(pNode, (int *)pSlot);
        if (err != 0)
            return err;
    }
    *pSlot   = page;
    pSlot[1] = (unsigned int)pNode[0x149];
    pNode[0x149]++;
    *(short *)(pSlot + 2) = 1;
    err = LoadOffsetPage(pNode, (int *)pSlot);
    if (err != 0)
        return err;
    *(int *)((char *)pSlot + (childIdx - page) * 4 + 10) = fileOffset;
    return 0;
}


/* Reads a cache slot's page of the offset table from the file. The table follows the 1024-byte
 * header (file offset 0x400 + 4 * child index); a page is 128 entries, 0x200 bytes. Returns the
 * seek's status, else the read's (Ghidra shows void: the status is left in ax). */
short __cdecl LoadOffsetPage(int *pNode, int *pSlot)  /* 0x00423660 */
{
    short sErr;

    sErr = SeekContainerFile((ContainerFileHandle *)pNode, (unsigned int)(pSlot[0] * 4 + 0x400));
    if (sErr == 0) {
        sErr = ReadFileBlock((ContainerFileHandle *)pNode,
                             (void *)((char *)pSlot + 0x0a),
                             0x200);
    }
    return sErr;
}


/* LoadOffsetPage's counterpart: writes a cache slot's page back to the offset table.
 * Returns the seek's status, else the write's. */
short __cdecl FlushOffsetPage(int *pNode, int *pEntry)  /* 0x004236A0 */
{
    short status;

    status = SeekContainerFile((ContainerFileHandle *)pNode,
                               (unsigned int)(*pEntry * 4 + 0x400));
    if (status == 0)                    /* test ax,ax on the seek's own status */
    {
        /* the exe returns the write's status (call, add esp, pop, pop, ret) */
        return (short)WriteToFileWithRetry(pNode, (const void *)((char *)pEntry + 10), 0x200u);
    }
    return status;
}


/* The file space a block with dataSize bytes of payload takes: the 8-byte header plus the
 * payload, rounded up to 64 bytes. */
int ComputeAlignedBlockSize(int dataSize)  /* 0x004236e0 */
{
    return (dataSize + 0x47) / 64 * 64;   /* header (8) + align-1 (63), then a signed /64*64: cdq / and / sar / shl */
}


/* Reads size bytes at the handle's position into pDest, in pieces of at most 32 KB, pumping
 * messages before each so the UI stays alive during slow CD reads. Each piece seeks to the
 * stored position first (SeekContainerFile only records it). A failed piece puts up the disk
 * error box (strings 0x90 text, 0x91 title): Abort returns 0x6b, Ignore returns 0 with the read
 * unfinished, Retry tries the piece again. Holds the handle's lock throughout. */
short __cdecl ReadFileBlock(ContainerFileHandle *pHandle, void *pDest, unsigned int size)  /* 0x00423700 */
{
    DWORD  chunk;
    DWORD  bytesRead;
    short  ok;          /* the exe tests ReadFile's BOOL as a word (`test ax,ax`) */
    short  dlgResult;
    char   errMsg[384];

    EnterCriticalSection(&pHandle->critsec);
    pHandle->state = 1;

    while (size != 0) {
        chunk = 0x8000;
        PumpMessages();
        if (size < chunk)
            chunk = size;

        if (SetFilePointer(pHandle->hFile, pHandle->filePos, NULL, FILE_BEGIN)
                != INVALID_SET_FILE_POINTER) {
            ok = (short)ReadFile(pHandle->hFile, pDest, chunk, &bytesRead, NULL);
            if (ok && bytesRead == chunk) {
                pDest            = (void *)((char *)pDest + chunk);
                size            -= chunk;
                pHandle->filePos += chunk;
                continue;
            }
        }

        /* The exe tests the answer as a word (`test ax,ax`): this caller saw PromptDiskRetry
         * declared `short` while its definition returns int (a 1996 cross-file disagreement). */
        sprintf(errMsg, GetStringResource(0x90), GetLastError());
        dlgResult = (short)PromptDiskRetry(GetStringResource(0x91), errMsg);
        if (dlgResult < 0) {
            pHandle->state = 0;
            LeaveCriticalSection(&pHandle->critsec);
            return 0x6b;
        }
        if (dlgResult > 0) {
            pHandle->state = 0;
            LeaveCriticalSection(&pHandle->critsec);
            return 0;
        }
    }
    pHandle->state = 0;
    LeaveCriticalSection(&pHandle->critsec);
    PumpMessages();
    return 0;
}


/* The Abort / Retry / Ignore box around a disk error, the message word-wrapped, with the game
 * UI suspended. Returns -1 for Abort, 1 for Ignore, 0 for Retry. The exe's flags are 0x2132:
 * task-modal, default button 2 (Retry), warning icon. */
int __cdecl PromptDiskRetry(const char *pTitle, const char *pMessage)  /* 0x00423830 */
{
    int  r;
    char text[512];

    WordWrapText((char *)pMessage, text);
    SuspendGameUiForModalDialog();
    r = MessageBoxA(NULL, text, pTitle, MB_ABORTRETRYIGNORE | MB_ICONWARNING | MB_DEFBUTTON2 | MB_TASKMODAL);
    RestoreGameUiAfterModalDialog();
    if (r == IDABORT)
        return -1;
    return r == IDIGNORE;
}


/* ReadFileBlock's write counterpart, in one piece and without the message pump: writes cb bytes
 * at the handle's position (pCtx is a ContainerFileHandle) and advances it. A failure puts up the
 * disk error box (strings 0x92 text, 0x93 title): Abort returns 0x6b, Ignore 0, Retry again.
 *
 * Port note: the original's success return reads `return uVar1 & 0xffff0000` in Ghidra, which is
 * 0; the port once returned 1 and every save reported "not enough space" (port-bug-classes.md
 * #18). */
short __cdecl WriteToFileWithRetry(int *pCtx, const void *pSrc, DWORD cb) /* 0x004238a0 */
{
    DWORD    got;
    char     errMsg[384];
    int      uDlg;

    EnterCriticalSection((LPCRITICAL_SECTION)(pCtx + 2));
    pCtx[8] = 1;
    for (;;)
    {
        SetFilePointer((HANDLE)(UINT_PTR)(unsigned)pCtx[0], (LONG)pCtx[1], NULL, FILE_BEGIN);
        if (WriteFile((HANDLE)(UINT_PTR)(unsigned)pCtx[0], pSrc, cb, &got, NULL) && got == cb)
        {
            pCtx[1] = (int)((DWORD)pCtx[1] + cb);
            pCtx[8] = 0;
            LeaveCriticalSection((LPCRITICAL_SECTION)(pCtx + 2));
            return 0;
        }
        sprintf(errMsg, (char *)GetStringResource(0x92u), (unsigned long)GetLastError());
        uDlg = PromptDiskRetry((const char *)GetStringResource(0x93u), errMsg);
        if (uDlg < 0)
        {
            pCtx[8] = 0;
            LeaveCriticalSection((LPCRITICAL_SECTION)(pCtx + 2));
            return 0x6b;                        /* Abort */
        }
        if (uDlg > 0)
            break;
    }
    pCtx[8] = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)(pCtx + 2));
    return 0;   /* Ignore */
}


/* Opens an existing container file into *pHandle (position 0, its lock initialised): read/write
 * first, read-only when that fails (the CD). Returns 0, or 0x6b when neither open works. */
short __cdecl OpenContainerFile(byte *pDescriptor, ContainerFileHandle *pHandle)  /* 0x004239b0 */
{
    HANDLE hFile;
    byte   szPath[256];

    GetResourcePath(pDescriptor, szPath);

    hFile = CreateFileA((LPCSTR)szPath,
                        GENERIC_READ | GENERIC_WRITE,              /* 0xC0000000 */
                        FILE_SHARE_READ,                           /* 1 */
                        (LPSECURITY_ATTRIBUTES)0,
                        OPEN_EXISTING,                             /* 3 */
                        FILE_FLAG_SEQUENTIAL_SCAN |
                            FILE_ATTRIBUTE_NORMAL,                 /* 0x8000080 */
                        (HANDLE)0);

    if (hFile == INVALID_HANDLE_VALUE) {
        /* DF_FIX_CONTAINER_SHARE_MODES (original: FILE_SHARE_READ only): FILE_SHARE_WRITE as in
         * ProbeContainerFile; this handle cannot write anyway, and without it a second instance
         * of the game is refused because the first holds a read/write handle. */
        hFile = CreateFileA((LPCSTR)szPath,
                            GENERIC_READ,                          /* 0x80000000 */
#if DF_FIX_CONTAINER_SHARE_MODES
                            FILE_SHARE_READ | FILE_SHARE_WRITE,    /* 1 | 2 */
#else
                            FILE_SHARE_READ,                       /* original: 1 */
#endif
                            (LPSECURITY_ATTRIBUTES)0,
                            OPEN_EXISTING,                         /* 3 */
                            FILE_FLAG_SEQUENTIAL_SCAN |
                                FILE_ATTRIBUTE_READONLY,           /* 0x8000001 */
                            (HANDLE)0);
        if (hFile == INVALID_HANDLE_VALUE) {
            return 0x6b;
        }
    }

    pHandle->hFile   = hFile;
    pHandle->filePos = 0;
    pHandle->state   = 0;
    InitializeCriticalSection(&pHandle->critsec);
    return 0;
}


/* Closes the file and deletes the handle's lock. Returns 0, or 0x6b if CloseHandle fails. */
short __cdecl CloseContainerFile(void *pHandle)  /* 0x00423a40 */
{
    ContainerFileHandle *ph = (ContainerFileHandle *)pHandle;
    BOOL ok = CloseHandle(ph->hFile);
    DeleteCriticalSection(&ph->critsec);
    return ok ? 0 : 0x6b;
}


/* OpenContainerFile for a new file: CREATE_ALWAYS, read/write, not shared. The two tags are
 * passed on by BootstrapContainerFile and not used. Returns 0 or 0x6b. */
short __cdecl OpenContainerFileCreateAlways(byte *pDescriptor,
                                                   unsigned int unusedParam2,
                                                   unsigned int unusedParam3,
                                                   ContainerFileHandle *pHandle) /* 0x00423a70 */
{
    HANDLE hFile;
    byte   szPath[256];

    (void)unusedParam2;
    (void)unusedParam3;

    GetResourcePath(pDescriptor, szPath);
    hFile = CreateFileA((LPCSTR)szPath,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        (LPSECURITY_ATTRIBUTES)NULL,
                        CREATE_ALWAYS,
                        FILE_FLAG_SEQUENTIAL_SCAN | FILE_ATTRIBUTE_NORMAL,
                        (HANDLE)NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return 0x6b;
    }

    pHandle->hFile   = hFile;
    pHandle->filePos = 0;
    pHandle->state   = 0;
    InitializeCriticalSection(&pHandle->critsec);
    return 0;
}


/* FlushFileBuffers on the node's file, under its lock. Returns 0 or 0x6b. */
short __cdecl FlushNodeFileBuffers(int *pNode) /* 0x00423AE0 */
{
    BOOL  ok;

    EnterCriticalSection((LPCRITICAL_SECTION)(pNode + 2));
    pNode[8] = 1;
    ok = FlushFileBuffers((HANDLE)(UINT_PTR)(unsigned)pNode[0]);
    pNode[8] = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)(pNode + 2));
    return ok ? 0 : 0x6b;               /* cmp/sbb/and cx,0x6b: the compiler idiom for this ternary */
}


/* Records the position the next read or write starts at; ReadFileBlock and WriteToFileWithRetry
 * issue the SetFilePointer. The handle's state word is the busy flag every I/O sets under the
 * lock. Always returns 0. */
short __cdecl SeekContainerFile(void *pHandle, unsigned int fileOffset)  /* 0x00423b30 */
{
    ContainerFileHandle *ph = (ContainerFileHandle *)pHandle;
    EnterCriticalSection(&ph->critsec);
    ph->state   = 1;
    ph->state   = 0;
    ph->filePos = fileOffset;
    LeaveCriticalSection(&ph->critsec);
    return 0;
}


/* Returns 0 and zeroes both outputs when the file pDescriptor names exists, else 0x6b. */
short __cdecl CheckContainerFileExists(byte *pDescriptor,  /* 0x00423b70 */
                                  int  *pContainerOut,
                                  int  *pLocalOut)
{
    DWORD dwAttribs;
    byte  szPath[256];

    GetResourcePath(pDescriptor, szPath);
    dwAttribs = GetFileAttributesA((LPCSTR)szPath);
    if (dwAttribs == INVALID_FILE_ATTRIBUTES) {
        return 0x6b;
    }
    *pLocalOut     = 0;
    *pContainerOut = 0;
    return 0;
}


/* Deletes the file the Pascal descriptor pName names. Returns 0 or 0x6b. */
short __cdecl TryDeleteResourceFile(byte *pName)  /* 0x00423BD0 */
{
  BOOL  BVar1;
  byte  path [256];  /* local_100 */

  GetResourcePath(pName, path);
  BVar1 = DeleteFileA((LPCSTR)path);
  return BVar1 ? 0 : 0x6b;            /* cmp/sbb/and, as FlushNodeFileBuffers */
}


/* The file's size in *pSizeOut. Returns 0, or 0x6b for an empty file (*pSizeOut untouched). */
short __cdecl GetActualFileSize(ContainerFileHandle *pHandle, DWORD *pSizeOut)  /* 0x00423c10 */
{
    DWORD size;

    EnterCriticalSection(&pHandle->critsec);
    pHandle->state = 1;
    size           = GetFileSize(pHandle->hFile, NULL);
    pHandle->state = 0;
    LeaveCriticalSection(&pHandle->critsec);

    if (size == 0)
        return 0x6b;
    *pSizeOut = size;
    return 0;
}


/* Reports on the file pDescriptor names: *pIsReadOnly its read-only attribute, *pOpenError 1 if
 * it cannot be opened (0 if it can), and the return value is CheckContainerFileExists's status.
 * The open error and the status are separate results. */
short __cdecl ProbeContainerFile(byte            *pDescriptor,
                                short           *pOpenError,
                                int             *pContainerOut,
                                unsigned short  *pIsReadOnly) /* 0x00423c60 */
{
    DWORD  dwAttribs;
    HANDLE hFile;
    int    localOut;
    byte   szPath[256];

    GetResourcePath(pDescriptor, szPath);

    dwAttribs    = GetFileAttributesA((LPCSTR)szPath);
    *pIsReadOnly = (dwAttribs & FILE_ATTRIBUTE_READONLY) != 0;   /* `and eax,1` then a word store */

    /* DF_FIX_CONTAINER_SHARE_MODES (original: exclusive, 0): this probe handle is closed at once,
     * and an exclusive open fails against the bootfile's handle still open on a script reboot.
     * Port note: FILE_SHARE_WRITE too, because OpenContainerFile's handles are read/write; the
     * full story and why the switch is suspect: port-bug-classes.md section 8. */
    hFile = CreateFileA((LPCSTR)szPath,
                        GENERIC_READ,               /* 0x80000000 */
#if DF_FIX_CONTAINER_SHARE_MODES
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
#else
                        0,                          /* original: exclusive */
#endif
                        (LPSECURITY_ATTRIBUTES)0,
                        OPEN_EXISTING,              /* 3 */
                        FILE_ATTRIBUTE_NORMAL,      /* 0x80 */
                        (HANDLE)0);

    if (hFile == INVALID_HANDLE_VALUE) {
        *pOpenError = 1;
    } else {
        *pOpenError = 0;
        CloseHandle(hFile);
    }

    return CheckContainerFileExists(pDescriptor, pContainerOut, &localOut);
}


/* Free bytes on the drive holding the folder pDescriptor names, capped at 0x7fffffff, in
 * *pFreeBytes; resolves the folder's test file to find the drive root. Returns 0 or 0x6b. Its
 * one caller is DispatchPluginEngineRequest, request 0x226.
 *
 * Port note: Ghidra merged the walking pointer's byte with path[0], and the port overwrote the
 * drive letter as it walked; the exe only reads. */
short __cdecl GetResourceDiskFreeSpace(byte *pDescriptor, uint32_t *pFreeBytes)  /* 0x00423CF0 */
{
    DWORD  sectorsPerCluster;
    DWORD  bytesPerSector;
    DWORD  freeClusters;
    DWORD  totalClusters;
    char   path[256];
    byte   name[256];
    char  *p;
    uint32_t cb;

    CopyPascalString(pDescriptor, name);
    AppendPascalString((byte *)UpdatePascalLength(&kStrTestfile), name);
    GetResourcePath(name, (byte *)path);
    /* a resolved path of exactly 8 characters reports 0 bytes free (exe: cmp ecx,8) */
    if (strlen(path) == 8) {
        *pFreeBytes = 0;
        return 0;
    }
    /* cut after the first backslash: the root GetDiskFreeSpaceA takes */
    for (p = path; *p != 0 && *p != 0x5c; p++)
        ;
    p[1] = 0;
    if (GetDiskFreeSpaceA(path, &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters) == 0)
        return 0x6b;
    cb = freeClusters * bytesPerSector * sectorsPerCluster;
    if (cb > 0x7fffffff)
        cb = 0x7fffffff;
    *pFreeBytes = cb;
    return 0;
}


/* Starts listing the folder pDesc names (kStrAllFiles appended): FindFirstFileA into gFindData,
 * the search handle in gDirSearchHandle. One search at a time; ReadNextDirectoryEntry walks it.
 * Returns 0, or 0x6b when nothing matches. */
short __cdecl OpenDirectorySearch(byte *pDesc)  /* 0x00423df0 */
{
    byte *unusedPtr;
    byte *pQuery;
    byte  pattern[256];  /* local_200 */
    byte  path[256];  /* local_100 */

    CopyPascalString(pDesc, pattern);
    pQuery = pattern;
    AppendPascalString((byte *)UpdatePascalLength((char *)&kStrAllFiles), pQuery);   /* the destination is pushed before the lookup call */
    GetResourcePath(pattern, path);

    gDirSearchHandle = (unsigned int)FindFirstFileA((LPCSTR)path,
                                                &gFindData);
    if ((HANDLE)gDirSearchHandle == INVALID_HANDLE_VALUE)
    {
        gDirSearchHandle = 0;
        return 0x6b;
    }
    return 0;
}


/* Copies the current entry's file name to pOut as a Pascal string and advances the search,
 * closing it after the last entry. Returns 0, or 0x6b when no search is open. */
short __cdecl ReadNextDirectoryEntry(byte *pOut)  /* 0x00423e80 */
{
    if (gDirSearchHandle != 0)
    {
        /* cFileName (+0x2c of the find data) as a Pascal string: an inlined
         * strlen for the length byte, then an inlined strcpy of the body */
        *pOut = (byte)strlen((char *)&gFindData.cFileName[0]);
        strcpy((char *)(pOut + 1), (char *)&gFindData.cFileName[0]);

        if (FindNextFileA((HANDLE)gDirSearchHandle, &gFindData) == 0)
        {
            FindClose((HANDLE)gDirSearchHandle);
            gDirSearchHandle = 0;
        }
        return 0;
    }
    return 0x6b;
}



/* Sets *pFlagOut to 0 when the folder pDesc names, with kStrColonTestfile appended, resolves to
 * a DOS path of exactly 8 characters (GetResourceDiskFreeSpace's test too), else 1. Returns 0. */
short __cdecl CheckDescriptorVolumeResolvable(byte *pDesc, short *pFlagOut)  /* 0x00423F00 */
{
  byte colonPath[256];
  char dosPath[256];

  CopyPascalString(pDesc, colonPath);
  AppendPascalString((byte *)UpdatePascalLength((char *)&kStrColonTestfile), colonPath);
  ResolveColonPathToDosPath(colonPath, (byte *)dosPath);
  if (strlen(dosPath) != 8)
    *pFlagOut = 1;
  else
    *pFlagOut = 0;
  return 0;
}


/* Nonzero while the display-attach loader thread runs; always 0 in TI.EXE, where nothing starts it.
 * Every caller pushes a context; the body (`mov ax,[0x45d694]; ret`) never reads it. */
ushort __cdecl IsDisplayAttachLoaderRunning(int pCtx)                      /* 0x00423f90 */
{
    return gAttachLoaderRunning;
}


/* The display-attach loader thread: LINK-DISCARDED original source, recovered from the March 1997
 * build (Disc 1 INSTALL\BIN\TI.EXE), where it sits between IsDisplayAttachLoaderRunning and
 * SetupDisplayAttachSlotRecord just as here. In TI.EXE (Dec 1996) InitDisplayAttachSlots and
 * ShutdownDisplayAttachSlots have empty bodies, so nothing starts or stops the thread and
 * /OPT:REF dropped all four functions; none has a TI.EXE address. The source still shaped TI.EXE:
 * the compiler counts its symbols (one match/symbol_phase.json row follows it), its imports (Sleep,
 * WaitForSingleObject, CreateEventA, ResetEvent) and _beginthread's LIBCMT members take their
 * places in the link, its DIR32 relocations size .reloc, and its two globals hold two bss slots of
 * this unit's block.
 *
 * The callees' declarations stand here, not in the header: match/build.py's
 * LOCAL_PROTOS keeps df_protos.h's copies out of this unit's header, where every earlier function
 * would count them too. The same for _beginthread, the one <process.h> declaration needed.
 *
 * The request record (AttachRequest below) is the 0x34-byte tail of an attach slot (slot + 10
 * shorts), filled by SetupDisplayAttachSlotRecord. */
void __stdcall RestorePrimaryHeap(void);
short __cdecl ReserveHeapSpace(int size, short *pSucceeded);
void __cdecl MoveResObjectToHeapTop(int *pHandle);

#ifdef _MSC_VER   /* section-defined, not COMMON: laid out in this unit's bss block */
#pragma bss_seg(".bss")
#endif
unsigned long g_hAttachLoaderThread;   /* TI.EXE bss 0x489d98: _beginthread's handle */
int           g_attachLoaderStop;      /* TI.EXE bss 0x489d9c: stop request, cleared by the thread to acknowledge */
#ifdef _MSC_VER
#pragma bss_seg()
#endif
unsigned long __cdecl _beginthread(void (__cdecl *)(void *), unsigned, void *);

/* The container handle's lock as the thread sees it: the record keeps &handle->critsec,
 * and the busy word sits right after the CRITICAL_SECTION (ContainerFileHandle.state). */
typedef struct {
    CRITICAL_SECTION cs;        /* +0x00 */
    int              busy;      /* +0x18 */
} LockedCritSec;

/* The request record (0x34 bytes, the tail of an attach slot) */
typedef struct {
    HANDLE         hFile;       /* +0x00 */
    unsigned int   chunkId;     /* +0x04 the id the chunk header must carry */
    LONG           offset;      /* +0x08 file offset, advanced while reading */
    unsigned int   hdrId;       /* +0x0c chunk header: id */
    unsigned int   hdrSize;     /* +0x10 chunk header: body size */
    int            unused14;    /* +0x14 */
    int            done;        /* +0x18 */
    int            cancel;      /* +0x1c set by UnregisterDisplayAttachWaitRecord */
    int            taken;       /* +0x20 */
    int            status;      /* +0x24 */
    int           *hRes;        /* +0x28 */
    char          *pWrite;      /* +0x2c */
    LockedCritSec *pLock;       /* +0x30 */
} AttachRequest;

void __cdecl DisplayAttachLoaderThreadProc(void *unused);
int __cdecl ReadFileAtWithRetry(HANDLE hFile, LockedCritSec *pLock, void *pDest, DWORD size, LONG offset);
/* Starts the loader thread. A no-op when it is already running or under Win32s (gIsWin31).
 * Otherwise empties the queue, creates the lock and a manual-reset event, starts the thread with a
 * 16 KB stack (FatalError(0, 0x431) if _beginthread fails) and sets the running flag that
 * SetupDisplayAttachSlotRecord tests to queue instead of loading inline.
 * LINK-DISCARDED: no TI.EXE address (March build 0x424fc0, called by InitDisplayAttachSlots). */
short __cdecl StartDisplayAttachLoaderThread(void)
{
    if (gAttachLoaderRunning != 0)
        return 0;
    if (gIsWin31 != 0)
        return 0;
    gAttachWaitCount = 0;
    g_attachLoaderStop = 0;
    InitializeCriticalSection(&gAttachLock);
    gAttachEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    g_hAttachLoaderThread = _beginthread(DisplayAttachLoaderThreadProc, 0x4000, NULL);
    if (g_hAttachLoaderThread == (unsigned long)-1)
        FatalError(0, 0x431);
    gAttachLoaderRunning = 1;
    return 0;
}

/* The worker. Sleeps on the event while the queue is empty, exits (clearing the stop flag as the
 * acknowledgement) when asked, else pops the head request under the lock and loads it: reads the
 * 8-byte block header at `offset`, checks its id against `chunkId` (0x6a if not), reserves heap
 * (size*2+20000, then size+20000), allocates and pins a resource of the block's size and reads the
 * payload in 32 KB pieces. Status: 0 loaded, 0x66 no memory, 0x6a wrong block, 0x6b cancelled or
 * read aborted, else ReserveHeapSpace's error. Cancellation is checked before every step. On
 * success `taken` is left set; every failure clears it.
 * LINK-DISCARDED: no TI.EXE address (March build 0x425050). */
void __cdecl DisplayAttachLoaderThreadProc(void *unused)
{
    AttachRequest *req;
    short          st;
    unsigned int   left;
    unsigned int   n;
    short          ok;

    RestorePrimaryHeap();
    for (;;) {
        if (gAttachWaitCount == 0) {
            ResetEvent(gAttachEvent);
            WaitForSingleObject(gAttachEvent, INFINITE);
        }
        if (g_attachLoaderStop != 0) {
            g_attachLoaderStop = 0;
            return;
        }
        if (gAttachWaitCount == 0)
            continue;

        EnterCriticalSection(&gAttachLock);
        req = (AttachRequest *)gAttachWaitList[0];
        if (--gAttachWaitCount != 0)
            memmove(&gAttachWaitList[0], &gAttachWaitList[1], gAttachWaitCount * 4);
        req->taken = 1;
        LeaveCriticalSection(&gAttachLock);

        if (req->cancel != 0)
            goto cancelled;
        if (!ReadFileAtWithRetry(req->hFile, req->pLock, &req->hdrId, 8, req->offset))
            goto cancelled;
        if (req->chunkId != req->hdrId) {
            req->done = 1;
            req->status = 0x6a;
            req->taken = 0;
            continue;
        }
        if (req->cancel != 0)
            goto cancelled;
        st = ReserveHeapSpace(req->hdrSize * 2 + 20000, &ok);
        if (st == 0 && ok == 0)
            st = ReserveHeapSpace(req->hdrSize + 20000, &ok);
        if (st != 0) {
            req->done = 1;
            req->status = st;
            req->taken = 0;
            continue;
        }
        if (ok == 0 || (req->hRes = (int *)AllocResObject(req->hdrSize)) == NULL) {
            req->done = 1;
            req->status = 0x66;
            req->taken = 0;
            continue;
        }
        MoveResObjectToHeapTop(req->hRes);
        req->pWrite = (char *)GetResDataPtr(req->hRes);
        if (req->cancel != 0)
            goto free_cancelled;
        req->offset += 8;
        for (left = req->hdrSize; left != 0; left -= n) {
            n = left;
            if (n > 0x8000)
                n = 0x8000;
            if (req->cancel != 0)
                goto free_cancelled;
            if (!ReadFileAtWithRetry(req->hFile, req->pLock, req->pWrite, n, req->offset))
                goto free_cancelled;
            req->offset += n;
            req->pWrite += n;
        }
        UnlockResObject(req->hRes);
        req->done = 1;
        req->status = 0;
        continue;

free_cancelled:
        UnlockResObject(req->hRes);
        FreeResObject(req->hRes);
cancelled:
        req->done = 1;
        req->status = 0x6b;
        req->taken = 0;
    }
}

/* ReadFileBlock's seek-and-read under the container's lock, for an explicit offset and one
 * piece, with the same disk error box (strings 0x90 / 0x91): Abort returns 0, Ignore 1, Retry
 * again. Returns 1 on success. Unlike ReadFileBlock it tests ReadFile's BOOL and
 * PromptDiskRetry's answer as int.
 * LINK-DISCARDED: no TI.EXE address (March build 0x425270). */
int __cdecl ReadFileAtWithRetry(HANDLE hFile, LockedCritSec *pLock, void *pDest,
                                DWORD size, LONG offset)
{
    DWORD got;
    char  msg[384];
    int   answer;

    EnterCriticalSection(&pLock->cs);
    pLock->busy = 1;
    do {
        got = SetFilePointer(hFile, offset, NULL, FILE_BEGIN);
        if (got != (DWORD)-1 && ReadFile(hFile, pDest, size, &got, NULL) && got == size) {
            pLock->busy = 0;
            LeaveCriticalSection(&pLock->cs);
            return 1;
        }
        sprintf(msg, GetStringResource(0x90), GetLastError());
        answer = PromptDiskRetry(GetStringResource(0x91), msg);
        if (answer < 0) {
            pLock->busy = 0;
            LeaveCriticalSection(&pLock->cs);
            return 0;
        }
    } while (answer <= 0);
    pLock->busy = 0;
    LeaveCriticalSection(&pLock->cs);
    return 1;
}

/* Raises the stop flag, wakes the thread and spins (Sleep(0)) until the thread clears the flag,
 * then frees the lock and the event and clears the running flag.
 * LINK-DISCARDED: no TI.EXE address (March build 0x425380, called by ShutdownDisplayAttachSlots). */
short __cdecl StopDisplayAttachLoaderThread(void)
{
    if (gAttachLoaderRunning == 0)
        return 0;
    g_attachLoaderStop = 1;
    SetEvent(gAttachEvent);
    while (g_attachLoaderStop != 0)
        Sleep(0);
    DeleteCriticalSection(&gAttachLock);
    CloseHandle(gAttachEvent);
    gAttachLoaderRunning = 0;
    return 0;
}


/* Fills an attach slot's request record (AttachRequest above, at pSlot) to load child childId of
 * pNode: the file handle, the child id, its block offset, status 1 (pending), the node's lock.
 * Without the loader thread (always, in TI.EXE) the child is loaded at once with OpenChildNode
 * and the record marked done with its status; otherwise the record is queued for the thread.
 * Returns 0, or GetChildBlockOffset's status. */
short __cdecl SetupDisplayAttachSlotRecord(int *pNode, unsigned int childId,
                                                  int *pSlot) /* 0x00423fa0 */
{
    short          err;
    int            slotIdx;
    unsigned int   openRc;

    err = GetChildBlockOffset(pNode, childId, pSlot + 2);
    if (err != 0)
        return err;

    pSlot[0]  = pNode[0];          /* hFile */
    pSlot[1]  = (int)childId;      /* chunkId */
    pSlot[7]  = 0;                 /* cancel */
    pSlot[8]  = 0;                 /* taken */
    pSlot[10] = 0;                 /* hRes */
    pSlot[9]  = 1;                 /* status */
    pSlot[12] = (int)(pNode + 2);  /* pLock: the handle's critsec */

    if (gAttachLoaderRunning == 0) {
        openRc = (unsigned short)OpenChildNode(pNode, childId, (int **)(pSlot + 10));
        pSlot[6] = 1;              /* done */
        pSlot[9] = (int)(short)openRc;
        return 0;
    }

    pSlot[6] = 0;
    EnterCriticalSection(&gAttachLock);
    slotIdx = gAttachWaitCount;
    gAttachWaitCount++;
    gAttachWaitList[slotIdx] = pSlot;
    LeaveCriticalSection(&gAttachLock);
    SetEvent(gAttachEvent);
    return 0;
}


/* Cancels a request record. Without the loader thread it frees the loaded resource and sets status
 * 0x6b; otherwise it takes the record off the wait list, sets `cancel`, and marks it done unless the
 * thread has already taken it (the thread then finishes it as cancelled). Returns 0. */
short __cdecl UnregisterDisplayAttachWaitRecord(int *payloadPtr) /* 0x00424060 */
{
    void **pp;
    int    i;

    if (gAttachLoaderRunning == 0) {
        FreeResObject((int *)payloadPtr[0x28 / 4]);
        payloadPtr[0x24 / 4] = 0x6b;
        return 0;
    }
    EnterCriticalSection(&gAttachLock);
    i = 0;
    if (gAttachWaitCount > 0) {
        pp = gAttachWaitList;
        do {
            if (*pp == (void *)payloadPtr) {
                gAttachWaitCount--;
                /* From the table BASE, as the exe does (`push 0x489ee4 /
                 * push 0x489ee0`, 0x004240c7): entry 0 is dropped whatever
                 * matched, with entry i's tail length. CyberFlix's bug, kept. */
                memmove(gAttachWaitList, gAttachWaitList + 1, (gAttachWaitCount - i) * sizeof(void *));
                break;
            }
            pp++;
            i++;
        } while (i < gAttachWaitCount);
    }
    LeaveCriticalSection(&gAttachLock);
    payloadPtr[0x1c / 4] = 1;
    if (payloadPtr[0x20 / 4] == 0)
        payloadPtr[0x18 / 4] = 1;
    return 0;
}


/* Reports a request record's progress: *pDoneOut 1 once it is done, *pAuxOut its status and
 * *ppResourceOut the loaded resource (any output may be NULL). Not done yet: all three zero. A
 * cancelled record that finished loading anyway has its resource freed and reports 0x6b. Without
 * the loader thread the record is always done. Returns 0 (Ghidra's `& 0xffff0000`). */
short __cdecl PollDisplayAttachSlotCompletion(void *payloadRaw,
                                                    short *pDoneOut, short *pAuxOut,
                                                    int **ppResourceOut) /* 0x00424100 */
{
    unsigned char *p = (unsigned char *)payloadRaw;

    if (gAttachLoaderRunning == 0) {
        if (pAuxOut != NULL)
            *pAuxOut = (short)*(unsigned int *)(p + 0x24);
        if (pDoneOut != NULL)
            *pDoneOut = 1;
        if (ppResourceOut != NULL)
            *ppResourceOut = *(int **)(p + 0x28);
        return 0;
    }

    if (*(int *)(p + 0x18) == 0) {
        if (pAuxOut != NULL)
            *pAuxOut = 0;
        if (pDoneOut != NULL)
            *pDoneOut = 0;
        if (ppResourceOut != NULL)
            *ppResourceOut = NULL;
        return 0;
    }

    if (*(int *)(p + 0x1c) != 0 && *(int *)(p + 0x24) == 0) {
        FreeResObject(*(int **)(p + 0x28));
        if (pAuxOut != NULL)
            *pAuxOut = 0x6b;
        if (pDoneOut != NULL)
            *pDoneOut = 1;
        if (ppResourceOut != NULL)
            *ppResourceOut = NULL;
        return 0;
    }

    if (pAuxOut != NULL)
        *pAuxOut = (short)*(unsigned int *)(p + 0x24);
    if (pDoneOut != NULL)
        *pDoneOut = 1;
    if (ppResourceOut != NULL)
        *ppResourceOut = *(int **)(p + 0x28);
    return 0;
}
