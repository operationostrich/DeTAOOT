/* sprite_hotspot_raster.c -- TI.EXE 0x00411e00..0x00413780: sprite frame geometry and the
 * scene / "trig" hotspot rasteriser (RLE stamping, the scaled walks, the scan-node steppers). */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stddef.h>
#include <mmsystem.h>
#include "df_protos.h"
#include "topics/scriptcmd.h"
#include "topics/scene.h"
#include "topics/scriptquery.h"
#include "topics/hotspot_raster.h"
#include "topics/renderer.h"
#include "topics/sound.h"

static void PrepareSlotEdgeCache(int edgeMode, int planeIdx);
static void GatherTrigBandHitSpans(int planeIdx);
static void ComputeTrigBandCrossings(int pPerPlane, int firstPlane, int endPlane);
static uint32_t FindNearestEdgeCrossing(int planeIdx, int colLo, int colHi, int leftEdge, int *pSpanIdxOut, int *pMappedOut, int maxDelta);
static int EnrollScanNode(int x0, int x1, int steps, int life);
static void DecrementAndTrimScanNodes(void);
static void AdvanceScanNodeSteps(void);


/* Points the eight plane-span buffers (gTrigPlaneSpanBufs, each at a 0x2000-byte region of
 * gTrigPlaneSpanStore, with an overlapping ninth entry), the RLE row scratch, the column maps, the
 * band list and the scan-node store at their static stores, and allocates the zone-map res-object.
 * Counterpart of ShutdownRenderState. */
void __stdcall InitRenderState(void)  /* 0x00411e00 */
{
    int  *pTable;
    char *pBuf;

    gTrigPlaneSpanBufs[8] = (int)&gTrigPlaneSpanStore8;    /* _gTrigPlaneSpanBuf8 (overlapping symbol) */
    gTrigPixelRowsPtr    = (int)(intptr_t)gTrigPixelRows;   /* byte[8192] RLE row scratch */
    gTrigSrcToDstMap    = (int)&gTrigSrcToDstStore;
    gTrigDstToSrcMap    = (int)&gTrigDstToSrcStore;
    gTrigBandList    = (int *)&gTrigBandListStore;   /* the band list's base (0x00411e00) */

    pBuf = &gTrigPlaneSpanStore;
    for (pTable = gTrigPlaneSpanBufs; pTable < &gTrigPlaneSpanBufs[8]; pTable++)
    {
        *pTable = (int)pBuf;
        pBuf += 0x2000;
    }

    gTrigHitSpans = (int)&gTrigHitSpanStore;
    gScanNodeBase = (int)&gScanNodeStore;
    gTrigAnimBuf = AllocResObject(0);
    gTrigAnimBufSize = 0;
}


/* Uninstalls the res-object that InitRenderState allocated. Called by EndGameSession after the
 * channel teardown messages. */
void __stdcall ShutdownRenderState(void)  /* 0x00411e80 */
{
    FreeResObject(gTrigAnimBuf);
}


/* Computes the screen rect of a sprite frame (hSprite) from its size and hot-spot: unpacks the
 * frame data and the packed position (a {short x, short y} by value) into pRectOut; flipV mirrors
 * the hot-spot vertically. */
void __cdecl ComputeSpriteFrameGeometry(int *hSprite, uint32_t packedPos, short flipV, short *pRectOut)  /* 0x00411E90 */
{
  short w;
  short h;
  short *pHdr;
  POINTS off;       /* the frame's {x,y} offset, copied as one struct */

  pHdr = (short *)GetResDataPtr(hSprite);
  w = pHdr[0];
  h = pHdr[1];
  off = *(POINTS *)(pHdr + 2);
  UnlockResObject(hSprite);
  if (flipV != 0)
    off.y = h - off.y;   /* flipped in the local copy: no separate dy */
  /* packedPos is a {short x, short y} point passed by value */
  pRectOut[0] = ((short *)&packedPos)[0] - off.x;
  pRectOut[1] = ((short *)&packedPos)[1] - off.y;
  pRectOut[2] = pRectOut[0] + w;
  pRectOut[3] = pRectOut[1] + h;
}


/* Stamps a sprite frame unscaled into the hotspot buffer pDstDesc at packedTopLeft, clipped to the
 * clip rect, decoding the frame's run-length rows. Rows above the clip top are decoded into the
 * scratch row only, so the "copy from previous row" runs have something to copy from. flipV
 * mirrors the frame. */
void RasterStampChildIntoSceneHotspotBuf(int *hSprite, uint32_t packedTopLeft, int pDstDesc,
                                         short flipV) /* 0x00411f10 */
{
    short    nRows;  /* local_10 */
    short    width;
    short    stride;
    struct { POINTS hot, end; } r;
    short    gap;
    short    clip;
    short    n;
    short    n2;
    short    len;
    short    left;
    short    span;
    short   *pHdr;
    short   *pStrip;
    uint8_t *pbScratch;
    uint8_t *pbDst;

    /* The descriptor reads before the header, `end` as one POINTS, a separate `clip`
     * and the pointers declared last: each measured against the exe. */
    pHdr   = (short *)GetResDataPtr(hSprite);
    pbDst    = (uint8_t *)*(int *)(pDstDesc + 4);
    stride    = *(short *)(pDstDesc + 2);
    nRows = pHdr[0];
    width    = pHdr[1];
    if (width > 0x400)
        FatalError(0, 0x463);
    r.hot     = *(POINTS *)(pHdr + 2);
    pStrip = pHdr + 4;
#if DF_TRACE_ENABLED
    {
        static int seen[64]; static int nseen = 0; int k, dup = 0;
        int id = (int)(intptr_t)hSprite;
        for (k = 0; k < nseen; k++) if (seen[k] == id) dup = 1;
        if (!dup && nseen < 64) {
            const byte *st = (const byte *)pStrip;
            seen[nseen++] = id;
            DF_TRACE("stampHdr: h=%p rows=%d width=%d hot=(y%d,x%d) pos=(y%d,x%d) strip0 len=%d ctl: %02x %02x %02x %02x %02x %02x %02x %02x",
                     (void *)hSprite, (int)nRows, (int)width, (int)r.hot.x, (int)r.hot.y,
                     (int)(short)(packedTopLeft & 0xffff), (int)(short)(packedTopLeft >> 16),
                     (int)*(const short *)st, st[2], st[3], st[4], st[5], st[6], st[7], st[8], st[9]);
            /* Decode the middle scanline's run list. Run type is the low two
             * bits of each control byte: 00=copy from previous row, 10=RLE fill,
             * 01=transparent skip, 11=literal copy; count is ctl>>2. This shows
             * directly whether a sprite is solid across its middle or has a
             * transparent hole there. */
            {
                const uint8_t *sp = (const uint8_t *)pStrip;
                int targetRow = (int)nRows / 2;
                char buf[320];
                int pos = 0, total = 0, r;
                for (r = 0; r < targetRow; r++)
                    sp += 2 + (int)*(const short *)sp;
                {
                    int L = (int)*(const short *)sp;
                    const uint8_t *q = sp + 2;
                    const uint8_t *end = q + L;
                    buf[0] = 0;
                    while (q < end && pos < 280) {
                        uint8_t c = *q++;
                        int n = c >> 2;
                        const char *t;
                        if ((c & 1) == 0) {
                            if ((c & 2) == 0) { t = "prev"; }
                            else { t = "fill"; q++; }
                        } else {
                            if ((c & 2) == 0) { t = "SKIP"; }
                            else { t = "copy"; q += n; }
                        }
                        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                                        "%s%d ", t, n);
                        total += n;
                    }
                    DF_TRACE("  midRow %d/%d: %s(total=%d width=%d)",
                             targetRow, (int)nRows, buf, total, (int)width);
                }
            }
        }
    }
