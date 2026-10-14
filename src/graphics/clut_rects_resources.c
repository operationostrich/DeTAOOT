/* clut_rects_resources.c -- TI.EXE 0x00434cf0..0x00437410: CLUT palettes, scene rects, Pascal strings,
 * resource loading, viewport windows and the plugin directory. */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <mmsystem.h>
#include <stdlib.h>
#include "df_protos.h"
#include "topics/initclut.h"
#include "topics/freeclut.h"
#include "topics/reset_channel.h"
#include "topics/scene.h"
#include "topics/renderer.h"
#include "topics/strutil.h"
#include "topics/clut.h"
#include "topics/scriptprop.h"
#include "topics/input.h"
#include "topics/multimedia.h"
#include "topics/resource.h"
#include "topics/scriptcmd.h"


/* Loads the two palettes from the embedded resources ("System" -> CLUT.SYSTEM, "Black" ->
 * CLUT.BLACK), makes a clone of the system one the active CLUT, and seeds the random table
 * from the 60 Hz tick. Fatal 0x474 / 0x475 if either palette is missing. */
void __stdcall InitCLUTPalettes(void)  /* 0x00434cf0 */
{
    byte        *pArchive;
    unsigned int uColourParam;

    pArchive    = (byte*)UpdatePascalLength((char *)&kStrSystem);
    gPrimaryClut = LoadResourceByTag(CHUNK_TAG_CLUT, pArchive);
    if (gPrimaryClut == NULL)
    {
        FatalError(0, 0x474);
    }

    pArchive    = (byte*)UpdatePascalLength((char *)&kStrBlack);
    gSecondaryClut = LoadResourceByTag(CHUNK_TAG_CLUT, pArchive);
    if (gSecondaryClut == NULL)
    {
        FatalError(0, 0x475);
    }

    gActiveClut = gPrimaryClut;
    CloneResource(&gActiveClut);

    uColourParam = GetTick60Hz();
    SeedRandomTable(uColourParam);

    gClutReady = 1;
}


/* Releases both loaded palettes and the active clone. */
void __stdcall FreeCLUTPalettes(void)  /* 0x00434d90 */
{
    ReleaseResource(gPrimaryClut, CHUNK_TAG_CLUT);
    ReleaseResource(gSecondaryClut, CHUNK_TAG_CLUT);
    FreeResObject(gActiveClut);
}


/* Press feedback for a rectangular hotspot: draws an XOR frame (pen size 3, mode 1) around pRect
 * and toggles it as the cursor leaves and re-enters while the button is held. Holds the frame at
 * least 10 ticks (1/6 s at 60 Hz), then erases it if the cursor is inside. Returns 1 if the
 * button was released inside the rect, else 0. */
ushort TrackHotspotRectPress(short *pRect) /* 0x00434dd0 */
{
    int           deadline;       /* formed up front: lea edi,[eax+0xa] */
    short         inside;         /* declared after deadline: inside in bx (xor bl,1) */
    ScenePointYX  pt;

    inside = 1;
    QdPenSize(3);
    QdPenMode(1);
    deadline = GetTick60Hz() + 10;
    QdFrameRect(pRect);
    while (IsMouseStillDown() != 0) {
        GetSceneCursorPos(0, (short *)&pt);
        /* the hit status is compared as returned (cmp ax,bx) */
        if (PointInSceneRect(pt, pRect) != inside) {
            QdFrameRect(pRect);
            inside ^= 1;
        }
    }
    while ((int)GetTick60Hz() < deadline)
        ;
    if (inside != 0)
        QdFrameRect(pRect);
    QdPenNormal();
    return inside;
}


/* Intersection of two {y1,x1,y2,x2} rects, written to pOutRect as two dwords. Returns 1 if the
 * result is non-empty (strictly y1 < y2 and x1 < x2); otherwise clears pOutRect and returns 0. */
short IntersectSceneRects(short *pRectA, short *pRectB, int *pOutRect) /* 0x00434e70 */
{
    short r[4];   /* the intersection is built in a copy of the second rect */

    *(SceneViewport *)r = *(SceneViewport *)pRectB;
    if (pRectB[0] < pRectA[0]) r[0] = pRectA[0];
    if (pRectB[1] < pRectA[1]) r[1] = pRectA[1];
    if (pRectB[2] > pRectA[2]) r[2] = pRectA[2];
    if (pRectB[3] > pRectA[3]) r[3] = pRectA[3];
    if (r[2] > r[0] && r[1] < r[3]) {
        *(SceneViewport *)pOutRect = *(SceneViewport *)r;
        return 1;
    }
    r[0] = 0; r[1] = 0; r[2] = 0; r[3] = 0;
    *(SceneViewport *)pOutRect = *(SceneViewport *)r;
    return 0;
}


/* Non-zero iff the two {y1,x1,y2,x2} rects are equal. */
short AreSceneRectsIdentical(short *rA, short *rB) /* 0x00434f30 */
{
    if (rA[0] != rB[0]) return 0;
    if (rA[1] != rB[1]) return 0;
    if (rA[2] != rB[2]) return 0;
    return rA[3] == rB[3];
}


/* Tests a scene-space point against a {y1,x1,y2,x2} rect, half-open: x1 <= X < x2 and
 * y1 <= Y < y2. The point is the packed dword GetSceneCursorPos writes (low word Y, high word X). */
short __cdecl PointInSceneRect(ScenePointYX pt, const short *pRect)  /* 0x00434f80 */
{
    if (pt.x < pRect[1])      return 0;
    if (pRect[3] <= pt.x)     return 0;
    if (pt.y < pRect[0])      return 0;
    return (pt.y < pRect[2]) ? 1 : 0;     /* mov ax,0 / jle / mov ax,1 */
}

#undef local_a4
#undef local_92
#undef local_90
#undef local_8e
#undef local_8c
#undef local_8a
#undef local_88
#undef local_7a

/* Bounding union of two {y1,x1,y2,x2} rects (min of y1/x1, max of y2/x2), written to pOutRect as
 * two dwords. AddDirtyRect uses it to merge a rect into an overlapping gTransitionRects slot. */
void UnionSceneRects(short *pRectA, short *pRectB, unsigned int *pOutRect) /* 0x00434fc0 */
{
    short r[4];   /* the union is built in a copy of the second rect */

    *(SceneViewport *)r = *(SceneViewport *)pRectB;
    if (pRectB[0] > pRectA[0]) r[0] = pRectA[0];
    if (pRectB[1] > pRectA[1]) r[1] = pRectA[1];
    if (pRectB[2] < pRectA[2]) r[2] = pRectA[2];
    if (pRectB[3] < pRectA[3]) r[3] = pRectA[3];
    *(SceneViewport *)pOutRect = *(SceneViewport *)r;
}


/* Non-zero if the {y1,x1,y2,x2} rect has no area (x2 <= x1 or y2 <= y1). */
short IsSceneRectEmpty(const short *r)  /* 0x00435030 */
{
    if (r[3] <= r[1])
        return 1;
    if (r[2] <= r[0])
        return 1;
    return 0;
}


/* Moves a {y1,x1,y2,x2} rect: dx is added to rect[0] and rect[2], dy to rect[1] and rect[3]
 * (Ghidra's parameter names, kept as decoded). */
void OffsetSceneRect(short *rect, short dy, short dx) /* 0x00435060 */
{
    rect[0] = (short)(rect[0] + dx);
    rect[1] = (short)(rect[1] + dy);
    rect[2] = (short)(rect[2] + dx);
    rect[3] = (short)(rect[3] + dy);
}


/* Shrinks a {y1,x1,y2,x2} rect inward by dx horizontally and dy vertically. */
void __cdecl InsetSceneRect(short *pRect, short dx, short dy)  /* 0x00435080 */
{
    pRect[0] += dy;
    pRect[1] += dx;
    pRect[2] -= dy;
    pRect[3] -= dx;
}


/* Maps pRect from the coordinate frame pFromFrame into pToFrame (both {y1,x1,y2,x2}): each edge is
 * translated, or scaled by toExtent/fromExtent when the two frames differ in size on that axis. */
void __cdecl MapRectangleLinearShort(short *pRect, short *pFromFrame, short *pToFrame) /* 0x004350A0 */
{
    short fromMin;
    short toMin;
    short fromExtent;
    short toExtent;

    fromMin = *pFromFrame;
    fromExtent = (short)(pFromFrame[2] - fromMin);
    toMin = *pToFrame;
    toExtent = (short)(pToFrame[2] - toMin);
    if (toExtent == fromExtent) {               /* cmp ax,si, as the exe compares */
        *pRect = (short)(*pRect + (toMin - fromMin));
        pRect[2] = (short)(pRect[2] + (*pToFrame - *pFromFrame));
    } else {
        *pRect = (short)(((int)(short)(*pRect - fromMin) * (int)toExtent) / (int)fromExtent) + toMin;
        pRect[2] = (short)(*pToFrame +
            (short)(((int)(short)(pRect[2] - *pFromFrame) * (int)toExtent) / (int)fromExtent));
    }
    fromMin = pFromFrame[1];
    fromExtent = (short)(pFromFrame[3] - fromMin);
    toMin = pToFrame[1];
    toExtent = (short)(pToFrame[3] - toMin);
    if (toExtent == fromExtent) {
        pRect[1] = (short)(pRect[1] + (toMin - fromMin));
        pRect[3] = (short)(pRect[3] + (pToFrame[1] - pFromFrame[1]));
        return;
    }
    pRect[1] = (short)(((int)(short)(pRect[1] - fromMin) * (int)toExtent) / (int)fromExtent) + toMin;
    pRect[3] = (short)(pToFrame[1] +
        (short)(((int)(short)(pRect[3] - pFromFrame[1]) * (int)toExtent) / (int)fromExtent));
}


