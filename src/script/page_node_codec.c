/* page_node_codec.c -- TI.EXE 0x00441c00..0x00441ca0: the rect copy and the page-node image codec,
 * both assembly in TI.EXE. */

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

/* The functions with a GEN-ASM-ARM block were assembly in TI.EXE: under VC++ they
 * are naked and the block is their whole body; GCC compiles the C arm. The VC++
 * definition comes last: compile_compare.py's harness drops the conditionals. */
#ifndef _MSC_VER
#define DF_NAKED
#else
#define DF_NAKED __declspec(naked)
#endif

#ifndef _MSC_VER   /* the C arm's helpers; the VC++ arm is the original assembly */
/* PORT-HELPER: DecodePageNodeImage's `movs` copies -- a byte, then a word, then
 * dwords, each unit read before it is written (0x00441f59 and the three like it). A
 * plain memcpy would differ only for an overlapping back-reference closer than 4 bytes. */
static void MovsCopy(byte *dst, const byte *src, int n)
{
    unsigned short wv;
    unsigned int   dv;

    if (n & 1) {
        *dst++ = *src++;
    }
    if (n & 2) {
        memcpy(&wv, src, 2);  memcpy(dst, &wv, 2);  dst += 2;  src += 2;
    }
    for (n >>= 2; n != 0; n--) {
        memcpy(&dv, src, 4);  memcpy(dst, &dv, 4);  dst += 4;  src += 4;
    }
}

/* PORT-HELPER: the bit-plane delta section of DecodePageNodeImage (0x00441fd7 ..
 * 0x004420b8). `n` pixels, each predicted from `ref` (the previous pixel, or the
 * reference row), coded MSB-first in big-endian words:
 *     1                  the reference pixel as it is
 *     0{z} 1 s           z = 1..7 zeros: reference + z (s = 1) or - z (s = 0)
 *     00000000 dddddddd  reference + d
 * The original keeps a 16-bit window (ax) and the next word (dx) with `left` valid
 * bits, and at the end hands the whole bytes still unread back to the stream.
 * Returns the resumed stream pointer. */
static const byte *DecodeDeltaPixels(const byte *src, byte *dst, const byte *ref, int n)
{
    unsigned int win, next;
    int          left, k, p, mag;

    win  = ((unsigned)src[0] << 8) | src[1];
    next = ((unsigned)src[2] << 8) | src[3];
    src += 4;
    left = 16;
    do {
        if (win & 0x8000) {                                 /* 1 */
            *dst++ = *ref++;
            k = 1;
        } else if ((win & 0xff00) == 0) {                   /* 00000000 dddddddd */
            *dst++ = (byte)(*ref++ + (win & 0xff));
            k = 16;
        } else {                                            /* 0{z} 1 s */
            for (p = 14; !(win & (1u << p)); p--)
                ;
            mag  = kDeltaPixelMagnitudes[p - 1];                     /* = z */
            *dst = (byte)(*ref++ + ((win & (1u << (p - 1))) ? mag : -mag));
            dst++;
            k = mag + 2;
        }
        /* consume k bits: shift the window, refilling from `next` and the stream */
        if (left >= k) {
            win  = ((win << k) | (next >> (16 - k))) & 0xffff;
            next = (next << k) & 0xffff;
            left -= k;
        } else {
            win  = ((win << left) | (next >> (16 - left))) & 0xffff;
            k   -= left;
            next = ((unsigned)src[0] << 8) | src[1];
            src += 2;
            win  = ((win << k) | (next >> (16 - k))) & 0xffff;
            next = (next << k) & 0xffff;
            left = 16 - k;
        }
        if (left == 0) {
            next = ((unsigned)src[0] << 8) | src[1];
            src += 2;
            left = 16;
        }
    } while (--n != 0);
    /* the window and `next` hold 16 + left unread bits: give the whole bytes back */
    src -= 2;
    if (left >= 8)
        src--;
    if (left >= 16)
        src--;
    return src;
}
#endif /* !_MSC_VER */


