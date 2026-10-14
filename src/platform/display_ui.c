/* display_ui.c -- TI.EXE 0x00419980..0x00421220: palette, display mode, window and modal UI. */

#include <windows.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mmsystem.h>
#include <mmsystem.h>   /* timeGetTime */
#include <malloc.h>
#include <commdlg.h>
#include "df_protos.h"
#include "topics/clut.h"
#include "topics/scene.h"
#include "topics/strutil.h"
#include "topics/mempool.h"
#include "topics/palette.h"
#include "topics/joystick.h"
#include "topics/input.h"
#include "topics/displaymode.h"
#include "topics/dialog.h"
#include "topics/appinit.h"
#include "topics/shutdown.h"
#include "topics/rendbackend.h"
#include "topics/checkdisplay.h"
#include "topics/checkcd.h"
#include "topics/winver.h"
#include "topics/iswinnt.h"
#include "topics/filehandles.h"
#include "topics/cpudetect.h"
#include "topics/font.h"
#include "topics/memory.h"
#include "topics/vxdmem.h"
#include "topics/tick.h"
#include "topics/container.h"
#include "topics/fatal.h"
#include "topics/gameloop.h"
#include "topics/anim.h"
#include "topics/modalui.h"
#include "topics/scriptcmd.h"
#include "topics/reset_channel.h"

static BOOL CALLBACK SaveWindowPositionEnumProc(HWND hwnd, LPARAM lParam);
#if DF_FIX_CMDLINE_BOUNDED_COPY
static void CopyLowerCmdLine(char *dest, size_t destSize, const char *src);
#endif
static int __cdecl FindBestDisplayMode(int nTargetWidth, int nTargetAspect, unsigned int nTargetBpp, int bValidate);
static int __cdecl ProbeWaveOutFormat(DWORD nSamplesPerSec, int nBitsPerSample, int nChannels);
static void __cdecl CopyHotspotRectsBetweenChannelBuffers(short *pSrcHotFields, short *pDstHotFields, short *pRectSrc, short *pRectDst);

/* was file-scope in palette.c */
int s_sysColorsSaved;

/* Clones a CLUT resource: copies it, locks both the original and the copy, and replaces the
 * caller's handle (*ppCLUT, e.g. &gActiveClut) with the new working copy. */
void __cdecl CloneResource(void **ppCLUT)  /* 0x00419980 */
{
    void  *pOrig;
    void  *pCopy;
    DWORD  cbSize;
    void  *pSrc;
    void  *pDst;

    pOrig  = *ppCLUT;
    cbSize = GetResObjectSize(pOrig);
    pCopy  = AllocResObject(cbSize);
    pSrc   = GetResDataPtr(pOrig);
    pDst   = GetResDataPtr(pCopy);
    memmove(pDst, pSrc, cbSize);
    UnlockResObject(pOrig);
    UnlockResObject(pCopy);
    *ppCLUT = pCopy;
}


/* Writes the DWORD-aligned size of a rect {y1, x1, y2, x2}: *pWidthOut = alignedW = ((x2 - x1) +
 * 3) & 0xfffc and *pMetricOut = (y2 - y1) * alignedW, the surface area. InitHotspotBuffer needs it
 * for the set view buffer and the movie buffer.
 *
 * Port note: the port had invented abs() on both spans and dropped the 4-byte alignment, so a
 * mis-read rect came out with width 0, which InitHotspotBuffer silently replaces with its 4-pixel
 * default. */
void __cdecl ComputeAlignedRectSize(short *pRect, int *pMetricOut,
                                              unsigned short *pWidthOut) /* 0x004199f0 */
{
    unsigned int alignedW;

    alignedW    = (((int)pRect[3] - (int)pRect[1]) + 3U) & 0xfffc;
    *pMetricOut = (int)(((int)pRect[2] - (int)pRect[0]) * alignedW);
    *pWidthOut  = (unsigned short)alignedW;
}


/* Turns the null-terminated C string at pDescriptor[1..n] into a Pascal string: counts the
 * characters and writes the count into pDescriptor[0]. Returns pDescriptor. The count is one
 * byte, so a string over 255 characters overflows. */
char * __cdecl UpdatePascalLength(char *pDescriptor)  /* 0x00419a30 */
{
    char  cLen  = 0;
    char *pChar = pDescriptor + 1;

    while (*pChar++ != '\0')        /* the pointer steps before the test is used */
        cLen++;

    pDescriptor[0] = cLen;

    return pDescriptor;
}


/* memset(ptr, 0, size), a DWORD at a time with a byte remainder. */
void __cdecl ZeroBlock(unsigned int *ptr, unsigned int size)  /* 0x00419a50 */
{
    memset(ptr, 0, size);               /* /Oi inlines it: rep stosd, then rep stosb for the tail */
}


/* Writes interpolated CLUT data to the logical palette (entries 10-245 in 8bpp, otherwise 1-254),
 * conditionally updates the per-viewport DCs (Win 3.1x) and the DIB color table, and always
 * calls PaletteUpdateFinalizer. */
void __cdecl CommitInterpolatedCLUTPalette(int pRawData)  /* 0x00419A70 */
{
    /* The CLUT entry (Mac ColorSpec: value, red, green, blue), copied whole */
    typedef struct { unsigned short value; unsigned short red, green, blue; } ColorSpec;
    ColorSpec cs;
    int       i;
    HDC       hdc;
    int      *pSlot;
    int       n;

    if (pRawData != 0) {
        if (gCustomPaletteFlag != 0) {
            for (i = 1; i < 0xff; i++) {
                cs = ((ColorSpec *)pRawData)[i + 1];
                SetPaletteEntry16(i, cs.red, cs.green, cs.blue);
            }
        } else {
            for (i = 10; i < 0xf6; i++) {
                cs = ((ColorSpec *)pRawData)[i + 1];
                SetPaletteEntry16(i, cs.red, cs.green, cs.blue);
            }
        }
        SyncLogicalPalette(1);
    }
    if (gIsWin31 != 0) {
        n = 0;
        if (n < gViewportCount) {
            /* Same dangling-pointer class as ApplyCLUTToPalette's walk:
             * DF_VIEWPORT_SLOT_OK (IsPointerReadable, VirtualQuery-based). */
            pSlot = &gViewportTable;
            do {
                if (DF_VIEWPORT_SLOT_OK((void *)*pSlot, sizeof(int) * 2) &&
                    *(int *)*pSlot == gMainWindow) {
                    hdc = (HDC)((int *)*pSlot)[1];
                    SelectPalette(hdc, gPalette, 0);
                    RealizePalette(hdc);
                }
                pSlot++;
                n++;
            } while (gViewportCount > n);
        }
    }
    if (gIsSceneBitmapSelected != 0)
        FlushDIBColorTable((HDC)(UINT_PTR)gBackBufferDC, 0, 0x100, gDibColorTable);
    PaletteUpdateFinalizer();
}


/* Pushes gDibColorTable[] to the active back end after a palette update, through a stored
 * procedure pointer: WinGSetDIBColorTable (gWinGSetDIBColorTableProc) when gIsWinGBackend != 0,
 * otherwise SetDIBColorTable (gSetDIBColorTableProc). Called from ApplyCLUTToPalette and
 * FadeCLUTFromBlack when gIsSceneBitmapSelected != 0, as FlushDIBColorTable(gBackBufferDC, 0,
 * 0x100, gDibColorTable). A plain 4-argument __cdecl pass-through thunk (0x00419ba0 ends with a bare
 * ret).
 *
 * Port note: it was declared with no arguments, so SetDIBColorTable read garbage off the stack and
 * the palette never reached the DIB (solid black). */
void __cdecl FlushDIBColorTable(HDC hdc, UINT iStart, UINT cEntries,
                                const RGBQUAD *prgbq)  /* 0x00419ba0 */
{
    DF_TRACE("FlushDIBColorTable: hdc=%p n=%u  pal[1]=%02x%02x%02x pal[200]=%02x%02x%02x",
             (void *)hdc, cEntries,
             prgbq[1].rgbRed, prgbq[1].rgbGreen, prgbq[1].rgbBlue,
             prgbq[200].rgbRed, prgbq[200].rgbGreen, prgbq[200].rgbBlue);
    if (gIsWinGBackend != 0) {
        ((PFN_SetDIBColorTable)(UINT_PTR)gWinGSetDIBColorTableProc)(hdc, iStart, cEntries, prgbq);
        return;
    }
    ((PFN_SetDIBColorTable)(UINT_PTR)gSetDIBColorTableProc)(hdc, iStart, cEntries, prgbq);
}


/* A no-op in the original too: always called after FlushDIBColorTable in ApplyCLUTToPalette. */
void __stdcall PaletteUpdateFinalizer(void)  /* 0x00419bd0 */
{
    return;
}


/* Writes palette entry idx from 16-bit (0..0xffff) colour components with piecewise-linear gamma
 * correction to 8 bits, updating all three palette tables (gPal16, gDibColorTable, gPalEntries):
 * out8 = gammaBase[val >> 8] + (short)gammaSlope[val >> 8] * (val & 0xff) >> 8. Entry 0 is forced
 * to (0,0,0) and entry 255 to (0xffff,0xffff,0xffff), both with peFlags 0; every other entry has
 * peFlags 5 (PC_RESERVED | PC_NOCOLLAPSE). Calls BuildGammaTables first if gGammaTablesBuilt is 0.
 * Four arguments: every caller in the exe pushes four ([esp+4..0x10]); the port's seven were
 * Ghidra's phantom slots (three leading copies). */
void SetPaletteEntry16(int idx, int r, int g, int b)  /* 0x00419be0 */
{
    int r8;    /* gamma-corrected 8-bit red   */
    int b8;    /* gamma-corrected 8-bit blue  */
    int g8;    /* gamma-corrected 8-bit green */
    int lo;    /* low byte of the component: the exe masks a copy and shifts v in place */
    int g_a;    /* unused: the compiled copy declares the original's 16 unused frame bytes here (line_patches.json) */

    if (gGammaTablesBuilt == 0)
        BuildGammaTables();

    /* Clamp anchor entries (chained: the exe loads b's register first) */
    if (idx == 0)
        r = g = b = 0;
    else if (idx == 0xff)
        r = g = b = 0xffff;

    gPal16[idx].wRed   = (short)r;
    gPal16[idx].wGreen = (short)g;
    gPal16[idx].wBlue  = (short)b;

    /* Piecewise-linear gamma correction to 8-bit. The index variable is reused for the base
     * entry: that stops C2 substituting the base load forward, so it is loaded into the
     * result's register after the multiply, as in the exe. */
    lo = r & 0xff; r >>= 8;
    lo = gGammaSlopeR[r] * lo >> 8; r = gGammaBaseR[r]; r += lo; r8 = r;
    lo = g & 0xff; g >>= 8;
    lo = gGammaSlopeG[g] * lo >> 8; g = gGammaBaseG[g]; g += lo; g8 = g;
    lo = b & 0xff; b >>= 8;
    lo = gGammaSlopeB[b] * lo >> 8; b = gGammaBaseB[b]; b += lo; b8 = b;

    if (idx == 1 || idx == 64 || idx == 200) {
        DF_TRACE("palEntry[%d]: rgb=(%02x,%02x,%02x) built=%d",
                 idx, (unsigned char)r8, (unsigned char)g8, (unsigned char)b8,
                 (int)gGammaTablesBuilt);
    }
    /* RGBQUAD DIB color table: layout is {Blue, Green, Red, Reserved} */
    gDibColorTable[idx].rgbRed      = r8;
    gDibColorTable[idx].rgbGreen    = g8;
    gDibColorTable[idx].rgbBlue     = b8;
    gDibColorTable[idx].rgbReserved = 0;

    gPalEntries[idx].peRed   = r8;
    gPalEntries[idx].peGreen = g8;
    gPalEntries[idx].peBlue  = b8;
    if (idx != 0 && idx != 0xff)
        gPalEntries[idx].peFlags = 5;   /* PC_RESERVED | PC_NOCOLLAPSE */
    else
        gPalEntries[idx].peFlags = 0;
}


/* PORT-HELPER: reapplies the engine CLUT blob after a modal dialog is torn down. The binary
 * computes this as gPal16 - 0x0A (offset 0x00487540); it has no function of its own. */
void __cdecl RestorePaletteAfterModalCover(void)
{
    ApplyCLUTToPalette((int)(INT_PTR)((char *)gPal16 - 0x0A));
}

/* Builds the six piecewise-linear gamma tables SetPaletteEntry16 uses to map 16-bit components to
 * 8 bits: base[i] = (short)(pow(i / 255.0, gamma) * 255.0) for each channel, then the
 * finite-difference slopes (indices 0-254, the last 0). Guarded by gGammaTablesBuilt. */
void __cdecl BuildGammaTables(void)  /* 0x00419d10 */
{
    int    i;
    double v;

    /* pow() is the CRT's x87 intrinsic in the exe (_CIpow via 0x0044e652, FLD ST0 keeps v for
     * the next channel); an earlier port had a sqrt() placeholder that made every gamma table
     * zero, rendering all black. */
    gGammaTablesBuilt = 1;

    for (i = 0; i < 0x100; i++) {
        v = i * GAMMA_IN_SCALE;
        gGammaBaseR[i] = (short)(pow(v, gGammaR) * GAMMA_OUT_SCALE);
        gGammaBaseG[i] = (short)(pow(v, gGammaG) * GAMMA_OUT_SCALE);
        gGammaBaseB[i] = (short)(pow(v, gGammaB) * GAMMA_OUT_SCALE);
    }
    for (i = 0; i < 0xff; i++) {
        gGammaSlopeR[i] = gGammaBaseR[i + 1] - gGammaBaseR[i];
        gGammaSlopeG[i] = gGammaBaseG[i + 1] - gGammaBaseG[i];
        gGammaSlopeB[i] = gGammaBaseB[i + 1] - gGammaBaseB[i];
    }
    gGammaSlopeR[255] = 0;
    gGammaSlopeG[255] = 0;
    gGammaSlopeB[255] = 0;
}


/* Creates the Win32 logical HPALETTE on the first call, then keeps it in sync with gPalEntries[],
 * the palPalEntry[] member of a LOGPALETTE whose header (palVersion 0x0300, palNumEntries 0x0100,
 * set in AppInit) is the two WORDs before it. animate 0: the first call (create) or Win3.x,
 * CreatePalette / SetPaletteEntries; 1: later calls on NT / Win9x, AnimatePalette (a smooth
 * hardware update). gPalette is zeroed from AppInit before the first call and deleted in Shutdown. */
void __cdecl SyncLogicalPalette(int animate)  /* 0x00419e00 */
{
    if (gPalette == NULL) {
        gPalette = CreatePalette(&gLogPal);
        return;
    }

    if ((animate != 0) && (gIsWin31 == 0)) {
        AnimatePalette(gPalette, 0, 256, gPalEntries);
        return;
    }

    SetPaletteEntries(gPalette, 0, 256, gPalEntries);
}


/* Applies CLUT colour data to the Win32 logical palette and realises the palette on every viewport
 * HDC. pClut points at the CLUT data (8-byte entries from base + 8); 0 runs only the viewport
 * realize and the post-processing. In 8-bit mode (gCustomPaletteFlag == 0) entries 10-245 are
 * applied, in 16 / 24-bit mode entries 1-254; SyncLogicalPalette(0) then pushes to the hardware
 * and each viewport HDC gets SelectPalette / RealizePalette. */
void __cdecl ApplyCLUTToPalette(int pClut)  /* 0x00419e60 */
{
    /* The CLUT entry (Mac ColorSpec: value, red, green, blue), copied whole */
    typedef struct { unsigned short value; unsigned short red, green, blue; } ColorSpec;
    ColorSpec cs;
    int       i;
    HDC       hdc;
    int      *pSlot;
    int       n;

    DF_TRACE("ApplyCLUTToPalette: gate_c28=%d", (int)gIsSceneBitmapSelected);

    if (pClut != 0) {
        if (gCustomPaletteFlag != 0) {
            for (i = 1; i < 0xff; i++) {
                cs = ((ColorSpec *)pClut)[i + 1];
                SetPaletteEntry16(i, cs.red, cs.green, cs.blue);
            }
        } else {
            for (i = 10; i < 0xf6; i++) {
                cs = ((ColorSpec *)pClut)[i + 1];
                SetPaletteEntry16(i, cs.red, cs.green, cs.blue);
            }
        }
        SyncLogicalPalette(0);
    }

    /* Realise palette on every viewport HDC. DF_VIEWPORT_SLOT_OK guards
     * against dangling pointers: DestroyChannelViewport clears entries to NULL
     * but doesn't decrement count (IsPointerReadable is VirtualQuery-based). */
    n = 0;
    if (n < gViewportCount) {
        pSlot = (int *)&gViewportTable;
        do {
            if (DF_VIEWPORT_SLOT_OK((void *)*pSlot, 4 + sizeof(HDC))) {
                hdc = *(HDC *)(*pSlot + 4);
                SelectPalette(hdc, gPalette, 0);
                RealizePalette(hdc);
            }
            pSlot++;
            n++;
        } while (gViewportCount > n);
    }

    if (gIsSceneBitmapSelected != 0) {
        FlushDIBColorTable((HDC)(UINT_PTR)gBackBufferDC, 0, 0x100, gDibColorTable);
    }
    PaletteUpdateFinalizer();
}


/* Loads string resource stringId with LoadStringA and replaces every "%p" (kStrPctP) with the
 * application title (gAppTitle, string resource 0, loaded by AppInit). The result is in the static
 * gStringResourceBuf (384 bytes): consume or copy it before the next call. */
LPCSTR __cdecl GetStringResource(UINT stringId)  /* 0x00419f80 */
{
    char  szTemp[384];
    char *pMatch;

    gStringResourceBuf[0] = '\0';
    LoadStringA(gInstance, stringId, (LPSTR)gStringResourceBuf, 0x180);

    for (;;)
    {
        pMatch = strstr(gStringResourceBuf, kStrPctP);
        if (pMatch == NULL)
            return (LPCSTR)gStringResourceBuf;

        *pMatch = '\0';

        strcpy(szTemp, gStringResourceBuf);
        strcat(szTemp, gAppTitle);
        strcat(szTemp, pMatch + 2);      /* skip 2-char "%p" */

        strcpy(gStringResourceBuf, szTemp);
    }
}


/* Reformats a string for a message box by inserting '\n' at word boundaries, at most 40 characters
 * (0x28) per line. pDst must hold at least 512 bytes (ShowOKCancelBox's local_200). */
void __cdecl WordWrapText(char *pSrc, char *pDst)  /* 0x0041a090 */
{
    int  nRemain;
    int  iInner;
    char cChar;

    nRemain = strlen(pSrc);
    for (;;) {
        for (iInner = 0; nRemain != 0 && iInner < 0x28; iInner++) {
            cChar   = *pSrc++;
            *pDst++ = cChar;
            nRemain--;
            if (cChar == 0x0a)
                break;
        }
        if (iInner == 0x28) {
            /* Hit the 40-char limit -- back up to the last space */
            while (*pSrc != 0 && *pSrc != 0x20) {
                pSrc--;
                nRemain++;
                pDst--;
            }
            *pDst++ = 0x0a;
        }
        cChar = *pSrc;
        if (cChar == 0)
            break;
        /* Skip leading spaces at the start of the next line */
        if (iInner == 0x28 && cChar == 0x20) {
            do {
                pSrc++;
                nRemain--;
            } while (*pSrc == 0x20);
        }
    }
    *pDst = 0;
}


/* Detects the first plugged-in joystick (0, else 1) unless the command line disabled it
 * (gNoJoystickFlag), records its axis centre and range and the deflect / release thresholds (160000
 * / 90000), and if the stick is not centred asks whether to continue without it (string resources
 * 0xbe and 0xbd). An answer of 1 switches the joystick off; a negative answer returns 1, otherwise 0. */
short __stdcall InitJoystick(void)  /* 0x0041a100 */
{
    JOYINFO  ji;
    JOYCAPSA caps;
    char     szBuf[256];
    int      idx;
    int      answer;

    gJoystickPresent = 0;

    if (gNoJoystickFlag != 0)
        return 0;
    if (joyGetNumDevs() == 0)
        return 0;
    if (joyGetPos(0, &ji) != JOYERR_UNPLUGGED)
        idx = 0;
    else if (joyGetPos(1, &ji) != JOYERR_UNPLUGGED)
        idx = 1;
    else
        return 0;
    gJoystickId = idx;
    if (joyGetDevCapsA(idx, &caps, sizeof(JOYCAPSA)) != JOYERR_NOERROR)
        return 0;
    gJoystickPresent = 1;
    gJoystickCenterX = (caps.wXmin + caps.wXmax) >> 1;
    gJoystickCenterY = (caps.wYmin + caps.wYmax) >> 1;
    gJoystickRangeX = caps.wXmax - gJoystickCenterX;
    gJoystickRangeY = caps.wYmax - gJoystickCenterY;
    gJoystickReleaseThreshold = 90000;
    gJoystickDeflectThreshold = 160000;
    gJoystickDirection = 0;   /* no address-of: taking it made C2 treat the global as aliased unit-wide */
    /* direction not centred: ask whether to continue without the joystick.
     * 0xbe is copied (GetStringResource reuses one buffer), 0xbd passed as is. */
    if ((short)PollJoystickDirection() != 0) {
        strcpy(szBuf, GetStringResource(0xbe));
        answer = ShowYesNoDialog(szBuf, GetStringResource(0xbd));
        if (answer < 0)
            return 1;
        if (answer == 1)
            gJoystickPresent = 0;
    }
    return 0;
}


/* Returns the active joystick's (gJoystickId) button bitmask from joyGetPos, or 0 on failure. */
unsigned short __stdcall PollJoystickButtons(void)  /* 0x0041a290 */
{
    MMRESULT mmr;
    JOYINFO  ji;

    mmr = joyGetPos((UINT)gJoystickId, &ji);
    if (mmr != JOYERR_NOERROR)
        return 0;

    return (unsigned short)ji.wButtons;
}


/* Reads the joystick position and returns an 8-direction code with hysteresis: from the centre (0)
 * the stick must exceed gJoystickDeflectThreshold (160000) to deflect, and a deflected stick stays
 * so until its distance is below gJoystickReleaseThreshold (90000). Codes: 0 centre, 1 up, 2 down,
 * 4 left, 8 right, diagonals the sum. Returns 0 if joyGetPos fails. Signed divisions and compares
 * throughout (cdq; idiv / jle in the exe). The switches have no default: an impossible quadrant
 * returns the uninitialised `dir`, as the exe does. */
short __stdcall PollJoystickDirection(void)  /* 0x0041a2c0 */
{
    JOYINFO  ji;
    int      dx;
    int      dy;
    int      sx;
    int      sy;
    int      dist;
    int      quadrant;
    short    dir;

    if (joyGetPos((UINT)gJoystickId, &ji) != JOYERR_NOERROR)
        return 0;

    quadrant = 2;   /* assume right-down */
    dx = (int)ji.wXpos - (int)gJoystickCenterX;
    dy = (int)ji.wYpos - (int)gJoystickCenterY;
    if (dx < 0) { dx = -dx; quadrant = 3; }     /* left half */
    if (dy < 0) { dy = -dy; quadrant ^= 2; }    /* upper half */

    sx = dx * 1000 / (int)gJoystickRangeX;
    sx *= sx;       /* the exe squares sx before it computes sy */
    sy = dy * 1000 / (int)gJoystickRangeY;
    sy *= sy;
    dist = sx + sy;

    if (gJoystickDirection != 0) {
        if (dist < (int)gJoystickReleaseThreshold) {
            gJoystickDirection = 0;
            return 0;
        }
    } else if (dist < (int)gJoystickDeflectThreshold)
        return 0;

    if (dx < (int)gJoystickRangeX * dy * 2 / ((int)gJoystickRangeY * 5)) {
        switch (quadrant) {
        case 0: case 1: dir = 1; break;
        case 2: case 3: dir = 2; break;
        }
    } else if (dy < (int)gJoystickRangeY * dx * 2 / ((int)gJoystickRangeX * 5)) {
        switch (quadrant) {
        case 0: case 2: dir = 8; break;
        case 1: case 3: dir = 4; break;
        }
    } else {
        switch (quadrant) {
        case 0: dir = 9;  break;
        case 1: dir = 5;  break;
        case 2: dir = 10; break;
        case 3: dir = 6;  break;
        }
    }
    gJoystickDirection = dir;
    return dir;
}


/* Enqueues one input event into the 32-entry game event queue (gEventQueue, 18-byte GameEvent
 * entries). eventType maps to a bitmask stored at entry +0x10: 1 -> 0x02, 2 -> 0x04, 3 -> 0x08,
 * 4 -> 0x10, 5 -> 0x20, 6 -> 0x40, 9 -> 0x80, 0x2a -> 0x100. If isRepeat is non-zero, any queued
 * event with the same type bitmask is removed first (FindQueueEntry + DequeueGameEvent). On
 * overflow (count above 31) the first entry whose flag byte has bit 0x20 set is evicted.
 * Entry: +0x00 short eventType, +0x02 int buttonId, +0x06 uint timestamp (timeGetTime() * 3 / 50),
 * +0x0a int payload (*pPayload if non-NULL), +0x0e short extraParam, +0x10 ushort type bitmask. */
void __cdecl EnqueueInputEvent(int isRepeat, int eventType,  /* 0x0041a450 */
                               int buttonId, unsigned int *pPayload, int extraParam)
{
    unsigned short typeMask;
    unsigned int   eventBuf[5];  /* 20-byte buffer for a full 18-byte event entry */
    short          found;
    short          si;
    int            last;
    GameEvent     *pE;

    switch (eventType) {
    case 1:  typeMask = 0x0002; break;
    case 2:  typeMask = 0x0004; break;
    case 3:  typeMask = 0x0008; break;
    case 4:  typeMask = 0x0010; break;
    case 5:  typeMask = 0x0020; break;
    case 6:  typeMask = 0x0040; break;
    case 9:  typeMask = 0x0080; break;
    case 0x2a: typeMask = 0x0100; break;
    default: typeMask = 0x0000; break;
    }

    if (isRepeat != 0) {
        found = FindQueueEntry(typeMask, eventBuf);
        while (found != 0 && buttonId == *(int *)((char *)eventBuf + 2)) {
            DequeueGameEvent(typeMask, eventBuf);
            found = FindQueueEntry(typeMask, eventBuf);
        }
    }

    /* Overflow eviction (0x0041a4ee). The compaction walks the outer index
     * itself, so the scan ends after the first entry flagged 0x20 is removed:
     * one eviction per enqueue, not all flagged entries. */
    if (gEventQueueCount >= 0x20) {
        for (si = 0; si < gEventQueueCount; si++) {
            if (*(byte *)&EVENT_QUEUE[si].typeMask & 0x20) {
                last = gEventQueueCount - 1;
                for (; si < last; si++)
                    EVENT_QUEUE[si] = EVENT_QUEUE[si + 1];
                gEventQueueCount = last;
            }
        }
        if (gEventQueueCount >= 0x20)
            gEventQueueCount = 0x1f;
    }

    pE = &EVENT_QUEUE[gEventQueueCount++];
    pE->eventType = (short)eventType;
    pE->typeMask = typeMask;
    *(int *)&pE->field_2 = buttonId;
    if (pPayload != NULL)
        *(unsigned int *)&pE->pNode = *pPayload;
    pE->modifiers = (short)extraParam;
    *(unsigned int *)&pE->field_6 = (timeGetTime() * 3) / 50;
}


/* Scans the event queue for the first entry whose type field matches typeMask (0xffff matches
 * any). If found, copies the 18-byte entry to pEventOut, shifts the remaining entries left,
 * decrements the count and returns 1; otherwise returns 0. The queue is gEventQueue (0x00486970),
 * 18-byte entries with the type field at +0x10. Ghidra shows 5 parameters, three of them register
 * artefacts; there are 2. */
short __cdecl DequeueGameEvent(unsigned short typeMask, unsigned int *pEventOut)  /* 0x0041a630 */
{
    int             last;
    int             i;
    int             n;
    unsigned short *puFlags;
    GameEvent      *pEntry;
    GameEvent      *pNext;

    i = 0;
    if (i < gEventQueueCount) {
        puFlags = (unsigned short *)&gEventQueue[0].typeMask;
        do {
            if (typeMask == 0xffff || (*puFlags & typeMask) != 0) {
                /* copy the 18-byte entry out as one packed GameEvent, then close
                 * the gap entry by entry; the new count is formed first */
                pEntry = &gEventQueue[i];
                *(GameEvent *)pEventOut = *pEntry;
                last = gEventQueueCount - 1;
                if (i < last) {
                    n = gEventQueueCount - i - 1;
                    do {
                        pNext = pEntry + 1;
                        n--;
                        *pEntry = *pNext;
                        pEntry = pNext;
                    } while (n != 0);
                }
                gEventQueueCount = last;
                return 1;
            }
            puFlags += 9;
            i++;
        } while (gEventQueueCount > i);
    }
    return 0;
}


/* Scans the event queue for the first entry whose type-flags field matches typeMask (a bitwise AND,
 * or 0xffff for any) and copies its 18 bytes to pEventOut (at least 18 bytes), returning 1; returns
 * 0 if there is none. Does not remove the entry (compare DequeueGameEvent). */
short FindQueueEntry(unsigned short typeMask, unsigned int *pEventOut)  /* 0x0041a6e0 */
{
    int             i;
    unsigned short *puFlags;

    i = 0;
    if (i < gEventQueueCount) {
        puFlags = (unsigned short *)&gEventQueue[0].typeMask;
        do {
            /* the 18-byte entry is copied as one packed GameEvent */
            if (typeMask == 0xffff || (*puFlags & typeMask) != 0) {
                *(GameEvent *)pEventOut = gEventQueue[i];
                return 1;
            }
            puFlags += 9;
            i++;
        } while (gEventQueueCount > i);
    }
    return 0;
}


/* Polls both joysticks for button changes and enqueues press (3), release (4) and repeat (5)
 * events through EnqueueInputEvent; a held button repeats after 200 ms. Button ids: joystick 1
 * bits 0-3 -> 0x1E, 0x1F, 0x1C, 0x1D; joystick 2 -> 0x18, 0x19, 0x1A, 0x1B. Called every frame by
 * PollEventQueue. Ghidra shows EnqueueInputEvent with 8 parameters, but 2 and 3 are stale ECX /
 * EDX values; it takes 6. */
void __stdcall PollJoystickInput(void)  /* 0x0041a750 */
{
    unsigned int joy1Buttons;
    unsigned int joy2Buttons;
    DWORD        now;
    int          repeatExpired;
    unsigned int bit;

    if (gJoystickPresent == 0)
        return;

    joy1Buttons = (unsigned int)(short)PollJoystickDirection();
    joy2Buttons = (unsigned int)(short)PollJoystickButtons();

    now = timeGetTime();
    if ((int)(gJoyRepeatTime + 200) < (int)now) {
        repeatExpired = 1;
        gJoyRepeatTime = now;
    } else
        repeatExpired = 0;

    bit = joy1Buttons & 1;
    if (bit) {
        if (!(gJoy1PrevButtons & 1))
            EnqueueInputEvent(0, 3, 0x1E, NULL, 0);
        else if (repeatExpired)
            EnqueueInputEvent(1, 5, 0x1E, NULL, 0);
    }
    if (!bit && (gJoy1PrevButtons & 1))
        EnqueueInputEvent(0, 4, 0x1E, NULL, 0);

    bit = joy1Buttons & 2;
    if (bit) {
        if (!(gJoy1PrevButtons & 2))
            EnqueueInputEvent(0, 3, 0x1F, NULL, 0);
        else if (repeatExpired)
            EnqueueInputEvent(1, 5, 0x1F, NULL, 0);
    }
    if (!bit && (gJoy1PrevButtons & 2))
        EnqueueInputEvent(0, 4, 0x1F, NULL, 0);

    bit = joy1Buttons & 4;
    if (bit) {
        if (!(gJoy1PrevButtons & 4))
            EnqueueInputEvent(0, 3, 0x1C, NULL, 0);
        else if (repeatExpired)
            EnqueueInputEvent(1, 5, 0x1C, NULL, 0);
    }
    if (!bit && (gJoy1PrevButtons & 4))
        EnqueueInputEvent(0, 4, 0x1C, NULL, 0);

    bit = joy1Buttons & 8;
    if (bit) {
        if (!(gJoy1PrevButtons & 8))
            EnqueueInputEvent(0, 3, 0x1D, NULL, 0);
        else if (repeatExpired)
            EnqueueInputEvent(1, 5, 0x1D, NULL, 0);
    }
    if (!bit && (gJoy1PrevButtons & 8))
        EnqueueInputEvent(0, 4, 0x1D, NULL, 0);

    bit = joy2Buttons & 1;
    if (bit) {
        if (!(gJoy2PrevButtons & 1))
            EnqueueInputEvent(0, 3, 0x18, NULL, 0);
        else if (repeatExpired)
            EnqueueInputEvent(1, 5, 0x18, NULL, 0);
    }
    if (!bit && (gJoy2PrevButtons & 1))
        EnqueueInputEvent(0, 4, 0x18, NULL, 0);

    bit = joy2Buttons & 2;
    if (bit) {
        if (!(gJoy2PrevButtons & 2))
            EnqueueInputEvent(0, 3, 0x19, NULL, 0);
        else if (repeatExpired)
            EnqueueInputEvent(1, 5, 0x19, NULL, 0);
    }
    if (!bit && (gJoy2PrevButtons & 2))
        EnqueueInputEvent(0, 4, 0x19, NULL, 0);

    bit = joy2Buttons & 4;
    if (bit) {
        if (!(gJoy2PrevButtons & 4))
            EnqueueInputEvent(0, 3, 0x1A, NULL, 0);
        else if (repeatExpired)
            EnqueueInputEvent(1, 5, 0x1A, NULL, 0);
    }
    if (!bit && (gJoy2PrevButtons & 4))
        EnqueueInputEvent(0, 4, 0x1A, NULL, 0);

    bit = joy2Buttons & 8;
    if (bit) {
        if (!(gJoy2PrevButtons & 8))
            EnqueueInputEvent(0, 3, 0x1B, NULL, 0);
        else if (repeatExpired)
            EnqueueInputEvent(1, 5, 0x1B, NULL, 0);
    }
    if (!bit && (gJoy2PrevButtons & 8))
        EnqueueInputEvent(0, 4, 0x1B, NULL, 0);

    gJoy1PrevButtons = joy1Buttons;
    gJoy2PrevButtons = joy2Buttons;
}