/* Case-insensitive equality of two Pascal strings (length byte first), folding each byte through
 * kCaseFoldTable (256 entries). A length mismatch is rejected up front. Returns 1 if equal. */
short __cdecl PascalStringsEqualNoCase(byte *pA, byte *pB)  /* 0x004351a0 */
{
    short len;                          /* movzx cx: a 16-bit count */
    byte *a;
    byte *b;

    len = *pA;
    a = pA + 1;
    b = pB + 1;
    if (*pB != len)
        return 0;
    while (len-- != 0) {
        if (kCaseFoldTable[*a++] != kCaseFoldTable[*b++])
            return 0;
    }
    return 1;
}


/* Case-insensitive equality of two raw byte runs of byteCount bytes (no length byte), through
 * kCaseFoldTable like PascalStringsEqualNoCase. Returns 1 if they match. */
short __cdecl CompareRawByteSlicesCaseFolded(byte *pA, byte *pB, unsigned short byteCount)  /* 0x00435200 */
{
    unsigned short n;
    byte *a;
    byte *b;

    /* short: `xor ax,ax` / `mov ax,1` in the exe */
    n = byteCount;
    a = pA;
    b = pB;
    while (n-- != 0) {
        if (kCaseFoldTable[*a++] != kCaseFoldTable[*b++])
            return 0;
    }
    return 1;
}


/* Appends the body of Pascal string pSrc to pDst. Fatal 0x6E / 0x478 if the combined length
 * would exceed 255. */
void __cdecl AppendPascalString(byte *pSrc, byte *pDst)  /* 0x00435250 */
{
    short  n;
    int    dstLen;
    byte  *pLen;
    int    len;

    /* dstLen is a zeroed dword with its low byte stored through a pointer (the exe's
     * mov [esp],0 / mov al,[edi] / mov [esp+0xc],al). Taking its address keeps the reload
     * `len = dstLen` from sinking past the *pDst store, so C2 emits `pDst = len + 1 + pDst` as
     * lea [eax+edi+1] rather than inc eax / add edi,eax. */
    dstLen = 0;
    pLen = (byte *)&dstLen;
    n = *pSrc;
    *pLen = *pDst;
    if (n + dstLen > 0xFF)
    {
        DF_TRACE("!! AppendPascalString overflow: pSrc=%p srcLen=%d '%.*s'  "
                 "pDst=%p dstLen=%d '%.*s'",
                 pSrc, (int)n, (int)n, (char *)(pSrc + 1),
                 pDst, dstLen, dstLen, (char *)(pDst + 1));
        FatalError(0x6E, 0x478);
    }
    pSrc++;
    len = dstLen;
    *pDst += (byte)n;
    pDst = len + 1 + pDst;
    for (; n > 0; n--)
        *pDst++ = *pSrc++;
}


/* Copies a Pascal string (length byte plus body, N+1 bytes) from pSrc to pDst. */
void __cdecl CopyPascalString(byte *pSrc, byte *pDst)  /* 0x004352b0 */
{
    short count;
    byte *s;
    byte *d;

    /* entry-tested loop (test ax,ax / jl at 0x004352b9); pDst is loaded only past the test */
    s = pSrc;
    count = *s;
    if (count >= 0) {
        d = pDst;
        do {
            *d++ = *s++;
        } while (--count >= 0);
    }
}


/* Converts a C string to a Pascal string: length byte, then the body. */
void __cdecl EncodePascalString(char *pCStr, undefined1 *pPascal)  /* 0x004352D0 */
{
  char ch;
  size_t _Size;

  _Size = 0;
  ch = *pCStr;
  while (ch != '\0') {
    _Size = _Size + 1;
    ch = pCStr[_Size];
  }
  /* memmove, not memcpy: the exe CALLs the CRT copy at 0x0044e440, and /O2 inlines memcpy as
   * rep movsd but never memmove. Mac heritage: BlockMove semantics. The same holds for every
   * block copy below. */
  memmove(pPascal + 1, pCStr, _Size);
  *pPascal = (char)_Size;
  return;
}


/* Copies a Pascal string's body to pCStr and NUL-terminates it. */
void __cdecl PascalStringToCString(byte *pPascal, void *pCStr)  /* 0x00435300 */
{
  byte len;

  len = *pPascal;
  memmove(pCStr, pPascal + 1, (uint32_t)len);
  ((byte *)pCStr)[len] = 0;
  return;
}


/* Seeds the 55-entry random table gRandTable (0x0048aa10) for palette animation: [0] = seed mod
 * 65535, then a[i] = (a[i-1] * 31 + 1) mod 65535 through [54], and the generator's index word
 * (0x0048aaec, just past the table) is zeroed.
 *
 * Port note: an early decode stopped the chain at [53], wrote 0 into [54] (misreading the index
 * store as the table's last element) and never reset the index. */
void __cdecl SeedRandomTable(unsigned int seed)  /* 0x00435330 */
{
    unsigned int *p;

    gRandTable[0] = seed % 0xffff;
    for (p = &gRandTable[1]; p < &gRandTable[55]; p++)
        *p = (p[-1] * 31 + 1) % 0xffff;

    /* the exe resets the index with `mov word ptr [0x48aaec],0` */
    *(short *)&gRandIndex = 0;
}


/* Random value in 1..limit, or 0 when limit <= 0. */
int __cdecl RandomUpTo(uint limit)  /* 0x00435380 */
{
    uint randomValue;

    if ((int)limit <= 0) {
        return 0;
    }
    randomValue = GenerateNextPseudoRandomValue();
    return (int)(randomValue % limit) + 1;
}


/* Next 32-bit value from the lagged table gRandTable[55]: two successive steps, the first
 * forming the high half of the result and the second the low half. Each step XORs the entries
 * 0x36 and 0x17 places back (mod 0x37 = 55), reduces mod 65535 and stores it at the cursor
 * gRandIndex. */
uint __stdcall GenerateNextPseudoRandomValue(void)  /* 0x004353A0 */
{
  /* the first value is the HIGH half (shl eax,0x10 at 0x00435445, then or the second) */
  short i;
  uint hi;
  uint lo;

  /* The cursor is volatile: the exe loads, increments and stores it (no `inc word [mem]`) and
   * keeps the intermediate `cursor = i + 1` store that a plain short would lose as dead. The
   * first index is reduced into a local only; the cursor is stored unreduced, then set to
   * i + 1 and reduced. One local carries both indices. */
  i = ++*(volatile short *)&gRandIndex % 0x37;
  hi = (gRandTable[(i + 0x36) % 0x37] ^ gRandTable[(i + 0x17) % 0x37]) % 0xffff;
  gRandTable[i] = hi;
  *(volatile short *)&gRandIndex = ++i;
  *(volatile short *)&gRandIndex = i %= 0x37;
  lo = (gRandTable[(i + 0x36) % 0x37] ^ gRandTable[(i + 0x17) % 0x37]) % 0xffff;
  gRandTable[i] = lo;
  return (hi << 0x10) | lo;
}


/* Writes value as decimal text into the Pascal string pPascalOut: a leading '-' when (int)value
 * is negative, then the digits, most significant first; byte 0 is the body length. */
void __cdecl FormatUintAsPascalDecimalString(unsigned int value, char *pPascalOut)  /* 0x00435450 */
{
    char          *pBody;
    short          nDigits;
    unsigned int   next;
    unsigned int   pow10;
    unsigned char  digit;

    pBody = pPascalOut + 1;
    if ((int)value < 0) {
        pPascalOut[0] = 1;
        value = -(int)value;
        *pBody++ = '-';
    } else {
        pPascalOut[0] = 0;
    }
    nDigits = 1;
    pow10 = 1;
    do {
        next = pow10 * 10;
        if (value < next)
            break;
        nDigits++;
        pow10 = next;
    } while (nDigits < 10);
    pPascalOut[0] += (char)nDigits;
    while (nDigits != 0) {
        digit = (unsigned char)(value / pow10);
        *pBody++ = digit + '0';
        value -= digit * pow10;
        pow10 /= 10;
        nDigits--;
    }
}


