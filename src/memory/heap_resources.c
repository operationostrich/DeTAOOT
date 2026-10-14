/* heap_resources.c -- TI.EXE 0x004325b0..0x00434cd0: the debug variable dialog, CD prompts, the heap,
 * the descriptor pool and resource objects.
 * The include list is longer than the code needs: the declaration count is part of the byte match. */

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

static void __cdecl InitDescriptorPool(int nBuckets);
static void __cdecl InitHeapDescriptor(int *pDesc, int pvPool, int cbPool);
static void __stdcall FreeDescriptorPages(void);
static int * __cdecl AllocHeapBlock(int heapDesc, int cbSize);
static void __cdecl CarveFromBlock(int **ppBlock, int cbSize);
static int * __cdecl FindFreeBlock(int heapDesc, int cbSize);

/* TI.EXE's TLS block (every FS:[2c] access in the exe): LIBCMT's 8-byte _tls_start, then
 * +8 the active descriptor, +0xc a 33-entry stack used 1-indexed (PushHeapContext stores at
 * +0xc+4*depth, depth <= 0x20), +0x90 the depth. One struct: as separate variables VC++ 4.2
 * 8-aligns the array to +0x10. */
__thread HeapTls g_heapTls;
#ifdef _MSC_VER   /* section-defined, not COMMON: the link build lays them out in this unit's bss block */
#pragma bss_seg(".bss")
#endif
void *gDescPageTail;    /* DAT_0048a9a0 -- most-recently-allocated descriptor page */
void *gDescPageHead;    /* DAT_0048a9f8 -- first descriptor page (teardown chain start) */
int  *gDescFreeList;    /* DAT_0048a9f4 -- free-descriptor pool head (linked via +0x04) */
int   gHeapPageCount;   /* DAT_0048a9fc -- total descriptor pages allocated */
int  *gDescLastEntry;   /* DAT_0048a9a4 -- last descriptor on most-recent page */
#ifdef _MSC_VER
#pragma bss_seg()
#endif

/* Two of the dialog's captions. char ARRAYS: they sit 8-aligned in .data, as arrays of
 * 8+ bytes do. "Local \"" and "Global \"" (4-aligned among this file's literals) are
 * literals in EditVariableDialogProc; their (char *) cast keeps the `repne scasb` copy.
 * Defined here, in their only user, and kept out of df_globals.h on purpose:
 * an extern there is one more declaration in every unit's VC++ 4.2 compile and
 * shifted register choice tree-wide (src/data/globals.c). */
char gFalseCaption[6] = "False";       /* 0x0045da28 */
char gTrueCaption[5] = "True";        /* 0x0045da30 */

/* Dialog procedure of the EDITVAR debug dialog: edits one script variable. The title is
 * `Global "name"` or `Local "name"` (by whether the record's scope is gGlobalsNode); edit
 * control 100 holds the value text, written back to gEditVarText on OK. Radio buttons
 * 0x65..0x68 pick the record's kind 0..2 or "a value" (0x68), which enables edit 0x69 and
 * stores it as kind 5 (true/false), 3 (a decimal number) or 4 (a string).
 * gEditVarValue's type word: 2 boolean, 3 string, 4 number.
 *
 * Port note: gEditVarRecord and gEditVarValue are arrays of shorts (the exe adds 2, 0x12,
 * 0x16, 0x1a bytes); the port once applied Ghidra's short offsets as byte offsets to these
 * byte * globals, so every field the dialog touched was the wrong one. */
INT_PTR CALLBACK EditVariableDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) /* 0x004325B0 */
{
    char  buf[260];
    char  num[260];
    byte  name[64];
    short err;
    uint32_t parsed;

    (void)lParam;

    switch (msg) {
    case WM_CLOSE:
        EndDialog(hDlg, 1);
        break;

    case WM_INITDIALOG:
        CenterWindowRelative(hDlg);
        CopyPascalString((byte *)((short *)gEditVarRecord + 1), name);
        name[(char)name[0] + 1] = 0;        /* movsx: a signed length */
        /* literals, not arrays: "Local" and "Global" sit 4-aligned among this file's
         * literal COMDATs in TI.EXE, after '"' (0x0045da38). The (char *) cast keeps
         * VC++ 4.2 from folding strcpy's length, so the copy stays repne scasb. */
        if (*(void **)((short *)gEditVarRecord + 9) == gGlobalsNode)
            strcpy(buf, (char *)"Global \"" /* 0x0045da44 */);
        else
            strcpy(buf, (char *)"Local \"" /* 0x0045da3c */);
        strcat(buf, (char *)name + 1);
        strcat(buf, kStrQuoteC);
        SetWindowTextA(hDlg, buf);
        switch (*(short *)gEditVarValue) {
        case 2:
            if (*(int *)((short *)gEditVarValue + 1) != 0)
                strcpy(buf, gTrueCaption);
            else
                strcpy(buf, gFalseCaption);
            break;
        case 3:
            err = TakeScriptString((short *)gEditVarValue, (byte *)num);
            if (err != 0)
                FatalError(err, 0x1146);
            num[(byte)num[0] + 1] = 0;
            strcpy(buf, num + 1);
            break;
        case 4:
            FormatUintAsPascalDecimalString(*(uint32_t *)((short *)gEditVarValue + 1), num);
            num[(byte)num[0] + 1] = 0;
            strcpy(buf, num + 1);
            break;
        default:
            FatalError(0, 0x1147);
            break;
        }
        SetDlgItemTextA(hDlg, 100, buf);
        switch (*(short *)gEditVarRecord) {
        case 0:
            EnableWindow(GetDlgItem(hDlg, 0x69), FALSE);
            SetDlgItemTextA(hDlg, 0x69, kStrEmpty);
            CheckRadioButton(hDlg, 0x65, 0x68, 0x65);
            return TRUE;
        case 1:
            EnableWindow(GetDlgItem(hDlg, 0x69), FALSE);
            SetDlgItemTextA(hDlg, 0x69, kStrEmpty);
            CheckRadioButton(hDlg, 0x65, 0x68, 0x66);
            return TRUE;
        case 2:
            EnableWindow(GetDlgItem(hDlg, 0x69), FALSE);
            SetDlgItemTextA(hDlg, 0x69, kStrEmpty);
            CheckRadioButton(hDlg, 0x65, 0x68, 0x67);
            return TRUE;
        case 3:
            EnableWindow(GetDlgItem(hDlg, 0x69), TRUE);
            FormatUintAsPascalDecimalString(*(uint32_t *)((short *)gEditVarRecord + 0xb), num);
            num[(byte)num[0] + 1] = 0;
            strcpy(buf, num + 1);
            SetDlgItemTextA(hDlg, 0x69, buf);
            CheckRadioButton(hDlg, 0x65, 0x68, 0x68);
            return TRUE;
        case 4:
            EnableWindow(GetDlgItem(hDlg, 0x69), TRUE);
            CopyPascalString((byte *)((short *)gEditVarRecord + 0xd), (byte *)num);
            num[(byte)num[0] + 1] = 0;
            strcpy(buf, num + 1);
            SetDlgItemTextA(hDlg, 0x69, buf);
            CheckRadioButton(hDlg, 0x65, 0x68, 0x68);
            return TRUE;
        case 5:
            EnableWindow(GetDlgItem(hDlg, 0x69), TRUE);
            if (*(int *)((short *)gEditVarRecord + 0xb) != 0)
                strcpy(buf, gTrueCaption);
            else
                strcpy(buf, gFalseCaption);
            SetDlgItemTextA(hDlg, 0x69, buf);
            CheckRadioButton(hDlg, 0x65, 0x68, 0x68);
            return TRUE;
        default:
            FatalError(0, 0x1148);
            return TRUE;
        }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            /* PascalStringsEqualNoCase returns 1 on a MATCH (the port once had every test inverted) */
            if (IsDlgButtonChecked(hDlg, 0x65) != 0)
                *(short *)gEditVarRecord = 0;
            else if (IsDlgButtonChecked(hDlg, 0x66) != 0)
                *(short *)gEditVarRecord = 1;
            else if (IsDlgButtonChecked(hDlg, 0x67) != 0)
                *(short *)gEditVarRecord = 2;
            else if (IsDlgButtonChecked(hDlg, 0x68) != 0) {
                GetDlgItemTextA(hDlg, 0x69, buf + 1, 0xff);
                buf[0] = (char)strlen(buf + 1);
                if (PascalStringsEqualNoCase((byte *)buf, UpdatePascalLength((byte *)kStrTrueLower)) != 0) {
                    *(short *)gEditVarRecord = 5;
                    *(uint32_t *)((short *)gEditVarRecord + 0xb) = 1;
                } else if (PascalStringsEqualNoCase((byte *)buf, UpdatePascalLength((byte *)kStrFalseLower)) != 0) {
                    *(short *)gEditVarRecord = 5;
                    *(uint32_t *)((short *)gEditVarRecord + 0xb) = 0;
                } else {
                    parsed = ParseLeadingIntFromPascalString((byte *)buf);
                    FormatUintAsPascalDecimalString(parsed, num);
                    if (PascalStringsEqualNoCase((byte *)buf, (byte *)num) != 0) {
                        *(short *)gEditVarRecord = 3;
                        *(uint32_t *)((short *)gEditVarRecord + 0xb) = parsed;
                    } else {
                        *(short *)gEditVarRecord = 4;
                        CopyPascalString((byte *)buf, (byte *)((short *)gEditVarRecord + 0xd));
                    }
                }
            }
            GetDlgItemTextA(hDlg, 100, (LPSTR)(gEditVarText + 1), 0xff);
            *gEditVarText = (byte)strlen((char *)gEditVarText + 1);
            EndDialog(hDlg, 0);
            return TRUE;
        case IDCANCEL:
            EndDialog(hDlg, 1);
            return TRUE;
        case 0x65:
            CheckRadioButton(hDlg, 0x65, 0x68, 0x65);
            EnableWindow(GetDlgItem(hDlg, 0x69), FALSE);
            return TRUE;
        case 0x66:
            CheckRadioButton(hDlg, 0x65, 0x68, 0x66);
            EnableWindow(GetDlgItem(hDlg, 0x69), FALSE);
            return TRUE;
        case 0x67:
            CheckRadioButton(hDlg, 0x65, 0x68, 0x67);
            EnableWindow(GetDlgItem(hDlg, 0x69), FALSE);
            return TRUE;
        case 0x68:
            CheckRadioButton(hDlg, 0x65, 0x68, 0x68);
            EnableWindow(GetDlgItem(hDlg, 0x69), TRUE);
            /* falls into default: the exe's out-of-range jump shares this return */
        default:
            return TRUE;
        }
    }
    return FALSE;
}