#endif
    if (flipV != 0)
        r.hot.y = width - r.hot.y;
    r.hot.x = ((short *)&packedTopLeft)[0] - r.hot.x;
    r.hot.y = ((short *)&packedTopLeft)[1] - r.hot.y;
    r.end.x = nRows + r.hot.x;
    r.end.y = width + r.hot.y;
    if (!(r.hot.x < gClipRect[2] && r.hot.y < gClipRect[3] &&
          gClipRect[0] < r.end.x && gClipRect[1] < r.end.y)) {
        UnlockResObject(hSprite);
        return;
    }
    gap = gClipRect[0] - r.hot.x;
    if (gap > 0) {
        /* Rows above the clip top are decoded into the scratch row only (dst == prev ==
         * scratch) so the "copy from previous row" runs have something to copy from. */
        n = gap;
        do {
            len = *pStrip;
            pStrip++;
            if (flipV != 0)
                BackwardHotspotRleBlitRow((uint8_t *)pStrip, 0, width, width,
                                          (uint8_t *)gTrigPixelRowsPtr, (uint8_t *)gTrigPixelRowsPtr);
            else
                ForwardHotspotRleBlitRow((uint8_t *)pStrip, 0, width, width,
                                         (uint8_t *)gTrigPixelRowsPtr, (uint8_t *)gTrigPixelRowsPtr);
            pStrip = (short *)((uint8_t *)pStrip + len);
        } while (--n != 0);
        nRows -= gap;
        pbDst += gClipRect[0] * stride;
    } else
        pbDst += r.hot.x * stride;
    clip = r.end.x - gClipRect[2];
    if (clip > 0)
        nRows -= clip;
    left = gClipRect[1] - r.hot.y;
    if (left <= 0)
        left = 0;
    pbDst += r.hot.y;
    span = width;
    gap = r.end.y - gClipRect[3];
    if (gap > 0)
        span -= gap;
    pbScratch = (uint8_t *)gTrigPixelRowsPtr;
    if (nRows > 0) {
        n2 = nRows;
        do {
            len = *pStrip;
            pStrip++;
            if (flipV != 0) {
                BackwardHotspotRleBlitRow((uint8_t *)pStrip, left, span, width, pbDst, pbScratch);
                pbScratch = pbDst;
                pbDst += stride;
                pStrip = (short *)((uint8_t *)pStrip + len);
            } else {
                ForwardHotspotRleBlitRow((uint8_t *)pStrip, left, span, width, pbDst, pbScratch);
                pbScratch = pbDst;
                pbDst += stride;
                pStrip = (short *)((uint8_t *)pStrip + len);
            }
        } while (--n2 != 0);
    }
    UnlockResObject(hSprite);
}

#undef local_a4
#undef local_92
#undef local_90
#undef local_8e
#undef local_8c
#undef local_8a
#undef local_88
#undef local_7a

/* Hit test of a point against an unscaled frame's RLE mask, after CalcActorRenderParams: locks the
 * child hotspot resource, maps the calc-space coordinates into a rect, biases the embedded hot-spot
 * dword when flipV is set, tests PointInSceneRect, then walks the horizontal strips
 * (ProbePackedHotspotStrip). packedPos and pt are dword-packed pairs of shorts (low word first). */
short ProbeSpriteHotspotStripHit(int *hSprite, int packedPos, short flipV, ScenePointYX pt) /* 0x00412140 */
{
    /* ONE rect {y1,x1,y2,x2}: 0x00412140 passes &local_c to PointInSceneRect and reads
     * local_c..local_6 as its four shorts. As four separate locals GCC scattered them: the HELP
     * button's probe said "outside" for a point inside its own rect (class 3). The hot-spot
     * dword is a local too (frame +8); the strip pointer walks. */
    short  rect[4];
    POINTS hot;
    short *hdr;
    byte  *p;
    short  w, n, hx;
    short  row, col, i, len, r;

    hdr = (short *)GetResDataPtr(hSprite);
    n = hdr[0];
    w = hdr[1];
    hot = *(POINTS *)(hdr + 2);
    p = (byte *)(hdr + 4);
    if (flipV != 0)
        hot.y = w - hot.y;
    rect[0] = ((short *)&packedPos)[0] - hot.x;
    rect[1] = ((short *)&packedPos)[1] - hot.y;
    rect[2] = rect[0] + n;
    rect[3] = rect[1] + w;
    DF_TRACE("      probe: hdr strips=%d width=%d hotX=%d flip=%d -> inner rect=(%d,%d,%d,%d) pt=(y=%d,x=%d)",
             (int)n, (int)w, (int)hot.y, (int)flipV, (int)rect[0], (int)rect[1], (int)rect[2],
             (int)rect[3], (int)pt.y, (int)pt.x);
    /* 0x004121c0: the point's halves are register copies loaded right after
     * the call, before its result is tested */
    r = PointInSceneRect(pt, rect);
    if (r == 0) {
        UnlockResObject(hSprite);
        return 0;
    }
    pt.y -= rect[0];
    pt.x -= rect[1];
    for (i = 0; i < n; i++) {
        len = *(short *)p;
        p += 2;
        if (pt.y <= 0) {
            r = ProbePackedHotspotStrip(p, w, pt.x);
            UnlockResObject(hSprite);
            return r;
        }
        pt.y--;
        p += len;
    }
    FatalError(0, 0x464);
    return 0;
}


/* Probes one RLE strip for probeCoord, from ProbeSpriteHotspotStripHit's and
 * ProbeScaledHotspotStripHit's column walks. In the control byte the high 6 bits are the run length
 * (>> 2) and the low 2 bits select how probeCoord / spanDim advance and how the data pointer steps.
 * Exhausting spanDim without finishing the probe is fatal (0x465). */