/* Integer square root of a 32-bit value, by 16 rounds of the bitwise restoring method. */
unsigned short __cdecl IntSqrt(unsigned int x)  /* 0x004354c0 */
{
    int result;
#ifdef _MSC_VER
    /* The original is hand-written inline assembly: the programmer's own push/pop of
     * ebx/ecx/edx/edi around a 15-round loop (shld/loop) plus a final round, the result parked
     * in a local and returned as a word. The C in the #else is its decode, for the GCC build,
     * which cannot take MSVC __asm. */
    __asm {
        push ebx
        push ecx
        push edx
        push edi
        mov  ebx, x
        xor  eax, eax
        xor  edx, edx
        mov  ecx, 15
    round:
        shld edx, ebx, 2
        shl  ebx, 2
        shl  eax, 1
        mov  edi, eax
        shl  edi, 1
        cmp  edi, edx
        jnc  nobit
        inc  eax
        sub  edx, edi
        dec  edx
    nobit:
        loop round
        shld edx, ebx, 2
        shl  eax, 1
        mov  edi, eax
        shl  edi, 1
        cmp  edi, edx
        jnc  done
        inc  eax
    done:
        pop  edi
        pop  edx
        pop  ecx
        pop  ebx
        mov  result, eax
    }
#else
    {
        int  trialRoot;
        int  partialRoot;
        int  bitIndex;
        uint remainder;

        remainder   = 0;
        bitIndex    = 0xf;
        partialRoot = 0;
        do {
            remainder = (remainder << 2) | (x >> 0x1e);
            x = x << 2;
            trialRoot = partialRoot * 2;
            if ((uint)(partialRoot * 4) < remainder) {
                trialRoot = trialRoot + 1;
                remainder = (remainder + partialRoot * -4) - 1;
            }
            bitIndex = bitIndex - 1;
            partialRoot = trialRoot;
        } while (bitIndex != 0);

        result = trialRoot * 2;
        if ((uint)(trialRoot << 2) < ((remainder << 2) | (x >> 0x1e))) {
            result = result + 1;
        }
    }
#endif
    return (unsigned short)result;
}


/* Blends palette pFrom toward pTo by step/total into a temporary copy, then installs the result
 * as the active CLUT. Entries are 8 bytes (a value word plus an RGB triple, at +10 from the
 * resource data); all 256 are blended. step and total are clamped to at least 1; once
 * step >= total it just reloads pTo (ReloadCLUT). */
void __cdecl InterpolateCLUT(int *pFrom, int *pTo, int step, int total)  /* 0x00435520 */
{
    /* Each entry's colour at +10 is a 6-byte record the exe copies to the stack whole
     * (dword + word moves) before blending it. */
    typedef struct { unsigned short red, green, blue; } RGBColor;
    typedef struct { short value; RGBColor rgb; } ColorSpec;
    int      *pWork;
    RGBColor  from;
    RGBColor  to;
    RGBColor  res;
    int       iFrom, iTo;
    int       iTmp, iSys;
    short     i;
    ColorSpec *pd;
    int       idx;
    int       idx2;   /* the copy loop's own index: Ghidra merged it with idx (the March build proves it) */

    if (total < 1) total = 1;
    if (step  < 1) step  = 1;
    if (total <= step) {
        ReloadCLUT(pTo);
        return;
    }
    pWork = pFrom;
    CloneResource((void **)&pWork);
    iFrom = GetResDataPtr(pFrom);
    iTmp  = GetResDataPtr(pWork);
    iTo   = GetResDataPtr(pTo);
    i = 0;
    do {
        idx  = (int)i * 8;
        i++;
        from = *(RGBColor *)(iFrom + idx + 10);
        to   = *(RGBColor *)(iTo   + idx + 10);
        /* destination entry taken first, as a ColorSpec whose rgb field is addressed later (the
         * exe adds +10 in two steps); iSys, not yet live, holds each channel as a 32-bit int */
        pd = (ColorSpec *)((char *)iTmp + idx + 8);
        iSys = from.red;   iSys = (to.red   - iSys) * step / total + iSys; res.red = iSys;
        iSys = from.green; iSys = (to.green - iSys) * step / total + iSys; res.green = iSys;
        iSys = from.blue;  iSys = (to.blue  - iSys) * step / total + iSys; res.blue = iSys;
        pd->rgb = res;
    } while (i < 0x100);
    UnlockResObject(pFrom);
    UnlockResObject(pTo);
    CommitInterpolatedCLUTPalette(iTmp);
    iSys = GetResDataPtr(gActiveClut);
    i    = 0;
    do {
        /* one 8-byte entry copied as a struct (two dword moves) */
        idx2 = (int)i * 8;
        *(SceneViewport *)(idx2 + iSys + 8) = *(SceneViewport *)(idx2 + iTmp + 8);
        i++;
    } while (i < 0x100);
    UnlockResObject(gActiveClut);
    UnlockResObject(pWork);
    FreeResObject(pWork);
}


/* Fades the active CLUT from hClut to the black palette in stepCount steps, one per 60 Hz tick:
 * the deadline is taken before the loop and advanced once per step.
 *
 * Port note: the GOG release hex-patches the wait away; DF_FIX_GOG_CLUT_FADE_NO_WAIT reproduces
 * that patch (present the back buffer instead of spinning). */
void __cdecl FadeCLUTToBlack(int *hClut, short stepCount)  /* 0x00435700 */
{
  short step;
  int deadline;
  step = 1;
  deadline = GetTick60Hz();
  while (step <= stepCount) {
    deadline = deadline + 1;
    InterpolateCLUT(hClut, gSecondaryClut, (int)step, (int)stepCount);
#if DF_FIX_GOG_CLUT_FADE_NO_WAIT
    PresentBackBufferRect(gClipRect);   /* GOG's patch, in place of the spin */
#else
    while ((int)GetTick60Hz() < deadline) {
    }
#endif
    step = step + 1;
  }
}


/* Fades the active CLUT from the black palette up to hClut in stepCount steps, one per tick;
 * the mirror of FadeCLUTToBlack (same GOG fix). */
void __cdecl FadeCLUTFromBlack(int *hClut,short stepCount)  /* 0x00435750 */
{
  short step;
  int deadline;
  step = 1;
  deadline = GetTick60Hz();
  while (step <= stepCount) {
    deadline = deadline + 1;
    InterpolateCLUT(gSecondaryClut,hClut, (int)step, (int)stepCount);
#if DF_FIX_GOG_CLUT_FADE_NO_WAIT
    PresentBackBufferRect(gClipRect);   /* GOG's patch, in place of the spin */
#else
    while ((int)GetTick60Hz() < deadline) {
    }
#endif
    step = step + 1;
  }
}


/* Loads string resource baseId + offsetId from the executable into pPascal as a Pascal string
 * (at most 255 characters). */
void __cdecl LoadStringResourceAsPascal(short baseId, short offsetId, char *pPascal)  /* 0x004357A0 */
{
  uint32_t len;
  CHAR stringBuf [256];  /* local_100 */

  len = LoadStringA(gInstance, offsetId + baseId, stringBuf, 0xff);   /* both ids are shorts: two movsx */
  *pPascal = (char)len;
  if ((char)len != '\0') {
    memmove(pPascal + 1, stringBuf, (uint32_t)(byte)len);
  }
  return;
}


/* True while Escape is held; the script interpreter polls it to abort when gKeyAborts allows. */
short __stdcall IsEscapeKeyDown(void)  /* 0x00435800 */
{
    /* A short flag, not a BOOL, set AFTER the call (nothing lives across it):
     * mov cx,1 / test ax,ax / jl / xor cx,cx / mov ax,cx. The three modifier-key tests below
     * share the shape. */
    SHORT state = GetAsyncKeyState(VK_ESCAPE);
    short down  = 1;
    if (state >= 0)
        down = 0;
    return down;
}

#undef trackFlags
#undef trackLen
#undef voiceChanId
#undef renderChildId
#undef pNodeEntry
#undef trackText

/* Hold-to-toggle "sequential" playback modifiers: Shift for multimedia, Ctrl for sound, Shift
 * for music (the multimedia and music tests read the same key). True while the key is down. */
short __stdcall IsMultimediaSequentialModifierKeyDown(void) /* 0x00435820 */
{
    SHORT state = GetAsyncKeyState(VK_SHIFT);
    short down  = 1;
    if (state >= 0)
        down = 0;
    return down;
}


short __stdcall IsSoundSequentialModifierKeyDown(void) /* 0x00435840 */
{
    SHORT state = GetAsyncKeyState(VK_CONTROL);
    short down  = 1;
    if (state >= 0)
        down = 0;
    return down;
}


short __stdcall IsMusicSequentialModifierKeyDown(void) /* 0x00435860 */
{
    SHORT state = GetAsyncKeyState(VK_SHIFT);
    short down  = 1;
    if (state >= 0)
        down = 0;
    return down;
}


/* Parses a decimal integer from the start of a Pascal string (sscanf with kStrPctLd, "%ld").
 * Returns the value, or 0 if no integer was read.
 *
 * Port note: DF_FIX_PARSE_INT_BUFFER bounds the body copy to the buffer; the original copies
 * pPascal[0] bytes (up to 255) into a 20-byte stack buffer. */
