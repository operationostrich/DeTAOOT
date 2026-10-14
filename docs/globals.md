# Global Variables

Prose about the globals, grouped by subsystem. **The declarations of record are
`include/df_globals.h` and `src/data/globals.c`**; `match/globals.tsv` lists, per address, the port
name and the identifier the byte-match build knows it by. Rows here that show a type are
documentation, not the declaration.

## CPU Detection (set in DetectCPUType)

| Address | C name | Type | Notes |
|---|---|---|---|
| 0x00486bf4 | `gIs386Cpu` | `int` | Is-386 flag: 1 if `PROCESSOR_INTEL_386` (`dwProcessorType==0x182`), else 0 |

## Windows Version Detection (set in CheckWindowsVersion)

| Address | C name | Type | Notes |
|---|---|---|---|
| 0x004885e0 | `gIsWin95` | `int` | Win-class flag: 1 = non-NT Win32 consumer OS (Win 95/98); 0 otherwise |
| 0x004883b4 | `gIsWin31` | `int` | Win-class flag: 1 = Win 3.1x; read by `CheckDisplayColourDepth` to bypass the yes/no display-change dialog (Win3.x cannot call `ChangeDisplaySettingsA`) |
| 0x00486e64 | `gIsWin95OrNT4` | `int` | Capability flag: 1 = `ChangeDisplaySettingsA` available (Win 95, or NT 4.0 build > 1380) |
| 0x0045b720 | `gWinNTBuild` | `DWORD` | NT OS build number — written by `IsWindowsNT()` from `osvi.dwBuildNumber` (`GetVersionExA`); compared against 0x564 (1380) in `CheckWindowsVersion` to distinguish NT 3.51 (build 1057) from NT 4.0 (build 1381) |
| 0x0045bbf8 | `kStrEmpty` | `const char[4]` | `""` — empty version-type prefix for non-NT Windows |
| 0x0045bbfc | `kStrNtPrefix` | `const char[4]` | `"NT "` — version-type prefix injected into error message format strings |

## Joystick (set in InitJoystick)

| Address | C name | Type | Notes |
|---|---|---|---|
| 0x00485f18 | `gJoystickPresent` | `int` | Joystick-present flag: 1 = a joystick was detected and initialised, 0 = no joystick |
| 0x00486270 | `gJoystickId` | `int` | Active joystick index: 0 = JOYSTICKID1, 1 = JOYSTICKID2 |
| 0x00486c24 | `gJoystickCenterX` | `UINT` | Joystick X-axis centre = `(wXmin + wXmax) >> 1` |
| 0x00486e60 | `gJoystickCenterY` | `UINT` | Joystick Y-axis centre = `(wYmin + wYmax) >> 1` |
| 0x004862d4 | `gJoystickRangeX` | `int` | X-axis half-range = `wXmax − X_centre` |
| 0x004862c8 | `gJoystickRangeY` | `int` | Y-axis half-range = `wYmax − Y_centre` |
| 0x00486c20 | `gJoystickReleaseThreshold` | `DWORD` | Release threshold of the joystick hysteresis: 90000 -- deflected -> centred when the squared distance from centre drops below it (`PollJoystickDirection`) |
| 0x00486e74 | `gJoystickDeflectThreshold` | `DWORD` | Deflect threshold of the joystick hysteresis: 160000 -- centred -> deflected when the squared distance from centre exceeds it; both thresholds compare the same squared distance, not one axis each |
| 0x00487328 | `gJoystickDirection` | `short` | Last joystick 8-direction code reported by `PollJoystickDirection`; non-zero = deflected; reset to 0 during init |
| 0x0045b714 | `gJoy1PrevButtons` | `unsigned int` | Previous joystick 1 button bitmask; bits 0–3 = buttons 1–4; updated each frame by `PollJoystickInput` |
| 0x0045b718 | `gJoy2PrevButtons` | `unsigned int` | Previous joystick 2 button bitmask; bits 0–3 = buttons 1–4; updated each frame by `PollJoystickInput` |
| 0x0045b71c | `gJoyRepeatTime` | `DWORD` | Timestamp (`timeGetTime`) of last auto-repeat event; 200 ms repeat interval |

## Event Queue (used by PollJoystickInput / DequeueGameEvent)

| Address | C name | Type | Notes |
|---|---|---|---|
| 0x00486970 | `gEventQueue` | `unsigned char[]` | Event queue base; each entry is 18 (0x12) bytes: 16 bytes data + 2-byte type field at offset 0x10 |
| 0x00486278 | `gEventQueueCount` | `int` | Event queue entry count; decremented by `DequeueGameEvent`, incremented by `EnqueueInputEvent` |

## Memory Manager (set in InitMemoryManager / AllocMemoryPool)