short ProbePackedHotspotStrip(byte *pSliceData, short spanDim, short probeCoord)  /* 0x00412260 */
{
    /* short: every return is a word (`mov ax,1` / `xor ax,ax`); the data
     * pointer itself walks (`inc edx` before the control byte is read) */
    short ctl;
    short runLen;

    do {
        ctl = *pSliceData++;
        runLen = ctl >> 2;
        DF_TRACE("        run: ctl=%02x type=%d len=%d probeLeft=%d spanLeft=%d",
                 ctl, ctl & 3, (int)runLen, (int)probeCoord, (int)spanDim);
        switch (ctl & 3) {
        case 0:
            probeCoord -= runLen;
            if (probeCoord <= 0)
                return 1;
            break;
        case 1:
            probeCoord -= runLen;
            if (probeCoord <= 0)
                return 0;
            break;
        case 2:
            probeCoord -= runLen;
            pSliceData++;
            if (probeCoord <= 0)
                return 1;
            break;
        case 3:
            probeCoord -= runLen;
            pSliceData += runLen;
            if (probeCoord <= 0)
                return 1;
            break;
        }
        spanDim -= runLen;
    } while (spanDim > 0);
    FatalError(0, 0x465);
    return 0;
}


/* The scaled stamp: sets the gTrig* source / destination state for a frame stamped at stampSize,
 * builds the column maps (InitTrigHotspotBresenhamTables) and picks the walk by scale: the
 * dual-plane walk when srcRows < dstRows >> 1, the steep walk when srcRows < dstRows, otherwise
 * the shallow walk. */
void __cdecl RasterStampChildIntoTrigSceneHotspotBuf(int *hSprite, uint32_t packedTopLeft,
                                                     int pDstDesc, short flipV, short zoneBand,
                                                     ScenePointYX stampSize) /* 0x00412310 */
{
    /* stampSize is the stamped size {short rows, short cols} and packedTopLeft the {short y,
     * short x} position, both by value and read by halves. r.hot (frame +0x1c) is the
     * resource's {y, x} hot-spot dword; its halves are reused for the scaled hot-spot row and
     * the left edge, then for the clipped width; r.end.y (+0x22) is the right edge (+0x20 is
     * never touched); hx is also the clip-gap variable once the left edge is stored. pv is the
     * stamped width widened once (spill slot +0x24), used for the hot-spot scale and the
     * gTrigDstWidth store.
     * Byte match: the right edge re-reads the stored left edge through a volatile lvalue (C2
     * forwards it to cx after colouring); the bounds test's own re-read is kept by the first
     * test's block (an empty __asm block from line_patches.json stops C2's forwarding); u0..u12
     * only move the locals' symbol indices (register ties); and line_patches.json adds zero-code
     * padding that makes C2 spill pv with four slot references, so the pv / width
     * slots (0x24 / 0x18) are packed as in TI.EXE. */
    short  u0;
    short  u1;
    short  u2;
    short  u3;
    short  u4;
    short  u5;
    short  u6;
    short  u7;
    short  u8;
    short  u9;
    short  u10;
    short  u11;
    short  u12;
    int    pv;
    short *clip;
    short *hdr;
    byte  *p;
    short  nRows;
    short  width;
    short  stride;
    short  srcRows;
    short  dstRows;
    short  top;
    short  hx;
    short  skip;
    short  k;
    short  len;
    short  leftGap;
    short  bottom;
    short  b2;
    struct { POINTS hot, end; } r;
    int    dst;
    HotspotPlaneFn fn;
#define P6_HI (stampSize.x)
#define PK_Y  (((short *)&packedTopLeft)[0])
#define PK_X  (((short *)&packedTopLeft)[1])

    clip = gClipRect;
    if (!(stampSize.y > 0 && P6_HI > 0 && P6_HI < 0x1000))
        return;

    hdr = (short *)GetResDataPtr(hSprite);
    dst    = *(int *)(pDstDesc + 4);
    stride = *(short *)(pDstDesc + 2);
    nRows  = hdr[0];
    width  = hdr[1];

    if (0x400 < width)
        FatalError(0, 0x466);

    *(int *)&r.hot = *(int *)(hdr + 2);
    p = (byte *)(hdr + 4);
    hx = *(volatile short *)&r.hot.y;
    if (flipV != 0)
        hx = width - hx;

    r.hot.x = (r.hot.x * stampSize.y) / nRows;
    pv = P6_HI;
    hx = (hx * pv) / width;
    top    = PK_Y - r.hot.x;
    hx = PK_X - hx;
    r.hot.y = hx;
    r.end.y = P6_HI + *(volatile short *)&r.hot.y;
    bottom = stampSize.y + top;

    if (top < clip[2]) {
        if (*(volatile short *)&r.hot.y < clip[3] && clip[0] < bottom && clip[1] < r.end.y) {
            b2 = bottom;
            srcRows = nRows;
            hx = gClipRect[0];
            gTrigDstWidth = pv;
            dstRows = stampSize.y;
            gTrigSrcWidth = width;
            hx -= top;
            gTrigPixelRowBase = gTrigPixelRowsPtr;
            if (hx > 0) {
                fn = PickHotspotPlaneFn();
                dstRows -= hx;
                skip = (hx * nRows) / stampSize.y;
                if (0 < skip) {
                    k = skip;
                    do {
                        len = *(short *)p;
                        p += 2;
                        fn(p, 0, 0);
                        p += len;
                    } while (--k != 0);
                }
                srcRows = nRows - skip;
                dst += clip[0] * stride;
                top = clip[0];
            } else {
                dst += top * stride;
            }
            b2 -= clip[2];
            if (b2 > 0) {
                dstRows -= b2;
                srcRows -= (b2 * nRows) / stampSize.y;
            }
            leftGap = clip[1] - r.hot.y;
            if (leftGap <= 0)
                leftGap = 0;
            dst += r.hot.y;
            hx = r.end.y - clip[3];     /* the overhang is never stored back: a reused short */
            if (hx > 0)
                r.hot.x = P6_HI - hx;
            else
                r.hot.x = P6_HI;
            gTrigZoneRowOffset = 0;
            gTrigZoneMapData = GetResDataPtr((int *)gTrigAnimBuf);
            gTrigSrcRows = srcRows;
            gTrigDstRows = dstRows;
            gTrigClipLeft = leftGap;
            gTrigClipRight = r.hot.x;
            gTrigBandOriginX = -r.hot.y;
            gTrigScanRow = top;
            gTrigZoneBand = zoneBand;
            gTrigMirrored = flipV;
            gTrigDstStride = stride;
            InitTrigHotspotBresenhamTables();
            if (srcRows < dstRows >> 1) {
                WalkTrigHotspotRasterBandDualPlane(p, dst);
            } else if (srcRows < dstRows) {
                WalkTrigHotspotRasterBandSteep(p, dst);
            } else {
                WalkTrigHotspotRasterBandShallow(p, dst);
            }
            UnlockResObject((int *)gTrigAnimBuf);
            UnlockResObject(hSprite);
            return;
        }
    }
    UnlockResObject(hSprite);
#undef P6_HI
#undef PK_Y
#undef PK_X
}