/* --- the window procedures, 0x0041aa50..0x0041b29f (moved here from wndproc.c:
 *     they sit between PollJoystickInput and AdjustRgbGammaScales in TI.EXE) --- */
#if DF_FIX_VIEWPORT_SLOT_GUARD
/* PORT-HELPER: the probe behind DF_VIEWPORT_SLOT_OK (df_fixes.h); no original counterpart.
 * IsBadReadPtr is documented by Microsoft as unreliable and deprecated: it works by attempting
 * the read and catching the exception internally, which can itself crash (confirmed live: it
 * segfaulted inside kernel32 on a sufficiently invalid pointer). VirtualQuery queries the OS's
 * page-table state directly with no speculative read, so it cannot fault the same way. */
int __cdecl IsPointerReadable(const void *p, size_t size)
{
    MEMORY_BASIC_INFORMATION mbi;

    if (p == NULL)
        return 0;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0)
        return 0;
    if (mbi.State != MEM_COMMIT)
        return 0;
    if (mbi.Protect == PAGE_NOACCESS || (mbi.Protect & PAGE_GUARD) != 0)
        return 0;
    if ((char *)p + size > (char *)mbi.BaseAddress + mbi.RegionSize)
        return 0;
    return 1;
}
#endif /* DF_FIX_VIEWPORT_SLOT_GUARD */

/* Window procedure of the DFCOVER class: only WM_PAINT does anything, filling with the black
 * brush. */
LRESULT CALLBACK WndProc_DFCOVER(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) /* 0x0041aa50 */
{
    PAINTSTRUCT ps;

    if (msg != WM_PAINT)
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    BeginPaint(hwnd, &ps);
    FillRect(ps.hdc, &ps.rcPaint, (HBRUSH)(UINT_PTR)gBlackBrush);
    EndPaint(hwnd, &ps);
    return 0;
}

/* Window procedure of the DFWINDOW class: viewport routing and the input queue. A switch on msg
 * (the exe's compare tree, case bodies in message order). Ground truth 0x0041aac0:
 * the key-release case is WM_KEYUP (cmp ecx,0x101), not WM_CHAR; both key cases zero
 * keyState[VK_CONTROL / VK_RCONTROL / VK_LCONTROL] before ToAscii; the click point is LOWORD /
 * HIWORD (unsigned); GetScrollRange is called once and the scroll position is a short. The
 * per-case locals are block-scoped: the exe packs PAINTSTRUCT, keyState, SCROLLINFO, the POINT
 * and the command word into one 256-byte slot. */
LRESULT CALLBACK WndProc_DFWINDOW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) /* 0x0041aac0 */
{
    int   vpi;
    void *pSlot;
    int   mod;      /* one variable for every case: edi in the exe throughout */
    int   vk;       /* the key cases' copy of wParam (an int: keeps the KEYUP copy in ebp) */

    for (vpi = 0; vpi < gViewportCount; vpi++) {
        if (DF_VIEWPORT_SLOT_OK(gViewportTable[vpi], sizeof(HWND)) &&
            *(HWND *)gViewportTable[vpi] == hwnd)
            break;
    }
    if (vpi == gViewportCount)
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    pSlot = gViewportTable[vpi];

    switch (msg) {
    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);

    case WM_SETFOCUS:
        if (gMainWindow == 0)
            return 0;
        if (gMainWindow == (int)(INT_PTR)hwnd)
            return 0;
        SetFocus((HWND)gMainWindow);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;

        BeginPaint(hwnd, &ps);
        DF_TRACE("WM_PAINT: vpi=%d gate358=%d cover=%p isMain=%d",
                 vpi, gIsAppActive, (void *)gCoverWindow,
                 (int)((HWND)gMainWindow == hwnd));
        if ((HWND)gMainWindow == hwnd && gCoverWindow != NULL &&
            IsWindowVisible(gCoverWindow)) {
            FillRect(ps.hdc, &ps.rcPaint, (HBRUSH)gBlackBrush);
            ShowWindow(gCoverWindow, SW_HIDE);
        } else if (gIsAppActive != 0)
            EnqueueInputEvent(1, 6, (int)pSlot, NULL, 0);
        else
            FillRect(ps.hdc, &ps.rcPaint, (HBRUSH)gBlackBrush);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CLOSE:
        if (gMainWindow != 0 && (HWND)gMainWindow != hwnd) {
            short cmd[2];

            cmd[0] = (short)0xf830;
            cmd[1] = (short)vpi;
            EnqueueInputEvent(0, 1, 0, (unsigned int *)cmd, 0);
            return 0;
        }
        gGamePaused = 0;
        gGameLoopState = 1;
        return 0;

    case WM_ACTIVATEAPP:
        HandleActivateAppMessage((BOOL)wParam);
        return 0;

    case WM_DISPLAYCHANGE:
        if (wParam != 0)
            EnqueueInputEvent(1, 0x2a, 0, NULL, 0);
        return 0;

    {
        BYTE  keyState[256];
        WORD  ascii;
        char  ch;

    case WM_KEYDOWN: {
        UINT  repeat;
        UINT  scan;
        short st;

        /* lParam is loaded once and copied (the copy becomes scan); the masks
         * are applied after the GetAsyncKeyState call, as in the exe */
        mod = 0;
        vk = wParam;
        scan = lParam;
        repeat = scan;
        st = GetAsyncKeyState(VK_CONTROL);
        scan = (scan & 0xff0000) >> 16;
        repeat &= 0x40000000;
        if (st < 0)
            mod = 0x1fa0;
        switch (vk) {
        case 27:       ch = '.'; mod |= 0x1fa0; break;
        case VK_LEFT:  ch = '\x1c'; break;
        case VK_UP:    ch = '\x1e'; break;
        case VK_RIGHT: ch = '\x1d'; break;
        case VK_DOWN:  ch = '\x1f'; break;
        case VK_F1:    AdjustRgbGammaScales(0, 1, 1, 1); ch = 0; break;
        case VK_F2:    AdjustRgbGammaScales(1, 1, 1, 1); ch = 0; break;
        case VK_F3:    AdjustRgbGammaScales(0, 1, 0, 0); ch = 0; break;
        case VK_F4:    AdjustRgbGammaScales(1, 1, 0, 0); ch = 0; break;
        case VK_F5:    AdjustRgbGammaScales(0, 0, 1, 0); ch = 0; break;
        case VK_F6:    AdjustRgbGammaScales(1, 0, 1, 0); ch = 0; break;
        case VK_F7:    AdjustRgbGammaScales(0, 0, 0, 1); ch = 0; break;
        case VK_F8:    AdjustRgbGammaScales(1, 0, 0, 1); ch = 0; break;
        case VK_F9:    AdjustRgbGammaScales(0, 0, 0, 0); ch = 0; break;
        case VK_F12:   ShowEngineAboutDialog(); ch = 0; break;
        default:
            GetKeyboardState(keyState);
            keyState[VK_CONTROL] = 0;
            keyState[VK_RCONTROL] = 0;
            keyState[VK_LCONTROL] = 0;
            if (ToAscii(wParam, scan, keyState, &ascii, 0) == 1)
                ch = (char)ascii;
            else
                ch = 0;
            break;
        }
        if (ch != 0) {
            if (repeat != 0)
                EnqueueInputEvent(1, 5, ch, NULL, mod);
            else
                EnqueueInputEvent(0, 3, ch, NULL, mod);
        }
        return 0;
    }

    case WM_KEYUP: {
        short st;
        UINT  scan;

        mod = 0;
        vk = wParam;
        scan = (lParam & 0xff0000) >> 16;
        st = GetAsyncKeyState(VK_CONTROL);
        if (st < 0)
            mod = 0x1fa0;
        switch (wParam) {
        case 27:       ch = '.'; mod |= 0x1fa0; break;
        case VK_LEFT:  ch = '\x1c'; break;
        case VK_UP:    ch = '\x1e'; break;
        case VK_RIGHT: ch = '\x1d'; break;
        case VK_DOWN:  ch = '\x1f'; break;
        default:
            GetKeyboardState(keyState);
            keyState[VK_CONTROL] = 0;
            keyState[VK_RCONTROL] = 0;
            keyState[VK_LCONTROL] = 0;
            if (ToAscii(vk, scan, keyState, &ascii, 0) == 1)
                ch = (char)ascii;
            else
                ch = 0;
            break;
        }
        if (ch != 0)
            EnqueueInputEvent(0, 4, ch, NULL, mod);
        return 0;
    }
    }

    case WM_COMMAND: {
        short cmd[2];

        cmd[0] = (short)0xfc18;
        cmd[1] = (short)wParam;
        EnqueueInputEvent(0, 1, 0, (unsigned int *)cmd, 0);
        return 0;
    }

    case WM_HSCROLL:
    case WM_VSCROLL: {
        int        nBar;
        int        smin;
        int        smax;
        SCROLLINFO si;
        short      pos;
        UINT       code;

        code = wParam & 0xffff;     /* first in the exe: and ebx,0xffff */
        nBar = (msg != WM_HSCROLL);
        GetScrollRange(hwnd, nBar, &smin, &smax);
        si.cbSize = sizeof(si);
        si.fMask = 0x17;
        if (!GetScrollInfo(hwnd, nBar, &si))
            return 0;
        pos = (short)si.nPos;
        switch (code) {
        case SB_LINEUP:        pos--; break;
        case SB_LINEDOWN:      pos++; break;
        case SB_PAGEUP:        pos -= 10; break;
        case SB_PAGEDOWN:      pos += 10; break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK:    pos = (short)si.nTrackPos; break;
        case SB_TOP:           pos = (short)smin; break;
        case SB_BOTTOM:        pos = (short)smax; break;
        }
        if (pos < smin)
            pos = (short)smin;
        if (pos > smax)
            pos = (short)smax;
        if (si.nPos == pos)
            return 0;
        SetScrollPos(hwnd, nBar, pos, TRUE);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        POINT pt;
        short packed[2];

        mod = 0;
        if ((GetAsyncKeyState(VK_SHIFT) & 0xfffffffe) != 0)
            mod = 0x800;
        if ((GetAsyncKeyState(VK_CONTROL) & 0xfffffffe) != 0)
            mod |= 0x1fa0;
        pt.x = LOWORD(lParam);
        pt.y = HIWORD(lParam);
        ClientToScreen(hwnd, &pt);
        /* Engine convention: {Y low word, X high word} (MapScreenPointToNode). */
        packed[1] = (short)pt.x;
        packed[0] = (short)pt.y;
        EnqueueInputEvent(0, 1, 0, (unsigned int *)packed, mod);
        return 0;
    }

    case WM_LBUTTONUP:
    case WM_RBUTTONUP: {
        POINT pt;
        short packed[2];

        pt.x = LOWORD(lParam);
        pt.y = HIWORD(lParam);
        ClientToScreen(hwnd, &pt);
        packed[1] = (short)pt.x;
        packed[0] = (short)pt.y;
        EnqueueInputEvent(0, 2, 0, (unsigned int *)packed, 0);
        return 0;
    }

    case 0x30f:  /* WM_QUERYNEWPALETTE */
    case 0x311:  /* WM_PALETTECHANGED */
        return 0;
    }
}


/* F-key debug path: nudges three floating RGB scale factors (clamped to 0.15..2.5), clears
 * gGammaTablesBuilt and re-applies the engine CLUT / palette. decAll 0 multiplies the selected
 * channels by 20/21 (0.95238, 0x0045a058), 1 by 21/20 (1.05, 0x0045a050); tweakR / tweakG /
 * tweakB non-zero select that channel for the step, and if all three are zero every scale resets
 * to 0.65. */
void __cdecl AdjustRgbGammaScales(int decAll, int tweakR, int tweakG, int tweakB)  /* 0x0041b2a0 */
{
    if (tweakR == 0 && tweakG == 0 && tweakB == 0) {
        gGammaR = 0.65;
        gGammaG = 0.65;
        gGammaB = 0.65;
    } else if (decAll != 0) {
        /* 0x0045a050 = 1.05 */
        if (tweakR != 0)
            gGammaR *= 1.05;
        if (tweakG != 0)
            gGammaG *= 1.05;
        if (tweakB != 0)
            gGammaB *= 1.05;
    } else {
        /* 0x0045a058 = 0.95238... */
        if (tweakR != 0)
            gGammaR *= 20.0 / 21.0;
        if (tweakG != 0)
            gGammaG *= 20.0 / 21.0;
        if (tweakB != 0)
            gGammaB *= 20.0 / 21.0;
    }
    if (gGammaR > 2.5)
        gGammaR = 2.5;
    if (gGammaG > 2.5)
        gGammaG = 2.5;
    if (gGammaB > 2.5)
        gGammaB = 2.5;
    if (gGammaR < 0.15)
        gGammaR = 0.15;
    if (gGammaG < 0.15)
        gGammaG = 0.15;
    if (gGammaB < 0.15)
        gGammaB = 0.15;
    gGammaTablesBuilt = 0;
    ApplyCLUTToPalette((int)(INT_PTR)((char *)gPal16 - 0x0A));
}


/* WM_ACTIVATEAPP handler: on loss of activation it restores the display mode and hides the
 * viewports; on regain it reapplies the mode and shows them again. */
void __cdecl HandleActivateAppMessage(BOOL fActive)  /* 0x0041b480 */
{
    char       titleBuf[64];
    HWND       hwndVp;
    int        showCmd;
    int        i;

    if (fActive) {
        if (gIsAppActive == 0) {
            for (i = 0; i < gViewportCount; i++) {
                if (!DF_VIEWPORT_SLOT_OK(gViewportTable[i], sizeof(HWND)))
                    continue;
                hwndVp = *(HWND *)gViewportTable[i];
                if (gMainWindow == (int)(INT_PTR)hwndVp) {
                    SetWindowLongA((HWND)(UINT_PTR)gMainWindow, GWL_STYLE, (LONG)(DWORD)0x80000000u);
                    hwndVp = (HWND)(UINT_PTR)gMainWindow;
                    showCmd = SW_RESTORE;
                    ShowWindow(hwndVp, showCmd);
                } else if (gViewportWasVisible[i] != 0)
                    ShowWindow(hwndVp, SW_SHOW);
            }
            ApplyDisplayMode();
            EnqueueInputEvent(1, 0x2a, 0, NULL, 0);
#if DF_FIX_REPAINT_ON_ACTIVATE
    /* The repaint after a restore normally comes from the paint event (type 6) the WM_PAINT
     * handler queues, which only the MAIN loop dequeues. When the engine is inside a script
     * loop that owns the thread (a flat's `while` loop calling forceupdate every frame; the
     * trunk's gramophone close-up runs one for minutes) nothing dequeues it and the only drawing
     * is the dirty walk: one animating row on a black screen. Raising the redraw level to 2 makes
     * the next tick merge the whole viewport as a dirty rect, as visualeffect does for a full
     * scene draw. The main-loop case gets one extra full repaint, which is harmless. */
    BumpSceneOverlayModeFloor(2);
#endif
            if (gCustomPaletteFlag != 0)
                ManageSystemPalette(1);
            ApplyCLUTToPalette((int)(INT_PTR)((char *)gPal16 - 0x0A));
            SetWindowTextA((HWND)(UINT_PTR)gMainWindow, gAppTitle);
            gIsAppActive = 1;
            gGamePaused = 0;
            if (gSoundOn != 0)
                SetSoundMixerSilenced(0);
            InvalidateRect((HWND)(UINT_PTR)gMainWindow, NULL, FALSE);
        }
    } else {
        if (gIsAppActive != 0) {
            gIsAppActive = 0;
            if (gSoundOn != 0)
                SetSoundMixerSilenced(1);
            strcpy(titleBuf, gAppTitle);
            strcat(titleBuf, GetStringResource(0x65));
            SetWindowTextA((HWND)(UINT_PTR)gMainWindow, titleBuf);
            for (i = 0; i < gViewportCount; i++) {
                if (!DF_VIEWPORT_SLOT_OK(gViewportTable[i], sizeof(HWND)))
                    continue;
                InvalidateRect(*(HWND *)gViewportTable[i], NULL, TRUE);
            }
            RestoreDisplayMode();
            for (i = 0; i < gViewportCount; i++) {
                if (!DF_VIEWPORT_SLOT_OK(gViewportTable[i], sizeof(HWND)))
                    continue;
                hwndVp = *(HWND *)gViewportTable[i];
                if (gMainWindow == (int)(INT_PTR)hwndVp) {
                    SetWindowLongA((HWND)(UINT_PTR)gMainWindow, GWL_STYLE, (LONG)(DWORD)0x80080000u);
                    ShowWindow((HWND)(UINT_PTR)gMainWindow, SW_MINIMIZE);
                } else {
                    gViewportWasVisible[i] = IsWindowVisible(hwndVp);
                    ShowWindow(*(HWND *)gViewportTable[i], SW_HIDE);
                }
            }
            if (gCustomPaletteFlag != 0)
                ManageSystemPalette(0);
            gGamePaused = 1;
        }
    }
}


/* Acquires or releases ownership of the Windows system palette; idempotent (the state is
 * gPalOwnerState: 0 Windows owns / SYSPAL_STATIC, 1 the game owns / SYSPAL_NOSTATIC).
 * acquire == 1 saves the system colours (SaveSystemColors), sets SYSPAL_NOSTATIC and applies the
 * game colours (ApplyGameSysColors), giving the app all 256 entries; acquire == 0 restores
 * SYSPAL_STATIC and writes the 25 saved system UI colours back with SetSysColors (kSysPalIDs lists
 * the 25 element ids, gSavedSysColors / kGameSysColors the colours). */
void __cdecl ManageSystemPalette(int acquire)  /* 0x0041b6e0 */
{
    HDC  hdc;

    if (gPalOwnerState == acquire)
        return;

    gPalOwnerState = acquire;
    hdc = GetDC(NULL);
    DF_TRACE("sysPalette: acquire=%d GetSystemPaletteUse=%u RASTERCAPS&RC_PALETTE=%d bpp=%d saved=%d",
             acquire, (unsigned)GetSystemPaletteUse(hdc),
             (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE) != 0, GetDeviceCaps(hdc, BITSPIXEL),
             s_sysColorsSaved);

        /* Acquire: take full ownership of the system palette; if it is already SYSPAL_NOSTATIC
         * there is nothing more to do (the exe's shape: the release branch is an else-if on the
         * same flag). */
    if (acquire != 0 && GetSystemPaletteUse(hdc) == SYSPAL_STATIC) {
        SaveSystemColors();
        SetSystemPaletteUse(hdc, SYSPAL_NOSTATIC);
        ApplyGameSysColors();
    } else if (acquire == 0) {
        SetSystemPaletteUse(hdc, SYSPAL_STATIC);
        /* DF_FIX_SYSCOLOR_RESTORE_GUARD, 0x0041b6e0 (port-bug-classes.md section 8). The original
         * restores unconditionally, because on a 256-colour Win95 display the acquire branch
         * always saw SYSPAL_STATIC and saved first. On a modern 32-bit display
         * GetSystemPaletteUse returns SYSPAL_ERROR, the acquire saves nothing, and this call
         * wrote 25 zero COLORREFs into the user's live system colours on every clean quit (the
         * "theme messed up" symptom, which a force-kill actually avoids). Restore only what was
         * really saved. */
#if DF_FIX_SYSCOLOR_RESTORE_GUARD
        if (s_sysColorsSaved)
#endif
        {
            DF_TRACE("sysPalette: SetSysColors(restore) first=%06lx", (unsigned long)gSavedSysColors[0]);
            SetSysColors(25, kSysPalIDs, gSavedSysColors);
        }
    }

    ReleaseDC(NULL, hdc);
}



/* Snapshots all 25 Windows system UI colours into gSavedSysColors[] before the game steals the
 * palette (SYSPAL_NOSTATIC). Called only from ManageSystemPalette(1) when SYSPAL_STATIC is active. */
void __stdcall SaveSystemColors(void)  /* 0x0041b750 */
{
    int i;
    for (i = 0; i < 25; i++)
        gSavedSysColors[i] = GetSysColor(kSysPalIDs[i]);
#if DF_FIX_SYSCOLOR_RESTORE_GUARD
    s_sysColorsSaved = 1;
#endif
}


/* Overwrites the 25 Windows system UI colour slots with the game-defined values of kGameSysColors[]
 * (SetSysColors), so that system UI colours use game-palette entries instead of the Windows
 * defaults. Called from ManageSystemPalette(1) after SYSPAL_NOSTATIC; ManageSystemPalette(0)
 * reverses it with gSavedSysColors. */
void __stdcall ApplyGameSysColors(void)  /* 0x0041b780 */
{
    DF_TRACE("sysPalette: SetSysColors(GAME colours)");
    SetSysColors(25, kSysPalIDs, kGameSysColors);
}


/* Enumerates the preferred and alternate modes with EnumDisplaySettingsA, builds the DEVMODE and
 * switches mode with ChangeDisplaySettingsA, saving the window positions first. */
void __stdcall ApplyDisplayMode(void)  /* 0x0041b7a0 */
{
    BOOL bAlternate;   /* EnumDisplaySettingsA result for alternate mode */
    BOOL bPreferred;   /* EnumDisplaySettingsA result for preferred mode */
    DEVMODEA dm;       /* DEVMODE for EnumDisplaySettingsA output and
                        * ChangeDisplaySettingsA input. See Ghidra at
                        * 0x0041b7ee-0x0041b803: this call passes &dm (not NULL),
                        * which tells ChangeDisplaySettingsA to apply the mode. */

    if (gUser32Lib == 0 || gIsDisplayModeChanged != 0) {
        DF_TRACE("displayMode: ApplyDisplayMode skipped (user32=%d changed=%d)", gUser32Lib != 0, gIsDisplayModeChanged);
        return;
    }

    /* Verify both candidate mode indices are valid. The exe sets dmSize
     * once, before the first call only. */
    dm.dmSize = sizeof(DEVMODEA);
    bPreferred = ((PFNENUMDS)gEnumDisplaySettingsProc)(NULL, 0, &dm);
    bAlternate = ((PFNENUMDS)gEnumDisplaySettingsProc)(NULL, gAlternateDisplayMode, &dm);

    if ((bPreferred & bAlternate) != 0)
    {
        SaveWindowPositions();
        dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;  /* 0x1C0000 */
        {
            LONG cds = ((PFNCHANGEDS)gChangeDisplaySettingsProc)(&dm, 0);
            DF_TRACE("displayMode: ChangeDisplaySettings(%lux%lu x %lubpp, modeIdx=%d) -> %ld",
                     (unsigned long)dm.dmPelsWidth, (unsigned long)dm.dmPelsHeight,
                     (unsigned long)dm.dmBitsPerPel, (int)gAlternateDisplayMode, (long)cds);
        }
        gIsDisplayModeChanged = 1;
    }
}


/* Enumerates all top-level windows before ChangeDisplaySettingsA to minimise / hide them (a
 * Win9x-era pattern to avoid display artefacts), saving their rects. */
void __stdcall SaveWindowPositions(void)  /* 0x0041b820 */
{
    gSavedWindowCount = 0;
    EnumWindows((WNDENUMPROC)SaveWindowPositionEnumProc, 0);
}


/* EnumWindows callback: records HWND + screen rect into gSavedWindowPositions (max 0xFF entries). */
static BOOL CALLBACK SaveWindowPositionEnumProc(HWND hwnd, LPARAM lParam) /* 0x0041B840 */
{
    int idx;

    (void)lParam;
    if (gSavedWindowCount >= 0xff)      /* cmp 0xff / jl, as 0x0041b845 has it */
        return FALSE;
    idx = gSavedWindowCount;
    gSavedWindowPositions[idx].hWnd = hwnd;
    GetWindowRect(hwnd, (LPRECT)&gSavedWindowPositions[idx].left);
    gSavedWindowCount = gSavedWindowCount + 1;
    return TRUE;
}


/* Reverts the display mode with ChangeDisplaySettingsA(NULL, 0), lets Windows settle, then
 * restores the saved window positions (a tail jump in the exe). */
void __stdcall RestoreDisplayMode(void)  /* 0x0041b880 */
{
    if (gUser32Lib != 0 && gIsDisplayModeChanged != 0)
    {
        DF_TRACE("displayMode: reverting (ChangeDisplaySettings(NULL,0))");
        ((PFNCHANGEDS)gChangeDisplaySettingsProc)(NULL, 0);
        gIsDisplayModeChanged = 0;
        PumpMessages();
        RestoreSavedWindowPositions();
    }
}

/* MoveWindow()s every saved window back to its RECT, then clears the count. RestoreDisplayMode
 * tail-jumps here (0x0041b8ab);
 *
 * Port note: DF_FIX_OWN_WINDOWS_ONLY. The original saved and restored all top-level windows on the
 * desktop; on modern Windows (300+ windows from many processes) MoveWindow blocks forever when the
 * target thread is not pumping messages, the "black screen that never recovers". Only this
 * process's own windows are restored. */
void __cdecl RestoreSavedWindowPositions(void)  /* 0x0041b8c0 */
{
    int          i;
    HWND         hWnd;
    RECT         rc;
#if DF_FIX_OWN_WINDOWS_ONLY
    DWORD        dwOwnerPid;
    DWORD        dwSelfPid = GetCurrentProcessId();
#endif

    for (i = 0; i < gSavedWindowCount; i++)
    {
        hWnd = gSavedWindowPositions[i].hWnd;
        if (IsWindow(hWnd))
        {
#if DF_FIX_OWN_WINDOWS_ONLY
            GetWindowThreadProcessId(hWnd, &dwOwnerPid);
            if (dwOwnerPid != dwSelfPid)
                continue;
#endif
            rc = *(RECT *)&gSavedWindowPositions[i].left;
            MoveWindow(hWnd, rc.left, rc.top,
                       rc.right - rc.left, rc.bottom - rc.top, FALSE);
        }
    }
    gSavedWindowCount = 0;
}


/* Shows a Yes/No/Cancel MessageBox (0x2023) with the message word-wrapped. Returns 1 for Yes, 0
 * for No and -1 for Cancel. Used by InitJoystick when the stick is not centred. */
int __cdecl ShowYesNoDialog(const char *pszTitle, LPCSTR pszMessage)  /* 0x0041b950 */
{
    int  result;
    char szWrapped[512];

    WordWrapText((char *)pszMessage, szWrapped);
    result = MessageBoxA(NULL, szWrapped, pszTitle, 0x2023);

    if (result == IDYES)   /* 6 */
        return 1;
    return (result == IDNO) ? 0 : -1;   /* mov eax,0 / adc eax,-1, as 0x0041b98f has it */
}


/* Shows the engine About box (F12): the build date, the OS line, the sound line (16-bit / 8-bit,
 * stereo / mono), the heap limits and whether the joystick is enabled. */
void __stdcall ShowEngineAboutDialog(void)  /* 0x0041b9b0 */
{
    char szOs[64];
    char szSound[64];
    char szDate[64];
    char szText[512];
    char *pszCompiled;

    /* build date, OS line, sound line; the exe reads the pool size through
     * GetHeapDescPtr() with no null test (the port had added one, no DF_FIX) */
    /* through a pointer: the exe copies it with an inline strlen (repne scasb),
     * which VC++ 4.2 emits only when the source is not a literal in the call */
    pszCompiled = "Dec 31 1996 at 16:47:05" /* 0x0045baa0 */;
    strcpy(szDate, pszCompiled);
    if (gIsWin95 != 0)
        sprintf(szOs, "Windows 95, build %ld" /* 0x0045ba88 */, (long)(gWinNTBuild & 0xffff));
    else if (gIsWin31 != 0)
        sprintf(szOs, "Windows 3.1" /* 0x0045ba7c */);
    else
        sprintf(szOs, "Windows NT, build %ld" /* 0x0045ba64 */, (long)gWinNTBuild);
    sprintf(szSound, "%s %s" /* 0x0045ba3c */,
            (gSound16Bit != 0) ? "16-bit" /* 0x0045ba4c */ : "8-bit" /* 0x0045ba44 */,
            (gSoundStereo != 0) ? "stereo" /* 0x0045ba5c */ : (const char *)kStrMono);
    sprintf(szText, "DreamFactory v4.0\n(C) Copyright 1993-1996 CyberFlix, Inc.\nAll rights reserved.\n\nWindows RT4 engine, standard version\nCompiled %s\n\n%s\nHeap zone is %ldK\n%ldK is currently free\nJoystick %s\nAudio is %s via WinMM" /* 0x0045b958 */,
            szDate, szOs,
            *(int *)(GetHeapDescPtr() + 0x0C) / 1024,
            (int)GetLargestFreeBlockSize() / 1024,
            (gJoystickPresent != 0) ? "enabled" /* 0x0045ba34 */ : "disabled" /* 0x0045ba28 */,
            szSound);
    SuspendGameUiForModalDialog();
    ShowFatalDialog("About" /* 0x0045b950 */, szText);
    RestoreGameUiAfterModalDialog();
}


/* Shows a fatal error dialog with the given title and message (typically followed by exit). */
void __cdecl ShowFatalDialog(LPCSTR pszTitle, char *pszMessage)  /* 0x0041bb00 */
{
    char szWrapped[512];

    WordWrapText(pszMessage, szWrapped);
    MessageBoxA(NULL, szWrapped, pszTitle, 0x2040);
}


#if DF_FIX_CMDLINE_BOUNDED_COPY
/* AppInit's command-line copy, bounded to the buffer (the exe's strcpy is not). */
static void CopyLowerCmdLine(char *dest, size_t destSize, const char *src)
{
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
    _strlwr(dest);
}
#endif

#ifdef _MSC_VER
#define DF_NAKED __declspec(naked)
#define DATA_(a) AppInit + (0x##a - 0x0041bb40)   /* a global or literal, no C name */
int __cdecl _sscanf(const char *, const char *, ...);
void *__cdecl _memcpy(void *, const void *, unsigned int);
/* The application entry: pInitArgs is the {HINSTANCE, LPSTR} struct WinMain builds. Stores
 * hInstance and loads the application title, parses the command line (-r no display change, -j no
 * joystick, -16 16-bit sound, -s stereo, -e undocumented), runs the startup checks and sub-system
 * initialisation (any failure is fatal), sets the error mode and process priority, loads the stock
 * GDI objects, cursor and menu, registers the DFWINDOW and DFCOVER window classes, creates and
 * shows the cover window, switches the display mode, checks the colour depth, initialises the
 * rendering backend, the accelerator table and the application palette, and installs the embedded
 * font.
 *
 * Under VC++ 4.2 (the match build) this is a __declspec(naked) inline-asm body, because no C input
 * to that compiler produces the exe's bytes: the exe loads the RegisterClassA import pointer
 * before the WNDCLASS stores (an ALIASED memory dependence only C1XX creates: C1 never emits IL op
 * 0x5a) and orders one store differently; every C spelling, both front ends, 25 switch sets,
 * VC++ 4.0 / 4.1 / 4.2 and the 4.2b patches give 25 differing bytes (docs/differences.md). The C body below is the believed original source and
 * what the GCC port runs; under _MSC_VER it is compiled as AppInit_Source (the linker discards it)
 * so that its 15 string literals are emitted as the unit's pooled COMDATs in the original order
 * (the 12 only AppInit uses are kept by link_build.py's /INCLUDE list; LINK 4.20 ignores
 * /INCLUDE from .drectve). Asm spelling rules (measured): a global named in __asm is
 * asm-referenced for the rest of the unit (later functions reload it), so globals and literals are
 * addressed as this function's own symbol plus a displacement (DATA_); imports keep their names
 * (DIR32 to __imp__X); _sscanf / _memcpy are called by this unit with no declaration in scope and
 * an asm call needs one (count-neutral, measured). */