| Address | C name | Type | Notes |
|---|---|---|-----------|
| 0x00485f58 | `gTotalPhysRam` | `DWORD` | Total physical RAM — `GlobalMemoryStatus.dwTotalPhys`; written by both `InitMemoryManager` and `AllocMemoryPool` |
| 0x00486e6c | `gAvailPhysBytes` | `DWORD` | Available physical RAM — `GlobalMemoryStatus.dwAvailPhys`; written by `AllocMemoryPool` |
| 0x004883c0 | `gAvailPageFileBytes` | `DWORD` | Available page-file space — `GlobalMemoryStatus.dwAvailPageFile`; written by `AllocMemoryPool` |
| 0x00487538 | `gPoolMinBytes` | `DWORD` | Pool config minA — string ID 6 (`"4000"` KB) × 1024 ≈ 3.9 MB; minimum pool / failure threshold |
| 0x004883c4 | `gPoolMaxBytes` | `DWORD` | Pool config maxB — string ID 7 (`"12000"` KB) × 1024 ≈ 11.7 MB; pool allocation cap |
| 0x00486c54 | `gPoolReserveBytes` | `DWORD` | Pool config baseC — string ID 8 (`"512"` KB) × 1024 + 4 MB base ≈ 4.5 MB; OS headroom reserve subtracted during pool-size calculation |
| 0x004883bc | `gProbedUsableBytes` | `SIZE_T` | Probed usable RAM — result of `AllocMemoryPool` probe loop; stored before pool-size calculation |
| 0x00486274 | `gPoolSizeBytes` | `SIZE_T` | Calculated pool size in bytes; 0 = not enough RAM (probe < minA) |
| 0x004862d0 | `gPoolBase` | `void *` | Game memory pool base pointer — written by `AllocMemoryPool` on successful allocation; passed to `InitHeap` in `AppInit` |
| 0x00486e70 | `gPoolAllocatedBytes` | `SIZE_T` | Game memory pool size in bytes — written by `AllocMemoryPool`; passed to `InitHeap` in `AppInit` |
| 0x0045bc30 | `s_OUTOFMEM_0045bc30` | `const char[]` | `"OUTOFMEM"` — dialog resource name passed to `DialogBoxParamA` on failure |
| 0x0045bc3c | `kStrPctLd` | `const char[]` | `"%d"` (inferred) — `_sscanf` format for parsing KB values from string IDs 6/7/8 |
| 0x0045b724 | `gVxdDevice` | `HANDLE` (stored as `int`) | Win 9x only: open Win32 handle to the `VFIXMEM.VXD` device; 0 = not open; written by `OpenVxDDevice`; read by `QueryVxDAvailMem` and `AllocVxDBlock` |
| 0x0045bc40 | `kStrVfixmemVxd` | `const char[]` | `"FIXMEM.VXD"` — VxD filename appended to Windows dir in `OpenVxDDevice`; Ghidra CSV shows `0x0045bc41` (off by one) |
| 0x0045bc4c | `kStrBackslash` | `const char[]` | `"\\"` — path separator appended by `OpenVxDDevice` when `%WINDIR%` has no trailing backslash |
| 0x0045bc7c | `kStrDriveA` | `uint32_t` | Rodata-style dword `0x005c3a41` (little-endian `41 3A 5C 00` = ASCII `"A:\\"` plus a trailing NUL packed into the low four bytes). In the decompile it is initialized in `scene.c`. When `gIsWin31 != 0` (Win 3.1x), `GetFirstCdRomDriveLetter` (`0x004330d0`, checkcd.c) builds a multi-root drive list from `GetLogicalDrives()`, copying each present root starting from this template and incrementing **only** the low byte (`A`→`B`→…) between bitmask bits |

## Application Instance / Window

| Address | C name | Type | Set in | Notes |
|---|---|---|---|---|
| 0x00488264 | `gInstance` | `HINSTANCE` | `AppInit` | App HINSTANCE |
| 0x00488268 | `gTempFontPath` | `CHAR[MAX_PATH]` | `ManageEmbeddedFont` | Temp font file path (`.FNT`); `[0]=='\0'` means no font installed |
| 0x00486768 | `gAppTitle` | `char[0x40]` | `AppInit` | App title string (resource ID 0); substituted for `%p` by `GetStringResource` |
| 0x004867e8 | `gStringResourceBuf` | `char[384]` | `GetStringResource` | Static string buffer; result of `GetStringResource`; overwritten each call |
| 0x0045b94c | `kStrPctP` | `char[3]` | (rodata) | Placeholder pattern `"%p"` — searched and replaced by `GetStringResource` |
| 0x00486460 | `gCoverWindow` | `HWND` | `AppInit` | Cover/splash window handle |
| 0x004872a8 | `gThePortDesc` | `int[32]` | scene | 32-int scene-state snapshot; written by `QdSetPort`; `[0]` = primary scene object handle (doubles as the scene node HWND when non-zero) |
| 0x00488150 | `gMainMenu` | `HMENU` | `AppInit` | Main menu handle; used by `DispatchNavCommand` via `GetMenuStringA` |
| 0x00486160 | `gWatchCursor` | `HCURSOR` | `AppInit` | Watch cursor (CURS.WATCH) |

## Display Mode (set in InitDisplayMode)

