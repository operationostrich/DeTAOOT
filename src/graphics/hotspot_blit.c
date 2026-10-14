/* hotspot_blit.c -- TI.EXE 0x0042b7d0..0x00431470: the hotspot plane RLE decoders and the scaled hotspot blit. */

#include <stdint.h>
#include <windows.h>
#include <stdio.h>
#include "df_protos.h"
#include "topics/hotspot_raster.h"

/* The functions with a GEN-ASM-ARM block were assembly in TI.EXE: under VC++ they
 * are naked and the block is their whole body; GCC compiles the C arm. The VC++
 * definition comes last: compile_compare.py's harness drops the conditionals. */
#ifndef _MSC_VER
#define DF_NAKED
#else
#define DF_NAKED __declspec(naked)
#endif


/* Decodes one run-length row of a hotspot plane into the plane's pixel row (planeIdx * 0x400 bytes
 * past gTrigPixelRowBase) and records which stretches of it are opaque. Opcode byte b: bit 0 clear =
 * b >> 1 transparent pixels, bit 0 set = b >> 1 literal bytes follow. The opaque stretches become
 * [start, end) offset pairs in gTrigPlaneSpanBufs[planeIdx]; gTrigPlaneSpans / gTrigPlaneSpanCounts
 * get the list and its length. deltaPx is unused. FatalError 0x434 if the row does not end exactly at
 * gTrigSrcWidth pixels. */
void __cdecl DecodeHotspotPlaneRowForward(byte *pRle, int deltaPx, int planeIdx) /* 0x0042b7d0 */
{
    int   n;
    byte *pEnd;
    int   pos;
    int  *pSpan;
    unsigned int b;
    int   k;
    byte *pOut;

    pOut  = (byte *)(planeIdx * 0x400 + gTrigPixelRowBase);
    pEnd  = pOut + gTrigSrcWidth;
    /* gTrigPlaneSpanBufs is int[9] (the plane base table); `(&gTrigPlaneSpanBufs)[k]`
     * stepped 36 bytes per plane -- port-bug-classes.md #17. pSpan is read
     * before pos is zeroed (0x0042b7eb): that order gives the exe's registers. */
    pSpan = (int *)gTrigPlaneSpanBufs[planeIdx];
    pos   = 0;
    *pSpan = 0;
    do {
        b = *pRle++;
        n = (int)b >> 1;
        switch (b & 1) {          /* `and ecx,1` then a compare chain: a switch */
        case 0:
            if (*pSpan != pos) {
                pSpan[1] = pos;
                pSpan += 2;
            }
            pos  += n;
            pOut += n;
            *pSpan = pos;
            break;
        case 1:
            for (k = 0; k < n; k++)
                *pOut++ = *pRle++;
            pos += n;
            break;
        }
    } while (pOut < pEnd);
    if (*pSpan != pos) {
        pSpan[1] = pos;
        pSpan    = pSpan + 2;
    }
    gTrigPlaneSpans[planeIdx] = gTrigPlaneSpanBufs[planeIdx];
    gTrigPlaneSpanCounts[planeIdx] = (int)pSpan - gTrigPlaneSpanBufs[planeIdx] >> 3;
    if (pOut != pEnd)
        FatalError(0, 0x434);
}
/* Raises FatalError code 0x6e (the engine's common internal-error code) from source line 0x435, the
 * line after DecodeHotspotPlaneRowForward's own check. Nothing in the decoded code calls it. */
void RaiseFatalPlaneConfigError(void)  /* 0x0042b890 */
{
    FatalError(0x6e, 0x435);
}


/* Writes the hit spans gathered for one scanline into the destination row at dstBase.
 * gTrigHitSpans holds gTrigHitSpanCount records of four ints: x (source column in the pixel row at
 * gTrigPixelRowBase), w (source width; negative = mirrored, read backwards from its last pixel),
 * dx (destination offset from dstBase) and cnt (destination width). cnt == w copies; cnt < w shrinks
 * and cnt > w stretches, stepping the source by w / cnt (or cnt / w pixels written per source pixel)
 * with an error term for the remainder. Callers: the sprite/hotspot rasterisers (08_*).
 * Port note: the VC++ arm is a rewrite from the disassembly, instantiating the macros below. */