/* Builds the source <-> destination column maps of the scaled stamp (gTrigSrcToDstMap,
 * gTrigDstToSrcMap) by Bresenham stepping over gTrigSrcWidth and gTrigDstWidth. */
void __stdcall InitTrigHotspotBresenhamTables(void) /* 0x004126a0 */
{
    /* 0x004126a0: the doubled extents stay in esi / edi for both walks, the
     * byte offset of the next step entry is its own variable, both tables are
     * re-read from their globals at every store, and the dst < src walk is
     * the fall-through (the exe's first block). */
    int   twoSrc;   /* 2 * gTrigSrcWidth */
    int   twoDst;   /* 2 * gTrigDstWidth */
    int   err;
    int   off;
    int   srcIdx;
    int   dstIdx;

    dstIdx = 0;
    srcIdx = 0;
    twoSrc = gTrigSrcWidth * 2;
    twoDst = gTrigDstWidth * 2;
    if (gTrigDstWidth < gTrigSrcWidth) {
        err = -gTrigSrcWidth;
        *(int *)gTrigDstToSrcMap = dstIdx;
        off = 0;
        *(int *)gTrigSrcToDstMap = dstIdx;
        for (;;) {
            if (0 < err) {
                off += 4;
                dstIdx++;
                srcIdx++;
                *(int *)(gTrigDstToSrcMap + off) = srcIdx;
                err -= twoSrc;
            } else
                srcIdx++;
            if (gTrigSrcWidth == srcIdx)
                break;
            ((int *)gTrigSrcToDstMap)[srcIdx] = dstIdx;
            err += twoDst;
        }
        ((int *)gTrigDstToSrcMap)[gTrigDstWidth] = gTrigSrcWidth;
        return;
    }
    err = -gTrigDstWidth;
    *(int *)gTrigSrcToDstMap = 0;
    *(int *)gTrigDstToSrcMap = 0;
    off = 0;
    for (;;) {
        if (0 < err) {
            off += 4;
            srcIdx++;
            dstIdx++;
            err -= twoDst;
            *(int *)(gTrigSrcToDstMap + off) = dstIdx;
        } else
            dstIdx++;
        ((int *)gTrigDstToSrcMap)[dstIdx] = srcIdx;
        if (gTrigDstWidth == dstIdx)
            break;
        err += twoSrc;
    }
    ((int *)gTrigSrcToDstMap)[gTrigSrcWidth] = gTrigDstWidth;
}


/* Rasterises the scaled stamp when it upscales by less than 2x (src rows < dst rows): one source row
 * per band; "steep" is the slope of the row map. */
void WalkTrigHotspotRasterBandSteep(byte *pRleCursor, int scanBase) /* 0x00412790 */
{
    /* frame: step +0, fn +4, dec +8, the row length +0xc */
    int            step;
    HotspotPlaneFn fn;
    int            dec;
    int            len;
    int            n;
    int            err;
    int            k;

    n = gTrigDstRows;
    if (n != 0) {
        step = gTrigSrcRows * 2;
        dec  = n * 2 - step;
        err  = gTrigSrcRows - dec;
        fn   = PickHotspotPlaneFn();
        do {
            k = 1;
            while (err < 0) {
                k++;
                err += step;
            }
            len = *(short *)pRleCursor;      /* DF_PLANE_ROW, with the length a frame local */
            pRleCursor += 2;
            fn(pRleCursor, 0, 0);
            pRleCursor += len;
            PrepareSlotEdgeCache(0, 0);
            do {
                BuildTrigScanBandList();
                gTrigScanRow++;
                if (gTrigBandListLen != 0) {
                    GatherTrigBandHitSpans(0);
                    if (gTrigHitSpanCount != 0)
                        BlitScaledTrigHotspotSpans(scanBase);
                }
                scanBase += gTrigDstStride;
            } while (--n != 0 && --k != 0);
            err -= dec;
        } while (n != 0);
    }
}


/* Rewrites plane planeIdx's spans in place as {src a, src b, dst(a), dst(b)} through the
 * source-to-destination column map; edgeMode selects the end-inclusive form. */
static void PrepareSlotEdgeCache(int edgeMode, int planeIdx) /* 0x004128a0 */
{
  int n;
  int *src;
  int *dst;
  int b;
  int x;
  int *tab;

  /* 0x004128a0: n*8-8 / n*16-16 as lea displacements (pointer arithmetic on
   * int *, not an (n-1) the compiler would share), and a post-decrement test.
   * The table is read twice per row (two non-volatile reads C2 keeps apart:
   * the second is forwarded after colouring into the exe's dead `mov ebx,ebp`,
   * and the scheduler reads through ebx). `a` is block-local in both loops (its
   * web numbering); src -= 2 after the stores so the n-- copy is not renamed
   * off eax by the post-colouring renamer (FUN_00422f24 avoids the last two
   * defined registers: here esi and edx, not eax). */
  src = (int *)gTrigPlaneSpans[planeIdx] + gTrigPlaneSpanCounts[planeIdx] * 2 - 2;
  dst = (int *)gTrigPlaneSpans[planeIdx] + gTrigPlaneSpanCounts[planeIdx] * 4 - 4;
  n = gTrigPlaneSpanCounts[planeIdx];
  if (edgeMode != 0) {
    while (n-- != 0) { int a_b;
      a_b = src[0];
      b = src[1] - 1;
      dst[0] = a_b;
      dst[1] = b;
      tab = *(int **)&gTrigSrcToDstMap;
      a_b = (*(int **)&gTrigSrcToDstMap)[a_b];
      x = tab[b];
      dst[2] = a_b;
      dst[3] = x;
      src -= 2;
      dst -= 4;
    }
  } else {
    while (n-- != 0) { int a_a;
      a_a = src[0];
      b = src[1] - 1;
      dst[0] = a_a;
      dst[1] = b;
      tab = *(int **)&gTrigSrcToDstMap;
      a_a = (*(int **)&gTrigSrcToDstMap)[a_a];
      x = tab[b + 1] - 1;
      dst[2] = a_a;
      dst[3] = x;
      src -= 2;
      dst -= 4;
    }
  }
}






