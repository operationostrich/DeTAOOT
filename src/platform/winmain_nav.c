/* winmain_nav.c -- TI.EXE 0x00431ef0..0x00432570: WinMain, the navigation event hooks and the edit-variable dialog.
 * .data has this file's literals end with "EDITVAR" (0x0045da20), then initialised arrays, which
 * VC++ 4.2 emits before an object's literals -- so the next file (heap_resources.c) starts there. */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <ctype.h>
#include <stddef.h>
#include "df_protos.h"
#include "topics/scriptcmd.h"
#include "topics/gameloop.h"
#include "topics/scene.h"
#include "topics/winmain.h"
#include "topics/anim.h"
#include "topics/multimedia.h"
#include "topics/container.h"
#include "topics/dialog.h"
#include "topics/sound.h"
#include "topics/checkcd.h"
#include "topics/heap.h"
#include "topics/resource.h"
#include "topics/renderer.h"
#include "topics/nodestore.h"
#include "topics/fatal.h"


/* The ExportClut action command: a no-op in this build. cdecl with the four handler arguments,
 * none read: the dispatcher (0x0043c2c0) pushes and pops them itself. Leaves the skip count unwritten. */
short __cdecl ActionCmd_ExportClut(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)   /* 0x00431ef0 */
{
    return 0;
}


/* Copies the machine-type name kStrWIN ("WIN") into pBuf as a Pascal string. */
void __cdecl CopyMachineTypeName(void *pBuf)                         /* 0x00431f00 */
{
    CopyPascalString((byte *)UpdatePascalLength((char *)kStrWIN), (byte *)pBuf);
}


/* Returns SYSTEM_INFO.dwProcessorType, or 0x182 (PROCESSOR_INTEL_386) when KERNEL32 does not
 * export GetSystemInfo. */
DWORD __stdcall QueryProcessorType(void)                            /* 0x00431f20 */
{
    HMODULE    hMod;
    SYSTEM_INFO si;

    /* GetProcAddress only gates the call: the exe calls the GetSystemInfo
     * import directly, as DetectCPUType does, and has no NULL check on hMod */
    hMod = LoadLibraryA("KERNEL32.DLL");
    if (GetProcAddress(hMod, "GetSystemInfo") != NULL) {
        GetSystemInfo(&si);
        FreeLibrary(hMod);
        return si.dwProcessorType;
    }
    FreeLibrary(hMod);
    return 0x182;
}


/* Looks up the window record at event +2 in gViewportTable; if present, fills *pDesc from it with
 * FetchHotspotDesc and returns 0 (proceed), else returns 1. Used for event types 6 (navigation
 * with state) and 8 (drag / grab-move). */
short __cdecl FindEventViewportDesc(NodeDesc *pDesc, GameEvent *pEv)  /* 0x00431f70 */
{
    int    i;
    void **p;
    void  *pTarget;

    pTarget = *(void **)((char *)pEv + 2);
    i = 0;
    if (gViewportCount > 0) {
        p = (void **)gViewportTable;
        do {
            if (*p == pTarget)
                break;
            p++;
            i++;
        } while (i < gViewportCount);
    }
    if (gViewportCount == i)
        return 1;
    /* the exe's `return FetchHotspotDesc(..) & 0xffff0000` is 0 (class 18); the call must stay */
    FetchHotspotDesc((HWND *)pTarget, pDesc);
    return 0;
}


/* Gate before every action-code 3-8 handler in the event switch: always 1 (allowed) in this build. */
short __cdecl IsInteractionAllowed(NodeDesc *pNode)  /* 0x00431fc0 */
{
    return 1;
}


/* Event type 7 (quit / close requested): empty in this build. */
void __cdecl HandleQuitRequest(GameEvent *pEv)  /* 0x00431fd0 */
{
}


/* Event type 0x17 (23): empty in this build. */
void __cdecl HandleEvent23(GameEvent *pEv)  /* 0x00431fe0 */
{
}


/* Guard before entering a sub-view node: 0 proceeds with the channel message, non-zero suppresses
 * it. Always 0 in this build. */
short __cdecl CheckSubviewEntry(NodeDesc *pNode, GameEvent *pEv)  /* 0x00431ff0 */
{
    return 0;
}


/* The pan / scroll delta packed as low 16 = dx, high 16 = dy. Always 0: no scrolling in this build. */
int __cdecl GetScrollDelta(NodeDesc *pNode, GameEvent *pEv)  /* 0x00432000 */
{
    return 0;
}