unsigned int __cdecl ParseLeadingIntFromPascalString(byte *pPascal) /* 0x00435880 */
{
#if DF_FIX_PARSE_INT_BUFFER
    char         buf[32];
    unsigned int len = pPascal[0];
    unsigned int out = 0;
    int          n;

    if (len >= sizeof(buf))
        len = (unsigned int)(sizeof(buf) - 1u);
    memmove(buf, pPascal + 1, len);
    buf[len] = '\0';
    n = sscanf(buf, kStrPctLd, &out);
    if (n == 1)
        return out;
    return 0u;
#else
    int          n;
    unsigned int out;
    char         buf[20];

    memmove(buf, pPascal + 1, (unsigned int)*pPascal);
    buf[*pPascal] = '\0';
    n = sscanf(buf, kStrPctLd, &out);
    if (n == 1)
        return out;
    return 0;
#endif
}


/* Loads the embedded resource "TAG.NAME" (tag spelled from its FourCC, e.g. 0x636c7574 ->
 * "clut"; NAME is the Pascal descriptor pDesc), uppercased, through LoadEmbeddedResource. */
void * __cdecl LoadResourceByTag(unsigned int tag, byte *pDesc)  /* 0x004358e0 */
{
    /* The FourCC as a big-endian C string: the exe stores the tag whole and swaps its bytes end
     * for end with local_tagStr[4] as the temporary, then terminates it (as GetResourceHandle). */
    char local_tagStr[5];
    char local_resName[32];
    char local_filename[32];

    *(unsigned int *)local_tagStr = tag;
    local_tagStr[4] = local_tagStr[0];
    local_tagStr[0] = local_tagStr[3];
    local_tagStr[3] = local_tagStr[4];
    local_tagStr[4] = local_tagStr[1];
    local_tagStr[1] = local_tagStr[2];
    local_tagStr[2] = local_tagStr[4];
    local_tagStr[4] = '\0';

    memmove(local_resName, pDesc + 1, (unsigned int)*pDesc);
    local_resName[*pDesc] = '\0';

    sprintf(local_filename, "%s.%s" /* 0x0045db68 */, local_tagStr, local_resName);
    _strupr(local_filename);   /* e.g. "CLUT.SHIPPAL" */

    return LoadEmbeddedResource(local_filename, tag);
}


/* Loads the named resource from the executable's .rsrc and returns a game handle. pName is the
 * uppercased "TAG.NAME" built by LoadResourceByTag / GetResourceHandle; tag is the FourCC as a
 * little-endian dword.
 *
 * tag 'CURS' (0x43555253): returns the HCURSOR from LoadCursorA, which callers keep as a cursor
 * handle. Any other tag: looks the name up as type RAW (kStrRAW) and returns a ResObject whose
 * buffer holds a copy of the bytes (e.g. gPrimaryClut for the primary palette). Ghidra types the
 * return HCURSOR from the CURS arm; it is void * on both.
 *
 * Fatal 0x66 with 0x48a (resource not found), 0x48b (LoadResource failed, tested after
 * FreeResource as in the exe) or 0x48c (final NULL guard: allocation or cursor load failed). */
void * __cdecl LoadEmbeddedResource(LPCSTR pName, unsigned int tag)  /* 0x00435980 */
{
    void   *pObj;
    HRSRC   hResInfo;
    DWORD   dwSize;
    void   *pDst;
    HGLOBAL hData;
    void   *pSrc;

    if (tag != 0x43555253u)   /* not 'CURS': generic Win32 resource path */
    {
        hResInfo = FindResourceA(gInstance, pName, kStrRAW);
        if (hResInfo == NULL)
        {
            FatalError(0x66, 0x48a);
        }

        dwSize = SizeofResource(gInstance, hResInfo);

        pObj = AllocResObject(dwSize);
        pDst = GetResDataPtr(pObj);

        hData = LoadResource(gInstance, hResInfo);
        pSrc  = LockResource(hData);
        memmove(pDst, pSrc, dwSize);
        FreeResource(hData);

        /* the exe tests hData only after FreeResource; Ghidra's order is the binary's */
        if (hData == NULL)
        {
            FatalError(0x66, 0x48b);
        }

        UnlockResObject(pObj);
    }
    else
    {
        pObj = (void *)LoadCursorA(gInstance, pName);
    }

    if (pObj == NULL)
    {
        FatalError(0x66, 0x48c);
    }

    return pObj;
}


/* Like LoadResourceByTag, but the resource name is "TAG<id>" (kStrPctSPctD with the decimal id),
 * e.g. 'CURS' 2002 -> "CURS2002".
 *
 * Port note: an early decode built the tag LSB first with no terminator ("SRUC2002" plus stack
 * garbage), which broke BeginGameSession's cursor load. */
void * __cdecl GetResourceHandle(unsigned int tag, short id) /* 0x00435a60 */
{
    unsigned char tagBytes[5];   /* FourCC, MSB first, NUL-terminated */
    char          name[32];

    /* swap through tagBytes[4], then terminate (as LoadResourceByTag) */
    *(unsigned int *)tagBytes = tag;
    tagBytes[4] = tagBytes[0];
    tagBytes[0] = tagBytes[3];
    tagBytes[3] = tagBytes[4];
    tagBytes[4] = tagBytes[1];
    tagBytes[1] = tagBytes[2];
    tagBytes[2] = tagBytes[4];
    tagBytes[4] = '\0';

    sprintf(name, kStrPctSPctD, tagBytes, (int)id);
    _strupr(name);
    return LoadEmbeddedResource(name, tag);
}


/* Frees a game resource handle; a no-op for tag 'CURS' (0x43555253), whose handles are plain
 * Win32 cursors with nothing to tear down. */
void __cdecl ReleaseResource(int *pHandle, int tag)  /* 0x00435ae0 */
{
    if (tag != 0x43555253)
    {
        FreeResObject(pHandle);
    }
}


/* Fills *pOut with the node descriptor kept in the six extra window longs (offsets 0, 4, 8, 0xc,
 * 0x10, 0x14) of the node's window. pHwnd points at the node object, whose first field is its HWND
 * (a plain HWND * works too). The layout is the one SetChannelWindowState writes. */
void __cdecl FetchHotspotDesc(HWND *pHwnd, NodeDesc *pOut)  /* 0x00435b00 */
{
    LONG v;
    v = GetWindowLongA((HWND)*pHwnd, 0);
    *(short *)pOut = (short)v;
    *(LONG *)((char *)pOut + 2)  = GetWindowLongA((HWND)*pHwnd, 4);
    *(LONG *)((char *)pOut + 6)  = GetWindowLongA((HWND)*pHwnd, 8);
    *(LONG *)((char *)pOut + 10) = GetWindowLongA((HWND)*pHwnd, 0x0c);
    *(LONG *)((char *)pOut + 14) = GetWindowLongA((HWND)*pHwnd, 0x10);
    *(LONG *)((char *)pOut + 18) = GetWindowLongA((HWND)*pHwnd, 0x14);
}


/* Stores a channel's state descriptor in the six extra window longs of its HWND (created with
 * cbWndExtra >= 0x18): +0 channel type, +4..+0x10 four fields, +0x14 the resource handle.
 * InitMessagesChannel, InitSceneNodeChannel and InitDebugger call it after filling the 146-byte
 * channel struct; FetchHotspotDesc reads it back. */
void __cdecl SetChannelWindowState(void *pHwnd, short *pState)  /* 0x00435b60 */
{
    /* the exe re-reads *pHwnd for every call */
    SetWindowLongA(*(HWND *)pHwnd, 0x00, (LONG)*pState);
    SetWindowLongA(*(HWND *)pHwnd, 0x04, *(LONG *)(pState + 1));
    SetWindowLongA(*(HWND *)pHwnd, 0x08, *(LONG *)(pState + 3));
    SetWindowLongA(*(HWND *)pHwnd, 0x0C, *(LONG *)(pState + 5));
    SetWindowLongA(*(HWND *)pHwnd, 0x10, *(LONG *)(pState + 7));
    SetWindowLongA(*(HWND *)pHwnd, 0x14, *(LONG *)(pState + 9));
}


/* If the active window is one of the registered hotspot viewports, fills *pOut with its node
 * descriptor and returns 1; otherwise 0. Used for the keyboard and navigation events (types 3
 * and 5), where the target is found by focus rather than cursor position. */
short __cdecl FindActiveHotspot(NodeDesc *pOut)  /* 0x00435bc0 */
{
    /* The exe hands FetchHotspotDesc a 0x74-byte frame local (the size of a viewport record, see
     * CreateSceneViewport) whose first dword is the window; only that dword is read. */
    struct { HWND hWnd; char rest[0x70]; } target;
    HWND hWnd;
    int  i;

    hWnd = GetActiveWindow();

    for (i = 0; i < gViewportCount; i++) {
        /* dangling-slot guard, as in HitTestHotspot below */
        if (DF_VIEWPORT_SLOT_OK(gViewportTable[i], sizeof(HWND)) && *(HWND *)gViewportTable[i] == hWnd)
            break;
    }
    if (i == gViewportCount)
        return 0;
    target.hWnd = hWnd;
    FetchHotspotDesc(&target.hWnd, pOut);
    return 1;
}