| Address | C name | Type | Notes |
|---|---|---|---|
| 0x0045b710 | `gUser32Lib` | `HMODULE` | Loaded `USER32.DLL` handle; 0 if not loaded or procs missing |
| 0x0048732c | `gChangeDisplaySettingsProc` | proc ptr | `ChangeDisplaySettingsA` from USER32 |
| 0x004863ac | `gEnumDisplaySettingsProc` | proc ptr | `EnumDisplaySettingsA` from USER32 |
| 0x00486c58 | `gExitWindowsExProc` | proc ptr | `ExitWindowsEx` from USER32 |
| 0x00486298 | `gPreferredDisplayMode` | `int` | Preferred display mode index (from `FindBestDisplayMode` slot 0) |
| 0x0048627c | `gAlternateDisplayMode` | `int` | Alternate display mode index (from `FindBestDisplayMode` slot 1); < 0 = none found |
| 0x00486bf0 | `gIsDisplayModeChanged` | `int` | Display-state flag; zeroed by `InitDisplayMode` after mode search succeeds; set to 1 by `ApplyDisplayMode` after `ChangeDisplaySettingsA` succeeds |
| 0x0045b70c | `gSavedWindowCount` | `int` | Saved-window count; zeroed by `SaveWindowPositions` before `EnumWindows`; callback `LAB_0041b840` populates entries; `RestoreDisplayMode` reads + clears |
| 0x004885e8 | `gSavedWindowPositions` | `SavedWndPos[]` | Saved-window-position array; each entry = `{HWND, left, top, right, bottom}`; populated by `LAB_0041b840` callback during `SaveWindowPositions`; restored by `RestoreDisplayMode` |
| 0x00486c2c | `gIsMenuVisible` | `short` | Menu-visible flag: set by `RestoreMenuBar`, cleared by `HideMenuBar`, read by `ScriptCmd_MenuVisible` (`menuvisible`); cleared to 0 early in `AppInit`, set to 1 by `InitializeEngineState` |
| 0x004885d4 | `gCustomPaletteFlag` | `int` | Colour-depth flag: 0 = palettised (8 bpp), non-0 = true-colour. Checked by `FatalError` to decide whether to unload palette before showing dialog |
| 0x00489d94 | `DAT_00489d94` | `int` (or struct head) | Zeroed early in `AppInit`; Ghidra warns of overlapping larger symbol (`_DAT_00489d94`) — likely first field of a struct at this address; purpose TBD |
| 0x0048a9b0 | `gPrimaryHeap` | struct (heap descriptor) | Heap descriptor struct; address published to TLS slot 2; contains critical section at +0x14 and shutdown flag at +0x2C; initialised by `InitHeap`, torn down by `ShutdownHeap` |
| 0x0048a9c4 | `DAT_0048a9c4` | `CRITICAL_SECTION` | Critical section embedded in heap descriptor (offset +0x14 from `gPrimaryHeap`); guards heap operations; destroyed by `ShutdownHeap` |
| 0x0048a9dc | `DAT_0048a9dc` | `int` | Heap shutdown flag (offset +0x2C from `gPrimaryHeap`); 1 while teardown in progress; set by `ShutdownHeap` |

## OS Version (set in entry())

| Address | C name | Type | Notes |
|---|---|---|---|
| 0x0045e28c | `_osver` | `DWORD` | Low byte of `GetVersion()` (OS minor) |
| 0x0045e290 | `_winminor` | `DWORD` | Byte 1 of `GetVersion()` (Windows minor) |
| 0x0045e284 | `_winver` | `DWORD` | High word of `GetVersion()` |
| 0x0045e288 | `_osver_packed` | `DWORD` | `_osver * 0x100 + _winminor` |

## CRT / Process

| Address | C name | Type | Notes |
|---|---|---|---|
| 0x0048e118 | `__argv_raw` | `byte *` | Raw `GetCommandLineA()` pointer |
| 0x0045e2d0 | `__envp_raw` | `LPVOID` | `crtGetEnvironmentStringsA` result |

## Command-line Flags (set in AppInit)

Token strings live in the binary `.data` section, 4 bytes apart. Exact values unknown; placeholder strings used in the decompile.

| Address | Token sym | Type | Flag global | Meaning |
|---|---|---|---|---|
| 0x0045bb08 | `kStrOptE` | `const char[4]` | `gNoEjectFlag` | Flag 5 (unknown purpose) |
| 0x0045bb0c | `kStrOptS` | `const char[4]` | `gStereoSoundFlag` | Flag 4 (unknown purpose) |
| 0x0045bb10 | `kStrOpt16` | `const char[4]` | `gSound16BitFlag` | Flag 3 (unknown purpose) |
| 0x0045bb14 | `kStrOptJ` | `const char[4]` | `gNoJoystickFlag` | Flag 2 — joystick init bypass |
| 0x0045bb18 | `kStrOptR` | `const char[4]` | `gNoDisplayChangeFlag` | Flag 1 — no display change |
| 0x0045bb1c | `kStrOptHelp` | `const char[4]` | (none) | Bad-launch-context token 1 — triggers `ShowFatalDialog` |
| 0x0045bb20 | `kStrOptH` | `const char[4]` | (none) | Bad-launch-context token 2 — triggers `ShowFatalDialog` |

## GDI Stock Objects (set in AppInit)

| Address | Stock object constant | Value |
|---|---|---|
| 0x00487330 | `NULL_BRUSH` | 5 |
| 0x004885d8 | `BLACK_BRUSH` | 4 |
| 0x004867a8 | `WHITE_BRUSH` | 0 |
| 0x004867e0 | `NULL_PEN` | 8 |
| 0x004885d0 | `BLACK_PEN` | 7 |
| 0x00486288 | `SYSTEM_FONT` | 13 |
| 0x0048628c | `DEFAULT_PALETTE` | 15 |

## Accelerator Table

| Address | C name | Type | Notes |
|---|---|---|---|
| 0x00486e68 | `g_hAccel` | `HACCEL` | Loaded from `ACCELTABLE` resource |
| 0x00486e78 | `gNavCommandCount` | `DWORD` | Entry count for `gNavCommandTable`; also walked as ACCEL table (last-entry flag 0x80) |
| 0x00486c50 | `gNavCommandTable` | `byte *` | Base of stride-8 nav command entries; entry layout: [+2]=key char (uppercase), [+4]=menu command ID (ushort); also used as `LocalAlloc`'d ACCEL buffer |