DF_NAKED void __cdecl AppInit(void *pInitArgs)  /* 0x0041bb40 */
{
    __asm {
            sub esp, 0x274
            push   ebx
            push   esi
            mov esi, dword ptr [esp+0x280]
            push   edi
            push   ebp
            xor edi, edi
            mov eax, dword ptr [esi]
            push   0x40
            push   offset gAppTitle
            mov ebx, dword ptr [LoadStringA]
            push   edi
            mov dword ptr [DATA_(00488264)], eax    ; _DAT_00488264_uyz
            push   eax
            call   ebx
            mov dword ptr [DATA_(00486294)], edi    ; _DAT_00486294_iwy
            mov dword ptr [DATA_(00486290)], edi    ; _DAT_00486290_1Y
            mov ecx, 0xffffffff
            sub eax, eax
            mov dword ptr [DATA_(004883b8)], edi    ; _DAT_004883b8_yxx
            mov dword ptr [DATA_(00486464)], edi    ; _DAT_00486464_aAY
            mov dword ptr [DATA_(00485f50)], edi    ; _DAT_00485f50_qw
            mov edi, dword ptr [esi+0x4]
            repne scasb
            not    ecx
            sub edi, ecx
            mov edx, ecx
            shr ecx, 0x2
            mov esi, edi
            lea edi, dword ptr [esp+0x84]
            rep movsd
            mov ecx, edx
            and ecx, 0x3
            rep movsb
            lea eax, dword ptr [esp+0x84]
            push   eax
            call _strlwr
            lea eax, dword ptr [esp+0x88]
            add esp, 0x4
            push   offset DATA_(0045bb20)    ; '-h'
            push   eax
            call strstr
            add esp, 0x8
            test eax, eax
            jne short L_b2
            lea eax, dword ptr [esp+0x84]
            push   offset DATA_(0045bb1c)    ; '-?'
            push   eax
            call strstr
            add esp, 0x8
            test eax, eax
            je short L_107
L_b2:       push   0xbb
            call GetStringResource
            add esp, 0x4
            mov edi, eax
            mov ecx, 0xffffffff
            sub eax, eax
            repne scasb
            not    ecx
            sub edi, ecx
            mov eax, ecx
            shr ecx, 0x2
            mov esi, edi
            lea edi, dword ptr [esp+0x44]
            rep movsd
            mov ecx, eax
            push   0xbc
            and ecx, 0x3
            rep movsb
            call GetStringResource
            lea ecx, dword ptr [esp+0x48]
            add esp, 0x4
            push   eax
            push   ecx
            call ShowFatalDialog
            add esp, 0x8
            push   0xffffffff
            call exit
            add esp, 0x4
L_107:      lea eax, dword ptr [esp+0x84]
            push   offset DATA_(0045bb18)    ; '-r'
            push   eax
            call strstr
            add esp, 0x8
            test eax, eax
            je short L_12a
            mov dword ptr [DATA_(00486294)], 0x1    ; _DAT_00486294_iwy
L_12a:      lea eax, dword ptr [esp+0x84]
            push   offset DATA_(0045bb14)    ; '-j'
            push   eax
            call strstr
            add esp, 0x8
            test eax, eax
            je short L_14d
            mov dword ptr [DATA_(00486290)], 0x1    ; _DAT_00486290_1Y
L_14d:      lea eax, dword ptr [esp+0x84]
            push   offset DATA_(0045bb10)    ; '-16'
            push   eax
            call strstr
            add esp, 0x8
            test eax, eax
            je short L_170
            mov dword ptr [DATA_(004883b8)], 0x1    ; _DAT_004883b8_yxx
L_170:      lea eax, dword ptr [esp+0x84]
            push   offset DATA_(0045bb0c)    ; '-s'
            push   eax
            call strstr
            add esp, 0x8
            test eax, eax
            je short L_193
            mov dword ptr [DATA_(00486464)], 0x1    ; _DAT_00486464_aAY
L_193:      lea eax, dword ptr [esp+0x84]
            push   offset DATA_(0045bb08)    ; '-e'
            push   eax
            call strstr
            add esp, 0x8
            test eax, eax
            je short L_1b6
            mov dword ptr [DATA_(00485f50)], 0x1    ; _DAT_00485f50_qw
L_1b6:      call DetectCPUType
            test eax, eax
            je short L_1c9
            push   0xffffffff
            call exit
            add esp, 0x4
L_1c9:      call CheckWindowsVersion
            test eax, eax
            je short L_1dc
            push   0xffffffff
            call exit
            add esp, 0x4
L_1dc:      call CheckFileHandles
            test eax, eax
            je short L_1ef
            push   0xffffffff
            call exit
            add esp, 0x4
L_1ef:      call InitJoystick
            test ax, ax
            je short L_203
            push   0xffffffff
            call exit
            add esp, 0x4
L_203:      push   0x8005
            call   dword ptr [SetErrorMode]
            push   0x80
            call   dword ptr [GetCurrentProcess]
            push   eax
            call   dword ptr [SetPriorityClass]
            call InitMemoryManager
            test eax, eax
            je short L_235
            push   0xffffffff
            call exit
            add esp, 0x4
            jmp short L_24f
L_235:      push   0x1000
            mov eax, dword ptr [DATA_(00486e70)]    ; _DAT_00486e70_oxy
            push   eax
            mov ecx, dword ptr [DATA_(004862d0)]    ; _DAT_004862d0_aA3
            push   ecx
            call InitHeap
            add esp, 0xc
L_24f:      call InitializeEngineState
            push   offset DATA_(0045bafc)    ; 'CURS.WATCH'
            mov eax, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            push   eax
            mov edi, dword ptr [LoadCursorA]
            mov dword ptr [DATA_(0045b618)], 0x0    ; _gGamePaused
            call   edi
            push   0x5
            mov ebp, dword ptr [GetStockObject]
            mov dword ptr [DATA_(00486160)], eax    ; _DAT_00486160_Iwy
            call   ebp
            push   0x4
            mov dword ptr [DATA_(00487330)], eax    ; _DAT_00487330_Hy
            call   ebp
            push   0x0
            mov dword ptr [DATA_(004885d8)], eax    ; _DAT_004885d8_Az
            call   ebp
            push   0x8
            mov dword ptr [DATA_(004867a8)], eax    ; _DAT_004867a8_hxw
            call   ebp
            push   0x7
            mov dword ptr [DATA_(004867e0)], eax    ; _DAT_004867e0_08
            call   ebp
            push   0xd
            mov dword ptr [DATA_(004885d0)], eax    ; _DAT_004885d0_aAO
            call   ebp
            push   0xf
            mov dword ptr [DATA_(00486288)], eax    ; _DAT_00486288_hyz
            call   ebp
            cmp dword ptr [DATA_(00487330)], 0x0    ; _DAT_00487330_Hy
            mov dword ptr [DATA_(0048628c)], eax    ; _DAT_0048628c_aAI
            je short L_2f1
            cmp dword ptr [DATA_(004885d8)], 0x0    ; _DAT_004885d8_Az
            je short L_2f1
            cmp dword ptr [DATA_(004867a8)], 0x0    ; _DAT_004867a8_hxw
            je short L_2f1
            cmp dword ptr [DATA_(004867e0)], 0x0    ; _DAT_004867e0_08
            je short L_2f1
            cmp dword ptr [DATA_(004885d0)], 0x0    ; _DAT_004885d0_aAO
            je short L_2f1
            cmp dword ptr [DATA_(00486288)], 0x0    ; _DAT_00486288_hyz
            jne short L_300
L_2f1:      push   0x4fc
            push   0x66
            call FatalError
            add esp, 0x8
L_300:      push   offset DATA_(0045baf4)    ; 'MENU'
            mov eax, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            push   eax
            xor esi, esi
            call   dword ptr [LoadMenuA]
            mov word ptr [DATA_(00486c2c)], si    ; _DAT_00486c2c_jxw
            push   offset DATA_(0045badc)    ; 'CURS.ARROW'
            mov dword ptr [esp+0x44], offset DATA_(0045bae8)    ; 'DFWINDOW'
            mov dword ptr [esp+0x24], offset WndProc_DFWINDOW
            mov dword ptr [DATA_(00488150)], eax    ; _DAT_00488150_a26
            mov eax, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            mov dword ptr [esp+0x30], eax
            push   eax
            call   edi
            push   offset DATA_(0045bad0)    ; 'WINDOWICON'
            mov ecx, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            mov dword ptr [esp+0x38], eax
            push   ecx
            call   dword ptr [LoadIconA]
            mov dword ptr [esp+0x30], eax
            push   0x4
            mov dword ptr [esp+0x40], esi
            call   ebp
            lea ecx, dword ptr [esp+0x1c]
            mov dword ptr [esp+0x38], eax
            mov dword ptr [esp+0x1c], esi
            push   ecx
            mov dword ptr [esp+0x2c], 0x18
            mov dword ptr [esp+0x28], esi
            mov edi, dword ptr [RegisterClassA]
            call   edi
            mov ecx, dword ptr [DATA_(00486160)]    ; _DAT_00486160_Iwy
            mov dword ptr [esp+0x28], esi
            lea eax, dword ptr [esp+0x1c]
            mov dword ptr [esp+0x34], ecx
            mov dword ptr [esp+0x40], offset DATA_(0045bac8)    ; 'DFCOVER'
            mov dword ptr [esp+0x20], offset WndProc_DFCOVER
            mov dword ptr [esp+0x1c], 0x200
            push   eax
            call   edi
            lea ecx, dword ptr [esp+0x84]
            push   0x20
            push   ecx
            mov edx, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            push   0x2
            push   edx
            call   ebx
            lea ecx, dword ptr [esp+0x18]
            lea edx, dword ptr [esp+0x84]
            push   ecx
            push   offset DATA_(0045bac4)    ; '%d'
            push   edx
            call _sscanf
            lea ecx, dword ptr [esp+0x90]
            add esp, 0xc
            mov edx, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            push   0x20
            push   ecx
            push   0x3
            push   edx
            call   ebx
            lea ecx, dword ptr [esp+0x14]
            lea edx, dword ptr [esp+0x84]
            push   ecx
            push   offset DATA_(0045bac4)    ; '%d'
            push   edx
            call _sscanf
            lea ecx, dword ptr [esp+0x90]
            add esp, 0xc
            mov edx, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            push   0x20
            push   ecx
            push   0x4
            push   edx
            call   ebx
            lea ecx, dword ptr [esp+0x44]
            lea edx, dword ptr [esp+0x84]
            push   ecx
            push   offset DATA_(0045bac4)    ; '%d'
            push   edx
            call _sscanf
            lea ecx, dword ptr [esp+0x90]
            add esp, 0xc
            mov edx, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            push   0x20
            push   ecx
            push   0x5
            push   edx
            call   ebx
            movsx ecx, byte ptr [esp+0x84]
            mov dword ptr [DATA_(004885d4)], ecx    ; _DAT_004885d4_a0f
            mov eax, dword ptr [esp+0x14]
            mov edx, dword ptr [esp+0x44]
            mov ecx, dword ptr [esp+0x18]
            push   edx
            push   eax
            push   ecx
            call InitDisplayMode
            add esp, 0xc
            mov ecx, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            mov edi, dword ptr [GetSystemMetrics]
            push   esi
            push   ecx
            push   esi
            push   esi
            push   0x1
            call   edi
            push   eax
            push   esi
            call   edi
            push   eax
            push   esi
            push   esi
            push   0x80000000
            push   offset gAppTitle
            push   offset DATA_(0045bac8)    ; 'DFCOVER'
            push   esi
            call   dword ptr [CreateWindowExA]
            push   0x3
            mov dword ptr [DATA_(00486460)], eax    ; _DAT_00486460_aBx
            push   eax
            mov ebp, dword ptr [ShowWindow]
            call   ebp
            mov ecx, dword ptr [DATA_(00486460)]    ; _DAT_00486460_aBx
            push   ecx
            call   dword ptr [GetDC]
            mov edi, eax
            push   esi
            mov eax, dword ptr [DATA_(0048628c)]    ; _DAT_0048628c_aAI
            push   eax
            push   edi
            call   dword ptr [SelectPalette]
            push   edi
            mov ebp, dword ptr [RealizePalette]
            call   ebp
            push   edi
            mov eax, dword ptr [DATA_(00486460)]    ; _DAT_00486460_aBx
            push   eax
            call   dword ptr [ReleaseDC]
            mov eax, dword ptr [DATA_(00486460)]    ; _DAT_00486460_aBx
            push   eax
            call   dword ptr [UpdateWindow]
            mov ecx, dword ptr [DATA_(00486160)]    ; _DAT_00486160_Iwy
            push   ecx
            call   dword ptr [SetCursor]
            call ApplyDisplayMode
            call CheckDisplayColourDepth
            cmp eax, 0x2
            mov ebx, eax
            jne short L_582
            call RestoreDisplayMode
            mov eax, dword ptr [DATA_(00486460)]    ; _DAT_00486460_aBx
            mov ebp, dword ptr [DestroyWindow]
            push   eax
            call   ebp
            mov dword ptr [DATA_(00486460)], esi    ; _DAT_00486460_aBx
            call SubsystemCleanup
            push   0x8d
            call GetStringResource
            add esp, 0x4
            mov edi, eax
            mov ecx, 0xffffffff
            sub eax, eax
            repne scasb
            not    ecx
            sub edi, ecx
            mov edx, ecx
            shr ecx, 0x2
            mov esi, edi
            lea edi, dword ptr [esp+0x84]
            rep movsd
            mov ecx, edx
            and ecx, 0x3
            rep movsb
            lea eax, dword ptr [esp+0x84]
            push   eax
            call RevertDisplayMode
            add esp, 0x4
            push   0xffffffff
            call exit
            add esp, 0x4
            jmp short L_588
L_582:      mov ebp, dword ptr [DestroyWindow]
L_588:      test ebx, ebx
            jne short L_595
            call CheckSoundCardCapability
            test eax, eax
            je short L_5bb
L_595:      call RestoreDisplayMode
            mov eax, dword ptr [DATA_(00486460)]    ; _DAT_00486460_aBx
            push   eax
            call   ebp
            mov dword ptr [DATA_(00486460)], 0x0    ; _DAT_00486460_aBx
            call SubsystemCleanup
            push   0xffffffff
            call exit
            add esp, 0x4
L_5bb:      call InitGraphicsBackend
            test eax, eax
            jne short L_63b
            push   0x8e
            call GetStringResource
            add esp, 0x4
            mov edi, eax
            mov ecx, 0xffffffff
            sub eax, eax
            repne scasb
            not    ecx
            sub edi, ecx
            mov eax, ecx
            shr ecx, 0x2
            mov esi, edi
            lea edi, dword ptr [esp+0x84]
            rep movsd
            mov ecx, eax
            and ecx, 0x3
            rep movsb
            lea ecx, dword ptr [esp+0x84]
            push   ecx
            push   0x8f
            call GetStringResource
            add esp, 0x4
            push   eax
            call ShowInfoDialog
            add esp, 0x8
            call RestoreDisplayMode
            mov eax, dword ptr [DATA_(00486460)]    ; _DAT_00486460_aBx
            push   eax
            call   ebp
            mov dword ptr [DATA_(00486460)], 0x0    ; _DAT_00486460_aBx
            call SubsystemCleanup
            push   0xffffffff
            call exit
            add esp, 0x4
L_63b:      xor esi, esi
            push   offset DATA_(0045bab8)    ; 'ACCELTABLE'
            mov eax, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            mov dword ptr [DATA_(0048836c)], esi    ; _DAT_0048836c_Fzw
            push   eax
            mov dword ptr [DATA_(00486278)], esi    ; _DAT_00486278_AH
            mov dword ptr [DATA_(004883c8)], esi    ; _DAT_004883c8_uzz
            call   dword ptr [LoadAcceleratorsA]
            mov dword ptr [DATA_(00486e68)], eax    ; _DAT_00486e68_Ew
            cmp eax, esi
            je L_70d
            push   0x9
            mov eax, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            push   offset DATA_(0045bab8)    ; 'ACCELTABLE'
            push   eax
            call   dword ptr [FindResourceA]
            push   eax
            mov esi, eax
            mov eax, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            push   eax
            call   dword ptr [SizeofResource]
            shr eax, 0x3
            mov dword ptr [DATA_(00486e78)], eax    ; _DAT_00486e78_ixw
            shl eax, 0x3
            push   eax
            push   0x40
            call   dword ptr [LocalAlloc]
            push   esi
            mov ecx, dword ptr [DATA_(00488264)]    ; _DAT_00488264_uyz
            push   ecx
            mov dword ptr [DATA_(00486c50)], eax    ; _DAT_00486c50_Cy
            call   dword ptr [LoadResource]
            push   eax
            mov esi, eax
            call   dword ptr [LockResource]
            mov dword ptr [DATA_(00486e78)], 0x1    ; _DAT_00486e78_ixw
            test byte ptr [eax], 0x80
            jne short L_6dc
L_6ce:      add eax, 0x8
            inc    dword ptr [DATA_(00486e78)]    ; _DAT_00486e78_ixw
            test byte ptr [eax], 0x80
            je short L_6ce
L_6dc:      mov ecx, 0x1
            mov edx, dword ptr [DATA_(00486e78)]    ; _DAT_00486e78_ixw
            shl edx, 0x3
            sub ecx, dword ptr [DATA_(00486e78)]    ; _DAT_00486e78_ixw
            push   edx
            lea eax, dword ptr [eax+ecx*8]
            push   eax
            mov ecx, dword ptr [DATA_(00486c50)]    ; _DAT_00486c50_Cy
            push   ecx
            call _memcpy
            add esp, 0xc
            push   esi
            call   dword ptr [FreeResource]
            jmp short L_717
L_70d:      mov dword ptr [DATA_(00486e78)], 0x0    ; _DAT_00486e78_ixw
L_717:      mov edi, 0x1
            xor ebx, ebx
            mov dword ptr [DATA_(00486358)], edi    ; _DAT_00486358_gzx
            mov dword ptr [DATA_(00486c28)], ebx    ; _DAT_00486c28_Dyz
            call CreateBackBufferDC
            mov dword ptr [DATA_(00485f54)], eax    ; _DAT_00485f54_0l
            mov dword ptr [DATA_(00486e84)], ebx    ; 
            mov word ptr [DATA_(00486e8c)], di    ; 
            mov eax, 0x100
            mov word ptr [DATA_(00487d4a)], ax    ; 
            mov dword ptr [DATA_(00486e88)], ebx    ; 
            mov dword ptr [DATA_(00486e90)], ebx    ; 
            mov dword ptr [DATA_(00486e94)], ebx    ; 
            mov dword ptr [DATA_(00486e98)], ebx    ; 
            mov dword ptr [DATA_(00486e9c)], ebx    ; 
            mov dword ptr [DATA_(00486ea0)], eax    ; 
            mov dword ptr [DATA_(00486ea4)], eax    ; 
            mov dword ptr [DATA_(00486e80)], 0x28    ; _DAT_00486e80_Gz
            mov word ptr [DATA_(00487d48)], 0x300    ; _DAT_00487d48_pxw
            mov word ptr [DATA_(00486e8e)], 0x8    ; 
            push   ebx
            call   dword ptr [GetDC]
            mov esi, eax
            cmp dword ptr [DATA_(004885d4)], ebx    ; _DAT_004885d4_a0f
            je short L_7fd
            mov byte ptr [esp+0x12], bl
            push   ebx
            mov byte ptr [esp+0x15], bl
            push   ebx
            mov byte ptr [esp+0x18], bl
            push   ebx
            push   ebx
            call SetPaletteEntry
            add esp, 0x10
L_7b9:      push   0x0
            push   0x0
            push   0x0
            push   edi
            inc    edi
            call SetPaletteEntry
            add esp, 0x10
            cmp edi, 0xff
            jl short L_7b9
            mov eax, 0xff
            mov byte ptr [esp+0x12], al
            push   eax
            mov byte ptr [esp+0x15], al
            push   eax
            mov byte ptr [esp+0x18], al
            push   eax
            push   eax
            call SetPaletteEntry
            add esp, 0x10
            push   0x1
            call ManageSystemPalette
            add esp, 0x4
            jmp L_8c2
L_7fd:      xor eax, eax
            mov byte ptr [esp+0x12], al
            push   eax
            mov byte ptr [esp+0x15], al
            push   eax
            push   eax
            mov ebx, 0x1
            mov byte ptr [esp+0x1c], al
            push   eax
            call SetPaletteEntry
            add esp, 0x10
            mov edi, dword ptr [GetSystemPaletteEntries]
L_822:      lea eax, dword ptr [esp+0x10]
            push   eax
            push   0x1
            push   ebx
            push   esi
            call   edi
            xor eax, eax
            mov al, byte ptr [esp+0x12]
            push   eax
            xor eax, eax
            mov al, byte ptr [esp+0x15]
            push   eax
            xor eax, eax
            mov al, byte ptr [esp+0x18]
            push   eax
            push   ebx
            inc    ebx
            call SetPaletteEntry
            add esp, 0x10
            cmp ebx, 0xa
            jl short L_822
            mov ebx, 0xa
L_856:      push   0x0
            push   0x0
            push   0x0
            push   ebx
            inc    ebx
            call SetPaletteEntry
            add esp, 0x10
            cmp ebx, 0xf6
            jl short L_856
            mov ebx, 0xf6
L_873:      lea eax, dword ptr [esp+0x10]
            push   eax
            push   0x1
            push   ebx
            push   esi
            call   edi
            xor eax, eax
            mov al, byte ptr [esp+0x12]
            push   eax
            xor eax, eax
            mov al, byte ptr [esp+0x15]
            push   eax
            xor eax, eax
            mov al, byte ptr [esp+0x18]
            push   eax
            push   ebx
            inc    ebx
            call SetPaletteEntry
            add esp, 0x10
            cmp ebx, 0xff
            jl short L_873
            mov eax, 0xff
            mov byte ptr [esp+0x12], al
            push   eax
            mov byte ptr [esp+0x15], al
            push   eax
            mov byte ptr [esp+0x18], al
            push   eax
            push   eax
            call SetPaletteEntry
            add esp, 0x10
L_8c2:      push   esi
            xor esi, esi
            push   esi
            call   dword ptr [ReleaseDC]
            push   esi
            mov dword ptr [DATA_(00485f4c)], esi    ; _DAT_00485f4c_ac
            call SyncLogicalPalette
            add esp, 0x4
            mov dword ptr [DATA_(004863dc)], esi    ; _DAT_004863dc_j
            push   0x1
            call ManageEmbeddedFont
            add esp, 0x4
            pop    ebp
            pop    edi
            pop    esi
            pop    ebx
            add esp, 0x274
            ret
    }
}
#undef DATA_
/* the believed source, compiled for its string literals only (see above); discarded by the linker */
void __cdecl AppInit_Source(void *pInitArgs)
#else
void __cdecl AppInit(void *pInitArgs)  /* 0x0041bb40 */
#endif
{
    HINSTANCE  *ppArgs      = (HINSTANCE *)pInitArgs;

    int   iResult;
    BITMAPINFOHEADER *pbh;
    HDC   hDC;
    HRSRC hResInfo;
    HGLOBAL hResData;
    BYTE   *pbAccel;
    int     iPalIdx;
    int    *pDepth;

    WNDCLASSA   wc;
    tagPALETTEENTRY pe;
    char        szCmdLineLower[512];  /* local_200 */
#if DF_FIX_CMDLINE_BOUNDED_COPY
    char        szScratch[512];       /* re-used for string copies */
#else
    char        szScratch[64];        /* local_240 */
#endif
    int         nWindowWidth;         /* local_26c */
    int         nWindowHeight;        /* local_270 */
#if DF_FIX_SKIP_DISPLAY_CHECK
    int         bDebugSkipDisplayCheck;  /* no original counterpart -- see below */
#endif

    /* Every later use reads gInstance (and the cover window gCoverWindow) back: the exe reloads
     * both globals at each call. The colour depth is sscanf'd into szScratch's first dword
     * (local_240[0]): the exe's frame has no slot of its own for it. */
    gInstance = ppArgs[0];
    LoadStringA(gInstance, 0, (LPSTR)gAppTitle, 0x40);

    gNoDisplayChangeFlag = 0;
    gNoJoystickFlag = 0;
    gSound16BitFlag = 0;
    gStereoSoundFlag = 0;
    gNoEjectFlag = 0;

#if DF_FIX_CMDLINE_BOUNDED_COPY
    CopyLowerCmdLine(szCmdLineLower, sizeof(szCmdLineLower), (LPSTR)ppArgs[1]);
#else
    strcpy(szCmdLineLower, (LPSTR)ppArgs[1]);
    _strlwr(szCmdLineLower);
#endif

    /* -? and -h show the command-line usage text (string 0xbc) under the title from string
     * 0xbb ("Command Line Parameters"), then exit: a --help, not an error path. */
    if ((strstr(szCmdLineLower, kStrOptH) != NULL) ||
        (strstr(szCmdLineLower, kStrOptHelp) != NULL))
    {
        strcpy(szScratch, GetStringResource(0xbb));
        ShowFatalDialog((LPCSTR)szScratch, GetStringResource(0xbc));
        _exit(-1);
    }

    if (strstr(szCmdLineLower, kStrOptR) != NULL) gNoDisplayChangeFlag = 1;  /* -r: disable resolution changes */
    if (strstr(szCmdLineLower, kStrOptJ) != NULL) gNoJoystickFlag = 1;  /* -j: disable joystick support */
    if (strstr(szCmdLineLower, kStrOpt16) != NULL) gSound16BitFlag = 1;  /* -16: enable 16-bit sound */
    if (strstr(szCmdLineLower, kStrOptS) != NULL) gStereoSoundFlag = 1;  /* -s: enable stereo sound */
    if (strstr(szCmdLineLower, kStrOptE) != NULL) gNoEjectFlag  = 1; /* -e: undocumented */

    /* DF_FIX_SKIP_DISPLAY_CHECK, debug-only, no original counterpart: "-skipdisplaycheck"
     * forces CheckDisplayColourDepth's result to 0 (depth OK), bypassing modern GPU drivers'
     * lack of a genuine 8bpp mode (see docs/differences.md). The real binary
     * gets past this through a Windows compatibility shim (DWM8And16BitMitigation) applied to
     * that specific file; replicating it by hand did not work. Never set unless explicitly
     * requested; the 7 real command-line tokens above are still verified against TI.EXE's
     * bytes. */
#if DF_FIX_SKIP_DISPLAY_CHECK
    bDebugSkipDisplayCheck = (strstr(szCmdLineLower, "-skipdisplaycheck") != NULL);
#endif

    if (DetectCPUType() != 0) _exit(-1);   /* detect 386 CPU; sets gIs386Cpu; always 0 */
    if (CheckWindowsVersion() != 0) _exit(-1);
    if (CheckFileHandles() != 0) _exit(-1);

    if ((short)InitJoystick() != 0) _exit(-1);

    SetErrorMode(0x8005);                       /* SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX */
    /* 0x0041bd4e: push 0x80 -- HIGH_PRIORITY_CLASS (the port had
     * ABOVE_NORMAL_PRIORITY_CLASS, 0x8000, under a "0x80" comment) */
#if DF_FIX_ABOVE_NORMAL_PRIORITY
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
#else
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif

    if (InitMemoryManager())
        _exit(-1);
    else
        /* an else in the exe: the exit path jumps past it */
        InitHeap(gPoolBase, gPoolAllocatedBytes, 0x1000);
    InitializeEngineState();
    gGamePaused = 0;

    gWatchCursor = LoadCursorA(gInstance, "CURS.WATCH" /* 0x0045bafc */);

    gNullBrush = (int)GetStockObject(NULL_BRUSH);      /* 5  */
    gBlackBrush = (int)GetStockObject(BLACK_BRUSH);     /* 4  */
    gWhiteBrush = (int)GetStockObject(WHITE_BRUSH);     /* 0  */
    gNullPen = (int)GetStockObject(NULL_PEN);        /* 8  */
    gBlackPen = (int)GetStockObject(BLACK_PEN);       /* 7  */
    gSystemFont = (int)GetStockObject(SYSTEM_FONT);     /* 13 */
    gDefaultPalette =      GetStockObject(DEFAULT_PALETTE); /* 15 */

    if ((gNullBrush == 0) || (gBlackBrush == 0) || (gWhiteBrush == 0) ||
        (gNullPen == 0) || (gBlackPen == 0) || (gSystemFont == 0))
    {
        FatalError(0x66, 0x4fc);     /* fatal: stock object missing */
    }

    gMainMenu = LoadMenuA(gInstance, (LPCSTR)&kStrMENU);
    gIsMenuVisible = 0;

    wc.lpszClassName  = "DFWINDOW" /* 0x0045bae8 */;
    wc.lpfnWndProc    = WndProc_DFWINDOW;
    wc.hInstance      = gInstance;
    wc.hCursor        = LoadCursorA(gInstance, "CURS.ARROW" /* 0x0045badc */);
    wc.hIcon          = LoadIconA  (gInstance, "WINDOWICON" /* 0x0045bad0 */);
    wc.lpszMenuName   = NULL;
    wc.hbrBackground  = GetStockObject(BLACK_BRUSH);   /* 4 */
    wc.style          = 0;
    wc.cbWndExtra     = 0x18;   /* 24 bytes of per-window data */
    wc.cbClsExtra     = 0;
    RegisterClassA(&wc);

    wc.cbWndExtra    = 0;
    wc.hCursor       = gWatchCursor;           /* watch cursor */
    wc.lpszClassName = "DFCOVER" /* 0x0045bac8 */;
    wc.lpfnWndProc   = WndProc_DFCOVER;
    wc.style         = CS_NOCLOSE;             /* 0x200 (0x0041bee5; was CS_PARENTDC = 0x80) */
    RegisterClassA(&wc);

    /* the window geometry and colour depth come from string resources holding decimal text */
    LoadStringA(gInstance, 2, szCmdLineLower, 0x20);
    _sscanf(szCmdLineLower, kStrPctD, &nWindowWidth);

    LoadStringA(gInstance, 3, szCmdLineLower, 0x20);
    _sscanf(szCmdLineLower, kStrPctD, &nWindowHeight);

    LoadStringA(gInstance, 4, szCmdLineLower, 0x20);
    pDepth = (int *)szScratch;
    _sscanf(szCmdLineLower, kStrPctD, pDepth);

    LoadStringA(gInstance, 5, szCmdLineLower, 0x20);
    gCustomPaletteFlag = (int)szCmdLineLower[0];    /* colour-depth flag char */

    InitDisplayMode(nWindowWidth, nWindowHeight, *pDepth);

    gCoverWindow = CreateWindowExA(
        0,
        "DFCOVER" /* 0x0045bac8 */,
        (LPCSTR)gAppTitle,    /* app title */
        WS_POPUP,                /* 0x80000000 */
        0, 0,
        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL,
        gInstance, NULL);

    ShowWindow(gCoverWindow, SW_SHOWMAXIMIZED);    /* nCmdShow = 3 */

    hDC = GetDC(gCoverWindow);
    SelectPalette(hDC, (HPALETTE)gDefaultPalette, FALSE);
    RealizePalette(hDC);
    ReleaseDC(gCoverWindow, hDC);

    UpdateWindow(gCoverWindow);
    SetCursor(gWatchCursor);

    ApplyDisplayMode();   /* switch display to game resolution */

    /* CheckDisplayColourDepth returns 0 / 1 / 2, but the branches below exit on anything
     * non-zero, so in practice this only proceeds when the depth is already right (0): neither
     * dialog button leads back into gameplay once the dialog has appeared. */
#if DF_FIX_SKIP_DISPLAY_CHECK
    if (bDebugSkipDisplayCheck)
    {
        /* DEBUG-ONLY (see -skipdisplaycheck above): skip the check outright rather than
         * discarding its result, because CheckDisplayColourDepth puts up a modal MB_TASKMODAL
         * message box before it returns anything; unattended runs (gdb sessions, the DF_TRACE
         * build) would hang at startup on any modern desktop, which is never 8bpp. */
        iResult = 0;
    }
    else
#endif
    {
        iResult = CheckDisplayColourDepth();
    }

    if (iResult == 2)
    {
        RestoreDisplayMode();
        DestroyWindow(gCoverWindow);
        gCoverWindow = (HWND)0x0;
        SubsystemCleanup();

        strcpy(szCmdLineLower, GetStringResource(0x8d));
        RevertDisplayMode(szCmdLineLower);
        _exit(-1);
    }

    if (iResult != 0 || (iResult = CheckSoundCardCapability(), iResult != 0))
    {
        RestoreDisplayMode();
        DestroyWindow(gCoverWindow);
        gCoverWindow = (HWND)0x0;
        SubsystemCleanup();
        _exit(-1);
    }

    if (InitGraphicsBackend() == 0)
    {
        strcpy(szCmdLineLower, GetStringResource(0x8e));
        ShowInfoDialog(GetStringResource(0x8f), szCmdLineLower);

        RestoreDisplayMode();
        DestroyWindow(gCoverWindow);
        gCoverWindow = (HWND)0x0;
        SubsystemCleanup();
        _exit(-1);
    }

    gViewportCount = 0;
    gEventQueueCount = 0;
    gThePort   = NULL;
    gAccel = (int)LoadAcceleratorsA(gInstance, "ACCELTABLE" /* 0x0045bab8 */);

    if ((HACCEL)gAccel != NULL)
    {
        hResInfo     = FindResourceA(gInstance, "ACCELTABLE" /* 0x0045bab8 */, (LPCSTR)RT_ACCELERATOR);
        gNavCommandCount = SizeofResource(gInstance, hResInfo) >> 3;    /* initial estimate: size / 8 */

        gNavCommandTable = LocalAlloc(LMEM_ZEROINIT, gNavCommandCount << 3);

        hResData = LoadResource(gInstance, hResInfo);
        pbAccel  = (BYTE *)LockResource(hResData);

        gNavCommandCount = 1;
        while ((*pbAccel & 0x80) == 0)    /* 0x80 = last entry flag */
        {
            pbAccel       += 8;
            gNavCommandCount  += 1;
        }
        /* pbAccel now points at last entry; back-compute start and copy all
         * (LIBCMT's memcpy, called: the exe does not inline it here). The
         * exe computes 1 - n on its own (mov ecx,1; sub ecx,[n]; lea
         * eax,[eax+ecx*8]) and n*8 separately: the index went through a
         * variable, and the size is unsigned (n * sizeof of an 8-byte entry). */
        iResult = 1 - gNavCommandCount;
        _memcpy(gNavCommandTable,
                pbAccel + iResult * 8,  /* start of table */
                gNavCommandCount * sizeof(double));

        FreeResource(hResData);
    }
    else
    {
        gNavCommandCount = 0;
    }

    gIsAppActive  = 1;
    gIsSceneBitmapSelected  = 0;
    gBackBufferDC  = CreateBackBufferDC();   /* create off-screen / back-buffer DC */

    pbh = &gBitmapHeader;
    gBitmapHeader.biWidth  = 0;
    pbh->biPlanes  = 1;   /* gBitmapHeader.biPlanes, through a local pointer copy: the exe keeps this store
                           * after the two above, as C2 does only for a possibly aliased store */
    gLogPal.palNumEntries = 0x100;   /* a word store in the exe: LOGPALETTE.palNumEntries */
    gBitmapHeader.biHeight  = 0;
    gBitmapHeader.biCompression  = 0;
    gBitmapHeader.biSizeImage  = 0;
    gBitmapHeader.biXPelsPerMeter  = 0;
    gBitmapHeader.biYPelsPerMeter  = 0;
    gBitmapHeader.biClrUsed  = 0x100;
    gBitmapHeader.biClrImportant  = 0x100;
    gBitmapHeader.biSize = 0x28;
    gLogPal.palVersion  = 0x300;    /* LOGPALETTE palVersion = 0x0300 */
    gBitmapHeader.biBitCount  = 8;

    /* gCustomPaletteFlag == 0 (palettised): borrow the system palette entries for indices 0,
     * 1-9, 246-254 and 255, and zero the application indices 10-245. Non-zero (true-colour /
     * custom palette): zero everything and load the custom palette through
     * ManageSystemPalette(1). */
    hDC = GetDC(NULL);   /* screen DC for system palette queries */

    if (gCustomPaletteFlag != 0)
    {
        /* True-colour mode: zero entire palette then load custom colours */
        pe.peRed = pe.peGreen = pe.peBlue = 0;
        SetPaletteEntry(0, 0, 0, 0);

        for (iPalIdx = 1; iPalIdx < 0xff; iPalIdx++)
        {
            SetPaletteEntry(iPalIdx, 0, 0, 0);
        }

        pe.peRed = pe.peGreen = pe.peBlue = 0xff;
        SetPaletteEntry(0xff, 0xff, 0xff, 0xff);

        ManageSystemPalette(1);   /* acquire system palette (SYSPAL_NOSTATIC) */
    }
    else
    {
        /* SetPaletteEntry (0x0041c5a0) is defined after AppInit and the exe's
         * calls below pass the channel bytes as K&R-promoted ints (xor eax,eax;
         * mov al,..): the original had no prototype in scope here. The cast
         * reproduces that call for VC++; GCC gets the plain prototyped call
         * (the callee reads the low words either way). */
#ifdef _MSC_VER
#define SetPaletteEntry_KR ((void (__cdecl *)())SetPaletteEntry)
#else
#define SetPaletteEntry_KR SetPaletteEntry
#endif
        pe.peRed = pe.peGreen = pe.peBlue = 0;
        SetPaletteEntry(0, 0, 0, 0);

        for (iPalIdx = 1; iPalIdx < 10; iPalIdx++)
        {
            GetSystemPaletteEntries(hDC, iPalIdx, 1, &pe);
            SetPaletteEntry_KR(iPalIdx, pe.peRed, pe.peGreen, pe.peBlue);
        }

        for (iPalIdx = 10; iPalIdx < 0xf6; iPalIdx++)
        {
            SetPaletteEntry(iPalIdx, 0, 0, 0);
        }

        for (iPalIdx = 0xf6; iPalIdx < 0xff; iPalIdx++)
        {
            GetSystemPaletteEntries(hDC, iPalIdx, 1, &pe);
            SetPaletteEntry_KR(iPalIdx, pe.peRed, pe.peGreen, pe.peBlue);
        }

        pe.peRed = pe.peGreen = pe.peBlue = 0xff;
        SetPaletteEntry(0xff, 0xff, 0xff, 0xff);
    }
    ReleaseDC(NULL, hDC);

    gPalette = NULL;              /* reset before first SyncLogicalPalette call */
    SyncLogicalPalette(0);          /* create 256-entry logical HPALETTE */

    gMainWindow  = 0;
    ManageEmbeddedFont(1);   /* extract & install embedded font -- font.c */

}