/* Maps a packed scene cursor position (low word Y, high word X) to an interaction code and, on
 * a hit, fills *pNodeDesc with the target viewport's descriptor. Returns 0 miss, 1 confirm,
 * 3 enter sub-view, 6 table hit. Two sentinel Y values are not coordinates: Y == -1000 returns 1
 * at once (no fill); Y == -2000 uses X as a direct index into gViewportTable (6 if in range, else
 * 0). Otherwise the window under the point is looked up among the registered viewports.
 *
 * Port note: DF_VIEWPORT_SLOT_OK guards the table scans against dangling slots, which the
 * original's environment tolerated (DF_FIX_VIEWPORT_SLOT_GUARD, port-bug-classes.md section 8). */
short __cdecl HitTestHotspot(unsigned int packedYX, NodeDesc *pNodeDesc)  /* 0x00435c20 */
{
    /* The point is built in the first 8 bytes of the same 0x74-byte record FindActiveHotspot
     * uses; the hit window is then written over its first dword and handed to FetchHotspotDesc.
     * X is the high word, sign-extended (movsx). */
    union { POINT pt; HWND hWnd; char rec[0x74]; } target;
    HWND hWnd;
    int  i;
    int  x;

    x = ((short *)&packedYX)[1];
    target.pt.x = x;
    target.pt.y = (short)packedYX;
    if ((short)packedYX == -1000)
        return 1;
    if ((short)packedYX == -2000) {
        if (x >= gViewportCount)
            return 0;
        if (!DF_VIEWPORT_SLOT_OK(gViewportTable[x], sizeof(HWND)))
            return 0;
        FetchHotspotDesc((HWND *)gViewportTable[x], pNodeDesc);
        return 6;
    }
    hWnd = WindowFromPoint(target.pt);
    for (i = 0; i < gViewportCount; i++) {
        /* DestroyChannelViewport leaves a destroyed viewport's slot dangling (see its Port note),
         * so the entry must be probed for readability, not just tested for NULL. */
        if (DF_VIEWPORT_SLOT_OK(gViewportTable[i], sizeof(HWND)) && *(HWND *)gViewportTable[i] == hWnd)
            break;
    }
    if (i == gViewportCount)
        return 0;
    target.hWnd = hWnd;
    FetchHotspotDesc(&target.hWnd, pNodeDesc);
    return 3;
}


/* Tears down a channel's viewport: re-shows the cover window when it is the main window's,
 * releases the hotspot bitmaps, DC and window, and frees the node object. Called on msg 4 (end
 * scene) from SceneChannelHandler, MessagesChannelHandler and DebuggerChannelHandler.
 *
 * Port note: DF_FIX_VIEWPORT_SLOT_GUARD also clears the viewport's slot in gViewportTable; the
 * original left it dangling and a later table scan crashed live. */
void __cdecl DestroyChannelViewport(int pChannel)  /* 0x00435cf0 */
{
    int *pNodeObj = *(int **)(pChannel + 0x12);
    int  i;

    if (*pNodeObj == gMainWindow) {
        ShowWindow(gCoverWindow, SW_SHOW);
        UpdateWindow(gCoverWindow);
    }
    CleanupHotspotBitmaps((int)(pNodeObj + 2));
    ReleaseDC((HWND)*pNodeObj, (HDC)pNodeObj[1]);
    DestroyWindow((HWND)*pNodeObj);

#if DF_FIX_VIEWPORT_SLOT_GUARD
    for (i = 0; i < gViewportCount; i++) {
        if (gViewportTable[i] == pNodeObj) {
            gViewportTable[i] = NULL;
            break;
        }
    }
#endif

    FreeEngineBuffer(pNodeObj);
}


/* No-op (a single ret); cdecl with one argument, which its caller pushes and pops. */
void __cdecl FlushChannelDisplay(int pChannel)  /* 0x00435D50 */
{
}


/* No-op hook called after a scene-state refresh (msg 3). cdecl with two parameters:
 * DebuggerChannelHandler pushes (pChannel, pParam) at 0x0040bbd1 and cleans them. */
void __cdecl RefreshChannelState(int pChannel, int *pParam)  /* 0x00435d60 */
{
    return;
}


/* Creates an off-screen "DFWINDOW" popup (100,100, 300x200) and its 0x74-byte viewport record:
 * +0 HWND, +4 HDC, +8 the 0x6c-byte InitHotspotBitmapState block. Stores the record in *ppOut
 * and registers it as gViewportTable[gViewportCount++] (gViewportWasVisible cleared); the first
 * viewport's window also becomes gMainWindow. Called from InitSceneNodeChannel and
 * InitMessagesChannel. Its caller pushes three arguments (the third a 0); only ppOut is read. */
void __cdecl CreateSceneViewport(void *pUnused, void **ppOut, int unused3)  /* 0x00435d70 */
{
    HWND   hWnd;
    int   *pViewport;
    HDC    hDC;
    int    idx;

    hWnd = CreateWindowExA(0, "DFWINDOW" /* 0x0045bae8 */, (LPCSTR)gAppTitle,
                           WS_POPUP, 100, 100, 300, 200,
                           NULL, NULL, gInstance, NULL);

    pViewport = (int *)AllocEngineBuffer(0x74);
    pViewport[0] = (int)hWnd;

    hDC = GetDC(hWnd);
    pViewport[1] = (int)hDC;

    InitHotspotBitmapState(pViewport + 2);

    *ppOut = pViewport;

    idx = gViewportCount;
    gViewportCount = gViewportCount + 1;

    gViewportWasVisible[idx] = 0;
    gViewportTable[idx] = pViewport;

    if (gMainWindow == 0)
    {
        gMainWindow = (int)hWnd;
    }
}


/* Sets the maximum of both scrollbar ranges of a channel's window (hMax horizontal, vMax
 * vertical), keeping the existing minimums. */
void __cdecl SetChannelScrollMax(int pChannel, short hMax, short vMax)  /* 0x00435e00 */
{
    HWND hWnd = (HWND)**(int **)(pChannel + 0x12);
    int hMin, hCurMax, vMin, vCurMax;
    GetScrollRange(hWnd, SB_HORZ, &hMin, &hCurMax);
    GetScrollRange(hWnd, SB_VERT, &vMin, &vCurMax);
    vCurMax = vMax;                     /* the new maxima overwrite the fetched ones (the exe stores both) */
    hCurMax = hMax;
    SetScrollRange(hWnd, SB_HORZ, hMin, hCurMax, FALSE);
    SetScrollRange(hWnd, SB_VERT, vMin, vCurMax, TRUE);
}


/* Sets the minimum of both scrollbar ranges of a channel's window (hMin horizontal, vMin
 * vertical), keeping the existing maximums. */
void __cdecl SetChannelScrollMin(int pChannel, short hMin, short vMin)  /* 0x00435e70 */
{
    HWND hWnd = (HWND)**(int **)(pChannel + 0x12);
    int hCurMin, hMax, vCurMin, vMax;
    GetScrollRange(hWnd, SB_HORZ, &hCurMin, &hMax);
    GetScrollRange(hWnd, SB_VERT, &vCurMin, &vMax);
    vCurMin = vMin;                     /* the new minima overwrite the fetched ones (the exe stores both) */
    hCurMin = hMin;
    SetScrollRange(hWnd, SB_HORZ, hCurMin, hMax, FALSE);
    SetScrollRange(hWnd, SB_VERT, vCurMin, vMax, TRUE);
}


/* Reads a channel window's scroll positions (*pHOut horizontal, *pVOut vertical);
 * DebuggerChannelHandler uses it to report display dimensions (case 0xe). */
void __cdecl GetChannelScrollPos(int pChannel, short *pHOut, short *pVOut)  /* 0x00435ee0 */
{
    HWND hWnd = (HWND)**(int **)(pChannel + 0x12);
    *pHOut = (short)GetScrollPos(hWnd, SB_HORZ);
    *pVOut = (short)GetScrollPos(hWnd, SB_VERT);
}


/* Sets a channel window's scroll positions; the horizontal bar without redraw, the vertical one
 * with. */
void __cdecl SetChannelScrollPos(int pChannel, short hPos, short vPos)  /* 0x00435f10 */
{
    HWND hWnd = (HWND)**(int **)(pChannel + 0x12);
    SetScrollPos(hWnd, SB_HORZ, (int)hPos, FALSE);
    SetScrollPos(hWnd, SB_VERT, (int)vPos, TRUE);
}


/* No-op hook called after a viewport rect is applied (msg 7); the caller pushes pChannel at
 * 0x0040bd03 and cleans it. */
void __cdecl PostViewportSetAction(int pChannel)  /* 0x00435f40 */
{
    return;
}


/* Copies entries 1..254 of pSrcClut into the active CLUT, sets entry 0 to black and entry 255 to
 * white, then applies the result to the Win32 palette (ApplyCLUTToPalette). Entries are 8 bytes,
 * from +8 in the resource data. */