## Display / Palette

| Address | Type | Notes |
|---|---|---|
| 0x0045b618 | `int` | `gGamePaused` — 0 = active (PeekMessageA drain path); non-0 = paused/lost-focus (blocks in GetMessageA at NORMAL priority until cleared, e.g. by WM_ACTIVATEAPP handler); zeroed by both AppInit and Shutdown |
| 0x00485f4c | `HPALETTE` | `gPalette` — logical palette handle created by `SyncLogicalPalette` via `CreatePalette`; NULL-initialised in AppInit before first call; deleted by `DeleteObject` in Shutdown |
| 0x00487d48 | `WORD` | `palVersion` field of the engine's `LOGPALETTE` (= 0x0300); `palNumEntries` (0x0100) follows at 0x00487d4a, then `gPalEntries[256]` at 0x00487d4c |
| 0x0048754a | `PAL16ENTRY[256]` | 16-bit colour table; each channel = 8-bit value « 8 stored in a WORD; stride 8; used by `SetPaletteEntry` |
| 0x00486ea8 | `RGBQUAD[256]` | DIB colour table for `SetDIBColorTable`; layout B, G, R, reserved; filled by `SetPaletteEntry` |
| 0x00487d4c | `PALETTEENTRY[256]` | Logical palette entries for `AnimatePalette` / `SetPaletteEntries`; peFlags set by `SetPaletteEntry` |
| 0x0045b6ec | `int` | `gPalOwnerState` — current palette ownership: 0 = Windows (`SYSPAL_STATIC`), 1 = game (`SYSPAL_NOSTATIC`); written by `ManageSystemPalette` |
| 0x0045b620 | `const int[25]` | `kSysPalIDs` — const array of 25 Win32 system-colour element IDs passed to `SetSysColors` on palette release |
| 0x00489c10 | `COLORREF[25]` | `gSavedSysColors` — 25 system UI COLORREF values saved by `SaveSystemColors` (0x0041b750) before stealing the palette; restored on release |
| 0x0045b688 | `const COLORREF[25]` | `kGameSysColors` — static game-defined replacement COLORREFs for the 25 system UI colour slots; written by `ApplyGameSysColors` (0x0041b780) |

## Scene Viewport (set in CreateSceneViewport)

| Address | C name | Type | Notes |
|---|---|---|---|
| 0x004883c8 | `gThePort` | `void *` | Active scene-reference object; set by `QdSetPort` via `QdSetPortWindow`/`QdSetPortOffscreen`; read by `QdClipRect`, `QdPenNormal`, `QdSetOrigin`; NULL when no scene active |
| 0x0048836c | `gViewportCount` | `int` | Viewport count; incremented each time `CreateSceneViewport` is called; zeroed by `AppInit` |
| 0x004863dc | `gMainWindow` | `int` (HWND) | Primary viewport HWND; set from the first viewport created; zeroed by `AppInit`; used by `RestoreMenuBar` |
| 0x00486358 | `gIsAppActive` | `int` | App-active flag: set / cleared by `HandleActivateAppMessage` (WM_ACTIVATEAPP); gates paint events and scene-DC drawing, `QdInvertRect` (0x00420170) among them |
| 0x00488370 | `gViewportWasVisible` | `int[]` | Per-viewport state/flags array; `[i]` cleared to 0 by `CreateSceneViewport` |
| 0x00486bb0 | `gViewportTable` | `void *[]` | Per-viewport struct pointer array; `[i]` = ViewportCtx pointer |

## Game Loop State (set in RunGameLoop)