/* Clears the engine state word and sets the ready flag. */
void __stdcall InitializeEngineState(void)  /* 0x0041c440 */

{
  DAT_00489d94 = 0;
  gIsMenuVisible = 1;
  return;
}


/* No-op stub; called during Shutdown() and on fatal-exit paths in AppInit. */
void __stdcall SubsystemCleanup(void)  /* 0x0041C460 */
{
    return;
}


/* Loads the rendering back end and resolves its function pointers: GDI32.DLL (CreateDIBSection /
 * SetDIBColorTable; NT and Win9x only, skipped on Win 3.x) or, if that is unavailable or
 * incomplete, WING32.DLL (WinGCreateDC, WinGCreateBitmap, WinGSetDIBColorTable, WinGBitBlt).
 * Sets gIsWinGBackend accordingly. */
int __stdcall InitGraphicsBackend(void)  /* 0x0041c470 */
{
    /* Path 1 -- GDI32.DLL (NT and Win9x only; skipped on Win 3.x). The module
     * handle lives in the global and is re-read for every GetProcAddress. */
    if (gIsWin31 == 0)
    {
        gIsWinGBackend = 0;   /* select GDI DIBSection back-end */
        gGraphicsLibrary = (int)LoadLibraryA("GDI32.DLL" /* 0x0045bba0 */);
        if (gGraphicsLibrary != 0)
        {
            gCreateDIBSectionProc = (int)GetProcAddress((HMODULE)gGraphicsLibrary, "CreateDIBSection" /* 0x0045bb8c */);
            gSetDIBColorTableProc = (int)GetProcAddress((HMODULE)gGraphicsLibrary, "SetDIBColorTable" /* 0x0045bb78 */);
            if (gCreateDIBSectionProc != 0 && gSetDIBColorTableProc != 0)
                return 1;
        }
    }
    /* Path 2 -- WING32.DLL (Win 3.x, or GDI32 unavailable / incomplete).
     * Proc -> global order confirmed from the exe (0x0041c4fe..0x0041c548). */
    gGraphicsLibrary = (int)LoadLibraryA("WING32.DLL" /* 0x0045bb6c */);
    if (gGraphicsLibrary == 0)
        return 0;
    gWinGCreateDCProc = (int)GetProcAddress((HMODULE)gGraphicsLibrary, "WinGCreateDC" /* 0x0045bb5c */);
    gWinGCreateBitmapProc = (int)GetProcAddress((HMODULE)gGraphicsLibrary, "WinGCreateBitmap" /* 0x0045bb48 */);
    gWinGSetDIBColorTableProc = (int)GetProcAddress((HMODULE)gGraphicsLibrary, "WinGSetDIBColorTable" /* 0x0045bb30 */);
    gWinGBitBltProc = (int)GetProcAddress((HMODULE)gGraphicsLibrary, "WinGBitBlt" /* 0x0045bb24 */);
    if (gWinGCreateDCProc != 0 && gWinGCreateBitmapProc != 0 && gWinGSetDIBColorTableProc != 0 && gWinGBitBltProc != 0)
    {
        gIsWinGBackend = 1;   /* WinG back-end */
        return 1;
    }
    return 0;
}


/* Creates the off-screen device context: CreateCompatibleDC(NULL) for GDI, WinGCreateDC() (a tail
 * call) for WinG. Returns the HDC. */
HDC __cdecl CreateBackBufferDC(void)  /* 0x0041c580 */
{
    if (gIsWinGBackend != 0) {
        return ((HDC (WINAPI*)())gWinGCreateDCProc)();   /* WinGCreateDC() */
    }
    return CreateCompatibleDC(NULL);
}


/* Writes colour (r, g, b) at palette index idx into all three palette tables (gPal16,
 * gDibColorTable, gPalEntries) and computes the PALETTEENTRY flags. 8bpp (gCustomPaletteFlag ==
 * 0): indices 10-245 get 0x05 (PC_RESERVED | PC_NOCOLLAPSE), indices 0-9 and 246-255 get 0
 * (system owned). True-colour (gCustomPaletteFlag != 0): indices 1-254 get 0x05, indices 0 and
 * 255 (the black / white anchors) get 0. */
void __cdecl SetPaletteEntry(int idx, unsigned short r, unsigned short g, unsigned short b)  /* 0x0041c5a0 */
{
    /* The channels are words in the exe (`mov dx,word ptr`, `shl ax,8`);
     * callers pass bytes, which arrive zero-extended. */
    gPal16[idx].wRed   = r << 8;
    gPal16[idx].wGreen = g << 8;
    gPal16[idx].wBlue  = b << 8;
    gDibColorTable[idx].rgbRed      = (BYTE)r;
    gDibColorTable[idx].rgbGreen    = (BYTE)g;
    gDibColorTable[idx].rgbBlue     = (BYTE)b;
    gDibColorTable[idx].rgbReserved = 0;
    gPalEntries[idx].peRed   = (BYTE)r;
    gPalEntries[idx].peGreen = (BYTE)g;
    gPalEntries[idx].peBlue  = (BYTE)b;
    if (gCustomPaletteFlag != 0) {
        if (idx != 0 && idx != 0xff)
            gPalEntries[idx].peFlags = 5;
        else
            gPalEntries[idx].peFlags = 0;
    } else {
        if (idx >= 10 && idx <= 0xf5)
            gPalEntries[idx].peFlags = 5;
        else
            gPalEntries[idx].peFlags = 0;
    }
}


/* Reverts display mode via ChangeDisplaySettingsA(NULL, 0); prompts to
 * restart Windows if DISP_CHANGE_RESTART is returned.
 *
 * Reads message from parameter (not caller's stack frame); see appinit.c. */
void __cdecl RevertDisplayMode(const char *pszCallerMsg)  /* 0x0041c670 */
{
    char  szWrapped[512];
    int   iResult;
    BOOL  bAlternate, bPreferred;
    DEVMODEA dm;   /* EnumDisplaySettingsA output buffer -- see ApplyDisplayMode
                    * for the same NULL-pointer crash this call had too */

    if (gUser32Lib == 0 || gPreferredDisplayMode < 0)
        return;

#if !DF_FIX_REVERT_TO_DEFAULT_MODE
    /* The exe: enumerate mode 0 and the chosen mode, then APPLY the chosen
     * mode and write it to the registry (CDS_UPDATEREGISTRY, dmFields =
     * BITSPERPEL|PELSWIDTH|PELSHEIGHT); on DISP_CHANGE_RESTART offer a reboot. */
    dm.dmSize = sizeof(DEVMODEA);
    bPreferred = ((PFNENUMDS)gEnumDisplaySettingsProc)(NULL, 0, &dm);
    bAlternate = ((PFNENUMDS)gEnumDisplaySettingsProc)(NULL, (DWORD)gPreferredDisplayMode, &dm);
    if ((bAlternate & bPreferred) == 0)
        return;
    dm.dmFields = 0x1c0000;
    iResult = ((PFNCHANGEDS)gChangeDisplaySettingsProc)(&dm, 1);   /* CDS_UPDATEREGISTRY */
    if (iResult == 1)   /* DISP_CHANGE_RESTART */
    {
        WordWrapText((char *)pszCallerMsg, szWrapped);
        if (MessageBoxA(NULL, szWrapped, GetStringResource(0x64), 0x2041) == IDOK)
            ((PFNEXITWIN)gExitWindowsExProc)(2, 0);   /* EWX_REBOOT */
    }
#else

    dm.dmSize = sizeof(DEVMODEA);
    bPreferred = ((PFNENUMDS)gEnumDisplaySettingsProc)(NULL, (DWORD)gPreferredDisplayMode, &dm);
    dm.dmSize = sizeof(DEVMODEA);
    bAlternate = ((PFNENUMDS)gEnumDisplaySettingsProc)(NULL, (DWORD)gAlternateDisplayMode, &dm);
    if ((bPreferred & bAlternate) == 0)
        return;

    iResult = ((PFNCHANGEDS)gChangeDisplaySettingsProc)(NULL, 0);

    if (iResult == 1)   /* DISP_CHANGE_RESTART */
    {
        WordWrapText((char *)pszCallerMsg, szWrapped);
        iResult = MessageBoxA(NULL, szWrapped,
                              GetStringResource(0x64),
                              0x2041);  /* MB_OKCANCEL|MB_ICONINFORMATION|MB_TASKMODAL */

        if (iResult == IDOK)
        {
            ((PFNEXITWIN)gExitWindowsExProc)(0, 0);   /* log off */
        }
    }
#endif
}


/* Resolves display-change API pointers from USER32.DLL and finds the
 * display mode indices that match the requested screen geometry. */
void __cdecl InitDisplayMode(int nWidth, int nHeight, unsigned int nBpp)  /* 0x0041c730 */
{
    if (gNoDisplayChangeFlag != 0 || gIsWin31 != 0)
        return;

    /* Load USER32.DLL and resolve display-change API entry points (the exe
     * re-reads the module handle from its global for each call) */
    gUser32Lib = LoadLibraryA("USER32.DLL" /* 0x0045bbec */);
    if (gUser32Lib == NULL)
        return;

    gChangeDisplaySettingsProc = (void *)GetProcAddress(gUser32Lib, "ChangeDisplaySettingsA" /* 0x0045bbd4 */);
    gEnumDisplaySettingsProc = (void *)GetProcAddress(gUser32Lib, "EnumDisplaySettingsA" /* 0x0045bbbc */);
    gExitWindowsExProc = (int)GetProcAddress(gUser32Lib, "ExitWindowsEx" /* 0x0045bbac */);

    /* Neither failure below returns: the exe frees the library, clears the
     * handle and carries on (0x0041c7cb jmp, 0x0041c820 fall-through). */
    if (gEnumDisplaySettingsProc == 0 || gChangeDisplaySettingsProc == 0 || gExitWindowsExProc == 0) {
        FreeLibrary(gUser32Lib);
        gUser32Lib = 0;
    }

    /* Compute fixed-point aspect ratio (height / width x 1000). It is kept
     * in nHeight, not a new local: the exe holds it in ebx and nBpp in ebp,
     * which only this spelling reproduces at every symbol count. */
    nHeight = (nHeight * 1000) / nWidth;

    gPreferredDisplayMode = FindBestDisplayMode(nWidth, nHeight, nBpp, 0);
    gAlternateDisplayMode = FindBestDisplayMode(nWidth, nHeight, nBpp, 1);
    if (gAlternateDisplayMode < 0) {
        FreeLibrary(gUser32Lib);
        gUser32Lib = 0;
    }

    gIsDisplayModeChanged = 0;
}


/* Enumerates all display modes via EnumDisplaySettingsA; selects best by
 * width distance, aspect ratio, bpp; optionally tests with CDS_TEST.
 * Returns 0-based mode index, or -1 if no mode found. */
static int __cdecl FindBestDisplayMode(int nTargetWidth, int nTargetAspect,  /* 0x0041c840 */
                                       unsigned int nTargetBpp, int bValidate)
{
    int   bestIndex;
    int   bestAspectDist;
    DEVMODEA dm;
    int   bestWidthDist;
    int   widthDist;
    int   aspectDist;
    int   modeIndex;
    uint  bestBpp;

    bestWidthDist = 0x7FFFFFFF;
    bestIndex = -1;
    bestBpp = 0;
    modeIndex = 0;
    for (;;) {
        dm.dmSize = sizeof(DEVMODEA);   /* 0x9C = 156 */

        if (!((PFNENUMDS)gEnumDisplaySettingsProc)(NULL, modeIndex, &dm))
            return bestIndex;

        widthDist = (int)dm.dmPelsWidth - nTargetWidth;
        if (widthDist < 0) widthDist = -widthDist;

        aspectDist = (int)((dm.dmPelsHeight * 1000) / dm.dmPelsWidth) - nTargetAspect;
        if (aspectDist < 0) aspectDist = -aspectDist;

        /* Accept this mode if it is a better match than the current best:
         *   - strictly closer width, OR
         *   - same width + strictly closer aspect, OR
         *   - same width + same aspect + current best bpp != target and this
         *     mode has at least 8 bpp (0x0041c8d6: cmp [dm.dmBitsPerPel],8 / jb;
         *     the port had tested dmFields > 7, always true) */
        if (bestWidthDist >= widthDist &&
            (bestWidthDist != widthDist ||
             (aspectDist <= bestAspectDist &&
              (aspectDist != bestAspectDist ||
               (bestBpp != nTargetBpp && dm.dmBitsPerPel >= 8)))))
        {
            dm.dmFields = 0x1C0000;   /* DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT */
            if (bValidate == 0 || ((PFNCHANGEDS)gChangeDisplaySettingsProc)(&dm, 2) == 0) {   /* CDS_TEST */
                bestBpp        = dm.dmBitsPerPel;
                bestWidthDist  = widthDist;
                bestAspectDist = aspectDist;
                bestIndex      = modeIndex;
            }
        }
        modeIndex++;
    }
}


/* Show info dialog with word-wrapped message body. */
void __cdecl ShowInfoDialog(LPCSTR pszTitle, char *pszMessage)  /* 0x0041c930 */
{
    char szWrapped[512];

    WordWrapText(pszMessage, szWrapped);
    MessageBoxA(NULL, szWrapped, pszTitle, 0x2010);
}



/* Checks that the display is 256-colour (8 bpp). Returns 0 on success, 1 to continue despite the
 * wrong depth, 2 to abort. */
int __stdcall CheckDisplayColourDepth(void)  /* 0x0041c970 */
{
    HDC  hDC;
    int  nPlanes;
    int  nBits;
    int  nColourDepth;   /* effective depth = BITSPIXEL * PLANES */
    int  nScreenW;
    int  nScreenH;
    char szDepthLabel[16];
    char szMessage[512];

    hDC = GetDC(NULL);
    nBits = GetDeviceCaps(hDC, BITSPIXEL);
    nPlanes = GetDeviceCaps(hDC, PLANES);
    nColourDepth = nBits * nPlanes;
    ReleaseDC(NULL, hDC);
    nScreenW = GetSystemMetrics(SM_CXSCREEN);
    nScreenH = GetSystemMetrics(SM_CYSCREEN);

    switch (nColourDepth) {
    case 1:
        strcpy(szDepthLabel, GetStringResource(0x66));   /* "monochrome" */
        break;
    case 4:
        strcpy(szDepthLabel, GetStringResource(0x67));   /* "16 colors" */
        break;
    case 8:
        strcpy(szDepthLabel, GetStringResource(0x68));   /* "256 colors" */
        break;
    default:
        _sprintf(szDepthLabel, GetStringResource(0x69), nColourDepth);
        break;
    }

    /* Ground truth: the 8-bit return is the last block, and the two branches
     * each start with the same _sprintf (the exe hoists its pushes above the
     * gIsWin31 test). */
    if (nColourDepth != 8) {
        if (gIsWin31 == 0) {
            _sprintf(szMessage, GetStringResource(0x6a), szDepthLabel, nScreenW, nScreenH);
            strcat(szMessage, GetStringResource(0x6b));
            /* ShowOKCancelBox: 1 = Cancel -> 1, OK -> 2 (AppInit exits on both). */
            return ShowOKCancelBox(GetStringResource(0x6c), szMessage) == 1 ? 1 : 2;
        }
        _sprintf(szMessage, GetStringResource(0x6a), szDepthLabel, nScreenW, nScreenH);
        strcat(szMessage, GetStringResource(0x6d));
        ShowInfoDialog(GetStringResource(0x6e), szMessage);
        return 1;
    }
    return 0;
}


/* Word-wraps pszMessage to 40 characters per line (WordWrapText) into a local buffer and shows a
 * modal MB_OKCANCEL | MB_ICONHAND | MB_TASKMODAL (0x2011) message box. Returns 1 if the user
 * clicked Cancel (IDCANCEL), 0 for OK. For CheckDisplayColourDepth, Cancel means ignore the display
 * warning and continue loading the game; OK acknowledges and quits. */
int __cdecl ShowOKCancelBox(LPCSTR pszTitle, char *pszMessage)  /* 0x0041cb40 */
{
    char   szExpanded[512];
    int    iResult;

    WordWrapText(pszMessage, szExpanded);

    iResult = MessageBoxA(
                  NULL,
                  szExpanded,
                  pszTitle,
                  0x2011);    /* MB_OKCANCEL | MB_ICONHAND | MB_TASKMODAL */

    return iResult == IDCANCEL;
}


/* Returns 0 on success, 1 if no 22050 Hz-capable audio device is found. */
int __stdcall CheckSoundCardCapability(void)  /* 0x0041cb90 */
{
    char szMessage[384];

    if (gIsWin31 == 0)
    {
        if (ProbeWaveOutFormat(0x5622, 0x10, 2) != 0)
            return 0;   /* found */
        if (ProbeWaveOutFormat(0x5622, 0x08, 2) != 0)
            return 0;   /* found */
    }

    if (ProbeWaveOutFormat(0x5622, 0x10, 1) != 0)
        return 0;   /* found */
    if (ProbeWaveOutFormat(0x5622, 0x08, 1) != 0)
        return 0;   /* found */

    strcpy(szMessage, GetStringResource(0x7f));    /* "%p requires a multimedia sound card..." */
    strcat(szMessage, GetStringResource(0x80));    /* "If you have a sound card installed..." */

    ShowInfoDialog(GetStringResource(0x81), szMessage);   /* "Sound Error" */
    return 1;   /* failure */
}


/* Tests whether any installed waveform audio output device can play
 * audio at the specified PCM format.  Used by CheckSoundCardCapability to
 * verify that the system has working audio hardware.
 *
 * Tries each enumerated device in order.  If device 0 fails, stops
 * any currently playing system sound (sndPlaySoundA) and retries.
 * As a last resort, tries WAVE_MAPPER (0xFFFFFFFF).
 *
 * Returns 1 if a device supports the format, 0 if none do. */
static int __cdecl ProbeWaveOutFormat(DWORD nSamplesPerSec, int nBitsPerSample, int nChannels)  /* 0x0041ccc0 */
{
    HWAVEOUT      hwo;
    int           align;
    UINT          numDevs;
    UINT          devId;
    PCMWAVEFORMAT wfx;  /* 16 bytes, no cbSize: the exe's frame is 0x14 (hwo +
                         * this), and cbSize is ignored for WAVE_FORMAT_PCM.
                         * was: separate loose locals cast via &wFormatTag,
                         * relying on the original MSVC binary's specific
                         * stack layout placing them contiguously in
                         * WAVEFORMATEX field order -- a compiler-specific
                         * coincidence, not real C. MinGW's locals aren't laid
                         * out that way, so waveOutOpen read garbage for every
                         * field after the first, requesting a nonsense format
                         * no device could ever match on any machine. Same bug
                         * class as RevertDisplayMode's pszCallerMsg. */

    align                  = nBitsPerSample / 8 * nChannels;
    wfx.wf.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.wf.nChannels       = nChannels;
    wfx.wf.nSamplesPerSec  = nSamplesPerSec;
    wfx.wBitsPerSample     = nBitsPerSample;
    wfx.wf.nBlockAlign     = align;
    wfx.wf.nAvgBytesPerSec = nSamplesPerSec * align;
    /* a while loop, and sndPlaySoundA shifted in unit_layout.json: the exe loads waveOutOpen
     * once above the guard (as in OpenWaveOutPrimaryDevice) */
    numDevs = waveOutGetNumDevs();
    devId   = 0;
    while ((int)devId < (int)numDevs)
    {
            if (waveOutOpen(&hwo, devId, (LPWAVEFORMATEX)&wfx, 0, 0, WAVE_FORMAT_QUERY) == MMSYSERR_NOERROR)
                break;
            if (devId == 0)
            {
                sndPlaySoundA(NULL, 0);
                if (waveOutOpen(&hwo, devId, (LPWAVEFORMATEX)&wfx, 0, 0, WAVE_FORMAT_QUERY) == MMSYSERR_NOERROR)
                    break;
            }
        devId++;
    }

    if (devId == numDevs)
    {
        if (waveOutOpen(&hwo, (UINT)-1, (LPWAVEFORMATEX)&wfx, 0, 0, WAVE_FORMAT_QUERY) != MMSYSERR_NOERROR)
            return 0;   /* no device supports this format */
    }

    return 1;   /* format supported */
}


/* Checks the Windows version and refuses unsupported ones: pre-3.x Windows (strings 0x82 / 0x83),
 * Windows 3.0 (0x84 / 0x85) and NT 3.50 or earlier (0x86). NT 3.51 or later is supported, and
 * ChangeDisplaySettingsA is trusted from build 1381 (NT 4.0 RTM) on. The NT build number (gWinNTBuild)
 * comes from GetVersionExA and gates that support. */
int __stdcall CheckWindowsVersion(void)  /* 0x0041cd90 */
{
    DWORD dwVersion;
    BYTE  major;
    BYTE  minor;
    char  szBuf[384];

    dwVersion = GetVersion();
    major = LOBYTE(LOWORD(dwVersion));
    minor = HIBYTE(LOWORD(dwVersion));
    /* Case 1: pre-3.x Windows -- "Mouldy Windows" (0x82 text, 0x83 title).
     * The "NT " / "" prefix is chosen inline in the argument list. */
    if (major < 3) {
        _sprintf(szBuf, GetStringResource(0x82),
                 IsWindowsNT() ? kStrNtPrefix : kStrEmpty, major, minor);
        ShowInfoDialog(GetStringResource(0x83), szBuf);
        return 1;
    }
    /* Case 2: Windows 3.0 -- unsupported (0x84 text, 0x85 title) */
    if (major == 3 && minor < 10) {
        _sprintf(szBuf, GetStringResource(0x84),
                 IsWindowsNT() ? kStrNtPrefix : kStrEmpty, minor);
        ShowInfoDialog(GetStringResource(0x85), szBuf);
        return 1;
    }
    /* NT: 3.51 or later is supported; ChangeDisplaySettingsA is trusted from
     * build 1381 (NT 4.0 RTM) on */
    if (IsWindowsNT()) {
        if (major * 100 + minor > 0x15e) {
            gIsWin31 = 0;
            gIsWin95 = 0;
            if ((int)gWinNTBuild >= 0x565)
                gIsWin95OrNT4 = 1;
            else
                gIsWin95OrNT4 = 0;
            return 0;
        }
        /* NT 3.50 or earlier (0x86: "...NT version %d.%02d (build %d).") */
        _sprintf(szBuf, GetStringResource(0x86), major, minor, HIWORD(dwVersion));
        ShowInfoDialog(GetStringResource(0x85), szBuf);
        return 1;
    }
    /* Non-NT: Win 95/98 (> 3.50), else Win 3.1x */
    if (major * 100 + minor > 0x15e) {
        gIsWin95OrNT4 = gIsWin95 = 1;
        gIsWin31 = 0;
    } else {
        gIsWin95OrNT4 = gIsWin95 = 0;
        gIsWin31 = 1;
    }
    return 0;
}


/* Returns non-zero when GetVersionExA reports VER_PLATFORM_WIN32_NT. */
BOOL __stdcall IsWindowsNT(void)  /* 0x0041cfa0 */
{
    HMODULE        hKernel32;
    FARPROC        pfnGetVersionEx;
    OSVERSIONINFOA osvi;

    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);   /* stored first in the exe */
    hKernel32 = LoadLibraryA("KERNEL32.DLL");
    if (hKernel32 == NULL)
        return FALSE;

    pfnGetVersionEx = GetProcAddress(hKernel32, "GetVersionExA");
    if (pfnGetVersionEx == NULL) {
        FreeLibrary(hKernel32);
        return FALSE;
    }

    ((BOOL (WINAPI *)(LPOSVERSIONINFOA))pfnGetVersionEx)(&osvi);
    FreeLibrary(hKernel32);

    gWinNTBuild = osvi.dwBuildNumber;
    return osvi.dwPlatformId == VER_PLATFORM_WIN32_NT;  /* 2 */
}


/* Requests 64 file handles and, if fewer are available, shows the "Not Enough File Handles" error
 * (strings 0x87-0x89). */
int __stdcall CheckFileHandles(void)  /* 0x0041d020 */
{
    UINT uHandles;
    char szBuf[384];

    /* Request 64 file handles; returns how many are actually available. */
    uHandles = SetHandleCount(0x40);
    if ((int)uHandles < 0x20) {
        /* 0x87 "...only %d file handles...", 0x88 "If you have no FILES= line...",
         * 0x89 the title "Not Enough File Handles" */
        _sprintf(szBuf, (char *)GetStringResource(0x87), uHandles);
        strcat(szBuf, GetStringResource(0x88));
        ShowInfoDialog(GetStringResource(0x89), szBuf);
        return 1;
    }
    return 0;
}


/* Detects a 386 CPU (sets gIs386Cpu) through GetSystemInfo, resolved dynamically so the code
 * degrades gracefully where the API is absent (Win32s, early NT 3.x). Always returns 0, so
 * AppInit's guard is dead code for it. */
int __stdcall DetectCPUType(void)  /* 0x0041d0d0 */
{
    typedef void (WINAPI *PFN_GetSystemInfo)(LPSYSTEM_INFO);
    HMODULE           hKernel;
    PFN_GetSystemInfo pfnGetSystemInfo;

    gIs386Cpu = 0;
    hKernel = LoadLibraryA("KERNEL32.DLL" /* 0x0045b07c */);
    pfnGetSystemInfo = (PFN_GetSystemInfo)GetProcAddress(hKernel, "GetSystemInfo" /* 0x0045bc00 */);

    if (pfnGetSystemInfo != NULL)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);             /* the exe calls the import directly; GetProcAddress only gates it */

        if (si.dwProcessorType == PROCESSOR_INTEL_386)   /* 0x182 = 386 */
        {
            gIs386Cpu = 1;
        }
    }

    FreeLibrary(hKernel);
    return 0;
}