/* Centres hWnd over the main window and makes it topmost (HWND_TOPMOST, SWP_NOSIZE). */
void __cdecl CenterWindowRelative(HWND hWnd)  /* 0x00432D70 */
{
  tagRECT rcCenter;  /* local_30 */
  tagRECT rcWindow;  /* local_20 */
  tagRECT rcParent;  /* local_10 */
  HWND    hParent;

  hParent = (HWND)gMainWindow;   /* loaded into a register first, as the exe does */
  GetWindowRect(hParent, &rcParent);
  GetWindowRect(hWnd, &rcWindow);
  CopyRect(&rcCenter, &rcParent);
  OffsetRect(&rcWindow, -rcWindow.left, -rcWindow.top);
  OffsetRect(&rcCenter, -rcCenter.left, -rcCenter.top);
  OffsetRect(&rcCenter, -rcWindow.right, -rcWindow.bottom);
  SetWindowPos(hWnd, (HWND)0xffffffff, rcParent.left + rcCenter.right / 2,
               rcParent.top + rcCenter.bottom / 2, 0, 0, 1);
  return;
}


/* Makes sure the CD named by the Pascal-string descriptor is in a drive. Returns 0x2a if
 * CheckDescriptorVolumeResolvable fails, else 0. Present: *pFlagOut = 1. Absent: the "insert
 * the CD" prompt -- string 0xba formatted with the name, the drive ejected unless -e
 * (gNoEjectFlag), then every 4000 ms a task-modal OK/Cancel box (caption 0xb9; 0x2041 =
 * MB_TASKMODAL|MB_ICONINFORMATION|MB_OKCANCEL) until the volume appears (*pFlagOut = 1) or
 * Cancel (*pFlagOut = 0). The flag tells SetCurrentCdName whether to keep the descriptor it
 * copied into gCurrentCdName: 0 makes boot() fail (opcode 12053).
 *
 * GOG's TI.EXE NOPs the jz at 0x00432e59 that enters the prompt, so an absent volume counts
 * as present and the loop (0x00432e75..0x00432f97) is dead; DF_FIX_GOG_NO_CD_CHECK keeps that. */
short __cdecl PromptUntilDescriptorVolumePresent(byte *pDesc, short *pFlagOut) /* 0x00432e20 */
{
    short present;
    uint  len;
    DWORD lastPrompt;
    char  name[32];
    char  msg[256];
    char  text[256];

    if (CheckDescriptorVolumeResolvable(pDesc, &present) != 0)
        return 0x2a;
#if DF_FIX_GOG_NO_CD_CHECK
    /* GOG: the jz into the prompt is NOPed -- present or not, carry on */
    *pFlagOut = 1;
    return 0;
#else
    if (present != 0) {
        *pFlagOut = 1;
        return 0;
    }
    lastPrompt = 0;
    SuspendGameUiForModalDialog();
    len = *pDesc;
    _memcpy(name, pDesc + 1, len);
    name[len] = 0;
    _sprintf(msg, GetStringResource(0xba), name);
    WordWrapText(msg, text);
    if (gNoEjectFlag == 0)
        EjectCdDrive();
    for (;;) {
        if (timeGetTime() - lastPrompt > 4000) {
            if (MessageBoxA(NULL, text, GetStringResource(0xb9), 0x2041) == IDCANCEL) {
                RestoreGameUiAfterModalDialog();
                *pFlagOut = 0;
                return 0;
            }
            lastPrompt = timeGetTime();
        }
        if (CheckDescriptorVolumeResolvable(pDesc, &present) != 0) {
            RestoreGameUiAfterModalDialog();
            *pFlagOut = 0;
            return 0x2a;
        }
        if (present != 0) {
            RestoreGameUiAfterModalDialog();
            *pFlagOut = 1;
            return 0;
        }
    }
#endif
}


/* Ejects the first CD-ROM drive after a 1000 ms pause: opens \\.\X: and, if
 * FSCTL_LOCK_VOLUME (0x90018) succeeds, sends IOCTL_DISK_EJECT_MEDIA (0x74808) and
 * FSCTL_UNLOCK_VOLUME (0x9001c). On Win 3.x, with no CD drive, or if the device will not
 * open, it falls back to EjectCdViaMci. */
void __stdcall EjectCdDrive(void)  /* 0x00432FA0 */
{
  CHAR driveLetter;
  HANDLE hDevice;
  DWORD bytesReturned;  /* local_24 */
  char devicePath [32];  /* local_20 */

  Sleep(1000);
  if (gIsWin31 == 0 && (driveLetter = GetFirstCdRomDriveLetter()) != '\0') {
    _sprintf(devicePath, "\\\\.\\%c:" /* 0x0045da50 */, (int)driveLetter);
    hDevice = CreateFileA(devicePath,0x80000000,3,(LPSECURITY_ATTRIBUTES)0x0,3,0x80,(HANDLE)0x0);
    if (hDevice != (HANDLE)0xffffffff)
      goto opened;
  }
  /* the exe's layout: every failure path reaches one block through a constant-true test */
  hDevice = (HANDLE)0xffffffff;
  if (hDevice == (HANDLE)0xffffffff) {
    EjectCdViaMci();
    return;
  }
opened:
  if (DeviceIoControl(hDevice,0x90018,(LPVOID)0x0,0,(LPVOID)0x0,0,&bytesReturned,
                      (LPOVERLAPPED)0x0) != 0) {
    DeviceIoControl(hDevice,0x74808,(LPVOID)0x0,0,(LPVOID)0x0,0,&bytesReturned,(LPOVERLAPPED)0x0);
    DeviceIoControl(hDevice,0x9001c,(LPVOID)0x0,0,(LPVOID)0x0,0,&bytesReturned,(LPOVERLAPPED)0x0);
  }
  CloseHandle(hDevice);
}


/* Ejects the CD through MCI: MCI_OPEN (0x803) with MCI_OPEN_TYPE|MCI_OPEN_TYPE_ID (0x3000) of
 * device type 0x204 (MCI_DEVTYPE_CD_AUDIO), MCI_SET (0x80d) with MCI_SET_DOOR_OPEN (0x100),
 * then MCI_CLOSE (0x804). Nothing happens if the open fails.
 * Only wDeviceID and the device type are set: OPEN with those flags reads nothing else, and
 * SET with MCI_SET_DOOR_OPEN reads no field of its 12-byte MCI_SET_PARMS. No memsets. */
void __stdcall EjectCdViaMci(void) /* 0x00433070 */
{
    MCI_OPEN_PARMSA openPrm;
    MCI_SET_PARMS   setPrm;

    openPrm.wDeviceID       = 0;
    openPrm.lpstrDeviceType = (LPCSTR)(ULONG_PTR)(WORD)0x204;
    if (mciSendCommandA(0, 0x0803u, 0x3000u, (DWORD_PTR)&openPrm) == 0) {
        mciSendCommandA(openPrm.wDeviceID, 0x080du, 0x100u, (DWORD_PTR)&setPrm);
        mciSendCommandA(openPrm.wDeviceID, 0x0804u, 0, 0);
    }
}


/* Returns the letter of the first drive GetDriveTypeA reports as DRIVE_CDROM, or 0 if none.
 * The drive-root list is a double-NUL-terminated list from GetLogicalDriveStringsA or, on
 * Win 3.x, built from the GetLogicalDrives bitmask and the "A:\" template kStrDriveA. */
char __stdcall GetFirstCdRomDriveLetter(void) /* 0x004330d0 */
{
    char   root[4];
    char   buf[2048];
    int    i;
    DWORD  mask;
    char  *p;

    /* "A:\" copied as one dword (not the array's address, truncated: a past port bug) */
    *(int *)root = *(const int *)kStrDriveA;
    if (gIsWin31 != 0) {
        p = buf;
        mask = GetLogicalDrives();
        for (i = 0x20; i != 0; i--) {
            if (mask & 1) {
                strcpy(p, root);
                p += strlen(buf) + 1;   /* sic: strlen of the FIRST root, as the
                                         * exe; every root is 3 chars, so equal */
            }
            mask >>= 1;
            root[0]++;
        }
        p[0] = '\0';
        p[1] = '\0';
    } else {
        GetLogicalDriveStringsA(sizeof(buf), buf);
    }
    for (p = buf; strlen(p) != 0; p += strlen(p) + 1) {
        if (GetDriveTypeA(p) == DRIVE_CDROM)
            return *p;
    }
    return '\0';
}


/* The malloc-backed variant of InitHeap: sets up gPrimaryHeap over a malloc'd pool of
 * size + 0x60 bytes, with ceil(align / 256) descriptor pages, and makes it the active heap.
 * Called from OpenSoundDevice. Fatal 0xee if size < 0x30 (one block header), 0x105 if malloc
 * fails. Torn down by ShutdownVxDHeap. */
void __cdecl InitVxDHeap(unsigned int size, unsigned int align)  /* 0x004331c0 */
{
    int   nBuckets;
    void *pvPool;

    if (size < 0x30)
        FatalErrorWithCode(0xee);

    /* nBuckets = ceil(align / 256), a signed divide in the exe (cdq idiom) */
    if ((int)align % 256 != 0)
        nBuckets = (int)align / 256 + 1;
    else
        nBuckets = (int)align / 256;

    InitDescriptorPool(nBuckets);
    size += 0x60;

    pvPool = malloc(size);
    if (pvPool == NULL)
        FatalErrorWithCode(0x105);

    g_pActiveHeapDesc = &gPrimaryHeap;

    InitHeapDescriptor(&gPrimaryHeap, (int)pvPool, size);
}