/* Copies the rect pRect {y1, x1, y2, x2} of 8-bit pixels from pSrc into pDst, each buffer
 * with its own `w` as the row stride.
 *
 * Port note: the first argument is the source. The port once had them swapped, so
 * RestoreDirtyRectFromBackdrop wiped the stored room image instead of restoring from it. */
void CopyHotspotRectBetweenBuffers(HotspotBuf *pSrc, HotspotBuf *pDst, short *pRect) /* 0x00441c00 */
#ifdef _MSC_VER
{
    /* The original is inline assembly (a rep movsd / rep movsb row loop) fed from six frame
     * locals; the #else C is its decode for GCC. The strides are the signed `w` words. */
    char *dst, *src;
    int y1, x1, srcStride, dstStride;
    int width, rows, dstSkip, srcSkip;

    x1 = pRect[1];
    y1 = pRect[0];
    srcStride = pSrc->w;
    dstStride = pDst->w;
    width = pRect[3] - x1;
    rows = pRect[2] - y1;
    if (width > 0 && rows > 0) {
        srcSkip = srcStride - width;
        dstSkip = dstStride - width;
        src = (char *)pSrc->pPx + y1 * srcStride + x1;
        dst = (char *)pDst->pPx + y1 * dstStride + x1;
        __asm {
            pushad
            mov  esi, src
            mov  edi, dst
            mov  eax, srcSkip
            mov  ebx, rows
            mov  edx, dstSkip
        row:
            mov  ecx, width
            shr  ecx, 2
            rep  movsd
            mov  ecx, width
            and  ecx, 3
            rep  movsb
            add  esi, eax
            add  edi, edx
            dec  ebx
            jnz  row
            popad
        }
    }
}
#else
{
    int          y1, x1, y2, x2, y;
    int          strideDst, strideSrc, span;
    unsigned char *bd;
    unsigned char *bs;

    y1 = (int)pRect[0];
    x1 = (int)pRect[1];
    y2 = (int)pRect[2];
    x2 = (int)pRect[3];
    if (y1 >= y2 || x1 >= x2)
        return;

    strideDst = (int)(unsigned short)pDst->w;
    strideSrc = (int)(unsigned short)pSrc->w;
    span      = x2 - x1;

    for (y = y1; y < y2; y++) {
        bs = (unsigned char *)pSrc->pPx + y * strideSrc + x1;
        bd = (unsigned char *)pDst->pPx + y * strideDst + x1;
        memcpy(bd, bs, (size_t)span);
    }
}
#endif




/* The page-node image codec: decodes one image resource into pDst (stride
 * pDst->w, which may be negative) and returns the stream bytes consumed.
 * Header: rows, then the bytes per row. Each row starts with an opcode, a
 * multiple of 4 (the original's jump-table stride):
 *   4         the row straight from the stream
 *   8..36     RLE against the row 4,3,2,1 above / 1,2,3,4 below
 *   40        leave the row as it is
 *   44..72    copy the row 4,3,2,1 above / 1,2,3,4 below
 * An RLE row is a sequence of byte-prefixed runs (bits 0..2 the kind, bits
 * 3..7 the count, 0 = the next byte + 32), each advancing the reference-row
 * cursor in step: repeat the previous pixel, skip, fill, literal, copy from
 * the reference row, copy from `dist` bytes back, or a delta-coded run
 * (DecodeDeltaPixels) against the previous pixel or the reference row.
 * */