| Address | Type | Notes |
|---|---|---|
| 0x0048a990 | `int` | Loop control: 0=run, 1=exit, 2=restart outer loop |
| 0x0048a604 | `int` | Current scene descriptor |
| 0x0048a2c0 | `short[4]` | a QuickDraw rect `{y1, x1, y2, x2}` (`SceneRect`, include/topics/gameloop.h), NOT `{originX, originY, viewH, viewW}`: `ResetSceneRectToScreen` sets it to `{0,0,SM_CYSCREEN,SM_CXSCREEN}` and the msg-0x10 centring code uses left+right; `DAT_0048a2c4` is the `{y2,x2}` half read as one int; also passed to `UpdateScrollPos` |
| 0x0048a560 | `ChannelState` (146 B) | Input channel A state buffer (type 2 = animation); initialised by `InitMessagesChannel` with viewport strip (37px tall, 436px wide) |
| 0x0048a5f2 | `ChannelState` (146 B) | Input channel B state buffer (type 1 = scene-node); initialised by `InitSceneNodeChannel` from “Runtime” resource |
| 0x0048a684 | `ChannelState` (146 B) | Input channel C state buffer (type 3 = multimedia); initialised by `InitDebugger` with playback viewport (200px tall, offset right of centre) |
| 0x0048a0d0 | `int` | Scene-node interaction state; cleared to 0 by `InitSceneNodeChannel` |
| 0x0048a572 | `void *` | Channel A resource handle (offset 0x12 into `gChannels`); non-zero = already initialised (fatal guard in `InitMessagesChannel`) |
| 0x0048ab38 | `int` | Animation channel state; cleared each scene by `InitMessagesChannel` |
| 0x0048a696 | `void *` | Channel C resource handle (offset 0x12 into `gChannelC`); non-zero = already initialised (fatal guard in `InitDebugger`) |
| 0x00460dd4 | `int` | Multimedia state A; cleared each scene by `InitDebugger` |
| 0x00460dc8 | `int` | Multimedia state B; cleared each scene by `InitDebugger` |
| 0x00460df0 | `void *` | Multimedia resource object 1 (empty, allocated via `AllocResObject(0)`) |
| 0x00460dd8 | `void *` | Multimedia resource object 2 (empty, allocated via `AllocResObject(0)`) |
| 0x00460df4 | `void *` | Multimedia resource object 3 (empty, allocated via `AllocResObject(0)`) |
| 0x00460dc4 | `int` | Multimedia channel state; cleared each scene by `InitDebugger` |
| 0x00460dcc | `int` | Current multimedia playback index; init to -1 (none) by `ResetDebuggerState` |
| 0x00460dec | `int` | Multimedia playback counter; init to 0 by `ResetDebuggerState` |
| 0x00460df8 | `int` | Multimedia playback state; init to 0 by `ResetDebuggerState` |
| 0x00460de4 | `int` | Multimedia playback mode; init to 2 by `ResetDebuggerState` |
| 0x00460dc0 | `int` | Multimedia playback flag; init to 0 by `ResetDebuggerState` |
| 0x00460dd0 | `int` | Current playback time position; used by `ListScriptSourceToDebugger` to seek into entry data |
| 0x00460c10 | `int` | Base address of multimedia descriptor slot table (20 entries × 256 bytes); indexed by `TakeScriptString` |
| 0x00460be8 | `int[]` | Per-slot occupied flags for multimedia descriptor table (20 entries); set to mark slot populated, cleared after retrieval by `TakeScriptString` |
| 0x00460aa0 | `byte[320]` | Script work queue base (40 entries × 8 bytes each, two int fields per entry); used by `ProcessScriptBlock` to accumulate instruction/result pairs for priority-based resolution |
| 0x00460be0 | `short` | Script work queue entry count; max 39 (0x27); incremented/decremented by `ProcessScriptBlock` |
| 0x0048a716 | `short` | Sound cue dispatch reentrancy counter; incremented before `FUN_004420e0` call and decremented after, by `ActionCmd_VisualEffect` |
| 0x0048a718 | `short` | Scene resource state; cleared by `InitSceneResource` |
| 0x0048a71a | `short` | Scene resource state; cleared by `InitSceneResource` |
| 0x0048a71e | `short` | Scene resource operating mode; set to 3 by `InitSceneResource` |
| 0x0048a722 | `short` | Scene resource state (Ghidra overlap); cleared by `InitSceneResource` |
| 0x0048a724 | `void *` | Slot 0 entry table handle — resource object holding 0xA0-byte (40-int) entries; count in gActorCount; managed by `ActionCmd_OpenCastFile`/`ActionCmd_CloseCastFile` |
| 0x0048a728 | `int` | Slot 0 entry count (number of 0xA0-byte entries in gActorTable table) |
| 0x0048a72c | `void *` | Slot 1 entry table handle — resource object holding 0x1C-byte (7-int) entries; count in gCastCount; managed by `ActionCmd_OpenCastFile`/`ActionCmd_CloseCastFile` |
| 0x0048a730 | `int` | Slot 1 entry count (number of 0x1C-byte entries in gCastTable table) |
| 0x0048a734 | `void *` | Scene resource slot 3 handle (`AllocResObject(0)`) |
| 0x0048a738 | `int` | Scene resource slot 3 state; cleared |
| 0x0048a73c | `void *` | Scene resource slot 4 handle (`AllocResObject(0)`) |
| 0x0048a740 | `int` | Scene resource slot 4 state; cleared |
| 0x0048a744 | `void *` | Slot 2 entry table handle — resource object holding 0x28-byte (10-int) entries; count in gTrackCount; managed by `ActionCmd_OpenTrackFile`/`ActionCmd_CloseTrackFile` |
| 0x0048a748 | `int` | Slot 2 entry count |
| 0x0048a74c | `int` | Cleared by `InitSceneResource` |
| 0x0048a778 | `int` | Cleared by `InitSceneResource` |
| 0x0048a7f8 | `short` | Scene transition guard flag; checked by `ActionCmd_OpenPuppetFile` (returns error 0x2d if set); passed to FUN_0043eab0 |
| 0x0048a82c | `short` | UI layout; cleared by `InitSceneResource` |
| 0x0048a82e | `short` | UI layout constant = 0x80 (128); set by `InitSceneResource` |
| 0x0048a830 | `short` | UI layout constant = 0xFA (250); set by `InitSceneResource` |
| 0x0048a832 | `short` | UI layout constant = 0xFB (251); set by `InitSceneResource` |
| 0x0048a834 | `short` | UI layout constant = 0x378 (888); set by `InitSceneResource` |
| 0x0048a836 | `short` | UI layout constant = 0xC (12); set by `InitSceneResource` |
| 0x0048a838 | `short` | UI layout; cleared by `InitSceneResource` |
| 0x0048a83a | `short` | UI layout; cleared by `InitSceneResource` |
| 0x0048a83c | `short` | UI layout constant = 2; set by `InitSceneResource` |
| 0x0048a83e | `short` | UI layout constant = 8; set by `InitSceneResource` |
| 0x0048a840 | `short` | UI layout; cleared by `InitSceneResource` |
| 0x0048a842 | `short` | UI layout; cleared by `InitSceneResource` |
| 0x0048a130 | `int` | Cleared before bootfile load in `InitSceneResource` |
| 0x0048a148 | `byte[256]` | Shared 256-byte working buffer; used as resource descriptor copy target by `GetSearchPathSlot` / `InitSceneResource`; also passed to `ComposeModuleDirBootPath` and `SetSearchPathSlot` during animation init |
| 0x0048a890 | `byte[256]` | **The script `result()` slot.** Pascal string read back by `ScriptCmd_Result` (0x00427bc0). Commands that answer one thing but have a second to report leave it here: `hittest` (0x00427350) writes the hit *kind* — `"actor"`, `"prop"`, `"painting"`, `"scene"`, `"button"`, `"flat"`, `"None"` — and `indextoprop` (0x00418750) / `indextoactor` (0x00410700) write the *file* the entry came from while returning its name. Reached by `PUSH 0x48a890` immediates, so it is a buffer, never a pointer |
| 0x0045d8c0 | `char[4]` | 4-byte Pascal-string slot (length byte + up to 3 chars), zero-filled in the image; sealed by `ClearScriptResult` (0x0042aff0) and returned by `hittest`'s no-hit arm. The next string constant, `"\0flat"`, starts at 0x0045d8c4 |
| 0x0048a866 | `short` | From bootfile resource data offset 0x10 |
| 0x0048a868 | `short` | From bootfile resource data offset 0x12 |
| 0x0048a86a | `short` | `g_sceneOriginY` — computed scene Y display offset (centres scene in viewport) |
| 0x0048a86c | `short` | `g_sceneOriginX` — computed scene X display offset (centres scene in viewport) |
| 0x0048a86e | `int *` | Bootfile container node ptr; set by `OpenContainerNode` in `InitSceneResource` |
| 0x0048a878 | `HCURSOR` | `gCursor` — main game cursor; loaded from engine resource 'CURS' id 2002 by `BeginGameSession` |
| 0x0048a2c8 | `void *` | `gCursorAlt` — secondary cursor handle; 'CURS' id 131 |
| 0x0048a862 | `short` | `gSceneW` — scene canvas width (pixels) |
| 0x0048a864 | `short` | `gSceneH` — scene canvas height (pixels) |
| 0x0048a350 | `SceneViewport` | `gSceneViewport` — full-canvas viewport rect `{short x; short y; short h; short w}`; zeroed origin, h/w set from canvas dims by `BeginGameSession` |
| 0x0048a248 | `HotspotBuf` | `gHotspotBuf` — scene hotspot/interaction pixel buffer struct; layout `{short h, short w, void *pPx, void *pBufHandle, ...}`; `pPx` at +4 points to `gSceneW × gSceneH` byte buffer; initialised by `InitHotspotBuffer` + `ZeroBlock` in `BeginGameSession` |
| 0x0048a2d0 | `unsigned int` | Secondary scene state word; passed to `CopyHotspotBufState` during `BeginGameSession` |
| 0x0045b728 | `int` | `gCursorShowDepth` — signed present/flip state polled by `QdInitCursor`; while `< 0`, the loop calls `QdShowCursor()` until ready |
| 0x00486bb0 | `void *[]` | `gViewportTable[]` — per-viewport struct pointers (the 0x74-byte viewport built by `CreateSceneViewport` / `OpenAnimWindow` / `OpenMultimediaWindow`: HWND at +0, HDC, HotspotBitmapState); same table as the row above; indexed by `HitTestHotspot` and `FetchHotspotDesc` |
| 0x0048836c | `int` | `gViewportCount` — number of viewports registered in `gViewportTable[]`; used by `HitTestHotspot` |