/* Builds the list of visible bands of the current scan row from the zone map, against gTrigZoneBand
 * (pairs of {height, level} bytes, at gTrigBandList). */
void __stdcall BuildTrigScanBandList(void)                             /* 0x00412940 */
{
    int    y;         /* running scan line: loaded before the guard (esi) */
    int    off;
    byte  *p;         /* the band entries: {height, level} byte pairs */
    int    count;
    int    i;
    int    top;
    int    bottom;

    y = gTrigBandOriginX;
    if (gTrigAnimBufSize == 0) {
        /* Ground truth 0x00412940 stores THROUGH the pointer (`*gTrigBandList =
         * gTrigClipRight;`). The port dropped the `*` and assigned the pointer itself,
         * overwriting the band-list base (set to &gTrigBandListStore by InitRenderState) with a
         * scan height, 0, and the next line wrote through NULL: SIGSEGV on the stairs,
         * -Wint-conversion is suppressed in the Makefile, which is why it compiled
         * silently (class 1). */
        *gTrigBandList = gTrigClipRight;
        gTrigBandList[1] = gTrigClipLeft;
        gTrigBandListLen    = 2;
        return;
    }

    off = *(ushort *)(gTrigZoneMapData + gTrigScanRow * 2);
    if (gTrigZoneRowOffset == off)
        return;
    p = (byte *)(off + gTrigZoneMapData);
    gTrigZoneRowOffset = off;
    count = *p++;
    gTrigBandListLen = 0;
    for (i = 0; i < count; i++) {
        top = y;
        if (gTrigClipRight <= y)
            return;
        y += *p++;
        bottom = y;
        if (*p++ >= gTrigZoneBand) {
            if (top < gTrigClipLeft)
                top = gTrigClipLeft;
            if (bottom > gTrigClipRight)
                bottom = gTrigClipRight;
            if (bottom > top) {
                if (gTrigBandListLen > 0 && gTrigBandList[gTrigBandListLen - 2] == top)
                    gTrigBandList[gTrigBandListLen - 2] = bottom;
                else {
                    gTrigBandList[gTrigBandListLen]     = bottom;
                    gTrigBandList[gTrigBandListLen + 1] = top;
                    gTrigBandListLen += 2;
                }
            }
        }
    }
}



/* Intersects plane planeIdx's spans with the band list into gTrigHitSpans. */
static void GatherTrigBandHitSpans(int planeIdx) /* 0x00412a70 */
{
  int *pRow;
  int *pBand;
  int *out;
  int rows;
  int bands;
  int nOut;
  int a;
  int b;
  int hi;
  int x0;
  int lo;
  int x1;
  int x2;
  int x3;
  int top;
  int bot;
  int left;
  int right;
  int zz0;

  /* Structured rewrite of Ghidra's goto-joined loop (same reads, same order of
   * row/band advances, the a == x3 case still adds planeIdx * 0x400). The
   * exe decrements a inside each arm of the hi choice (`dec esi` after the
   * compare: a-- / --a in the subscript) and negates x * 0x28 before adding
   * the scan-node base (a byte-pointer subtraction, -0x28 as displacement). */
  gTrigHitSpanCount = 0;
  nOut = 0;
  rows = gTrigPlaneSpanCounts[planeIdx];
  if (rows == 0)
    return;
  bands = gTrigBandListLen >> 1;
  if (bands == 0)
    return;
  pRow = (int *)gTrigPlaneSpans[planeIdx];
  out = (int *)gTrigHitSpans;
  pBand = gTrigBandList;
  a = *pBand++;
  b = *pBand++;
  if (gTrigDstWidth < gTrigSrcWidth)
    hi = ((int *)gTrigDstToSrcMap)[a--] - 1;
  else
    hi = ((int *)gTrigDstToSrcMap)[--a];
  lo = ((int *)gTrigDstToSrcMap)[b];
  x0 = *pRow++;
  x1 = *pRow++;
  x2 = *pRow++;
  x3 = *pRow++;
  if (x2 < 0)
    x2 = *(int *)((byte *)gScanNodeBase - x2 * 0x28 - 0x28);
  if (x3 < 0)
    x3 = *(int *)((byte *)gScanNodeBase - x3 * 0x28 - 0x28);
  while (rows != 0 && bands != 0) {
    if (b < x2) {
      top = x2;
      if (lo < x0)
        left = x0;
      else
        left = lo;
    } else {
      top = b;
      if (lo > x0)
        left = lo;
      else
        left = x0;
    }
    if (a > x3) {
      bot = x3;
      if (x1 < hi)
        right = x1;
      else
        right = hi;
    } else {
      bot = a;
      if (x1 > hi)
        right = hi;
      else
        right = x1;
    }
    if (bot >= top && right >= left) {
      nOut++;
      *out++ = planeIdx * 0x400 + left;
      *out++ = right - left + 1;
      *out++ = top;
      *out++ = bot - top + 1;
    }
    if (a > x3) {
      if (--rows != 0) {
        x0 = *pRow++;
        x1 = *pRow++;
        x2 = *pRow++;
        x3 = *pRow++;
        if (x2 < 0)
          x2 = *(int *)((byte *)gScanNodeBase - x2 * 0x28 - 0x28);
        if (x3 < 0)
          x3 = *(int *)((byte *)gScanNodeBase - x3 * 0x28 - 0x28);
      }
    } else if (a < x3) {
      if (--bands != 0) {
        a = *pBand++;
        b = *pBand++;
        if (gTrigDstWidth < gTrigSrcWidth)
          hi = ((int *)gTrigDstToSrcMap)[a--] - 1;
        else
          hi = ((int *)gTrigDstToSrcMap)[--a];
        lo = ((int *)gTrigDstToSrcMap)[b];
      }
    } else {
      if (--rows != 0) {
        x0 = *pRow++;
        x1 = *pRow++;
        x2 = *pRow++;
        x3 = *pRow++;
        if (x2 < 0)
          x2 = *(int *)((byte *)gScanNodeBase - x2 * 0x28 - 0x28);
        if (x3 < 0)
          x3 = *(int *)((byte *)gScanNodeBase - x3 * 0x28 - 0x28);
        if (--bands != 0) {
          a = *pBand++;
          b = *pBand++;
          if (gTrigDstWidth < gTrigSrcWidth)
            hi = ((int *)gTrigDstToSrcMap)[a--] - 1 + planeIdx * 0x400;
          else
            hi = planeIdx * 0x400 + ((int *)gTrigDstToSrcMap)[--a];
          lo = planeIdx * 0x400 + ((int *)gTrigDstToSrcMap)[b];
        }
      }
    }
  }
  gTrigHitSpanCount = nOut;
}