/* Install or remove the game's custom embedded font. */
void __cdecl ManageEmbeddedFont(int install)  /* 0x0041d130 */
{
    /* Declaration order sets the exe's register choice (hFind in ebp). */
    HANDLE           hFind;
    FileWriterCtx    ctx;
    CHAR             szTempPath[MAX_PATH];
    CHAR             szPattern[MAX_PATH];
    WIN32_FIND_DATAA findData;
    HRSRC            hResInfo;
    HGLOBAL          hResData;
    LPVOID           pFontData;
    DWORD            dwSize;

    GetTempPathA(sizeof(szTempPath), szTempPath);

    if (install != 0)
    {

        gTempFontPath[0] = '\0';

        hResInfo = FindResourceA(gInstance, "FONT.FONT" /* 0x0045bc20 */, kStrRAW);
        if (hResInfo == NULL)
        {
            /* Font resource not present -- silently skip; not fatal.
             * USER32's SetLastErrorEx(0, 0): the exe pushes two zeros. */
            SetLastErrorEx(0, 0);
            return;
        }

        dwSize    = SizeofResource(gInstance, hResInfo);
        hResData  = LoadResource(gInstance, hResInfo);
        pFontData = LockResource(hResData);

        if (pFontData == NULL)
        {
            return;
        }

        /* Create a unique temp filename; GetTempFileNameA also creates the
         * zero-byte placeholder file on disk. */
        GetTempFileNameA(szTempPath, kStrCFX, 0, gTempFontPath);

        /* Patch last 3 chars of the temp path from "TMP" to "FNT" in-memory.
         * The placeholder .TMP file is orphaned; it will be deleted next time
         * ManageEmbeddedFont(0) sweeps the temp directory. */
        gTempFontPath[strlen(gTempFontPath) - 3] = 'F';
        gTempFontPath[strlen(gTempFontPath) - 2] = 'N';
        gTempFontPath[strlen(gTempFontPath) - 1] = 'T';

        ctx.hFile = CreateFileA(gTempFontPath, GENERIC_WRITE, 0, NULL,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (ctx.hFile == INVALID_HANDLE_VALUE)
        {
            FreeResource(hResData);
            return;
        }
        ctx.reserved0 = 0;
        ctx.reserved1 = 0;
        InitializeCriticalSection(&ctx.cs);

        WriteToFileWithRetry(&ctx, pFontData, dwSize);
        CloseContainerFile(&ctx);
        FreeResource(hResData);

        if (AddFontResourceA(gTempFontPath) != 0)
            SendMessageA((HWND)0xffff, WM_FONTCHANGE, 0, 0);
        return;
    }

    if (gTempFontPath[0] != '\0' && RemoveFontResourceA(gTempFontPath))
        SendMessageA((HWND)0xffff, WM_FONTCHANGE, 0, 0);

    /* tempPath + kStrCfxFontGlob (wildcard): delete every matching file
     * (catches current .FNT and any prior-run leftovers). Each file's path
     * is built in gTempFontPath itself, as the exe does. */
    strcpy(szPattern, szTempPath);
    strcat(szPattern, kStrCfxFontGlob);
    hFind = FindFirstFileA(szPattern, &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do {
            strcpy(gTempFontPath, szTempPath);
            strcat(gTempFontPath, findData.cFileName);
            DeleteFileA(gTempFontPath);
        } while (FindNextFileA(hFind, &findData));
    }
    FindClose(hFind);
}


/* Sets up the game's memory: reads the total physical RAM and the three pool sizes from string
 * resources, shows a wait cursor, probes for the largest allocatable block and allocates the game
 * pool (AllocMemoryPool); if that fails, the OUTOFMEM dialog decides. */
BOOL __stdcall InitMemoryManager(void)  /* 0x0041d3f0 */
{
    int          cbProbeSize;
    void        *pProbeBlock;
    MEMORYSTATUS ms;

    ms.dwLength = sizeof(MEMORYSTATUS);
    GlobalMemoryStatus(&ms);
    gTotalPhysRam = ms.dwTotalPhys;

    /* the three pool sizes come from string resources, in KB (kStrPctLd = "%ld") */
    _sscanf(GetStringResource(6), kStrPctLd, &gPoolMinBytes);
    _sscanf(GetStringResource(7), kStrPctLd, &gPoolMaxBytes);
    _sscanf(GetStringResource(8), kStrPctLd, &gPoolReserveBytes);
    gPoolMinBytes <<= 10;
    gPoolMaxBytes <<= 10;
    gPoolReserveBytes <<= 10;
    gPoolReserveBytes += 0x400000;

    /* probe for the largest allocatable block in 64 KB steps, zeroing it to force the pages to
     * commit */
    SetCursor(LoadCursorA(NULL, IDC_WAIT));

    for (cbProbeSize = gTotalPhysRam; cbProbeSize > 0; cbProbeSize -= 0x10000) {
        pProbeBlock = malloc(cbProbeSize);
        if (pProbeBlock != NULL) {
            ZeroBlock(pProbeBlock, cbProbeSize);
            free(pProbeBlock);
            break;
        }
    }

    /* allocate the game pool; on failure the OUTOFMEM dialog decides */
    if (AllocMemoryPool(1) != 0)
        return FALSE;
    return DialogBoxParamA(gInstance, "OUTOFMEM" /* 0x0045bc30 */, (HWND)NULL,
                           (DLGPROC)OutOfMemoryDialogProc, 0) == 0;
}


/* Probes RAM (3 OS paths: NT/Win3x/Win9x), calculates pool size, allocates game heap.
 * doAllocate=0: probe only; doAllocate=1: probe + allocate. Returns 1 on success, 0 on failure. */
int __cdecl AllocMemoryPool(int doAllocate)  /* 0x0041d510 */
{
    HGLOBAL    hMem;
    void      *pvPool;
    SIZE_T     cbPool;
    SIZE_T     cbWorkingSet;
    MEMORYSTATUS ms;
    SIZE_T     cbUsable;

    ms.dwLength = sizeof(MEMORYSTATUS);
    GlobalMemoryStatus(&ms);
    cbUsable = ms.dwTotalPhys;          /* both probes start from it */
    gTotalPhysRam = cbUsable;
    gAvailPhysBytes = ms.dwAvailPhys;
    gAvailPageFileBytes = ms.dwAvailPageFile;

    if (gIsWin95 != 0)
    {
        /* Win 9x: query available RAM via custom function (VxD?) */
        cbUsable = QueryVxDAvailMem();
    }
    else if (gIsWin31 != 0)
    {
        /* Win 3.x: malloc/free probe loop (the exe tests this case first) */
        do {
            pvPool = malloc(cbUsable);
            if (pvPool != NULL) {
                free(pvPool);
                break;
            }
            cbUsable -= 0x10000;
        } while ((int)cbUsable > 0);

        if ((int)cbUsable < 0)
            cbUsable = 0;
    }
    else
    {
        /* NT: loop with SetProcessWorkingSetSize + GlobalAlloc probe */
        do {
            if (SetProcessWorkingSetSize(GetCurrentProcess(), cbUsable, cbUsable)) {
                hMem = GlobalAlloc(GMEM_FIXED, cbUsable);
                if (hMem != NULL) {
                    GlobalFree(hMem);
                    break;
                }
            } else {
                SetLastErrorEx(0, 0);   /* clear error; try smaller size (USER32, push 0 / push 0) */
            }
            cbUsable -= 0x10000;    /* subtract 64 KB */
        } while ((int)cbUsable > 0);

        /* Reserve 512 KB for OS overhead; floor at 0 then at minA */
        cbUsable -= 0x80000;
        if ((int)cbUsable < 0)
            cbUsable = 0;
        if ((int)cbUsable < (int)gPoolMinBytes)
            cbUsable = gPoolMinBytes;
    }

    gProbedUsableBytes = cbUsable;

    /* pool size: below the minimum (gPoolMinBytes) fails, above the maximum (gPoolMaxBytes) is
     * capped, else the usable size minus the reserve (gPoolReserveBytes) */
    /* `*(int *)&`: the exe re-reads the minimum at each of the three uses below;
     * a second `(int)gPoolMinBytes` lets the compiler keep it in a register. */
    if ((int)cbUsable < *(int *)&gPoolMinBytes) {
        gPoolSizeBytes = 0;
        return 0;
    }

    if ((int)cbUsable > (int)gPoolMaxBytes) {
        cbPool = gPoolMaxBytes;      /* cap at maxB ~= 11.7 MB */
    } else {
        cbPool = cbUsable - gPoolReserveBytes;
        if ((int)cbPool < (int)gPoolMinBytes)
            cbPool = gPoolMinBytes;  /* floor at minA ~= 3.9 MB */
    }

#if DF_TRACE_ENABLED
    /* Trace-build knob: DF_POOL_KB=<n> shrinks the resource pool so that the
     * eviction path runs within minutes instead of hours. */
    {
        const char *knob = getenv("DF_POOL_KB");
        if (knob != NULL && atoi(knob) > 0) {
            cbPool = (SIZE_T)atoi(knob) * 1024;
            if ((int)cbPool < (int)gPoolMinBytes) cbPool = gPoolMinBytes;
            DF_TRACE("memPool: DF_POOL_KB override -> pool=%d", (int)cbPool);
        }
    }
#endif
    gPoolSizeBytes = cbPool;
    /* On any machine with more than 4 GB GlobalMemoryStatus reports
     * dwTotalPhys = 0xFFFFFFFF; the working-set probe then fails once, the
     * 64 KB decrement makes the value negative, the loop exits on its first
     * pass and the pool collapses to the MINIMUM (gPoolMinBytes). Same code
     * path in TI.EXE on this hardware. Print the numbers once at startup. */
    DF_TRACE("memPool: totalPhys=%lu availPhys=%lu probe=%d -> usable=%d min=%d max=%d base=%d pool=%d",
             (unsigned long)ms.dwTotalPhys, (unsigned long)ms.dwAvailPhys,
             (int)gProbedUsableBytes, (int)cbUsable, (int)gPoolMinBytes, (int)gPoolMaxBytes,
             (int)gPoolReserveBytes, (int)cbPool);

    if (doAllocate == 0)
        return 0;   /* probe-only mode: size calculated, no allocation */

    if (gIsWin95 != 0)
    {
        /* Win 9x: custom VxD allocator */
        pvPool = AllocVxDBlock(cbPool);
    }
    else if (gIsWin31 != 0)
    {
        /* Win 3.x: plain malloc */
        pvPool = malloc(cbPool);
    }
    else
    {
        /* NT: GlobalAlloc then expand working set to cover pool + overhead */
        pvPool = GlobalAlloc(GMEM_FIXED, cbPool);

        cbWorkingSet = gPoolReserveBytes + 0x100000 + cbPool;  /* baseC + 1 MB + pool */
        do {
            if (SetProcessWorkingSetSize(GetCurrentProcess(), cbWorkingSet, cbWorkingSet))
                break;
            cbWorkingSet -= 0x10000;
        } while ((int)cbWorkingSet > 0);
    }

    if (pvPool == NULL)
        return 0;
    gPoolBase = pvPool;
    gPoolAllocatedBytes = cbPool;
    return 1;
}


/* Returns: available VxD-managed memory in bytes, or 0 on failure. */
int __stdcall QueryVxDAvailMem(void)  /* 0x0041d6e0 */
{
    BOOL  bOk;
    int   nPages;       /* IOCTL 2 output: available page count */
    DWORD dwReturned;

    OpenVxDDevice();

    bOk = DeviceIoControl(
            (HANDLE)gVxdDevice,
            2,              /* IOCTL: query available pages */
            NULL, 0,
            &nPages, sizeof(nPages),
            &dwReturned,
            NULL);

    if (bOk == FALSE)
        return 0;

    return nPages << 12;    /* pages x 4096 = bytes */
}


/* Opens the VFIXMEM VxD: a direct open first (the driver may already be loaded); otherwise it
 * extracts the VxD from the PE resources (VFIXMEM) to %WINDIR%\FIXMEM.VXD, re-opens that file with
 * FILE_FLAG_DELETE_ON_CLOSE so it cleans itself up, and retries the device open. Returns 1 (the
 * handle is in gVxdDevice) or 0. */
int __stdcall OpenVxDDevice(void)  /* 0x0041d720 */
{
    HRSRC    hResInfo;
    DWORD    dwSize;
    HGLOBAL  hResData;
    LPVOID   lpBuffer;
    HANDLE   hFile;
    HANDLE   hTempFile;
    DWORD    dwWritten;
    CHAR     szPath[MAX_PATH];  /* local_104 */

    if (gVxdDevice != 0)
        return 1;

    gVxdDevice = (int)CreateFileA(
                            "\\\\.\\VFIXMEM.VXD" /* 0x0045bc58 */,
                            0, 0, NULL,
                            0,
                            FILE_FLAG_DELETE_ON_CLOSE,
                            NULL);
    if ((HANDLE)gVxdDevice != INVALID_HANDLE_VALUE)
        return 1;

    gVxdDevice = 0;

    hResInfo = FindResourceA(gInstance, "VFIXMEM" /* 0x0045bc50 */,
                             (LPCSTR)kStrRAW);
    if (hResInfo == NULL)
        return 0;

    dwSize   = SizeofResource(gInstance, hResInfo);
    hResData = LoadResource(gInstance, hResInfo);
    lpBuffer = LockResource(hResData);
    if (lpBuffer == NULL) {
        FreeResource(hResData);
        return 0;
    }

    GetWindowsDirectoryA(szPath, sizeof(szPath));
    if (szPath[strlen(szPath) - 1] != '\\')
        strcat(szPath, kStrBackslash);   /* append "\" if no trailing slash */
    strcat(szPath, kStrVfixmemVxd);       /* append "VFIXMEM.VXD" */

    (void)strlen(szPath);   /* computed and discarded in the exe too (0x0041d878 repne scasb) */
    hFile = CreateFileA(szPath,
                        GENERIC_WRITE, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        FreeResource(hResData);
        return 0;
    }

    WriteFile(hFile, lpBuffer, dwSize, &dwWritten, NULL);
    CloseHandle(hFile);
    FreeResource(hResData);

    hTempFile = CreateFileA(szPath,
                            GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_NORMAL,
                            NULL);

    if (dwWritten != dwSize)
        return 0;   /* write was truncated -- VxD on disk is corrupt */

    gVxdDevice = (int)CreateFileA(
                            "\\\\.\\VFIXMEM.VXD" /* 0x0045bc58 */,
                            0, 0, NULL,
                            0,
                            FILE_FLAG_DELETE_ON_CLOSE,
                            NULL);
    if ((HANDLE)gVxdDevice != INVALID_HANDLE_VALUE)
        return 1;

    /* VxD refused to load -- clean up temp file and fail */
    CloseHandle(hTempFile);   /* FILE_FLAG_DELETE_ON_CLOSE deletes FIXMEM.VXD */
    gVxdDevice = 0;
    return 0;
}


/* param_1 -- requested allocation size in bytes
 * Returns: pointer to the VxD-allocated block, or NULL on failure. */
void * __cdecl AllocVxDBlock(unsigned int cbBytes)  /* 0x0041d970 */
{
    BOOL         bOk;
    unsigned int nPages;    /* IOCTL 3 input: page count (ceiling of cbBytes/4096) */
    void        *pBlock;    /* IOCTL 3 output: pointer to allocated block */
    DWORD        dwReturned;

    OpenVxDDevice();

    nPages = cbBytes >> 12;
    if (cbBytes & 0xFFF)
        nPages++;

    bOk = DeviceIoControl(
            (HANDLE)gVxdDevice,
            3,              /* IOCTL: allocate N pages */
            &nPages, sizeof(nPages),
            &pBlock, sizeof(pBlock),
            &dwReturned,
            NULL);

    if (bOk == FALSE)
        return NULL;

    return pBlock;
}


/* OUTOFMEM dialog handler: centers on screen, updates memory stats, processes user input. */
INT_PTR CALLBACK OutOfMemoryDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)  /* 0x0041D9D0 */
{
    /* The exe returns 0 for WM_COMMAND and WM_TIMER alike (one shared `xor eax,eax`
     * epilogue); only WM_INITDIALOG returns 1. */
    size_t  cbProbe;
    void   *pProbe;
    RECT    screenRect;
    RECT    dlgRect;
    RECT    centerRect;
    char    szBuf[40];

    (void)lParam;

    switch (msg) {
    case WM_INITDIALOG:
        screenRect.top    = 0;
        screenRect.left   = 0;
        screenRect.right  = GetSystemMetrics(SM_CXSCREEN);
        screenRect.bottom = GetSystemMetrics(SM_CYSCREEN);
        GetWindowRect(hDlg, &dlgRect);
        CopyRect(&centerRect, &screenRect);
        OffsetRect(&dlgRect, -dlgRect.left, -dlgRect.top);
        OffsetRect(&centerRect, -centerRect.left, -centerRect.top);
        OffsetRect(&centerRect, -dlgRect.right, -dlgRect.bottom);
        SetWindowPos(hDlg, HWND_TOPMOST,
                     screenRect.left + centerRect.right / 2,
                     screenRect.top + centerRect.bottom / 2,
                     0, 0, SWP_NOSIZE);
        SetDlgItemTextA(hDlg, 200, GetStringResource(0x8a));
        GetDlgItemTextA(hDlg, 100, gOutOfMemProbeSizeFmt, 0x28);
        GetDlgItemTextA(hDlg, 0x65, gOutOfMemAvailPhysFmt, 0x28);
        GetDlgItemTextA(hDlg, 0x66, gOomPageFileFormat, 0x28);
        GetDlgItemTextA(hDlg, 0x67, gOutOfMemUsableFmt, 0x28);
        GetDlgItemTextA(hDlg, 0x68, gOomPoolMinFormat, 0x28);
        GetDlgItemTextA(hDlg, 0x69, gOutOfMemPoolSizeFmt, 0x28);
        SendMessageA(hDlg, WM_TIMER, 0, 0);
        return TRUE;

    case WM_COMMAND:
        switch (wParam) {
        case 1:
            SetCursor(LoadCursorA(NULL, IDC_WAIT));
            for (cbProbe = gTotalPhysRam; (int)cbProbe > 0; cbProbe -= 0x10000) {
                pProbe = malloc(cbProbe);
                if (pProbe != NULL) {
                    ZeroBlock(pProbe, cbProbe);
                    free(pProbe);
                    break;
                }
            }
            SetCursor(LoadCursorA(NULL, IDC_ARROW));
            if (AllocMemoryPool(1) != 0)
                EndDialog(hDlg, 1);
            break;
        case 2:
            EndDialog(hDlg, 0);
            break;
        }
        break;

    case WM_TIMER:
        AllocMemoryPool(0);
        sprintf(szBuf, gOutOfMemProbeSizeFmt, (int)gTotalPhysRam / 1024);
        SetDlgItemTextA(hDlg, 100, szBuf);
        sprintf(szBuf, gOutOfMemAvailPhysFmt, (int)gAvailPhysBytes / 1024);
        SetDlgItemTextA(hDlg, 0x65, szBuf);
        sprintf(szBuf, gOomPageFileFormat, (int)gAvailPageFileBytes / 1024);
        SetDlgItemTextA(hDlg, 0x66, szBuf);
        sprintf(szBuf, gOutOfMemUsableFmt, (int)gProbedUsableBytes / 1024);
        SetDlgItemTextA(hDlg, 0x67, szBuf);
        sprintf(szBuf, gOomPoolMinFormat, (int)gPoolMinBytes / 1024);
        SetDlgItemTextA(hDlg, 0x68, szBuf);
        sprintf(szBuf, gOutOfMemPoolSizeFmt, (int)gPoolSizeBytes / 1024);
        SetDlgItemTextA(hDlg, 0x69, szBuf);
        SetTimer(hDlg, 1, 0xfa, NULL);
        break;
    }

    return FALSE;
}


/* Tears the application down: marks it no longer running, deletes the logical HPALETTE, restores
 * the default object into the back-buffer DC and deletes it, unloads the graphics back end,
 * destroys the accelerator table and the raw ACCEL buffer and the main menu, unloads USER32
 * (FreeDisplayLibrary), unloads the custom palette in true-colour mode, runs the subsystem
 * cleanups, frees the game pool (gPoolBase: a VxD block when gIsWin95, the CRT heap when gIsWin31,
 * else the GlobalAlloc pool), destroys the cover window, posts WM_QUIT and drains the message queue
 * so pending WM_DESTROY / WM_NCDESTROY are dispatched cleanly, unregisters the DFCOVER and DFWINDOW
 * classes and removes the embedded font. */
void __stdcall Shutdown(void)  /* 0x0041dd30 */
{
    tagMSG msg;
    BOOL   bMsgPending;

    gGamePaused = 0;

    if (gPalette != NULL)
    {
        DeleteObject(gPalette);
    }

    if (gIsSceneBitmapSelected != 0)
    {
        SelectObject((HDC)gBackBufferDC, gBackBufferSavedObject);
    }
    DeleteDC((HDC)gBackBufferDC);

    FreeGraphicsLibrary();

    if (gAccel != 0)
    {
        DestroyAcceleratorTable((HACCEL)gAccel);
        LocalFree(gNavCommandTable);
    }

    if (gMainMenu != NULL)
    {
        DestroyMenu(gMainMenu);
    }

    /* reverts a changed display mode first: DF_FIX_REVERT_DISPLAY_MODE */
    FreeDisplayLibrary();

    if (gCustomPaletteFlag != 0)
    {
        ManageSystemPalette(0);
    }

    SubsystemCleanup();
    ShutdownHeap();

    if (gIsWin95 != 0)
    {
        FreeVxDBlock(gPoolBase);
    }
    else if (gIsWin31 != 0)
    {
        free(gPoolBase);
    }
    else
    {
        GlobalFree(gPoolBase);
    }

    if (IsWindow(gCoverWindow))
    {
        DestroyWindow(gCoverWindow);
    }

    PostQuitMessage(0);

    bMsgPending = GetMessageA(&msg, NULL, 0, 0);
    while (bMsgPending != 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        bMsgPending = GetMessageA(&msg, NULL, 0, 0);
    }

    UnregisterClassA("DFCOVER" /* 0x0045bac8 */,  gInstance);
    UnregisterClassA("DFWINDOW" /* 0x0045bae8 */, gInstance);

    ManageEmbeddedFont(0);
}


/* Unload the rendering backend DLL (GDI32 or WING32). */
void __stdcall FreeGraphicsLibrary(void)  /* 0x0041dea0 */
{
    FreeLibrary((HMODULE)gGraphicsLibrary);
}


/* Unloads USER32.DLL loaded by InitDisplayMode. The source reverts a changed display mode first; in
 * our TI.EXE that call is patched out (the jz at 0x0041dec1 made a jmp: DF_FIX_REVERT_DISPLAY_MODE
 * and match/gog_patches.py), in the March 1997 build it is live. */
void __stdcall FreeDisplayLibrary(void) /* 0x0041DEB0 */
{
    if (gUser32Lib != 0)
    {
#if DF_FIX_REVERT_DISPLAY_MODE
        if (gIsDisplayModeChanged != 0)
            RestoreDisplayMode();
#else
        /* the source; GOG's post-build patch skips the call */
        if (gIsDisplayModeChanged != 0)
            RestoreDisplayMode();
#endif
        FreeLibrary((HMODULE)gUser32Lib);
    }
}


/* Sends IOCTL 4 (free) to the VxD memory driver with the block pointer. A __cdecl with one
 * argument (0x0041dee0 ends in a plain ret): Ghidra shows it as __stdcall with none, reading
 * stack0x00000004. Shutdown pushes gPoolBase before the call. */
void __cdecl FreeVxDBlock(void *pBlock)  /* 0x0041dee0 -- ends in `ret`, not `ret 4`: cdecl */
{
    DWORD dwReturned;

    OpenVxDDevice();

    DeviceIoControl(
            (HANDLE)gVxdDevice,
            4,              /* IOCTL: free block */
            &pBlock, sizeof(pBlock),
            NULL, 0,
            &dwReturned,
            NULL);
}


/* Returns current time in 60 Hz ticks; pumps message queue as side effect */
unsigned int __stdcall GetTick60Hz(void)  /* 0x0041df10 */
{
    PumpMessages();                      /* drain message queue as side effect */
    return (timeGetTime() * 3) / 50;    /* ms -> 60 Hz ticks                   */
}


/* Busy-waits nTicks x 50/3 ms (~nTicks frames at 60 Hz) while pumping messages */
void __cdecl SleepTicks(int nTicks)  /* 0x0041df30 */
{
    DWORD dwStart;

    dwStart = timeGetTime();
    dwStart += (nTicks * 0x32) / 3;     /* the deadline lives in the same variable: that is what puts it in esi */
    while (timeGetTime() < dwStart)
        PumpMessages();
}


/* Applies hCursor as the active cursor for every registered render window.
 * Uses SetClassLongA(hWnd, GCL_HCURSOR) so the cursor is restored
 * automatically by the OS whenever the mouse re-enters the window.
 *
 * If the current mouse position is inside one of the registered windows,
 * calls SetCursor(hCursor) immediately so the change takes effect without
 * waiting for the next WM_SETCURSOR.
 *
 * Registered windows are tracked in gViewportTable[0..gViewportCount-1];
 * each entry is a pointer to a viewport struct whose first field is HWND. */
void __cdecl InstallCursor(HCURSOR hCursor)  /* 0x0041DF70 */
{
    POINT  ptClient;  /* top-left then bottom-right of client rect, in screen coords */
    HWND   hWndHit;   /* window under the cursor */
    int    i;
    bool   bInHit;    /* true if hWndHit is one of our windows */
    POINT  ptScreen;  /* current cursor position (screen coords) */
    RECT   rcClient;  /* client rect in window coords */

    GetCursorPos(&ptScreen);
    hWndHit = WindowFromPoint(ptScreen);
    bInHit = false;

    for (i = 0; i < gViewportCount; i++)
    {
        if (!DF_VIEWPORT_SLOT_OK(gViewportTable[i], sizeof(HWND)))
            continue;
        /* no HWND local: the exe reloads the slot after the call */
        SetClassLongA(*(HWND *)gViewportTable[i], GCL_HCURSOR, (LONG)hCursor);
        if (*(HWND *)gViewportTable[i] == hWndHit)
            bInHit = true;
    }

    if (bInHit)
    {
        GetClientRect(hWndHit, &rcClient);

        ptClient.x = rcClient.left;
        ptClient.y = rcClient.top;
        ClientToScreen(hWndHit, &ptClient);
        if (ptClient.x <= ptScreen.x && ptClient.y <= ptScreen.y)
        {
            ptClient.x = rcClient.right;
            ptClient.y = rcClient.bottom;
            ClientToScreen(hWndHit, &ptClient);
            if (ptScreen.x < ptClient.x && ptScreen.y < ptClient.y)
                SetCursor(hCursor);
        }
    }
}


/* Installs the standard arrow cursor and runs the present worker until
 * the signed present-state word becomes non-negative. */
void __stdcall QdInitCursor(void)  /* 0x0041e050 */
{
    HCURSOR hArrow;

    hArrow = LoadCursorA(gInstance, "CURS.ARROW" /* 0x0045badc */);
    DF_TRACE("QdInitCursor: LoadCursorA(\"%s\") -> %p",
             "CURS.ARROW" /* 0x0045badc */, (void *)hArrow);
    InstallCursor(hArrow);

    while (gCursorShowDepth < 0)
    {
        QdShowCursor();
    }
}


/* Single iteration of the present-worker loop called by QdInitCursor.
 * Shows the cursor (in case it was hidden during rendering), increments
 * the present-state counter, and drains the Windows message queue.
 *
 * QdInitCursor calls this in a tight loop while gCursorShowDepth < 0;
 * once PumpMessages dispatches a message that sets gCursorShowDepth >= 0,
 * the loop exits and QdInitCursor returns. */
void __stdcall QdShowCursor(void)  /* 0x0041e090 */
{
    ShowCursor(1);
    gCursorShowDepth++;
    PumpMessages();
}


/* Counterpart of QdShowCursor: hides the system cursor via
 * ShowCursor(0), decrements the application cursor depth counter
 * (gCursorShowDepth), then drains the Windows message queue.
 *
 * Called from opcode 12002 (ActionCmd_HideCursor in scriptcmd.c). */
void __stdcall QdHideCursor(void)  /* 0x0041e0b0 */
{
    ShowCursor(0);
    gCursorShowDepth--;
    PumpMessages();
}


/* Makes a channel's associated window visible.  The HWND is obtained by
 * double-dereferencing: the channel struct at offset +0x12 holds a
 * pointer to a viewport struct whose first field is the HWND.
 *
 * Called at the end of InitSceneNodeChannel to make channel B visible. */
void __cdecl ShowChannelWindow(int pChannel)  /* 0x0041e0d0 */
{
    ShowWindow((HWND)**(int **)(pChannel + 0x12), SW_SHOW);
}


/* Hide the Win32 window associated with a scene channel.
 * pChannel is the channel descriptor; pNodeObj = *(int*)(pChannel+0x12);
 * HWND is stored as the first DWORD of the node-object struct. */
void __cdecl HideChannelWindow(int pChannel)  /* 0x0041e0f0 */
{
    ShowWindow(*(HWND *)(*(int *)(pChannel + 0x12)), SW_HIDE);
}


/* Resize the Win32 window associated with a scene channel to the given
 * screen-space rect (top/left/bottom/right as shorts).
 * Skips the move if the window is currently maximised. */
void __cdecl ResizeChannelWindow(short *pRect, int pChannel)  /* 0x0041e110 */
{
    HWND  hWnd = (HWND)*(int *)(*(int *)(pChannel + 0x12));
    if (!IsZoomed(hWnd)) {
        DWORD dwStyle = GetWindowLongA(hWnd, GWL_STYLE);
        RECT  rc;
        rc.left   = pRect[1];
        rc.right  = pRect[3];
        rc.top    = pRect[0];
        rc.bottom = pRect[2];
        AdjustWindowRect(&rc, dwStyle, FALSE);
        MoveWindow(hWnd, rc.left, rc.top,
                   rc.right - rc.left, rc.bottom - rc.top, TRUE);
    }
}


/* Return the client-area rect of the channel's window in screen coordinates.
 * Output pOut[0..3] = { top, left, bottom, right } as shorts. */
void __cdecl GetChannelWindowRect(int pChannel, short *pOut)  /* 0x0041e190 */
{
    HWND  hWnd = (HWND)*(int *)(*(int *)(pChannel + 0x12));
    RECT  rc;
    POINT pt;
    GetClientRect(hWnd, &rc);
    pt.x = rc.left;  pt.y = rc.top;
    ClientToScreen(hWnd, &pt);
    rc.left = pt.x;  rc.top = pt.y;
    pt.x = rc.right; pt.y = rc.bottom;
    ClientToScreen(hWnd, &pt);
    pOut[0] = (short)rc.top;
    pOut[1] = (short)rc.left;
    pOut[2] = (short)pt.y;
    pOut[3] = (short)pt.x;
}


/* UNC \\ branch for ComposeVolumeDescriptorFromPath: packs the C string into a
 * Pascal string, turning every backslash into ':'. The port used to forward the
 * path through GetVolumeLabelForPath here; the exe does no lookup at all. */
void __cdecl PackUncPathAsPascal31(char *uncPath, char *pascalOut) /* 0x0041e220 */
{
    char c;

    *pascalOut++ = (char)strlen(uncPath);   /* the intrinsic: repne scasb */
    c = *uncPath++;
    while (c != '\0') {
        if (c == '\\')
            c = ':';
        *pascalOut++ = c;
        c = *uncPath++;
    }
}


/* Converts a Pascal-string resource descriptor into a null-terminated
 * Windows file path written to pszOut (256-byte buffer).
 *
 * The descriptor uses ':' as a path separator (engine-internal format).
 * The first component before the first ':' is a volume/disk label that
 * is resolved to a real Windows drive path via ResolveFilePathByVolume.
 *
 * Special case: if the Pascal body starts with "::" (absolute path
 * descriptor), delegates to ConvertColonToBackslash.
 *
 * Example:
 *   descriptor: "\x0FTITANIC:DATA:BOO" (body "TITANIC:DATA:BOO")
 *   -> translate ':' -> '\': "TITANIC\DATA\BOO"
 *   -> ResolveFilePathByVolume scans for CD with label "TITANIC" -> "D:\"
 *   -> result: "D:\DATA\BOO"
 *
 * Descriptor -> Windows path (volume label resolution) */
void __cdecl GetResourcePath(byte *pDescriptor, byte *pszOut)  /* 0x0041e260 */
{
    char  firstComponent[32];
    char  resolvedBase[256];
    byte *pSrc;
    byte *pDst;
    byte  len;
    byte  ch;
    byte  prefixLen;

    len  = pDescriptor[0];
    pSrc = pDescriptor + 1;
    pDst = pszOut;
    if (len > 2 && pSrc[0] == 0x3a && pSrc[1] == 0x3a) {
        ConvertColonToBackslash(pDescriptor, pszOut);
        return;
    }
    while (len-- != 0) {
        ch = *pSrc++;
        if (ch == 0x3a)
            ch = 0x5c;
        *pDst++ = ch;
    }
    *pDst = 0;
    prefixLen = 0;
    pSrc = pszOut + 1;
    ch = pszOut[0];
    if (ch != 0) {
        do {
            if (ch == 0x5c)
                break;
            prefixLen++;
            ch = *pSrc++;
        } while (ch != 0);
        if (ch != 0) {
            if (prefixLen > 31)
                prefixLen = 31;
            /* Volume name (first component, max 31 chars); the exe CALLs the CRT copy */
            memmove(firstComponent, pszOut, prefixLen);
            firstComponent[prefixLen] = 0;
            ResolveFilePathByVolume(firstComponent, resolvedBase, 1);
            strcat(resolvedBase, (char *)pSrc);
            strcpy((char *)pszOut, resolvedBase);
        }
    }
}


/* Resolves a volume / disk label (e.g. "TITANIC") to a Windows drive-root path. Checks the 4-slot
 * name -> path cache first (when flag != 0), then the "UntitledN" special case (the synthetic label
 * GetVolumeLabelForPath invents for an unlabelled drive: "Untitled" + the drive letter, 9
 * characters), then scans all local / removable / CD / network drives (GetDriveTypeA +
 * GetVolumeInformationA) for one whose label matches. Falls back to kStrEmpty ("") when nothing
 * matches: callers combine that with the rest of the descriptor's path, which then resolves
 * relative to the current directory instead of an absolute drive path. A real gap for volume names
 * that were never CD / disk labels, and faithful to the original engine.
 *
 * Port note: rewritten from Ghidra's pseudocode, which relied on gVolumeLabelCache /
 * gVolumePathCache sitting contiguously, 0x40 bytes per slot (true in the original layout, not
 * guaranteed for the recompiled globals), into two independent 4-slot arrays indexed directly; the
 * behaviour is unchanged. The UntitledN test is strlen == 9 (0x0041e3f0 tests the scasb counter
 * against -0xb); the port tested 10, so the branch never fired and every unlabelled-drive
 * descriptor fell through to the empty fallback, which is why the launcher needed cwd at the drive
 * root. */