/* Empties the descriptor pool and pre-allocates nBuckets descriptor pages; the first page is
 * gDescPageHead, where FreeDescriptorPages starts its walk. */
static void __cdecl InitDescriptorPool(int nBuckets)  /* 0x00433260 */
{
    int i;

    i = 0;
    gDescPageTail  = 0;
    gHeapPageCount = 0;
    gDescFreeList  = 0;

    if (nBuckets > 0)
    {
        do {
            AllocDescriptorPage();
            if (i == 0)
                gDescPageHead = gDescPageTail;
            i++;
        } while (i < nBuckets);
    }
}


/* Mallocs one 0x800-byte descriptor page and chains its 255 descriptors into the free list.
 * Page layout (512 ints): [0] page sequence number (gHeapPageCount), [1] next-page link (set
 * by the next call; the teardown chain), then descriptors of two ints {data, link to the next
 * free one} at [2..3] .. [510..511]. A non-empty free list gets the new page behind its head
 * (the head's link is overwritten); an empty one starts at the page's first descriptor.
 * Fatal 200 if malloc fails. */
void __stdcall AllocDescriptorPage(void)  /* 0x004332a0 */
{
    int *pPage;
    int  i;
    int *pDesc;

    pPage = (int *)malloc(0x800);
    if (pPage == NULL)
        FatalErrorWithCode(200);

    if (gDescPageTail != NULL)
        *(int **)((char *)gDescPageTail + 4) = pPage;

    pPage[1] = 0;
    pPage[0] = gHeapPageCount;
    gDescPageTail = pPage;

    /* From here pPage walks the descriptors (the exe's `add esi,8`) */
    pPage += 2;
    if (gDescFreeList != NULL)
        gDescFreeList[1] = (int)pPage;
    else
        gDescFreeList = pPage;

    /* descriptors 0..253 each link to the next; pDesc points at a link field */
    pDesc = pPage + 1;
    for (i = 0xfe; i != 0; i--) {
        *pDesc    = (int)(pDesc + 1);
        pDesc[-1] = 0;
        pDesc    += 2;
    }

    /* descriptor 254 ends the chain */
    pPage[0x1fd] = 0;
    pPage[0x1fc] = 0;
    pPage += 0x1fc;
    gDescLastEntry = pPage;
    gHeapPageCount++;
}


/* Initialises a heap descriptor over [pvPool, pvPool + cbPool) with one free block spanning
 * the whole pool (the size rounded down to 16 bytes).
 * Heap descriptor (ints): [0] first block of the physical chain (the pool base), [1] free-list
 * tail (lowest free block), [2] free-list head (highest), [3] pool size, [4] bytes in use (headers included),
 * [5..10] CRITICAL_SECTION, [11] busy flag (set while the lock is held).
 * Block header (0x30 bytes, data follows): +0x00 owning resource descriptor (0 = free block),
 * +0x04 requested size, +0x08 data capacity, +0x0c / +0x10 next / previous block in address
 * order, +0x14 owning heap descriptor, +0x18 / +0x1c free-list links (toward head / tail),
 * +0x20 lock count (GetResDataPtr / UnlockResObject; a locked block never moves). */
static void __cdecl InitHeapDescriptor(int *pDesc, int pvPool, int cbPool)  /* 0x00433350 */
{
    pDesc[0x0b] = 0;
    InitializeCriticalSection((LPCRITICAL_SECTION)(pDesc + 5));

    pDesc[0] = pvPool;
    pDesc[1] = pvPool;
    pDesc[2] = pvPool;
    pDesc[4] = 0;

    /* signed: the exe's cdq idiom */
    if (cbPool % 16 != 0)
        cbPool = cbPool / 16 * 16;

    pDesc[3] = cbPool;

    *(unsigned int *)(pvPool + 8)    = cbPool - 0x30;
    *(int *)pDesc[0]                 = 0;
    *(int *)(pDesc[0] + 0x20)        = 0;
    *(int *)(pDesc[0] + 0x0c)        = 0;
    *(int *)(pDesc[0] + 0x10)        = 0;
    *(int **)(pDesc[0] + 0x14)       = pDesc;
    *(int *)(pDesc[0] + 0x18)        = 0;
    *(int *)(pDesc[0] + 0x1c)        = 0;
}


/* Tears down the heap InitVxDHeap made: frees the malloc'd pool and the descriptor pages
 * under the heap's lock, with the shutting-down flag set meanwhile.
 * DAT_0048a9c4 and DAT_0048a9dc are gPrimaryHeap's lock (+0x14) and shutting-down flag (+0x2c). */
void __stdcall ShutdownVxDHeap(void)  /* 0x004333d0 */
{
    EnterCriticalSection(&DAT_0048a9c4);

    DAT_0048a9dc = 1;
    free((void *)gPrimaryHeap[0]);
    FreeDescriptorPages();

    g_pActiveHeapDesc = NULL;

    DAT_0048a9dc = 0;

    LeaveCriticalSection(&DAT_0048a9c4);
    DeleteCriticalSection(&DAT_0048a9c4);
}


/* Frees every descriptor page, from gDescPageHead along the next-page links through
 * gDescPageTail. Called under the heap's lock. */
static void __stdcall FreeDescriptorPages(void)  /* 0x00433430 */
{
    void *pPage;
    void *pCur;

    pPage = gDescPageHead;
    do {
        pCur = pPage;
        pPage = *(void **)((char *)pPage + 4);
        free(pCur);
    } while (gDescPageTail != pCur);
}


/* Sets up the engine heap, gPrimaryHeap, over the caller's pool and makes it the active heap.
 * cbBlockMax is a descriptor count: ceil(cbBlockMax / 256) descriptor pages are pre-allocated.
 * Fatal 0xee if the pool is under 0x30 bytes (one block header), 0x105 if pvPool is NULL.
 * Called from AppInit right after AllocMemoryPool. */
void __cdecl InitHeap(void *pvPool, unsigned int cbPool, unsigned int cbBlockMax)  /* 0x00433460 */
{
    int nBuckets;

    if (cbPool < 0x30)
        FatalErrorWithCode(0xee);

    /* nBuckets = ceil(cbBlockMax / 256), a signed divide in the exe (cdq idiom) */
    if ((int)cbBlockMax % 256 != 0)
        nBuckets = (int)cbBlockMax / 256 + 1;
    else
        nBuckets = (int)cbBlockMax / 256;

    InitDescriptorPool(nBuckets);

    if (pvPool == NULL)
        FatalErrorWithCode(0x105);

    g_pActiveHeapDesc = &gPrimaryHeap;

    InitHeapDescriptor(&gPrimaryHeap, (int)pvPool, cbPool);
}


/* Counterpart of InitHeap: frees the descriptor pages under the heap's lock and clears the
 * active heap. The pool itself belongs to the caller. */
void __stdcall ShutdownHeap(void)  /* 0x00433500 */
{
    EnterCriticalSection((LPCRITICAL_SECTION)&DAT_0048a9c4);

    DAT_0048a9dc = 1;
    FreeDescriptorPages();

    g_pActiveHeapDesc = NULL;

    DAT_0048a9dc = 0;

    LeaveCriticalSection((LPCRITICAL_SECTION)&DAT_0048a9c4);
    DeleteCriticalSection((LPCRITICAL_SECTION)&DAT_0048a9c4);
}


/* Makes pDesc a secondary heap over a cbSize-byte engine buffer: a heap inside the engine
 * heap, such as the animation code's compositing scratch heap. Fatal 0xdf if pDesc is the
 * active heap, 0xe0 if the buffer cannot be allocated. */
void __cdecl AllocSecondaryHeap(int *pDesc, unsigned int cbSize)  /* 0x00433550 */
{
    int iBlock;

    if (g_pActiveHeapDesc == pDesc)
        FatalErrorWithCode(0xdf);

    iBlock = (int)AllocEngineBuffer(cbSize);
    if (iBlock == 0)
        FatalErrorWithCode(0xe0);

    InitHeapDescriptor(pDesc, iBlock, cbSize);
}


/* Frees a heap made by AllocSecondaryHeap and, if it was the active heap, makes gPrimaryHeap
 * active again. Fatal 0xe1 on gPrimaryHeap itself. Called by ShutdownScriptRuntime. */
void __cdecl FreeSecondaryHeap(int *pDesc)  /* 0x004335b0 */
{
    LPCRITICAL_SECTION lpCS;

    if (pDesc == &gPrimaryHeap)
        FatalErrorWithCode(0xe1);

    lpCS = (LPCRITICAL_SECTION)(pDesc + 5);
    EnterCriticalSection(lpCS);

    pDesc[0x0b] = 1;
    FreeEngineBuffer(pDesc[0]);
    pDesc[0x0b] = 0;

    LeaveCriticalSection(lpCS);
    DeleteCriticalSection(lpCS);

    if (g_pActiveHeapDesc == pDesc)
        g_pActiveHeapDesc = &gPrimaryHeap;
}


/* Sets this thread's active heap; GetHeapDescPtr reads it. */
void __cdecl SetActiveHeap(int *pHeapDesc)  /* 0x00433620 */
{
    g_pActiveHeapDesc = pHeapDesc;
}


/* Makes gPrimaryHeap this thread's active heap again, after work in a secondary heap. */
void __stdcall RestorePrimaryHeap(void)  /* 0x00433640 */
{
    g_pActiveHeapDesc = &gPrimaryHeap;
}


/* Returns this thread's active heap descriptor, NULL before any heap exists. OpenSoundDevice
 * uses it to test whether the engine heap is up before calling InitVxDHeap. */
int __stdcall GetHeapDescPtr(void)  /* 0x00433660 */
{
    return (int)g_pActiveHeapDesc;
}


/* Returns the capacity of the active heap's largest free block, read under its lock: the
 * largest buffer to be had without compaction. MoveNodeBlock sizes its copy buffer by it.
 * Every public heap entry point brackets its work this way: the heap's lock (+0x14) held
 * and its busy flag (+0x2c) set meanwhile. */