## Animation System (set in InitScriptRuntime)

| Address | Type | Notes |
|---|---|---|
| 0x0048a0d4 | `int` | Max animation slot count; set to 10 by `InitScriptRuntime` |
| 0x0048a138 | `int` | Active-animation counter; cleared to 0 each scene |
| 0x0048a140 | `void *` | Animation descriptor table handle; 320-byte resource object allocated via `AllocResObject(0x140)` |
| 0x0048a148 | `byte[256]` (shared) | Shared 256-byte working buffer; reset by `ComposeModuleDirBootPath` during anim init; passed to `SetSearchPathSlot` per slot; also used as resource descriptor copy target by `GetSearchPathSlot` / `InitSceneResource` |
| 0x0048a0d8 | `void *` | Resource descriptor table; 9 entries × 256 bytes = 2304 B; allocated via `AllocEngineBuffer(0x900)` by `InitScriptRuntime`; indexed by `GetSearchPathSlot(index)` |
| 0x0048a0a0 | `int` (block) | Compositing / decode scratch area; 409 600 bytes allocated via `AllocSecondaryHeap` |

## Scene Dimensions (set in InitHotspotBuffer)

| Address | Type | Notes |
|---|---|---|
| 0x00486e84 | `int` | Scene width (positive); aligned to 4-byte boundary; zeroed in `AppInit`; set by `InitHotspotBuffer` |
| 0x00486e88 | `int` | Scene height (negated, top-down DIB convention); zeroed in `AppInit`; set by `InitHotspotBuffer` |

## Message Pump / Timer