/* Action code 6 (tertiary action on a node): hides the node's window. The caller also pushes the
 * event, which is never read. */
void __cdecl HideHitNodeWindow(NodeDesc *pNode, GameEvent *pEv)  /* 0x00432010 */
{
    HideChannelWindow((int)pNode);
}


/* Action code 4 (secondary action on a node): empty in this build. */
void __cdecl SecondaryNodeAction(NodeDesc *pNode, GameEvent *pEv)  /* 0x00432020 */
{
}


/* Action code 2 (cancel / back) from HitTestHotspot: empty in this build. */
void __cdecl NavigateBack(NodeDesc *pNode, GameEvent *pEv)  /* 0x00432030 */
{
}


/* Returns the menu command id an event navigates to, or 0.
 * flag 0 (keyboard): the key in field_2, upper-cased, looked up in gNavCommandTable (8-byte
 * entries: key byte at +2, command ushort at +4).
 * flag != 0 (hotspot): if the low word of pNode (the Y from GetSceneCursorPos) is the sentinel
 * -1000, the high word (X) is the command id itself. */
unsigned int __cdecl ResolveNavCommandId(GameEvent *pEv, short flag)  /* 0x00432040 */
{
    unsigned short  key;
    byte           *pEntry;
    int             i;

    if (flag != 0) {
        /* read signed: the exe's movsx */
        if (*(short *)&pEv->pNode == -1000)
            return *((short *)&pEv->pNode + 1);
    } else {
        key = (unsigned short)pEv->field_2;
        if (key >= 0x61 && key <= 0x7a)
            key -= 0x20;   /* ASCII a-z -> A-Z */
        i = 0;
        if (gNavCommandCount > 0) {
            pEntry = gNavCommandTable + 2;
            do {
                /* the whole 16-bit key against the entry byte (movzx / cmp ebx,eax) */
                if (*pEntry == key)
                    return *(unsigned short *)(gNavCommandTable + 4 + i * 8);
                pEntry += 8;
                i++;
            } while (i < gNavCommandCount);
        }
    }
    return 0;
}


/* Called after MapScreenPointToNode on hit codes 7 and 8, with the code as op: empty in this build. */
void __cdecl PerformNodeOperation(NodeDesc *pNode, GameEvent *pEv, short op)  /* 0x004320b0 */
{
}


/* Composes the boot volume descriptor in pState from the exe's directory, with a trailing
 * backslash. InitScriptRuntime calls it to reset gScratchStr. */
void __cdecl ComposeModuleDirBootPath(char *pState)  /* 0x004320c0 */
{
    char  modulePath[256];  /* local_100 */

    GetModuleDirectory(0x100, modulePath);

    /* strlen / strcat inline in the exe (repne scasb, rep movsd); no empty-string guard: the
     * directory is never empty */
    if (modulePath[strlen(modulePath) - 1] != '\\')
        strcat(modulePath, kStrBackslash);

    ComposeVolumeDescriptorFromPath(modulePath, pState);
}

/* Writes the exe's directory (no trailing backslash) into pBuf. FatalError 0x1142 if
 * GetModuleFileNameA fails, 0x1143 if the path has no backslash. */
void __cdecl GetModuleDirectory(DWORD bufLen, char *pBuf) /* 0x00432140 */
{
    DWORD  nameLen;
    char  *pLastSlash;

    nameLen = GetModuleFileNameA((HINSTANCE)(uintptr_t)gInstance, pBuf, bufLen);
    if (nameLen == 0)
        FatalError(0, 0x1142);
    pLastSlash = (char *)0x0;
    while (1) {
        if (*pBuf == '\\') pLastSlash = pBuf;
        if (*pBuf == '\0') break;
        pBuf++;
    }
    if (pLastSlash == (char *)0x0)
        FatalError(0, 0x1143);
    *pLastSlash = '\0';
}


/* Sets the scene rect {top, left, bottom, right} to the whole screen (event type 0x2a, attract mode). */
void __cdecl ResetSceneRectToScreen(short *pState)  /* 0x004321a0 */
{
    pState[0] = 0;
    pState[1] = 0;
    pState[3] = (short)GetSystemMetrics(SM_CXSCREEN);
    pState[2] = pState[0] + (short)GetSystemMetrics(SM_CYSCREEN);
}


/* The program: AppInit, the CLUT palettes, RunGameLoop, then teardown. Always returns 0.
 * nShowCmd is never read (Ghidra shows three parameters). The CRT entry calls WinMain by name;
 * the port once had it behind a one-line wrapper the original does not have. */