unsigned int __stdcall GetLargestFreeBlockSize(void)  /* 0x00433680 */
{
    unsigned int result;
    int         *pHeapDescPtr;

    pHeapDescPtr = (int *)&g_pActiveHeapDesc;

    EnterCriticalSection((LPCRITICAL_SECTION)(*pHeapDescPtr + 0x14));
    *(int *)(*pHeapDescPtr + 0x2c) = 1;

    result = GetHeapFreeCapacity(*pHeapDescPtr);

    *(int *)(*pHeapDescPtr + 0x2c) = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)(*pHeapDescPtr + 0x14));

    return result;
}


/* Returns the largest capacity on heapDesc's free list. */
unsigned int __cdecl GetHeapFreeCapacity(int heapDesc)  /* 0x004336e0 */
{
    int          maxCapacity;            /* signed: the exe compares with jge */
    int         *pBlock;

    maxCapacity = 0;

    pBlock = *(int **)(heapDesc + 4);
    while (pBlock != NULL) {                      /* a while, not a for: the for form compares against the zeroed eax */
        if (maxCapacity < *(int *)((char *)pBlock + 8))
            maxCapacity = *(int *)((char *)pBlock + 8);
        pBlock = *(int **)((char *)pBlock + 0x18);
    }

    return maxCapacity;
}


/* Returns the active heap's free bytes: pool size - bytes in use - one block header, clamped
 * at 0. Fragmentation can leave no single block that large. */
int __stdcall GetHeapFreeBytes(void)  /* 0x00433700 */
{
    int   *pTlsBlock;
    int    iBlock;
    int    freeBytes;

    pTlsBlock = (int *)&g_pActiveHeapDesc;
    EnterCriticalSection((LPCRITICAL_SECTION)(*pTlsBlock + 0x14));
    *(uint32_t *)(*pTlsBlock + 0x2c) = 1;
    iBlock    = *pTlsBlock;
    freeBytes = (*(int *)(iBlock + 0xc) - *(int *)(iBlock + 0x10)) - 0x30;
    if (freeBytes < 0)
        freeBytes = 0;
    *(uint32_t *)(iBlock + 0x2c) = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)(*pTlsBlock + 0x14));
    return freeBytes;
}


/* Returns 1 if a cbSize-byte block (rounded up to 16) can be had from the active heap, else
 * 0: allocates one, compacting once if needed, and frees it again. A compaction it triggers
 * stays done. */
int __cdecl TryAllocHeapSpace(uint32_t cbSize)  /* 0x00433760 */
{
    int      *pHeapRef;
    int      *pBlock;

    pHeapRef = (int *)&g_pActiveHeapDesc;
    EnterCriticalSection((LPCRITICAL_SECTION)(*pHeapRef + 0x14));
    *(uint32_t *)(*pHeapRef + 0x2c) = 1;

    /* Round cbSize up to 16 (a signed divide in the exe: cdq idiom) */
    if ((int)cbSize % 16 != 0)
        cbSize = ((int)cbSize / 16 + 1) * 16;

    pBlock = AllocHeapBlock(*pHeapRef, (int)cbSize);
    if (pBlock == NULL) {
        CompactHeap(*pHeapRef, (int)(cbSize + 0x30));
        pBlock = AllocHeapBlock(*pHeapRef, (int)cbSize);
        if (pBlock == NULL) {
            *(uint32_t *)(*pHeapRef + 0x2c) = 0;
            LeaveCriticalSection((LPCRITICAL_SECTION)(*pHeapRef + 0x14));
            return 0;
        }
    }
    FreeHeapBlock(*pHeapRef, pBlock);
    *(uint32_t *)(*pHeapRef + 0x2c) = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)(*pHeapRef + 0x14));
    return 1;
}


/* First fit from the low end: walks the free list from the tail and takes the first block
 * with capacity >= cbSize, split to size by SplitHeapBlock. Returns it, or NULL.
 * FindFreeBlock is the same search from the head, the high end. */
static int * __cdecl AllocHeapBlock(int heapDesc, int cbSize)  /* 0x00433830 */
{
    int pBlock;

    for (pBlock = *(int *)(heapDesc + 4); pBlock != 0; pBlock = *(int *)(pBlock + 0x18)) {
        if (cbSize <= *(int *)(pBlock + 8)) {
            SplitHeapBlock((char *)pBlock, cbSize);
            return (int *)pBlock;
        }
    }
    return NULL;
}


/* Takes cbSize bytes of the free block pBlock for an allocation. If the rest would not hold a
 * block header and one byte (<= 0x30), the whole block is unlinked from the free list and
 * used; otherwise the rest becomes a new free block at pBlock + 0x30 + cbSize that takes
 * pBlock's place in the free list. Either way the bytes used go into the heap's in-use count;
 * fatal 0xce if that passes the pool size. */
void __cdecl SplitHeapBlock(char *pBlock, int cbSize)  /* 0x00433860: a pointer, and pointer arithmetic on it */
{
    int  iHeapDesc;
    int *pNewBlock;
    int  pLink;

    iHeapDesc = *(int *)(pBlock + 0x14);

    if ((unsigned int)(*(int *)(pBlock + 8) - cbSize) <= 0x30)   /* cmp 0x30 / ja */
    {
        if (*(int *)(pBlock + 0x18) != 0)    /* test / je: the linked case first */
        {
            *(int *)(*(int *)(pBlock + 0x18) + 0x1c) = *(int *)(pBlock + 0x1c);
        }
        else
        {
            *(int *)(iHeapDesc + 8) = *(int *)(pBlock + 0x1c);
        }

        if (*(int *)(pBlock + 0x1c) != 0)
        {
            *(int *)(*(int *)(pBlock + 0x1c) + 0x18) = *(int *)(pBlock + 0x18);
        }
        else
        {
            *(int *)(iHeapDesc + 4) = *(int *)(pBlock + 0x18);
        }

        *(int *)(iHeapDesc + 0x10) += *(int *)(pBlock + 8) + 0x30;
    }
    else
    {
        pNewBlock    = (int *)(pBlock + 0x30 + cbSize);
        pNewBlock[0] = 0;
        pNewBlock[2] = *(int *)(pBlock + 8) - cbSize - 0x30;
        pNewBlock[8] = 0;
        pNewBlock[5] = iHeapDesc;

        pNewBlock[3] = *(int *)(pBlock + 0x0c);
        if (*(int *)(pBlock + 0x0c) != 0)
            *(int *)(*(int *)(pBlock + 0x0c) + 0x10) = (int)pNewBlock;

        pNewBlock[4] = (int)pBlock;

        pLink = *(int *)(pBlock + 0x18);
        pNewBlock[6] = pLink;
        if (pLink != 0)                                     /* test / je: the linked case first */
            *(int *)(pLink + 0x1c) = (int)pNewBlock;
        else
            *(int *)(iHeapDesc + 8) = (int)pNewBlock;

        pLink = *(int *)(pBlock + 0x1c);
        pNewBlock[7] = pLink;
        if (pLink != 0)
            *(int *)(pLink + 0x18) = (int)pNewBlock;
        else
            *(int *)(iHeapDesc + 4) = (int)pNewBlock;

        *(int *)(pBlock + 0x0c) = (int)pNewBlock;
        *(int *)(pBlock + 8)    = cbSize;

        *(int *)(iHeapDesc + 0x10) += cbSize + 0x30;
    }

    if (*(int *)(iHeapDesc + 0x0c) < *(int *)(iHeapDesc + 0x10))
        FatalErrorWithCode(0xce);
}


/* Returns pBlock to heapDesc's free list and merges it with free neighbours: it absorbs a free
 * block after it (+0x0c), then is absorbed by a free block before it (+0x10). Fatal 0xd2 if
 * the in-use count goes negative. */
void __cdecl FreeHeapBlock(int heapDesc, int *pBlock)  /* 0x00433930 */
{
    int *pNeighbour;
    int  usedBytes;

    usedBytes = *(int *)(heapDesc + 0x10) - (pBlock[2] + 0x30);
    *(int *)(heapDesc + 0x10) = usedBytes;
    if (usedBytes < 0)
        FatalErrorWithCode(0xd2);

    pBlock[0] = 0;
    pBlock[5] = heapDesc;
    pBlock[8] = 0;

    InsertFreeBlock(heapDesc, (int)pBlock);

    pNeighbour = (int *)pBlock[3];
    if (pNeighbour != NULL && *pNeighbour == 0)
    {
        pBlock[2] += pNeighbour[2] + 0x30;

        pBlock[3] = pNeighbour[3];
        if (pBlock[3] != 0)
            *(int *)(pBlock[3] + 0x10) = (int)pBlock;

        pBlock[6] = pNeighbour[6];
        if (pBlock[6] != 0)
            *(int *)(pBlock[6] + 0x1c) = (int)pBlock;
        else
            *(int *)(heapDesc + 8) = (int)pBlock;
    }

    pNeighbour = (int *)pBlock[4];
    if (pNeighbour != NULL && *pNeighbour == 0)
    {
        pNeighbour[2] += pBlock[2] + 0x30;

        pNeighbour[3] = pBlock[3];
        if (pBlock[3] != 0)
            *(int *)(pBlock[3] + 0x10) = (int)pNeighbour;

        pNeighbour[6] = pBlock[6];
        if (pNeighbour[6] != 0)
            *(int *)(pNeighbour[6] + 0x1c) = (int)pNeighbour;
        else
            *(int *)(heapDesc + 8) = (int)pNeighbour;
    }
}


/* Links the free block pBlock into heapDesc's free list, which is kept in address order (tail
 * lowest, head highest): finds the nearest free block above and below it in the physical
 * chain and splices it between them. */
void __cdecl InsertFreeBlock(int heapDesc, int pBlock)  /* 0x004339e0 */
{
    int *pIter;

    for (pIter = *(int **)(pBlock + 0x0c);
         pIter != NULL && *pIter != 0;
         pIter = (int *)pIter[3])
    {}

    *(int **)(pBlock + 0x18) = pIter;

    if (pIter != NULL)                            /* test / je: the found case first */
        pIter[7] = pBlock;
    else
        *(int *)(heapDesc + 8) = pBlock;

    for (pIter = *(int **)(pBlock + 0x10);
         pIter != NULL && *pIter != 0;
         pIter = (int *)pIter[4])
    {}

    *(int **)(pBlock + 0x1c) = pIter;

    if (pIter != NULL)
        pIter[6] = pBlock;
    else
        *(int *)(heapDesc + 4) = pBlock;
}