| Address | Type | Notes |
|---|---|---|
| 0x0045b938 | `DWORD` | `gLastPollTime` — `timeGetTime()` ms value captured by `PumpMessages`; throttle: only runs again after 66 ms have elapsed |
| 0x0045b934 | `int` | `gMsgPumpDepth` — re-entrancy counter; non-zero forces PeekMessageA active path even when gGamePaused is set |
| 0x0045d6b8 | `int` | `gSoundOn` — non-0 triggers `ServiceSoundMixer` hardware input flush (DirectInput / joystick) inside `PumpMessages`; also read as a "sound subsystem running" flag by `modalui.c`, `displaymode.c`, and `IsVoicePlaybackStillActive` (0x00425390) |
| 0x0045d6ac | `byte[]` | Pascal string `":testfile"` — probe path appended by `CheckDescriptorVolumeResolvable` (0x00423F00); resolved result length 8 ("testfile") means the descriptor has no volume prefix |
| 0x0045b940 | `byte[8]` | 8x8 stipple draw pattern installed via `QdPenPat` (0x0041FEF0) from `QdFrameRectStippled` (anim.c) |

## Win32 Resource Section

| Address | Type | Notes |
|---|---|---|
| 0x0045bc2c | `const char[]` | Win32 resource type string passed to `FindResourceA`; custom game type name (e.g. `"DFRESOURCE"`; exact value TBD) |
| 0x0045bc20 | `s_FONT_FONT_0045bc20` | `const char[]` | `"FONT.FONT"` – resource name for the embedded font |
| 0x0045bc1c | `kStrCFX` | `const char[]` | Prefix `"FX"` passed to `GetTempFileNameA` (confirmed by CSV; creates `FXxxxxxx.TMP` files) |
| 0x0045bc11 | `DAT_0045bc11` | `const char[]` | Wildcard `"FX*.FNT"` for temp-font cleanup sweep (note: CSV confirms bc11, not bc10) |

## CLUT / Palette Resource System (set in InitCLUTPalettes)

| Address | Type | Notes |
|---|---|---|
| 0x0045db8c | `RenderNode *` | `gRenderNodeList` — head of singly linked render-node list walked by `FlushAllRenderNodes`; each `RenderNode` has its `pNext` pointer at byte offset 0x540 |
| 0x0045db9c | `int *` | `gCurrentParentNode` — handle of the parent node currently locked by `SetRenderParentContext`; cleared to NULL by `ReleaseRenderParentContext` |
| 0x0045dba0 | `int` | `gChildSerialCounter` — monotonic order stamp; incremented by `RegisterChildEntry` and `PromoteChildEntry` and written into `slot+0x0a` to track MRU access order |
| 0x0045dbd4 | `int` | `gScanLockInited` — 0 until `AcquireRenderScanLock` first initialises `gScanLock`; guards lazy `InitializeCriticalSection` |
| 0x0048ab10 | `int **` | `gCurrentParentData` — `GetResDataPtr(gCurrentParentNode)`; locked pointer to the parent node's data block (child-list hash table, slot array, active-list state) |
| 0x0048ab14 | `int *` | `gCurrentChildListData` — `GetResDataPtr(parentData[0])`; locked pointer to the flat child-slot array (each slot 0x18 bytes) |
| 0x0048ab18 | `CRITICAL_SECTION` (24 B) | `gScanLock` — critical section protecting the render scan; acquired by `AcquireRenderScanLock`, released by `ReleaseRenderScanLock` |
| 0x0048ab30 | `int` | `gScanLockBusy` — set to 1 inside `AcquireRenderScanLock`, cleared to 0 by `ReleaseRenderScanLock` |
| 0x0045db94 | `short (*)(void)` | `gMemoryPressureCallback` — optional callback function pointer; called after each heap compaction pass in `ReserveHeapSpace`; return value propagated as error code if non-zero |
| 0x0045da58 | `byte[256]` | `kCaseFoldTable` — 256-byte case-folding lookup table used by `PascalStringsEqualNoCase` for case-insensitive byte comparison |
| 0x0045db60 | Pascal-string descriptor | Primary CLUT: `byte[0]`=length, `byte[1..]`=`"System"` → resource `"CLUT.SYSTEM"` |
| 0x0045db58 | Pascal-string descriptor | Secondary CLUT: same format; name = `"Black"` → resource `"CLUT.BLACK"` |
| 0x0048ab00 | `void *` | Handle to primary CLUT resource |
| 0x0048aaf4 | `void *` | Handle to secondary CLUT resource |
| 0x0048aa08 | `void *` | Active CLUT handle (copy of primary at init) |
| 0x0048aa00 | `int` | CLUT subsystem ready flag; set to 1 by `InitCLUTPalettes` |
| 0x0048aa10 | `unsigned int[55]` | `gRandTable[55]` — PRNG state for palette animation; `[0]`=seed%65535, `[1..53]`=LCG chain (mult=31, inc=1, mod=65535), `[54]`=0; seeded by `SeedRandomTable` from `GetTick60Hz()` |

## Misc Globals