int __stdcall WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR     lpCmdLine,
                   int       nShowCmd)  /* 0x004321d0 */
{
    /* the two parameters AppInit reads, adjacent on the stack: it takes a pointer to both */
    struct {
        HINSTANCE hInstance;   /* local_8 = param_1 */
        LPSTR     lpCmdLine;   /* local_4 = param_3 */
    } initArgs;

    initArgs.hInstance  = hInstance;
    initArgs.lpCmdLine  = lpCmdLine;

    AppInit(&initArgs);
    InitCLUTPalettes();
    RunGameLoop();
    FreeCLUTPalettes();
    Shutdown();
    DF_COVERAGE_FLUSH();                /* trace build: final df_coverage.txt */

    return 0;
}

/* Navigates by menu command: takes the menu item's text, drops the '&' markers and everything
 * from the tab on (the accelerator hint), and sends it as a Pascal string with channel message
 * 0xc (navigate to the named scene) on channel B. */
void __cdecl DispatchNavCommand(unsigned int menuId)  /* 0x00432210 */
{
    byte  pascalBuf[256];   /* [0] = length, [1..] = name body */
    char  menuStr[256];     /* raw string from GetMenuStringA */
    char *pSrc;
    byte *pDst;

    if (GetMenuStringA(gMainMenu, menuId, menuStr, 0xff, 0) != 0) {
        pSrc = menuStr;
        pDst = pascalBuf + 1;
        for (; *pSrc != 0 && *pSrc != 9; pSrc++) {      /* 9 = tab, 0x26 = '&' */
            if (*pSrc != 0x26)
                *pDst++ = *pSrc;
        }
        pascalBuf[0] = (byte)(pDst - pascalBuf) - 1;
        DispatchChannelMessage((short *)&gChannelB, 0xc, pascalBuf);
    }
}


/* Opens the "Messages" window (class DFWINDOW), allocates its 0x74-byte window record (HWND,
 * HDC, then the hotspot bitmap state), stores the record in *ppWindowRec and appends it to
 * gViewportTable. Style 0x80ca0000 = WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX; ex-style
 * 0x88 = WS_EX_TOOLWINDOW|WS_EX_TOPMOST, retried as plain WS_EX_TOPMOST if that fails. */
void __cdecl OpenAnimWindow(int *ppWindowRec)  /* 0x00432290 */
{
    int   slot;
    HWND  hWnd;
    void *pWindowRec;
    HDC   hdc;

    hWnd = CreateWindowExA(0x88, "DFWINDOW", "Messages", 0x80ca0000,
                           100, 100, 100, 100,
                           (HWND)0x0, (HMENU)0x0, gInstance, (LPVOID)0x0);
    if (hWnd == (HWND)0x0) {
        hWnd = CreateWindowExA(8, "DFWINDOW", "Messages", 0x80ca0000,
                               100, 100, 100, 100,
                               (HWND)0x0, (HMENU)0x0, gInstance, (LPVOID)0x0);
    }

    pWindowRec = (void *)AllocEngineBuffer(0x74);
    *(HWND *)pWindowRec = hWnd;
    hdc = GetDC(hWnd);
    *((HDC *)pWindowRec + 1) = hdc;
    InitHotspotBitmapState((int *)pWindowRec + 2);

    *ppWindowRec = (int)pWindowRec;
    slot = gViewportCount;
    gViewportCount = gViewportCount + 1;
    gViewportWasVisible[slot] = 0;
    ((int *)&gViewportTable)[slot] = (int)pWindowRec;
}


/* OpenAnimWindow for the "Debug" window; style 0x80f40000 =
 * WS_POPUP|WS_CAPTION|WS_VSCROLL|WS_HSCROLL|WS_THICKFRAME. */
void __cdecl OpenMultimediaWindow(int *ppWindowRec) /* 0x00432340 */
{
    int    slot;
    HWND   hWnd;
    void  *pWindowRec;
    HDC    hdc;

    hWnd = CreateWindowExA(0x88, "DFWINDOW", "Debug", 0x80f40000,
                           100, 100, 100, 100,
                           (HWND)0, (HMENU)0, gInstance, (LPVOID)0);
    if (hWnd == (HWND)0) {
        hWnd = CreateWindowExA(8, "DFWINDOW", "Debug", 0x80f40000,
                                 100, 100, 100, 100,
                                 (HWND)0, (HMENU)0, gInstance, (LPVOID)0);
    }
    pWindowRec = AllocEngineBuffer(0x74);
    *(HWND *)pWindowRec   = hWnd;
    hdc = GetDC(hWnd);
    *((HDC *)pWindowRec + 1) = hdc;
    InitHotspotBitmapState((int *)pWindowRec + 2);
    *ppWindowRec = (int)pWindowRec;
    slot = gViewportCount;
    gViewportCount = gViewportCount + 1;
    gViewportWasVisible[slot] = 0;
    ((int *)&gViewportTable)[slot] = (int)pWindowRec;
}