DF_NAKED int __cdecl DecodePageNodeImage(int *pHandle, HotspotBuf *pDst)  /* 0x00441ca0 */
{
#ifdef _MSC_VER
    /* GEN-ASM-ARM BEGIN -- the instruction sequence transcribed from TI.EXE; do not hand-edit */
    __asm {
        push ebp
        mov eax,DWORD PTR [esp+0x8]
        mov ebp,esp
        sub esp,0x2c
        push ebx
        push esi
        push edi
        push eax
        call GetResDataPtr
        add esp,0x4
        mov DWORD PTR [ebp-0xc],eax
        mov DWORD PTR [ebp-0x1c],eax
        mov eax,DWORD PTR [ebp+0xc]
        movsx edx,WORD PTR [eax+0x2]
        mov ecx,DWORD PTR [eax+0x4]
        mov DWORD PTR [ebp-0x4],edx
        add edx,edx
        mov eax,DWORD PTR [ebp-0x4]
        add eax,edx
        mov DWORD PTR [ebp-0x2c],ecx
        mov ecx,DWORD PTR [ebp-0xc]
        mov DWORD PTR [ebp-0x14],edx
        mov DWORD PTR [ebp-0x10],eax
        add eax,DWORD PTR [ebp-0x4]
        mov DWORD PTR [ebp-0x18],eax
        movsx edx,WORD PTR [ecx]
        add DWORD PTR [ebp-0xc],0x2
        mov ecx,DWORD PTR [ebp-0xc]
        mov DWORD PTR [ebp-0x28],edx
        movsx eax,WORD PTR [ecx]
        add DWORD PTR [ebp-0xc],0x2
        mov DWORD PTR [ebp-0x8],eax
        sar eax,0x2
        mov DWORD PTR [ebp-0x24],eax
        mov eax,DWORD PTR [ebp-0x8]
        and eax,0x2
        mov DWORD PTR [ebp-0x20],eax
        pushad
        push ds
        pop es
        mov esi,DWORD PTR [ebp-0xc]
        mov edi,DWORD PTR [ebp-0x2c]
        mov ecx,DWORD PTR [ebp-0x28]
    L_74:
        xor eax,eax
        lodsb
        jmp DWORD PTR L_9d[eax*2]
    L_7e:
        loop L_74
        sub esi,DWORD PTR [ebp-0x1c]
        mov DWORD PTR [ebp-0x1c],esi
        popad
        mov eax,DWORD PTR [ebp+0x8]
        push eax
        call UnlockResObject
        add esp,0x4
        mov eax,DWORD PTR [ebp-0x1c]
        pop edi
        pop esi
        pop ebx
        mov esp,ebp
        pop ebp
        ret
    L_9d:
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        mov eax,DWORD PTR L_131
        nop
        nop
        nop
        mov eax,DWORD PTR L_14b
        nop
        nop
        nop
        mov eax,DWORD PTR L_155
        nop
        nop
        nop
        mov eax,DWORD PTR L_15f
        nop
        nop
        nop
        mov eax,DWORD PTR L_169
        nop
        nop
        nop
        mov eax,DWORD PTR L_170
        nop
        nop
        nop
        mov eax,DWORD PTR L_177
        nop
        nop
        nop
        mov eax,DWORD PTR L_17e
        nop
        nop
        nop
        mov eax,DWORD PTR L_185
        nop
        nop
        nop
        mov eax,DWORD PTR L_18c
        nop
        nop
        nop
        mov eax,DWORD PTR L_194
        nop
        nop
        nop
        mov eax,DWORD PTR L_19c
        nop
        nop
        nop
        mov eax,DWORD PTR L_1a4
        nop
        nop
        nop
        mov eax,DWORD PTR L_1ac
        nop
        nop
        nop
        mov eax,DWORD PTR L_1b4
        nop
        nop
        nop
        mov eax,DWORD PTR L_1bc
        nop
        nop
        nop
        mov eax,DWORD PTR L_1c4
        nop
        nop
        nop
        mov eax,DWORD PTR L_1cc
    L_131:
        push ecx
        mov ecx,DWORD PTR [ebp-0x24]
        rep movsd
        cmp DWORD PTR [ebp-0x20],0x0
        je L_13f
        movsw
    L_13f:
        pop ecx
        add edi,DWORD PTR [ebp-0x4]
        sub edi,DWORD PTR [ebp-0x8]
        jmp L_7e
    L_14b:
        mov ebx,edi
        sub ebx,DWORD PTR [ebp-0x18]
        jmp L_1ed
    L_155:
        mov ebx,edi
        sub ebx,DWORD PTR [ebp-0x10]
        jmp L_1ed
    L_15f:
        mov ebx,edi
        sub ebx,DWORD PTR [ebp-0x14]
        jmp L_1ed
    L_169:
        mov ebx,edi
        sub ebx,DWORD PTR [ebp-0x4]
        jmp L_1ed
    L_170:
        mov ebx,edi
        add ebx,DWORD PTR [ebp-0x4]
        jmp L_1ed
    L_177:
        mov ebx,edi
        add ebx,DWORD PTR [ebp-0x14]
        jmp L_1ed
    L_17e:
        mov ebx,edi
        add ebx,DWORD PTR [ebp-0x10]
        jmp L_1ed
    L_185:
        mov ebx,edi
        add ebx,DWORD PTR [ebp-0x18]
        jmp L_1ed
    L_18c:
        add edi,DWORD PTR [ebp-0x4]
        jmp L_7e
    L_194:
        push esi
        mov esi,edi
        sub esi,DWORD PTR [ebp-0x18]
        jmp L_1d2
    L_19c:
        push esi
        mov esi,edi
        sub esi,DWORD PTR [ebp-0x10]
        jmp L_1d2
    L_1a4:
        push esi
        mov esi,edi
        sub esi,DWORD PTR [ebp-0x14]
        jmp L_1d2
    L_1ac:
        push esi
        mov esi,edi
        sub esi,DWORD PTR [ebp-0x4]
        jmp L_1d2
    L_1b4:
        push esi
        mov esi,edi
        add esi,DWORD PTR [ebp-0x4]
        jmp L_1d2
    L_1bc:
        push esi
        mov esi,edi
        add esi,DWORD PTR [ebp-0x14]
        jmp L_1d2
    L_1c4:
        push esi
        mov esi,edi
        add esi,DWORD PTR [ebp-0x10]
        jmp L_1d2
    L_1cc:
        push esi
        mov esi,edi
        add esi,DWORD PTR [ebp-0x18]
    L_1d2:
        push ecx
        mov ecx,DWORD PTR [ebp-0x24]
        rep movsd
        cmp DWORD PTR [ebp-0x20],0x0
        je L_1e0
        movsw
    L_1e0:
        pop ecx
        pop esi
        add edi,DWORD PTR [ebp-0x4]
        sub edi,DWORD PTR [ebp-0x8]
        jmp L_7e
    L_1ed:
        push ecx
        mov edx,DWORD PTR [ebp-0x8]
    L_1f1:
        xor eax,eax
        lodsb
        shr eax,0x1
        jb L_293
        shr eax,0x1
        jb L_24f
        shr eax,0x1
        jb L_21f
        jne L_20d
        lodsb
        add eax,0x20
    L_20d:
        push ebp
        sub edx,eax
        add ebx,eax
        dec eax
        push edx
        mov ebp,eax
        push ebx
        mov ebx,edi
        movsb
        jmp L_337
    L_21f:
        jne L_225
        lodsb
        add eax,0x20
    L_225:
        mov ecx,eax
        add ebx,eax
        sub edx,eax
        mov al,BYTE PTR [edi-0x1]
        shr ecx,0x1
        jae L_234
        stosb
    L_234:
        je L_24a
        mov ah,al
        shr ecx,0x1
        jae L_23f
        stosw
    L_23f:
        je L_24a
        push ax
        shl eax,0x10
        pop ax
        rep stosd
    L_24a:
        jmp L_321
    L_24f:
        shr eax,0x1
        jb L_265
        jne L_25a
        lodsb
        add eax,0x20
    L_25a:
        add edi,eax
        add ebx,eax
        sub edx,eax
        jmp L_321
    L_265:
        jne L_26b
        lodsb
        add eax,0x20
    L_26b:
        mov ecx,eax
        add ebx,eax
        sub edx,eax
        lodsb
        shr ecx,0x1
        jae L_278
        stosb
    L_278:
        je L_28e
        mov ah,al
        shr ecx,0x1
        jae L_283
        stosw
    L_283:
        je L_28e
        push ax
        shl eax,0x10
        pop ax
        rep stosd
    L_28e:
        jmp L_321
    L_293:
        shr eax,0x1
        jb L_2d2
        shr eax,0x1
        jb L_2b3
        jne L_2a3
        lodsb
        add eax,0x20
    L_2a3:
        push ebp
        sub edx,eax
        push edx
        mov edx,ebx
        add edx,eax
        push edx
        mov ebp,eax
        jmp L_337
    L_2b3:
        jne L_2b9
        lodsb
        add eax,0x20
    L_2b9:
        mov ecx,eax
        add ebx,eax
        sub edx,eax
        shr ecx,0x1
        jae L_2c5
        movsb
    L_2c5:
        je L_2d0
        shr ecx,0x1
        jae L_2ce
        movsw
    L_2ce:
        rep movsd
    L_2d0:
        jmp L_321
    L_2d2:
        shr eax,0x1
        jb L_2fa
        jne L_2dd
        lodsb
        add eax,0x20
    L_2dd:
        push esi
        mov esi,ebx
        mov ecx,eax
        sub edx,eax
        shr ecx,0x1
        jae L_2ea
        movsb
    L_2ea:
        je L_2f5
        shr ecx,0x1
        jae L_2f3
        movsw
    L_2f3:
        rep movsd
    L_2f5:
        mov ebx,esi
        pop esi
        jmp L_321
    L_2fa:
        jne L_300
        lodsb
        add eax,0x20
    L_300:
        mov ecx,eax
        add ebx,eax
        sub edx,eax
        lodsw
        push esi
        mov esi,edi
        sub esi,eax
        shr ecx,0x1
        jae L_313
        movsb
    L_313:
        je L_31e
        shr ecx,0x1
        jae L_31c
        movsw
    L_31c:
        rep movsd
    L_31e:
        pop esi
        jmp L_321
    L_321:
        cmp edx,0x0
        jle L_32b
        jmp L_1f1
    L_32b:
        pop ecx
        sub edi,DWORD PTR [ebp-0x8]
        add edi,DWORD PTR [ebp-0x4]
        jmp L_7e
    L_337:
        lodsw
        xchg ah,al
        mov edx,eax
        lodsw
        xchg esi,ebx
        xchg ah,al
        xchg eax,edx
        push ebx
        mov ebx,0x10
        xor ecx,ecx
    L_34c:
        bsr cx,ax
        je L_386
        cmp ecx,0xf
        jne L_381
        movsb
        shld ax,dx,0x1
        dec ebx
        jne L_378
        dec ebp
        je L_3f2
        pop ebx
        mov dx,WORD PTR [ebx]
        add ebx,0x2
        xchg dh,dl
        push ebx
        mov ebx,0x10
        jmp L_34c
    L_378:
        dec bp
        je L_3f2
        shl edx,0x1
        jmp L_34c
    L_381:
        cmp ecx,0x8
        jge L_391
    L_386:
        add al,BYTE PTR [esi]
        stosb
        inc esi
        mov ecx,0x10
        jmp L_3ae
    L_391:
        dec ecx
        bt ax,cx
        mov cl,BYTE PTR kDeltaPixelMagnitudes[ecx]
        movsb
        jb L_3a4
        sub BYTE PTR [edi-0x1],cl
        jmp L_3a7
    L_3a4:
        add BYTE PTR [edi-0x1],cl
    L_3a7:
        add ecx,0x2
        cmp ebx,ecx
        jge L_3c8
    L_3ae:
        xchg ebx,ecx
        shld ax,dx,cl
        sub bx,cx
        pop ecx
        mov dx,WORD PTR [ecx]
        add ecx,0x2
        xchg dh,dl
        push ecx
        mov ecx,0x10
        xchg ebx,ecx
    L_3c8:
        shld ax,dx,cl
        sub ebx,ecx
        je L_3db
        shl dx,cl
        dec ebp
        je L_3f2
        jmp L_34c
    L_3db:
        pop ebx
        mov dx,WORD PTR [ebx]
        add ebx,0x2
        xchg dh,dl
        push ebx
        mov ebx,0x10
        dec ebp
        je L_3f2
        jmp L_34c
    L_3f2:
        mov ecx,ebx
        pop ebx
        sub ebx,0x2
        jecxz L_406
        cmp ecx,0x8
        jl L_406
        dec ebx
        cmp ecx,0x10
        jl L_406
        dec ebx
    L_406:
        mov esi,ebx
        pop ebx
        pop edx
        pop ebp
        jmp L_321
        xor eax,eax
        pop edi
        pop esi
        pop ebx
        mov esp,ebp
        pop ebp
        ret
    }
    /* GEN-ASM-ARM END */
#else
    static const signed char kRefRow[8] = { -4, -3, -2, -1, 1, 2, 3, 4 };
    const byte *start;
    const byte *src;
    byte       *dst;
    byte       *ref;
    int         w;
    int         rows;
    int         rowStep;
    int         copyLen;
    int         left;
    int         n;
    unsigned    a;
    int         op;

    start = src = (const byte *)GetResDataPtr(pHandle);
    w   = pDst->w;
    dst = (byte *)pDst->pPx;
    rows    = *(const short *)src;  src += 2;
    rowStep = *(const short *)src;  src += 2;
    copyLen = ((rowStep >> 2) << 2) + (rowStep & 2);    /* rep movsd, then one movsw */
    do {
        op = *src++;
        if (op == 4) {                                      /* the row from the stream */
            MovsCopy(dst, src, copyLen);
            src += copyLen;
            dst += copyLen + w - rowStep;
        } else if (op == 40) {                              /* keep the row */
            dst += w;
        } else if (op >= 44 && op <= 72 && (op & 3) == 0) { /* copy another row */
            ref = dst + kRefRow[op / 4 - 11] * w;
            MovsCopy(dst, ref, copyLen);
            dst += copyLen + w - rowStep;
        } else if (op >= 8 && op <= 36 && (op & 3) == 0) {  /* RLE against another row */
            ref  = dst + kRefRow[op / 4 - 2] * w;
            left = rowStep;
            do {
                a = *src++;
                n = (int)(a >> 3);
                if (n == 0)
                    n = *src++ + 0x20;
                switch (a & 7) {
                case 4:                                     /* repeat the previous pixel */
                    memset(dst, dst[-1], n);
                    dst += n;  ref += n;  left -= n;
                    break;
                case 0:                                     /* a literal, then deltas on it */
                    *dst = *src++;
                    src = DecodeDeltaPixels(src, dst + 1, dst, n - 1);
                    dst += n;  ref += n;  left -= n;
                    break;
                case 2:                                     /* skip */
                    dst += n;  ref += n;  left -= n;
                    break;
                case 6:                                     /* fill with the next byte */
                    memset(dst, *src++, n);
                    dst += n;  ref += n;  left -= n;
                    break;
                case 5:                                     /* literal run */
                    MovsCopy(dst, src, n);
                    src += n;  dst += n;  ref += n;  left -= n;
                    break;
                case 1:                                     /* deltas on the reference row */
                    src = DecodeDeltaPixels(src, dst, ref, n);
                    dst += n;  ref += n;  left -= n;
                    break;
                case 3:                                     /* copy from the reference row */
                    MovsCopy(dst, ref, n);
                    dst += n;  ref += n;  left -= n;
                    break;
                default:                                    /* 7: copy from `dist` bytes back */
                    MovsCopy(dst, dst - *(const unsigned short *)src, n);
                    src += 2;
                    dst += n;  ref += n;  left -= n;
                    break;
                }
            } while (left > 0);
            dst += w - rowStep;
        } else {
            /* unreachable from a well-formed stream: the original indexes its jump
             * table with the byte and lands in the padding */
            DF_TRACE("!! page codec: opcode %d at stream +%d", op, (int)(src - 1 - start));
            rows = 1;
        }
    } while (--rows != 0);
    UnlockResObject(pHandle);
    return (int)(src - start);
#endif
}