void __cdecl ReloadCLUT(int *pSrcClut)  /* 0x00435f50 */
{
    typedef struct { int lo; int hi; } ClutEnt;   /* 8-byte entry, copied whole */
    int   idx;      /* declared first: it sets the lea operand order */
    int   iSrc;
    int   iDst;
    short i;

    iSrc = GetResDataPtr(pSrcClut);
    iDst = GetResDataPtr(gActiveClut);
    i = 1;
    do {
        idx = i * 8;
        i++;
        *(ClutEnt *)(idx + iDst + 8) = *(ClutEnt *)(idx + iSrc + 8);
    } while (i < 0xff);
    /* entry 0's colour is at +10, entry 255's at +0x802 */
    *(short *)(iDst + 0xe) = 0;
    *(short *)(iDst + 0xc) = 0;
    *(short *)(iDst + 0xa) = 0;
    *(unsigned short *)(iDst + 0x806) = 0xffff;   /* `mov ecx,0xffff`: unsigned */
    *(unsigned short *)(iDst + 0x804) = 0xffff;
    *(unsigned short *)(iDst + 0x802) = 0xffff;
    UnlockResObject(gActiveClut);
    ApplyCLUTToPalette(iSrc);
    UnlockResObject(pSrcClut);
}


/* Resets a palette resource to the identity table: resized to 0x808 bytes, entry i's index word
 * set to i (0..255), entry 255's colour white and entry 0's black. */
void __cdecl InitCLUT(int *hClut)  /* 0x00435FF0 */
{
    /* Mac ColorTable heritage: entry 255's and entry 0's colour are each written by copying one
     * stack RGBColor (6 bytes: dword + word moves) */
    typedef struct { unsigned short red, green, blue; } RGBColor;
    char    *p;
    short    i;
    RGBColor c;

    ResizeResObject((void *)*hClut, 0x808);
    p = (char *)GetResDataPtr((void *)*hClut);
    i = 0;
    *(unsigned short *)(p + 4) &= 0x7fff;
    *(int *)p = 0;
    *(short *)(p + 6) = 0xff;
    do {
        *(short *)(p + i * 8 + 8) = i;
        i++;
    } while (i <= 0xff);
    c.red   = 0xffff;
    c.green = 0xffff;
    c.blue  = 0xffff;
    *(RGBColor *)(p + 0x802) = c;
    c.red   = 0;
    c.green = 0;
    c.blue  = 0;
    *(RGBColor *)(p + 10) = c;
    UnlockResObject((void *)*hClut);
}


/* Scans the "Plugins:" folder (kStrPluginsColon) and builds gPluginDirectory, a resource of
 * 24-byte entries. Each file is bound as a plugin (TryBindSceneDirectoryDll); the entry is sent
 * verb 0 ("what are you called?") and must answer with a Pascal string name under 16 bytes,
 * else error 0x6d. Accepted entries are named and appended; rejected ones are finalized. Fatal
 * 0x1050 if finalizing fails. Called by BeginGameSession. */
void __stdcall LoadPluginDirectory(void)  /* 0x004360a0 */
{
    typedef struct { int d[6]; } DirEntry24;
    short       status;
    byte        pluginsPath[256];  /* local_100 */
    DfPluginMsg msg;
    char        entry[24];      /* one directory entry */

    gPluginDirectoryCount = 0;
    gPluginDirectory = AllocResObject(0);

    GetSearchPathSlot(0, pluginsPath);
    AppendPascalString((byte *)UpdatePascalLength((char *)&kStrPluginsColon), pluginsPath);   /* the lookup written inline: the other arguments are pushed before it is called */

    status = OpenDirectorySearch(pluginsPath);
    if (status == 0)
    {
        while ((status = ReadNextDirectoryEntry((byte *)&gScratchStr)) == 0)
        {

            status = TryBindSceneDirectoryDll(pluginsPath, (byte *)&gScratchStr, entry);
            if (status != 0)
                continue;

            /* the entry answers with a Pascal string in the result slot (0x00436136-0x00436195) */
            msg.verb        = 0;
            msg.arg[0].code = DF_PLUGIN_NONE;
            msg.arg[1].code = DF_PLUGIN_NONE;
            msg.arg[2].code = DF_PLUGIN_NONE;
            msg.arg[3].code = DF_PLUGIN_NONE;
            msg.result.code = DF_PLUGIN_NONE;

            status = (short)InvokeEntryCallback(entry, &msg);
            if (msg.result.code != DF_PLUGIN_STR)
                status = 0x6d;
            /* the exe copies the result string whatever its code (0x004361d6) */
            CopyPascalString((byte *)msg.result.data, (byte *)&gScratchStr);
            if (((byte *)&gScratchStr)[0] >= 0x10)
                status = 0x6d;   /* 0x00436188: name must be under 16 bytes */

            if (status == 0) {
                /* accepted: name the entry and append it (24 bytes copied as one struct, rep movsd 6) */
                gPluginDirectoryCount++;
                CopyPascalString((byte *)msg.result.data, (byte *)entry);
                ResizeResObject((int *)gPluginDirectory,
                                (unsigned int)(gPluginDirectoryCount * 24));
                *(DirEntry24 *)((char *)GetResDataPtr((int *)gPluginDirectory)
                                + gPluginDirectoryCount * 24 - 24) = *(DirEntry24 *)entry;
                UnlockResObject((int *)gPluginDirectory);
            } else {
                status = FinalizeDirectoryEntry(entry);
                if (status != 0)
                    FatalError(status, 0x1050);
            }
        }
    }
}


/* Shuts down every plugin in gPluginDirectory (verb 1, which must leave the result slot untouched,
 * else error 0x6d), finalizes each entry, and frees the directory. Fatal 0x1051 on a failed
 * shutdown, 0x1052 on a failed finalize. Called by EndGameSession. */
void __stdcall UnloadPluginDirectory(void)  /* 0x00436240 */
{
    typedef struct { int d[6]; } DirEntry24;
    short          status;
    unsigned int  *pEntry;
    int            i;
    unsigned int   entryCopy[6];     /* stack copy of one directory entry */  /* local_18 */
    DfPluginMsg    msg;

    i = 0;
    pEntry = (unsigned int *)GetResDataPtr((int *)gPluginDirectory);

    if (gPluginDirectoryCount > 0)
    {
        do
        {
            msg.verb        = 1;
            msg.arg[0].code = DF_PLUGIN_NONE;
            msg.arg[1].code = DF_PLUGIN_NONE;
            msg.arg[2].code = DF_PLUGIN_NONE;
            msg.arg[3].code = DF_PLUGIN_NONE;
            msg.result.code = DF_PLUGIN_NONE;

            /* the entry copied to the stack as one struct (rep movsd 6) */
            *(DirEntry24 *)entryCopy = *(DirEntry24 *)pEntry;

            status = (short)InvokeEntryCallback(entryCopy, &msg);
            if (msg.result.code != DF_PLUGIN_NONE)
                status = 0x6d;
            if (status != 0) FatalError(status, 0x1051);

            status = FinalizeDirectoryEntry(entryCopy);
            if (status != 0) FatalError(status, 0x1052);

            pEntry += 6;
            i++;
        } while (i < gPluginDirectoryCount);
    }

    UnlockResObject((int *)gPluginDirectory);
    FreeResObject(gPluginDirectory);
    gPluginDirectoryCount = 0;
}


/* Script command 20109: pushes 1 if a plugin with the given name is in the directory, else 0
 * (an int result, type word 2). */
short __cdecl ScriptCmd_PluginExists(int *pCtx, short *pArgBlock,
                                                                   int *pTable, short *pResult,
                                                                   int *pSkipCount) /* 0x00436330 */
{
    short  status;
    short  lookupStatus;
    short argResult[4];  /* local_20 */
    int   entryCtx[6];  /* local_18 */

    status = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, gScratchStr);
    if (status != 0)
        return status;

    lookupStatus = LookupPluginByDescriptor(gScratchStr, entryCtx);
    if (lookupStatus != 0)
        *(int *)(pResult + 1) = 0;
    else
        *(int *)(pResult + 1) = 1;
    pResult[3]             = 0;
    *pResult               = 2;
    return 0;
}


/* Action-command wrapper around ScriptCmd_PluginFx (opcode 12027). Action commands discard
 * results, so PluginFx's "no return value" (5) becomes success, its success (a value was
 * returned) becomes error 6, and any other error passes through. */
short __cdecl ActionCmd_Plugin(int pCtx, int pArgBlock, int *pTable,
                                     int *pSubSkip)  /* 0x004363b0 */
{
    short  status;
    short  result[4];

    status = (short)ScriptCmd_PluginFx(pCtx, pArgBlock, pTable, result, pSubSkip);
    if (status == 5)
        return 0;
    if (status != 0)
        return status;
    return 6;
}


/* Script command 12027: evaluates a plugin name, resolves it in the directory, parses up to four
 * optional comma-separated arguments (0xfb4 is the comma token), and sends the entry verb 2
 * through InvokeEntryCallback. Each argument is an int (DF_PLUGIN_INT, code 0x3e9) or a string
 * (DF_PLUGIN_STR, 0x3ea); missing ones stay DF_PLUGIN_NONE (1000). The callback's result becomes
 * the command's: an int, a script string, or none (returns 5). *pSubSkip receives the number of
 * 8-byte script entries consumed.
 *
 * Port note: the message block is passed by pointer to InvokeEntryCallback. An earlier comment
 * claimed the locals had to be contiguous in MSVC stack order; that rationalised a dropped
 * argument. */