/* Calls a plugin directory entry's PlugProc (entry +0x14) with the message block and the engine's
 * own command dispatcher, DispatchPluginEngineRequest (clut_rects_resources.c), so the plugin can
 * call back in.
 *
 * Port note: the port once declared this (int pEntry) and called the slot with no arguments, so
 * no plugin call received its message block. */
uint __cdecl InvokeEntryCallback(void *pEntry, DfPluginMsg *pMsg)  /* 0x004323f0 */
{
    typedef uint (__cdecl *DfEntryProc)(DfPluginMsg *, void *);

    return (*(DfEntryProc *)((char *)pEntry + 0x14))(pMsg, (void *)DispatchPluginEngineRequest);
}


/* For LoadPluginDirectory: joins the directory prefix and entry name, resolves the path with
 * GetResourcePath and, if it contains ".DLL" (case-insensitive), loads it and binds its
 * "PlugProc" export into the entry (+0x10 HMODULE, +0x14 PlugProc).
 * Returns 0 on success, 0x33 if the file is not a DLL, does not load or exports no PlugProc. */
short __cdecl TryBindSceneDirectoryDll(byte *pDirPrefixDesc, byte *pEntryNameDesc,
                                   void *pEntryStruct) /* 0x00432410 */
{
    byte        winPath[256];
    byte        combined[256];
    HMODULE     hModule;

    /* winPath is declared first: it sits below `combined` in the exe's frame */
    CopyPascalString(pDirPrefixDesc, combined);
    AppendPascalString(pEntryNameDesc, combined);
    GetResourcePath(combined, winPath);
    _strupr((char *)winPath);

    /* ".DLL" is kStrDllExt in the exe */
    if (strstr((char *)winPath, ".DLL") == NULL)
        return 0x33;

    hModule = LoadLibraryA((LPCSTR)winPath);
    if (hModule == NULL)
        return 0x33;

    *(FARPROC *)((char *)pEntryStruct + 0x14) = GetProcAddress(hModule, "PlugProc");
    if (*(FARPROC *)((char *)pEntryStruct + 0x14) == NULL) {
        FreeLibrary(hModule);
        return 0x33;
    }

    *(HMODULE *)((char *)pEntryStruct + 0x10) = hModule;
    return 0;
}


/* Frees a plugin directory entry's DLL (HMODULE at +0x10). Always returns 0: the exe's
 * `FreeLibrary(..) & 0xffff0000` (class 18). pEntry is a pointer; the caller re-forms &entry at
 * each call. */
short __cdecl FinalizeDirectoryEntry(void *pEntry)  /* 0x004324f0 */
{
    return (short)(FreeLibrary(*(HMODULE *)((char *)pEntry + 0x10)) & 0xffff0000);
}


/* Draws the right-pointing triangle marker (x 4..12) for the playback row at y, pen mode 1. */
void __cdecl DrawPlaybackPositionMarker(short y)  /* 0x00432510 */
{
    QdPenNormal();
    QdPenMode(1);
    QdMoveTo(4, y - 8);
    QdLineTo(0xc, y);
    QdLineTo(4, y + 8);
    QdLineTo(4, y - 8);
    QdPenNormal();
}

/* Runs the EDITVAR debug dialog on a script variable (record, value and name handed to
 * EditVariableDialogProc through globals) and returns DialogBoxParamA's result. */
int __cdecl ShowEditVariableDialog(uint32_t pEntry,uint32_t pValue,uint32_t pName)  /* 0x00432570 */

{
  gEditVarRecord = pEntry;
  gEditVarValue = pValue;
  gEditVarText = pName;
  /* Ghidra types this void, but EditDebuggerVariable tests ax after the call: returning the value
   * DialogBoxParamA leaves in eax compiles to the same bytes */
  return (int)DialogBoxParamA(gInstance,"EDITVAR" /* 0x0045da20 */,gMainWindow,(DLGPROC)EditVariableDialogProc,0);
}