void __cdecl ResolveFilePathByVolume(char *pVolumeName, char *pszOut, int flag)  /* 0x0041E380 */
{
    char *pEntry;
    char  label[128];
    char  driveList[2048];
    char *pDrive;
    int   i;
    UINT  driveType;
    char  saved;

    /* the cache walked by pointer to the next table's address (0x0041e3b4) */
    if (flag != 0) {
        i = 0;
        pEntry = gVolumeLabelCache;
        do {
            if (__strcmpi(pEntry, pVolumeName) == 0) {
                strcpy(pszOut, &gVolumePathCache[i * 0x40]);
                return;
            }
            pEntry += 0x40;
            i++;
        } while (pEntry < gVolumePathCache);
    }

    if (strlen(pVolumeName) == 9) {
        saved = pVolumeName[8];
        pVolumeName[8] = '\0';
        i = __strcmpi(pVolumeName, "Untitled" /* 0x0045bc70 */);
        pVolumeName[8] = saved;
        if (i == 0) {
            _sprintf(pszOut, kStrDriveRootFormat, (int)saved);
            return;
        }
    }

    EnumerateDriveLettersForMounted(sizeof(driveList), driveList);
    for (pDrive = driveList; strlen(pDrive) != 0; pDrive += strlen(pDrive) + 1) {
        driveType = GetDriveTypeA(pDrive);
        if ((driveType == DRIVE_FIXED || driveType == DRIVE_REMOTE ||
             driveType == DRIVE_CDROM || driveType == DRIVE_RAMDISK) &&
            GetVolumeInformationA(pDrive, label, sizeof(label), NULL, NULL, NULL, NULL, 0) &&
            __strcmpi(pVolumeName, label) == 0)
        {
            goto found;
        }
    }

    /* through a pointer: the exe copies the empty prefix with an inline
     * strlen, which VC++ 4.2 emits only for a non-literal source */
    pEntry = kStrEmpty;
    strcpy(pszOut, pEntry);
    return;

found:
    strcpy(pszOut, pDrive);
    if (flag != 0 && strlen(pVolumeName) < 0x40 && strlen(pszOut) < 0x40)
    {
        strcpy(&gVolumeLabelCache[gVolumeCacheNext * 0x40], pVolumeName);
        strcpy(&gVolumePathCache[gVolumeCacheNext * 0x40], pszOut);
        gVolumeCacheNext = (gVolumeCacheNext + 1) % 4;
    }
}


/* Enumerates available drive letters into output buffer for mounted drives. */
void __cdecl EnumerateDriveLettersForMounted(DWORD bufSize, char *pBuf)  /* 0x0041E600 */
{
  int   i;
  DWORD mask;
  char  root[4];

  *(int *)root = *(const int *)kStrDriveA;   /* "A:\" template, copied as one dword;
                                                   * the port had `local_4 = kStrDriveA`, the
                                                   * array's ADDRESS truncated to an int */
  if (gIsWin31 != 0) {
    mask = GetLogicalDrives();
    for (i = 0; i < 0x20; i++) {
      if (mask & 1) {
        strcpy(pBuf, root);
        pBuf += strlen(pBuf) + 1;
      }
      mask >>= 1;
      root[0]++;
    }
    pBuf[0] = '\0';
    pBuf[1] = '\0';
  } else {
    GetLogicalDriveStringsA(bufSize, pBuf);
  }
}


/* Copy Pascal string, converting colons to backslashes.
 * Used to convert Mac-style paths to DOS paths. */
void __cdecl ConvertColonToBackslash(byte *pPascalIn, byte *pOut)  /* 0x0041E6A0 */
{
  uint32_t n;
  byte c;

  n = *pPascalIn++;                       /* Pascal length, then the characters */
  while (n-- != 0) {
    c = *pPascalIn++;
    if (c == ':')
      c = '\\';                         /* Mac path separator -> Windows */
    *pOut++ = c;
  }
  *pOut = 0;
}


/* Converts a Pascal colon path to a C DOS path (':' -> '\') and expands
 * the first path component through ResolveFilePathByVolume.  A "::"
 * prefix delegates to ConvertColonToBackslash (relative path, no volume
 * resolution). */
void __cdecl ResolveColonPathToDosPath(byte *pDescriptor, byte *pszOut)  /* 0x0041E6E0 */
{
    char  resolvedBase[256];
    byte *pSrc;
    byte *pDst;
    byte  len;
    char  firstComponent[32];
    byte  ch;
    byte  prefixLen;

    len  = pDescriptor[0];
    pSrc = pDescriptor + 1;
    pDst = pszOut;
    if (len > 2 && pSrc[0] == 0x3a && pSrc[1] == 0x3a) {
        ConvertColonToBackslash(pDescriptor, pszOut);
        return;
    }
    while (len-- != 0) {
        ch = *pSrc++;
        if (ch == 0x3a)
            ch = 0x5c;
        *pDst++ = ch;
    }
    *pDst = 0;
    prefixLen = 0;
    pSrc = pszOut + 1;
    ch = pszOut[0];
    if (ch != 0) {
        do {
            if (ch == 0x5c)
                break;
            prefixLen++;
            ch = *pSrc++;
        } while (ch != 0);
        if (ch != 0) {
            if (prefixLen > 31)
                prefixLen = 31;
            /* Volume name (first component, max 31 chars); the exe CALLs the CRT copy */
            memmove(firstComponent, pszOut, prefixLen);
            firstComponent[prefixLen] = 0;
            ResolveFilePathByVolume(firstComponent, resolvedBase, 0);
            strcat(resolvedBase, (char *)pSrc);
            strcpy((char *)pszOut, resolvedBase);
        }
    }
}


/* Turns a Win32 directory path into the engine's Pascal "Volume:dir:dir:" descriptor: the first
 * component is replaced by the drive's volume label (GetVolumeLabelForPath), kStrColon (":") is
 * appended to it and every remaining backslash becomes ':'.
 *
 * Port note: the binary treats the working string as a C string and appends: 0x0041e87d (DEC EDI)
 * lands on the NUL before the copy, 0x0041e8ee writes the length byte to pOut, and 0x0041e841 keeps
 * the trailing backslash, giving "D:\" for GetVolumeInformationA, not the "D:" the port produced
 * (it had treated the string as Pascal and prepended kStrColon by memmove). */
void __cdecl ComposeVolumeDescriptorFromPath(char *pPath, char *pOut) /* 0x0041e800 */
{
    int   n;              /* frame layout from the exe: n, then two 32-byte buffers */
    /* ONE 64-byte frame region: the label (up to 127 bytes from
     * GetVolumeLabelForPath, 32 on NTFS, plus ':') can run into `first`,
     * which is already consumed by then -- exactly as in the exe. As two
     * separate locals that overrun could land anywhere (class 3). */
    char  bufs[64];
#define label bufs                /* volume label + ':' -- a C string, not Pascal */
#define first (bufs + 32)         /* first path component, e.g. "D:\" */
    char *d;
    char  c;
    char *tail;

    d = first;
    if (pPath[0] == '\\' && pPath[1] == '\\') {
        PackUncPathAsPascal31(pPath, pOut);
        return;
    }

    for (;;) {
        c = *pPath++;
        *d++ = c;
        if (c == '\0')
            goto no_label;
        if (c == '\\')
            break;
    }
    *d = '\0';                                        /* keep the separator */
    if (c != '\0') {
        GetVolumeLabelForPath(first, label);
        strcat(label, kStrColon);                  /* append ":" (inlined strcat) */
    } else
no_label:
        /* No separator anywhere in the path: 0x0041e88e emits an empty
         * first component and performs no lookup at all (the exe's NUL exit
         * jumps straight here, past the terminator store). */
        label[0] = '\0';

    n = (unsigned char)strlen(label);                 /* xor eax,eax / mov al,cl */
    memmove(pOut + 1, label, n + 1);
    tail = pOut + 1 + n;

    if (c != '\0') {
        do {
            c = *pPath++;
            if (c == '\\')
                c = ':';
            *tail++ = c;
        } while (c != '\0');
    }
    pOut[0] = (char)strlen(pOut + 1);
#undef label
#undef first
}


/* Maps a drive root path ("D:\") to the volume LABEL the engine uses as the first component of its
 * Mac-style "Volume:dir:dir:" descriptors, so ResolveFilePathByVolume can map it back to a drive
 * later. Unlabelled drives get the synthetic label "Untitled<driveletter>", which that resolver
 * special-cases. Results are memoised in a 4-entry round-robin cache shared with
 * ResolveFilePathByVolume (gVolumeLabelCache the labels, gVolumePathCache the paths,
 * gVolumeCacheNext the next slot): this function searches paths -> labels, the resolver labels ->
 * paths.
 *
 * Port note: it was a fabricated PackAscii31AsPascal (no lookup, it returned the raw "D:"), so the
 * resolver used its fallback; that is why the cwd-at-drive-root launch workaround existed. */
void __cdecl GetVolumeLabelForPath(char *drivePath, char *labelOut) /* 0x0041e900 */
{
    char  label[128];
    int   i;
    char *p;

    /* the exe walks the path table by pointer to its end address, with no
     * entry test, and copies with the inlined strcpy/strlen (repne scasb /
     * rep movsd), not the lstr* APIs */
    i = 0;
    p = gVolumePathCache;
    do {
        if (__strcmpi(p, drivePath) == 0) {
            strcpy(labelOut, &gVolumeLabelCache[i * 0x40]);
            return;
        }
        p += 0x40;
        i++;
    } while (p < gVolumePathCache + 0x100);

    if (GetVolumeInformationA(drivePath, label, sizeof(label),
                              NULL, NULL, NULL, NULL, 0) && strlen(label) != 0) {
        strcpy(labelOut, label);
        /* Ground truth bails out before caching when either string would
         * overflow a 0x40-byte slot, but keeps the result it just wrote. */
        if (strlen(labelOut) >= 0x40 || strlen(drivePath) >= 0x40)
            return;
        strcpy(&gVolumeLabelCache[gVolumeCacheNext * 0x40], labelOut);
        strcpy(&gVolumePathCache[gVolumeCacheNext * 0x40], drivePath);
        gVolumeCacheNext = (gVolumeCacheNext + 1) % 4;
        return;
    }

    _sprintf(labelOut, "Untitled%c" /* 0x0045bc84 */, (int)drivePath[0]);
}


/* Drains message queue (throttled ~15 Hz), optionally flushes hardware input */
void __stdcall PumpMessages(void)  /* 0x0041eaa0 */
{
    DWORD  dwNow;
    BOOL   bGot;
    tagMSG msg;

    dwNow = timeGetTime();
    if (gLastPollTime + 0x42 > dwNow)   /* 0x42 = 66 ms throttle (~15 Hz) */
        return;

    gLastPollTime = dwNow;

    if (gSoundOn != 0)
        ServiceSoundMixer();

    if ((gGamePaused == 0) || (gMsgPumpDepth != 0))
    {
        /* Active: non-blocking PeekMessageA drain */
        gMsgPumpDepth++;
        while (PeekMessageA(&msg, (HWND)0, 0, 0, PM_REMOVE) != 0)
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (gSoundOn != 0)
                ServiceSoundMixer();
        }
        gMsgPumpDepth--;
    }
    else
    {
        /* Paused: lower CPU priority and block until focus returns */
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);   /* 0x20 */
        gMsgPumpDepth++;
        while (gGamePaused != 0)
        {
            bGot = GetMessageA(&msg, (HWND)0, 0, 0);
            if (bGot != 0)
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }
        gMsgPumpDepth--;
        /* Restore high-priority for active gameplay */
#if DF_FIX_ABOVE_NORMAL_PRIORITY
        SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
#else
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);     /* 0x80 */
#endif
        if (gSoundOn != 0)
            ServiceSoundMixer();
    }
}


/* Returns total physical RAM available on the system. */
SIZE_T __stdcall QueryTotalSystemMemory(void)  /* 0x0041EBE0 */
{
  MEMORYSTATUS memStatus;  /* local_20 */

  memStatus.dwLength = 0x20;
  GlobalMemoryStatus(&memStatus);
  return memStatus.dwTotalPhys;
}


/* PumpMessages + returns INT_MAX; used by script evaluators as unlimited recursion budget */
int __stdcall GetCallBudget(void)  /* 0x0041ec00 */
{
    PumpMessages();
    return 0x7fffffff;   /* INT_MAX -- unlimited call budget */
}



/* Switch on errCode (101-111, 999, default) -> string resource -> MB_ICONSTOP dialog -> ExitProcess(0) */
void __cdecl FatalError(short errCode, short errSub)  /* 0x0041ec10 */
{
    DWORD   lastErr;
    char    fmtBuf[256];
    char    tmpBuf[256];
    char    textBuf[256];
    char    wrapBuf[256];

    lastErr = GetLastError() & 0xFFFF;   /* the exe masks at the call (`and eax,0xffff`) */

    DF_TRACE("!!!! FatalError(errCode=%d/0x%x, errSub=%d/0x%x) GetLastError=%lu",
             (int)errCode, (int)(unsigned short)errCode,
             (int)errSub, (int)(unsigned short)errSub, (unsigned long)lastErr);

    switch ((int)errCode)
    {
    case 0x65: strcpy(fmtBuf, GetStringResource(0xA8)); break;
    case 0x66: strcpy(fmtBuf, GetStringResource(0xA9)); break;
    case 0x67: strcpy(fmtBuf, GetStringResource(0xAF)); break;
    case 0x68: strcpy(fmtBuf, GetStringResource(0xB0)); break;
    case 0x69: strcpy(fmtBuf, GetStringResource(0xAC)); break;
    case 0x6A: strcpy(fmtBuf, GetStringResource(0xAA)); break;
    case 0x6B: strcpy(fmtBuf, GetStringResource(0xAB)); break;
    case 0x6C: strcpy(fmtBuf, GetStringResource(0xAD)); break;
    case 0x6D: strcpy(fmtBuf, GetStringResource(0xB1)); break;
    case 0x6E: strcpy(fmtBuf, GetStringResource(0xAE)); break;
    case 0x6F: strcpy(fmtBuf, GetStringResource(0xB2)); break;

    case 999:
        /* Parametric: load a format string, then resolve a second string
         * by ID (errSub if >= 200, else fallback to 0x104 = 260) */
        strcpy(tmpBuf, GetStringResource(0xB6));
        if (errSub >= 200)
            sprintf(fmtBuf, tmpBuf, GetStringResource(errSub));
        else
            sprintf(fmtBuf, tmpBuf, GetStringResource(0x104));
        break;

    default:
        strcpy(fmtBuf, GetStringResource(0xB3));
        break;
    }

    RestoreDisplayMode();

    if (gCustomPaletteFlag != 0)
        ManageSystemPalette(0);

    sprintf(textBuf, fmtBuf, (int)errSub, lastErr);

    strcat(textBuf, GetStringResource(0xB4));

    WordWrapText(textBuf, wrapBuf);

    MessageBoxA(NULL, wrapBuf, GetStringResource(0xB5),
                0x2010);   /* MB_OK | MB_ICONSTOP | MB_TASKMODAL */

    DF_COVERAGE_FLUSH();   /* trace build: write df_coverage.txt before the process dies */
    ExitProcess(0);
}


/* Pumps the message queue, then returns TRUE if the left mouse button is
 * currently held down (GetAsyncKeyState bit 15 set).
 * Called by ScriptCmd_Button (opcode 20005) in scriptcmd.c. */
short __stdcall IsMouseDown(void)  /* 0x0041f020 */
{
    SHORT state;                        /* a short flag set after the call: see IsEscapeKeyDown */
    short down;
    PumpMessages();
    state = GetAsyncKeyState(VK_LBUTTON);
    down  = 1;
    if (state >= 0)
        down = 0;
    return down;
}


/* Polls PeekEventQueue and the mouse-button state and returns a packed 32-bit value: the high word
 * is the position or state from the poll, the low word is 1 if any mouse button is pressed, else 0.
 * If PeekEventQueue returns non-zero (no cursor data yet) only the high word is returned. Called
 * by ScriptCmd_StillDown (opcode 20007). */
short __stdcall IsMouseStillDown(void)  /* 0x0041f040 */
{
    uint result;
    unsigned char eventBuf[20];  /* local_14 */

    result = PeekEventQueue(4, (uint)eventBuf);
    if ((short)result != 0)
        return 0;

    if (GetAsyncKeyState(VK_LBUTTON) < 0 || GetAsyncKeyState(VK_RBUTTON) < 0)
        return 1;
    return 0;
}


#if DF_FIX_STRETCH_DISPLAY
/* Where the scene sits in the main window when it is stretched to fill the client (DF_FIX_STRETCH_DISPLAY).
 * on = 0 unless the window is the main game window and its client is larger than the viewport in both
 * directions; then the viewport (gSceneViewport w x h) is centred at (left, top) in the client and
 * scaled by cw / vw and ch / vh. */
typedef struct {
    int on;
    int left, top;
    int cw, ch;
    int vw, vh;
} DisplayStretch;

static void GetDisplayStretch(HWND hwnd, DisplayStretch *s)  /* PORT-HELPER: DF_FIX_STRETCH_DISPLAY */
{
    RECT rc;

    s->on = 0;
    if (hwnd == NULL || (int)(intptr_t)hwnd != gMainWindow || !GetClientRect(hwnd, &rc))
        return;
    s->cw = rc.right;
    s->ch = rc.bottom;
    s->vw = gSceneViewport.w;
    s->vh = gSceneViewport.h;
    if (s->vw <= 0 || s->vh <= 0 || s->cw <= s->vw || s->ch <= s->vh)
        return;
    s->left = (s->cw - s->vw) / 2;
    s->top  = (s->ch - s->vh) / 2;
    s->on   = 1;
}

/* The display blit, stretched: returns 0 and does nothing when GetDisplayStretch says no stretch, so the
 * caller falls through to its own BitBlt. Destination coordinates are window coordinates, as for BitBlt. */
static int DisplayStretchBlt(HDC hdcDest, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1)  /* PORT-HELPER: DF_FIX_STRETCH_DISPLAY */
{
    DisplayStretch s;
    int X0, Y0, X1, Y1;

    GetDisplayStretch(WindowFromDC(hdcDest), &s);
    if (!s.on)
        return 0;
    X0 = (x - s.left) * s.cw / s.vw;
    Y0 = (y - s.top) * s.ch / s.vh;
    X1 = (x + cx - s.left) * s.cw / s.vw;
    Y1 = (y + cy - s.top) * s.ch / s.vh;
    SetStretchBltMode(hdcDest, COLORONCOLOR);
    StretchBlt(hdcDest, X0, Y0, X1 - X0, Y1 - Y0, hdcSrc, x1, y1, cx, cy, SRCCOPY);
    return 1;
}
#endif


/* Translates the current Windows cursor position from screen space into
 * scene space, then writes the result into pOut as {pOut[0]=y, pOut[1]=x}.
 *
 * Scene object layout (the 'undefined4' array referenced via snapshot or
 * override ref; offsets in sizeof(unsigned int) = 4 bytes):
 *   [0]      -- HWND of the scene's render window
 *   [0x16]   -- scene viewport X origin (pixel offset added to client X)
 *   [0x17]   -- scene viewport Y origin (pixel offset added to client Y)
 *
 * param_1 (sceneOverride):
 *   0          -- use gThePortDesc[0] as the scene object pointer
 *   non-zero   -- dereference *(unsigned int**)(sceneOverride + 0x12) as
 *               the scene object pointer (offset +18 = alternative ref)
 *
 * Fatal error 0x6e / 0x53f if the resolved scene object pointer is NULL. */
void __cdecl GetSceneCursorPos(int sceneOverride, short *pOut)  /* 0x0041f090 */
{
    unsigned int *pScene;
    POINT         ptScreen;

    PumpMessages();

    if (sceneOverride != 0)
        pScene = *(unsigned int **)(sceneOverride + 0x12);
    else
        pScene = (unsigned int *)gThePortDesc[0];

    if (pScene == NULL)
        FatalError(0x6e, 0x53f);   /* no return */

    GetCursorPos(&ptScreen);
    ScreenToClient((HWND)pScene[0], &ptScreen);
#if DF_FIX_STRETCH_DISPLAY
    {
        /* Same undo as MapScreenPointToNode: the cursor is in the stretched display's client. */
        DisplayStretch s;

        GetDisplayStretch((HWND)pScene[0], &s);
        if (s.on) {
            ptScreen.x = s.left + ptScreen.x * s.vw / s.cw;
            ptScreen.y = s.top + ptScreen.y * s.vh / s.ch;
        }
    }
#endif

    pOut[1] = (short)pScene[0x16] + (short)ptScreen.x;
    pOut[0] = (short)pScene[0x17] + (short)ptScreen.y;
}


/* Pumps the Windows message queue, polls the joysticks (PollJoystickInput) and dequeues the next
 * game event matching typeMask. Returns non-zero if an event was dequeued (pEventOut filled), 0 if
 * the queue is empty. typeMask is a short: RunGameLoop passes all bits as -1 (`push -1`,
 * 0x004317b1). */
short __cdecl PollEventQueue(short typeMask, unsigned int *pEventOut)  /* 0x0041f100 */
{
    PumpMessages();
    PollJoystickInput();
    return DequeueGameEvent(typeMask, pEventOut);
}


/* Pumps the message queue, polls the joysticks and then scans the event queue with FindQueueEntry
 * for an event of the given type bitmask (e.g. 4 = cursor), writing it to the caller's 18-byte
 * buffer. Returns FindQueueEntry's result: 1 if one was found, 0 if not. */
short __cdecl PeekEventQueue(ushort eventMask, uint pEventOut)  /* 0x0041f120 */
{
    PumpMessages();
    PollJoystickInput();
    return FindQueueEntry(eventMask, (uint *)pEventOut);   /* no movsx: the exe returns ax as-is */
}


/* Pumps the message queue, then scans the event queue for a pending display-change event (type
 * 0x2a, queue mask 0x100; not a cursor click). Clears the event queue count (gEventQueueCount = 0)
 * and, if one was pending, re-enqueues it so the mode change survives the flush. Called by
 * ActionCmd_FlushEvents (opcode 12073). */
void __stdcall FlushEventQueue(void)  /* 0x0041f140 */
{
    uint         found;
    unsigned int eventBuf[5];   /* 18-byte event output buffer (FindQueueEntry) */  /* local_14 */

    PumpMessages();
    found = (uint)(short)FindQueueEntry(0x100, eventBuf);
    gEventQueueCount = 0;
    if (found != 0)
        /* 0x0041f16b..0x0041f17a: PUSH 0,0,0,0x2a,1 / ADD ESP,0x14, five arguments (flush=1,
         * eventType=0x2a, buttonId=0, payload=NULL, extra=0); the found flag is only the branch
         * test and is NOT passed. Every call site of 0x0041a450 pushes five and cleans 0x14 (our
         * declaration carries Ghidra's spurious leading param_1, which the body never reads).
         * The port passed `found` as a real argument and shifted the rest by one: flush=0 (the
         * pending event was never dequeued) and eventType=1 (queue mask 0x2), so it queued a
         * bogus type-1 event instead of re-queueing the display change (class 2). */
        EnqueueInputEvent(1, 0x2a, 0, NULL, 0);
}


/* Initializes hotspot buffer's bitmap sub-struct (27 DWORDs at pFields+0x0C).
 * Notable defaults:
 *   [0..4]  = 0 (GDI/bitmap pointers)
 *   [6]     = 0xFFFFFF (white RGB)
 *   [7]     = 0xFF (alpha/mask)
 *   [10]    = 1 (enabled)
 *   [0x0F]  = 10 (pen width)
 *   [0x12-13] = 1 (render modes)
 *   [0x16-19] = -32000/32000 (clip bounds)
 * Counterpart: CleanupHotspotBitmaps. */
void __cdecl InitHotspotBitmapState(int *pFields)  /* 0x0041f190 */
{
    int *p;   /* the exe forms pFields + 8 (lea eax,[ecx+0x20]) for the +0x20 pair */

    pFields[0x00] = 0;
    pFields[0x01] = 0;
    pFields[0x02] = 0;
    pFields[0x03] = 0;
    pFields[0x04] = 0;
    pFields[0x06] = 0xFFFFFF;     /* foreground colour (white) */
    pFields[0x07] = 0xFF;         /* alpha / mask */
    pFields[0x05] = 0;
    p = pFields + 8;
    p[0] = 0;
    p[1] = 0;
    pFields[0x0a] = 1;            /* enabled */
    pFields[0x0b] = 0;
    pFields[0x0c] = 0;
    pFields[0x0d] = 0;
    pFields[0x0e] = 0;
    pFields[0x10] = 0;
    pFields[0x11] = 0;
    pFields[0x12] = 1;            /* render mode A */
    pFields[0x13] = 1;            /* render mode B */
    pFields[0x15] = 0;
    pFields[0x14] = 0;
    pFields[0x16] = (int)0xFFFF8300;  /* clip min X = -32000 */
    pFields[0x17] = (int)0xFFFF8300;  /* clip min Y = -32000 */
    pFields[0x1a] = 0;
    pFields[0x18] = 32000;        /* clip max X */
    pFields[0x19] = 32000;        /* clip max Y */
    /* A dead store the compiler drops: it raises the weight of the constant 0 above
     * pFields's, which is what puts pFields in ecx and 0 in edx as in the exe. */
    pFields[0x0f] = 0;
    pFields[0x0f] = 10;           /* line width / pen size */
}


/* Deletes up to 4 GDI objects from hotspot buffer sub-struct.
 * Called by DestroyHotspotBuffer with (pBuf + 0x0c).
 * Counterpart of InitHotspotBitmapState.
 * GDI objects: +0x04, +0x08, +0x0C, +0x40. */
void __cdecl CleanupHotspotBitmaps(int pBufInternal)  /* 0x0041f210 */
{
    if (*(HGDIOBJ *)(pBufInternal + 0x04) != NULL)
        DeleteObject(*(HGDIOBJ *)(pBufInternal + 0x04));

    if (*(HGDIOBJ *)(pBufInternal + 0x08) != NULL)
        DeleteObject(*(HGDIOBJ *)(pBufInternal + 0x08));

    if (*(HGDIOBJ *)(pBufInternal + 0x0c) != NULL)
        DeleteObject(*(HGDIOBJ *)(pBufInternal + 0x0c));

    if (*(HGDIOBJ *)(pBufInternal + 0x40) != NULL)
        DeleteObject(*(HGDIOBJ *)(pBufInternal + 0x40));
}


/* Applies a hotspot bitmap state block (pState) to the DC: the text and background colours (the
 * back-buffer DC takes the COLORREFs directly, any other DC the palette-index form | 0x1000000),
 * the raster op (R2_COPYPEN or R2_XORPEN by mode), the pen, brush and font (falling back to the
 * black pen, black brush and system font), the text alignment and the palette; then stores the
 * HDC in the block, marks it dirty and applies the pending clip (QdApplyPendingClip). */
void __cdecl ApplyHotspotBitmapGdiState(HDC hdc, void *pState) /* 0x0041f250 */
{
    unsigned *pPort = (unsigned *)pState;
    HGDIOBJ   hObj;
    int       rop2;

    if (gBackBufferDC == hdc) {
        SetTextColor(hdc, (COLORREF)pPort[4]);
        SetBkColor(hdc, (COLORREF)pPort[6]);
    } else {
        SetTextColor(hdc, (COLORREF)(*(unsigned short *)(pPort + 5) | 0x1000000u));
        SetBkColor(hdc, (COLORREF)(*(unsigned short *)(pPort + 7) | 0x1000000u));
    }
    switch (pPort[0xb]) {
    case 0:
        rop2 = 0xd;
        SetROP2(hdc, rop2);
        break;
    case 1:
        rop2 = 7;
        SetROP2(hdc, rop2);
        break;
    }
    hObj = (HGDIOBJ)(UINT_PTR)pPort[1];
    if (hObj == (HGDIOBJ)NULL)
        hObj = (HGDIOBJ)(UINT_PTR)gBlackPen;
    SelectObject(hdc, hObj);
    hObj = (HGDIOBJ)(UINT_PTR)pPort[2];
    if (hObj == (HGDIOBJ)NULL)
        hObj = (HGDIOBJ)(UINT_PTR)gBlackBrush;
    SelectObject(hdc, hObj);
    hObj = (HGDIOBJ)(UINT_PTR)pPort[0x10];
    if (hObj == (HGDIOBJ)NULL)
        hObj = (HGDIOBJ)(UINT_PTR)gSystemFont;
    SelectObject(hdc, hObj);
    SetTextAlign(hdc, 0x19);
    SelectPalette(hdc, gPalette, FALSE);
    RealizePalette(hdc);
    pPort[0]   = (unsigned)(UINT_PTR)hdc;
    pPort[0x1a] = 1;
    QdApplyPendingClip(pState);
}


/* Selects the offscreen bitmap of a port state block into the back-buffer DC (remembering the
 * previous object in gBackBufferSavedObject), applies the block's GDI state to it and returns the
 * state at pStateData + 3. */
void * __cdecl QdSetPortOffscreen(int *pStateData)  /* 0x0041f330 */
{
    void *pRet = (void *)(pStateData + 3);   /* lea esi,[eax+0xc] before the SelectObject arguments */

    gBackBufferSavedObject = SelectObject(gBackBufferDC, *(HGDIOBJ *)(pStateData + 2));
    gIsSceneBitmapSelected = 1;
    ApplyHotspotBitmapGdiState(gBackBufferDC, pRet);
    return pRet;
}


/* ApplyHotspotBitmapGdiState on pNode+4 HDC and pNode+8 state; returns pNode+8 */
void * __cdecl QdSetPortWindow(int hNode)  /* 0x0041f370 */
{
    int *p = (int *)hNode;
    ApplyHotspotBitmapGdiState(*(HDC *)(p + 1), (void *)(p + 2));
    return (void *)(p + 2);
}


/* Restores prior SelectObject targets when releasing retained scene GDI state */
void __cdecl QdReleasePort(void *pRef)  /* 0x0041f390 */
{
    unsigned *pPort = (unsigned *)pRef;

    if ((HDC)gBackBufferDC == (HDC)pPort[0]) {
        SelectObject((HDC)pPort[0], gBackBufferSavedObject);
        gIsSceneBitmapSelected = 0;
    }
    if (pPort[1] != 0)
        SelectObject((HDC)pPort[0], (HGDIOBJ)(UINT_PTR)gBlackPen);
    if (pPort[2] != 0)
        SelectObject((HDC)pPort[0], (HGDIOBJ)(UINT_PTR)gBlackBrush);
    if (pPort[0x10] != 0)
        SelectObject((HDC)pPort[0], (HGDIOBJ)(UINT_PTR)gSystemFont);
}


/* Applies the pen, brush, text colours and pattern brush to the scene-node HDC when the vector-dirty
 * flag (pBitmapState[0x11]) is set. */
void __cdecl PrepareSceneNodeHdcForVectorDraw(int *pPort) /* 0x0041f400 */
{
    HPEN     hPen;
    int      i;
    HBITMAP  hbm;
    HBRUSH   patBrush;
    HBRUSH   solidBrush;
    COLORREF clr;
    unsigned char pat8[16];

    if (pPort[0x11] == 0)
        return;

    /* one CreatePen per case; the exe tail-merges their calls */
    if (pPort[0xb] == 1)
        hPen = CreatePen(PS_SOLID, pPort[10], 0xffffff);
    else if (pPort[4] == 0)
        hPen = (HPEN)NULL;
    else if ((HDC)(UINT_PTR)*pPort == gBackBufferDC)
        hPen = CreatePen(PS_SOLID, pPort[10], (COLORREF)pPort[4]);
    else
        hPen = CreatePen(PS_SOLID, pPort[10],
                         (COLORREF)(*(unsigned short *)(pPort + 5) | 0x1000000u));

    if (pPort[4] == 0 && pPort[0x13] != 0) {
        patBrush = (HBRUSH)NULL;
    } else {
        /* indexed: the compiler strength-reduces it to the pointer walk the
         * exe has, with the bound re-formed each pass */
        for (i = 0; i < 8; i++) {
            pat8[i * 2] = ((unsigned char *)pPort)[0x20 + i];
            pat8[i * 2 + 1] = 0;
        }
        hbm = CreateBitmap(8, 8, 1, 1, pat8);
        if (hbm == (HBITMAP)NULL)
            FatalError(0, 0x540);
        patBrush = CreatePatternBrush(hbm);
        DeleteObject(hbm);
    }

    clr = (COLORREF)pPort[6];
    if (clr == 0xffffff) {
        solidBrush = (HBRUSH)NULL;
    } else {
        if ((HDC)(UINT_PTR)*pPort != gBackBufferDC)
            clr = (COLORREF)(*(unsigned short *)(pPort + 7) | 0x1000000u);
        solidBrush = CreateSolidBrush(clr);
    }

    if (gBackBufferDC == (HDC)(UINT_PTR)*pPort) {
        SetTextColor((HDC)(UINT_PTR)*pPort, (COLORREF)pPort[4]);
        SetBkColor((HDC)(UINT_PTR)*pPort, (COLORREF)pPort[6]);
    } else {
        SetTextColor((HDC)(UINT_PTR)*pPort, (COLORREF)(*(unsigned short *)(pPort + 5) | 0x1000000u));
        SetBkColor((HDC)(UINT_PTR)*pPort, (COLORREF)(*(unsigned short *)(pPort + 7) | 0x1000000u));
    }
    switch (pPort[0xb]) {
    case 0:
        SetROP2((HDC)(UINT_PTR)*pPort, 0xd); /* R2_COPYPEN */
        break;
    case 1:
        SetROP2((HDC)(UINT_PTR)*pPort, 7);
        break;
    }

    if (pPort[1] != 0)
        SelectObject((HDC)(UINT_PTR)*pPort, (HGDIOBJ)(UINT_PTR)gBlackPen);
    if (pPort[2] != 0)
        SelectObject((HDC)(UINT_PTR)*pPort, (HGDIOBJ)(UINT_PTR)gBlackBrush);

    if ((HGDIOBJ)(UINT_PTR)pPort[1] != NULL)
        DeleteObject((HGDIOBJ)(UINT_PTR)pPort[1]);
    if ((HGDIOBJ)(UINT_PTR)pPort[2] != NULL)
        DeleteObject((HGDIOBJ)(UINT_PTR)pPort[2]);
    if ((HGDIOBJ)(UINT_PTR)pPort[3] != NULL)
        DeleteObject((HGDIOBJ)(UINT_PTR)pPort[3]);

    pPort[1] = (int)(UINT_PTR)hPen;
    pPort[2] = (int)(UINT_PTR)patBrush;
    pPort[3] = (int)(UINT_PTR)solidBrush;

    if (hPen != (HPEN)NULL)
        SelectObject((HDC)(UINT_PTR)*pPort, hPen);
    else
        SelectObject((HDC)(UINT_PTR)*pPort, (HGDIOBJ)(UINT_PTR)gBlackPen);
    if (pPort[2] != 0)
        SelectObject((HDC)(UINT_PTR)*pPort, (HGDIOBJ)(UINT_PTR)pPort[2]);
    else
        SelectObject((HDC)(UINT_PTR)*pPort, (HGDIOBJ)(UINT_PTR)gBlackBrush);

    pPort[0x11] = 0;
}