void BlitScaledTrigHotspotSpans(int dstBase) /* 0x0042b8a0 */
{
#ifdef _MSC_VER
    /* Byte-match arm: the original was C with one __asm block (the byte-swapping copy below), which
     * is why it keeps cnt, src and dst in memory. Four switches of 16 cases (shrink/expand,
     * mirrored/forward), each case one instance of the SHRINK / EXPAND macros. The #else arm is
     * the port's C, unchanged. */
    int           r;
    byte         *dst;
    int           d1;
    int           q;
    int          *pSpan;
    int           d2;
    int           e;
    int           w;
    int           dx;
    int           nSpans;
    int           t;
    int           x;
    int           cnt;
    byte         *p;
    unsigned int  b;
    int           n;
    unsigned int  b2;
    byte         *p2;
    byte         *src;

/* PUTk stores k copies of the pixel B at dst: a byte, a word, then dwords. W8 / W16 widen the pixel
 * to 2 / 4 copies in B, as the original did. */
#define PX1(B, P)     *dst++ = (byte)B
#define PX2(B, P)     *(unsigned short *)P = (unsigned short)B; P += 2
#define PX4(B, P)     *(unsigned int *)P = B; P += 4
#define W8(B, P)      B |= B << 8
#define W16(B, P)     B |= B << 16
#define PUT1(B, P)    PX1(B, P)
#define PUT2(B, P)    W8(B, P); P = dst; PX2(B, P); dst = P
#define PUT3(B, P)    PX1(B, P); W8(B, P); P = dst; PX2(B, P); dst = P
#define PUT4(B, P)    W8(B, P); W16(B, P); P = dst; PX4(B, P); dst = P
#define PUT5(B, P)    PX1(B, P); W8(B, P); W16(B, P); P = dst; PX4(B, P); dst = P
#define PUT6(B, P)    W8(B, P); P = dst; PX2(B, P); W16(B, P); PX4(B, P); dst = P
#define PUT7(B, P)    PX1(B, P); W8(B, P); P = dst; PX2(B, P); W16(B, P); PX4(B, P); dst = P
#define PUT8(B, P)    W8(B, P); W16(B, P); P = dst; PX4(B, P); PX4(B, P); dst = P
#define PUT9(B, P)    PX1(B, P); W8(B, P); W16(B, P); P = dst; PX4(B, P); PX4(B, P); dst = P
#define PUT10(B, P)   W8(B, P); P = dst; PX2(B, P); W16(B, P); PX4(B, P); PX4(B, P); dst = P
#define PUT11(B, P)   PX1(B, P); W8(B, P); P = dst; PX2(B, P); W16(B, P); PX4(B, P); PX4(B, P); dst = P
#define PUT12(B, P)   W8(B, P); W16(B, P); P = dst; PX4(B, P); PX4(B, P); PX4(B, P); dst = P
#define PUT13(B, P)   PX1(B, P); W8(B, P); W16(B, P); P = dst; PX4(B, P); PX4(B, P); PX4(B, P); dst = P
#define PUT14(B, P)   W8(B, P); P = dst; PX2(B, P); W16(B, P); PX4(B, P); PX4(B, P); PX4(B, P); dst = P
#define PUT15(B, P)   PX1(B, P); W8(B, P); P = dst; PX2(B, P); W16(B, P); PX4(B, P); PX4(B, P); PX4(B, P); dst = P
#define PUT16(B, P)   W8(B, P); W16(B, P); P = dst; PX4(B, P); PX4(B, P); PX4(B, P); PX4(B, P); dst = P

/* one destination pixel of a shrink step: e < 0 skips one more source pixel */
#define SH_STEP(k, OP, E, D1, D2) \
            if (E < 0) { src OP##= 1; *dst++ = *src; src OP##= k; E += D1; } \
            else       { *dst++ = *src; src OP##= k; E += D2; }
/* Shrink: every destination pixel steps k (or k+1) source pixels; OP is - (mirrored) or +.
 * q = w / cnt is k, r = w % cnt: r == 0 takes the unrolled fast path, otherwise d1 / d2 are the
 * error increments and e the running error. ORD lists the set-up statements in the order the
 * original wrote them (O_* below). */
#define SHRINK1(OP, ORD, D1, D2, N, E) \
    case 1: \
        ORD(D1 = (cnt * 2 - w) * 2;, D2 = (cnt - w) * 2;, N = cnt;, E = D1 >> 1;) \
        do { SH_STEP(1, OP, E, D1, D2) } while (--N != 0); \
        break;
#define SHRINK_FAST(k, OP, N1, N2) \
            src OP##= (k) >> 1; \
            for (N1 = cnt >> 2; N1 != 0; N1--) { \
                *dst++ = *src; src OP##= (k); \
                *dst++ = *src; src OP##= (k); \
                *dst++ = *src; src OP##= (k); \
                *dst++ = *src; src OP##= (k); \
            } \
            for (N2 = cnt & 3; N2 != 0; N2--) { \
                *dst++ = *src; src OP##= (k); \
            }
#define SHRINK(k, OP, ORD, D1, D2, N, E, N1, N2) \
    case k: \
        if (r != 0) { \
            ORD(D1 = ((k + 1) * cnt - w) * 2;, src OP##= (k) >> 1;, D2 = (k * cnt - w) * 2;, N = cnt;, E = D1 >> 1;) \
            do { SH_STEP((k), OP, E, D1, D2) } while (--N != 0); \
        } else { \
            SHRINK_FAST(k, OP, N1, N2) \
        } \
        break;
/* SHRINK with the error step taken before the copy on the skip path (the order one case of the original used) */
#define SH_STEP_E1(k, OP, E, D1, D2) \
            if (E < 0) { src OP##= 1; E += D1; *dst++ = *src; src OP##= k; } \
            else       { *dst++ = *src; src OP##= k; E += D2; }
#define SHRINK_E1(k, OP, ORD, D1, D2, N, E, N1, N2) \
    case k: \
        if (r != 0) { \
            ORD(D1 = ((k + 1) * cnt - w) * 2;, src OP##= (k) >> 1;, D2 = (k * cnt - w) * 2;, N = cnt;, E = D1 >> 1;) \
            do { SH_STEP_E1((k), OP, E, D1, D2) } while (--N != 0); \
        } else { \
            SHRINK_FAST(k, OP, N1, N2) \
        } \
        break;
#define SHRINK_DEFAULT(OP, ORD, D1, D2, N, E, N1, N2, N1d, N2d) \
    case 16: \
        if (r == 0) { \
            SHRINK_FAST(16, OP, N1d, N2d) \
            break; \
        } \
    default: \
        if (r != 0) { \
            ORD(D1 = ((q + 1) * cnt - w) * 2;, src OP##= q >> 1;, D2 = (q * cnt - w) * 2;, N = cnt;, E = D1 >> 1;) \
            do { SH_STEP(q, OP, E, D1, D2) } while (--N != 0); \
        } else { \
            SHRINK_FAST(q, OP, N1, N2) \
        } \
        break;

/* the pixel count beyond 16 in T: a byte, a word, then dwords */
#define PUTX(B, P, T) \
    if (T & 1) { PX1(B, P); } T >>= 1; \
    if (T & 1) { P = dst; PX2(B, P); dst = P; } T >>= 1; \
    if (T != 0) { P = dst; do { PX4(B, P); } while (--T != 0); dst = P; }
#define LD(B, OP)     B = *src; src OP##= 1
/* Expand: every source pixel becomes k (or k+1) destination pixels; q = cnt / w is k, r = cnt % w. */
#define EXPAND1(OP, ORD, D1, D2, E, BN, PN) \
    case 1: \
        ORD(D1 = (w * 2 - cnt) * 2;, D2 = (w - cnt) * 2;, E = D2;) \
        do { \
            if (E < 0) { LD(BN, OP); PUT2(BN, PN); E += D1; } \
            else       { *dst++ = *src; src OP##= 1; E += D2; } \
        } while (--w != 0); \
        break;
#define EXPAND(k, k1, OP, ORD, D1, D2, E, BN, PN, BP, PP, N, BF, PF, BR, PR) \
    case k: \
        if (r != 0) { \
            ORD(D1 = ((k + 1) * w - cnt) * 2;, D2 = (k * w - cnt) * 2;, E = D2;) \
            do { \
                if (E < 0) { LD(BN, OP); PUT##k1(BN, PN); E += D1; } \
                else       { LD(BP, OP); PUT##k(BP, PP); E += D2; } \
            } while (--w != 0); \
        } else { \
            for (N = w >> 2; N != 0; N--) { \
                LD(BF, OP); PUT##k(BF, PF); \
                LD(BF, OP); PUT##k(BF, PF); \
                LD(BF, OP); PUT##k(BF, PF); \
                LD(BF, OP); PUT##k(BF, PF); \
            } \
            for (w &= 3; w != 0; w--) { \
                LD(BR, OP); PUT##k(BR, PR); \
            } \
        } \
        break;
/* EXPAND with the unrolled loop's four iterations as macros (B, P, OP): the original did not write
 * every iteration's statements in the same order */
#define EXPAND_I(k, k1, OP, ORD, D1, D2, E, BN, PN, BP, PP, N, BF, PF, BR, PR, I1, I2, I3, I4) \
    case k: \
        if (r != 0) { \
            ORD(D1 = ((k + 1) * w - cnt) * 2;, D2 = (k * w - cnt) * 2;, E = D2;) \
            do { \
                if (E < 0) { LD(BN, OP); PUT##k1(BN, PN); E += D1; } \
                else       { LD(BP, OP); PUT##k(BP, PP); E += D2; } \
            } while (--w != 0); \
        } else { \
            for (N = w >> 2; N != 0; N--) { \
                I1(BF, PF, OP) \
                I2(BF, PF, OP) \
                I3(BF, PF, OP) \
                I4(BF, PF, OP) \
            } \
            for (w &= 3; w != 0; w--) { \
                LD(BR, OP); PUT##k(BR, PR); \
            } \
        } \
        break;
/* Unrolled-loop iterations: as PUT10 / PUT16 write them, and the statement orders the original used in some cases. */
#define IT_PUT10(B, P, OP)  LD(B, OP); PUT10(B, P);
#define IT10_SRC_LATE(B, P, OP)  B = *src; W8(B, P); P = dst; W16(B, P); PX2(B, P); src OP##= 1; PX4(B, P); PX4(B, P); dst = P;
#define IT10_SRC_AFTER_W8(B, P, OP)  B = *src; W8(B, P); src OP##= 1; P = dst; PX2(B, P); W16(B, P); PX4(B, P); PX4(B, P); dst = P;
#define IT16_SRC_AFTER_P(B, P, OP)  B = *src; W8(B, P); W16(B, P); P = dst; src OP##= 1; PX4(B, P); PX4(B, P); PX4(B, P); PX4(B, P); dst = P;
#define IT16_SRC_LAST(B, P, OP)  B = *src; W8(B, P); W16(B, P); P = dst; PX4(B, P); PX4(B, P); PX4(B, P); PX4(B, P); dst = P; src OP##= 1;
#define EXPAND_DEFAULT(OP, ORD, D1, D2, E, T, BX, PX, DX, T2, N, BF, PF, BR, PR, Nd, BFd, PFd, BRd, PRd, I16b) \
    case 16: \
        if (r == 0) { \
            for (Nd = w >> 2; Nd != 0; Nd--) { \
                LD(BFd, OP); PUT16(BFd, PFd); \
                I16b(BFd, PFd, OP) \
                LD(BFd, OP); PUT16(BFd, PFd); \
                LD(BFd, OP); PUT16(BFd, PFd); \
            } \
            for (w &= 3; w != 0; w--) { \
                LD(BRd, OP); PUT16(BRd, PRd); \
            } \
            break; \
        } \
    default: \
        if (r != 0) { \
            ORD(D1 = w * 2;, D2 = (w * 16 - cnt) * 2;, E = D2;) \
            do { \
                T = 0; \
                while (E < 0) { E += D1; T++; } \
                E += D2; \
                LD(BX, OP); PUT16(BX, PX); \
                PUTX(BX, PX, T) \
            } while (--w != 0); \
        } else { \
            DX = q - 16; \
            for (N = w >> 2; N != 0; N--) { \
                LD(BF, OP); PUT16(BF, PF); T2 = DX; PUTX(BF, PF, T2) \
                LD(BF, OP); PUT16(BF, PF); T2 = DX; PUTX(BF, PF, T2) \
                LD(BF, OP); PUT16(BF, PF); T2 = DX; PUTX(BF, PF, T2) \
                LD(BF, OP); PUT16(BF, PF); T2 = DX; PUTX(BF, PF, T2) \
            } \
            for (w &= 3; w != 0; w--) { \
                LD(BR, OP); PUT16(BR, PR); T2 = DX; PUTX(BR, PR, T2) \
            } \
        } \
        break;
/* set-up statement orders: arguments in canonical order, expanded in the order written */
#define O_d1_d2_e(S_d1, S_d2, S_e) S_d1 S_d2 S_e
#define O_d2_d1_e(S_d1, S_d2, S_e) S_d2 S_d1 S_e
#define O_d1_d2_e_src_n(S_d1, S_src, S_d2, S_n, S_e) S_d1 S_d2 S_e S_src S_n
#define O_d1_d2_src_e_n(S_d1, S_src, S_d2, S_n, S_e) S_d1 S_d2 S_src S_e S_n
#define O_d1_d2_src_n_e(S_d1, S_src, S_d2, S_n, S_e) S_d1 S_d2 S_src S_n S_e
#define O_d1_n_d2_e_src(S_d1, S_src, S_d2, S_n, S_e) S_d1 S_n S_d2 S_e S_src
#define O_d1_src_d2_e_n(S_d1, S_src, S_d2, S_n, S_e) S_d1 S_src S_d2 S_e S_n
#define O_d1_src_d2_n_e(S_d1, S_src, S_d2, S_n, S_e) S_d1 S_src S_d2 S_n S_e
#define O_d1_d2_n_src_e(S_d1, S_src, S_d2, S_n, S_e) S_d1 S_d2 S_n S_src S_e
#define O_d2_d1_src_n_e(S_d1, S_src, S_d2, S_n, S_e) S_d2 S_d1 S_src S_n S_e
#define O_d2_d1_src_e_n(S_d1, S_src, S_d2, S_n, S_e) S_d2 S_d1 S_src S_e S_n
#define O_d1_d2_e_n(S_d1, S_d2, S_n, S_e) S_d1 S_d2 S_e S_n

    nSpans = gTrigHitSpanCount;
    if (nSpans != 0) {
        pSpan = (int *)gTrigHitSpans;
        do {
            x   = *pSpan++;
            w   = *pSpan++;
            dx  = *pSpan++;
            cnt = *pSpan++;
            if (w < 0) {
                w   = -w;
                src = (byte *)(w + x + gTrigPixelRowBase - 1);
                dst = (byte *)(dstBase + dx);
                if (cnt == w) {
                    if (gIs386Cpu == 0) {
                        /* the original's own assembly (mirrored span, same width): four bytes at a time, byte-swapped */
                        __asm {
                            push esi
                            push edi
                            push ecx
                            std
                            mov  ecx, cnt
                            shr  ecx, 2
                            je   done
                            mov  esi, src
                            mov  edi, dst
                            sub  esi, 3
                        dwords:
                            lodsd
                            bswap eax
                            mov  [edi], eax
                            add  edi, 4
                            loop dwords
                            mov  ecx, cnt
                            and  ecx, 3
                            je   done
                            add  esi, 3
                        bytes:
                            lodsb
                            mov  [edi], al
                            inc  edi
                            loop bytes
                        done:
                            cld
                            pop  ecx
                            pop  edi
                            pop  esi
                        }
                    } else {
                        for (n = cnt; n != 0; n--) {
                            *dst++ = *src--;
                        }
                    }
                } else if (cnt < w) {
                    q = w / cnt;
                    r = w % cnt;
                    switch (q) {
                    SHRINK1(-, O_d1_d2_e_n, d1, d2, n, e)
                    SHRINK(2, -, O_d1_d2_src_e_n, d1, d2, n, e, n, n)
                    SHRINK(3, -, O_d1_src_d2_n_e, e, d2, n, d1, n, n)
                    SHRINK(4, -, O_d1_d2_src_e_n, d1, d2, n, e, n, n)
                    SHRINK(5, -, O_d1_src_d2_e_n, d1, d2, n, e, n, n)
                    SHRINK(6, -, O_d1_src_d2_e_n, n, d2, d1, e, n, n)
                    SHRINK(7, -, O_d1_d2_src_e_n, d1, d2, n, e, n, n)
                    SHRINK(8, -, O_d1_d2_src_e_n, d1, d2, n, e, n, n)
                    SHRINK(9, -, O_d1_src_d2_e_n, d1, d2, n, e, n, n)
                    SHRINK(10, -, O_d1_d2_src_n_e, d1, d2, n, e, n, n)
                    SHRINK(11, -, O_d1_d2_src_e_n, e, d2, n, d1, n, n)
                    SHRINK(12, -, O_d1_d2_src_e_n, e, n, d2, d1, n, n)
                    SHRINK(13, -, O_d1_d2_src_n_e, d1, d2, n, e, n, n)
                    SHRINK(14, -, O_d1_d2_src_n_e, d1, d2, n, e, e, n)
                    SHRINK(15, -, O_d1_d2_src_n_e, d1, d2, n, e, n, n)
                    SHRINK_DEFAULT(-, O_d1_d2_e_src_n, n, d1, d2, e, n, n, n, n)
                    }
                } else {
                    q = cnt / w;
                    r = cnt % w;
                    switch (q) {
                    EXPAND1(-, O_d1_d2_e, d1, d2, n, b, p)
                    EXPAND(2, 3, -, O_d1_d2_e, d1, d2, e, b2, p2, b, p, n, b, p, b, p)
                    EXPAND(3, 4, -, O_d1_d2_e, d1, d2, e, dx, p2, b, p, n, b, p, b, p)
                    EXPAND(4, 5, -, O_d1_d2_e, d1, d2, e, b2, p2, b, p, n, b, p, b, p)
                    EXPAND(5, 6, -, O_d1_d2_e, d1, d2, e, b2, p2, b, p, n, b, p, b, p)
                    EXPAND(6, 7, -, O_d1_d2_e, d1, d2, e, b2, p2, b, p, n, b, p, b, p2)
                    EXPAND(7, 8, -, O_d1_d2_e, d1, d2, e, b2, p, b, p, n, b, p, b, p)
                    EXPAND(8, 9, -, O_d1_d2_e, d1, d2, e, b2, p, b2, p2, n, b, p, b, p)
                    EXPAND(9, 10, -, O_d1_d2_e, d1, d2, e, b, p2, b, p, n, b, p, b, p)
                    EXPAND_I(10, 11, -, O_d1_d2_e, d1, d2, e, b2, p, b, p2, n, b, p2, b, p, IT_PUT10, IT_PUT10, IT_PUT10, IT10_SRC_LATE)
                    EXPAND(11, 12, -, O_d1_d2_e, d1, d2, e, b2, p, b, p2, n, b, p, b, p)
                    EXPAND(12, 13, -, O_d1_d2_e, d1, d2, n, b2, p, b, p, e, b, p, b, p)
                    EXPAND(13, 14, -, O_d1_d2_e, d1, d2, n, b2, p2, b, p, e, b, p, b, p)
                    EXPAND(14, 15, -, O_d1_d2_e, d1, e, r, b2, p, b, p2, n, b, p, b, p)
                    EXPAND(15, 16, -, O_d1_d2_e, d1, d2, e, b2, p2, b, p, n, b, p, b, p)
                    EXPAND_DEFAULT(-, O_d1_d2_e, d1, d2, e, t, b, p, dx, t, n, b, p, b, p, n, b, p, b, p, IT16_SRC_AFTER_P)
                    }
                }
            } else {
                src = (byte *)(gTrigPixelRowBase + x);
                dst = (byte *)(dstBase + dx);
                if (cnt == w) {
                    n = cnt >> 2;
                    if (n != 0) {
                        unsigned int *ps = (unsigned int *)src;
                        unsigned int *pd = (unsigned int *)dst;
                        do {
                            *pd++ = *ps++;
                        } while (--n != 0);
                        src = (byte *)ps;
                        dst = (byte *)pd;
                    }
                    for (n = cnt & 3; n != 0; n--) {
                        *dst = *src;
                        src++;
                        dst++;
                    }
                } else if (cnt < w) {
                    q = w / cnt;
                    r = w % cnt;
                    switch (q) {
                    SHRINK1(+, O_d1_d2_e_n, d1, d2, n, e)
                    SHRINK(2, +, O_d1_d2_src_e_n, d1, d2, n, e, n, n)
                    SHRINK(3, +, O_d1_src_d2_n_e, d1, d2, n, e, n, n)
                    SHRINK(4, +, O_d1_d2_src_e_n, d1, d2, n, e, n, n)
                    SHRINK(5, +, O_d1_src_d2_e_n, d1, d2, n, e, n, n)
                    SHRINK(6, +, O_d1_src_d2_e_n, d1, d2, n, e, n, n)
                    SHRINK(7, +, O_d1_d2_n_src_e, d1, d2, n, e, b, n)
                    SHRINK(8, +, O_d1_d2_src_e_n, d1, d2, n, e, b, n)
                    SHRINK(9, +, O_d1_src_d2_e_n, d1, d2, n, e, n, n)
                    SHRINK(10, +, O_d1_d2_src_e_n, n, d2, b, e, d1, n)
                    SHRINK(11, +, O_d1_d2_src_n_e, d2, d1, e, n, n, n)
                    SHRINK_E1(12, +, O_d2_d1_src_e_n, d1, n, d2, e, n, n)
                    SHRINK(13, +, O_d1_d2_src_n_e, d1, d2, n, e, n, n)
                    SHRINK(14, +, O_d1_d2_src_n_e, n, d2, d1, e, n, n)
                    SHRINK(15, +, O_d1_d2_src_n_e, d1, d2, n, e, n, n)
                    SHRINK_DEFAULT(+, O_d1_d2_e_src_n, n, d1, d2, e, n, n, n, n)
                    }
                } else {
                    q = cnt / w;
                    r = cnt % w;
                    switch (q) {
                    EXPAND1(+, O_d1_d2_e, d1, d2, e, b, p2)
                    EXPAND(2, 3, +, O_d1_d2_e, d1, d2, e, b2, p, n, p, n, b, p, n, p)
                    EXPAND(3, 4, +, O_d1_d2_e, d1, d2, e, b2, p, n, p2, n, b, p, b, p)
                    EXPAND(4, 5, +, O_d1_d2_e, d1, d2, e, b2, p, b, p2, n, b, p, b, p)
                    EXPAND(5, 6, +, O_d1_d2_e, d1, d2, e, b2, p2, b, p, n, b, p, b, p)
                    EXPAND(6, 7, +, O_d1_d2_e, d1, d2, e, b2, p2, b, p, n, b, p, b, p)
                    EXPAND(7, 8, +, O_d1_d2_e, d1, d2, e, b2, p2, b, p, n, b, p, b, p2)
                    EXPAND(8, 9, +, O_d1_d2_e, d1, d2, e, b2, p2, b, p, n, b, p, b, p)
                    EXPAND(9, 10, +, O_d1_d2_e, d1, d2, e, b2, p, b, p, n, b, p, b, p)
                    EXPAND_I(10, 11, +, O_d1_d2_e, d1, d2, n, b2, p, b, p, e, b, p, b, p, IT_PUT10, IT_PUT10, IT10_SRC_AFTER_W8, IT10_SRC_LATE)
                    EXPAND(11, 12, +, O_d1_d2_e, d1, d2, e, b2, p2, b, p, n, b, p, b, p)
                    EXPAND(12, 13, +, O_d1_d2_e, d1, d2, e, b2, p2, b, p, n, b, p2, b, p)
                    EXPAND(13, 14, +, O_d1_d2_e, d1, r, n, b2, p, b, p2, e, b, p, b, p)
                    EXPAND(14, 15, +, O_d1_d2_e, d1, d2, e, b2, p2, b, p, n, b, p, b, p)
                    EXPAND(15, 16, +, O_d1_d2_e, d1, d2, e, b2, p, b, p2, n, b, p, b, p)
                    EXPAND_DEFAULT(+, O_d1_d2_e, d1, d2, e, x, b, p, dx, t, n, b, p, b, p, n, b, p, b, p, IT16_SRC_LAST)
                    }
                }
            }
        } while (--nSpans != 0);
    }
#undef PX1
#undef PX2
#undef PX4
#undef W8
#undef W16
#undef PUTX
#undef SHRINK1
#undef SHRINK_FAST
#undef SHRINK
#undef SHRINK_DEFAULT
#undef EXPAND1
#undef EXPAND
#undef EXPAND_DEFAULT
#undef EXPAND_I
#undef IT_PUT10
#undef IT10_SRC_LATE
#undef IT10_SRC_AFTER_W8
#undef IT16_SRC_AFTER_P
#undef IT16_SRC_LAST
#undef O_d1_d2_n_src_e
#undef O_d2_d1_src_n_e
#undef O_d2_d1_src_e_n
#undef SHRINK_E1
#undef SH_STEP_E1
#undef PUT1
#undef PUT2
#undef PUT3
#undef PUT4
#undef PUT5
#undef PUT6
#undef PUT7
#undef PUT8
#undef PUT9
#undef PUT10
#undef PUT11
#undef PUT12
#undef PUT13
#undef PUT14
#undef PUT15
#undef PUT16
#undef SH_NEG
#undef SH_POS
#undef SH_STEP
#undef LD
#undef O_d1_d2_e
#undef O_d1_d2_e_n
#undef O_d1_d2_e_src_n
#undef O_d1_d2_src_e_n
#undef O_d1_d2_src_n_e
#undef O_d1_n_d2_e_src
#undef O_d1_src_d2_e_n
#undef O_d1_src_d2_n_e
#undef O_d2_d1_e
#else
  int *pDstOff;
  uint8_t *srcBytePtr;
  uint32_t *srcWalk;
  uint8_t pixel;
  int errStepAlt;
  uint16_t pixelPair;
  uint32_t *srcPtr;
  int spanX;
  int errStep;
  uint32_t srcWidth;
  uint32_t spanW;
  uint32_t dstCount;
  int quadCount;
  int errTerm;
  uint32_t pixelDword;
  uint32_t pixelDwordB;
  uint32_t extraWordFlag;
  int spanCount;  /* local_38 */
  int srcQuads;  /* local_20 */
  int *pRec;  /* local_1c */
  uint32_t *srcCursor;  /* local_c */
  uint32_t *dstCursor;  /* local_8 */
  
  spanCount = gTrigHitSpanCount;
  if (gTrigHitSpanCount != 0) {
    pRec = gTrigHitSpans;
    do {
      spanX = *pRec;
      pDstOff = pRec + 2;
      spanW = pRec[1];
      srcPtr = (uint32_t *)(pRec + 3);
      pRec = pRec + 4;
      dstCount = *srcPtr;
      quadCount = (int)dstCount >> 2;
      if ((int)spanW < 0) {
        srcWidth = -spanW;
        srcPtr = (uint32_t *)(srcWidth + spanX + gTrigPixelRowBase);
        srcCursor = (uint32_t *)((int)srcPtr + -1);
        dstCursor = (uint32_t *)(dstBase + *pDstOff);
        if (dstCount == srcWidth) {
          if (gIs386Cpu == 0) {
            spanW = dstCount >> 2;
            if (spanW != 0) {
              do {
                srcPtr = srcPtr + -1;
                srcWidth = *srcPtr;
                *dstCursor = srcWidth >> 0x18 | (srcWidth & 0xff0000) >> 8 | (srcWidth & 0xff00) << 8 |
                           srcWidth << 0x18;
                dstCursor = dstCursor + 1;
                spanW = spanW - 1;
              } while (spanW != 0);
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                srcPtr = (uint32_t *)((int)srcPtr + -1);
                *(uint8_t *)dstCursor = *(uint8_t *)srcPtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
              }
            }
          }
          else {
            for (; dstCount != 0; dstCount = dstCount - 1) {
              *(uint8_t *)dstCursor = *(uint8_t *)srcCursor;
              srcCursor = (uint32_t *)((int)srcCursor + -1);
              dstCursor = (uint32_t *)((int)dstCursor + 1);
            }
          }
        }
        else if ((int)dstCount < (int)srcWidth) {
          spanX = (int)srcWidth / (int)dstCount;
          errTerm = (int)srcWidth % (int)dstCount;
          switch(spanX) {
          case 1:
            spanX = (dstCount * 2 + spanW) * 2;
            quadCount = dstCount + spanW;
            errTerm = spanX >> 1;
            do {
              if (errTerm < 0) {
                *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                srcCursor = (uint32_t *)((int)srcCursor + -2);
                errStep = spanX;
              }
              else {
                *(char *)dstCursor = (char)*srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                errStep = quadCount * 2;
              }
              dstCursor = (uint32_t *)((int)dstCursor + 1);
              errTerm = errTerm + errStep;
              dstCount = dstCount - 1;
            } while (dstCount != 0);
            break;
          case 2:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcPtr + -2);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[-2];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[-4];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[-6];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + -8;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + -2;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcPtr + -2);
              spanX = (dstCount * 3 + spanW) * 2;
              quadCount = dstCount * 2;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((int)srcCursor + -3);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -2);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 3:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcPtr + -2);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[-3];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[-6];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[-9];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + -0xc;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + -3;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcPtr + -2);
              spanX = (dstCount * 4 + spanW) * 2;
              quadCount = dstCount * 3;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((int)srcCursor + -4);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -3);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 4:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcPtr + -3);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[-4];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[-8];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[-0xc];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + -0x10;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + -4;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcPtr + -3);
              spanX = (dstCount * 5 + spanW) * 2;
              quadCount = dstCount * 4;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((int)srcCursor + -5);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -4);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 5:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcPtr + -3);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[-5];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[-10];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[-0xf];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + -0x14;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + -5;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcPtr + -3);
              spanX = (dstCount * 6 + spanW) * 2;
              quadCount = dstCount * 5;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((int)srcCursor + -6);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -5);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 6:
            if (errTerm == 0) {
              srcPtr = srcPtr + -1;
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(char *)dstCursor = (char)*srcPtr;
                *(uint8_t *)((int)dstCursor + 1) = *(uint8_t *)((int)srcPtr + -6);
                *(char *)((int)dstCursor + 2) = (char)srcPtr[-3];
                *(uint8_t *)((int)dstCursor + 3) = *(uint8_t *)((int)srcPtr + -0x12);
                dstCursor = dstCursor + 1;
                srcPtr = srcPtr + -6;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(char *)dstCursor = (char)*srcPtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcPtr = (uint32_t *)((int)srcPtr + -6);
              }
            }
            else {
              srcCursor = srcPtr + -1;
              spanX = (dstCount * 7 + spanW) * 2;
              quadCount = dstCount * 6;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((int)srcCursor + -7);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -6);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 7:
            if (errTerm == 0) {
              srcPtr = srcPtr + -1;
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(char *)dstCursor = (char)*srcPtr;
                *(uint8_t *)((int)dstCursor + 1) = *(uint8_t *)((int)srcPtr + -7);
                *(uint8_t *)((int)dstCursor + 2) = *(uint8_t *)((int)srcPtr + -0xe);
                *(uint8_t *)((int)dstCursor + 3) = *(uint8_t *)((int)srcPtr + -0x15);
                dstCursor = dstCursor + 1;
                srcPtr = srcPtr + -7;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(char *)dstCursor = (char)*srcPtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcPtr = (uint32_t *)((int)srcPtr + -7);
              }
            }
            else {
              srcCursor = srcPtr + -1;
              spanX = (dstCount * 8 + spanW) * 2;
              quadCount = dstCount * 7;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = srcCursor + -2;
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -7);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 8:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcPtr + -5);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[-8];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[-0x10];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[-0x18];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + -0x20;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + -8;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcPtr + -5);
              spanX = (dstCount * 9 + spanW) * 2;
              quadCount = dstCount * 8;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((int)srcCursor + -9);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -8);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 9:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcPtr + -5);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[-9];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[-0x12];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[-0x1b];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + -0x24;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + -9;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcPtr + -5);
              spanX = (dstCount * 10 + spanW) * 2;
              quadCount = dstCount * 9;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((int)srcCursor + -10);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -9);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 10:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcPtr + -6);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[-10];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[-0x14];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[-0x1e];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + -0x28;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + -10;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcPtr + -6);
              spanX = dstCount * 10;
              quadCount = (dstCount * 0xb + spanW) * 2;
              errTerm = quadCount >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((int)srcCursor + -0xb);
                  errStep = quadCount;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -10);
                  errStep = (spanX + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 0xb:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcPtr + -6);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[-0xb];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[-0x16];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[-0x21];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + -0x2c;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + -0xb;
              }
            }
            else {
              spanX = (dstCount * 0xc + spanW) * 2;
              srcCursor = (uint32_t *)((int)srcPtr + -6);
              quadCount = dstCount * 0xb;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((int)srcCursor + -0xc);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -0xb);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 0xc:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcPtr + -7);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[-0xc];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[-0x18];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[-0x24];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + -0x30;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + -0xc;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcPtr + -7);
              spanX = (dstCount * 0xd + spanW) * 2;
              quadCount = dstCount * 0xc;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((int)srcCursor + -0xd);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -0xc);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 0xd:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcPtr + -7);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[-0xd];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[-0x1a];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[-0x27];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + -0x34;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + -0xd;
              }
            }
            else {
              spanX = (dstCount * 0xe + spanW) * 2;
              srcCursor = (uint32_t *)((int)srcPtr + -7);
              quadCount = dstCount * 0xd;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((int)srcCursor + -0xe);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -0xd);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 0xe:
            if (errTerm == 0) {
              srcPtr = srcPtr + -2;
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(char *)dstCursor = (char)*srcPtr;
                *(uint8_t *)((int)dstCursor + 1) = *(uint8_t *)((int)srcPtr + -0xe);
                *(char *)((int)dstCursor + 2) = (char)srcPtr[-7];
                *(uint8_t *)((int)dstCursor + 3) = *(uint8_t *)((int)srcPtr + -0x2a);
                dstCursor = dstCursor + 1;
                srcPtr = srcPtr + -0xe;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(char *)dstCursor = (char)*srcPtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcPtr = (uint32_t *)((int)srcPtr + -0xe);
              }
            }
            else {
              spanX = (dstCount * 0xf + spanW) * 2;
              srcCursor = srcPtr + -2;
              quadCount = dstCount * 0xe;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((int)srcCursor + -0xf);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -0xe);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 0xf:
            if (errTerm == 0) {
              srcPtr = srcPtr + -2;
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(char *)dstCursor = (char)*srcPtr;
                *(uint8_t *)((int)dstCursor + 1) = *(uint8_t *)((int)srcPtr + -0xf);
                *(uint8_t *)((int)dstCursor + 2) = *(uint8_t *)((int)srcPtr + -0x1e);
                *(uint8_t *)((int)dstCursor + 3) = *(uint8_t *)((int)srcPtr + -0x2d);
                dstCursor = dstCursor + 1;
                srcPtr = srcPtr + -0xf;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(char *)dstCursor = (char)*srcPtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcPtr = (uint32_t *)((int)srcPtr + -0xf);
              }
            }
            else {
              spanX = (dstCount * 0x10 + spanW) * 2;
              srcCursor = srcPtr + -2;
              quadCount = dstCount * 0xf;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = srcCursor + -4;
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -0xf);
                  errStep = (quadCount + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 0x10:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcPtr + -9);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[-0x10];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[-0x20];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[-0x30];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + -0x40;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + -0x10;
              }
            }
            else {
LAB_0042c719:
              quadCount = ((spanX + 1) * dstCount + spanW) * 2;
              errTerm = dstCount * spanX;
              srcCursor = (uint32_t *)((int)srcCursor + -(spanX >> 1));
              errStep = quadCount >> 1;
              do {
                if (errStep < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + -1);
                  srcCursor = (uint32_t *)((uint8_t *)((int)srcCursor + -1) + -spanX);
                  errStepAlt = quadCount;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + -spanX);
                  errStepAlt = (errTerm + spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errStep = errStep + errStepAlt;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          default:
            if (errTerm != 0) goto LAB_0042c719;
            srcBytePtr = (uint8_t *)((int)srcCursor + -(spanX >> 1));
            for (; quadCount != 0; quadCount = quadCount + -1) {
              *(uint8_t *)dstCursor = *srcBytePtr;
              srcBytePtr = srcBytePtr + -spanX;
              *(uint8_t *)((int)dstCursor + 1) = *srcBytePtr;
              srcBytePtr = srcBytePtr + -spanX;
              *(uint8_t *)((int)dstCursor + 2) = *srcBytePtr;
              *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[-spanX];
              dstCursor = dstCursor + 1;
              srcBytePtr = srcBytePtr + -spanX + -spanX;
            }
            for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
              *(uint8_t *)dstCursor = *srcBytePtr;
              dstCursor = (uint32_t *)((int)dstCursor + 1);
              srcBytePtr = srcBytePtr + -spanX;
            }
          }
        }
        else {
          spanX = (int)dstCount % (int)srcWidth;
          srcQuads = (int)srcWidth >> 2;
          switch((int)dstCount / (int)srcWidth) {
          case 1:
            spanX = (srcWidth - dstCount) * 2;
            quadCount = spanX;
            do {
              if (quadCount < 0) {
                *(uint16_t *)dstCursor = CONCAT11((char)*srcCursor,(char)*srcCursor);
                dstCursor = (uint32_t *)((int)dstCursor + 2);
                errTerm = (spanW * -2 - dstCount) * 2;
              }
              else {
                *(char *)dstCursor = (char)*srcCursor;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = spanX;
              }
              srcCursor = (uint32_t *)((int)srcCursor + -1);
              quadCount = quadCount + errTerm;
              srcWidth = srcWidth - 1;
            } while (srcWidth != 0);
            break;
          case 2:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                *(uint16_t *)dstCursor = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                *(uint16_t *)((int)dstCursor + 2) =
                     CONCAT11(*(uint8_t *)((int)srcCursor + -1),*(uint8_t *)((int)srcCursor + -1))
                ;
                srcBytePtr = (uint8_t *)((int)srcCursor + -3);
                *(uint16_t *)(dstCursor + 1) =
                     CONCAT11(*(uint8_t *)((int)srcCursor + -2),*(uint8_t *)((int)srcCursor + -2))
                ;
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                *(uint16_t *)((int)dstCursor + 6) = CONCAT11(*srcBytePtr,*srcBytePtr);
                dstCursor = dstCursor + 2;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                *(uint16_t *)dstCursor = CONCAT11(pixel,pixel);
                dstCursor = (uint32_t *)((int)dstCursor + 2);
              }
            }
            else {
              spanX = (spanW * -2 - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  *(uint16_t *)((int)dstCursor + 1) = CONCAT11(pixel,pixel);
                  dstCursor = (uint32_t *)((int)dstCursor + 3);
                  errTerm = (spanW * -3 - dstCount) * 2;
                }
                else {
                  *(uint16_t *)dstCursor = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  dstCursor = (uint32_t *)((int)dstCursor + 2);
                  errTerm = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 3:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                *(uint16_t *)((int)dstCursor + 1) = CONCAT11(pixel,pixel);
                pixel = *(uint8_t *)((int)srcCursor + -1);
                *(uint8_t *)((int)dstCursor + 3) = pixel;
                *(uint16_t *)(dstCursor + 1) = CONCAT11(pixel,pixel);
                pixel = *(uint8_t *)((int)srcCursor + -2);
                *(uint8_t *)((int)dstCursor + 6) = pixel;
                *(uint16_t *)((int)dstCursor + 7) = CONCAT11(pixel,pixel);
                pixel = *(uint8_t *)((int)srcCursor + -3);
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                *(uint8_t *)((int)dstCursor + 9) = pixel;
                *(uint16_t *)((int)dstCursor + 10) = CONCAT11(pixel,pixel);
                dstCursor = dstCursor + 3;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                *(uint8_t *)dstCursor = pixel;
                *(uint16_t *)((int)dstCursor + 1) = CONCAT11(pixel,pixel);
                dstCursor = (uint32_t *)((int)dstCursor + 3);
              }
            }
            else {
              spanX = (spanW * -3 - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *dstCursor = CONCAT22(pixelPair,pixelPair);
                  dstCursor = dstCursor + 1;
                  errTerm = (spanW * -4 - dstCount) * 2;
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  *(uint16_t *)((int)dstCursor + 1) = CONCAT11(pixel,pixel);
                  dstCursor = (uint32_t *)((int)dstCursor + 3);
                  errTerm = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 4:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                *dstCursor = CONCAT22(pixelPair,pixelPair);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -1),
                                 *(uint8_t *)((int)srcCursor + -1));
                dstCursor[1] = CONCAT22(pixelPair,pixelPair);
                srcBytePtr = (uint8_t *)((int)srcCursor + -3);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -2),
                                 *(uint8_t *)((int)srcCursor + -2));
                dstCursor[2] = CONCAT22(pixelPair,pixelPair);
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                pixelPair = CONCAT11(*srcBytePtr,*srcBytePtr);
                dstCursor[3] = CONCAT22(pixelPair,pixelPair);
                dstCursor = dstCursor + 4;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                pixelPair = CONCAT11(pixel,pixel);
                *dstCursor = CONCAT22(pixelPair,pixelPair);
                dstCursor = dstCursor + 1;
              }
            }
            else {
              spanX = (spanW * -4 - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  *(uint32_t *)((int)dstCursor + 1) =
                       CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                  dstCursor = (uint32_t *)((int)dstCursor + 5);
                  errTerm = (spanW * -5 - dstCount) * 2;
                }
                else {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *dstCursor = CONCAT22(pixelPair,pixelPair);
                  dstCursor = dstCursor + 1;
                  errTerm = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 5:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                *(uint32_t *)((int)dstCursor + 1) = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                pixel = *(uint8_t *)((int)srcCursor + -1);
                *(uint8_t *)((int)dstCursor + 5) = pixel;
                *(uint32_t *)((int)dstCursor + 6) = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                pixel = *(uint8_t *)((int)srcCursor + -2);
                *(uint8_t *)((int)dstCursor + 10) = pixel;
                *(uint32_t *)((int)dstCursor + 0xb) =
                     CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                pixel = *(uint8_t *)((int)srcCursor + -3);
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                *(uint8_t *)((int)dstCursor + 0xf) = pixel;
                dstCursor[4] = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                dstCursor = dstCursor + 5;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                *(uint8_t *)dstCursor = pixel;
                *(uint32_t *)((int)dstCursor + 1) = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                dstCursor = (uint32_t *)((int)dstCursor + 5);
              }
            }
            else {
              spanX = (spanW * -5 - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *(uint16_t *)dstCursor = pixelPair;
                  *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                  errTerm = (spanW * -6 - dstCount) * 2;
                  dstCursor = (uint32_t *)((int)dstCursor + 6);
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  *(uint32_t *)((int)dstCursor + 1) =
                       CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                  dstCursor = (uint32_t *)((int)dstCursor + 5);
                  errTerm = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 6:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                *(uint16_t *)dstCursor = pixelPair;
                *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -1),
                                 *(uint8_t *)((int)srcCursor + -1));
                *(uint16_t *)((int)dstCursor + 6) = pixelPair;
                dstCursor[2] = CONCAT22(pixelPair,pixelPair);
                srcBytePtr = (uint8_t *)((int)srcCursor + -3);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -2),
                                 *(uint8_t *)((int)srcCursor + -2));
                *(uint16_t *)(dstCursor + 3) = pixelPair;
                *(uint32_t *)((int)dstCursor + 0xe) = CONCAT22(pixelPair,pixelPair);
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                pixelPair = CONCAT11(*srcBytePtr,*srcBytePtr);
                *(uint16_t *)((int)dstCursor + 0x12) = pixelPair;
                dstCursor[5] = CONCAT22(pixelPair,pixelPair);
                dstCursor = dstCursor + 6;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                *(uint16_t *)dstCursor = pixelPair;
                *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                dstCursor = (uint32_t *)((int)dstCursor + 6);
              }
            }
            else {
              spanX = (spanW * -6 - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelPair = CONCAT11(pixel,pixel);
                  *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                  *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                  errTerm = (spanW * -7 - dstCount) * 2;
                  dstCursor = (uint32_t *)((int)dstCursor + 7);
                }
                else {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *(uint16_t *)dstCursor = pixelPair;
                  *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                  errTerm = spanX;
                  dstCursor = (uint32_t *)((int)dstCursor + 6);
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 7:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                pixel = *(uint8_t *)((int)srcCursor + -1);
                *(uint8_t *)((int)dstCursor + 7) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)(dstCursor + 2) = pixelPair;
                *(uint32_t *)((int)dstCursor + 10) = CONCAT22(pixelPair,pixelPair);
                pixel = *(uint8_t *)((int)srcCursor + -2);
                *(uint8_t *)((int)dstCursor + 0xe) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 0xf) = pixelPair;
                *(uint32_t *)((int)dstCursor + 0x11) = CONCAT22(pixelPair,pixelPair);
                pixel = *(uint8_t *)((int)srcCursor + -3);
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                *(uint8_t *)((int)dstCursor + 0x15) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 0x16) = pixelPair;
                dstCursor[6] = CONCAT22(pixelPair,pixelPair);
                dstCursor = dstCursor + 7;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                *(uint8_t *)dstCursor = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                dstCursor = (uint32_t *)((int)dstCursor + 7);
              }
            }
            else {
              spanX = (spanW * -7 - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  pixelDword = CONCAT22(pixelPair,pixelPair);
                  *dstCursor = pixelDword;
                  dstCursor[1] = pixelDword;
                  errTerm = (spanW * -8 - dstCount) * 2;
                  dstCursor = dstCursor + 2;
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelPair = CONCAT11(pixel,pixel);
                  *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                  *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                  errTerm = spanX;
                  dstCursor = (uint32_t *)((int)dstCursor + 7);
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 8:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                spanW = CONCAT22(pixelPair,pixelPair);
                *dstCursor = spanW;
                dstCursor[1] = spanW;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -1),
                                 *(uint8_t *)((int)srcCursor + -1));
                spanW = CONCAT22(pixelPair,pixelPair);
                dstCursor[2] = spanW;
                dstCursor[3] = spanW;
                srcBytePtr = (uint8_t *)((int)srcCursor + -3);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -2),
                                 *(uint8_t *)((int)srcCursor + -2));
                spanW = CONCAT22(pixelPair,pixelPair);
                dstCursor[4] = spanW;
                dstCursor[5] = spanW;
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                pixelPair = CONCAT11(*srcBytePtr,*srcBytePtr);
                spanW = CONCAT22(pixelPair,pixelPair);
                dstCursor[6] = spanW;
                dstCursor[7] = spanW;
                dstCursor = dstCursor + 8;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                pixelPair = CONCAT11(pixel,pixel);
                spanW = CONCAT22(pixelPair,pixelPair);
                *dstCursor = spanW;
                dstCursor[1] = spanW;
                dstCursor = dstCursor + 2;
              }
            }
            else {
              spanX = (spanW * -8 - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                  *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 9);
                  errTerm = (spanW * -9 - dstCount) * 2;
                }
                else {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  pixelDword = CONCAT22(pixelPair,pixelPair);
                  *dstCursor = pixelDword;
                  dstCursor[1] = pixelDword;
                  dstCursor = dstCursor + 2;
                  errTerm = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 9:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + -1);
                *(uint8_t *)((int)dstCursor + 9) = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 10) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0xe) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + -2);
                *(uint8_t *)((int)dstCursor + 0x12) = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 0x13) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x17) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + -3);
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                *(uint8_t *)((int)dstCursor + 0x1b) = pixel;
                spanW = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                dstCursor[7] = spanW;
                dstCursor[8] = spanW;
                dstCursor = dstCursor + 9;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                *(uint8_t *)dstCursor = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                dstCursor = (uint32_t *)((int)dstCursor + 9);
              }
            }
            else {
              spanX = (spanW * -9 - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *(uint16_t *)dstCursor = pixelPair;
                  *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 6) = CONCAT22(pixelPair,pixelPair);
                  errTerm = (spanW * -10 - dstCount) * 2;
                  dstCursor = (uint32_t *)((int)dstCursor + 10);
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                  *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 9);
                  errTerm = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 10:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                *(uint16_t *)dstCursor = pixelPair;
                *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 6) = CONCAT22(pixelPair,pixelPair);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -1),
                                 *(uint8_t *)((int)srcCursor + -1));
                *(uint16_t *)((int)dstCursor + 10) = pixelPair;
                dstCursor[3] = CONCAT22(pixelPair,pixelPair);
                dstCursor[4] = CONCAT22(pixelPair,pixelPair);
                srcBytePtr = (uint8_t *)((int)srcCursor + -3);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -2),
                                 *(uint8_t *)((int)srcCursor + -2));
                *(uint16_t *)(dstCursor + 5) = pixelPair;
                *(uint32_t *)((int)dstCursor + 0x16) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 0x1a) = CONCAT22(pixelPair,pixelPair);
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                pixelPair = CONCAT11(*srcBytePtr,*srcBytePtr);
                *(uint16_t *)((int)dstCursor + 0x1e) = pixelPair;
                dstCursor[8] = CONCAT22(pixelPair,pixelPair);
                dstCursor[9] = CONCAT22(pixelPair,pixelPair);
                dstCursor = dstCursor + 10;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                *(uint16_t *)dstCursor = pixelPair;
                *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 6) = CONCAT22(pixelPair,pixelPair);
                dstCursor = (uint32_t *)((int)dstCursor + 10);
              }
            }
            else {
              spanX = (spanW * -10 - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelPair = CONCAT11(pixel,pixel);
                  *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                  *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 7) = CONCAT22(pixelPair,pixelPair);
                  errTerm = (spanW * -0xb - dstCount) * 2;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xb);
                }
                else {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *(uint16_t *)dstCursor = pixelPair;
                  *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 6) = CONCAT22(pixelPair,pixelPair);
                  errTerm = spanX;
                  dstCursor = (uint32_t *)((int)dstCursor + 10);
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 0xb:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 7) = CONCAT22(pixelPair,pixelPair);
                pixel = *(uint8_t *)((int)srcCursor + -1);
                *(uint8_t *)((int)dstCursor + 0xb) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)(dstCursor + 3) = pixelPair;
                *(uint32_t *)((int)dstCursor + 0xe) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 0x12) = CONCAT22(pixelPair,pixelPair);
                pixel = *(uint8_t *)((int)srcCursor + -2);
                *(uint8_t *)((int)dstCursor + 0x16) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 0x17) = pixelPair;
                *(uint32_t *)((int)dstCursor + 0x19) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 0x1d) = CONCAT22(pixelPair,pixelPair);
                pixel = *(uint8_t *)((int)srcCursor + -3);
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                *(uint8_t *)((int)dstCursor + 0x21) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 0x22) = pixelPair;
                dstCursor[9] = CONCAT22(pixelPair,pixelPair);
                dstCursor[10] = CONCAT22(pixelPair,pixelPair);
                dstCursor = dstCursor + 0xb;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                *(uint8_t *)dstCursor = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 7) = CONCAT22(pixelPair,pixelPair);
                dstCursor = (uint32_t *)((int)dstCursor + 0xb);
              }
            }
            else {
              spanX = (spanW * -0xb - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  pixelDword = CONCAT22(pixelPair,pixelPair);
                  *dstCursor = pixelDword;
                  dstCursor[1] = pixelDword;
                  dstCursor[2] = pixelDword;
                  errTerm = (spanW * -0xc - dstCount) * 2;
                  dstCursor = dstCursor + 3;
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelPair = CONCAT11(pixel,pixel);
                  *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                  *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 7) = CONCAT22(pixelPair,pixelPair);
                  errTerm = spanX;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xb);
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 0xc:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                spanW = CONCAT22(pixelPair,pixelPair);
                *dstCursor = spanW;
                dstCursor[1] = spanW;
                dstCursor[2] = spanW;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -1),
                                 *(uint8_t *)((int)srcCursor + -1));
                spanW = CONCAT22(pixelPair,pixelPair);
                dstCursor[3] = spanW;
                dstCursor[4] = spanW;
                dstCursor[5] = spanW;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -2),
                                 *(uint8_t *)((int)srcCursor + -2));
                srcBytePtr = (uint8_t *)((int)srcCursor + -3);
                spanW = CONCAT22(pixelPair,pixelPair);
                dstCursor[6] = spanW;
                dstCursor[7] = spanW;
                dstCursor[8] = spanW;
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                pixelPair = CONCAT11(*srcBytePtr,*srcBytePtr);
                spanW = CONCAT22(pixelPair,pixelPair);
                dstCursor[9] = spanW;
                dstCursor[10] = spanW;
                dstCursor[0xb] = spanW;
                dstCursor = dstCursor + 0xc;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                pixelPair = CONCAT11(pixel,pixel);
                spanW = CONCAT22(pixelPair,pixelPair);
                *dstCursor = spanW;
                dstCursor[1] = spanW;
                dstCursor[2] = spanW;
                dstCursor = dstCursor + 3;
              }
            }
            else {
              spanX = (spanW * -0xc - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                  *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 9) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xd);
                  errTerm = (spanW * -0xd - dstCount) * 2;
                }
                else {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  pixelDword = CONCAT22(pixelPair,pixelPair);
                  *dstCursor = pixelDword;
                  dstCursor[1] = pixelDword;
                  dstCursor[2] = pixelDword;
                  dstCursor = dstCursor + 3;
                  errTerm = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 0xd:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 9) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + -1);
                *(uint8_t *)((int)dstCursor + 0xd) = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 0xe) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x12) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x16) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + -2);
                *(uint8_t *)((int)dstCursor + 0x1a) = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 0x1b) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x1f) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x23) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + -3);
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                *(uint8_t *)((int)dstCursor + 0x27) = pixel;
                spanW = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                dstCursor[10] = spanW;
                dstCursor[0xb] = spanW;
                dstCursor[0xc] = spanW;
                dstCursor = dstCursor + 0xd;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                *(uint8_t *)dstCursor = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 9) = pixelDwordB;
                dstCursor = (uint32_t *)((int)dstCursor + 0xd);
              }
            }
            else {
              spanX = (spanW * -0xd - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *(uint16_t *)dstCursor = pixelPair;
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 2) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 6) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 10) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xe);
                  errTerm = (spanW * -0xe - dstCount) * 2;
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                  *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 9) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xd);
                  errTerm = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 0xe:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                *(uint16_t *)dstCursor = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 2) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 6) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 10) = pixelDwordB;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -1),
                                 *(uint8_t *)((int)srcCursor + -1));
                *(uint16_t *)((int)dstCursor + 0xe) = pixelPair;
                spanW = CONCAT22(pixelPair,pixelPair);
                dstCursor[4] = spanW;
                dstCursor[5] = spanW;
                dstCursor[6] = spanW;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -2),
                                 *(uint8_t *)((int)srcCursor + -2));
                *(uint16_t *)(dstCursor + 7) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 0x1e) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x22) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x26) = pixelDwordB;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -3),
                                 *(uint8_t *)((int)srcCursor + -3));
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                *(uint16_t *)((int)dstCursor + 0x2a) = pixelPair;
                spanW = CONCAT22(pixelPair,pixelPair);
                dstCursor[0xb] = spanW;
                dstCursor[0xc] = spanW;
                dstCursor[0xd] = spanW;
                dstCursor = dstCursor + 0xe;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                *(uint16_t *)dstCursor = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 2) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 6) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 10) = pixelDwordB;
                dstCursor = (uint32_t *)((int)dstCursor + 0xe);
              }
            }
            else {
              spanX = (spanW * -0xe - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelPair = CONCAT11(pixel,pixel);
                  *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 3) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 7) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 0xb) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xf);
                  errTerm = (spanW * -0xf - dstCount) * 2;
                }
                else {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *(uint16_t *)dstCursor = pixelPair;
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 2) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 6) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 10) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xe);
                  errTerm = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 0xf:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 3) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 7) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0xb) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + -1);
                *(uint8_t *)((int)dstCursor + 0xf) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)(dstCursor + 4) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 0x12) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x16) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x1a) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + -2);
                *(uint8_t *)((int)dstCursor + 0x1e) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 0x1f) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 0x21) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x25) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x29) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + -3);
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                *(uint8_t *)((int)dstCursor + 0x2d) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 0x2e) = pixelPair;
                spanW = CONCAT22(pixelPair,pixelPair);
                dstCursor[0xc] = spanW;
                dstCursor[0xd] = spanW;
                dstCursor[0xe] = spanW;
                dstCursor = dstCursor + 0xf;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                *(uint8_t *)dstCursor = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 3) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 7) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0xb) = pixelDwordB;
                dstCursor = (uint32_t *)((int)dstCursor + 0xf);
              }
            }
            else {
              spanX = (spanW * -0xf - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  pixelDword = CONCAT22(pixelPair,pixelPair);
                  *dstCursor = pixelDword;
                  dstCursor[1] = pixelDword;
                  dstCursor[2] = pixelDword;
                  dstCursor[3] = pixelDword;
                  dstCursor = dstCursor + 4;
                  errTerm = (spanW * -0x10 - dstCount) * 2;
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelPair = CONCAT11(pixel,pixel);
                  *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 3) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 7) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 0xb) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xf);
                  errTerm = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                quadCount = quadCount + errTerm;
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          case 0x10:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                spanW = CONCAT22(pixelPair,pixelPair);
                *dstCursor = spanW;
                dstCursor[1] = spanW;
                dstCursor[2] = spanW;
                dstCursor[3] = spanW;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -1),
                                 *(uint8_t *)((int)srcCursor + -1));
                spanW = CONCAT22(pixelPair,pixelPair);
                dstCursor[4] = spanW;
                dstCursor[5] = spanW;
                dstCursor[6] = spanW;
                dstCursor[7] = spanW;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -2),
                                 *(uint8_t *)((int)srcCursor + -2));
                spanW = CONCAT22(pixelPair,pixelPair);
                dstCursor[8] = spanW;
                dstCursor[9] = spanW;
                dstCursor[10] = spanW;
                dstCursor[0xb] = spanW;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + -3),
                                 *(uint8_t *)((int)srcCursor + -3));
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                spanW = CONCAT22(pixelPair,pixelPair);
                dstCursor[0xc] = spanW;
                dstCursor[0xd] = spanW;
                dstCursor[0xe] = spanW;
                dstCursor[0xf] = spanW;
                dstCursor = dstCursor + 0x10;
              }
              for (srcWidth = srcWidth & 3; srcWidth != 0; srcWidth = srcWidth - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                pixelPair = CONCAT11(pixel,pixel);
                spanW = CONCAT22(pixelPair,pixelPair);
                *dstCursor = spanW;
                dstCursor[1] = spanW;
                dstCursor[2] = spanW;
                dstCursor[3] = spanW;
                dstCursor = dstCursor + 4;
              }
            }
            else {
LAB_0042e043:
              spanX = (spanW * -0x10 - dstCount) * 2;
              quadCount = spanX;
              do {
                dstCount = 0;
                for (; quadCount < 0; quadCount = quadCount + spanW * -2) {
                  dstCount = dstCount + 1;
                }
                quadCount = quadCount + spanX;
                pixel = (uint8_t)*srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                pixelPair = CONCAT11(pixel,pixel);
                pixelDword = CONCAT22(pixelPair,pixelPair);
                *dstCursor = pixelDword;
                dstCursor[1] = pixelDword;
                dstCursor[2] = pixelDword;
                dstCursor[3] = pixelDword;
                srcPtr = dstCursor + 4;
                if ((dstCount & 1) != 0) {
                  *(uint8_t *)(dstCursor + 4) = pixel;
                  srcPtr = (uint32_t *)((int)dstCursor + 0x11);
                }
                dstCursor = srcPtr;
                if (((int)dstCount >> 1 & 1U) != 0) {
                  *(uint16_t *)dstCursor = pixelPair;
                  dstCursor = (uint32_t *)((int)dstCursor + 2);
                }
                for (errTerm = (int)dstCount >> 2; errTerm != 0; errTerm = errTerm + -1) {
                  *dstCursor = pixelDword;
                  dstCursor = dstCursor + 1;
                }
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
            break;
          default:
            if (spanX != 0) goto LAB_0042e043;
            spanW = (int)dstCount / (int)srcWidth - 0x10;
            spanX = (int)spanW >> 2;
            if (srcQuads != 0) {
              dstCount = spanW & 1;
              do {
                pixel = (uint8_t)*srcCursor;
                pixelPair = CONCAT11(pixel,pixel);
                pixelDword = CONCAT22(pixelPair,pixelPair);
                *dstCursor = pixelDword;
                dstCursor[1] = pixelDword;
                dstCursor[2] = pixelDword;
                dstCursor[3] = pixelDword;
                srcPtr = dstCursor + 4;
                if (dstCount != 0) {
                  *(uint8_t *)(dstCursor + 4) = pixel;
                  srcPtr = (uint32_t *)((int)dstCursor + 0x11);
                }
                dstCursor = srcPtr;
                extraWordFlag = (int)spanW >> 1 & 1;
                srcPtr = dstCursor;
                quadCount = spanX;
                if (extraWordFlag != 0) {
                  *(uint16_t *)dstCursor = pixelPair;
                  srcPtr = (uint32_t *)((int)dstCursor + 2);
                }
                for (; quadCount != 0; quadCount = quadCount + -1) {
                  *srcPtr = pixelDword;
                  srcPtr = srcPtr + 1;
                }
                pixel = *(uint8_t *)((int)srcCursor + -1);
                pixelPair = CONCAT11(pixel,pixel);
                pixelDword = CONCAT22(pixelPair,pixelPair);
                dstCursor = srcPtr + 4;
                *srcPtr = pixelDword;
                srcPtr[1] = pixelDword;
                srcPtr[2] = pixelDword;
                srcPtr[3] = pixelDword;
                if (dstCount != 0) {
                  *(uint8_t *)dstCursor = pixel;
                  dstCursor = (uint32_t *)((int)srcPtr + 0x11);
                }
                srcPtr = dstCursor;
                quadCount = spanX;
                if (extraWordFlag != 0) {
                  *(uint16_t *)dstCursor = pixelPair;
                  srcPtr = (uint32_t *)((int)dstCursor + 2);
                }
                for (; quadCount != 0; quadCount = quadCount + -1) {
                  *srcPtr = pixelDword;
                  srcPtr = srcPtr + 1;
                }
                pixel = *(uint8_t *)((int)srcCursor + -2);
                pixelPair = CONCAT11(pixel,pixel);
                pixelDword = CONCAT22(pixelPair,pixelPair);
                dstCursor = srcPtr + 4;
                *srcPtr = pixelDword;
                srcPtr[1] = pixelDword;
                srcPtr[2] = pixelDword;
                srcPtr[3] = pixelDword;
                if (dstCount != 0) {
                  *(uint8_t *)dstCursor = pixel;
                  dstCursor = (uint32_t *)((int)srcPtr + 0x11);
                }
                srcPtr = dstCursor;
                quadCount = spanX;
                if (extraWordFlag != 0) {
                  *(uint16_t *)dstCursor = pixelPair;
                  srcPtr = (uint32_t *)((int)dstCursor + 2);
                }
                for (; quadCount != 0; quadCount = quadCount + -1) {
                  *srcPtr = pixelDword;
                  srcPtr = srcPtr + 1;
                }
                pixel = *(uint8_t *)((int)srcCursor + -3);
                srcCursor = (uint32_t *)((int)srcCursor + -4);
                pixelPair = CONCAT11(pixel,pixel);
                pixelDword = CONCAT22(pixelPair,pixelPair);
                dstCursor = srcPtr + 4;
                *srcPtr = pixelDword;
                srcPtr[1] = pixelDword;
                srcPtr[2] = pixelDword;
                srcPtr[3] = pixelDword;
                if (dstCount != 0) {
                  *(uint8_t *)dstCursor = pixel;
                  dstCursor = (uint32_t *)((int)srcPtr + 0x11);
                }
                quadCount = spanX;
                if (extraWordFlag != 0) {
                  *(uint16_t *)dstCursor = pixelPair;
                  dstCursor = (uint32_t *)((int)dstCursor + 2);
                }
                for (; quadCount != 0; quadCount = quadCount + -1) {
                  *dstCursor = pixelDword;
                  dstCursor = dstCursor + 1;
                }
                srcQuads = srcQuads + -1;
              } while (srcQuads != 0);
            }
            srcWidth = srcWidth & 3;
            if (srcWidth != 0) {
              do {
                pixel = (uint8_t)*srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + -1);
                pixelPair = CONCAT11(pixel,pixel);
                dstCount = CONCAT22(pixelPair,pixelPair);
                *dstCursor = dstCount;
                dstCursor[1] = dstCount;
                dstCursor[2] = dstCount;
                dstCursor[3] = dstCount;
                srcPtr = dstCursor + 4;
                if ((spanW & 1) != 0) {
                  *(uint8_t *)(dstCursor + 4) = pixel;
                  srcPtr = (uint32_t *)((int)dstCursor + 0x11);
                }
                dstCursor = srcPtr;
                quadCount = spanX;
                if (((int)spanW >> 1 & 1U) != 0) {
                  *(uint16_t *)dstCursor = pixelPair;
                  dstCursor = (uint32_t *)((int)dstCursor + 2);
                }
                for (; quadCount != 0; quadCount = quadCount + -1) {
                  *dstCursor = dstCount;
                  dstCursor = dstCursor + 1;
                }
                srcWidth = srcWidth - 1;
              } while (srcWidth != 0);
            }
          }
        }
      }
      else {
        srcCursor = (uint32_t *)(gTrigPixelRowBase + spanX);
        dstCursor = (uint32_t *)(dstBase + *pDstOff);
        if (dstCount == spanW) {
          for (; quadCount != 0; quadCount = quadCount + -1) {
            pixelDwordB = *srcCursor;
            srcCursor = srcCursor + 1;
            *dstCursor = pixelDwordB;
            dstCursor = dstCursor + 1;
          }
          for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
            *(uint8_t *)dstCursor = *(uint8_t *)srcCursor;
            srcCursor = (uint32_t *)((int)srcCursor + 1);
            dstCursor = (uint32_t *)((int)dstCursor + 1);
          }
        }
        else if ((int)dstCount < (int)spanW) {
          spanX = (int)spanW / (int)dstCount;
          errTerm = (int)spanW % (int)dstCount;
          switch(spanX) {
          case 1:
            spanX = (dstCount * 2 - spanW) * 2;
            quadCount = dstCount - spanW;
            errTerm = spanX >> 1;
            do {
              if (errTerm < 0) {
                *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                srcCursor = (uint32_t *)((int)srcCursor + 2);
                errStep = spanX;
              }
              else {
                *(char *)dstCursor = (char)*srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errStep = quadCount * 2;
              }
              dstCursor = (uint32_t *)((int)dstCursor + 1);
              errTerm = errTerm + errStep;
              dstCount = dstCount - 1;
            } while (dstCount != 0);
            break;
          case 2:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcCursor + 1);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[2];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[4];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[6];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + 8;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + 2;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcCursor + 1);
              spanX = (dstCount * 3 - spanW) * 2;
              quadCount = dstCount * 2;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 3);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 2);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 3:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcCursor + 1);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[3];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[6];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[9];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + 0xc;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + 3;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcCursor + 1);
              spanX = (dstCount * 4 - spanW) * 2;
              quadCount = dstCount * 3;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 4);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 3);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 4:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcCursor + 2);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[4];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[8];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[0xc];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + 0x10;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + 4;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcCursor + 2);
              spanX = (dstCount * 5 - spanW) * 2;
              quadCount = dstCount * 4;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 5);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 4);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 5:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcCursor + 2);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[5];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[10];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[0xf];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + 0x14;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + 5;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcCursor + 2);
              spanX = (dstCount * 6 - spanW) * 2;
              quadCount = dstCount * 5;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 6);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 5);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 6:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcCursor + 3);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[6];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[0xc];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[0x12];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + 0x18;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + 6;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcCursor + 3);
              spanX = (dstCount * 7 - spanW) * 2;
              quadCount = dstCount * 6;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 7);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 6);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 7:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcCursor + 3);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[7];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[0xe];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[0x15];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + 0x1c;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + 7;
              }
            }
            else {
              spanX = (dstCount * 8 - spanW) * 2;
              quadCount = dstCount * 7;
              srcCursor = (uint32_t *)((int)srcCursor + 3);
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 8);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 7);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 8:
            if (errTerm == 0) {
              srcWalk = srcCursor + 1;
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *(uint8_t *)srcWalk;
                *(uint8_t *)((int)dstCursor + 1) = *(uint8_t *)(srcWalk + 2);
                *(uint8_t *)((int)dstCursor + 2) = *(uint8_t *)(srcWalk + 4);
                *(uint8_t *)((int)dstCursor + 3) = *(uint8_t *)(srcWalk + 6);
                dstCursor = dstCursor + 1;
                srcWalk = srcWalk + 8;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *(uint8_t *)srcWalk;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcWalk = srcWalk + 2;
              }
            }
            else {
              srcCursor = srcCursor + 1;
              spanX = (dstCount * 9 - spanW) * 2;
              quadCount = dstCount * 8;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 9);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = srcCursor + 2;
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 9:
            if (errTerm == 0) {
              srcWalk = srcCursor + 1;
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *(uint8_t *)srcWalk;
                *(uint8_t *)((int)dstCursor + 1) = *(uint8_t *)((int)srcWalk + 9);
                *(uint8_t *)((int)dstCursor + 2) = *(uint8_t *)((int)srcWalk + 0x12);
                *(uint8_t *)((int)dstCursor + 3) = *(uint8_t *)((int)srcWalk + 0x1b);
                dstCursor = dstCursor + 1;
                srcWalk = srcWalk + 9;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *(uint8_t *)srcWalk;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcWalk = (uint32_t *)((int)srcWalk + 9);
              }
            }
            else {
              srcCursor = srcCursor + 1;
              spanX = (dstCount * 10 - spanW) * 2;
              quadCount = dstCount * 9;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 10);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 9);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 10:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcCursor + 5);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[10];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[0x14];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[0x1e];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + 0x28;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + 10;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcCursor + 5);
              spanX = (dstCount * 0xb - spanW) * 2;
              quadCount = dstCount * 10;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 0xb);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 10);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 0xb:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcCursor + 5);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[0xb];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[0x16];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[0x21];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + 0x2c;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + 0xb;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcCursor + 5);
              quadCount = dstCount * 0xb;
              spanX = (dstCount * 0xc - spanW) * 2;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 0xc);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 0xb);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 0xc:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcCursor + 6);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[0xc];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[0x18];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[0x24];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + 0x30;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + 0xc;
              }
            }
            else {
              srcCursor = (uint32_t *)((int)srcCursor + 6);
              quadCount = dstCount * 0xc;
              spanX = (dstCount * 0xd - spanW) * 2;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 0xd);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 0xc);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 0xd:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcCursor + 6);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[0xd];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[0x1a];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[0x27];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + 0x34;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + 0xd;
              }
            }
            else {
              spanX = (dstCount * 0xe - spanW) * 2;
              srcCursor = (uint32_t *)((int)srcCursor + 6);
              quadCount = dstCount * 0xd;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 0xe);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 0xd);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 0xe:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcCursor + 7);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[0xe];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[0x1c];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[0x2a];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + 0x38;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + 0xe;
              }
            }
            else {
              spanX = (dstCount * 0xf - spanW) * 2;
              srcCursor = (uint32_t *)((int)srcCursor + 7);
              quadCount = dstCount * 0xe;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 0xf);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 0xe);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 0xf:
            if (errTerm == 0) {
              srcBytePtr = (uint8_t *)((int)srcCursor + 7);
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                *(uint8_t *)((int)dstCursor + 1) = srcBytePtr[0xf];
                *(uint8_t *)((int)dstCursor + 2) = srcBytePtr[0x1e];
                *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[0x2d];
                dstCursor = dstCursor + 1;
                srcBytePtr = srcBytePtr + 0x3c;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *srcBytePtr;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcBytePtr = srcBytePtr + 0xf;
              }
            }
            else {
              spanX = (dstCount * 0x10 - spanW) * 2;
              srcCursor = (uint32_t *)((int)srcCursor + 7);
              quadCount = dstCount * 0xf;
              errTerm = spanX >> 1;
              do {
                if (errTerm < 0) {
                  *(uint8_t *)dstCursor = *(uint8_t *)((int)srcCursor + 1);
                  srcCursor = (uint32_t *)((int)srcCursor + 0x10);
                  errStep = spanX;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  srcCursor = (uint32_t *)((int)srcCursor + 0xf);
                  errStep = (quadCount - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errTerm = errTerm + errStep;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          case 0x10:
            if (errTerm == 0) {
              srcWalk = srcCursor + 2;
              for (; quadCount != 0; quadCount = quadCount + -1) {
                *(uint8_t *)dstCursor = *(uint8_t *)srcWalk;
                *(uint8_t *)((int)dstCursor + 1) = *(uint8_t *)(srcWalk + 4);
                *(uint8_t *)((int)dstCursor + 2) = *(uint8_t *)(srcWalk + 8);
                *(uint8_t *)((int)dstCursor + 3) = *(uint8_t *)(srcWalk + 0xc);
                dstCursor = dstCursor + 1;
                srcWalk = srcWalk + 0x10;
              }
              for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
                *(uint8_t *)dstCursor = *(uint8_t *)srcWalk;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcWalk = srcWalk + 4;
              }
            }
            else {
LAB_0042f13d:
              quadCount = ((spanX + 1) * dstCount - spanW) * 2;
              errTerm = dstCount * spanX;
              srcCursor = (uint32_t *)((int)srcCursor + (spanX >> 1));
              errStep = quadCount >> 1;
              do {
                if (errStep < 0) {
                  srcCursor = (uint32_t *)((int)srcCursor + 1);
                  *(uint8_t *)dstCursor = *(uint8_t *)srcCursor;
                  errStepAlt = quadCount;
                }
                else {
                  *(char *)dstCursor = (char)*srcCursor;
                  errStepAlt = (errTerm - spanW) * 2;
                }
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                srcCursor = (uint32_t *)((int)srcCursor + spanX);
                errStep = errStep + errStepAlt;
                dstCount = dstCount - 1;
              } while (dstCount != 0);
            }
            break;
          default:
            if (errTerm != 0) goto LAB_0042f13d;
            srcBytePtr = (uint8_t *)((int)srcCursor + (spanX >> 1));
            for (; quadCount != 0; quadCount = quadCount + -1) {
              *(uint8_t *)dstCursor = *srcBytePtr;
              srcBytePtr = srcBytePtr + spanX;
              *(uint8_t *)((int)dstCursor + 1) = *srcBytePtr;
              srcBytePtr = srcBytePtr + spanX;
              *(uint8_t *)((int)dstCursor + 2) = *srcBytePtr;
              *(uint8_t *)((int)dstCursor + 3) = srcBytePtr[spanX];
              dstCursor = dstCursor + 1;
              srcBytePtr = srcBytePtr + spanX + spanX;
            }
            for (dstCount = dstCount & 3; dstCount != 0; dstCount = dstCount - 1) {
              *(uint8_t *)dstCursor = *srcBytePtr;
              dstCursor = (uint32_t *)((int)dstCursor + 1);
              srcBytePtr = srcBytePtr + spanX;
            }
          }
        }
        else {
          spanX = (int)dstCount % (int)spanW;
          srcQuads = (int)spanW >> 2;
          switch((int)dstCount / (int)spanW) {
          case 1:
            spanX = spanW * 2;
            quadCount = (spanW - dstCount) * 2;
            errTerm = quadCount;
            do {
              if (errTerm < 0) {
                *(uint16_t *)dstCursor = CONCAT11((char)*srcCursor,(char)*srcCursor);
                dstCursor = (uint32_t *)((int)dstCursor + 2);
                errStep = (spanX - dstCount) * 2;
              }
              else {
                *(char *)dstCursor = (char)*srcCursor;
                dstCursor = (uint32_t *)((int)dstCursor + 1);
                errStep = quadCount;
              }
              srcCursor = (uint32_t *)((int)srcCursor + 1);
              errTerm = errTerm + errStep;
              spanW = spanW - 1;
            } while (spanW != 0);
            break;
          case 2:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                *(uint16_t *)dstCursor = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                *(uint16_t *)((int)dstCursor + 2) =
                     CONCAT11(*(uint8_t *)((int)srcCursor + 1),*(uint8_t *)((int)srcCursor + 1));
                srcBytePtr = (uint8_t *)((int)srcCursor + 3);
                *(uint16_t *)(dstCursor + 1) =
                     CONCAT11(*(uint8_t *)((int)srcCursor + 2),*(uint8_t *)((int)srcCursor + 2));
                srcCursor = srcCursor + 1;
                *(uint16_t *)((int)dstCursor + 6) = CONCAT11(*srcBytePtr,*srcBytePtr);
                dstCursor = dstCursor + 2;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                *(uint16_t *)dstCursor = CONCAT11(pixel,pixel);
                dstCursor = (uint32_t *)((int)dstCursor + 2);
              }
            }
            else {
              quadCount = spanW * 3;
              spanX = (spanW * 2 - dstCount) * 2;
              errTerm = spanX;
              do {
                if (errTerm < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  *(uint16_t *)((int)dstCursor + 1) = CONCAT11(pixel,pixel);
                  dstCursor = (uint32_t *)((int)dstCursor + 3);
                  errStep = (quadCount - dstCount) * 2;
                }
                else {
                  *(uint16_t *)dstCursor = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  dstCursor = (uint32_t *)((int)dstCursor + 2);
                  errStep = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errTerm = errTerm + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 3:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                *(uint16_t *)((int)dstCursor + 1) = CONCAT11(pixel,pixel);
                pixel = *(uint8_t *)((int)srcCursor + 1);
                *(uint8_t *)((int)dstCursor + 3) = pixel;
                *(uint16_t *)(dstCursor + 1) = CONCAT11(pixel,pixel);
                pixel = *(uint8_t *)((int)srcCursor + 2);
                *(uint8_t *)((int)dstCursor + 6) = pixel;
                *(uint16_t *)((int)dstCursor + 7) = CONCAT11(pixel,pixel);
                pixel = *(uint8_t *)((int)srcCursor + 3);
                srcCursor = srcCursor + 1;
                *(uint8_t *)((int)dstCursor + 9) = pixel;
                *(uint16_t *)((int)dstCursor + 10) = CONCAT11(pixel,pixel);
                dstCursor = dstCursor + 3;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                *(uint8_t *)dstCursor = pixel;
                *(uint16_t *)((int)dstCursor + 1) = CONCAT11(pixel,pixel);
                dstCursor = (uint32_t *)((int)dstCursor + 3);
              }
            }
            else {
              spanX = spanW * 4;
              quadCount = (spanW * 3 - dstCount) * 2;
              errTerm = quadCount;
              do {
                if (errTerm < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *dstCursor = CONCAT22(pixelPair,pixelPair);
                  dstCursor = dstCursor + 1;
                  errStep = (spanX - dstCount) * 2;
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  *(uint16_t *)((int)dstCursor + 1) = CONCAT11(pixel,pixel);
                  dstCursor = (uint32_t *)((int)dstCursor + 3);
                  errStep = quadCount;
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errTerm = errTerm + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 4:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                *dstCursor = CONCAT22(pixelPair,pixelPair);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 1),*(uint8_t *)((int)srcCursor + 1)
                                );
                dstCursor[1] = CONCAT22(pixelPair,pixelPair);
                srcBytePtr = (uint8_t *)((int)srcCursor + 3);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 2),*(uint8_t *)((int)srcCursor + 2)
                                );
                dstCursor[2] = CONCAT22(pixelPair,pixelPair);
                srcCursor = srcCursor + 1;
                pixelPair = CONCAT11(*srcBytePtr,*srcBytePtr);
                dstCursor[3] = CONCAT22(pixelPair,pixelPair);
                dstCursor = dstCursor + 4;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                pixelPair = CONCAT11(pixel,pixel);
                *dstCursor = CONCAT22(pixelPair,pixelPair);
                dstCursor = dstCursor + 1;
              }
            }
            else {
              quadCount = spanW * 5;
              spanX = (spanW * 4 - dstCount) * 2;
              errTerm = spanX;
              do {
                if (errTerm < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  *(uint32_t *)((int)dstCursor + 1) =
                       CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                  dstCursor = (uint32_t *)((int)dstCursor + 5);
                  errStep = (quadCount - dstCount) * 2;
                }
                else {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *dstCursor = CONCAT22(pixelPair,pixelPair);
                  dstCursor = dstCursor + 1;
                  errStep = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errTerm = errTerm + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 5:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                *(uint32_t *)((int)dstCursor + 1) = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                pixel = *(uint8_t *)((int)srcCursor + 1);
                *(uint8_t *)((int)dstCursor + 5) = pixel;
                *(uint32_t *)((int)dstCursor + 6) = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                pixel = *(uint8_t *)((int)srcCursor + 2);
                *(uint8_t *)((int)dstCursor + 10) = pixel;
                *(uint32_t *)((int)dstCursor + 0xb) =
                     CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                pixel = *(uint8_t *)((int)srcCursor + 3);
                srcCursor = srcCursor + 1;
                *(uint8_t *)((int)dstCursor + 0xf) = pixel;
                dstCursor[4] = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                dstCursor = dstCursor + 5;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                *(uint8_t *)dstCursor = pixel;
                *(uint32_t *)((int)dstCursor + 1) = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                dstCursor = (uint32_t *)((int)dstCursor + 5);
              }
            }
            else {
              quadCount = spanW * 6;
              spanX = (spanW * 5 - dstCount) * 2;
              errTerm = spanX;
              do {
                if (errTerm < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *(uint16_t *)dstCursor = pixelPair;
                  *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                  errStep = (quadCount - dstCount) * 2;
                  dstCursor = (uint32_t *)((int)dstCursor + 6);
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  *(uint32_t *)((int)dstCursor + 1) =
                       CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                  dstCursor = (uint32_t *)((int)dstCursor + 5);
                  errStep = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errTerm = errTerm + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 6:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                *(uint16_t *)dstCursor = pixelPair;
                *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 1),*(uint8_t *)((int)srcCursor + 1)
                                );
                *(uint16_t *)((int)dstCursor + 6) = pixelPair;
                dstCursor[2] = CONCAT22(pixelPair,pixelPair);
                srcBytePtr = (uint8_t *)((int)srcCursor + 3);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 2),*(uint8_t *)((int)srcCursor + 2)
                                );
                *(uint16_t *)(dstCursor + 3) = pixelPair;
                *(uint32_t *)((int)dstCursor + 0xe) = CONCAT22(pixelPair,pixelPair);
                srcCursor = srcCursor + 1;
                pixelPair = CONCAT11(*srcBytePtr,*srcBytePtr);
                *(uint16_t *)((int)dstCursor + 0x12) = pixelPair;
                dstCursor[5] = CONCAT22(pixelPair,pixelPair);
                dstCursor = dstCursor + 6;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                *(uint16_t *)dstCursor = pixelPair;
                *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                dstCursor = (uint32_t *)((int)dstCursor + 6);
              }
            }
            else {
              quadCount = spanW * 7;
              spanX = (spanW * 6 - dstCount) * 2;
              errTerm = spanX;
              do {
                if (errTerm < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelPair = CONCAT11(pixel,pixel);
                  *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                  *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                  errStep = (quadCount - dstCount) * 2;
                  dstCursor = (uint32_t *)((int)dstCursor + 7);
                }
                else {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *(uint16_t *)dstCursor = pixelPair;
                  *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                  errStep = spanX;
                  dstCursor = (uint32_t *)((int)dstCursor + 6);
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errTerm = errTerm + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 7:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                pixel = *(uint8_t *)((int)srcCursor + 1);
                *(uint8_t *)((int)dstCursor + 7) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)(dstCursor + 2) = pixelPair;
                *(uint32_t *)((int)dstCursor + 10) = CONCAT22(pixelPair,pixelPair);
                pixel = *(uint8_t *)((int)srcCursor + 2);
                *(uint8_t *)((int)dstCursor + 0xe) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 0xf) = pixelPair;
                *(uint32_t *)((int)dstCursor + 0x11) = CONCAT22(pixelPair,pixelPair);
                pixel = *(uint8_t *)((int)srcCursor + 3);
                srcCursor = srcCursor + 1;
                *(uint8_t *)((int)dstCursor + 0x15) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 0x16) = pixelPair;
                dstCursor[6] = CONCAT22(pixelPair,pixelPair);
                dstCursor = dstCursor + 7;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                *(uint8_t *)dstCursor = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                dstCursor = (uint32_t *)((int)dstCursor + 7);
              }
            }
            else {
              spanX = spanW * 8;
              quadCount = (spanW * 7 - dstCount) * 2;
              errTerm = quadCount;
              do {
                if (errTerm < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *dstCursor = pixelDwordB;
                  dstCursor[1] = pixelDwordB;
                  errStep = (spanX - dstCount) * 2;
                  dstCursor = dstCursor + 2;
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelPair = CONCAT11(pixel,pixel);
                  *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                  *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                  errStep = quadCount;
                  dstCursor = (uint32_t *)((int)dstCursor + 7);
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errTerm = errTerm + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 8:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *dstCursor = pixelDwordB;
                dstCursor[1] = pixelDwordB;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 1),*(uint8_t *)((int)srcCursor + 1)
                                );
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor[2] = pixelDwordB;
                dstCursor[3] = pixelDwordB;
                srcBytePtr = (uint8_t *)((int)srcCursor + 3);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 2),*(uint8_t *)((int)srcCursor + 2)
                                );
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor[4] = pixelDwordB;
                dstCursor[5] = pixelDwordB;
                srcCursor = srcCursor + 1;
                pixelPair = CONCAT11(*srcBytePtr,*srcBytePtr);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor[6] = pixelDwordB;
                dstCursor[7] = pixelDwordB;
                dstCursor = dstCursor + 8;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                pixelPair = CONCAT11(pixel,pixel);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *dstCursor = pixelDwordB;
                dstCursor[1] = pixelDwordB;
                dstCursor = dstCursor + 2;
              }
            }
            else {
              quadCount = spanW * 9;
              spanX = (spanW * 8 - dstCount) * 2;
              errTerm = spanX;
              do {
                if (errTerm < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                  *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 9);
                  errStep = (quadCount - dstCount) * 2;
                }
                else {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *dstCursor = pixelDwordB;
                  dstCursor[1] = pixelDwordB;
                  dstCursor = dstCursor + 2;
                  errStep = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errTerm = errTerm + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 9:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + 1);
                *(uint8_t *)((int)dstCursor + 9) = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 10) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0xe) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + 2);
                *(uint8_t *)((int)dstCursor + 0x12) = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 0x13) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x17) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + 3);
                srcCursor = srcCursor + 1;
                *(uint8_t *)((int)dstCursor + 0x1b) = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                dstCursor[7] = pixelDwordB;
                dstCursor[8] = pixelDwordB;
                dstCursor = dstCursor + 9;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                *(uint8_t *)dstCursor = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                dstCursor = (uint32_t *)((int)dstCursor + 9);
              }
            }
            else {
              quadCount = spanW * 10;
              spanX = (spanW * 9 - dstCount) * 2;
              errTerm = spanX;
              do {
                if (errTerm < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *(uint16_t *)dstCursor = pixelPair;
                  *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 6) = CONCAT22(pixelPair,pixelPair);
                  errStep = (quadCount - dstCount) * 2;
                  dstCursor = (uint32_t *)((int)dstCursor + 10);
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                  *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 9);
                  errStep = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errTerm = errTerm + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 10:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                *(uint16_t *)dstCursor = pixelPair;
                *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 6) = CONCAT22(pixelPair,pixelPair);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 1),*(uint8_t *)((int)srcCursor + 1)
                                );
                *(uint16_t *)((int)dstCursor + 10) = pixelPair;
                dstCursor[3] = CONCAT22(pixelPair,pixelPair);
                dstCursor[4] = CONCAT22(pixelPair,pixelPair);
                srcBytePtr = (uint8_t *)((int)srcCursor + 3);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 2),*(uint8_t *)((int)srcCursor + 2)
                                );
                *(uint16_t *)(dstCursor + 5) = pixelPair;
                *(uint32_t *)((int)dstCursor + 0x16) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 0x1a) = CONCAT22(pixelPair,pixelPair);
                srcCursor = srcCursor + 1;
                pixelPair = CONCAT11(*srcBytePtr,*srcBytePtr);
                *(uint16_t *)((int)dstCursor + 0x1e) = pixelPair;
                dstCursor[8] = CONCAT22(pixelPair,pixelPair);
                dstCursor[9] = CONCAT22(pixelPair,pixelPair);
                dstCursor = dstCursor + 10;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                *(uint16_t *)dstCursor = pixelPair;
                *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 6) = CONCAT22(pixelPair,pixelPair);
                dstCursor = (uint32_t *)((int)dstCursor + 10);
              }
            }
            else {
              quadCount = spanW * 0xb;
              spanX = (spanW * 10 - dstCount) * 2;
              errTerm = spanX;
              do {
                if (errTerm < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelPair = CONCAT11(pixel,pixel);
                  *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                  *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 7) = CONCAT22(pixelPair,pixelPair);
                  errStep = (quadCount - dstCount) * 2;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xb);
                }
                else {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *(uint16_t *)dstCursor = pixelPair;
                  *(uint32_t *)((int)dstCursor + 2) = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 6) = CONCAT22(pixelPair,pixelPair);
                  errStep = spanX;
                  dstCursor = (uint32_t *)((int)dstCursor + 10);
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errTerm = errTerm + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 0xb:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 7) = CONCAT22(pixelPair,pixelPair);
                pixel = *(uint8_t *)((int)srcCursor + 1);
                *(uint8_t *)((int)dstCursor + 0xb) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)(dstCursor + 3) = pixelPair;
                *(uint32_t *)((int)dstCursor + 0xe) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 0x12) = CONCAT22(pixelPair,pixelPair);
                pixel = *(uint8_t *)((int)srcCursor + 2);
                *(uint8_t *)((int)dstCursor + 0x16) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 0x17) = pixelPair;
                *(uint32_t *)((int)dstCursor + 0x19) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 0x1d) = CONCAT22(pixelPair,pixelPair);
                pixel = *(uint8_t *)((int)srcCursor + 3);
                srcCursor = srcCursor + 1;
                *(uint8_t *)((int)dstCursor + 0x21) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 0x22) = pixelPair;
                dstCursor[9] = CONCAT22(pixelPair,pixelPair);
                dstCursor[10] = CONCAT22(pixelPair,pixelPair);
                dstCursor = dstCursor + 0xb;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                *(uint8_t *)dstCursor = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 7) = CONCAT22(pixelPair,pixelPair);
                dstCursor = (uint32_t *)((int)dstCursor + 0xb);
              }
            }
            else {
              errTerm = spanW * 0xc;
              spanX = (spanW * 0xb - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *dstCursor = pixelDwordB;
                  dstCursor[1] = pixelDwordB;
                  dstCursor[2] = pixelDwordB;
                  errStep = (errTerm - dstCount) * 2;
                  dstCursor = dstCursor + 3;
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelPair = CONCAT11(pixel,pixel);
                  *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                  *(uint32_t *)((int)dstCursor + 3) = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 7) = CONCAT22(pixelPair,pixelPair);
                  errStep = spanX;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xb);
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                quadCount = quadCount + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 0xc:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *dstCursor = pixelDwordB;
                dstCursor[1] = pixelDwordB;
                dstCursor[2] = pixelDwordB;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 1),*(uint8_t *)((int)srcCursor + 1)
                                );
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor[3] = pixelDwordB;
                dstCursor[4] = pixelDwordB;
                dstCursor[5] = pixelDwordB;
                srcBytePtr = (uint8_t *)((int)srcCursor + 3);
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 2),*(uint8_t *)((int)srcCursor + 2)
                                );
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor[6] = pixelDwordB;
                dstCursor[7] = pixelDwordB;
                dstCursor[8] = pixelDwordB;
                srcCursor = srcCursor + 1;
                pixelPair = CONCAT11(*srcBytePtr,*srcBytePtr);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor[9] = pixelDwordB;
                dstCursor[10] = pixelDwordB;
                dstCursor[0xb] = pixelDwordB;
                dstCursor = dstCursor + 0xc;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                pixelPair = CONCAT11(pixel,pixel);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *dstCursor = pixelDwordB;
                dstCursor[1] = pixelDwordB;
                dstCursor[2] = pixelDwordB;
                dstCursor = dstCursor + 3;
              }
            }
            else {
              quadCount = spanW * 0xd;
              spanX = (spanW * 0xc - dstCount) * 2;
              errTerm = spanX;
              do {
                if (errTerm < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                  *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 9) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xd);
                  errStep = (quadCount - dstCount) * 2;
                }
                else {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *dstCursor = pixelDwordB;
                  dstCursor[1] = pixelDwordB;
                  dstCursor[2] = pixelDwordB;
                  dstCursor = dstCursor + 3;
                  errStep = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errTerm = errTerm + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 0xd:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 9) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + 1);
                *(uint8_t *)((int)dstCursor + 0xd) = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 0xe) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x12) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x16) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + 2);
                *(uint8_t *)((int)dstCursor + 0x1a) = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 0x1b) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x1f) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x23) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + 3);
                srcCursor = srcCursor + 1;
                *(uint8_t *)((int)dstCursor + 0x27) = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                dstCursor[10] = pixelDwordB;
                dstCursor[0xb] = pixelDwordB;
                dstCursor[0xc] = pixelDwordB;
                dstCursor = dstCursor + 0xd;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                *(uint8_t *)dstCursor = pixel;
                pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 9) = pixelDwordB;
                dstCursor = (uint32_t *)((int)dstCursor + 0xd);
              }
            }
            else {
              quadCount = spanW * 0xe;
              spanX = (spanW * 0xd - dstCount) * 2;
              errTerm = spanX;
              do {
                if (errTerm < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *(uint16_t *)dstCursor = pixelPair;
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 2) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 6) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 10) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xe);
                  errStep = (quadCount - dstCount) * 2;
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelDwordB = CONCAT22(CONCAT11(pixel,pixel),CONCAT11(pixel,pixel));
                  *(uint32_t *)((int)dstCursor + 1) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 5) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 9) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xd);
                  errStep = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errTerm = errTerm + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 0xe:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                *(uint16_t *)dstCursor = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 2) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 6) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 10) = pixelDwordB;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 1),*(uint8_t *)((int)srcCursor + 1)
                                );
                *(uint16_t *)((int)dstCursor + 0xe) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor[4] = pixelDwordB;
                dstCursor[5] = pixelDwordB;
                dstCursor[6] = pixelDwordB;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 2),*(uint8_t *)((int)srcCursor + 2)
                                );
                *(uint16_t *)(dstCursor + 7) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 0x1e) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x22) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x26) = pixelDwordB;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 3),*(uint8_t *)((int)srcCursor + 3)
                                );
                srcCursor = srcCursor + 1;
                *(uint16_t *)((int)dstCursor + 0x2a) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor[0xb] = pixelDwordB;
                dstCursor[0xc] = pixelDwordB;
                dstCursor[0xd] = pixelDwordB;
                dstCursor = dstCursor + 0xe;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                *(uint16_t *)dstCursor = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 2) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 6) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 10) = pixelDwordB;
                dstCursor = (uint32_t *)((int)dstCursor + 0xe);
              }
            }
            else {
              quadCount = spanW * 0xf;
              spanX = (spanW * 0xe - dstCount) * 2;
              errTerm = spanX;
              do {
                if (errTerm < 0) {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelPair = CONCAT11(pixel,pixel);
                  *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 3) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 7) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 0xb) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xf);
                  errStep = (quadCount - dstCount) * 2;
                }
                else {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  *(uint16_t *)dstCursor = pixelPair;
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 2) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 6) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 10) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xe);
                  errStep = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                errTerm = errTerm + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 0xf:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixel = *(uint8_t *)srcCursor;
                *(uint8_t *)dstCursor = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 3) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 7) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0xb) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + 1);
                *(uint8_t *)((int)dstCursor + 0xf) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)(dstCursor + 4) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 0x12) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x16) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x1a) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + 2);
                *(uint8_t *)((int)dstCursor + 0x1e) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 0x1f) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 0x21) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x25) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0x29) = pixelDwordB;
                pixel = *(uint8_t *)((int)srcCursor + 3);
                srcCursor = srcCursor + 1;
                *(uint8_t *)((int)dstCursor + 0x2d) = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 0x2e) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor[0xc] = pixelDwordB;
                dstCursor[0xd] = pixelDwordB;
                dstCursor[0xe] = pixelDwordB;
                dstCursor = dstCursor + 0xf;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                *(uint8_t *)dstCursor = pixel;
                pixelPair = CONCAT11(pixel,pixel);
                *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *(uint32_t *)((int)dstCursor + 3) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 7) = pixelDwordB;
                *(uint32_t *)((int)dstCursor + 0xb) = pixelDwordB;
                dstCursor = (uint32_t *)((int)dstCursor + 0xf);
              }
            }
            else {
              errTerm = spanW * 0x10;
              spanX = (spanW * 0xf - dstCount) * 2;
              quadCount = spanX;
              do {
                if (quadCount < 0) {
                  pixelPair = CONCAT11((char)*srcCursor,(char)*srcCursor);
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *dstCursor = pixelDwordB;
                  dstCursor[1] = pixelDwordB;
                  dstCursor[2] = pixelDwordB;
                  dstCursor[3] = pixelDwordB;
                  dstCursor = dstCursor + 4;
                  errStep = (errTerm - dstCount) * 2;
                }
                else {
                  pixel = (uint8_t)*srcCursor;
                  *(uint8_t *)dstCursor = pixel;
                  pixelPair = CONCAT11(pixel,pixel);
                  *(uint16_t *)((int)dstCursor + 1) = pixelPair;
                  pixelDwordB = CONCAT22(pixelPair,pixelPair);
                  *(uint32_t *)((int)dstCursor + 3) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 7) = pixelDwordB;
                  *(uint32_t *)((int)dstCursor + 0xb) = pixelDwordB;
                  dstCursor = (uint32_t *)((int)dstCursor + 0xf);
                  errStep = spanX;
                }
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                quadCount = quadCount + errStep;
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          case 0x10:
            if (spanX == 0) {
              for (; srcQuads != 0; srcQuads = srcQuads + -1) {
                pixelPair = CONCAT11(*(uint8_t *)srcCursor,*(uint8_t *)srcCursor);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *dstCursor = pixelDwordB;
                dstCursor[1] = pixelDwordB;
                dstCursor[2] = pixelDwordB;
                dstCursor[3] = pixelDwordB;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 1),*(uint8_t *)((int)srcCursor + 1)
                                );
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor[4] = pixelDwordB;
                dstCursor[5] = pixelDwordB;
                dstCursor[6] = pixelDwordB;
                dstCursor[7] = pixelDwordB;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 2),*(uint8_t *)((int)srcCursor + 2)
                                );
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor[8] = pixelDwordB;
                dstCursor[9] = pixelDwordB;
                dstCursor[10] = pixelDwordB;
                dstCursor[0xb] = pixelDwordB;
                pixelPair = CONCAT11(*(uint8_t *)((int)srcCursor + 3),*(uint8_t *)((int)srcCursor + 3)
                                );
                srcCursor = srcCursor + 1;
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor[0xc] = pixelDwordB;
                dstCursor[0xd] = pixelDwordB;
                dstCursor[0xe] = pixelDwordB;
                dstCursor[0xf] = pixelDwordB;
                dstCursor = dstCursor + 0x10;
              }
              for (spanW = spanW & 3; spanW != 0; spanW = spanW - 1) {
                pixel = *(uint8_t *)srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                pixelPair = CONCAT11(pixel,pixel);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *dstCursor = pixelDwordB;
                dstCursor[1] = pixelDwordB;
                dstCursor[2] = pixelDwordB;
                dstCursor[3] = pixelDwordB;
                dstCursor = dstCursor + 4;
              }
            }
            else {
LAB_00430a67:
              spanX = spanW * 2;
              quadCount = (spanW * 0x10 - dstCount) * 2;
              errTerm = quadCount;
              do {
                dstCount = 0;
                for (; errTerm < 0; errTerm = errTerm + spanX) {
                  dstCount = dstCount + 1;
                }
                errTerm = errTerm + quadCount;
                pixel = (uint8_t)*srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                pixelPair = CONCAT11(pixel,pixel);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *dstCursor = pixelDwordB;
                dstCursor[1] = pixelDwordB;
                dstCursor[2] = pixelDwordB;
                dstCursor[3] = pixelDwordB;
                srcPtr = dstCursor + 4;
                if ((dstCount & 1) != 0) {
                  *(uint8_t *)(dstCursor + 4) = pixel;
                  srcPtr = (uint32_t *)((int)dstCursor + 0x11);
                }
                dstCursor = srcPtr;
                if (((int)dstCount >> 1 & 1U) != 0) {
                  *(uint16_t *)dstCursor = pixelPair;
                  dstCursor = (uint32_t *)((int)dstCursor + 2);
                }
                for (errStep = (int)dstCount >> 2; errStep != 0; errStep = errStep + -1) {
                  *dstCursor = pixelDwordB;
                  dstCursor = dstCursor + 1;
                }
                spanW = spanW - 1;
              } while (spanW != 0);
            }
            break;
          default:
            if (spanX != 0) goto LAB_00430a67;
            dstCount = (int)dstCount / (int)spanW - 0x10;
            spanX = (int)dstCount >> 2;
            if (srcQuads != 0) {
              srcWidth = dstCount & 1;
              do {
                pixel = (uint8_t)*srcCursor;
                pixelPair = CONCAT11(pixel,pixel);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *dstCursor = pixelDwordB;
                dstCursor[1] = pixelDwordB;
                dstCursor[2] = pixelDwordB;
                dstCursor[3] = pixelDwordB;
                srcPtr = dstCursor + 4;
                if (srcWidth != 0) {
                  *(uint8_t *)(dstCursor + 4) = pixel;
                  srcPtr = (uint32_t *)((int)dstCursor + 0x11);
                }
                dstCursor = srcPtr;
                pixelDword = (int)dstCount >> 1 & 1;
                srcPtr = dstCursor;
                quadCount = spanX;
                if (pixelDword != 0) {
                  *(uint16_t *)dstCursor = pixelPair;
                  srcPtr = (uint32_t *)((int)dstCursor + 2);
                }
                for (; quadCount != 0; quadCount = quadCount + -1) {
                  *srcPtr = pixelDwordB;
                  srcPtr = srcPtr + 1;
                }
                pixel = *(uint8_t *)((int)srcCursor + 1);
                pixelPair = CONCAT11(pixel,pixel);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor = srcPtr + 4;
                *srcPtr = pixelDwordB;
                srcPtr[1] = pixelDwordB;
                srcPtr[2] = pixelDwordB;
                srcPtr[3] = pixelDwordB;
                if (srcWidth != 0) {
                  *(uint8_t *)dstCursor = pixel;
                  dstCursor = (uint32_t *)((int)srcPtr + 0x11);
                }
                srcPtr = dstCursor;
                quadCount = spanX;
                if (pixelDword != 0) {
                  *(uint16_t *)dstCursor = pixelPair;
                  srcPtr = (uint32_t *)((int)dstCursor + 2);
                }
                for (; quadCount != 0; quadCount = quadCount + -1) {
                  *srcPtr = pixelDwordB;
                  srcPtr = srcPtr + 1;
                }
                pixel = *(uint8_t *)((int)srcCursor + 2);
                pixelPair = CONCAT11(pixel,pixel);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor = srcPtr + 4;
                *srcPtr = pixelDwordB;
                srcPtr[1] = pixelDwordB;
                srcPtr[2] = pixelDwordB;
                srcPtr[3] = pixelDwordB;
                if (srcWidth != 0) {
                  *(uint8_t *)dstCursor = pixel;
                  dstCursor = (uint32_t *)((int)srcPtr + 0x11);
                }
                srcPtr = dstCursor;
                quadCount = spanX;
                if (pixelDword != 0) {
                  *(uint16_t *)dstCursor = pixelPair;
                  srcPtr = (uint32_t *)((int)dstCursor + 2);
                }
                for (; quadCount != 0; quadCount = quadCount + -1) {
                  *srcPtr = pixelDwordB;
                  srcPtr = srcPtr + 1;
                }
                pixel = *(uint8_t *)((int)srcCursor + 3);
                srcCursor = srcCursor + 1;
                pixelPair = CONCAT11(pixel,pixel);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                dstCursor = srcPtr + 4;
                *srcPtr = pixelDwordB;
                srcPtr[1] = pixelDwordB;
                srcPtr[2] = pixelDwordB;
                srcPtr[3] = pixelDwordB;
                if (srcWidth != 0) {
                  *(uint8_t *)dstCursor = pixel;
                  dstCursor = (uint32_t *)((int)srcPtr + 0x11);
                }
                quadCount = spanX;
                if (pixelDword != 0) {
                  *(uint16_t *)dstCursor = pixelPair;
                  dstCursor = (uint32_t *)((int)dstCursor + 2);
                }
                for (; quadCount != 0; quadCount = quadCount + -1) {
                  *dstCursor = pixelDwordB;
                  dstCursor = dstCursor + 1;
                }
                srcQuads = srcQuads + -1;
              } while (srcQuads != 0);
            }
            spanW = spanW & 3;
            if (spanW != 0) {
              do {
                pixel = (uint8_t)*srcCursor;
                srcCursor = (uint32_t *)((int)srcCursor + 1);
                pixelPair = CONCAT11(pixel,pixel);
                pixelDwordB = CONCAT22(pixelPair,pixelPair);
                *dstCursor = pixelDwordB;
                dstCursor[1] = pixelDwordB;
                dstCursor[2] = pixelDwordB;
                dstCursor[3] = pixelDwordB;
                srcPtr = dstCursor + 4;
                if ((dstCount & 1) != 0) {
                  *(uint8_t *)(dstCursor + 4) = pixel;
                  srcPtr = (uint32_t *)((int)dstCursor + 0x11);
                }
                dstCursor = srcPtr;
                quadCount = spanX;
                if (((int)dstCount >> 1 & 1U) != 0) {
                  *(uint16_t *)dstCursor = pixelPair;
                  dstCursor = (uint32_t *)((int)dstCursor + 2);
                }
                for (; quadCount != 0; quadCount = quadCount + -1) {
                  *dstCursor = pixelDwordB;
                  dstCursor = dstCursor + 1;
                }
                spanW = spanW - 1;
              } while (spanW != 0);
            }
          }
        }
      }
      spanCount = spanCount + -1;
    } while (spanCount != 0);
  }
  return;