short __cdecl ScriptCmd_PluginFx(char *pCtx, int pArgBlock, int *pTable,  /* 0x004363f0 */
                                  short *pResult, int *pSubSkip)
{
    short   status;
    int    pStart;
    int    instrCount;
    short  argResult[4];   /* 8-byte typed result from ProcessScriptBlock */

    DfPluginMsg msg;                   /* 0x20 bytes, passed by pointer */
    int    entryCtx[6];                /* the 24-byte directory entry */

    byte   descBuf1[256];
    byte   descBuf2[256];
    byte   descBuf3[256];
    byte   descBuf4[256];

    pStart = pArgBlock;   /* pArgBlock walks; its start gives the skip count */
    msg.verb        = 2;
    msg.arg[0].code = DF_PLUGIN_NONE;
    msg.arg[1].code = DF_PLUGIN_NONE;
    msg.arg[2].code = DF_PLUGIN_NONE;
    msg.arg[3].code = DF_PLUGIN_NONE;
    msg.result.code = DF_PLUGIN_NONE;

    status = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable,
                               (int *)argResult, &instrCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPluginByDescriptor(&gScratchStr, (unsigned int *)entryCtx);
    if (status != 0)
        return status;

    pArgBlock += instrCount * 8;

    /* An argResult type of 4 is an int; anything else is taken as a string. */
    if (*(short *)pArgBlock == 0xfb4) {
        pArgBlock += 8;
        status = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable,
                                   (int *)argResult, &instrCount);
        if (status != 0)
            return status;

        if (argResult[0] == 4) {
            msg.arg[0].code = DF_PLUGIN_INT;
            msg.arg[0].data = *(void **)(argResult + 1);
        } else {
            msg.arg[0].code = DF_PLUGIN_STR;
            status = TakeScriptString(argResult, descBuf1);
            if (status != 0)
                return status;
            msg.arg[0].data = descBuf1;
        }

        pArgBlock += instrCount * 8;

        if (*(short *)pArgBlock == 0xfb4) {
            pArgBlock += 8;
            status = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable,
                                       (int *)argResult, &instrCount);
            if (status != 0)
                return status;

            if (argResult[0] == 4) {
                msg.arg[1].code = DF_PLUGIN_INT;
                msg.arg[1].data = *(void **)(argResult + 1);
            } else {
                msg.arg[1].code = DF_PLUGIN_STR;
                status = TakeScriptString(argResult, descBuf2);
                if (status != 0)
                    return status;
                msg.arg[1].data = descBuf2;
            }

            pArgBlock += instrCount * 8;

            if (*(short *)pArgBlock == 0xfb4) {
                pArgBlock += 8;
                status = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable,
                                           (int *)argResult, &instrCount);
                if (status != 0)
                    return status;

                if (argResult[0] == 4) {
                    msg.arg[2].code = DF_PLUGIN_INT;
                    msg.arg[2].data = *(void **)(argResult + 1);
                } else {
                    msg.arg[2].code = DF_PLUGIN_STR;
                    status = TakeScriptString(argResult, descBuf3);
                    if (status != 0)
                        return status;
                    msg.arg[2].data = descBuf3;
                }

                pArgBlock += instrCount * 8;

                if (*(short *)pArgBlock == 0xfb4) {
                    pArgBlock += 8;
                    status = ProcessScriptBlock(pCtx, (short *)pArgBlock, pTable,
                                               (int *)argResult, &instrCount);
                    if (status != 0)
                        return status;

                    if (argResult[0] == 4) {
                        msg.arg[3].code = DF_PLUGIN_INT;
                        msg.arg[3].data = *(void **)(argResult + 1);
                    } else {
                        msg.arg[3].code = DF_PLUGIN_STR;
                        status = TakeScriptString(argResult, descBuf4);
                        if (status != 0)
                            return status;
                        msg.arg[3].data = descBuf4;
                    }

                    pArgBlock += instrCount * 8;
                }
            }
        }
    }

    *pSubSkip = (pArgBlock - pStart) >> 3;

    status = InvokeEntryCallback(entryCtx, &msg);
    if (status != 0)
        return status;

    if (msg.result.code == DF_PLUGIN_INT) {
        *(void **)(pResult + 1) = msg.result.data;
        pResult[0] = 4;
        pResult[3] = 0;
        return status & 0xffff0000;
    }
    if (msg.result.code == DF_PLUGIN_STR) {
        return AllocScriptString((byte *)msg.result.data, pResult);
    }
    return (status & 0xffff0000) | 5;
}


/* The engine's API for plugin DLLs. InvokeEntryCallback (0x004323f0) hands its address to every
 * plugin callback, which calls back with a DfPluginMsg whose verb is the request (500, then
 * 0x1f5..0x227). Each arm checks the four argument kinds (CheckPluginArgTypes, fatal
 * 0x1054..0x1057 on a mismatch), calls one engine function and stores the result with its kind.
 * Unknown verb: 0x6d. The shipped game has no plugin directory, so nothing reaches this; the
 * byte match against TI.EXE is its test. */