/* Recreates the HFONT and selects it into the node HDC when pBitmapState[0x12] is set. The face is
 * Arial, or Raven Digital for mode 0x10 (string literals at 0x0045bc90 / 0x0045bc98). */
void __cdecl RefreshHotspotBitmapFontIfNeeded(unsigned *pBitmapState) /* 0x0041f600 */
{
    HFONT       hFont;

    if (pBitmapState[0x12] == 0)
        return;
    /* two whole calls, not a ternary face argument: VC++ 4.2 merges their common
     * tail, leaving one push per branch (a ternary compiles to mov eax + push).
     * A switch with the 0x10 case first: the same code as `if (!= 0x10)`, but the
     * compiler creates "Arial" before "Raven Digital", and each literal is its own
     * COMDAT laid out in creation order -- TI.EXE has Arial first. */
    switch (pBitmapState[0xe]) {
    case 0x10:
        hFont = CreateFontA(-(int)pBitmapState[0xf], 0, 0, 0, FW_NORMAL, 0, 0, 0, 1, 6, 1, 0, 0, "Raven Digital");
        break;
    default:
        hFont = CreateFontA(-(int)pBitmapState[0xf], 0, 0, 0, FW_NORMAL, 0, 0, 0, 1, 6, 1, 0, 0, "Arial");
    }
    /* the HDC (pBitmapState[0]) is re-read for every call */
    if (pBitmapState[0x10] != 0)
        SelectObject((HDC)(UINT_PTR)pBitmapState[0], (HGDIOBJ)(UINT_PTR)gSystemFont);
    if ((HGDIOBJ)(UINT_PTR)pBitmapState[0x10] != NULL)
        DeleteObject((HGDIOBJ)(UINT_PTR)pBitmapState[0x10]);
    pBitmapState[0x10] = (unsigned)(UINT_PTR)hFont;
    if (hFont != (HFONT)NULL) {
        SelectObject((HDC)(UINT_PTR)pBitmapState[0], hFont);
        pBitmapState[0x12] = 0;
        return;
    }
    SelectObject((HDC)(UINT_PTR)pBitmapState[0], (HGDIOBJ)(UINT_PTR)gSystemFont);
    pBitmapState[0x12] = 0;
}


/* Clears the deferred clip region on a display node DC (+0x68): applies
 * CreateRectRgn clip from +0x58..+0x64 offsets and clears +0x68 flag. */
void __cdecl QdApplyPendingClip(void *pNode)  /* 0x0041F6B0 */
{
  HRGN hrgn;
  int *piNode = (int *)pNode;
  
  if (piNode[0x1a] != 0) {
    hrgn = CreateRectRgn(piNode[0x16] - piNode[0x14],piNode[0x17] - piNode[0x15],
                         piNode[0x18] - piNode[0x14],piNode[0x19] - piNode[0x15]);
    SelectClipRgn((HDC)(UINT_PTR)piNode[0],hrgn);
    DeleteObject(hrgn);
    piNode[0x1a] = 0;
  }
  return;
}


/* Initialises the HotspotBuf for scene dimensions: width and height default to 4 and 1 when zero,
 * the width is aligned up to a 4-byte DWORD boundary, and the dimensions go to
 * gBitmapHeader.biWidth (width) and biHeight (height, negated for a top-down DIB). Writes {h, w}
 * to the start of the buffer, allocates the pixel buffer with AllocateHotspotPixelBuffer (which
 * returns through EDX:EAX: EAX = handle at +8, EDX = pixels at +4) and initialises the bitmap
 * state at +12. Called by BeginGameSession. */
void __cdecl InitHotspotBuffer(short *pBuf, unsigned short w, short h)  /* 0x0041f700 */
{
    void *pBits;

    if (w == 0)
        w = 4;
    if (h == 0)
        h = 1;
    if (w & 3)                              /* DWORD row stride */
        w = w - (w & 3) + 4;
    gBitmapHeader.biWidth = (short)w;                /* biWidth */
    gBitmapHeader.biHeight = -h;                      /* biHeight: top-down */
    pBuf[1] = w;
    pBuf[0] = h;
    /* the global BITMAPINFO AppInit filled, colour table gDibColorTable */
    *(HBITMAP *)((char *)pBuf + 8) = AllocateHotspotPixelBuffer((HDC)gBackBufferDC,
                                                                (BITMAPINFO *)&gBitmapHeader, &pBits);
    *(void **)((char *)pBuf + 4) = pBits;
    InitHotspotBitmapState((int *)((char *)pBuf + 12));
}



/* Creates the pixel buffer: WinGCreateBitmap when WinG is in use (gIsWinGBackend), otherwise
 * CreateDIBSection with DIB_RGB_COLORS, both through their resolved pointers. The caller passes the
 * back-buffer DC and the global BITMAPINFO at 0x00486e80. */
HBITMAP __cdecl AllocateHotspotPixelBuffer(HDC hdc, BITMAPINFO *pBmi, void **ppBits)  /* 0x0041f780 */
{
    if (gIsWinGBackend != 0)
        return ((PFN_WinGCreateBitmap)(UINT_PTR)gWinGCreateBitmapProc)(hdc, pBmi, ppBits);
    return ((PFN_CreateDIBSection)(UINT_PTR)gCreateDIBSectionProc)(hdc, pBmi, 0, ppBits, NULL, 0);
}


/* Tears down HotspotBuf initialized by InitHotspotBuffer (0x0041f700):
 * cleans up bitmaps via CleanupHotspotBitmaps(pBuf+0x0c), deletes GDI object
 * at pBuf+8. Called by EndGameSession with (int)&gHotspotBuf. */
void __cdecl DestroyHotspotBuffer(int pBuf)  /* 0x0041f7c0 */
{
    CleanupHotspotBitmaps(pBuf + 0x0c);
    DeleteObject(*(HGDIOBJ *)(pBuf + 8));
}


/* Converts a scene-space rect to window client coordinates and calls
 * InvalidateRect on the main game window, scheduling a WM_PAINT.
 *
 * pRect[4] = { y1, x1, y2, x2 } in scene space.
 * Scroll offsets are read from gThePort at +0x50 (x) and +0x54 (y). */
void __cdecl InvalidateSceneRect(short *pRect)  /* 0x0041f7e0 */
{
    RECT rc;
    HWND hwnd;

    /* The window is the FIRST FIELD of the snapshot's scene object (0x0041f7e3: mov eax,[0x004872a8];
     * mov edx,[eax]). The port passed the object pointer itself as the HWND, so InvalidateRect
     * failed and nothing was invalidated (class 12, one indirection short). */
    hwnd = *(HWND *)gThePortDesc[0];
#if DF_FIX_STRETCH_DISPLAY
    {
        /* A stretched display maps scene rects to other window rects, so repaint the whole window. */
        DisplayStretch s;

        GetDisplayStretch(hwnd, &s);
        if (s.on) {
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }
    }
#endif
    if (pRect[1] < pRect[3]) {
        if (pRect[0] < pRect[2]) {
            rc.left   = (int)pRect[1] - *(int *)((char *)gThePort + 0x50);
            rc.top    = (int)pRect[0] - *(int *)((char *)gThePort + 0x54);
            rc.right  = (int)pRect[3] - *(int *)((char *)gThePort + 0x50);
            rc.bottom = (int)pRect[2] - *(int *)((char *)gThePort + 0x54);
            InvalidateRect(hwnd, &rc, FALSE);
        }
    }
}


/* Convert screen point to node-local coords via ScreenToClient + offsets. */
void __cdecl MapScreenPointToNode(NodeDesc *pNode, short *pPoint)  /* 0x0041f860 */
{
    DWORD *pNodeObj;
    POINT  pt;

    QdSetPort((int *)&pNode->pNodeObj);
    pNodeObj = (DWORD *)pNode->pNodeObj;
    if (pNodeObj != NULL) {
        BOOL ok;
        pt.x = (LONG)pPoint[1];
        pt.y = (LONG)pPoint[0];
        ok = ScreenToClient(*(HWND *)pNodeObj, &pt);
        DF_TRACE("mapToNode: in=(y=%d,x=%d) hwnd=%p s2c=%d -> client=(%ld,%ld) node[0x16]=%d node[0x17]=%d",
                 (int)pPoint[0], (int)pPoint[1], (void *)*(HWND *)pNodeObj, (int)ok,
                 pt.x, pt.y, (int)pNodeObj[0x16], (int)pNodeObj[0x17]);
#if DF_FIX_STRETCH_DISPLAY
        {
            /* Undo DisplayStretchBlt: client point -> offset inside the centred viewport. */
            DisplayStretch s;

            GetDisplayStretch(*(HWND *)pNodeObj, &s);
            if (s.on) {
                pt.x = s.left + pt.x * s.vw / s.cw;
                pt.y = s.top + pt.y * s.vh / s.ch;
            }
        }
#endif
        pPoint[1] = (short)pNodeObj[0x16] + (short)pt.x;
        pPoint[0] = (short)pNodeObj[0x17] + (short)pt.y;
        DF_TRACE("mapToNode: out=(y=%d,x=%d) nodeType=%d", (int)pPoint[0], (int)pPoint[1], (int)pNode->nodeType);
    }
}


/* Saves current navigation state.  Empty in this build (a bare `ret`), but
 * every caller pushes an argument and cleans 4 bytes: gMovieChannel at
 * 0x00449d44, RunGameLoop's frame at 0x00431b2b.  A pointer parameter: an
 * `int` one lets RunGameLoop's two `&frame` pushes share one register. */
void __cdecl SaveNavigationState(void *ctx)  /* 0x0041f8c0 */
{
}


/* Restores previously saved navigation state.  Empty, like its twin; the
 * callers push gMovieChannel (0x00449d67). */
void __cdecl RestoreNavigationState(void *ctx)  /* 0x0041f8d0 */
{
}


/* Store a new display extent (width/height) in the node object and mark it
 * dirty.  Only acts when the scene snapshot is active (gThePortDesc[0]
 * != 0).  Calls QdApplyPendingClip to propagate the change. */
void __cdecl QdSetOrigin(int pChannel, short width, short height)  /* 0x0041f8e0 */
{
    int pPort = *(int *)(pChannel + 0x12);
    if (gThePortDesc[0] != 0) {
        *(int *)(pPort + 0x58) = (int)width;
        *(int *)(pPort + 0x5c) = (int)height;
        *(int *)(pPort + 0x70) = 1;
        QdApplyPendingClip((void *)(pPort + 8));
    }
}


/* Copies HotspotBuf (30 DWORDs: 3 header + 27 bitmap-state) to pDst[1..30],
 * zeroes pDst[0] as tag. Called by BeginGameSession to snapshot state. */
void __cdecl CopyHotspotBufState(unsigned int *pSrc, unsigned int *pDst)  /* 0x0041f920 */
{
    typedef struct { unsigned int v[30]; } HotspotState120;   /* rep movsd, ecx = 30 */

    *(HotspotState120 *)(pDst + 1) = *(HotspotState120 *)pSrc;
    pDst[0] = 0;
}


/* A no-op (the exe's body is RET); EndGameSession calls it after the hotspot buffer is destroyed,
 * and the caller pushes and cleans one argument. */
void __cdecl TeardownSceneRefs(void *pSavedHotspot)  /* 0x0041f940 */
{
    (void)pSavedHotspot;                /* the exe's body is RET; the caller pushes and cleans one argument */
}


/* Snapshots the 32-int scene-state buffer pSceneState into gThePortDesc and then updates the
 * retained scene reference gThePort: pSceneState[0] is a direct scene-node handle when non-zero
 * (QdSetPortWindow), otherwise the node is described by the remaining 31 ints
 * (QdSetPortOffscreen(pSceneState + 1)). RunGameLoop calls it several times per frame. */
void __cdecl QdSetPort(int *pSceneState)  /* 0x0041f950 */
{
    typedef struct { int d[0x20]; } SceneStateImage;

    if (gThePort != NULL)
        QdReleasePort(gThePort);

    /* Snapshot the full 32-int state buffer: one struct copy (rep movsd, ecx = 0x20) */
    *(SceneStateImage *)gThePortDesc = *(SceneStateImage *)pSceneState;

    if (pSceneState[0] != 0)
        gThePort = QdSetPortWindow(pSceneState[0]);      /* direct node handle */
    else
        gThePort = QdSetPortOffscreen(pSceneState + 1);    /* indirect state data */
}


/* Scrolls / copies a hotspot rect between channel pixels (CPU) or BitBlt between
 * scene-node HDCs and the back-buffer DIB. Guarded by gIsAppActive and valid rects. */
void __cdecl QdCopyBits(int *pSrcBits, int *pDstBits, short *pSrcRect, short *pDstRect) /* 0x0041f9b0 */
{
    int     cx, cy, pNode, pDstNode;
    short   sR, sL;
    HGDIOBJ h;
    int     isDisplay;

    sR = pDstRect[3];
    sL = pDstRect[1];
    cx = (int)sR - (int)sL;
    cy = (int)pDstRect[2] - (int)pDstRect[0];
    DF_TRACE("scrollDelta: gate358=%d src=(%d,%d,%d,%d) dst=(%d,%d,%d,%d) "
             "p1type=%d p2type=%d", (int)gIsAppActive,
             (int)pSrcRect[0], (int)pSrcRect[1], (int)pSrcRect[2], (int)pSrcRect[3],
             (int)pDstRect[0], (int)pDstRect[1], (int)pDstRect[2], (int)pDstRect[3],
             *pSrcBits, *pDstBits);
    if ((((gIsAppActive != 0) && ((int)pSrcRect[1] < (int)pSrcRect[3])) &&
         ((int)pSrcRect[0] < (int)pSrcRect[2])) &&
        (((int)pDstRect[1] < (int)pDstRect[3] && ((int)pDstRect[0] < (int)pDstRect[2])))) {
        if (*pSrcBits == 0) {
            if (*pDstBits == 0) {
                CopyHotspotRectsBetweenChannelBuffers((short *)(pSrcBits + 1), (short *)(pDstBits + 1),
                                                      pSrcRect, pDstRect);
                return;
            }
            if (gIsAppActive != 0) {
                h = SelectObject(gBackBufferDC, (HGDIOBJ)(UINT_PTR)pSrcBits[3]);
                SelectPalette(*(HDC *)(*pDstBits + 4), gPalette, FALSE);
                RealizePalette(*(HDC *)(*pDstBits + 4));
                pNode = *pDstBits;
                DF_TRACE("blitNode: node=%p dstOrg=(%d,%d) srcOrg=(%d,%d)",
                         (void *)(intptr_t)pNode,
                         *(int *)(pNode + 0x58), *(int *)(pNode + 0x5c),
                         pSrcBits[0x18], pSrcBits[0x19]);
                /* This was inlined as a bare BitBlt, which silently dropped
                 * RenderSceneDisplayBuffer's SetDIBColorTable call: the back-buffer DIB was
                 * blitted with a colour table that had never been uploaded. The SOURCE origin
                 * comes from pSrcBits, the CPU / DIB buffer state block itself, at +0x60 / +0x64,
                 * NOT from the destination node (0x0041fab3: MOVSX EAX,word [EDI]; SUB EAX,
                 * [EBX+0x64]; EBX = pSrcBits): subtracting the destination node's centring
                 * origin put the source read at (704,348) inside a 512x264 DIB, entirely outside
                 * the bitmap. The two-node branch below does use (*pSrcBits)+0x58 / +0x5c. This
                 * caller pushes ten arguments and cleans 0x28; a9 / a10 are unused by both of the
                 * callee's paths (0x0041fbe2, 0x0041fc0e) but are passed as the exe computes
                 * them: a9 = the destination is the display node, a10 = a9 ? the source pixel
                 * buffer : NULL (0x0041fa96). */
                isDisplay = *(int *)pNode == gMainWindow;
                RenderSceneDisplayBuffer(*(HDC *)(pNode + 4),
                       (int)pDstRect[1] - *(int *)(pNode + 0x58),
                       (int)pDstRect[0] - *(int *)(pNode + 0x5c),
                       cx, cy,
                       (HDC)(UINT_PTR)gBackBufferDC,
                       (int)pSrcRect[1] - pSrcBits[0x18],   /* pSrcBits + 0x60 */
                       (int)pSrcRect[0] - pSrcBits[0x19],   /* pSrcBits + 0x64 */
                       isDisplay, isDisplay ? pSrcBits + 1 : 0);
                SelectObject(gBackBufferDC, h);
                return;
            }
        } else {
            if (*pDstBits == 0) {
                FatalError(0, 0x544);
                return;
            }
            SelectPalette(*(HDC *)(*pDstBits + 4), gPalette, FALSE);
            RealizePalette(*(HDC *)(*pDstBits + 4));
            SelectPalette(*(HDC *)(*pSrcBits + 4), gPalette, FALSE);
            RealizePalette(*(HDC *)(*pSrcBits + 4));
            pNode = *pSrcBits;
            pDstNode = *pDstBits;
#if DF_FIX_STRETCH_DISPLAY
            if (*(int *)pDstNode != gMainWindow
                || !DisplayStretchBlt(*(HDC *)(pDstNode + 4),
                                      (int)pDstRect[1] - *(int *)(pDstNode + 0x58),
                                      (int)pDstRect[0] - *(int *)(pDstNode + 0x5c),
                                      cx, cy,
                                      *(HDC *)(pNode + 4),
                                      (int)pSrcRect[1] - *(int *)(pNode + 0x58),
                                      (int)pSrcRect[0] - *(int *)(pNode + 0x5c)))
#endif
            BitBlt(*(HDC *)(pDstNode + 4),
                   (int)pDstRect[1] - *(int *)(pDstNode + 0x58),
                   (int)pDstRect[0] - *(int *)(pDstNode + 0x5c),
                   cx, cy,
                   *(HDC *)(pNode + 4),
                   (int)pSrcRect[1] - *(int *)(pNode + 0x58),
                   (int)pSrcRect[0] - *(int *)(pNode + 0x5c),
                   SRCCOPY);
        }
    }
}


/* Uploads the palette to the source DC with SetDIBColorTable and then BitBlts, passing its first
 * eight parameters straight through. Its caller pushes ten (add esp,0x28): the last two, whether the
 * destination is the display node and then the source's pixel buffer or NULL, are never read.
 * Critical: a DIB section whose colour table has never been set renders as solid black regardless
 * of its pixels. Ghidra cannot recover it (literal addresses as BitBlt arguments, unaff_ESI /
 * in_stack_00000000); the port reads the raw disassembly 0x0041fbb8..0x0041fc37. */
void __cdecl RenderSceneDisplayBuffer(HDC hdcDest, int x, int y, int cx, int cy,
                                      HDC hdcSrc, int x1, int y1,
                                      int isDisplayNode, void *pPixels)  /* 0x0041FBB0 */
{
  typedef UINT (WINAPI *PFN_SetDIBColorTable)(HDC, UINT, UINT, const RGBQUAD *);
  typedef BOOL (WINAPI *PFN_WinGBitBlt)(HDC, int, int, int, int, HDC, int, int);

  DF_TRACE("blit: dst=%p (%d,%d) %dx%d src=%p (%d,%d)", (void *)hdcDest,
           x, y, cx, cy, (void *)hdcSrc, x1, y1);
  if (gIsWinGBackend != 0) {
    ((PFN_SetDIBColorTable)(UINT_PTR)gWinGSetDIBColorTableProc)(hdcSrc, 0, 0x100, gDibColorTable);
  }
  else {
    ((PFN_SetDIBColorTable)(UINT_PTR)gSetDIBColorTableProc)(hdcSrc, 0, 0x100, gDibColorTable);
  }
#if DF_FIX_STRETCH_DISPLAY
  if (DisplayStretchBlt(hdcDest, x, y, cx, cy, hdcSrc, x1, y1))
    return;
#endif
  if (gIsWinGBackend != 0) {
    ((PFN_WinGBitBlt)(UINT_PTR)gWinGBitBltProc)(hdcDest, x, y, cx, cy, hdcSrc, x1, y1);
    return;
  }
  BitBlt(hdcDest, x, y, cx, cy, hdcSrc, x1, y1, SRCCOPY);
}


/* CPU 8-bpp copy between two HotspotBuf blobs that sit immediately after a leading dword in each
 * channel snapshot block (see QdCopyBits): from the first buffer at pRectSrc to the second at
 * pRectDst. The size is pRectDst's, clipped to both buffers' widths (+2) and heights (+0).
 * */
static void __cdecl CopyHotspotRectsBetweenChannelBuffers(short *pSrcHotFields, short *pDstHotFields,
                                                          short *pRectSrc, short *pRectDst) /* 0x0041fc40 */
#ifdef _MSC_VER
{
    /* The buffer parameters are read through `*(HotspotBuf **)&param` at each
     * use, and pRectSrc through `*(int *)&pRectSrc`: the exe reloads them from
     * the frame every time and keeps srcW / dstW / dstX / w in registers
     * instead (a local pointer copy, or a plain cast, lets VC cache them).
     * srcX and srcY live in memory (frame -0xc / -0x10): the compiled copy
     * takes their address (match/line_patches.json). Declaration order
     * and nDwords-before-nBytes: register choice. */
    int dstW;
    int srcX;
    int dstX;
    int dstY;
    int srcY;
    int h;
    int srcSkip;
    int dstSkip;
    int nDwords;
    int nBytes;
    char *bs;
    char *bd;
    int srcW;
    int w;

    bs = (char *)(*(HotspotBuf **)&pSrcHotFields)->pPx;
    bd = (char *)(*(HotspotBuf **)&pDstHotFields)->pPx;
    srcW = (*(HotspotBuf **)&pSrcHotFields)->w;
    dstW = (*(HotspotBuf **)&pDstHotFields)->w;
    dstX = pRectDst[1];
    w    = pRectDst[3] - dstX;
    dstY = pRectDst[0];
    h    = pRectDst[2] - dstY;
    srcX = ((short *)*(int *)&pRectSrc)[1];
    if (srcX + w > srcW)
        w = srcW - srcX;
    if (dstX + w > dstW)
        w = dstW - dstX;
    srcY = ((short *)*(int *)&pRectSrc)[0];
    if (h + srcY > (*(HotspotBuf **)&pSrcHotFields)->h)
        h = (*(HotspotBuf **)&pSrcHotFields)->h - srcY;
    if (h + dstY > (*(HotspotBuf **)&pDstHotFields)->h)
        h = (*(HotspotBuf **)&pDstHotFields)->h - dstY;
    srcSkip = srcW - w;
    bs += srcW * srcY + srcX;
    dstSkip = dstW - w;
    bd += dstW * dstY + dstX;
    if (h > 0 && w > 0) {
        nDwords = w >> 2;
        nBytes = w & 3;
        __asm {
            pushad
            mov  esi, bs
            mov  edi, bd
            mov  eax, srcSkip
            mov  ebx, dstSkip
            mov  edx, h
        row:
            mov  ecx, nDwords
            rep  movsd
            mov  ecx, nBytes
            rep  movsb
            add  esi, eax
            add  edi, ebx
            dec  edx
            jnz  row
            popad
        }
    }
}
#else
{
    HotspotBuf *src = (HotspotBuf *)pSrcHotFields;
    HotspotBuf *dst = (HotspotBuf *)pDstHotFields;
    int srcW, dstW, dstX, dstY, srcX, srcY, w, h;
    unsigned char *bs, *bd;

    srcW = src->w;
    dstW = dst->w;
    dstX = pRectDst[1];
    w    = pRectDst[3] - dstX;
    dstY = pRectDst[0];
    h    = pRectDst[2] - dstY;
    srcX = pRectSrc[1];
    if (srcX + w > srcW)
        w = srcW - srcX;
    if (dstX + w > dstW)
        w = dstW - dstX;
    srcY = pRectSrc[0];
    if (h + srcY > src->h)
        h = src->h - srcY;
    if (h + dstY > dst->h)
        h = dst->h - dstY;
    bs = (unsigned char *)src->pPx + srcW * srcY + srcX;
    bd = (unsigned char *)dst->pPx + dstW * dstY + dstX;
    if (h > 0 && w > 0) {
        do {
            memcpy(bd, bs, (size_t)w);
            bs += srcW;
            bd += dstW;
        } while (--h != 0);
    }
}
#endif


/* Computes the scene viewport rectangle in scene-space coordinates.
 *
 * pSceneState[0] is a pointer to the scene object.  Two paths:
 *
 *   Scene node present (pSceneState[0] != 0):
 *     Get the client rectangle via GetClientRect(HWND at obj[0]).
 *     piVar2 = scene object + 2  (viewport offsets at piVar2[0x14] / [0x15]).
 *
 *   No scene node (pSceneState[0] == 0):
 *     Build a synthetic rect {0, 0, *(short*)(pSceneState + 6 bytes), pSceneState[1]}.
 *     piVar2 = pSceneState + 4   (viewport offsets at piVar2[0x14] / [0x15]).
 *
 * Output (4 shorts):
 *   pOut[0] = viewportY + top       (Y min)
 *   pOut[1] = viewportX + left      (X min)
 *   pOut[2] = viewportY + bottom    (Y max)
 *   pOut[3] = viewportX + right     (X max)
 *
 * The {y, x, y, x} ordering matches the ScenePos convention seen in
 * GetSceneCursorPos where [0]=y, [1]=x. */
void __cdecl GetSceneRect(int *pSceneState, short *pOut)  /* 0x0041fd40 */
{
    unsigned int *pObj;
    int          *piSrc;
    RECT          rc;

    pObj = (unsigned int *)pSceneState[0];
    if (pObj != NULL)
    {
        piSrc = (int *)(pObj + 2);
        GetClientRect((HWND)pObj[0], &rc);
    }
    else
    {
        piSrc = pSceneState + 4;
        rc.top    = 0;
        rc.left   = 0;
        rc.right  = *(short *)((char *)pSceneState + 6);
        rc.bottom = (short)pSceneState[1];
    }

    pOut[1] = (short)piSrc[0x14] + (short)rc.left;
    pOut[0] = (short)piSrc[0x15] + (short)rc.top;
    pOut[3] = (short)piSrc[0x14] + (short)rc.right;
    pOut[2] = (short)piSrc[0x15] + (short)rc.bottom;
}



/* Looks up RGB components for palette index idx in three byte arrays
 * (gDibColorTable/a9/aa, stride 4), packs them into a color word, and
 * stores the result at gThePort+0x10. Also saves the index at +0x14
 * and marks the node dirty at +0x44. */
void __cdecl SetSceneNodeColorFromPalette(short idx)  /* 0x0041fdc0 */
{
    int i = (int)idx;

    /* gThePort re-read for each store. gDibColorTable is RGBQUAD[256]: the
     * COLORREF is built from its blue/green/red bytes (+0/+1/+2) */
    *(unsigned int *)((char *)gThePort + 0x10) =
        ((unsigned int)gDibColorTable[i].rgbBlue << 0x10) | ((unsigned int)gDibColorTable[i].rgbGreen << 8) | gDibColorTable[i].rgbRed;
    *(int *)((char *)gThePort + 0x14) = i;
    *(unsigned int *)((char *)gThePort + 0x44) = 1;
}


/* Stores display-surface mode in the retained scene node (**+0x28**) and marks
 * **+0x44** dirty (same layout Ghidra uses on **`gThePort`** / `gThePort`). */
void __cdecl QdPenSize(short size) /* 0x0041fe10 */
{
    /* no null guard in the exe, and the scene pointer is re-read for the second
     * store (an int store through it may alias the pointer itself) */
    *(int *)((char *)gThePort + 0x28) = (int)size;
    *(unsigned int *)((char *)gThePort + 0x44) = 1u;
}


/* Zero/reset the viewport-state fields inside the current scene-ref object.
 * Sets default values for clip rect, enable flags, and scroll offsets. */
void __stdcall QdPenNormal(void)  /* 0x0041fe30 */
{
    int i;

    /* the scene pointer is re-read after every store (an int store may alias it) */
    *(int *)((char *)gThePort + 0x2c) = 0;
    i = 0;
    do { ((char *)gThePort)[0x20 + i] = 0; i++; } while (i < 8);
    *(int *)((char *)gThePort + 0x4c) = 1;
    *(int *)((char *)gThePort + 0x28) = 1;
    *(int *)((char *)gThePort + 0x10) = 0;
    *(int *)((char *)gThePort + 0x18) = 0xffffff;
    *(int *)((char *)gThePort + 0x44) = 1;
}


/* Write a new viewport rect into the scene-ref object at the standard offsets
 * and set the dirty flag, then call QdApplyPendingClip.
 * pRect[0..3] = { top, left, bottom, right } as shorts. */
void __cdecl QdClipRect(short *pRect)  /* 0x0041fe90 */
{
    /* the scene pointer is re-read for every store: an int store through it may
     * alias the pointer itself */
    ((int *)gThePort)[0x16] = (int)pRect[1];   /* left   */
    ((int *)gThePort)[0x17] = (int)pRect[0];   /* top    */
    ((int *)gThePort)[0x18] = (int)pRect[3];   /* right  */
    ((int *)gThePort)[0x19] = (int)pRect[2];   /* bottom */
    ((int *)gThePort)[0x1a] = 1;               /* dirty  */
    QdApplyPendingClip(gThePort);
}


/* Copies an 8-byte (8x8) draw pattern into the active scene-reference
 * object (gThePort+0x20), sets the pattern-enabled flag (+0x44) and a
 * solid flag (+0x4c) when every pattern byte is 0xFF. */
void __cdecl QdPenPat(int pPattern)  /* 0x0041FEF0 */
{
  int i;
  uint32_t isSolid;

  isSolid = 1;
  i = 0;
  do {
    *((byte *)gThePort + 0x20 + i) = *(byte *)(pPattern + i);
    if (*((byte *)gThePort + 0x20 + i) != 0xff) {
      isSolid = 0;
    }
    i = i + 1;
  } while (i < 8);
  *(uint32_t *)((char *)gThePort + 0x44) = 1;
  *(uint32_t *)((char *)gThePort + 0x4c) = isSolid;
}


/* Raster op / pen mode on the scene HDC: writes **+0x2c**, applies **SetROP2**
 * (**0xd** or **7** for modes **0** / **1**), sets **+0x44** dirty. */
void __cdecl QdPenMode(short mode) /* 0x0041ff40 */
{
    /* no null guard in the exe (the port had added one with no DF_FIX, as in
     * QdPenSize); the scene pointer is re-read after each store */
    *(int *)((char *)gThePort + 0x2c) = (int)mode;
    switch (mode) {
    case 0:
        SetROP2(*(HDC *)gThePort, 0xd);   /* R2_COPYPEN */
        break;
    case 1:
        SetROP2(*(HDC *)gThePort, 7);     /* R2_XORPEN */
        break;
    }
    *(unsigned int *)((char *)gThePort + 0x44) = 1u;   /* dirty */
}


/* Writes `font` into the scroll-column-width field of the scene node
 * (gThePort+0x38) and sets the dirty flag (gThePort+0x48) to 1. */
void __cdecl QdTextFont(short font)  /* 0x0041ff80 */
{
    *(int *)((char *)gThePort + 0x38) = (int)font;
    *(int *)((char *)gThePort + 0x48) = 1;
}


/* Writes `size` into the scroll-row-count field of the scene node
 * (gThePort+0x3c) and sets the dirty flag (gThePort+0x48) to 1. */
void __cdecl QdTextSize(short size)  /* 0x0041ffa0 */
{
    *(int *)((char *)gThePort + 0x3c) = (int)size;
    *(int *)((char *)gThePort + 0x48) = 1;
}