/* Rasterises the scaled stamp when it upscales by 2x or more (src rows < dst rows >> 1): up to
 * eight source rows are in flight, with scan nodes interpolating the edges. */
void WalkTrigHotspotRasterBandDualPlane(byte *pRleCursor, int scanBase) /* 0x00412df0 */
{
    /* Frame as the exe: cur +0, fn +4, rowsLeft +8, next +0xc, pCount +0x10,
     * dstLeft +0x14, counts[8] +0x18. */
    int            cur;
    HotspotPlaneFn fn;
    int            rowsLeft;
    int            next;
    int           *pCount;
    int            dstLeft;
    int            counts[8];
    int            err;
    int            idx;
    /* 0x00412e2e zeroes ECX for the first row and 0x00412e9d sets it to
     * 0xfffffc00 for every later one. */
    int            delta;
    int            k;
    int            len;
    int            j;

    fn = PickHotspotPlaneFn();
    err = -gTrigDstRows;
    cur = 0;
    dstLeft = gTrigDstRows;
    idx = 0;
    delta = 0;
    gScanNodeCount = 0;
    rowsLeft = gTrigSrcRows;
    if (rowsLeft != 0) {
        do {
            k = 0;
            do {
                k++;
                err += gTrigSrcRows;
            } while (err < 0);
            err -= gTrigDstRows;
            counts[idx] = k;
            len = *(short *)pRleCursor;          /* DF_PLANE_ROW, length in a local */
            pRleCursor += 2;
            fn(pRleCursor, delta, idx);
            pRleCursor += len;
            PrepareSlotEdgeCache(0, idx);
            idx++;
            if (--rowsLeft == 0)
                break;
            delta = -0x400;
        } while (idx < 7);
    }
    do {
        if (rowsLeft != 0) {
            k = 0;
            do {
                k++;
                err += gTrigSrcRows;
            } while (err < 0);
            err -= gTrigDstRows;
            counts[idx] = k;
            len = *(short *)pRleCursor;
            pRleCursor += 2;
            /* 0x00412ed8..0x00412ee2: `cmp edi,1 / sbb / and 0x2000 / sub 0x400`,
             * VC++'s branchless `x == 0 ? A : B`, through delta (the cursor
             * argument is loaded before it) */
            delta = idx == 0 ? 0x1c00 : -0x400;
            fn(pRleCursor, delta, idx);
            pRleCursor += len;
            PrepareSlotEdgeCache(0, idx);
            idx++;
            if (idx == 8)
                idx = 0;
            rowsLeft--;
        }
        next = cur + 1;
        if (next == 8)
            next = 0;
        /* ComputeTrigBandCrossings walks a rolling 0..7 band index from param_2 and stops only
         * when it EQUALS param_3; an 8 never compares equal after the 8->0 wrap, so the loop
         * runs forever, writing 4 bytes further up the stack each pass until it smashes its
         * frame. Log the arguments if they can do that; silent otherwise. */
        if ((unsigned)idx > 7u || (unsigned)cur > 7u)
            DF_TRACE("trigBands: RUNAWAY args start=%d end=%d (both must be 0..7)",
                     (int)cur, (int)idx);
        ComputeTrigBandCrossings(counts, cur, idx);
        j = 0;
        pCount = &counts[cur];
        if (*pCount > 0) {
            do {
                BuildTrigScanBandList();
                if (gTrigBandListLen != 0) {
                    GatherTrigBandHitSpans(cur);
                    if (gTrigHitSpanCount != 0)
                        BlitScaledTrigHotspotSpans(scanBase);
                }
                scanBase += gTrigDstStride;
                gTrigScanRow++;
                AdvanceScanNodeSteps();
                if (--dstLeft == 0)
                    break;
                j++;
            } while (*pCount > j);
        }
        cur = next;
    } while (dstLeft != 0);
}




/* For each span edge of the current row, finds the matching edge in the following rows and enrols a
 * scan node per crossing. */
static void ComputeTrigBandCrossings(int pPerPlane, int firstPlane, int endPlane) /* 0x00412fb0 */
{
  /* Four 8-entry stack arrays, as in the exe's frame (once separate locals, whose per-band
   * writes overran them; class 3): rowPtr (esp+0x2c) the band's edge-record cursor, count
   * (esp+0x50) its record count, leftAt / rightAt (esp+0x70 / 0x90) the record words each band's
   * left and right crossing is written into. The inner loop's j is strength-reduced to a byte
   * offset (ebx = 4j); only the count is kept as j itself. */
  int r;
  int d;
  int bLeft;
  int bRight;
  int n;
  int j;
  int m;
  uint32_t **pp;
  int row;
  int i;
  int v94;
  int v98;
  uint32_t *p8;
  uint32_t *p12;
  int *pBand;
  uint32_t *rowPtr[8];
  int count[8];
  uint32_t *leftAt[8];
  uint32_t *rightAt[8];

  DecrementAndTrimScanNodes();
  n = 0;
  i = firstPlane;
  do {
    count[n] = gTrigPlaneSpanCounts[i];
    rowPtr[n] = (uint32_t *)gTrigPlaneSpans[i];
    n++;
    i++;
    if (i == 8)
      i = 0;
  } while (endPlane != i);
  for (m = n; m < 8; m++) {
    count[m] = 0;
    rowPtr[m] = 0;
  }
  row = 0;
  if (0 < count[0]) {
    pBand = (int *)(pPerPlane + firstPlane * 4);
    do {
      n = *pBand;
      p8 = rowPtr[0] + 2;
      leftAt[0] = p8;
      p12 = rowPtr[0] + 3;
      rightAt[0] = p12;
      bRight = 0;
      bLeft = 0;
      i = firstPlane;
      for (j = 1; j < 8; j++) {
        if (rowPtr[j] == 0)
          break;
        i++;
        if (i == 8)
          i = 0;
        if (bLeft && bRight)
          break;
        if (!bLeft) {
          d = 7;
          if (row != 0)
            d = (*rowPtr[0] & 0x3ff) - (rowPtr[0][-3] & 0x3ff);
          r = FindNearestEdgeCrossing(i, *rowPtr[0], rowPtr[0][1], 1, &v94, &v98, d);
          if (r > 0) {
            r = EnrollScanNode(*p8, v98, n, j);
            if (j > 0) {
              pp = leftAt;
              m = j;
              do {
                **pp = r;
                pp++;
              } while (--m != 0);
            }
            bLeft = 1;
          } else if (r < 0) {
            leftAt[j] = rowPtr[j] + v94 * 4 + 2;
          } else {
            bLeft = 1;
          }
        }
        if (!bRight) {
          /* 0x00413156: count[0] - 1 is loaded before the 7 */
          if (row < count[0] - 1)
            d = (rowPtr[0][4] & 0x3ff) - (rowPtr[0][1] & 0x3ff);
          else
            d = 7;
          r = FindNearestEdgeCrossing(i, *rowPtr[0], rowPtr[0][1], 0, &v94, &v98, d);
          if (r > 0) {
            r = EnrollScanNode(*p12, v98, n, j);
            if (j > 0) {
              pp = rightAt;
              m = j;
              do {
                **pp = r;
                pp++;
              } while (--m != 0);
            }
            bRight = 1;
          } else if (r < 0) {
            rightAt[j] = rowPtr[j] + v94 * 4 + 3;
          } else {
            bRight = 1;
          }
        }
        n += *(int *)(pPerPlane + i * 4);
      }
      rowPtr[0] += 4;
      row++;
    } while (row < count[0]);
  }
}