#endif
}


/* Decodes one hotspot-plane row, left to right, from the general four-opcode RLE and builds its span
 * list, like DecodeHotspotPlaneRowForward but with the full opcode set. Opcode low two bits:
 * 0 = copy from the reference row deltaPx away (skip when deltaPx is 0), 1 = transparent gap,
 * 2 = fill with the next byte, 3 = literal bytes; the length is opcode >> 2, written as n & 3 bytes
 * then dwords. Spans are built as pointers and rebased to row offsets at the end.
 * The plane hook used when the DIB has no ext; DecodeHotspotPlaneRowBackward is the ext-set sibling.
 * Port note: the VC++ arm is the exe's instructions (GEN-ASM-ARM), the C arm is the port's decode. */
DF_NAKED void __cdecl DecodeHotspotPlaneRow(byte *pRle, int deltaPx, int planeIdx) /* 0x00430e70 */
{
#ifdef _MSC_VER
    /* GEN-ASM-ARM BEGIN -- the instruction sequence transcribed from TI.EXE; do not hand-edit */
    __asm {
        push ebp
        mov ebp,esp
        push ebx
        push esi
        push edi
        pushad
        mov eax,DWORD PTR [ebp+0x10]
        mov ebx,eax
        imul eax,eax,0x400
        mov edi,eax
        add edi,DWORD PTR gTrigPixelRowBase
        push edi
        mov eax,DWORD PTR gTrigPlaneSpanBufs[ebx*4]
        mov DWORD PTR gTrigPlaneSpans[ebx*4],eax
        push eax
        mov ebx,eax
        mov esi,DWORD PTR [ebp+0x8]
        mov DWORD PTR [ebx],edi
        mov edx,DWORD PTR gTrigSrcWidth
        add edx,edi
        xor eax,eax
        push ebp
        mov ebp,DWORD PTR [ebp+0xc]
    L_3f:
        lodsb
        shr eax,0x1
        jae L_72
        shr eax,0x1
        jae L_5e
        mov ecx,eax
        and ecx,0x3
        rep movsb
        mov ecx,eax
        shr ecx,0x2
        rep movsd
        cmp edi,edx
        jb L_3f
        jmp L_d0
    L_5e:
        cmp DWORD PTR [ebx],edi
        je L_68
        mov DWORD PTR [ebx+0x4],edi
        add ebx,0x8
    L_68:
        add edi,eax
        mov DWORD PTR [ebx],edi
        cmp edi,edx
        jb L_3f
        jmp L_d0
    L_72:
        shr eax,0x1
        jae L_a3
        mov ecx,eax
        shr eax,0x2
        je L_9a
        push eax
        lodsb
        and ecx,0x3
        rep stosb
        mov ah,al
        mov cx,ax
        shl ecx,0x10
        or eax,ecx
        pop ecx
        rep stosd
        xor eax,eax
        cmp edi,edx
        jb L_3f
        jmp L_d0
    L_9a:
        lodsb
        rep stosb
        cmp edi,edx
        jb L_3f
        jmp L_d0
    L_a3:
        cmp ebp,0x0
        je L_c6
        push esi
        mov esi,edi
        add esi,ebp
        mov ecx,eax
        and ecx,0x3
        rep movsb
        mov ecx,eax
        shr ecx,0x2
        rep movsd
        pop esi
        cmp edi,edx
        jb L_3f
        jmp L_d0
    L_c6:
        add edi,eax
        cmp edi,edx
        jb L_3f
    L_d0:
        cmp DWORD PTR [ebx],edi
        je L_da
        mov DWORD PTR [ebx+0x4],edi
        add ebx,0x8
    L_da:
        pop ebp
        pop edx
        pop edi
        xor eax,eax
        mov ecx,DWORD PTR [ebp+0x10]
    L_e2:
        cmp ebx,edx
        je L_f1
        sub DWORD PTR [edx],edi
        sub DWORD PTR [edx+0x4],edi
        add edx,0x8
        inc eax
        jmp L_e2
    L_f1:
        mov DWORD PTR gTrigPlaneSpanCounts[ecx*4],eax
        popad
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
    /* GEN-ASM-ARM END */
#else
    byte     opcode;
    byte     fillByte;
    uint32_t dwordCount;
    int      spanCount;
    uint32_t runLen;
    byte    *rowEnd;
    int     *spanBuf;
    int     *pSpan;
    byte    *pRleNext;
    byte    *pRefSrc;
    byte    *rowStart;
    byte    *pOut;

    rowStart = (byte *)(planeIdx * 0x400 + gTrigPixelRowBase);
    spanBuf  = (int *)gTrigPlaneSpanBufs[planeIdx];
    gTrigPlaneSpans[planeIdx] = spanBuf;
    *spanBuf  = (int)rowStart;
    rowEnd   = rowStart + gTrigSrcWidth;
    pSpan   = spanBuf;
    pOut  = rowStart;
    do {
        while (1) {
            pRleNext = pRle + 1;
            opcode  = *pRle;
            if ((opcode & 1) == 0) break;
            if ((opcode & 2) == 0) {
                if ((byte *)*pSpan != pOut) {
                    pSpan[1] = (int)pOut;
                    pSpan    = pSpan + 2;
                }
                pOut += (opcode >> 2);
                *pSpan  = (int)pOut;
                pRle     = pRleNext;
            } else {
                for (runLen = (opcode & 0xc) >> 2; runLen != 0; runLen--) {
                    *pOut++ = *pRleNext++;
                }
                for (runLen = (uint32_t)(opcode >> 4); pRle = pRleNext, runLen != 0; runLen--) {
                    *(uint32_t *)pOut = *(uint32_t *)pRleNext;
                    pRleNext  += 4;
                    pOut += 4;
                }
            }
joined_r0x00430eca:
            if (rowEnd <= pOut) goto LAB_00430f40;
        }
        runLen = (uint32_t)(opcode >> 2);
        if ((opcode & 2) == 0) {
            if (deltaPx == 0) {
                pOut += runLen;
                pRle     = pRleNext;
            } else {
                pRefSrc = pOut + deltaPx;
                for (runLen = (opcode & 0xc) >> 2; runLen != 0; runLen--) {
                    *pOut++ = *pRefSrc++;
                }
                for (runLen = (uint32_t)(opcode >> 4); pRle = pRleNext, runLen != 0; runLen--) {
                    *(uint32_t *)pOut = *(uint32_t *)pRefSrc;
                    pRefSrc += 4;
                    pOut += 4;
                }
            }
            goto joined_r0x00430eca;
        }
        dwordCount = (uint32_t)(opcode >> 4);
        if (dwordCount == 0) {
            pRle  = pRle + 2;
            opcode = *pRleNext;
            for (; runLen != 0; runLen--)
                *pOut++ = opcode;
            goto joined_r0x00430eca;
        }
        pRle  = pRle + 2;
        fillByte = *pRleNext;
        for (runLen = (opcode & 0xc) >> 2; runLen != 0; runLen--)
            *pOut++ = fillByte;
        for (; dwordCount != 0; dwordCount--) {
            *(uint32_t *)pOut = CONCAT22(CONCAT11(fillByte,fillByte),CONCAT11(fillByte,fillByte));
            pOut += 4;
        }
    } while (pOut < rowEnd);
LAB_00430f40:
    if ((byte *)*pSpan != pOut) {
        pSpan[1] = (int)pOut;
        pSpan    = pSpan + 2;
    }
    spanCount = 0;
    for (; pSpan != spanBuf; spanBuf = spanBuf + 2) {
        *spanBuf   -= (int)rowStart;
        spanBuf[1] -= (int)rowStart;
        spanCount++;
    }
    gTrigPlaneSpanCounts[planeIdx] = spanCount;
#endif
}


/* Decodes one hotspot-plane row from its right end to its left, using the same four opcodes as
 * DecodeHotspotPlaneRow (0 delta copy, 1 gap, 2 fill, 3 literal bytes stored right to left). The span
 * list grows downward from offset 0xff8 of the plane's span buffer; gTrigPlaneSpans points at its
 * first record and gTrigPlaneSpanCounts holds the count. FatalError 0x436 if the row does not end
 * exactly at its left edge. The plane hook used when the DIB has ext set. */
void __cdecl DecodeHotspotPlaneRowBackward(byte *pRle, int deltaPx, int planeIdx) /* 0x00430f70 */
{
#ifdef _MSC_VER
    /* Byte-match arm: plain C (an /O2 frame without ebp and a compiler switch table), not
     * assembly. The #else arm is the port's C, unchanged; the two compute the same thing. */
    int   c;
    int   n;
    int   k;
    int   pos;
    int  *pSpan;
    byte *pCur;
    byte *pBase;
    byte *src;
    byte *dst;
    unsigned int fill;
    byte  v;

    pCur  = (byte *)(planeIdx * 0x400 + gTrigPixelRowBase);
    pBase = pCur;
    pCur += gTrigSrcWidth;
    pSpan = (int *)(gTrigPlaneSpanBufs[planeIdx] + 0xff8);
    pos   = gTrigSrcWidth;
    pSpan[1] = gTrigSrcWidth;
    do {
        c = *pRle++;
        n = c >> 2;
        pCur -= n;
        switch (c & 3) {
        case 0:
            if (deltaPx != 0) {
                k = n & 3;
                src = pCur + deltaPx;
                dst = pCur;
                for (; k != 0; k--)
                    *dst++ = *src++;
                for (k = n >> 2; k != 0; k--) {
                    *(int *)dst = *(int *)src;
                    src += 4;
                    dst += 4;
                }
            }
            pos -= n;
            break;
        case 1:
            if (pSpan[1] != pos) {
                *pSpan = pos;
                pSpan -= 2;
            }
            pos -= n;
            pSpan[1] = pos;
            break;
        case 2:
            v = *pRle++;
            dst = pCur;
            k = n & 3;
            if (k != 0) {
                do {
                    *dst++ = v;
                } while (--k != 0);
            }
            k = n >> 2;
            if (k != 0) {
                fill = v;
                fill |= fill << 8;
                fill |= fill << 16;
                do {
                    *(unsigned int *)dst = fill;
                    dst += 4;
                } while (--k != 0);
            }
            pos -= n;
            break;
        case 3:
            k = n & 3;
            dst = pCur + n;
            for (; k != 0; k--)
                *--dst = *pRle++;
            for (k = n >> 2; k != 0; k--) {
                *--dst = *pRle++;
                *--dst = *pRle++;
                *--dst = *pRle++;
                *--dst = *pRle++;
            }
            pos -= n;
            break;
        }
    } while (pBase < pCur);
    if (pSpan[1] != pos) {
        *pSpan = pos;
        pSpan -= 2;
    }
    pos = 0x3fe - ((int)pSpan - gTrigPlaneSpanBufs[planeIdx] >> 2);
    gTrigPlaneSpanCounts[planeIdx] = pos;
    gTrigPlaneSpanCounts[planeIdx] = pos >> 1;
    gTrigPlaneSpans[planeIdx] = (int)(pSpan + 2);
    if (pBase != pCur)
        FatalError(0, 0x436);
#else
    byte      token;
    uint32_t  dwordTmp;
    byte     *pBase;
    byte     *pNext;
    int       pos;
    int       runLen4;
    byte     *pSrc;
    byte     *pDst;
    uint32_t  tailCount;
    uint32_t  runLen;
    int      *pSpan;
    byte     *pCur;

    pos   = gTrigSrcWidth;
    pBase = (byte *)(planeIdx * 0x400 + gTrigPixelRowBase);
    pCur  = pBase + gTrigSrcWidth;
    pSpan = (int *)(gTrigPlaneSpanBufs[planeIdx] + 0xff8);
    *(int *)(gTrigPlaneSpanBufs[planeIdx] + 0xffc) = gTrigSrcWidth;
    do {
        token  = *pRle;
        pNext  = pRle + 1;
        runLen = (uint32_t)(uint)token >> 2;
        pCur   = pCur - runLen;
        if ((token & 3) < 4) {
            runLen4 = (int)(uint)token >> 4;
            switch (token & 3) {
            case 0:
                if (deltaPx != 0) {
                    pSrc = pCur + deltaPx;
                    pDst = pCur;
                    for (tailCount = runLen & 3; tailCount != 0; tailCount--)
                        *pDst++ = *pSrc++;
                    for (; runLen4 != 0; runLen4--) {
                        dwordTmp = *(uint32_t *)pSrc;
                        pSrc += 4;
                        *(uint32_t *)pDst = dwordTmp;
                        pDst += 4;
                    }
                }
                break;
            case 1:
                if (pSpan[1] != pos) {
                    *pSpan = pos;
                    pSpan  = pSpan - 2;
                }
                pos     -= runLen;
                pSpan[1] = pos;
                goto LAB_004310b6;
            case 2:
                token = *pNext;
                pNext = pRle + 2;
                pDst  = pCur;
                for (tailCount = runLen & 3; tailCount != 0; tailCount--)
                    *pDst++ = token;
                if (runLen4 != 0) {
                    do {
                        *(uint32_t *)pDst = CONCAT22(CONCAT11(token,token),CONCAT11(token,token));
                        pDst += 4;
                    } while (--runLen4 != 0);
                }
                break;
            case 3:
                pDst = pCur + runLen;
                for (tailCount = runLen & 3; tailCount != 0; tailCount--)
                    *--pDst = *pNext++;
                for (; runLen4 != 0; runLen4--) {
                    pDst[-1] = pNext[0];
                    pDst[-2] = pNext[1];
                    pDst[-3] = pNext[2];
                    pDst[-4] = pNext[3];
                    pNext += 4;
                    pDst  -= 4;
                }
            }
            pos -= runLen;
        }
LAB_004310b6:
        pRle = pNext;
        if (pCur <= pBase) {
            if (pSpan[1] != pos) {
                *pSpan = pos;
                pSpan  = pSpan - 2;
            }
            pos = 0x3fe - ((int)pSpan - gTrigPlaneSpanBufs[planeIdx] >> 2);
            gTrigPlaneSpanCounts[planeIdx] = pos;
            gTrigPlaneSpanCounts[planeIdx] = pos >> 1;
            gTrigPlaneSpans[planeIdx] = pSpan + 2;
            if (pCur != pBase)
                FatalError(0, 0x436);
            return;
        }
    } while (1);
#endif
}



/* Blits one RLE-compressed sprite row left to right: skips `skip` output pixels, then writes `count`
 * pixels to pbDst. pbPrev is the previously written row, the source of copy runs. Opcode low two
 * bits, length opcode >> 2: 0 = copy from pbPrev, 2 = fill with the next byte, 1 = skip
 * (transparent), 3 = literal bytes. `width` is unused by the C arm. Called by the sprite blitters
 * in 08_*, with BackwardHotspotRleBlitRow when flipV is set. */
DF_NAKED void ForwardHotspotRleBlitRow(uint8_t *pbRle, short skip, short count, short width,
                              uint8_t *pbDst, uint8_t *pbPrev) /* 0x00431140 */
{
#ifdef _MSC_VER
    /* GEN-ASM-ARM BEGIN -- the instruction sequence transcribed from TI.EXE; do not hand-edit */
    __asm {
        push ebp
        mov ebp,esp
        push ebx
        push esi
        push edi
        pushad
        push ebp
        mov esi,DWORD PTR [ebp+0x8]
        mov edi,DWORD PTR [ebp+0x18]
        mov ebx,DWORD PTR [ebp+0x1c]
        xor eax,eax
        movsx edx,WORD PTR [ebp+0x10]
        or edx,edx
        jle L_135
        movsx ecx,WORD PTR [ebp+0xc]
        or ecx,ecx
        jle L_74
        sub edx,ecx
        add edi,ecx
        add ebx,ecx
    L_2d:
        lodsb
        shr eax,0x1
        jb L_4f
        shr eax,0x1
        jb L_42
        sub ecx,eax
        jg L_2d
        mov eax,ecx
        neg eax
        jmp L_7f
    L_42:
        sub ecx,eax
        jle L_49
        inc esi
        jmp L_2d
    L_49:
        mov eax,ecx
        neg eax
        jmp L_ae
    L_4f:
        shr eax,0x1
        jb L_61
        sub ecx,eax
        jg L_2d
        mov eax,ecx
        neg eax
        jmp L_fb
    L_61:
        cmp ecx,eax
        jbe L_6b
        sub ecx,eax
        add esi,eax
        jmp L_2d
    L_6b:
        sub eax,ecx
        add esi,ecx
        jmp L_10a
    L_74:
        lodsb
        shr eax,0x1
        jb L_f6
        shr eax,0x1
        jb L_ae
    L_7f:
        cmp edx,eax
        jbe L_99
        xchg ebx,esi
        mov ecx,eax
        shr ecx,0x2
        sub edx,eax
        and eax,0x3
        rep movsd
        mov ecx,eax
        rep movsb
        xchg ebx,esi
        jmp L_74
    L_99:
        mov esi,ebx
        mov ecx,edx
        shr ecx,0x2
        and edx,0x3
        rep movsd
        mov ecx,edx
        rep movsb
        jmp L_135
    L_ae:
        cmp edx,eax
        jbe L_d8
        mov ecx,eax
        mov ebp,eax
        sub edx,eax
        lodsb
        and ecx,0x3
        add ebx,ebp
        rep stosb
        shr ebp,0x2
        je L_74
        mov ah,al
        mov cx,ax
        shl eax,0x10
        mov ax,cx
        mov ecx,ebp
        rep stosd
        xor eax,eax
        jmp L_74
    L_d8:
        mov ecx,edx
        lodsb
        and ecx,0x3
        rep stosb
        shr edx,0x2
        je L_135
        mov ecx,edx
        mov ah,al
        mov dx,ax
        shl eax,0x10
        mov ax,dx
        rep stosd
        jmp L_135
    L_f6:
        shr eax,0x1
        jb L_10a
    L_fb:
        cmp edx,eax
        jbe L_135
        add edi,eax
        add ebx,eax
        sub edx,eax
        jmp L_74
    L_10a:
        cmp edx,eax
        jbe L_125
        mov ecx,eax
        add ebx,eax
        shr ecx,0x2
        sub edx,eax
        and eax,0x3
        rep movsd
        mov ecx,eax
        rep movsb
        jmp L_74
    L_125:
        mov ecx,edx
        shr ecx,0x2
        and edx,0x3
        rep movsd
        mov ecx,edx
        rep movsb
        jmp L_135
    L_135:
        pop ebp
        popad
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
    /* GEN-ASM-ARM END */
#else
    int      underflow;
    uint32_t scratch;
    uint8_t  opcode;
    uint32_t pixelsLeft;
    uint32_t runLen;
    uint32_t skipLeft;
    uint8_t *pRleCur;

    (void)width;

    scratch = 0;
    pixelsLeft = (uint32_t)(uint16_t)count;
    if ((int32_t)pixelsLeft < 1) {
        return;
    }
    skipLeft = (uint32_t)(uint16_t)skip;
    if ((int32_t)skipLeft < 1)
        goto LAB_004311b4;
    pixelsLeft = pixelsLeft - skipLeft;
    pbDst += skipLeft;
    pbPrev += skipLeft;
    pRleCur = pbRle;
    while (1) {
        while (1) {
            while (1) {
                pbRle    = pRleCur + 1;
                opcode      = *pRleCur;
                if ((opcode & 1) != 0)
                    break;
                scratch = (uint32_t)(opcode >> 2);
                if ((opcode & 2) == 0) {
                    runLen = skipLeft - scratch;
                    underflow = (int32_t)skipLeft < (int32_t)scratch;
                    skipLeft = runLen;
                    pRleCur = pbRle;
                    if (runLen == 0 || underflow) {
                        runLen = (uint32_t)(-(int32_t)runLen);
                        goto LAB_004311bf;
                    }
                } else {
                    runLen = skipLeft - scratch;
                    if (runLen == 0 || (int32_t)skipLeft < (int32_t)scratch) {
                        runLen = (uint32_t)(-(int32_t)runLen);
                        pRleCur = pbRle;
                        goto LAB_004311ee;
                    }
                    skipLeft = runLen;
                    pRleCur += 2;
                }
            }
            runLen = (uint32_t)(opcode >> 2);
            if ((opcode & 2) != 0)
                break;
            scratch = skipLeft - runLen;
            underflow = (int32_t)skipLeft < (int32_t)runLen;
            skipLeft = scratch;
            pRleCur = pbRle;
            if (scratch == 0 || underflow) {
                scratch = (uint32_t)(-(int32_t)scratch);
                goto LAB_0043123b;
            }
        }
        if (skipLeft <= runLen)
            break;
        skipLeft -= runLen;
        pRleCur = pbRle + runLen;
    }
    runLen -= skipLeft;
    pRleCur = pbRle + skipLeft;
    do {
        if (pixelsLeft <= runLen) {
            scratch = pixelsLeft & 3;
            for (pixelsLeft >>= 2; pixelsLeft != 0; pixelsLeft--) {
                *(uint32_t *)pbDst = *(uint32_t *)pRleCur;
                pRleCur += 4;
                pbDst += 4;
            }
            for (; scratch != 0; scratch--) {
                *pbDst++ = *pRleCur++;
            }
            return;
        }
        pbPrev += runLen;
        pixelsLeft -= runLen;
        scratch = runLen & 3;
        pbRle = pRleCur;
        for (runLen >>= 2; runLen != 0; runLen--) {
            *(uint32_t *)pbDst = *(uint32_t *)pbRle;
            pbRle += 4;
            pbDst += 4;
        }
        for (skipLeft = scratch; skipLeft != 0; skipLeft--) {
            *pbDst++ = *pbRle++;
        }
    LAB_004311b4:
        while (1) {
            while (1) {
                pRleCur    = pbRle + 1;
                opcode     = *pbRle;
                runLen     = CONCAT31_SHIFT(scratch, opcode);
                pbRle   = pRleCur;
                if ((opcode & 1) != 0)
                    break;
                runLen >>= 2;
                if ((opcode & 2) == 0) {
                LAB_004311bf:
                    if (pixelsLeft <= runLen) {
                        scratch = pixelsLeft & 3;
                        for (pixelsLeft >>= 2; pixelsLeft != 0; pixelsLeft--) {
                            *(uint32_t *)pbDst = *(uint32_t *)pbPrev;
                            pbPrev += 4;
                            pbDst += 4;
                        }
                        for (; scratch != 0; scratch--) {
                            *pbDst++ = *pbPrev++;
                        }
                        return;
                    }
                    pixelsLeft -= runLen;
                    scratch = runLen & 3;
                    for (runLen >>= 2; runLen != 0; runLen--) {
                        *(uint32_t *)pbDst = *(uint32_t *)pbPrev;
                        pbPrev += 4;
                        pbDst += 4;
                    }
                    for (skipLeft = scratch; skipLeft != 0; skipLeft--) {
                        *pbDst++ = *pbPrev++;
                    }
                } else {
                LAB_004311ee:
                    if (pixelsLeft <= runLen) {
                        opcode = *pRleCur;
                        for (scratch = pixelsLeft & 3; scratch != 0; scratch--) {
                            *pbDst++ = opcode;
                        }
                        pixelsLeft >>= 2;
                        if (pixelsLeft != 0) {
                            for (; pixelsLeft != 0; pixelsLeft--) {
                                *(uint32_t *)pbDst = DWORD_FROM_BYTE(opcode);
                                pbDst += 4;
                            }
                        }
                        return;
                    }
                    pixelsLeft -= runLen;
                    pbRle = pRleCur + 1;
                    opcode   = *pRleCur;
                    scratch   = CONCAT31_SHIFT(runLen, opcode);
                    pbPrev += runLen;
                    for (skipLeft = runLen & 3; skipLeft != 0; skipLeft--) {
                        *pbDst++ = opcode;
                    }
                    runLen >>= 2;
                    if (runLen != 0) {
                        for (; runLen != 0; runLen--) {
                            *(uint32_t *)pbDst = DWORD_FROM_BYTE(opcode);
                            pbDst += 4;
                        }
                        scratch = 0;
                    }
                }
            }
            runLen >>= 2;
            scratch = runLen;
            if ((opcode & 2) != 0)
                break;
        LAB_0043123b:
            if (pixelsLeft <= scratch)
                return;
            pbDst += scratch;
            pbPrev += scratch;
            pixelsLeft -= scratch;
        }
    } while (1);
#endif
}


/* ForwardHotspotRleBlitRow for the mirrored case: the same opcodes, but the pixels are written
 * right to left from pbDst + width. Under the 386 flag it hands over to BackwardHotspotRleBlitRow386 (the CPU dispatch is inside
 * this function in the exe). */
DF_NAKED void BackwardHotspotRleBlitRow(uint8_t *pbRle, short skip, short count, short width,
                               uint8_t *pbDst, uint8_t *pbPrev) /* 0x00431280 */
{
#ifdef _MSC_VER
    /* GEN-ASM-ARM BEGIN -- the instruction sequence transcribed from TI.EXE; do not hand-edit */
    __asm {
        cmp DWORD PTR gIs386Cpu,0x0
        push ebp
        mov ebp,esp
        push ebx
        push esi
        push edi
        je L_34
        mov eax,DWORD PTR [ebp+0x1c]
        mov ecx,DWORD PTR [ebp+0x18]
        push eax
        mov edx,DWORD PTR [ebp+0x14]
        push ecx
        mov eax,DWORD PTR [ebp+0x10]
        push edx
        mov ecx,DWORD PTR [ebp+0xc]
        push eax
        mov edx,DWORD PTR [ebp+0x8]
        push ecx
        push edx
        call BackwardHotspotRleBlitRow386
        add esp,0x18
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    L_34:
        pushad
        push ebp
        movsx eax,WORD PTR [ebp+0x14]
        mov esi,DWORD PTR [ebp+0x8]
        mov edi,DWORD PTR [ebp+0x18]
        mov ebx,DWORD PTR [ebp+0x1c]
        add edi,eax
        add ebx,eax
        movsx edx,WORD PTR [ebp+0xc]
        sub edx,eax
        neg edx
        jle L_1dc
        movsx ecx,WORD PTR [ebp+0x10]
        sub ecx,eax
        xor eax,eax
        neg ecx
        jle L_b1
        sub edx,ecx
        sub edi,ecx
        sub ebx,ecx
    L_67:
        lodsb
        shr eax,0x1
        jb L_8c
        shr eax,0x1
        jb L_7c
        sub ecx,eax
        jg L_67
        mov eax,ecx
        neg eax
        jmp L_c0
    L_7c:
        sub ecx,eax
        jle L_83
        inc esi
        jmp L_67
    L_83:
        mov eax,ecx
        neg eax
        jmp L_119
    L_8c:
        shr eax,0x1
        jb L_9e
        sub ecx,eax
        jg L_67
        mov eax,ecx
        neg eax
        jmp L_17a
    L_9e:
        cmp ecx,eax
        jbe L_a8
        sub ecx,eax
        add esi,eax
        jmp L_67
    L_a8:
        sub eax,ecx
        add esi,ecx
        jmp L_189
    L_b1:
        lodsb
        shr eax,0x1
        jb L_175
        shr eax,0x1
        jb L_119
    L_c0:
        cmp edx,eax
        jbe L_f0
        sub edx,eax
        xchg ebx,esi
        std
        mov ecx,eax
        shr ecx,0x2
        je L_de
        sub esi,0x4
        sub edi,0x4
        rep movsd
        add esi,0x4
        add edi,0x4
    L_de:
        mov ecx,eax
        and ecx,0x3
        je L_eb
        dec esi
        dec edi
        rep movsb
        inc esi
        inc edi
    L_eb:
        cld
        xchg ebx,esi
        jmp L_b1
    L_f0:
        std
        mov esi,ebx
        mov ecx,edx
        shr ecx,0x2
        je L_108
        sub esi,0x4
        sub edi,0x4
        rep movsd
        add esi,0x4
        add edi,0x4
    L_108:
        mov ecx,edx
        and ecx,0x3
        je L_113
        dec esi
        dec edi
        rep movsb
    L_113:
        cld
        jmp L_1dc
    L_119:
        cmp edx,eax
        jbe L_150
        mov ecx,eax
        mov ebp,eax
        sub edx,eax
        lodsb
        std
        and ecx,0x3
        sub ebx,ebp
        dec edi
        rep stosb
        inc edi
        shr ebp,0x2
        je L_14a
        mov ah,al
        mov cx,ax
        shl eax,0x10
        mov ax,cx
        mov ecx,ebp
        sub edi,0x4
        rep stosd
        add edi,0x4
        xor eax,eax
    L_14a:
        cld
        jmp L_b1
    L_150:
        mov ecx,edx
        lodsb
        std
        and ecx,0x3
        dec edi
        rep stosb
        inc edi
        shr edx,0x2
        je L_172
        mov ecx,edx
        mov ah,al
        mov dx,ax
        shl eax,0x10
        mov ax,dx
        sub edi,0x4
        rep stosd
    L_172:
        cld
        jmp L_1dc
    L_175:
        shr eax,0x1
        jb L_189
    L_17a:
        cmp edx,eax
        jbe L_1dc
        sub edi,eax
        sub ebx,eax
        sub edx,eax
        jmp L_b1
    L_189:
        cmp edx,eax
        jbe L_1ba
        mov ecx,eax
        sub ebx,eax
        mov ebp,eax
        shr ecx,0x2
        sub edx,eax
        and ebp,0x3
        jecxz L_1aa
    L_19d:
        lodsd
        bswap eax
        mov DWORD PTR [edi-0x4],eax
        sub edi,0x4
        loop L_19d
        xor eax,eax
    L_1aa:
        mov ecx,ebp
        jecxz L_1b5
    L_1ae:
        lodsb
        mov BYTE PTR [edi-0x1],al
        dec edi
        loop L_1ae
    L_1b5:
        jmp L_b1
    L_1ba:
        mov ecx,edx
        shr ecx,0x2
        and edx,0x3
        jecxz L_1cf
    L_1c4:
        lodsd
        bswap eax
        mov DWORD PTR [edi-0x4],eax
        sub edi,0x4
        loop L_1c4
    L_1cf:
        mov ecx,edx
        jecxz L_1dc
    L_1d3:
        lodsb
        mov BYTE PTR [edi-0x1],al
        dec edi
        loop L_1d3
        jmp L_1dc
    L_1dc:
        pop ebp
        popad
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
    /* GEN-ASM-ARM END */
#else
    uint8_t   opcode;
    int       underflow;
    int       edgeDelta;
    uint32_t  runLen;
    uint32_t  tokenRun;
    uint32_t  skipLeft;
    uint32_t  pixelsLeft;
    uint8_t  *pPrevCur;
    uint32_t *pRleCur;
    uint8_t  *pDstCur;
    uint32_t *pRle32;

    /* The i386 row lives at 0x00431470 and takes the same six arguments. */
    if (gIs386Cpu != 0) {
        BackwardHotspotRleBlitRow386(pbRle, skip, count, width, pbDst, pbPrev);
        return;
    }
    pRle32  = (uint32_t *)pbRle;

    edgeDelta   = (int)width;
    pDstCur = pbDst + edgeDelta;
    pPrevCur  = pbPrev + edgeDelta;
    pixelsLeft   = (uint32_t)(-(skip - edgeDelta));
    if (pixelsLeft != 0 && skip - edgeDelta < 1) {
        edgeDelta = count - edgeDelta;
        runLen = 0;
        if (-edgeDelta == 0 || 0 < edgeDelta)
            goto LAB_00431331;
        pixelsLeft += (uint32_t)edgeDelta;
        pDstCur += edgeDelta;
        pPrevCur += edgeDelta;
        skipLeft = (uint32_t)(-edgeDelta);
        pRleCur = pRle32;
        while (1) {
            while (1) {
                while (1) {
                    pRle32    = (uint32_t *)((uint8_t *)pRleCur + 1);
                    opcode      = *(uint8_t *)pRleCur;
                    if ((opcode & 1) == 0)
                        break;
                    tokenRun = (uint32_t)(opcode >> 2);
                    if ((opcode & 2) == 0) {
                        runLen = skipLeft - tokenRun;
                        underflow = (int32_t)skipLeft < (int32_t)tokenRun;
                        skipLeft = runLen;
                        pRleCur = pRle32;
                        if (runLen == 0 || underflow) {
                            runLen = (uint32_t)(-(int32_t)runLen);
                            goto LAB_004313fa;
                        }
                    } else {
                        if (skipLeft <= tokenRun) {
                            runLen = tokenRun - skipLeft;
                            pRleCur = (uint32_t *)((uint8_t *)pRle32 + skipLeft);
                            goto LAB_00431409;
                        }
                        skipLeft -= tokenRun;
                        pRleCur = (uint32_t *)((uint8_t *)pRle32 + tokenRun);
                    }
                }
                runLen = (uint32_t)(opcode >> 2);
                if ((opcode & 2) != 0)
                    break;
                tokenRun = skipLeft - runLen;
                underflow = (int32_t)skipLeft < (int32_t)runLen;
                skipLeft = tokenRun;
                pRleCur = pRle32;
                if (tokenRun == 0 || underflow) {
                    runLen = (uint32_t)(-(int32_t)tokenRun);
                    goto LAB_00431340;
                }
            }
            tokenRun = skipLeft - runLen;
            if (tokenRun == 0 || (int32_t)skipLeft < (int32_t)runLen)
                break;
            skipLeft   = tokenRun;
            pRleCur = (uint32_t *)((uint8_t *)pRleCur + 2);
        }
        tokenRun   = (uint32_t)(-(int32_t)tokenRun);
        pRleCur = pRle32;
        do {
            if (pixelsLeft <= tokenRun) {
                opcode = *(uint8_t *)pRleCur;
                for (runLen = pixelsLeft & 3; runLen != 0; runLen--) {
                    pDstCur[-1] = opcode;
                    pDstCur--;
                }
                pixelsLeft >>= 2;
                if (pixelsLeft != 0) {
                    for (; pixelsLeft != 0; pixelsLeft--) {
                        pDstCur -= 4;
                        *(uint32_t *)pDstCur = DWORD_FROM_BYTE(opcode);
                    }
                }
                return;
            }
            pixelsLeft -= tokenRun;
            pRle32 = (uint32_t *)((uint8_t *)pRleCur + 1);
            opcode   = *(uint8_t *)pRleCur;
            runLen   = CONCAT31_SHIFT(tokenRun, opcode);
            pPrevCur -= tokenRun;
            for (skipLeft = tokenRun & 3; skipLeft != 0; skipLeft--) {
                pDstCur[-1] = opcode;
                pDstCur--;
            }
            tokenRun >>= 2;
            if (tokenRun != 0) {
                for (; tokenRun != 0; tokenRun--) {
                    *(uint32_t *)(pDstCur - 4) = DWORD_FROM_BYTE(opcode);
                    pDstCur -= 4;
                }
                runLen = 0;
            }
        LAB_00431331:
            while (1) {
                while (1) {
                    pRleCur   = (uint32_t *)((uint8_t *)pRle32 + 1);
                    opcode     = *(uint8_t *)pRle32;
                    tokenRun     = CONCAT31_SHIFT(runLen, opcode);
                    pRle32   = pRleCur;
                    if ((opcode & 1) == 0)
                        break;
                    runLen = tokenRun >> 2;
                    if ((opcode & 2) == 0) {
                    LAB_004313fa:
                        if (pixelsLeft <= runLen)
                            return;
                        pDstCur -= runLen;
                        pPrevCur -= runLen;
                        pixelsLeft -= runLen;
                    } else {
                    LAB_00431409:
                        if (pixelsLeft <= runLen) {
                            uint32_t ru = pixelsLeft & 3;
                            for (pixelsLeft >>= 2; pixelsLeft != 0; pixelsLeft--) {
                                uint32_t w = *pRleCur;
                                pDstCur -= 4;
                                *(uint32_t *)pDstCur = BSWAP32(w);
                                pRleCur++;
                            }
                            for (; ru != 0; ru--) {
                                pDstCur[-1] = *(uint8_t *)pRleCur;
                                pDstCur--;
                                pRleCur = (uint32_t *)((uint8_t *)pRleCur + 1);
                            }
                            return;
                        }
                        pPrevCur -= runLen;
                        {
                            uint32_t uVar6w = runLen >> 2;
                            pixelsLeft -= runLen;
                            skipLeft = runLen & 3;
                            pRle32 = pRleCur;
                            if (uVar6w != 0) {
                                do {
                                    pRleCur = pRle32 + 1;
                                    runLen   = *pRle32;
                                    pDstCur -= 4;
                                    *(uint32_t *)pDstCur = BSWAP32(runLen);
                                    uVar6w--;
                                    pRle32 = pRleCur;
                                } while (uVar6w != 0);
                                runLen = 0;
                            }
                            for (; skipLeft != 0; skipLeft--) {
                                runLen       = CONCAT31_SHIFT(runLen, *(uint8_t *)pRle32);
                                pDstCur[-1] = *(uint8_t *)pRle32;
                                pDstCur--;
                                pRle32 = (uint32_t *)((uint8_t *)pRle32 + 1);
                            }
                        }
                    }
                }
                tokenRun >>= 2;
                runLen = tokenRun;
                if ((opcode & 2) != 0)
                    break;
            LAB_00431340:
                if (pixelsLeft <= runLen) {
                    runLen = pixelsLeft >> 2;
                    if (runLen != 0) {
                        while (runLen != 0) {
                            runLen--;
                            pDstCur -= 4;
                            pPrevCur -= 4;
                            *(uint32_t *)pDstCur = *(uint32_t *)pPrevCur;
                        }
                    }
                    pixelsLeft &= 3;
                    if (pixelsLeft == 0)
                        return;
                    while (pixelsLeft != 0) {
                        pDstCur--;
                        pPrevCur--;
                        *pDstCur = *pPrevCur;
                        pixelsLeft--;
                    }
                    return;
                }
                pixelsLeft -= runLen;
                skipLeft = runLen >> 2;
                if (skipLeft != 0) {
                    while (skipLeft != 0) {
                        skipLeft--;
                        pDstCur -= 4;
                        pPrevCur -= 4;
                        *(uint32_t *)pDstCur = *(uint32_t *)pPrevCur;
                    }
                }
                skipLeft = runLen & 3;
                if (skipLeft != 0) {
                    while (skipLeft != 0) {
                        skipLeft--;
                        pDstCur[-1] = pPrevCur[-1];
                        pPrevCur--;
                        pDstCur--;
                    }
                }
            }
        } while (1);
    }
    return;
#endif
}


/* The i386 version of BackwardHotspotRleBlitRow: the same six arguments and opcodes, with the packed
 * pixels moved a byte at a time instead of by dword. */
DF_NAKED void BackwardHotspotRleBlitRow386(uint8_t *pbRle, short skip, short count, short width,
                                  uint8_t *pbDst, uint8_t *pbPrev) /* 0x00431470 */
{
#ifdef _MSC_VER
    /* GEN-ASM-ARM BEGIN -- the instruction sequence transcribed from TI.EXE; do not hand-edit */
    __asm {
        push ebp
        mov ebp,esp
        push ebx
        push esi
        push edi
        pushad
        push ebp
        movsx eax,WORD PTR [ebp+0x14]
        mov esi,DWORD PTR [ebp+0x8]
        mov edi,DWORD PTR [ebp+0x18]
        mov ebx,DWORD PTR [ebp+0x1c]
        add edi,eax
        add ebx,eax
        movsx edx,WORD PTR [ebp+0xc]
        sub edx,eax
        neg edx
        jle L_1c2
        movsx ecx,WORD PTR [ebp+0x10]
        sub ecx,eax
        xor eax,eax
        neg ecx
        jle L_83
        sub edx,ecx
        sub edi,ecx
        sub ebx,ecx
    L_39:
        lodsb
        shr eax,0x1
        jb L_5e
        shr eax,0x1
        jb L_4e
        sub ecx,eax
        jg L_39
        mov eax,ecx
        neg eax
        jmp L_92
    L_4e:
        sub ecx,eax
        jle L_55
        inc esi
        jmp L_39
    L_55:
        mov eax,ecx
        neg eax
        jmp L_eb
    L_5e:
        shr eax,0x1
        jb L_70
        sub ecx,eax
        jg L_39
        mov eax,ecx
        neg eax
        jmp L_14c
    L_70:
        cmp ecx,eax
        jbe L_7a
        sub ecx,eax
        add esi,eax
        jmp L_39
    L_7a:
        sub eax,ecx
        add esi,ecx
        jmp L_15b
    L_83:
        lodsb
        shr eax,0x1
        jb L_147
        shr eax,0x1
        jb L_eb
    L_92:
        cmp edx,eax
        jbe L_c2
        sub edx,eax
        xchg ebx,esi
        std
        mov ecx,eax
        shr ecx,0x2
        je L_b0
        sub esi,0x4
        sub edi,0x4
        rep movsd
        add esi,0x4
        add edi,0x4
    L_b0:
        mov ecx,eax
        and ecx,0x3
        je L_bd
        dec esi
        dec edi
        rep movsb
        inc esi
        inc edi
    L_bd:
        cld
        xchg ebx,esi
        jmp L_83
    L_c2:
        std
        mov esi,ebx
        mov ecx,edx
        shr ecx,0x2
        je L_da
        sub esi,0x4
        sub edi,0x4
        rep movsd
        add esi,0x4
        add edi,0x4
    L_da:
        mov ecx,edx
        and ecx,0x3
        je L_e5
        dec esi
        dec edi
        rep movsb
    L_e5:
        cld
        jmp L_1c2
    L_eb:
        cmp edx,eax
        jbe L_122
        mov ecx,eax
        mov ebp,eax
        sub edx,eax
        lodsb
        std
        and ecx,0x3
        sub ebx,ebp
        dec edi
        rep stosb
        inc edi
        shr ebp,0x2
        je L_11c
        mov ah,al
        mov cx,ax
        shl eax,0x10
        mov ax,cx
        mov ecx,ebp
        sub edi,0x4
        rep stosd
        add edi,0x4
        xor eax,eax
    L_11c:
        cld
        jmp L_83
    L_122:
        mov ecx,edx
        lodsb
        std
        and ecx,0x3
        dec edi
        rep stosb
        inc edi
        shr edx,0x2
        je L_144
        mov ecx,edx
        mov ah,al
        mov dx,ax
        shl eax,0x10
        mov ax,dx
        sub edi,0x4
        rep stosd
    L_144:
        cld
        jmp L_1c2
    L_147:
        shr eax,0x1
        jb L_15b
    L_14c:
        cmp edx,eax
        jbe L_1c2
        sub edi,eax
        sub ebx,eax
        sub edx,eax
        jmp L_83
    L_15b:
        cmp edx,eax
        jbe L_196
        mov ecx,eax
        sub ebx,eax
        mov ebp,eax
        shr ecx,0x2
        sub edx,eax
        and ebp,0x3
        jecxz L_186
    L_16f:
        lodsb
        mov BYTE PTR [edi-0x1],al
        lodsb
        mov BYTE PTR [edi-0x2],al
        lodsb
        mov BYTE PTR [edi-0x3],al
        lodsb
        mov BYTE PTR [edi-0x4],al
        sub edi,0x4
        loop L_16f
        xor eax,eax
    L_186:
        mov ecx,ebp
        jecxz L_191
    L_18a:
        lodsb
        mov BYTE PTR [edi-0x1],al
        dec edi
        loop L_18a
    L_191:
        jmp L_83
    L_196:
        mov ecx,edx
        shr ecx,0x2
        and edx,0x3
        jecxz L_1b5
    L_1a0:
        lodsb
        mov BYTE PTR [edi-0x1],al
        lodsb
        mov BYTE PTR [edi-0x2],al
        lodsb
        mov BYTE PTR [edi-0x3],al
        lodsb
        mov BYTE PTR [edi-0x4],al
        sub edi,0x4
        loop L_1a0
    L_1b5:
        mov ecx,edx
        jecxz L_1c2
    L_1b9:
        lodsb
        mov BYTE PTR [edi-0x1],al
        dec edi
        loop L_1b9
        jmp L_1c2
    L_1c2:
        pop ebp
        popad
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
    /* GEN-ASM-ARM END */
#else
    uint8_t  opcode;
    int      underflow;
    int      edgeDelta;
    uint32_t tokenRun;
    uint32_t skipLeft;
    uint32_t pixelsLeft;
    uint8_t *pPrevCur;
    uint8_t *pRleCur;
    uint8_t *pDstCur;
    uint32_t runLen;


    edgeDelta   = (int)width;
    pDstCur = pbDst + edgeDelta;
    pPrevCur  = pbPrev + edgeDelta;
    pixelsLeft   = (uint32_t)(-((int)skip - edgeDelta));
    if (pixelsLeft == 0 || (int)skip - edgeDelta > 0)
        return;

    edgeDelta = (int)count - edgeDelta;
    runLen = 0;
    if (-edgeDelta == 0 || edgeDelta > 0)
        goto LAB_004314f3;

    pixelsLeft += (uint32_t)edgeDelta;
    pDstCur += edgeDelta;
    pPrevCur += edgeDelta;
    skipLeft = (uint32_t)(-edgeDelta);
    pRleCur = pbRle;

    while (1) {
        while (1) {
            while (1) {
                pbRle = pRleCur + 1;
                opcode     = *pRleCur;
                if ((opcode & 1) == 0)
                    break;
                tokenRun = (uint32_t)(opcode >> 2);
                if ((opcode & 2) == 0) {
                    runLen = skipLeft - tokenRun;
                    underflow = (int32_t)skipLeft < (int32_t)tokenRun;
                    skipLeft = runLen;
                    pRleCur = pbRle;
                    if (runLen == 0 || underflow) {
                        runLen = (uint32_t)(-(int32_t)runLen);
                        goto LAB_004315bc;
                    }
                } else {
                    if (skipLeft <= tokenRun) {
                        runLen = tokenRun - skipLeft;
                        pRleCur = pbRle + skipLeft;
                        goto LAB_004315cb;
                    }
                    skipLeft -= tokenRun;
                    pRleCur = pbRle + tokenRun;
                }
            }
            runLen = (uint32_t)(opcode >> 2);
            if ((opcode & 2) != 0)
                break;
            tokenRun = skipLeft - runLen;
            underflow = (int32_t)skipLeft < (int32_t)runLen;
            skipLeft = tokenRun;
            pRleCur = pbRle;
            if (tokenRun == 0 || underflow) {
                runLen = (uint32_t)(-(int32_t)tokenRun);
                goto LAB_00431502;
            }
        }
        tokenRun = skipLeft - runLen;
        if (tokenRun == 0 || (int32_t)skipLeft < (int32_t)runLen)
            break;
        skipLeft   = tokenRun;
        pRleCur += 2;
    }

    tokenRun   = (uint32_t)(-(int32_t)tokenRun);
    pRleCur = pbRle;

    do {
        if (pixelsLeft <= tokenRun) {
            opcode = *pRleCur;
            for (runLen = pixelsLeft & 3; runLen != 0; runLen--) {
                pDstCur--;
                *pDstCur = opcode;
            }
            pixelsLeft >>= 2;
            if (pixelsLeft != 0) {
                for (; pixelsLeft != 0; pixelsLeft--) {
                    pDstCur -= 4;
                    *(uint32_t *)pDstCur = DWORD_FROM_BYTE(opcode);
                }
            }
            return;
        }
        pixelsLeft -= tokenRun;
        pbRle = pRleCur + 1;
        opcode   = *pRleCur;
        runLen   = CONCAT31_SHIFT(tokenRun, opcode);
        pPrevCur -= tokenRun;
        for (skipLeft = tokenRun & 3; skipLeft != 0; skipLeft--) {
            pDstCur--;
            *pDstCur = opcode;
        }
        tokenRun >>= 2;
        if (tokenRun != 0) {
            for (; tokenRun != 0; tokenRun--) {
                pDstCur -= 4;
                *(uint32_t *)(pDstCur) = DWORD_FROM_BYTE(opcode);
            }
            runLen = 0;
        }
    LAB_004314f3:
        while (1) {
            while (1) {
                pRleCur   = pbRle + 1;
                opcode     = *pbRle;
                tokenRun     = CONCAT31_SHIFT(runLen, opcode);
                pbRle   = pRleCur;
                if ((opcode & 1) == 0)
                    break;
                runLen = tokenRun >> 2;
                if ((opcode & 2) == 0) {
                LAB_004315bc:
                    if (pixelsLeft <= runLen)
                        return;
                    pDstCur -= runLen;
                    pPrevCur -= runLen;
                    pixelsLeft -= runLen;
                } else {
                LAB_004315cb:
                    if (pixelsLeft <= runLen) {
                        uint32_t ru = pixelsLeft & 3;
                        for (pixelsLeft >>= 2; pixelsLeft != 0; pixelsLeft--) {
                            pDstCur[-1] = pRleCur[0];
                            pDstCur[-2] = pRleCur[1];
                            pDstCur[-3] = pRleCur[2];
                            pDstCur[-4] = pRleCur[3];
                            pRleCur += 4;
                            pDstCur -= 4;
                        }
                        for (; ru != 0; ru--) {
                            pDstCur--;
                            *pDstCur = *pRleCur++;
                        }
                        return;
                    }
                    pPrevCur -= runLen;
                    {
                        uint32_t uVar6w = runLen >> 2;
                        pixelsLeft -= runLen;
                        skipLeft = runLen & 3;
                        pbRle = pRleCur;
                        if (uVar6w != 0) {
                            do {
                                pDstCur[-1] = pRleCur[0];
                                pDstCur[-2] = pRleCur[1];
                                pDstCur[-3] = pRleCur[2];
                                pDstCur[-4] = pRleCur[3];
                                pRleCur += 4;
                                pDstCur -= 4;
                                uVar6w--;
                            } while (uVar6w != 0);
                            runLen    = 0;
                            pbRle = pRleCur;
                        }
                        for (; skipLeft != 0; skipLeft--) {
                            runLen       = CONCAT31_SHIFT(runLen, *pbRle);
                            pDstCur--;
                            *pDstCur = *pbRle;
                            pbRle++;
                        }
                    }
                }
            }
            tokenRun >>= 2;
            runLen = tokenRun;
            if ((opcode & 2) != 0)
                break;
        LAB_00431502:
            if (pixelsLeft <= runLen) {
                runLen = pixelsLeft >> 2;
                if (runLen != 0) {
                    while (runLen != 0) {
                        runLen--;
                        pDstCur -= 4;
                        pPrevCur -= 4;
                        *(uint32_t *)pDstCur = *(uint32_t *)pPrevCur;
                    }
                }
                pixelsLeft &= 3;
                if (pixelsLeft == 0)
                    return;
                while (pixelsLeft != 0) {
                    pDstCur--;
                    pPrevCur--;
                    *pDstCur = *pPrevCur;
                    pixelsLeft--;
                }
                return;
            }
            pixelsLeft -= runLen;
            skipLeft = runLen >> 2;
            if (skipLeft != 0) {
                while (skipLeft != 0) {
                    skipLeft--;
                    pDstCur -= 4;
                    pPrevCur -= 4;
                    *(uint32_t *)pDstCur = *(uint32_t *)pPrevCur;
                }
            }
            skipLeft = runLen & 3;
            if (skipLeft != 0) {
                while (skipLeft != 0) {
                    skipLeft--;
                    pDstCur--;
                    pPrevCur--;
                    *pDstCur = *pPrevCur;
                }
            }
        }
    } while (1);
#endif
}