| Address | C name | Type | Notes |
|---|---|---|---|
| 0x00485f54 | `g_hBackBufferDC` | `HDC` | Off-screen back-buffer DC; result of `CreateBackBufferDC()` (0x0041c580); deleted in `Shutdown()` |
| 0x00486164 | `g_hPrevDCObject` | `HGDIOBJ` | Default HGDIOBJ saved when a custom bitmap is selected into back-buffer DC; restored before `DeleteDC` in `Shutdown()` |
| 0x00489ef8 | `gSoundSlots` | `int[26]` | Sound channel slot 0 (primary); 26-int block; first int is priority key; used by `AssignPrimarySoundChannel` (channel 0) |
| 0x00489f60 | `DAT_00489f60` | `int[26]` | Sound channel slot 1; 26-int block; first int is priority key; used by `AssignSoundChannel`/`ReplaceSoundChannel`/`UpgradeSoundChannels`/`ForceAssignSoundChannels` (channel 1) |
| 0x00489fc8 | `DAT_00489fc8` | `int[26]` | Sound channel slot 2; 26-int block; first int is priority key; used by same functions as slot 1 (channel 2) |
| 0x0048b970 | gWalkTable | short[880] | Method binding table � 16 entries � 110 bytes (stride 0x37 shorts); entry layout: [0] active, [1] binding ID, [9] resource handle ptr (int**), [0x17] Pascal string name; managed by scriptprop.c |
| 0x0048c050 | gCricketTable | short[592] | Object binding table � 16 entries � 74 bytes (stride 0x25 shorts); entry layout: [0] active, [1] binding ID, [0x1d] Pascal string name; managed by scriptprop.c |
| 0x0048c4f0 | gLoopTable | short[672] | Property binding table � 32 entries � 42 bytes (stride 0x15 shorts); entry layout: [0] active, [1] binding ID, [2] type hash, [5..12] Pascal name (16 bytes), [13..20] secondary Pascal name; managed by scriptprop.c |
| 0x0045b3e0 | kStrAll | const byte[] | Wildcard Pascal string � when passed as name to Unregister* functions, clears ALL matching entries instead of a specific one |
| 0x0045d8e8 | kStrUnknown | const byte[] | Not-found fallback Pascal string � copied to output by GetLoopNameByIndex/GetCricketNameByIndex when requested index exceeds active entry count |
| 0x0048af00 | gQueuedVoiceIds | short[] | Queued sound channel IDs (up to 3), replayed by PollPuppetEventInput |
| 0x0048af08 | gBevelCount | short | On-screen text-entry table: row count (table header; also passed whole to DrawBevelTable) |
| 0x0048af0a | DAT_0048af0a | short | Text-entry table: selected row index (set by PollPuppetEventInput, 0x00441480) |
| 0x0048af0c | gBevelTable | int[] | Text-entry table rows (stride 0x41 ints = 0x104 bytes); first int is the entry id, name text follows at +4 (DAT_0048af10) |
| 0x0048b424 | gQueuedVoiceCount | short | Queued sound count (channel IDs in gQueuedVoiceIds[0..2]) |
| 0x0048b428 | gBevelTableSnapshot | short[] | Snapshot of the text-entry table region; [1] = selected row index |

## Window Classes

All window classes registered by the application via `RegisterClassA` in `AppInit`.

### DFWINDOW

**Purpose:** Main game window.

| Field | Value |
|---|---|
| `lpszClassName` | `"DFWINDOW"` (at 0x0045bae8) |
| `lpfnWndProc` | `WndProc_DFWINDOW` at `0x0041aac0` |
| `hInstance` | `gInstance` (app HINSTANCE) |
| `hCursor` | Arrow cursor loaded from `"CURS.ARROW"` resource |
| `hIcon` | Icon loaded from `"WINDOWICON"` resource |
| `lpszMenuName` | `NULL` |
| `hbrBackground` | `BLACK_BRUSH` (stock object 4) |
| `style` | `0` |
| `cbWndExtra` | `0x18` (24 bytes per-window data -- stores game state ptr + fields) |
| `cbClsExtra` | `0` |

### DFCOVER

**Purpose:** Full-screen splash / loading cover window shown at startup.

| Field | Value |
|---|---|
| `lpszClassName` | `"DFCOVER"` (at 0x0045bac8) |
| `lpfnWndProc` | `WndProc_DFCOVER` at `0x0041aa50` |
| `hInstance` | `gInstance` (app HINSTANCE) |
| `hCursor` | Watch cursor (`gWatchCursor`, loaded from `"CURS.WATCH"` resource) |
| `hbrBackground` | `BLACK_BRUSH` (inherited from DFWINDOW registration) |
| `style` | `CS_PARENTDC` (0x0200) |
| `cbWndExtra` | `0` |
| `cbClsExtra` | `0` |

**Creation:** `CreateWindowExA(0, "DFCOVER", title, WS_POPUP, 0, 0, SM_CXSCREEN, SM_CYSCREEN, ...)` -- borderless full-screen. Shown with `SW_SHOWMAXIMIZED` immediately. Created before any game data loads; destroyed on fatal error during startup.

Window procedures decoded -- see `src/wndproc.c` and the catalog in [ghidra-fun-symbols.md](generated/ghidra-fun-symbols.md).

## Script keyword tables (src/keyword_tables.c, generated)

| Address | C name | Type | Notes |
|---|---|---|---|
| `0x0045d5e0` | `g_keywordDefault` | `KeywordList` (6 bytes) | Punctuation and operators, 19 entries (three are Mac Roman aliases) |
| `0x0045d5e8` | `g_keywordByLetter` | `KeywordList[26]` | One list per initial letter; `LookupScriptKeyword` indexes it from 0x0045d3a2 + letter * 6 |