/* Defragments heapDesc until one run of free space holds cbNeeded bytes (headers included);
 * called when an allocation fails. The locked blocks cut the heap into segments, runs of free
 * and movable blocks, recorded in a malloc'd table ({first block, last block, free bytes,
 * free + movable bytes}; gHeapPageCount * 0x800 bytes, one row per possible block) and sorted
 * by SortSegmentTable. Every segment is first consolidated (its live blocks slid down, one
 * free block left at its end); then from the last row down, a segment with enough free +
 * movable bytes has its live blocks moved into another segment's trailing free block and is
 * consolidated again, until one segment has cbNeeded free.
 * Fatal 0x103 if the table cannot be malloc'd, 0xde if a destination block is not free.
 *
 * Port note: the port once dropped the first consolidation loop and read a block's capacity
 * through an int-scaled offset; both raised fatal 0xdc in ConsolidateSegment */
void __cdecl CompactHeap(int heapDesc, int cbNeeded)  /* 0x00433a30 */
{
    /* The scratch table is an array of 16-byte records: the exe indexes it
     * by segment number (shl 4), never by a running byte offset. */
    typedef struct { int *first; int *last; int freeB; int total; } Seg;
    int *pBlk;
    int *pStart;
    int *pPrev;
    int  segIdx;
    int  total;
    int  freeB;
    int  i;
    int *pNext;          /* next non-free block after the one being moved */
    int *pBound;         /* first block after the segment */
    int *pDst;           /* trailing free block in the destination segment */
    int *pAfter;
    int  tgt;
    Seg *pSeg;
    Seg *pTgt;

    gCompactWorkBuf = malloc((gHeapPageCount & 0xfffff) << 0xb);
    if (gCompactWorkBuf == NULL)
        FatalErrorWithCode(0x103);

    /* Each row field is stored through freshly reloaded globals, +0x0c before +0x08. */
    pBlk = *(int **)heapDesc;
    pStart = pBlk;
    gCompactSegCount = 0;
    total = 0;
    freeB = 0;
    pPrev = NULL;
    for (; pBlk != NULL; pBlk = (int *)pBlk[3]) {
        if (pBlk[0] == 0 || pBlk[8] == 0) {
            total += pBlk[2] + 0x30;
            if (pBlk[0] == 0)
                freeB += pBlk[2] + 0x30;
        } else {
            if (pStart != pBlk) {
                ((Seg *)gCompactWorkBuf)[gCompactSegCount].first = pStart;
                ((Seg *)gCompactWorkBuf)[gCompactSegCount].last = pPrev;
                ((Seg *)gCompactWorkBuf)[gCompactSegCount].total = total;
                ((Seg *)gCompactWorkBuf)[gCompactSegCount].freeB = freeB;
                gCompactSegCount++;
            }
            pStart = (int *)pBlk[3];
            total = 0;
            freeB = 0;
        }
        pPrev = pBlk;
    }
    if (pStart != NULL) {
        ((Seg *)gCompactWorkBuf)[gCompactSegCount].first = pStart;
        ((Seg *)gCompactWorkBuf)[gCompactSegCount].last = pPrev;
        ((Seg *)gCompactWorkBuf)[gCompactSegCount].total = total;
        ((Seg *)gCompactWorkBuf)[gCompactSegCount].freeB = freeB;
        gCompactSegCount++;
    }

    SortSegmentTable(0, gCompactSegCount - 1);

    /* Every segment consolidated before anything moves (0x00433b64-0x00433b8b): the moves
     * below track a destination segment by its ONE trailing free block. */
    for (i = 0; i < gCompactSegCount; i++)
        ConsolidateSegment(heapDesc, (int *)&((Seg *)gCompactWorkBuf)[i]);

    for (segIdx = gCompactSegCount - 1; segIdx >= 0; segIdx--) {
        pSeg = &((Seg *)gCompactWorkBuf)[segIdx];
        if (pSeg->total >= cbNeeded) {
            pBlk = pSeg->first;
            pBound = (int *)pSeg->last[3];
            while (pBlk < pBound) {
                for (pNext = (int *)pBlk[3]; pNext != NULL && *pNext == 0; pNext = (int *)pNext[3]) {
                }
                if (*pBlk != 0) {
                    for (tgt = gCompactSegCount - 1; tgt >= 0; tgt--) {
                        if (tgt != segIdx && ((Seg *)gCompactWorkBuf)[tgt].freeB >= pBlk[2] + 0x30) {
                            pTgt = &((Seg *)gCompactWorkBuf)[tgt];
                            pDst = pTgt->last;
                            if (*pDst != 0)
                                FatalErrorWithCode(0xde);
                            SplitHeapBlock((char *)pDst, pBlk[2]);
                            pDst[0] = pBlk[0];
                            pDst[1] = pBlk[1];
                            pDst[8] = 0;
                            pDst[5] = pBlk[5];
                            memcpy(pDst + 0xc, pBlk + 0xc, pBlk[2]);   /* the data, after the 0x30 header */
                            LinkDescToBlock((int *)pBlk[0], (int)pDst);
                            /* last[2] is the capacity, byte +8 (0x00433cd8: MOV EAX,[EDX+0x8]) */
                            pAfter = (int *)pDst[3];
                            if (pAfter != NULL && *pAfter == 0) {
                                ((Seg *)gCompactWorkBuf)[tgt].last = pAfter;
                                ((Seg *)gCompactWorkBuf)[tgt].freeB = ((Seg *)gCompactWorkBuf)[tgt].last[2] + 0x30;
                            } else {
                                ((Seg *)gCompactWorkBuf)[tgt].last = pDst;
                                ((Seg *)gCompactWorkBuf)[tgt].freeB = 0;
                            }
                            ((Seg *)gCompactWorkBuf)[segIdx].freeB += pBlk[2] + 0x30;
                            FreeHeapBlock(pBlk[5], pBlk);
                            break;
                        }
                    }
                }
                pBlk = pNext;
            }
        }
        ConsolidateSegment(heapDesc, (int *)&((Seg *)gCompactWorkBuf)[segIdx]);
        if (((Seg *)gCompactWorkBuf)[segIdx].freeB >= cbNeeded)
            break;
    }

    free(gCompactWorkBuf);
}


/* Points the resource descriptor at its block's data (pBlock + 0x30): a resource handle is a
 * pointer to a descriptor whose first word is the data address, so blocks can move.
 * Fatal 0xca on a NULL descriptor. */
void __cdecl LinkDescToBlock(int *pDesc, int pBlock)  /* 0x00433d90 */
{
    if (pDesc == NULL)
        FatalErrorWithCode(0xca);
    *pDesc = pBlock + 0x30;
}


/* Quicksorts rows lo..hi of CompactHeap's segment table by their +0x0c field (free + movable
 * bytes), largest first, with FindSegmentPivot and PartitionSegmentTable; recurses on the left
 * part and loops on the right. */
void __cdecl SortSegmentTable(int lo, int hi)  /* 0x00433dc0 */
{
    int pivotIdx;
    int splitIdx;

    while (1)
    {
        pivotIdx = FindSegmentPivot(lo, hi);
        if (pivotIdx < 0)
            break;
        splitIdx = PartitionSegmentTable(lo, hi,
                     *(int *)(pivotIdx * 0x10 + 0x0c + (int)gCompactWorkBuf));
        SortSegmentTable(lo, splitIdx - 1);
        lo = splitIdx;
    }
}


/* Picks SortSegmentTable's pivot: scans rows lo+1..hi-1 for the first key unlike row lo's and
 * returns the row with the smaller of the two keys, or -1 if every key is equal (nothing to
 * sort). */
int __cdecl FindSegmentPivot(int lo, int hi)  /* 0x00433e10 */
{
    int  pivotVal;
    int  idx;
    int *pKey;

    pivotVal = *(int *)((char *)gCompactWorkBuf + lo * 0x10 + 0x0c);
    idx = lo + 1;

    if (idx < hi)
    {
        pKey = (int *)((char *)gCompactWorkBuf + idx * 0x10 + 0x0c);
        do {
            if (*pKey < pivotVal)
                return idx;
            if (pivotVal < *pKey)
                return lo;
            pKey += 4;          /* the 16-byte row */
            idx++;
        } while (idx < hi);
    }

    return -1;
}


/* Partitions rows lo..hi around pivotVal, swapping rows through gPartitionSwapTemp: rows with
 * a key > pivotVal end up left of the returned index, the rest from it on. */
int __cdecl PartitionSegmentTable(int lo, int hi, int pivotVal)  /* 0x00433e60 */
{
    /* The swap goes through the 16-byte global at 0x0048a9e0 as four inline
     * dword moves (a whole-record struct assignment). */
    typedef struct { int d[4]; } Rec16;
    int *pKey;

    do {
        *(Rec16 *)&gPartitionSwapTemp = ((Rec16 *)gCompactWorkBuf)[lo];
        ((Rec16 *)gCompactWorkBuf)[lo] = ((Rec16 *)gCompactWorkBuf)[hi];
        ((Rec16 *)gCompactWorkBuf)[hi] = *(Rec16 *)&gPartitionSwapTemp;
        pKey = (int *)((char *)gCompactWorkBuf + lo * 0x10 + 0x0c);
        while (*pKey > pivotVal) { pKey += 4; lo++; }
        pKey = (int *)((char *)gCompactWorkBuf + hi * 0x10 + 0x0c);
        while (*pKey <= pivotVal) { pKey -= 4; hi--; }
    } while (lo <= hi);
    return lo;
}