#define PARG(i) (pMsg->arg[i].data)
#define PINT(i) ((int)(intptr_t)pMsg->arg[i].data)
#define PRESULT(kind, v) (pMsg->result.data = (void *)(intptr_t)(v), pMsg->result.code = (kind))
short __cdecl DispatchPluginEngineRequest(DfPluginMsg *pMsg)  /* 0x00436700 */
{
    short          st;
    short          ok;
    short          openErr;
    unsigned short readOnly;
    int           *pHandle;
    unsigned int   value;
    int            node;
    int            packedPoint;
    uint32_t       freeBytes;
    int            viewport[2];
    int            container;
    int            kind[4];         /* 16 bytes in the exe frame; only kind[0] is written here */
    int            image[30];       /* a HotspotBuf image: what the CopyTrig*Pair functions write */
    unsigned int   result;
    short         *pRect;
    void          *pDst;

    st = 0;
    pMsg->result.code = DF_PLUGIN_NONE;
    switch (pMsg->verb) {
    case 500:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_STR, DF_PLUGIN_INT, DF_PLUGIN_NONE);
        _memcpy(PARG(1), PARG(0), (size_t)PINT(2));
        break;
    case 0x1f5:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        PRESULT(DF_PLUGIN_INT, GetHeapFreeBytes());
        break;
    case 0x1f6:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_RESOURCE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        PRESULT(DF_PLUGIN_INT, GetResObjectSize(PARG(0)));
        break;
    case 0x1f7:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_RESOURCE, DF_PLUGIN_INT, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        pHandle = (int *)PARG(0);
        ResizeResObject(pHandle, (unsigned int)PINT(1));
        PRESULT(DF_PLUGIN_RESOURCE, pHandle);
        break;
    case 0x1f8:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_INT, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        pHandle = (int *)AllocResObject((DWORD)PINT(0));
        PRESULT(DF_PLUGIN_RESOURCE, pHandle);
        break;
    case 0x1f9:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_RESOURCE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        FreeResObject((int *)PARG(0));
        break;
    case 0x1fa:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_RESOURCE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        PRESULT(DF_PLUGIN_DATA, GetResDataPtr((int *)PARG(0)));
        break;
    case 0x1fb:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_RESOURCE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        UnlockResObject((int *)PARG(0));
        break;
    case 0x1fc:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_RESOURCE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        pHandle = (int *)PARG(0);
        CloneResource((void **)&pHandle);
        PRESULT(DF_PLUGIN_RESOURCE, pHandle);
        break;
    case 0x1fd:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        RunMessagesInputLine((byte *)PARG(0));
        break;
    case 0x1fe:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_STR, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        AppendPascalString((byte *)PARG(0), (byte *)PARG(1));
        break;
    case 0x1ff:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_STR, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        CopyPascalString((byte *)PARG(0), (byte *)PARG(1));
        break;
    case 0x200:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        PRESULT(DF_PLUGIN_INT, ParseLeadingIntFromPascalString((byte *)PARG(0)));
        break;
    case 0x201:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_INT, DF_PLUGIN_STR, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        FormatUintAsPascalDecimalString((unsigned int)PINT(0), (char *)PARG(1));
        break;
    case 0x202:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        FatalError(0, 0x1053);
        break;
    case 0x203:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_STR, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        pMsg->result.data = (void *)(intptr_t)PickSaveFileName((char *)PARG(0), (char *)PARG(1));
        pMsg->result.code = DF_PLUGIN_INT;
        break;
    case 0x204:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_STR, DF_PLUGIN_INT, DF_PLUGIN_NONE);
        kind[0] = PINT(2);
        /* the fifth argument (1) is one the callee never reads */
        pMsg->result.data = (void *)(intptr_t)PickOpenFileName((char *)PARG(0), (char *)PARG(1), 0, kind, 1);
        pMsg->result.code = DF_PLUGIN_INT;
        break;
    case 0x205:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_INT, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        st = ReserveHeapSpace(PINT(0), &ok);
        pMsg->result.data = (void *)(intptr_t)ok;
        pMsg->result.code = DF_PLUGIN_INT;
        break;
    case 0x206:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        st = OpenContainerNode((byte *)PARG(0), (int **)&node);
        PRESULT(DF_PLUGIN_NODE, node);
        break;
    case 0x207:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NODE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        st = DestroyRenderNode(PARG(0));
        break;
    case 0x208:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NODE, DF_PLUGIN_INT, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        st = AttachRenderChild((int *)PARG(0), (unsigned int)PINT(1), &pHandle);
        PRESULT(DF_PLUGIN_RESOURCE, pHandle);
        break;
    case 0x209:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NODE, DF_PLUGIN_INT, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        st = DetachRenderChild(PINT(0), PINT(1));
        break;
    case 0x20a:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NODE, DF_PLUGIN_INT, DF_PLUGIN_RESOURCE, DF_PLUGIN_NONE);
        st = SetChildRenderData(PINT(0), PINT(1), (uint32_t)PINT(2));
        break;
    case 0x20b:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NODE, DF_PLUGIN_RESOURCE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        st = SetupRenderObjectResource((int *)PARG(0), &value, (int *)PARG(1));
        pMsg->result.data = (void *)(intptr_t)value;
        pMsg->result.code = DF_PLUGIN_INT;
        break;
    case 0x20c:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NODE, DF_PLUGIN_INT, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        st = DestroyChildRenderEntry((uint32_t *)PARG(0), (uint32_t)PINT(1));
        break;
    case 0x20d:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_HOTSPOT, DF_PLUGIN_INT, DF_PLUGIN_INT, DF_PLUGIN_NONE);
        InitHotspotBuffer((short *)PARG(0), (unsigned short)PINT(1), (short)PINT(2));
        break;
    case 0x20e:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_HOTSPOT, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        DestroyHotspotBuffer(PINT(0));
        break;
    case 0x20f:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_HOTSPOT, DF_PLUGIN_HOTSPOT_STATE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        CopyHotspotBufState((unsigned int *)PARG(0), (unsigned int *)PARG(1));
        break;
    case 0x210:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_HOTSPOT_STATE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        TeardownSceneRefs(PARG(0));
        break;
    case 0x211:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_HOTSPOT_STATE, DF_PLUGIN_HOTSPOT_STATE, DF_PLUGIN_RECT, DF_PLUGIN_RECT);
        QdCopyBits((int *)PARG(0), (int *)PARG(1), (short *)PARG(2), (short *)PARG(3));
        break;
    case 0x212:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_HOTSPOT_STATE, DF_PLUGIN_RECT, DF_PLUGIN_RECT, DF_PLUGIN_NONE);
        QdCopyBits((int *)PARG(0), (int *)&gChannels[1].hResource, (short *)PARG(1), (short *)PARG(2));
        break;
    case 0x213:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_HOTSPOT, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        memcpy(PARG(0), &gHotspotBuf, 0x78);           /* the first 30 dwords of gHotspotBuf */
        break;
    case 0x214:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        PRESULT(DF_PLUGIN_INT, GetTick60Hz());
        break;
    case 0x215:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        PumpMessages();
        break;
    case 0x216:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_INT, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        PRESULT(DF_PLUGIN_INT, RandomUpTo((uint)PINT(0)));
        break;
    case 0x217:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_DATA, DF_PLUGIN_INT, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        ZeroBlock((unsigned int *)PARG(0), (unsigned int)PINT(1));
        break;
    case 0x218:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        pHandle = (int *)gActiveClut;
        CloneResource((void **)&pHandle);
        PRESULT(DF_PLUGIN_CLUT, pHandle);
        break;
    case 0x219:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_CLUT, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        ReloadCLUT((int *)PARG(0));
        break;
    case 0x21a:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_STR, DF_PLUGIN_INT, DF_PLUGIN_INT);
        pHandle = (int *)ClonePropFrameImage((byte *)PARG(0), (byte *)PARG(1), (short)PINT(2), (short)PINT(3));
        PRESULT(DF_PLUGIN_RESOURCE, pHandle);
        break;
    case 0x21b:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_STR, DF_PLUGIN_INT, DF_PLUGIN_INT);
        pHandle = (int *)CloneActorFrameImage((byte *)PARG(0), (byte *)PARG(1), (short)PINT(2), (short)PINT(3));
        PRESULT(DF_PLUGIN_RESOURCE, pHandle);
        break;
    case 0x21c:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_RESOURCE, DF_PLUGIN_POINT, DF_PLUGIN_HOTSPOT, DF_PLUGIN_NONE);
        pRect = (short *)PARG(2);
        gClipRect[0] = 0;
        gClipRect[1]    = 0;
        gClipRect[2]    = pRect[0];
        gClipRect[3]    = pRect[1];
        RasterStampChildIntoSceneHotspotBuf((int *)PARG(0), (uint32_t)PINT(1), (int)pRect, 0);
        break;
    case 0x21d:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_RESOURCE, DF_PLUGIN_POINT, DF_PLUGIN_RECT, DF_PLUGIN_NONE);
        ComputeSpriteFrameGeometry((int *)PARG(0), (uint32_t)PINT(1), 0, (short *)PARG(2));
        break;
    case 0x21e:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        GetSceneCursorPos((int)(intptr_t)&gChannelB, (short *)&packedPoint);
        PRESULT(DF_PLUGIN_POINT, packedPoint);
        break;
    case 0x21f:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        PRESULT(DF_PLUGIN_INT, (unsigned int)(int)IsMouseDown());
        break;
    case 0x220:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_INT, DF_PLUGIN_INT, DF_PLUGIN_INT);
        st = BootstrapContainerFile((byte *)PARG(0), PINT(1), (unsigned int)PINT(2), (unsigned int)PINT(3));
        break;
    case 0x221:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        st = DeleteResourceFile((byte *)PARG(0));
        break;
    case 0x222:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_STR, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        WriteSaveGameFile((byte *)PARG(0), (byte *)PARG(1));
        break;
    case 0x223:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_STR, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        if (ReadSaveGameFile((byte *)PARG(0), (byte *)PARG(1)) == 0)
            RestoreSavedGameState();
        break;
    case 0x224:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_HOTSPOT, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        pDst = PARG(0);
        CopyTrigSceneBufferAndViewportPair(image, viewport);
        memcpy(pDst, image, sizeof(image));
        break;
    case 0x225:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_HOTSPOT, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        pDst = PARG(0);
        CopyTrigOverlayBufferAndViewportPair(image, viewport);
        memcpy(pDst, image, sizeof(image));
        break;
    case 0x226:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        st = GetResourceDiskFreeSpace((byte *)PARG(0), &freeBytes);
        pMsg->result.data = (void *)(intptr_t)freeBytes;
        pMsg->result.code = DF_PLUGIN_INT;
        break;
    case 0x227:
        CheckPluginArgTypes(pMsg, DF_PLUGIN_STR, DF_PLUGIN_NONE, DF_PLUGIN_NONE, DF_PLUGIN_NONE);
        st = ProbeContainerFile((byte *)PARG(0), &openErr, &container, &readOnly);
        value = 0;
        if (openErr != 0)
            value = 1;
        if (readOnly != 0)
            value |= 2;
        PRESULT(DF_PLUGIN_INT, value);
        break;
    default:
        st = 0x6d;
    }
    return st;
}
#undef PARG
#undef PINT
#undef PRESULT


/* A plugin request's four argument kinds against what the engine function wants
 * (DispatchPluginEngineRequest); fatal 0x1054..0x1057 on the first mismatch. */
void __cdecl CheckPluginArgTypes(DfPluginMsg *pMsg, short k0, short k1, short k2, short k3)  /* 0x004373a0 */
{
    if (pMsg->arg[0].code != k0) FatalError(0, 0x1054);
    if (pMsg->arg[1].code != k1) FatalError(0, 0x1055);
    if (pMsg->arg[2].code != k2) FatalError(0, 0x1056);
    if (pMsg->arg[3].code != k3) FatalError(0, 0x1057);
}


/* Finds the gPluginDirectory entry whose leading Pascal name equals pDesc (case-insensitive) and
 * copies its 24-byte (6 dword) row to pEntryCtxOut. Returns 0 on a match, 10 if there is none. */
short __cdecl LookupPluginByDescriptor(byte *pDesc, unsigned int *pEntryCtxOut) /* 0x00437410 */
{
    typedef struct { int d[6]; } DirEntry24;
    byte *pRow;
    int   i;          /* declared last: i in edi, zeroed after the call */

    pRow = (byte *)GetResDataPtr((int *)gPluginDirectory);
    for (i = 0; i < gPluginDirectoryCount; i++) {
        if (PascalStringsEqualNoCase(pDesc, pRow)) {
            *(DirEntry24 *)pEntryCtxOut = *(DirEntry24 *)pRow;
            UnlockResObject((int *)gPluginDirectory);
            return 0;
        }
        pRow += 0x18;
    }
    UnlockResObject((int *)gPluginDirectory);
    return 10;
}

