/* Declarations for the "palette" topic: the preamble and shared definitions of the functions grouped under that name. */
#ifndef DF_TOPIC_PALETTE_H
#define DF_TOPIC_PALETTE_H

#include <windows.h>
#include <math.h>

/* Parallel palette tables */
#ifndef DF_R_DAT_00487540  /* VC++ only: a member of a region object, a macro */
extern PAL16ENTRY   gPal16[256];          /* 0x0048754a */
#else
extern int DAT_00487540_r[514];
#endif
#ifdef _MSC_VER   /* the GCC build packs the palette as one object (df_globals.h, g_logPalImage) */
#ifndef DF_R_DAT_00487d48  /* VC++ only: a member of a region object, a macro */
extern PALETTEENTRY gPalEntries[256];     /* 0x00487d4c -- for AnimatePalette / SetPaletteEntries */
#else
extern int gLogPal_r[257];
#endif
#endif

/* -- from between the functions -- */
extern int        gPalOwnerState;        /* 0x0045b6ec */
extern int  kSysPalIDs[25];        /* 0x0045b620 */
extern COLORREF   gSavedSysColors[25];   /* 0x00489c10 */
extern COLORREF kGameSysColors[25]; /* 0x0045b688 */
extern int s_sysColorsSaved;
typedef UINT (WINAPI *PFN_SetDIBColorTable)(HDC, UINT, UINT, const RGBQUAD *);
extern short gGammaBaseR[256];    /* 0x004899e8 -- gamma base,  red   channel */
extern short gGammaSlopeR[256];   /* 0x00486568 -- gamma slope, red   channel */
extern short gGammaBaseG[256];    /* 0x004883d0 -- gamma base,  green channel */
extern short gGammaSlopeG[256];   /* 0x00485f60 -- gamma slope, green channel */
extern short gGammaBaseB[256];    /* 0x00487338 -- gamma base,  blue  channel */
extern short gGammaSlopeB[256];   /* 0x00486c60 -- gamma slope, blue  channel */
#define GAMMA_IN_SCALE   (1.0 / 255.0)
#define GAMMA_OUT_SCALE  255.0
extern long   __ftol(void);   /* MSVC FPU->long helper */
extern HPALETTE gPalette;   /* 0x00485f4c */
#ifdef _MSC_VER   /* the GCC build packs the palette as one object (df_globals.h, g_logPalImage) */
#ifndef DF_R_DAT_00487d48  /* VC++ only: a member of a region object, a macro */
extern LOGPALETTE gLogPal;   /* 0x00487d48 */
#else
extern int gLogPal_r[257];
#endif
#endif
#endif