/* Slides the live blocks of one CompactHeap segment (pEntry: {first block, last block, free
 * bytes, ...}) down to its start, repointing each one's descriptor, and turns the free space
 * into one free block at the end that takes over the free-list links; then rebuilds the
 * previous-block links and makes pEntry[1] that free block. Fatal 0xd9 if the chain ends
 * early, 0xda on a locked block, 0xdb on a moved block with no descriptor, 0xdc if the free
 * bytes found differ from pEntry[2], 0xdd if they are less than a header. */
void __cdecl ConsolidateSegment(int heapDesc, int *pEntry)  /* 0x00433f30 */
{
    int *pNew;
    int  savedPrev;      /* the first block's prevPhys, restored after the move */
    int *pBound;         /* boundary = last block's nextPhys */
    int  found;
    unsigned int totalFree;
    int *pBlk;
    int  firstLink;      /* +0x1c nextFree of the first free block */
    int  lastLink;       /* +0x18 prevFree of the last free block */
    int *pWrite;         /* write cursor */

    if (pEntry[2] != 0) {
        pBlk = (int *)pEntry[0];
        pWrite = pBlk;
        savedPrev = pBlk[4];
        pBound = *(int **)(pEntry[1] + 0xc);
        totalFree = 0;
        lastLink = 0;
        firstLink = 0;
        found = 0;
        while (pBlk != pBound) {
            if (pBlk == NULL)
                FatalErrorWithCode(0xd9);
            if (*pBlk != 0) {
                if (pBlk[8] != 0)
                    FatalErrorWithCode(0xda);
                pNew = (int *)((char *)pWrite + pBlk[2] + 0x30);
                if (pWrite != pBlk) {
                    /* Relocate header + data: the exe's inline `rep movsd/movsb`,
                     * a forward copy with pWrite < pBlk */
                    memcpy(pWrite, pBlk, pBlk[2] + 0x30);
                    pBlk = (int *)pWrite[3];
                    pWrite[3] = (int)pNew;
                    if (*pWrite == 0)
                        FatalErrorWithCode(0xdb);
                    LinkDescToBlock((int *)*pWrite, (int)pWrite);
                    pWrite = pNew;
                } else {
                    pBlk = (int *)pBlk[3];
                    pWrite = pNew;
                }
            } else {
                totalFree += pBlk[2] + 0x30;
                if (!found) {
                    found = 1;
                    firstLink = pBlk[7];
                }
                lastLink = pBlk[6];
                pBlk = (int *)pBlk[3];
            }
        }
        if ((unsigned int)pEntry[2] != totalFree)
            FatalErrorWithCode(0xdc);
        if (totalFree != 0) {
            if (totalFree < 0x30)
                FatalErrorWithCode(0xdd);
            pWrite[2] = totalFree - 0x30;
            *pWrite = 0;
            pWrite[8] = 0;
            pWrite[5] = *(int *)(*pEntry + 0x14);
            pWrite[3] = (int)pBlk;
            pWrite[7] = firstLink;
            if (firstLink != 0)
                *(int **)(firstLink + 0x18) = pWrite;
            else
                *(int **)(heapDesc + 4) = pWrite;
            pWrite[6] = lastLink;
            if (lastLink != 0)
                *(int **)(lastLink + 0x1c) = pWrite;
            else
                *(int **)(heapDesc + 8) = pWrite;
            pBlk = (int *)*pEntry;
            pBlk[4] = savedPrev;
            for (pWrite = (int *)pBlk[3]; pWrite != pBound; pWrite = (int *)pWrite[3]) {
                pWrite[4] = (int)pBlk;
                pBlk = pWrite;
            }
            if (pWrite != NULL)
                pWrite[4] = (int)pBlk;
            pEntry[1] = (int)pBlk;
        }
    }
}


/* Makes pDesc this thread's active heap, saving the previous one on the per-thread heap stack
 * (32 deep; fatal 0xf9 when full). PopHeapContext undoes it. */
void __cdecl PushHeapContext(int *pDesc)  /* 0x00434100 */
{
    int *old;
    int d;

    if (g_heapCtxDepth >= 0x20)         /* cmp 0x20 / jl */
        FatalErrorWithCode(0xf9);

    /* the exe reads the depth once and stores the old descriptor last */
    d = g_heapCtxDepth;
    old = g_pActiveHeapDesc;
    g_pActiveHeapDesc = pDesc;
    g_heapCtxDepth = ++d;
    g_heapCtxStack[d] = (int)old;
}


/* Restores the active heap the last PushHeapContext saved. Fatal 0xfa if the stack is empty. */
void __stdcall PopHeapContext(void)  /* 0x00434150 */
{
    if (g_heapCtxDepth == 0)
        FatalErrorWithCode(0xfa);

    /* the exe decrements first and reads stack[depth + 1] (0x00434183) */
    g_heapCtxDepth--;
    g_pActiveHeapDesc = (int *)g_heapCtxStack[g_heapCtxDepth + 1];
}


/* Returns a resource object's size as last requested (AllocResObject's cbData or
 * ResizeResObject's newSize, kept at block +0x04). Fatal 0xe3 on a NULL handle. */
DWORD __cdecl GetResObjectSize(void *pHandle)  /* 0x004341a0 */
{
    int *pBlock;

    if (pHandle == NULL)
        FatalErrorWithCode(0xe3);

    pBlock = ResolveResBlock((int *)pHandle);
    return (DWORD)pBlock[1];
}


/* Returns the heap block behind a resource handle (the data address - 0x30), without taking
 * the heap's lock as LockResDescriptor does. Fatal 0xcb on a NULL handle, 0xcc if that
 * address comes out 0. */
int *__cdecl ResolveResBlock(int *pHandle)  /* 0x004341d0 */
{
    int v;

    if (pHandle == NULL)
        FatalErrorWithCode(0xcb);
    v = *pHandle - 0x30;                /* sub / jne, as the exe has it */
    if (v == 0)
        FatalErrorWithCode(0xcc);
    return (int *)v;
}


/* Resizes a resource object to newSize bytes, its capacity rounded up to 16. Growing tries
 * TryResizeBlock in place first, else moves the data to a new block (compacting once if
 * needed) and repoints the handle; shrinking gives the tail back with ShrinkHeapBlock. The
 * handle stays valid, a data pointer taken before does not. Fatal 0xe4 on a NULL handle, 0xe5
 * if the object is locked, 0xe6 if no block is to be had. */
void __cdecl ResizeResObject(int *pHandle, unsigned int newSize)  /* 0x00434200 */
{
    int  alignedSize;
    int *pNewBlock;
    int *pParent;
    int *pBlock;

    if (pHandle == NULL)
        FatalErrorWithCode(0xe4);

    /* LockResDescriptor returns the heap BLOCK (*pHandle - 0x30) */
    pBlock  = LockResDescriptor(pHandle);
    pParent = (int *)pBlock[5];

    if (pBlock[8] != 0)
        FatalErrorWithCode(0xe5);

    /* Round newSize up to 16 (a signed divide in the exe: cdq idiom) */
    if ((int)newSize % 16 != 0)
        alignedSize = ((int)newSize / 16 + 1) * 16;
    else
        alignedSize = (int)newSize;

    if (alignedSize > pBlock[2])
    {
        if (TryResizeBlock((int)pParent, pBlock, alignedSize) == 0)
        {
            pNewBlock = (int *)AllocHeapBlock((int)pParent, alignedSize);
            if (pNewBlock == NULL)
            {
                CompactHeap((int)pParent, alignedSize + 0x30);
                pNewBlock = (int *)AllocHeapBlock((int)pParent, alignedSize);
                pBlock = ResolveResBlock(pHandle);   /* the compaction may have moved it */
            }

            if (pNewBlock == NULL)
                FatalErrorWithCode(0xe6);

            memcpy((char *)pNewBlock + 0x30, (char *)pBlock + 0x30, pBlock[2]);

            LinkDescToBlock(pHandle, (int)pNewBlock);
            pNewBlock[0] = (int)pHandle;
            pNewBlock[1] = pBlock[1];

            FreeHeapBlock((int)pParent, pBlock);

            pBlock = pNewBlock;
        }
    }
    else if (alignedSize < pBlock[2])
    {
        ShrinkHeapBlock((int)pParent, (int)pBlock, alignedSize);
    }

    pBlock[1]     = (int)newSize;
    pParent[0x0b] = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)(pParent + 5));
}


/* Returns the heap block behind a resource handle with its heap's lock taken and busy flag
 * set; the caller clears the flag and leaves the lock. Fatal 0x108 on a NULL handle. */
int * __cdecl LockResDescriptor(int *pHandle)  /* 0x00434360 */
{
    int iOwner;

    if (pHandle == NULL)
    {
        FatalErrorWithCode(0x108);
    }

    iOwner = *(int *)(*pHandle - 0x1c);   /* block +0x14, the owning heap */
    EnterCriticalSection((LPCRITICAL_SECTION)(iOwner + 0x14));
    *(int *)(iOwner + 0x2c) = 1;

    return (int *)(*pHandle - 0x30);
}


/* Grows an allocated block in place to newSize from the free block physically after it:
 * shaves the bytes needed off its front (its header slides up) or, when they are at least its
 * capacity, absorbs it whole, header included. Returns 1 if the block now holds newSize
 * (or already did), 0 if the caller must move it.
 *
 * Port note: this was once a stub returning 0, and ResizeResObject tested it with the wrong
 * polarity, so no object ever grew; a property scope overran into the next heap block */