/* Among plane planeIdx's spans that overlap [colLo, colHi] (masked to 10 bits), finds the one whose
 * left edge (leftEdge != 0) or right edge is nearest colLo / colHi, clamping the difference to
 * maxDelta and accepting under 8. Writes the span index to *pSpanIdxOut and the matching destination
 * column (through gTrigSrcToDstMap) to *pMappedOut. Returns 0xffffffff for an exact hit, otherwise 1
 * if a span was found and 0 if not. */
static uint32_t FindNearestEdgeCrossing(int planeIdx, int colLo, int colHi, int leftEdge, int *pSpanIdxOut, int *pMappedOut, int maxDelta) /* 0x00413240 */
{
  /* 0x00413240: both columns are masked in place in their parameter slots and
   * compared signed; the span count is a post-decremented `while`; the
   * leftEdge != 0 case is the fall-through; a closer crossing stores the new
   * best before the two out-parameters. */
  int lo;
  int hi;
  int mappedCol;
  uint32_t *pSpan;
  int delta;
  int spanIdx;
  int bestDist;  /* local_8 */
  int spansLeft;  /* local_4 */

  colLo &= 0x3ff;
  colHi &= 0x3ff;
  bestDist = 8;
  pSpan = (uint32_t *)(uintptr_t)(unsigned int)(gTrigPlaneSpans[planeIdx]);
  spansLeft = gTrigPlaneSpanCounts[planeIdx];
  spanIdx = 0;
  while (spansLeft-- != 0) {
    lo = *pSpan & 0x3ff;
    hi = pSpan[1] & 0x3ff;
    if (lo <= colHi && colLo <= hi) {
      if (leftEdge != 0) {
        delta = colLo - lo;
        if (delta > maxDelta)
          delta = maxDelta;
        if (delta < -maxDelta)
          delta = -maxDelta;
        mappedCol = colLo - delta;
        if (delta < 0)
          delta = -delta;
        if (delta < bestDist) {
          bestDist = delta;
          *pSpanIdxOut = spanIdx;
          *pMappedOut = ((int *)gTrigSrcToDstMap)[mappedCol];
          if (delta == 0)
            return 0xffffffff;
        }
      }
      else {
        delta = colHi - hi;
        if (delta > maxDelta)
          delta = maxDelta;
        if (delta < -maxDelta)
          delta = -maxDelta;
        mappedCol = colHi - delta;
        if (delta < 0)
          delta = -delta;
        if (delta < bestDist) {
          bestDist = delta;
          *pSpanIdxOut = spanIdx;
          *pMappedOut = ((int *)gTrigSrcToDstMap)[mappedCol + 1] - 1;
          if (delta == 0)
            return 0xffffffff;
        }
      }
    }
    pSpan = pSpan + 4;
    spanIdx = spanIdx + 1;
  }
  return (uint32_t)(bestDist < 8);
}

/* Finds or creates a free 10-int node in the scan-node stepper table and initialises its Bresenham
 * position and target (x0, x1, steps, life). */
static int EnrollScanNode(int x0, int x1, int steps, int life) /* 0x00413390 */
{
  int *pLife;
  int dx;
  int slot;
  int *pNode;

  if (x0 < 0) {
    x0 = ((int *)(uintptr_t)(unsigned int)gScanNodeBase)[x0 * -10 - 10];
  }
  /* the exe re-compares the count with the index after the loop (Ghidra's
   * bVar5 was its flag reuse on the empty-table path) */
  pNode = (int *)(uintptr_t)(unsigned int)gScanNodeBase;
  slot = 0;
  if (0 < gScanNodeCount) {
    pLife = pNode + 9;
    do {
      if (*pLife == 0) break;
      pLife = pLife + 10;
      slot = slot + 1;
      pNode = pNode + 10;
    } while (slot < gScanNodeCount);
  }
  if (gScanNodeCount == slot) {
    gScanNodeCount = gScanNodeCount + 1;
  }
  *pNode = x0;
  pNode[1] = x0;
  pNode[2] = x1;
  dx = x1 - x0;
  pNode[3] = dx;
  if (dx < 0) {
    pNode[3] = -dx;
  }
  pNode[4] = 0;
  pNode[5] = steps;
  pNode[6] = pNode[3] * 2;
  pNode[7] = pNode[5] * 2;
  pNode[8] = pNode[6] - pNode[5];
  pNode[9] = life;
  return -1 - slot;
}


/* Decrements the countdown (field 9) of every active scan node, then trims nodes whose count is zero
 * from the tail. */
static void DecrementAndTrimScanNodes(void) /* 0x00413430 */
{
  int life;
  int *pLife;
  int i;
  int byteOff;

  if (gScanNodeCount != 0) {
    i = 0;
    byteOff = 0;
    if (0 < gScanNodeCount) {
      do {
        pLife = (int *)((uintptr_t)(unsigned int)gScanNodeBase + 0x24 + byteOff);
        life = *pLife;
        if (life != 0) {
          *pLife = life - 1;
        }
        byteOff = byteOff + 0x28;
        i = i + 1;
      } while (i < gScanNodeCount);
    }
    i--;
    do {
      if (*(int *)((uintptr_t)(unsigned int)gScanNodeBase + 0x24 + i * 0x28) != 0) {
        return;
      }
      i--;
      gScanNodeCount = gScanNodeCount - 1;
    } while (gScanNodeCount != 0);
  }
  return;
}




/* One Bresenham step for every live scan node. */
static void AdvanceScanNodeSteps(void) /* 0x00413490 */
{
  int n;
  int *p;

  n = gScanNodeCount;
  p = (int *)gScanNodeBase;
  for (; n != 0; n--, p += 10) {
    if (p[9] != 0) {
      while (p[8] >= 0) {
        if (p[1] < p[2])
          *p = *p + 1;
        else
          *p = *p - 1;
        p[8] = p[8] - p[7];
      }
      p[8] = p[8] + p[6];
      p[4] = p[4] + 1;
    }
  }
}