/* Stores the drawing-pen position (x, y) into the scene node
 * (gThePort+0x30 = penX, gThePort+0x34 = penY).  No dirty flag set;
 * position is consumed by QdLineTo and QdDrawString. */
void __cdecl QdMoveTo(short x, short y)  /* 0x0041ffc0 */
{
    *(int *)((char *)gThePort + 0x30) = (int)x;
    *(int *)((char *)gThePort + 0x34) = (int)y;
}


/* Draws a line from the node's last pen position (node[0xc], node[0xd]) to
 * (param_1, param_2) using the scene node's HDC, adjusting for screen origin
 * (node[0x14], node[0x15]).
 *
 * If the destination equals the current last-pen position, draws a single
 * pixel instead of a line:
 *   - if HDC == back-buffer DC (gBackBufferDC): SetPixel with node[4] (COLORREF)
 *   - otherwise: SetPixel with the alt-color (ushort at node+5, | 0x1000000)
 *
 * Guarded by gIsAppActive (invert/draw enable flag).
 * PrepareSceneNodeHdcForVectorDraw: applies pen/brush settings to the HDC before LineTo. */
void __cdecl QdLineTo(short param_1, short param_2)  /* 0x0041ffe0 */
{
    HDC    hdc;
    int    toX;
    int    y;
    int    toY;
    int    x;

    /* gThePort is re-read for every field (Ghidra 0x0041ffe0) */
    if (gIsAppActive != 0) {
        toX = (int)param_1;
        toY = (int)param_2;
        x     = toX - ((int *)gThePort)[0x14];
        y     = toY - ((int *)gThePort)[0x15];
        if (((int *)gThePort)[0xc] == toX && ((int *)gThePort)[0xd] == toY) {
            hdc = (HDC)((int *)gThePort)[0];
            if ((HDC)gBackBufferDC == hdc) {
                SetPixel(hdc, x, y, (COLORREF)((int *)gThePort)[4]);
                return;
            }
            SetPixel(hdc, x, y, (COLORREF)((*(ushort *)(((int *)gThePort) + 5)) | 0x1000000));
            return;
        }
        MoveToEx((HDC)((int *)gThePort)[0], ((int *)gThePort)[0xc] - ((int *)gThePort)[0x14], ((int *)gThePort)[0xd] - ((int *)gThePort)[0x15], (LPPOINT)0);
        PrepareSceneNodeHdcForVectorDraw(((int *)gThePort));
        LineTo((HDC)((int *)gThePort)[0], x, y);
        ((int *)gThePort)[0xc] = toX;
        ((int *)gThePort)[0xd] = toY;
    }
}


/* Needed by QdEraseRect
 *
 * Fills the given YX-ordered rect {y0,x0,y1,x1} with the scene node's
 * current brush (gThePort[3]), or WHITE_BRUSH if none is set.
 * Adjusts for scroll offsets from the node context.  No-op if the scene
 * is not in draw mode (gIsAppActive == 0) or the rect is empty/degenerate. */
void __cdecl QdEraseRect(short *pRect)  /* 0x004200d0 */
{
    HDC    hDC;
    HBRUSH hbr;
    RECT   rc;

    if (gIsAppActive == 0)
        return;
    if (pRect[1] >= pRect[3] || *pRect >= pRect[2])
        return;

    hDC         = (HDC)*gThePort;
    PrepareSceneNodeHdcForVectorDraw(gThePort);
    rc.left     = (int)pRect[1] - gThePort[0x14];
    rc.top      = (int)*pRect   - gThePort[0x15];
    rc.right    = (int)pRect[3] - gThePort[0x14];
    rc.bottom   = (int)pRect[2] - gThePort[0x15];
    hbr = (HBRUSH)gThePort[3];
    if (hbr == (HBRUSH)0)
        hbr = (HBRUSH)gWhiteBrush;
    FillRect(hDC, &rc, hbr);
}



#if DF_FIX_STRETCH_DISPLAY
/* Maps a window rect of the main window the way DisplayStretchBlt maps a blit, so the highlight
 * helpers draw on the stretched scene (DF_FIX_STRETCH_DISPLAY). No change unless GetDisplayStretch is on. */
static void StretchWindowRectForDisplay(HWND hwnd, RECT *rc)  /* PORT-HELPER: DF_FIX_STRETCH_DISPLAY */
{
    DisplayStretch s;

    GetDisplayStretch(hwnd, &s);
    if (!s.on)
        return;
    rc->left   = (rc->left   - s.left) * s.cw / s.vw;
    rc->top    = (rc->top    - s.top)  * s.ch / s.vh;
    rc->right  = (rc->right  - s.left) * s.cw / s.vw;
    rc->bottom = (rc->bottom - s.top)  * s.ch / s.vh;
}
#endif


/* XOR-inverts a scene-space rect on the back-buffer DC, producing a
 * visual highlight effect. Only acts when gIsAppActive is non-zero.
 *
 * pRect[4] = {y1, x1, y2, x2} in scene space. Scroll offsets at gThePort+0x50
 * (x) and +0x54 (y) are subtracted to convert to DC coordinates. The HDC is
 * taken from gThePort[0]. */
void __cdecl QdInvertRect(short *pRect)  /* 0x00420170 */
{
    RECT rc;

    if (gIsAppActive == 0) return;
    if (pRect[1] >= pRect[3]) return;
    if (pRect[0] >= pRect[2]) return;

    rc.left   = (int)pRect[1] - ((int *)gThePort)[0x14];
    rc.right  = (int)pRect[3] - ((int *)gThePort)[0x14];
    rc.top    = (int)pRect[0] - ((int *)gThePort)[0x15];
    rc.bottom = (int)pRect[2] - ((int *)gThePort)[0x15];
#if DF_FIX_STRETCH_DISPLAY
    StretchWindowRectForDisplay(WindowFromDC((HDC)((int *)gThePort)[0]), &rc);
#endif
    InvertRect((HDC)((int *)gThePort)[0], &rc);
}



/* Outlines a scene-space **`{y1,x1,y2,x2}`** rect on the node HDC (NULL_BRUSH),
 * respecting scroll fields +0x14 / +0x15 and halfpen +0x10[>>1].
 * Guarded like QdInvertRect by gIsAppActive and strict rect order. */
void __cdecl QdFrameRect(short *pRect) /* 0x004201f0 */
{
    HDC     hdc;
    HGDIOBJ prev;
    int     half;

    if (gIsAppActive == 0)
        return;
    if (pRect[1] >= pRect[3] || *pRect >= pRect[2])
        return;

    hdc = (HDC)(UINT_PTR)gThePort[0];
    PrepareSceneNodeHdcForVectorDraw(gThePort);
    prev = SelectObject(hdc, (HGDIOBJ)(UINT_PTR)gNullBrush);
    half = gThePort[10] >> 1;
    Rectangle(hdc,
              ((int)pRect[1] - gThePort[0x14]) + half,
              ((int)*pRect - gThePort[0x15]) + half,
              ((int)pRect[3] - half) - gThePort[0x14],
              ((int)pRect[2] - half) - gThePort[0x15]);
#if DF_FIX_STRETCH_DISPLAY
    RECT    rc;
    rc.left   = ((int)pRect[1] - gThePort[0x14]) + half;
    rc.top    = ((int)*pRect - gThePort[0x15]) + half;
    rc.right  = ((int)pRect[3] - half) - gThePort[0x14];
    rc.bottom = ((int)pRect[2] - half) - gThePort[0x15];
    StretchWindowRectForDisplay(WindowFromDC(hdc), &rc);
#endif
    SelectObject(hdc, prev);
}



/* Frames pRect with the 8-byte checkerboard stipple pattern (kStipplePattern) when the app is active
 * and the rect is not empty. */
void __cdecl QdFrameRectStippled(short *pRect)  /* 0x00420280 */
{
  if (((gIsAppActive != 0) && (pRect[1] < pRect[3])) && (*pRect < pRect[2])) {
    QdPenPat((int)(intptr_t)kStipplePattern);   /* 8-byte checkerboard stipple; was the literal 1996 address -- port-bug-classes.md #5 */
    QdFrameRect(pRect);
  }
  return;
}


/* Pattern-brush fill invalidation on the scene node HDC (not the same as InvalidateSceneRect). */
void __cdecl QdPaintRect(short *pRect) /* 0x004202c0 */
{
    HDC     hdc;
    HGDIOBJ h;
    POINT   brushOrg;  /* local_18 */
    RECT    fillRect;  /* local_10 */

    if (gIsAppActive == 0)
        return;
    if (pRect[1] >= pRect[3] || pRect[0] >= pRect[2])
        return;

    hdc = (HDC)(UINT_PTR)gThePort[0];
    PrepareSceneNodeHdcForVectorDraw(gThePort);
    fillRect.left   = (int)pRect[1] - gThePort[0x14];
    fillRect.top    = (int)*pRect   - gThePort[0x15];
    fillRect.right  = (int)pRect[3] - gThePort[0x14];
    fillRect.bottom = (int)pRect[2] - gThePort[0x15];
#if DF_FIX_STRETCH_DISPLAY
    StretchWindowRectForDisplay(WindowFromDC(hdc), &fillRect);
#endif
    h = (HGDIOBJ)(UINT_PTR)gThePort[2];
    if (h != (HGDIOBJ)NULL) {
        UnrealizeObject(h);
        brushOrg.x = fillRect.left;
        brushOrg.y = fillRect.top;
        LPtoDP(hdc, &brushOrg, 1);
        ClientToScreen(WindowFromDC(hdc), &brushOrg);
        brushOrg.x += 2;
        SetBrushOrgEx(hdc, brushOrg.x % 8, brushOrg.y % 8, (LPPOINT)NULL);
    } else
        h = (HGDIOBJ)(UINT_PTR)gBlackBrush;
    h = SelectObject(hdc, h);
    FillRect(hdc, &fillRect, (HBRUSH)h);
    SelectObject(hdc, h);
}


/* Draw a Pascal string at the current pen position in scene DC space when
 * gIsAppActive (invert/draw enable) is set. */
void __cdecl QdDrawString(byte *pPascalStr) /* 0x004203e0 */
{
    HDC    hdc;

    /* gThePort is re-read for every use, MoveToEx's HDC included */
    if (gIsAppActive != 0) {
        RefreshHotspotBitmapFontIfNeeded((unsigned *)gThePort);
        PrepareSceneNodeHdcForVectorDraw((int *)gThePort);
        hdc = *(HDC *)gThePort;
        SetBkMode(hdc, TRANSPARENT);
        MoveToEx(*(HDC *)gThePort, ((int *)gThePort)[0x0c] - ((int *)gThePort)[0x14],
                 ((int *)gThePort)[0x0d] - ((int *)gThePort)[0x15], NULL);
        TextOutA(hdc, 0, 0, (LPCSTR)(pPascalStr + 1), (UINT)*pPascalStr);
        SetBkMode(hdc, OPAQUE);
    }
}


/* Measures the pixel width of a Pascal string body using the scene node's HDC (gThePort). */
short __cdecl QdStringWidth(byte *pPascalStr) /* 0x00420460 */
{
    RECT  rc;
    HDC   hdc;

    /* gThePort is re-read for each use; the flags are DT_CALCRECT alone
     * (push 0x400 at 0x00420486) -- the port had added DT_SINGLELINE */
    RefreshHotspotBitmapFontIfNeeded((unsigned *)gThePort);
    PrepareSceneNodeHdcForVectorDraw((int *)gThePort);
    hdc = *(HDC *)gThePort;
    rc.bottom = 0x20;
    rc.top    = 0;
    rc.right  = 0;
    rc.left   = 0;
    DrawTextA(hdc, (LPCSTR)(pPascalStr + 1), (UINT)*pPascalStr, &rc, DT_CALCRECT);
    return (short)rc.right;
}


/* Re-attaches menu bar via SetMenu/DrawMenuBar; 3 guard checks */
void __stdcall RestoreMenuBar(void)  /* 0x004204d0 */
{
    if (gMainWindow != 0 && gIsMenuVisible == 0 && gMainMenu != 0)
    {
        if (IsWindow((HWND)gMainWindow))
        {
            gIsMenuVisible = 1;
            SetMenu((HWND)gMainWindow, (HMENU)gMainMenu);
            DrawMenuBar((HWND)gMainWindow);
        }
    }
}


/* Hides the application menu bar (counterpart to RestoreMenuBar).
 * Invoked from extended action 16037 when the script flag is zero. */
void __stdcall HideMenuBar(void)  /* 0x00420530 */
{
    BOOL ok;

    if (gMainWindow != 0 && gIsMenuVisible != 0 && gMainMenu != 0)
    {
        ok = IsWindow((HWND)gMainWindow);
        if (ok != 0)
        {
            gIsMenuVisible = 0;
            SetMenu((HWND)gMainWindow, (HMENU)0);
            DrawMenuBar((HWND)gMainWindow);
        }
    }
}




/* Prepares the screen for a modal dialog: silences the sound mixer, hides every viewport window
 * except the main one (remembering which were visible), in true-colour mode shows the cover window
 * and releases the system palette, and forces the cursor visible (saving the depth in
 * gSavedCursorShowDepth). RestoreGameUiAfterModalDialog undoes it. */
void __stdcall SuspendGameUiForModalDialog(void) /* 0x00420580 */
{
    int i;

    if (gSoundOn != 0)
        SetSoundMixerSilenced(1);

    /* The exe re-reads the slot's HWND for every use and stores
     * IsWindowVisible's result as is. */
    for (i = 0; i < gViewportCount; i++) {
        /* Same dangling-pointer class fixed repeatedly elsewhere --
         * this walk had no guard at all. IsPointerReadable is
         * VirtualQuery-based (IsBadReadPtr itself crashed on this exact
         * bug class when tried elsewhere). */
        if (!DF_VIEWPORT_SLOT_OK(gViewportTable[i], sizeof(HWND)))
            continue;
        if ((HWND)(UINT_PTR)gMainWindow != *(HWND *)gViewportTable[i]) {
            gViewportWasVisible[i] = IsWindowVisible(*(HWND *)gViewportTable[i]);
            ShowWindow(*(HWND *)gViewportTable[i], SW_HIDE);
        }
    }

    if (gCustomPaletteFlag != 0) {
        gCoverWindowWasVisible = IsWindowVisible(gCoverWindow);
        ShowWindow(gCoverWindow, SW_SHOW);
        UpdateWindow(gCoverWindow);
        if (gIsAppActive >= 0)
            ManageSystemPalette(0);
    }

    gSavedCursorShowDepth = gCursorShowDepth;
    while (gCursorShowDepth <= 0) {
        ShowCursor(TRUE);
        gCursorShowDepth++;
    }
}


/* Undoes SuspendGameUiForModalDialog: in true-colour mode re-acquires the system palette and
 * re-applies the CLUT and hides the cover window again if it was hidden, un-silences the sound,
 * restores the cursor show depth and re-shows the viewport windows that were visible. */
void __stdcall RestoreGameUiAfterModalDialog(void) /* 0x00420660 */
{
    void *vp;
    HWND  hwnd;
    int   i;

    if (gCustomPaletteFlag != 0) {
        /* Both calls sit under the test in the exe (0x0042066d `jl` skips
         * both); the port had re-applied the CLUT unconditionally. */
        if (gIsAppActive >= 0) {
            ManageSystemPalette(1);
            ApplyCLUTToPalette((int)(INT_PTR)((char *)gPal16 - 0x0A));   /* 0x487540 */
        }
        if (gCoverWindowWasVisible == 0)
            ShowWindow(gCoverWindow, SW_HIDE);
    }

    if (gSoundOn != 0)
        SetSoundMixerSilenced(0);

    while (gSavedCursorShowDepth < gCursorShowDepth) {
        ShowCursor(FALSE);
        gCursorShowDepth--;
    }

    for (i = 0; i < gViewportCount; i++) {
        vp = gViewportTable[i];
        if (!DF_VIEWPORT_SLOT_OK(vp, sizeof(HWND)))
            continue;
        hwnd = *(HWND *)vp;
        if ((HWND)(UINT_PTR)gMainWindow != hwnd && gViewportWasVisible[i] != 0)
            ShowWindow(hwnd, SW_SHOW);
    }
}


/* The OPENFILENAME.lpfnHook of the open / save dialogs. On WM_INITDIALOG, the only message
 * handled, it centres the dialog in the main window (CenterDialogInMainWindow(GetParent(hdlg)) when
 * gIsWin95OrNT4 is set, else CenterDialogInMainWindow(hdlg)) and returns 1; anything else returns
 * 0. __stdcall with four parameters (ret 0x10).
 *
 * Port note: Ghidra recovers no function here (only LAB_00420730, referenced solely through the
 * lpfnHook field), so the port's body was inferred and invented a WM_NOTIFY / OFNOTIFY /
 * CDN_INITDONE path that does not exist in the binary (class 12). The ground truth was carved out
 * of TI.EXE at 0x00420730 and disassembled. */
UINT_PTR __stdcall OpenFileNameModalHook(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) /* 0x00420730 */
{
    (void)wParam;
    (void)lParam;

    if (msg != WM_INITDIALOG)
        return 0;

    if (gIsWin95OrNT4 != 0)
        CenterDialogInMainWindow(GetParent(hdlg));
    else
        CenterDialogInMainWindow(hdlg);

    return 1;
}


/* Center dialog in main game window (fallback to gCoverWindow if needed). */
void __cdecl CenterDialogInMainWindow(HWND hDlg) /* 0x00420780 */
{
    HWND    hParent;
    tagRECT rcParent;
    tagRECT rcDlg;
    tagRECT rcAdj;

    hParent = (HWND)(UINT_PTR)gMainWindow;
    if (gMainWindow == 0)
        hParent = gCoverWindow;

    GetWindowRect(hParent, &rcParent);
    GetWindowRect(hDlg,    &rcDlg);

    CopyRect(&rcAdj, &rcParent);
    OffsetRect(&rcDlg, -rcDlg.left,   -rcDlg.top);
    OffsetRect(&rcAdj, -rcAdj.left,   -rcAdj.top);
    OffsetRect(&rcAdj, -rcDlg.right,  -rcDlg.bottom);

    SetWindowPos(hDlg, (HWND)0xffffffff,
                 rcParent.left + rcAdj.right  / 2,
                 rcParent.top  + rcAdj.bottom / 2,
                 0, 0, 1);   /* 1 = SWP_NOSIZE */
}


/* The Save As picker: a GetSaveFileNameA dialog that returns the boot-relative path and the file
 * name as a Pascal string (31 characters). Returns short (ax): non-zero when a file was chosen. */
short __cdecl PickSaveFileName(char *bootRelativeOut, char *pascal31FileOut) /* 0x00420840 */
{
    BOOL ok;
    int  lenFull;
    int  slash;

    /* The exe returns short (ax: `xor ax,ax` / `mov ax,1`); this stays
     * unsigned int until scriptcmd.c's externs, which test the full eax,
     * are retyped with it. Title copied with strcpy (strlen+1 bytes). */
    DF_TRACE("savepick: init static");
    InitOpenFileNameStaticOnce();
    strcpy(gSaveFileFilter, GetStringResource(1));
    ApplyOpenFileNameTitleFilterPair(gSaveFileFilter);
    gOpenFileName.Flags |= 0x20000e;
    SuspendGameUiForModalDialog();
    ok = GetSaveFileNameA(&gOpenFileName);
    DF_TRACE("savepick: GetSaveFileNameA -> %d", (int)ok);
    RestoreGameUiAfterModalDialog();
    if (!ok)
        return 0;
    if (!NormalizeOpenFileSelectionAfterOk())
        return 0;
    lenFull = strlen(gOpenFilePath);
    /* last backslash, searched from the final character (0x004208de) */
    for (slash = lenFull - 1; slash >= 0; slash--) {
        if (gOpenFilePath[slash] == 0x5c)
            break;
    }
    if (slash < 0)
        FatalError(0, (short)0x545);
    strcpy(gOpenFileInitialDir, gOpenFilePath);
    gOpenFileInitialDir[slash] = '\0';
    ComposeVolumeDescriptorFromPath(gOpenFilePath, bootRelativeOut);
    slash++;
    /* the name as a Pascal string; strcpy also leaves a NUL after the body */
    strcpy(pascal31FileOut + 1, gOpenFilePath + slash);
    pascal31FileOut[0] = (char)(lenFull - slash);
    return 1;
}


/* Fills the static OPENFILENAME (gOpenFileName) once: the 256-byte path buffer, gCoverWindow as the
 * owner, the initial directory (SetOpenFileInitialDirToModuleDir) and filter index 1. */
void __stdcall InitOpenFileNameStaticOnce(void) /* 0x00420990 */
{
    if (gOpenFileNameInited != 0)
        return;
    gOpenFileNameInited = 1;

    memset(&gOpenFileName, 0, sizeof(gOpenFileName));
    gOpenFileName.lStructSize = sizeof(gOpenFileName);
    gOpenFileName.nFilterIndex = 1;
    gOpenFileName.lpstrFile = gOpenFilePath;
    gOpenFileName.nMaxFile = 256;
    gOpenFileName.hwndOwner = gCoverWindow;
    gOpenFileName.lpstrFileTitle = NULL;
    gOpenFileName.nMaxFileTitle = 0;
    gOpenFileName.lpstrInitialDir = gOpenFileInitialDir;
    SetOpenFileInitialDirToModuleDir(0xff, gOpenFileInitialDir);
}


/* Was a memset stub. The exe fills the buffer with the module's own path and
 * cuts it at the last backslash: the open-file dialog starts in the exe's
 * directory. FatalError 0x506 if the name cannot be read, 0x507 if it has
 * no backslash at all. */
void __cdecl SetOpenFileInitialDirToModuleDir(DWORD cbBuf, char *dirBuf) /* 0x00420a20 */
{
    char  *pLastSep;

    if (GetModuleFileNameA(gInstance, dirBuf, cbBuf) == 0)
        FatalError(0, 0x506);
    pLastSep = NULL;
    for (;;) {
        if (*dirBuf == '\\')
            pLastSep = dirBuf;
        if (*dirBuf == '\0')
            break;
        dirBuf++;
    }
    if (pLastSep == NULL)
        FatalError(0, 0x507);
    *pLastSep = '\0';
}


/* Mutates buffer in-place: '!' becomes '\0' so it matches OFN lpstrFilter layout. */
void __cdecl ApplyOpenFileNameTitleFilterPair(char *titleFilterScratch) /* 0x00420a80 */
{
    int i;
    int n;

    /* the exe's shape: the length taken once, inlined strcpy copies */
    n = strlen(titleFilterScratch);
    for (i = 0; i < n; i++) {
        char *p = titleFilterScratch + i;
        if (*p == '!')
            *p = 0;
    }

    gOpenFileName.lpstrFilter = titleFilterScratch;
    gOpenFileName.lpstrDefExt = gOpenFileDefExt;
    strcpy(gOpenFilePath, titleFilterScratch + strlen(titleFilterScratch) + 1);
    strcpy(gOpenFileDefExt, gOpenFilePath + 2);
    _strupr(gOpenFileDefExt);

    gOpenFileName.lpfnHook = OpenFileNameModalHook;
    if (gIsWin95OrNT4 != 0)
        gOpenFileName.Flags = 0x80020;
    else
        gOpenFileName.Flags = 0x20;
}


/* After the user clicks OK: rejects a path that contains '?' or has no backslash (returns 0), splits
 * the selection into directory, stem and extension, trims leading and trailing spaces and rebuilds
 * it so that the extension is always gOpenFileDefExt: an extension that is not the expected one is
 * kept as part of the NAME ("mysave.dat" -> "mysave.dat.ti"). Returns 1. */
int __stdcall NormalizeOpenFileSelectionAfterOk(void) /* 0x00420b50 */
{
    /* The exe's own shape (byte-exact): every string operation is an inlined
     * strlen/strcpy/strcat with no bounds; the port's strncpy/sprintf length
     * guards had no DF_FIX and are gone. */
    char stem[256];
    char ext[256];
    char dir[256];
    int  i;
    int  sep;
    int  dot;
    char *p;

    for (i = 0; i < (int)strlen(gOpenFilePath); i++) {
        if (gOpenFilePath[i] == '?')
            return 0;
    }
    for (sep = strlen(gOpenFilePath) - 1; sep >= 0; sep--) {
        if (gOpenFilePath[sep] == 0x5c)
            break;
    }
    if (sep < 0)
        return 0;
    sep++;
    memcpy(dir, gOpenFilePath, sep);
    dir[sep] = 0;
    strcpy(stem, gOpenFilePath + sep);
    for (dot = strlen(stem) - 1; dot >= 0; dot--) {
        if (stem[dot] == '.')
            break;
    }
    if (dot >= 0) {
        p = stem + dot;
        *p = 0;
        strcpy(ext, p + 1);
    } else
        ext[0] = 0;
    /* leading spaces: an overlapping forward strcpy, as the exe's inline rep movs */
    while (stem[0] == ' ')
        strcpy(stem, stem + 1);
    while (stem[0] != 0) {
        if (stem[strlen(stem) - 1] != ' ')
            break;
        stem[strlen(stem) - 1] = 0;
    }
    while (ext[0] != 0) {
        if (ext[strlen(ext) - 1] != ' ')
            break;
        ext[strlen(ext) - 1] = 0;
    }
    /* 0x00420d3d: an extension that is not the expected one is kept as part of
     * the NAME ("mysave.dat" -> "mysave.dat.ti"). kStrDot is ".". */
    if (_stricmp(ext, gOpenFileDefExt) != 0) {
        strcat(stem, kStrDot);
        strcat(stem, ext);
        strcpy(ext, gOpenFileDefExt);
    }
    /* dir + stem + "." + ext (the separator goes AFTER the stem; see history) */
    strcat(stem, kStrDot);
    strcat(stem, ext);
    strcpy(gOpenFilePath, dir);
    strcat(gOpenFilePath, stem);
    return 1;
}


/* The Open picker: copies the title / filter pair for the asset kind ('VOOM', 'RATS', 'TXET' read
 * as dwords: string resources 0x96, 0x97, 0x95, otherwise 1) into gOpenFileFilter, shows
 * GetOpenFileNameA with the game UI suspended, normalises the selection
 * (NormalizeOpenFileSelectionAfterOk), remembers its directory as the next initial directory and
 * returns the boot-relative volume descriptor (ComposeVolumeDescriptorFromPath) in bootRelativeOut
 * and the file name as a Pascal string in pascal31FileOut. Returns 1 if a file was chosen, else 0;
 * fatal 0x546 if the path has no backslash. The exe's fifth argument is never read (the caller at
 * 0x00413957 pushes 1). */
short __cdecl PickOpenFileName(char *bootRelativeOut, char *pascal31FileOut,
                                                          unsigned int unusedAux, int *pAssetKindFourcc,
                                                          int unusedArg5) /* 0x00420ec0 -- the caller at 0x00413957 pushes 1, the body reads four */
{
    BOOL ok;
    int  lenFull;
    int  slash;

    /* The exe returns short (ax); see the save picker above. */
    (void)unusedAux;
    InitOpenFileNameStaticOnce();

    /* title/filter pair by asset kind ('VOOM', 'RATS', 'TXET' read as dwords);
     * each case copies its own string, as the exe's switch pushes the id */
    switch (*pAssetKindFourcc) {
    case 0x4d4f4f56:
        strcpy(gOpenFileFilter, GetStringResource(0x96));
        break;
    case 0x53544152:
        strcpy(gOpenFileFilter, GetStringResource(0x97));
        break;
    case 0x54455854:
        strcpy(gOpenFileFilter, GetStringResource(0x95));
        break;
    default:
        strcpy(gOpenFileFilter, GetStringResource(1));
        break;
    }
    ApplyOpenFileNameTitleFilterPair(gOpenFileFilter);
    gOpenFileName.lpstrFilter = gOpenFileFilter;
    gOpenFileName.Flags |= 0x20180c;

    SuspendGameUiForModalDialog();
    ok = GetOpenFileNameA(&gOpenFileName);
    RestoreGameUiAfterModalDialog();
    if (!ok)
        return 0;
    if (!NormalizeOpenFileSelectionAfterOk())
        return 0;

    /* last backslash, searched from the final character (0x00420f98) */
    lenFull = strlen(gOpenFilePath);
    for (slash = lenFull - 1; slash >= 0; slash--) {
        if (gOpenFilePath[slash] == 0x5c)
            break;
    }
    if (slash < 0)
        FatalError(0, (short)0x546);
    strcpy(gOpenFileInitialDir, gOpenFilePath);
    gOpenFileInitialDir[slash] = '\0';
    ComposeVolumeDescriptorFromPath(gOpenFilePath, bootRelativeOut);
    slash++;
    /* the name as a Pascal string; strcpy also leaves a NUL after the body */
    strcpy(pascal31FileOut + 1, gOpenFilePath + slash);
    pascal31FileOut[0] = (char)(lenFull - slash);
    return 1;
}


/* TEXTDLG: the text-entry prompt behind the `textdialog(prompt, default)` script command.
 * Ground truth 0x00421040 (not a function in the Ghidra database: it is only ever passed to
 * DialogBoxParamA). The prompt goes in control 0x67; on OK (0x65) the edit control 0x68 is
 * read into gTextDialogReplyDesc as a Pascal string and gTextDialogStatus is 1, or 2 if it is empty;
 * Cancel (0x66) leaves 2. The port had invented a two-pane Yes/No viewer here. */
INT_PTR CALLBACK TextEntryDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) /* 0x00421040 */
{
    char prompt[256];

    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowTextA(hDlg, gAppTitle);
        PascalStringToCString((byte *)gTextEntryPrompt, prompt);
        SetDlgItemTextA(hDlg, 0x67, prompt);
        CenterDialogInMainWindow(hDlg);
        return TRUE;
    case WM_COMMAND:
        if (wp == 0x65) {
            gTextDialogStatus = 1;
            GetDlgItemTextA(hDlg, 0x68, (LPSTR)gTextDialogReplyDesc, 0x100);
            EncodePascalString((char *)gTextDialogReplyDesc, (undefined1 *)gTextDialogReplyDesc);
            if (*(char *)gTextDialogReplyDesc == 0)
                gTextDialogStatus = 2;
            EndDialog(hDlg, 1);
            return TRUE;
        }
        if (wp == 0x66) {
            gTextDialogStatus = 2;
            EndDialog(hDlg, 1);
            return TRUE;
        }
        break;
    }
    return FALSE;
}


/* `textdialog`: prompt pDescA, the typed reply written into pDescB; 1 if one was entered. */
short __cdecl RunTextEntryDialog(void *pDescA, void *pDescB) /* 0x00421160: returns ax (mov ax,cx) */
{
    SuspendGameUiForModalDialog();
    gTextDialogReplyDesc = pDescB;
    gTextEntryPrompt = pDescA;
    DialogBoxParamA((HINSTANCE)(uintptr_t)gInstance, (LPCSTR)kStrTEXTDLG, gMainWindow,
                    (DLGPROC)TextEntryDlgProc, 0);
    RestoreGameUiAfterModalDialog();
    return gTextDialogStatus == 1;
}





/* Shows an information box (0x2040: MB_OK | MB_ICONINFORMATION | MB_TASKMODAL) for a Pascal string,
 * word-wrapped, under the application title, with the game UI suspended around it. */
void ShowEngineMessageBoxFromPascalString(byte *pPascal) /* 0x004211b0 */
{
    byte   b;
    char   cbuf[256];
    char   wbuf[256];

    b = *pPascal;
    memmove(cbuf, (char *)pPascal + 1, (unsigned int)b);
    cbuf[b] = '\0';
    WordWrapText(cbuf, wbuf);
    SuspendGameUiForModalDialog();
    MessageBoxA((HWND)NULL, wbuf, (LPCSTR)gAppTitle, 0x2040u);
    RestoreGameUiAfterModalDialog();
}


/* Shows a Pascal string as a Yes/No box (0x2024: MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL) under
 * the application title, word-wrapped, with the game UI suspended around it. Returns whether the
 * answer was Yes (IDYES = 6). */
short __cdecl RunDescriptorYesNoDialog(void *pDesc)                       /* 0x00421220 */
{
    byte len;
    char msgText[256];  /* local_200 */
    char wrappedText[256];  /* local_100 */
    int  rc;

    len = *(byte *)pDesc;
    memmove(msgText, (byte *)pDesc + 1, len);
    msgText[len] = '\0';
    WordWrapText(msgText, wrappedText);
    SuspendGameUiForModalDialog();
    /* NULL owner confirmed against ground truth (0x00421220) -- do not
     * "fix" this without live re-confirmation. A prior black-dialog
     * symptom traced to this call turned out to be caused by repeated
     * `Stop-Process -Force` kills during testing corrupting the *real*
     * Windows system colours that SaveSystemColors()/ManageSystemPalette
     * (palette.c) save-and-restore around every modal dialog -- so do
     * not force-kill the game while testing it. An owner-window change was tried
     * here and did NOT fix it either; reverted. */
    rc = MessageBoxA(NULL, wrappedText, (LPCSTR)&gAppTitle, 0x2024);
    RestoreGameUiAfterModalDialog();
    return rc == 6;
}