int __cdecl TryResizeBlock(int heapDesc, int *pBlock, unsigned int newSize)  /* 0x004343a0 */
{
    int *pMoved;
    int  need;
    int *pNext;

    if (pBlock[2] >= (int)newSize)
        return 1;

    pNext = (int *)pBlock[3];
    if (pNext == NULL)
        return 0;
    if (pNext[0] != 0)
        return 0;

    need = (int)newSize - pBlock[2];

    /* The exe re-reads pNext[2] at each use and puts the too-small return
     * (neighbour too small even if absorbed) last. */
    if (pNext[2] + 0x30 >= need)
    {
        if (need >= pNext[2])
        {
            if (pNext[6] != 0)
                *(int *)(pNext[6] + 0x1c) = pNext[7];
            else
                *(int *)(heapDesc + 8) = pNext[7];

            if (pNext[7] != 0)
                *(int *)(pNext[7] + 0x18) = pNext[6];
            else
                *(int *)(heapDesc + 4) = pNext[6];

            if ((pBlock[3] = pNext[3]) != 0)
                *(int *)(pBlock[3] + 0x10) = (int)pBlock;

            pBlock[2] += pNext[2] + 0x30;
            *(int *)(heapDesc + 0x10) += pNext[2] + 0x30;
        }
        else
        {
            /* memmove, not memcpy: the header moves up by `need` bytes, an overlap
             * whenever need < 0x30 */
            pNext[2] -= need;
            pBlock[2] = (int)newSize;

            pMoved = (int *)((char *)pNext + need);
            memmove(pMoved, pNext, 0x30);
            pBlock[3] = (int)pMoved;

            if (pMoved[3] != 0)
                *(int *)(pMoved[3] + 0x10) = (int)pMoved;

            if (pMoved[6] != 0)
                *(int *)(pMoved[6] + 0x1c) = (int)pMoved;
            else
                *(int *)(heapDesc + 8) = (int)pMoved;

            if (pMoved[7] != 0)
                *(int *)(pMoved[7] + 0x18) = (int)pMoved;
            else
                *(int *)(heapDesc + 4) = (int)pMoved;

            *(int *)(heapDesc + 0x10) += need;
        }

        if (*(int *)(heapDesc + 0xc) < *(int *)(heapDesc + 0x10))
            FatalErrorWithCode(0xcf);   /* in use > pool size */

        return 1;
    }
    return 0;
}


/* Shrinks an allocated block's capacity to newSize: the tail becomes a new free block when it
 * holds more than a header (else it stays with the block), or, when the next block is free,
 * that block's header slides down to take it. Fatal 0xd1 if the in-use count goes negative. */
void __cdecl ShrinkHeapBlock(int pHeap, int pBlock, int newSize)  /* 0x004344B0 */
{
  int *pNext;
  int *pNew;
  unsigned int delta;   /* the old size, then old - new in place (`sub edi,ebp`) */

  delta = *(int *)(pBlock + 8);
  if ((int)delta == newSize)
    return;
  pNext = *(int **)(pBlock + 0xc);
  /* the exe's order: a successor first, the in-use one before the free one */
  if (pNext != NULL) {
    if (*pNext != 0) {
      /* successor in use: split the tail off as a new free block */
      delta -= newSize;
      if (delta <= 0x30)
        goto check;
      pNew = (int *)(pBlock + 0x30 + newSize);
      pNew[0] = 0;
      pNew[2] = *(int *)(pBlock + 8) - newSize - 0x30;
      pNew[8] = 0;
      pNew[5] = pHeap;
      pNew[3] = (int)pNext;
      pNext[4] = (int)pNew;
      pNew[4] = pBlock;
      *(int **)(pBlock + 0xc) = pNew;
      *(int *)(pBlock + 8) = newSize;
      *(int *)(pHeap + 0x10) -= pNew[2] + 0x30;
      InsertFreeBlock(pHeap, (int)pNew);
      goto check;
    }
    /* successor free: slide its header down. The exe calls the CRT copy (0x0044e440),
     * which handles the overlap when delta < 0x30; a plain memcpy does not. */
    delta -= newSize;
    pNew = (int *)((int)pNext - delta);
    memmove(pNew, pNext, 0x30);
    pNew[2] += delta;
    *(int *)(pBlock + 8) = newSize;
    *(int **)(pBlock + 0xc) = pNew;
    if (pNew[3] != 0)
      *(int **)(pNew[3] + 0x10) = pNew;
    if (pNew[6] != 0)
      *(int **)(pNew[6] + 0x1c) = pNew;
    else
      *(int **)(pHeap + 8) = pNew;
    if (pNew[7] != 0)
      *(int **)(pNew[7] + 0x18) = pNew;
    else
      *(int **)(pHeap + 4) = pNew;
    *(int *)(pHeap + 0x10) -= delta;
    goto check;
  }
  /* no successor: split the tail off as a new free block */
  delta -= newSize;
  if (delta <= 0x30)
    goto check;
  pNew = (int *)(pBlock + 0x30 + newSize);
  pNew[0] = 0;
  pNew[2] = *(int *)(pBlock + 8) - newSize - 0x30;
  pNew[8] = 0;
  pNew[5] = pHeap;
  pNew[3] = 0;
  pNew[4] = pBlock;
  *(int *)(pBlock + 8) = newSize;
  *(int **)(pBlock + 0xc) = pNew;
  *(int *)(pHeap + 0x10) -= pNew[2] + 0x30;
  InsertFreeBlock(pHeap, (int)pNew);
check:
  if (*(int *)(pHeap + 0x10) < 0)
    FatalErrorWithCode(0xd1);
}


/* Allocates a movable resource object of cbData bytes (capacity rounded up to 16) in the
 * active heap, compacting once if needed, and returns its handle: a descriptor whose first
 * word is the data address. GetResDataPtr locks it for use. Fatal 0xe7 if no block is to be
 * had, 0xe8 if no descriptor is. */
void * __cdecl AllocResObject(DWORD cbData)  /* 0x004345e0 */
{
    int  *pHeapRef;
    int   cbAligned;
    int  *pBlock;
    int  *pDesc;

    /* the active heap is re-read through the TLS slot at every use */
    pHeapRef = (int *)&g_pActiveHeapDesc;
    EnterCriticalSection((LPCRITICAL_SECTION)(*pHeapRef + 0x14));
    *(int *)(*pHeapRef + 0x2c) = 1;

    /* signed divide in the exe: cdq idiom */
    if ((int)cbData % 16 != 0)
        cbAligned = ((int)cbData / 16 + 1) * 16;
    else
        cbAligned = (int)cbData;

    pBlock = AllocHeapBlock(*pHeapRef, cbAligned);
    if (pBlock == NULL)
    {
        CompactHeap(*pHeapRef, cbAligned + 0x30);
        pBlock = AllocHeapBlock(*pHeapRef, cbAligned);
        if (pBlock == NULL)
            FatalErrorWithCode(0xe7);
    }

    pDesc = AllocDescriptor();
    if (pDesc == NULL)
        FatalErrorWithCode(0xe8);

    pBlock[0] = (int)pDesc;
    pBlock[1] = (int)cbData;
    LinkDescToBlock(pDesc, (int)pBlock);

    *(int *)(*pHeapRef + 0x2c) = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)(*pHeapRef + 0x14));

    return (void *)pDesc;
}


/* Pops a descriptor off the free pool, allocating a new page when the pool is empty.
 * Ghidra shows it void; every caller takes the descriptor from eax. */
int * __stdcall AllocDescriptor(void)  /* 0x004346c0 */
{
    int *pDesc;
    if (gDescFreeList == NULL)
        AllocDescriptorPage();
    pDesc          = gDescFreeList;
    gDescFreeList = (int *)pDesc[1];
    return pDesc;
}


/* Frees a resource object: its descriptor goes back to the pool and its block to the heap.
 * Fatal 0xe9 if it is still locked. (pDesc here is the block LockResDescriptor returns.) */
void __cdecl FreeResObject(int *pHandle)  /* 0x004346e0 */
{
    int *pDesc;
    int  iOwner;

    pDesc  = LockResDescriptor(pHandle);
    iOwner = pDesc[5];

    if (pDesc[8] != 0)
    {
        FatalErrorWithCode(0xe9);
    }

    ReleaseDescriptor(pHandle);
    FreeHeapBlock(iOwner, pDesc);

    *(int *)(iOwner + 0x2c) = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)(iOwner + 0x14));
}


/* Returns a descriptor to the END of the free pool (gDescLastEntry), cleared, so a freed
 * handle is reused as late as possible. Fatal 0xc9 on NULL. */
void __cdecl ReleaseDescriptor(int *pDesc)  /* 0x00434730 */
{
    if (pDesc == NULL)
        FatalErrorWithCode(0xc9);

    *(int **)((char *)gDescLastEntry + 4) = pDesc;
    pDesc[1]        = 0;
    pDesc[0]        = 0;
    gDescLastEntry = pDesc;

    if (gDescFreeList == NULL)
        gDescFreeList = pDesc;
}


/* Locks a resource object (lock count +1) and returns its data address, which stays valid
 * until the matching UnlockResObject: a locked block is never moved by the compactor. */
int __cdecl GetResDataPtr(int *pHandle)  /* 0x00434780 */
{
    int *pDesc;

    pDesc = LockResDescriptor(pHandle);

    pDesc[8]++;

    *(int *)(pDesc[5] + 0x2c) = 0;                                /* the owner read twice, as the exe does */
    LeaveCriticalSection((LPCRITICAL_SECTION)(pDesc[5] + 0x14));

    return (int)pDesc + 0x30;
}


/* Undoes one GetResDataPtr (lock count -1); the data address may move once the count is 0.
 * Fatal 0xea if the object is not locked.
 *
 * Port note: void. The exe returns whatever LeaveCriticalSection left in eax, and 24 callers
 * had transcribed that as a status (class 18). */
void __cdecl UnlockResObject(int *pHandle)  /* 0x004347b0 */
{
    int *pDesc;

    pDesc = LockResDescriptor(pHandle);

    if (pDesc[8] == 0)
    {
        FatalErrorWithCode(0xea);
    }

    pDesc[8]--;

    *(int *)(pDesc[5] + 0x2c) = 0;                                /* the owner read twice, as the exe does */
    LeaveCriticalSection((LPCRITICAL_SECTION)(pDesc[5] + 0x14));
}


/* Returns the data address of a resource object that is already locked, without locking it
 * again. Fatal 0xeb if it is not locked. */
int *__cdecl GetLockedResDataPtr(int *pHandle) /* 0x004347F0 */
{
    int *pDesc;

    pDesc = LockResDescriptor(pHandle);
    if (pDesc[8] == 0)
        FatalErrorWithCode(0xeb);
    *(uint32_t *)(pDesc[5] + 0x2c) = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)(pDesc[5] + 0x14));
    return pDesc + 0xc; /* +0x30 bytes, past the block header */
}