/* Rasterises the scaled stamp when it downscales (dst rows < src rows), which every shrunk 3-D prop
 * takes.
 *
 * Port note: two parameters. 0x004134e0 reads the cursor at [ESP+0x20] and the blit base at
 * [ESP+0x24], and 0x00412310 pushes both before branching between this and the steep walk;
 * Ghidra shows one argument because the pushes precede the branch. Two defects fixed 
 * from the raw disassembly (frame: SUB ESP,0xc + four pushes, so [ESP+0x10] = 2*dst and [ESP+0x14]
 * = 2*src - 2*dst): the skip-row error step is 2 * gTrigDstRows (EBX holds the plane function
 * pointer, not a step; the port had guessed 2 * gTrigSrcRows), and every output row ends with
 * `ADD [ESP+0x24],[0x00482e88]`, advancing scanBase by the row stride. The port never advanced it,
 * so every row of the prop was blitted onto the same scanline and the bag and the watch were
 * invisible. The dual-plane and steep walks always had it. */
void WalkTrigHotspotRasterBandShallow(byte *pRleCursor, int scanBase) /* 0x004134e0 */
{
    int            ebx_step;
    int            err;
    HotspotPlaneFn planeFn;
    int            rowsLeft;  /* local_4 */

    rowsLeft = gTrigDstRows;
    if (gTrigDstRows != 0) {
        int errStep;
        int iVar2;
        ebx_step = gTrigDstRows * 2;
        errStep     = gTrigSrcRows * 2 - ebx_step;
        err     = ebx_step - gTrigSrcRows;
        planeFn    = PickHotspotPlaneFn();
        do {
            for (;;) {
                DF_PLANE_ROW(pRleCursor, planeFn, 0, 0);
                if (err >= 0)
                    break;
                err += ebx_step;
            }
            BuildTrigScanBandList();
            if (gTrigBandListLen != 0) {
                PrepareSlotEdgeCache(1, 0);
                GatherTrigBandHitSpans(0);
                if (gTrigHitSpanCount != 0) {
                    BlitScaledTrigHotspotSpans(scanBase);
                }
            }
            err -= errStep;
            gTrigScanRow++;
            scanBase += gTrigDstStride;   /* 0x004135a7..b2 */
            rowsLeft--;
        } while (rowsLeft != 0);
    }
}


/* The scaled mate of ProbeSpriteHotspotStripHit (the AVI / scaled path): takes the extra {y, x}
 * scale, which must be 1..0xfff high and positive low, and scales the hot-spot and the column
 * before the same rect test and strip walk. */
short ProbeScaledHotspotStripHit(int *hSprite, int packedPos, short flipV, ScenePointYX scale,
                   ScenePointYX pt) /* 0x004135d0 */
{
    /* ONE rect {y1,x1,y2,x2} passed to PointInSceneRect, as in ProbeSpriteHotspotStripHit
     * (class 3). Frame as the exe: the hot-spot dword +0 (its y scaled in place), rect +4, then
     * the four widened divisors. scale is the {y, x} scale, which must be 1..0xfff high and
     * positive low; the first two guards share one `return 0`. hx is read through a volatile
     * lvalue (C2 then keeps it in cx instead of re-reading hot.y for `w - hx`, as in
     * RasterStampChildIntoTrigSceneHotspotBuf) and holds the scaled column before rect[1]: the
     * exe loads packedPos's high half only after hx's last use, which frees cx for rect[0]. */
    byte  *p;
    POINTS hot;
    short  rect[4];
    int    nI;
    int    sxI;
    int    wI;
    int    syI;
    short *hdr;
    short  w;
    short  n;
    short  hx;
    short  sy;
    short  row;
    short  col;
    short  i;
    short  len;
    short  r;

    if (scale.y > 0 && (sy = scale.x) > 0) {
        if (sy >= 0x1000)
            return 0;
        hdr = (short *)GetResDataPtr(hSprite);
        n = hdr[0];
        w = hdr[1];
        hot = *(POINTS *)(hdr + 2);
        p = (byte *)(hdr + 4);
        hx = *(volatile short *)&hot.y;
        if (flipV != 0)
            hx = w - hx;
        sxI = scale.y;
        nI = n;
        hot.x = (hot.x * sxI) / nI;
        syI = sy;
        wI = w;
        hx = (hx * syI) / wI;
        rect[0] = ((short *)&packedPos)[0] - hot.x;
        rect[1] = ((short *)&packedPos)[1] - hx;
        rect[2] = rect[0] + scale.y;
        rect[3] = rect[1] + sy;
        if ((short)PointInSceneRect(pt, rect) == 0) {
            UnlockResObject(hSprite);
            return 0;
        }
        pt.y -= rect[0];
        pt.x -= rect[1];
        row = (pt.y * nI) / sxI;
        col = (pt.x * wI) / syI;
        for (i = 0; i < n; i++) {
            len = *(short *)p;
            p += 2;
            if (row <= 0) {
                r = ProbePackedHotspotStrip(p, w, col);
                UnlockResObject(hSprite);
                return r;
            }
            row--;
            p += len;
        }
        FatalError(0, 0x467);
        return 0;
    }
    return 0;
}


/* Is the point inside a zone of at least zoneBand in the overlay's zone map? Bounds-checks the
 * adjusted packed point against *pTrigOverlayDims, snapshots the overlay scan coordinate globals
 * into gTrigAnimBuf scratch (GetResDataPtr / UnlockResObject), runs BuildTrigScanBandList() and
 * tests gTrigBandListLen > 0. The point is a two-short struct by value, read straight from its
 * parameter slot ([esp+0x14] / [esp+0x16]) like PointInSceneRect's; it returns a short. */
short __cdecl ProbeTriggerOverlayHotspotAtPackedPoint(short *pTrigOverlayDims, short zoneBand,
                                                      ScenePointYX pt)  /* 0x00413780 */
{
    if (pt.y < 0 || pTrigOverlayDims[0] <= pt.y)
        return 0;
    if (pt.x < 0 || pTrigOverlayDims[1] <= pt.x)
        return 0;

    gTrigZoneRowOffset  = 0;
    gTrigZoneMapData  = GetResDataPtr(gTrigAnimBuf);
    gTrigBandOriginX  = 0;
    gTrigScanRow  = pt.y;
    gTrigClipLeft  = pt.x;
    gTrigClipRight  = gTrigClipLeft + 1;
    gTrigZoneBand  = zoneBand;
    BuildTrigScanBandList();
    UnlockResObject((int *)gTrigAnimBuf);
    return gTrigBandListLen > 0;
}