/* Moves an unlocked resource object up into the highest free block above it that fits, if
 * there is one (RelocateBlockToHighestFit), and repoints its handle. Fatal 0xe4 on a NULL
 * handle, 0xe5 if the object is locked. */
void __cdecl MoveResObjectToHeapTop(int *pHandle)  /* 0x00434830 */
{
    int *pDesc;
    int  iOwner;
    int *pTarget;

    if (pHandle == NULL)
        FatalErrorWithCode(0xe4);

    pDesc  = LockResDescriptor(pHandle);
    iOwner = pDesc[5];

    if (pDesc[8] != 0)
        FatalErrorWithCode(0xe5);

    pTarget = RelocateBlockToHighestFit(pDesc);
    if (pTarget == NULL)
    {
        *(int *)(iOwner + 0x2c) = 0;
        LeaveCriticalSection((LPCRITICAL_SECTION)(iOwner + 0x14));
        return;
    }

    LinkDescToBlock(pHandle, (int)pTarget);
    *(int *)(iOwner + 0x2c) = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)(iOwner + 0x14));
}


/* Moves the block pDesc into the highest free block above it with room for its capacity,
 * carved from that block's top end (CarveFromBlock), and frees the old block. Returns the new
 * block, or NULL if no free block above it fits. The caller repoints the descriptor. */
int *__cdecl RelocateBlockToHighestFit(int *pDesc)  /* 0x004348b0 */
{
    int *p;
    int *pChosen;

    for (p = pDesc; p != NULL && *p != 0; p = (int *)p[3])
        ;
    pChosen = NULL;
    for (; p != NULL; p = (int *)p[6]) {
        if (pDesc[2] <= p[2])
            pChosen = p;
    }
    if (pChosen == NULL)
        return NULL;

    CarveFromBlock(&pChosen, pDesc[2]);
    /* the data, after the header: an inlined memcpy in the exe (rep movsd/movsb) */
    memcpy(pChosen + 12, pDesc + 12, pDesc[2]);

    *pChosen   = *pDesc;
    pChosen[1] = pDesc[1];
    pChosen[8] = pDesc[8];
    FreeHeapBlock(pDesc[5], pDesc);
    return pChosen;
}


/* SplitHeapBlock's twin that takes the space from the TOP of the free block *ppBlock: if the
 * rest would not hold a header and one byte, the whole block is unlinked and used; otherwise
 * a new cbSize block is cut from its end and the free block keeps the front. *ppBlock is set
 * to the allocated block. Fatal 0x10b if the in-use count passes the pool size. */
static void __cdecl CarveFromBlock(int **ppBlock, int cbSize)  /* 0x00434960 */
{
    unsigned int remaining;
    int         *pFree;
    int          heapDesc;
    int         *pNew;

    /* The exe re-reads every link field where it uses it (0x0043497b..). */
    pFree = *ppBlock;
    heapDesc = pFree[5];
    remaining = (unsigned int)pFree[2] - (unsigned int)cbSize;
    if (remaining <= 0x30u) {
        if (pFree[6] != 0)
            *(int *)(pFree[6] + 0x1c) = pFree[7];
        else
            *(int *)(heapDesc + 8) = pFree[7];
        if (pFree[7] != 0)
            *(int *)(pFree[7] + 0x18) = pFree[6];
        else
            *(int *)(heapDesc + 4) = pFree[6];
        *(int *)(heapDesc + 0x10) += pFree[2] + 0x30;
    } else {
        /* the new block's free-list words are stale copies; it is not on the list */
        pNew = (int *)((char *)pFree + remaining);
        pNew[4] = (int)pFree;
        pNew[3] = pFree[3];
        if (pFree[3] != 0)
            *(int *)(pFree[3] + 0x10) = (int)pNew;
        pFree[3] = (int)pNew;
        pNew[6] = pFree[6];
        pNew[7] = (int)pFree;
        pNew[0] = 0;
        pNew[2] = cbSize;
        pNew[8] = 0;
        pNew[5] = heapDesc;
        pFree[2] -= cbSize + 0x30;
        *(int *)(heapDesc + 0x10) += cbSize + 0x30;
        pFree = pNew;
    }

    if (*(int *)(heapDesc + 0x0c) < *(int *)(heapDesc + 0x10))
        FatalErrorWithCode(0x10b);

    *ppBlock = pFree;
}



/* MoveResObjectToHeapTop's mirror: moves an unlocked resource object down into the lowest
 * free block below it that fits, if any, and repoints its handle. Fatal 0xe4 on a NULL
 * handle, 0xe5 if the object is locked. */
void __cdecl MoveResObjectToHeapBottom(int *pHandle)  /* 0x00434a20 */
{
    int *pDesc;
    int  iOwner;
    int *pTarget;

    if (pHandle == NULL)
        FatalErrorWithCode(0xe4);

    pDesc  = LockResDescriptor(pHandle);
    iOwner = pDesc[5];

    if (pDesc[8] != 0)
        FatalErrorWithCode(0xe5);

    pTarget = RelocateBlockToLowestFit(pDesc);
    if (pTarget == NULL) {
        *(int *)(iOwner + 0x2c) = 0;
        LeaveCriticalSection((LPCRITICAL_SECTION)(iOwner + 0x14));
        return;
    }
    LinkDescToBlock(pHandle, (int)pTarget);
    *(int *)(iOwner + 0x2c) = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)(iOwner + 0x14));
}



/* RelocateBlockToHighestFit's mirror: moves the block pDesc into the lowest free block below
 * it with room, taken from that block's bottom end (SplitHeapBlock), and frees the old block.
 * Returns the new block or NULL. */
int *__cdecl RelocateBlockToLowestFit(int *pDesc)  /* 0x00434AA0 */
{
    int *p;
    int *pBest;

    for (p = pDesc; p != NULL && *p != 0; p = (int *)p[4])
        ;
    pBest = NULL;
    for (; p != NULL; p = (int *)p[7]) {
        if (pDesc[2] <= p[2])
            pBest = p;
    }
    if (pBest == NULL)
        return NULL;
    SplitHeapBlock((char *)pBest, pDesc[2]);
    /* the payload copy is an inlined memcpy in the exe (rep movsd/movsb) */
    memcpy(pBest + 0xc, pDesc + 0xc, pDesc[2]);
    pBest[0] = pDesc[0];
    pBest[1] = pDesc[1];
    pBest[8] = pDesc[8];
    FreeHeapBlock(pDesc[5], pDesc);
    return pBest;
}


/* AllocResObject for a fixed buffer: the block comes from the TOP of the active heap
 * (FindFreeBlock), away from the movable objects at the bottom, and is returned already
 * locked, as a data pointer (lock count 1). FreeEngineBuffer frees it. Fatal 0x10c if no
 * block is to be had, 0x10d if no descriptor is. */
void * __cdecl AllocEngineBuffer(DWORD size)  /* 0x00434b30 */
{
    int  *pHeapRef;
    int   alignedSize;
    int  *pBlock;
    int  *pDesc;

    pHeapRef = (int *)&g_pActiveHeapDesc;
    EnterCriticalSection((LPCRITICAL_SECTION)(*pHeapRef + 0x14));
    *(int *)(*pHeapRef + 0x2c) = 1;

    if ((int)size % 16 != 0)
        alignedSize = ((int)size / 16 + 1) * 16;
    else
        alignedSize = (int)size;

    pBlock = FindFreeBlock(*pHeapRef, alignedSize);
    if (pBlock == NULL) {
        CompactHeap(*pHeapRef, alignedSize + 0x30);
        pBlock = FindFreeBlock(*pHeapRef, alignedSize);
        if (pBlock == NULL)
            FatalErrorWithCode(0x10c);
    }

    pDesc = AllocDescriptor();
    if (pDesc == NULL)
        FatalErrorWithCode(0x10d);

    pBlock[0] = (int)pDesc;
    pBlock[1] = (int)size;
    LinkDescToBlock(pDesc, (int)pBlock);

    *(int *)(*pHeapRef + 0x2c) = 0;
    LeaveCriticalSection((LPCRITICAL_SECTION)(*pHeapRef + 0x14));

    return GetResDataPtr(pDesc);
}


/* First fit from the high end: walks the free list from the head and cuts cbSize bytes from
 * the top of the first block with room (CarveFromBlock). Returns the block, or NULL. */
static int * __cdecl FindFreeBlock(int heapDesc, int cbSize)  /* 0x00434c20 */
{
    int *pBlock;

    for (pBlock = (int *)*(int *)(heapDesc + 8); pBlock != NULL;
         pBlock = (int *)*(int *)((char *)pBlock + 0x1c)) {
        if (cbSize <= *(int *)((char *)pBlock + 8)) {
            CarveFromBlock(&pBlock, cbSize);
            return pBlock;
        }
    }
    return NULL;
}


/* Frees a buffer from AllocEngineBuffer, given its data pointer (the block header is the 0x30
 * bytes before it). Fatal 0xec if its lock count is not exactly 1. */
void FreeEngineBuffer(int pBlock)  /* 0x00434c70 */
{
    int               *pHdr;
    int                pHeapDesc;
    LPCRITICAL_SECTION pCs;

    pHeapDesc = *(int *)(pBlock - 0x1c);     /* header +0x14, the owning heap */
    pHdr      = (int *)(pBlock - 0x30);
    pCs       = (LPCRITICAL_SECTION)(pHeapDesc + 0x14);

    EnterCriticalSection(pCs);
    *(int *)(pHeapDesc + 0x2c) = 1;
    if (pHdr[8] != 1)
        FatalErrorWithCode(0xec);
    ReleaseDescriptor((int *)pHdr[0]);
    FreeHeapBlock(pHeapDesc, pHdr);
    *(int *)(pHeapDesc + 0x2c) = 0;
    LeaveCriticalSection(pCs);
}


/* FatalError(999, errCode): the heap and resource code's fatal errors, reported as message 999
 * with the code. */
void __cdecl FatalErrorWithCode(short errCode)  /* 0x00434cd0 */
{
    FatalError(999, errCode);
}

