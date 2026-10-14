/* Declarations of every global defined in src/data/globals.c, with the packed images and the field
 * macros. Keep it in step with globals.c by hand; match/globals.tsv lists the identifier the
 * byte-match build knows each global by. */
#ifndef DF_GLOBALS_H
#define DF_GLOBALS_H

/* the match build's bss names (match/bss.py): must precede every declaration */
#ifdef DF_BSS_NAMES
#include "df_bss_names.h"
#endif

#include "df_types.h"
#include "df_fixes.h"

extern int kUnityGain;
extern int kUnityGainWord1;
extern int kUnityGainWord2;
extern int gWaveOutOpen;
extern int gWaveAltDeviceOpen;
extern int gMixerVolumeReady;
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrRight[7];
#else
#define kStrRight ("\0right")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrLeft[6];
#else
#define kStrLeft ("\0left")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrStrait[8];
#else
#define kStrStrait ("\0strait")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaOpensceneCall[16];
#else
#define kStrQuoteCommaOpensceneCall ("\0\", openscene()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuote[3];
#else
#define kStrQuote ("\0\"")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaClosesceneCall[17];
#else
#define kStrQuoteCommaClosesceneCall ("\0\", closescene()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrOpensetCall[11];
#else
#define kStrOpensetCall ("\0openset()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrClosesetCall[12];
#else
#define kStrClosesetCall ("\0closeset()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSystemColon[10];
#else
#define kStrSystemColon ("\0System: ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSceneMessage[15];
#else
#define kStrSceneMessage ("\0Scene Message")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrPostScript[13];
#else
#define kStrPostScript ("\0Post Script")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSetScriptColon[14];
#else
#define kStrSetScriptColon ("\0Set Script: ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSceneScriptColon[16];
#else
#define kStrSceneScriptColon ("\0Scene Script: ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSetMessage[13];
#else
#define kStrSetMessage ("\0Set Message")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrMoving[8];
#else
#define kStrMoving ("\0Moving")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrNone[6];
#else
#define kStrNone ("\0None")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrPaintScriptColon[16];
#else
#define kStrPaintScriptColon ("\0Paint Script: ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrCommaEveryoneClose[14];
#else
#define kStrCommaEveryoneClose ("\0,\"everyone\")")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrAddPlayerOpen[12];
#else
#define kStrAddPlayerOpen ("\0addplayer(")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrGetmemberid[13];
#else
#define kStrGetmemberid ("\0getmemberid")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrGetmembercount[16];
#else
#define kStrGetmembercount ("\0getmembercount")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrEveryone[10];
#else
#define kStrEveryone ("\0everyone")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrGetourid[10];
#else
#define kStrGetourid ("\0getourid")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrConnect[9];
#else
#define kStrConnect ("\0connect")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrNetwork[9];
#else
#define kStrNetwork ("\0network")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrDisconnect[12];
#else
#define kStrDisconnect ("\0disconnect")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSendmessage[13];
#else
#define kStrSendmessage ("\0sendmessage")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrRParen[3];
#else
#define kStrRParen ("\0)")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrComma[3];
#else
#define kStrComma ("\0,")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrLParen[3];
#else
#define kStrLParen ("\0(")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrJoingroup[11];
#else
#define kStrJoingroup ("\0joingroup")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrLeavegroup[12];
#else
#define kStrLeavegroup ("\0leavegroup")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteRParen[4];
#else
#define kStrQuoteRParen ("\0\")")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrCommaQuote[4];
#else
#define kStrCommaQuote ("\0,\"")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrDelPlayerOpen[12];
#else
#define kStrDelPlayerOpen ("\0delplayer(")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrJoinedSpace[9];
#else
#define kStrJoinedSpace ("\0joined ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrLeftSpace[7];
#else
#define kStrLeftSpace ("\0left ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrPlayerSpace[9];
#else
#define kStrPlayerSpace ("\0player ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrDisconnectedCall[16];
#else
#define kStrDisconnectedCall ("\0disconnected()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrDisconnected[14];
#else
#define kStrDisconnected ("\0disconnected")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrGetnotify[11];
#else
#define kStrGetnotify ("\0getnotify")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrGetmessage[12];
#else
#define kStrGetmessage ("\0getmessage")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrGetmessagesender[18];
#else
#define kStrGetmessagesender ("\0getmessagesender")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrGetmessagegroup[17];
#else
#define kStrGetmessagegroup ("\0getmessagegroup")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrClearAll[11];
#else
#define kStrClearAll ("\0Clear All")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrError[7];
#else
#define kStrError ("\0Error")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrGo[4];
#else
#define kStrGo ("\0Go")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrStepOut[10];
#else
#define kStrStepOut ("\0Step Out")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrStepIn[9];
#else
#define kStrStepIn ("\0Step In")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrStep[6];
#else
#define kStrStep ("\0Step")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrScripts[9];
#else
#define kStrScripts ("\0Scripts")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSCRIPTS[9];
#else
#define kStrSCRIPTS ("\0SCRIPTS")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrLocals[8];
#else
#define kStrLocals ("\0Locals")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrLOCALS[8];
#else
#define kStrLOCALS ("\0LOCALS")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrGlobals[9];
#else
#define kStrGlobals ("\0Globals")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrGLOBALS[9];
#else
#define kStrGLOBALS ("\0GLOBALS")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrCR[3];
#else
#define kStrCR ("\0\r")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrFalse[7];
#else
#define kStrFalse ("\0False")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrTrue[6];
#else
#define kStrTrue ("\0True")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrEquals[5];
#else
#define kStrEquals ("\0 = ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrFalseLower[7];
#else
#define kStrFalseLower ("\0false")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrTrueLower[6];
#else
#define kStrTrueLower ("\0true")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrAll[5];
#else
#define kStrAll ("\0all")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrCustom[8];
#else
#define kStrCustom ("\0custom")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaOpenactorCall[16];
#else
#define kStrQuoteCommaOpenactorCall ("\0\", openactor()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaOpencastCall[15];
#else
#define kStrQuoteCommaOpencastCall ("\0\", opencast()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrActor[7];
#else
#define kStrActor ("\0actor")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaClosecastCall[16];
#else
#define kStrQuoteCommaClosecastCall ("\0\", closecast()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaCloseactorCall[17];
#else
#define kStrQuoteCommaCloseactorCall ("\0\", closeactor()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrActorScriptColon[16];
#else
#define kStrActorScriptColon ("\0Actor Script: ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrCastScriptColon[15];
#else
#define kStrCastScriptColon ("\0Cast Script: ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrActorMessage[15];
#else
#define kStrActorMessage ("\0Actor Message")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrCastMessage[14];
#else
#define kStrCastMessage ("\0Cast Message")
#endif
extern int gTrigPlaneDecodeMode;
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSaveGame[10];
#else
#define kStrSaveGame ("\0SaveGame")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSaveWriteError[75];
#else
#define kStrSaveWriteError ("\0Error writing to save game file.  Be sure the disk has enough free space.")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSaveCreateError[75];
#else
#define kStrSaveCreateError ("\0Unable to create save game file.  Be sure the disk isn't write protected.")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSaveVersionError[60];
#else
#define kStrSaveVersionError ("\0This saved game is from a different version of this title.")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSaveInvalidError[38];
#else
#define kStrSaveInvalidError ("\0This is not a valid saved game file.")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaOpenpropCall[15];
#else
#define kStrQuoteCommaOpenpropCall ("\0\", openprop()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaOpenshopCall[15];
#else
#define kStrQuoteCommaOpenshopCall ("\0\", openshop()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrProp[6];
#else
#define kStrProp ("\0prop")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaCloseshopCall[16];
#else
#define kStrQuoteCommaCloseshopCall ("\0\", closeshop()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaClosepropCall[16];
#else
#define kStrQuoteCommaClosepropCall ("\0\", closeprop()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrPropScriptColon[15];
#else
#define kStrPropScriptColon ("\0Prop Script: ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrShopScriptColon[15];
#else
#define kStrShopScriptColon ("\0Shop Script: ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrPropMessage[14];
#else
#define kStrPropMessage ("\0Prop Message")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrShopMessage[14];
#else
#define kStrShopMessage ("\0Shop Message")
#endif
extern int gGamePaused;
extern int kSysPalIDs[25];
extern COLORREF kGameSysColors[25];
extern int gPalOwnerState;
extern double gGammaR;
extern double gGammaG;
extern double gGammaB;
extern int gGammaTablesBuilt;
extern int gSavedWindowCount;
extern HMODULE gUser32Lib;
extern int gJoy1PrevButtons;
extern int gJoy2PrevButtons;
extern DWORD gJoyRepeatTime;
extern DWORD gWinNTBuild;
extern HANDLE gVxdDevice;
extern int gCursorShowDepth;
extern char gVolumeLabelCache[256];
extern char gVolumePathCache[256];
extern int gVolumeCacheNext;
extern int gMsgPumpDepth;
extern DWORD gLastPollTime;
extern byte kStipplePattern[8];
extern int gOpenFileNameInited;
#define kStrPctP ("%p")
#define kStrMono ("mono")
#define kStrPctD ("%d")
#define kStrMENU ("MENU")
#define kStrOptE ("-e")
#define kStrOptS ("-s")
#define kStrOpt16 ("-16")
#define kStrOptJ ("-j")
#define kStrOptR ("-r")
#define kStrOptHelp ("-?")
#define kStrOptH ("-h")
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern const char kStrEmpty[4];
#else
#define kStrEmpty ("")
#endif
#define kStrNtPrefix ("NT ")
#define kStrCfxFontGlob ("CFX*.FNT")
#define kStrCFX ("CFX")
#define kStrRAW ("RAW")
#define kStrPctLd ("%ld")
#define kStrVfixmemVxd ("VFIXMEM.VXD")
#define kStrBackslash ("\\")
#define kStrDriveRootFormat ("%c:\\")
#define kStrDriveA ("A:\\")
#define kStrColon (":")
#define kStrDot (".")
#define kStrTEXTDLG ("TEXTDLG")
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSpace[3];
#else
#define kStrSpace ("\0 ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrTab[3];
#else
#define kStrTab ("\0\t")
#endif
extern int gDisplayAttachLoaderRunning;
extern int gDirSearchHandle;
extern int gAttachLoaderRunning;
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrTestfile[10];
#else
#define kStrTestfile ("\0testfile")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrAllFiles[5];
#else
#define kStrAllFiles ("\0*.*")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrColonTestfile[11];
#else
#define kStrColonTestfile ("\0:testfile")
#endif
extern short gSoundOn;
extern char kAudioDeltaTable1[256];
extern char kAudioDeltaTable2[256];
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrEmptyPascal[4];
#else
#define kStrEmptyPascal ("\0")
extern char df_slot_0045d8c0;
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrFlat[6];
#else
#define kStrFlat ("\0flat")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrButton[8];
#else
#define kStrButton ("\0button")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrScene[7];
#else
#define kStrScene ("\0scene")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrPainting[10];
#else
#define kStrPainting ("\0painting")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrUnknown[9];
#else
#define kStrUnknown ("\0unknown")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrLogic[7];
#else
#define kStrLogic ("\0logic")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrNumber[8];
#else
#define kStrNumber ("\0number")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrText[6];
#else
#define kStrText ("\0text")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrBootfile[10];
#else
#define kStrBootfile ("\0bootfile")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrRuntime[9];
#else
#define kStrRuntime ("\0Runtime")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrIdleCall[8];
#else
#define kStrIdleCall ("\0idle()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrBootCall[8];
#else
#define kStrBootCall ("\0boot()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrMenustateCall[13];
#else
#define kStrMenustateCall ("\0menustate()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrMenuSelectOpenQuote[14];
#else
#define kStrMenuSelectOpenQuote ("\0menuselect(\"")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrRightarrow[12];
#else
#define kStrRightarrow ("\0rightarrow")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrLeftarrow[11];
#else
#define kStrLeftarrow ("\0leftarrow")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrDownarrow[11];
#else
#define kStrDownarrow ("\0downarrow")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrUparrow[9];
#else
#define kStrUparrow ("\0uparrow")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrKeyDownOpenQuote[11];
#else
#define kStrKeyDownOpenQuote ("\0keydown(\"")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrKeyRepeatOpenQuote[13];
#else
#define kStrKeyRepeatOpenQuote ("\0keyrepeat(\"")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrMouseDownOpen[12];
#else
#define kStrMouseDownOpen ("\0mousedown(")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrBootScript[13];
#else
#define kStrBootScript ("\0Boot Script")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrServerScript[15];
#else
#define kStrServerScript ("\0Server Script")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrBootMessage[14];
#else
#define kStrBootMessage ("\0Boot Message")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrServerMessage[16];
#else
#define kStrServerMessage ("\0Server Message")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrRBracket[3];
#else
#define kStrRBracket ("\0]")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrWIN[5];
#else
#define kStrWIN ("\0WIN")
#endif
#define kStrDllExt (".DLL")
#define kStrQuoteC ("\"")
extern byte kCaseFoldTable[256];
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrBlack[7];
#else
#define kStrBlack ("\0Black")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSystem[8];
#else
#define kStrSystem ("\0System")
#endif
#define kStrPctSPctD ("%s%d")
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrPluginsColon[10];
#else
#define kStrPluginsColon ("\0plugins:")
#endif
extern int gRenderSlotCounter;
extern void *gRenderNodeList;
extern int gPendingRenderNodeList;
extern short  (*gMemoryPressureCallback)(void);
extern int gNodeDestroyHook;
extern int *gCurrentParentNode;
extern int gChildSerialCounter;
extern short kReadBufSizeTable[22];
extern int gScanLockInited;
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrOK[4];
#else
#define kStrOK ("\0OK")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrMessageBox[13];
#else
#define kStrMessageBox ("\0Message Box")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrTokenizationError[20];
#else
#define kStrTokenizationError ("\0tokenization error")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrWatch[7];
#else
#define kStrWatch ("\0watch")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrArrow[7];
#else
#define kStrArrow ("\0arrow")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrPuppet[8];
#else
#define kStrPuppet ("\0puppet")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrStage[7];
#else
#define kStrStage ("\0stage")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSet[5];
#else
#define kStrSet ("\0set")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrBlackLower[7];
#else
#define kStrBlackLower ("\0black")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrSystemLower[8];
#else
#define kStrSystemLower ("\0system")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrCurrent[9];
#else
#define kStrCurrent ("\0current")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaOpenpuppetCall[17];
#else
#define kStrQuoteCommaOpenpuppetCall ("\0\", openpuppet()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaClosepuppetCall[18];
#else
#define kStrQuoteCommaClosepuppetCall ("\0\", closepuppet()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrPuppetMessage[16];
#else
#define kStrPuppetMessage ("\0Puppet Message")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrPuppetScriptColon[17];
#else
#define kStrPuppetScriptColon ("\0Puppet Script: ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrPuppetVoice[14];
#else
#define kStrPuppetVoice ("\0Puppet Voice")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrIdle4[8];
#else
#define kStrIdle4 ("\0idle 4")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrIdle3[8];
#else
#define kStrIdle3 ("\0idle 3")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrIdle2[8];
#else
#define kStrIdle2 ("\0idle 2")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrIdle1[8];
#else
#define kStrIdle1 ("\0idle 1")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrPlayerVoice[14];
#else
#define kStrPlayerVoice ("\0Player Voice")
#endif
extern byte kDeltaPixelMagnitudes[16];
extern byte kDirectionAngleTable[512];
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrEmptyCall[4];
#else
#define kStrEmptyCall ("\0()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaSpace[5];
#else
#define kStrQuoteCommaSpace ("\0\", ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrWalktostar[12];
#else
#define kStrWalktostar ("\0walktostar")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrWalktoxyz[11];
#else
#define kStrWalktoxyz ("\0walktoxyz")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrWalkonpath[12];
#else
#define kStrWalkonpath ("\0walkonpath")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrResume[8];
#else
#define kStrResume ("\0resume")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrWalkonroad[12];
#else
#define kStrWalkonroad ("\0walkonroad")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrWalkonframes[14];
#else
#define kStrWalkonframes ("\0walkonframes")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrDefer[7];
#else
#define kStrDefer ("\0defer")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaEndwalkCall[14];
#else
#define kStrQuoteCommaEndwalkCall ("\0\", endwalk()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaEndturnCall[14];
#else
#define kStrQuoteCommaEndturnCall ("\0\", endturn()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaOpenflatCall[15];
#else
#define kStrQuoteCommaOpenflatCall ("\0\", openflat()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrQuoteCommaCloseflatCall[16];
#else
#define kStrQuoteCommaCloseflatCall ("\0\", closeflat()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrOpenstageCall[13];
#else
#define kStrOpenstageCall ("\0openstage()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrClosestageCall[14];
#else
#define kStrClosestageCall ("\0closestage()")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrStageScriptColon[16];
#else
#define kStrStageScriptColon ("\0Stage Script: ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrFlatScriptColon[15];
#else
#define kStrFlatScriptColon ("\0Flat Script: ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrButtonScriptColon[17];
#else
#define kStrButtonScriptColon ("\0Button Script: ")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrFlatMessage[14];
#else
#define kStrFlatMessage ("\0Flat Message")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrStageMessage[15];
#else
#define kStrStageMessage ("\0Stage Message")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrNoPrevFrame[29];
#else
#define kStrNoPrevFrame ("\0Can't go to previous frame.")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrNoNextFrame[25];
#else
#define kStrNoNextFrame ("\0Can't go to next frame.")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrNoPopFrame[28];
#else
#define kStrNoPopFrame ("\0Can't pop any more frames.")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrNoPushFrame[29];
#else
#define kStrNoPushFrame ("\0Can't push any more frames.")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrAposDot[4];
#else
#define kStrAposDot ("\0'.")
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
extern char kStrJumpToFrameError[24];
#else
#define kStrJumpToFrameError ("\0Can't jump to frame: '")
#endif
extern int gMixRingFillFrames;
extern int gWaveFlushLatencySum;
extern int gWaveBufferActive[4];
extern int gMixRingTailCursor;
extern int gWaveFeedLatencyCount;
extern int gMixRingBase;
extern int gMixRingWriteCursor;
extern int gWaveBufferFormatQueue[4];
extern int gWaveBufferLoopGatePending[4];
extern int gMixBufferCount;
extern int gMixChunkFrames;
extern int gMixSampleRate;
extern int gMixBytesPerSample;
extern int gMixSampleChannels;
extern int gMixFrameBytes;
extern int gMixFrameShift;
extern int gMixSilenced;
extern int gMixQueueTarget;
extern int gMixFlushMask;
extern int gSoundBufferDescs[30];
extern int gSoundBufferDescCount;
extern int gSoundVxdHeapSize;
extern int gSoundHeap[12];
extern int gWaveDeviceOps;
extern int gMixFramesPending;
extern int gWaveFeedLatencySum;
extern unsigned char gWaveHdrQueues[200];
extern int gWaveFlushLatencyCount;
extern CRITICAL_SECTION gSoundMixerLock;
extern int gSoundMixerLocked;
extern int gWaveBindsPending;
extern int gMixRingReadCursor;
extern int gMixRingEnd;
extern int gMixFramesQueuedTotal;
extern int gScriptWorkQueue[80];
extern short gScriptWorkQueueTop;
#ifdef _MSC_VER
extern short gStringSlotUsed[20];
#else
extern short gStringSlotUsed[64];
#endif
extern void *gStringSlots;
extern void *gMixerGetControlDetailsFn;
extern int gMixerControlDetails[6];
extern void *gMixerGetLineInfoFn;
extern int gMixerVolumeMin;
extern int gMixerVolumeMax;
extern int gWaveOutSyncPlaySlot;
extern int gWaveOutOnWinNT;
extern int gWaveOutSyncInFlight;
extern void *gMixerGetNumDevsFn;
extern void *gMixerGetIDFn;
extern void *gMixerGetLineControlsFn;
extern int DAT_00460c60;
extern void *DAT_00460c64;
extern void *DAT_00460c68;
extern void *DAT_00460c6c;
extern void *DAT_00460c9c;
extern int gWaveOutSampleRate;
extern int gWaveOutBlockAlign;
extern int gWaveOutInFlight;
extern int gWaveOutPlaySlot;
extern int gWaveOutWriteSlot;
extern int gWaveOutUseGetPosition;
#ifdef _MSC_VER
extern int gWaveOutFramesPerBuffer_r[33];
#else
extern int gWaveOutFramesPerBuffer;
#endif
#ifdef _MSC_VER
extern int gWaveOutHdrs[32];
#else
extern int gWaveOutHdrs[33];
#endif
extern int gWaveOutDeviceId;
extern int gWaveOut;
extern int gWaveOutQueueTarget;
extern int gWaveOutRingSize;
extern int gWaveOutBufferBytes;
extern int gWaveOutFlushMask;
extern int gWaveOutResetPending;
extern int gWinmmLib;
extern int gMixerVolumeValue;
extern void *gMixerSetControlDetailsFn;
extern int DAT_00460d80;
extern int DAT_00460d84;
extern void *gMixer;
extern int gWaveOutOnWin32s;
extern void *gMixerCloseFn;
extern void *gMixerOpenFn;
extern int gWaveOutOnWin95;
extern short gNetworkOn;
extern int gNetOurId;
extern int gNetDirEntry;
extern short gDebuggerStepMode;
extern int gDebuggerWindowRefCount;
extern short gDebuggerWatchCount;
extern int gDebuggerCaretOffset;
extern int gDebuggerToken;
extern short gDebuggerBreakpointCount;
extern void *gDebuggerBreakpoints;
extern short gDebuggerTargetDepth;
extern int gDebuggerLocalsNode;
extern short gDebuggerPane;
extern int gDebuggerFrame;
extern short gDebuggerDepth;
extern void *gDebuggerListing;
extern void *gDebuggerWatches;
extern short gDebuggerRunMode;
extern int gNamedEntryCacheIndex;
extern int gTrigZoneBand;
extern int gTrigSrcToDstStore[1026];
#ifdef _MSC_VER
extern int gTrigPlaneSpanBufs[8];
#else
extern int gTrigPlaneSpanBufs[9];
#endif
extern int gTrigPlaneSpanBuf8;
extern int gScanNodeStore[10240];
extern int gTrigPlaneSpanCounts[8];
extern int gTrigDstWidth;
extern byte gTrigPlaneSpanStore8[4096];
extern char gTrigPlaneSpanStore[65536];
extern int gTrigBandListLen;
extern int gTrigHitSpanCount;
extern int gTrigZoneRowOffset;
extern int gTrigScanRow;
extern int gTrigBandOriginX;
extern int gTrigDstToSrcMap;
extern int gTrigDstRows;
extern int gScanNodeCount;
extern int gTrigDstToSrcStore[4097];
extern int gScanNodeBase;
extern int gTrigHitSpanStore[2048];
extern int gTrigDstStride;
extern int gTrigPlaneSpans[8];
extern int gTrigPixelRowBase;
extern byte gTrigPixelRows[8192];
extern int gTrigHitSpans;
extern int gTrigSrcRows;
extern int gTrigPixelRowsPtr;
extern int gTrigZoneMapData;
extern int gTrigClipRight;
extern int gTrigBandListStore[1024];
extern int gTrigSrcToDstMap;
extern int *gTrigBandList;
extern int gTrigSrcWidth;
extern int gTrigClipLeft;
extern int gTrigMirrored;
extern void *gPropDescriptorTable;
extern int gStateEntryCacheIndex;
extern char gOomPoolMinFormat[40];
extern int gJoystickPresent;
extern char gOomPageFileFormat[40];
extern int gTextEntryPrompt;
extern HPALETTE gPalette;
extern int gNoEjectFlag;
extern int gBackBufferDC;
extern DWORD gTotalPhysRam;
extern short gGammaSlopeG[256];
extern HCURSOR gWatchCursor;
extern int gBackBufferSavedObject;
extern int gGraphicsLibrary;
extern int gTextDialogReplyDesc;
extern char gOpenFileInitialDir[256];
extern int gJoystickId;
extern SIZE_T gPoolSizeBytes;
extern int gEventQueueCount;
extern int gAlternateDisplayMode;
extern int gSavedCursorShowDepth;
extern int gTextDialogStatus;
extern int gSystemFont;
extern int gDefaultPalette;
extern int gNoJoystickFlag;
extern int gNoDisplayChangeFlag;
extern int gPreferredDisplayMode;
extern void *gWinGSetDIBColorTableProc;
extern char gOutOfMemPoolSizeFmt[40];
extern int gJoystickRangeY;
extern void *gPoolBase;
extern int gJoystickRangeX;
extern char gSaveFileFilter[128];
extern int gIsAppActive;
extern OPENFILENAMEA gOpenFileName;
extern void *gEnumDisplaySettingsProc;
extern char gOutOfMemUsableFmt[40];
extern int gIsWinGBackend;
extern int gMainWindow;
extern char gOpenFileFilter[128];
extern HWND gCoverWindow;
extern int gStereoSoundFlag;
extern char gOpenFilePath[256];
extern short gGammaSlopeR[256];
extern char gAppTitle[0x40];
extern int gWhiteBrush;
extern int gNullPen;
extern char gStringResourceBuf[384];
extern int gCoverWindowWasVisible;
extern GameEvent gEventQueue[32];
#ifdef _MSC_VER
extern void * gViewportTable[16];
#else
extern void * gViewportTable[32];
#endif
extern int gIsDisplayModeChanged;
extern int gIs386Cpu;
extern char gOutOfMemAvailPhysFmt[40];
extern DWORD gJoystickReleaseThreshold;
extern UINT gJoystickCenterX;
extern int gIsSceneBitmapSelected;
extern short gIsMenuVisible;
extern byte *gNavCommandTable;
extern DWORD gPoolReserveBytes;
extern int gExitWindowsExProc;
extern short gGammaSlopeB[256];
extern UINT gJoystickCenterY;
extern int gIsWin95OrNT4;
extern int gAccel;
extern DWORD gAvailPhysBytes;
extern SIZE_T gPoolAllocatedBytes;
extern DWORD gJoystickDeflectThreshold;
extern int gNavCommandCount;
#ifdef _MSC_VER
extern BITMAPINFOHEADER gBitmapHeader_r[1];
#endif
#ifdef _MSC_VER
extern int DAT_00486e84;
#endif
#ifdef _MSC_VER
extern int DAT_00486e88;
#endif
#ifdef _MSC_VER
extern short DAT_00486e8c;
#endif
#ifdef _MSC_VER
extern short DAT_00486e8e;
#endif
#ifdef _MSC_VER
extern int DAT_00486e90;
#endif
#ifdef _MSC_VER
extern int DAT_00486e94;
#endif
#ifdef _MSC_VER
extern int DAT_00486e98;
#endif
#ifdef _MSC_VER
extern int DAT_00486e9c;
#endif
#ifdef _MSC_VER
extern int DAT_00486ea0;
#endif
#ifdef _MSC_VER
extern int DAT_00486ea4;
#endif
#ifdef _MSC_VER
extern RGBQUAD gDibColorTable[256];
#endif
extern int gThePortDesc[32];
extern short gJoystickDirection;
extern void *gChangeDisplaySettingsProc;
extern int gNullBrush;
extern short gGammaBaseB[256];
extern DWORD gPoolMinBytes;
extern int gCreateDIBSectionProc;
#ifdef _MSC_VER
extern int DAT_00487540_r[514];
#else
extern PAL16ENTRY gPal16[256];
#endif
#ifdef _MSC_VER
extern int gLogPal_r[257];
#endif
#ifdef _MSC_VER
extern int DAT_00487d4a;
#endif
#ifdef _MSC_VER
extern PALETTEENTRY gPalEntries[256];
#endif
extern int gWinGCreateDCProc;
extern HMENU gMainMenu;
extern char gOutOfMemProbeSizeFmt[40];
extern HINSTANCE gInstance;
extern char gTempFontPath[260];
extern int gViewportCount;
#ifdef _MSC_VER
extern int gViewportWasVisible[16];
#else
extern int gViewportWasVisible[32];
#endif
extern void *gWinGBitBltProc;
extern int gIsWin31;
extern int gSound16BitFlag;
extern SIZE_T gProbedUsableBytes;
extern DWORD gAvailPageFileBytes;
extern DWORD gPoolMaxBytes;
extern int *gThePort;
extern short gGammaBaseG[256];
extern int gBlackPen;
extern int gCustomPaletteFlag;
extern int gBlackBrush;
extern void *gSetDIBColorTableProc;
extern int gIsWin95;
#ifdef _MSC_VER
extern SavedWndPos gSavedWindowPositions[256];
#else
extern SavedWndPos gSavedWindowPositions[255];
#endif
extern short gGammaBaseR[256];
extern char gOpenFileDefExt[32];
extern int gWinGCreateBitmapProc;
extern COLORREF gSavedSysColors[25];
extern short gAttachSlots[124];
extern int gAttachWaitCount;
extern CRITICAL_SECTION gAttachLock;
extern void *gAttachEvent;
extern short DAT_00489d94;
extern WIN32_FIND_DATAA gFindData;
#ifdef _MSC_VER
extern void * gAttachWaitList[4];
#else
extern void * gAttachWaitList[32];
#endif
extern int gSound16Bit;
extern int gSoundSlots[104];
extern int gSoundStereo;
extern int gScriptHeap[12];
extern int gSceneChannelRefCount;
extern int gMethodCacheCapacity;
extern void *gSearchPathTable;
extern byte gLatchedMethodFrame[78];
extern short gBootScriptRan;
extern short gMethodFrameLatched;
extern int gMethodCacheCount;
extern int gMethodCacheClock;
extern void *gMethodCache;
extern byte gScratchStr[256];
extern HotspotBuf gHotspotBuf;
extern short gScrollRect[4];
extern void *gCursorAlt;
extern byte gSavedHotspotBuf[128];
extern SceneViewport gSceneViewport;
extern byte gCurrentCdName[256];
extern byte gScratchStr2[256];
extern int gTrigAnimBufSize;
extern HCURSOR gCursor;
extern short gClipRect[4];
extern void *gTrigAnimBuf;
extern byte gScriptResult[256];
extern short gGameLoopState;
extern byte *gEditVarValue;
extern byte *gEditVarRecord;
extern byte *gEditVarText;
extern int DAT_0048a9a0;
extern int DAT_0048a9a4;
extern int gCompactSegCount;
extern int gPrimaryHeap[12];
extern int gPartitionSwapTemp[4];
extern void *gCompactWorkBuf;
extern int DAT_0048a9f4;
extern int DAT_0048a9f8;
extern int DAT_0048a9fc;
extern short gClutReady;
extern void *gActiveClut;
extern unsigned int gRandTable[55];
extern short gRandIndex;
extern void *gSecondaryClut;
extern void *gPrimaryClut;
extern void *gPluginDirectory;
extern int gPluginDirectoryCount;
extern int **gCurrentParentData;
extern int *gCurrentChildListData;
extern CRITICAL_SECTION gScanLock;
extern int gScanLockBusy;
extern int gAnimChannelRefCount;
extern byte gTextEntryBuf[256];
extern byte gMessageStr[256];
extern int gActiveTriggerSel;
extern HotspotBuf gTrigHotspotCopy;
extern short gTriggerOverlayValid;
extern void *gTransitionRects;
extern int gSinTableRes;
extern HotspotBuf gTrigBufScene;
extern short gTriggerState;
extern short gViewCos;
extern short gOverlayRequest;
extern int gTriggerFrame;
extern int gCosTable;
extern short gViewSin;
extern short gTransitionRectCount;
extern SceneViewport gTrigViewport;
extern void *gCalcRows;
extern int gCosTableRes;
extern HotspotBuf gTrigBufOverlay;
extern int gSinTable;
extern short gCalcRowCount;
extern int gLastTickTime;
extern short gQueuedVoiceIds[4];
#ifdef _MSC_VER
extern short gBevelCount_r[652];
#endif
#ifdef _MSC_VER
extern short DAT_0048af0a;
#endif
#ifdef _MSC_VER
extern int gBevelTable[325];
#endif
extern short gTrackInterrupted;
extern short gQueuedVoiceCount;
extern int gBevelTableSnapshot[326];
extern byte gWalkTable[1760];
extern byte gCricketTable[1184];
extern byte gLoopTable[1344];
extern int gObjectUpdateCount;
#ifdef _MSC_VER
extern int gWhileStack[20];
#else
extern int gWhileStack[32];
#endif
extern byte gForStack[280];
extern int DAT_0048cb;
extern short gForDepth;
extern short gWhileDepth;
extern short gMovieNavDepth;
extern int gMoviePollTick;
extern int gMovieEntryFrame;
extern int gMovieLastFrame;
extern int gMovieVoiceEndTick;
extern int *gMovieTemplateData;
extern int gMovieNode;
extern int gMovieVoiceRows;
extern short gMovieCellSize;
extern short gMovieSoundPhase;
extern short gMovieClutDirty;
extern byte *gMovieFrame;
extern int gMovieVoiceRowCount;
extern int gMovieFrameDueTick;
extern int gMovieStartTick;
extern byte gMovieName[16];
extern int *gMovieHeader;
extern int gMovieChannel;
extern int gMovieChannelRows;
extern int gMovieTemplateRows;
extern short gMovieStallCount;
extern byte gMovieHotspotBuf[120];
extern byte gMovieSavedHotspotBuf[128];
extern int DAT_0048cd;
extern int gMovieFrameIndex;
extern int gMovieTemplateCount;
extern short gMovieVoiceActive;
extern void *gMovieNavHistory;
extern int gMovieChildBase;
extern byte gMovieVoiceName[16];
extern short gMovieSoundArmed;
extern short gMovieNavState;
extern int gMovieTimedJumps;
extern void *gMovieClut;
extern byte gMovieVoiceRow[104];
extern int gMovieVoiceTable;
extern int gMovieNextFrame;
extern short gMovieRenderPhase;
extern void *gGlobalsNode;

#ifndef _MSC_VER
/* Packed image 0x00486e80..0x004872a8. The engine copies this range as one block, so every symbol in it is an lvalue macro at its exact offset rather than a separate object the compiler may re-align. */
extern byte g_hotspotBitmapInfo[0x428];
#define gBitmapHeader (*(BITMAPINFOHEADER *)&g_hotspotBitmapInfo[0x0])
#define DAT_00486e84 (*(int *)&g_hotspotBitmapInfo[0x4])
#define DAT_00486e88 (*(int *)&g_hotspotBitmapInfo[0x8])
#define DAT_00486e8c (*(short *)&g_hotspotBitmapInfo[0xc])
#define DAT_00486e8e (*(short *)&g_hotspotBitmapInfo[0xe])
#define DAT_00486e90 (*(int *)&g_hotspotBitmapInfo[0x10])
#define DAT_00486e94 (*(int *)&g_hotspotBitmapInfo[0x14])
#define DAT_00486e98 (*(int *)&g_hotspotBitmapInfo[0x18])
#define DAT_00486e9c (*(int *)&g_hotspotBitmapInfo[0x1c])
#define DAT_00486ea0 (*(int *)&g_hotspotBitmapInfo[0x20])
#define DAT_00486ea4 (*(int *)&g_hotspotBitmapInfo[0x24])
#define gDibColorTable (*(RGBQUAD (*)[256])&g_hotspotBitmapInfo[0x28])
#endif

/* Packed image 0x0048a560..0x0048a872. The engine copies this range as one block, so every symbol in it is an lvalue macro at its exact offset rather than a separate object the compiler may re-align. */
extern byte g_saveStateImage[0x312];
#define gChannels (*(ChannelState (*)[3])&g_saveStateImage[0x0])
#define gVisualEffectDepth (gSave.visualEffectDepth)
#define gKeyAborts (gSave.keyAborts)
#define gSceneFrame (gSave.sceneFrame)
#define gFrameRate (gSave.frameRate)
#define gActionFrameFlags (gSave.actionFrameFlags)
#define gActorTable (gSave.slot[0].table)
#define gActorCount (gSave.slot[0].count)
#define gCastTable (gSave.slot[1].table)
#define gCastCount (gSave.slot[1].count)
#define gPropTable (gSave.slot[2].table)
#define gPropCount (gSave.slot[2].count)
#define gShopTable (gSave.slot[3].table)
#define gShopCount (gSave.slot[3].count)
#define gTrackTable (gSave.slot[4].table)
#define gTrackCount (gSave.slot[4].count)
#define gStage (gSave.stage)
#define gSet (gSave.set)
#define gPuppet (gSave.puppet)
#define gClutSyncFirst (gSave.clutSyncFirst)
#define gClutSyncEnd (gSave.clutSyncEnd)
#define gTextColor (gSave.textColor)
#define gTextHighlightColor (gSave.textHighlightColor)
#define gTextFont (gSave.textFont)
#define gTextSize (gSave.textSize)
#define gPuppetParam7 (gSave.puppetParam7)
#define gPuppetNoTimeout (gSave.puppetNoTimeout)
#define gSpeakTicksPerChar (gSave.speakTicksPerChar)
#define gTextIndent (gSave.textIndent)
#define gPuppetGrab (gSave.puppetGrab)
#define gPuppetBase (*(int *)gSave.puppetBase)
#define gSceneW (gSave.sceneW)
#define gSceneH (gSave.sceneH)
#define gOverlayW (gSave.overlayW)
#define gOverlayH (gSave.overlayH)
#define gSceneOriginY (gSave.sceneOriginY)
#define gSceneOriginX (gSave.sceneOriginX)
#define gBootNode (gSave.bootNode)

#ifndef _MSC_VER
/* Packed image 0x0048af08..0x0048b420. The engine copies this range as one block, so every symbol in it is an lvalue macro at its exact offset rather than a separate object the compiler may re-align. */
extern byte g_textEntryTable[0x518];
#define gBevel (*(BevelTable *)g_textEntryTable)
#define gBevelCount (gBevel.count)
#define DAT_0048af0a (gBevel.selected)
#define gBevelTable (*(int (*)[325])gBevel.rows)
#endif

#ifndef _MSC_VER
/* Packed image 0x00487d48..0x0048814c. The engine copies this range as one block, so every symbol in it is an lvalue macro at its exact offset rather than a separate object the compiler may re-align. */
extern byte g_logPalImage[0x404];
#define gLogPal (*(LOGPALETTE *)&g_logPalImage[0x0])
#define DAT_00487d4a (*(int *)&g_logPalImage[0x2])
#define gPalEntries (*(PALETTEENTRY (*)[256])&g_logPalImage[0x4])
#endif

/* DAT_* symbols that are sub-fields/elements of another global --
 * Not real storage. */
#define DAT_004609a4 (gWaveHdrQueueTbl[0].tail)
#define DAT_004609a8 (gWaveHdrQueueTbl[0].cur)
#define DAT_004609ac (gWaveHdrQueueTbl[0].gainA.a.value)
#define DAT_004609ca (gWaveHdrQueueTbl[0].recycleMode)
#define DAT_004609ce (gWaveHdrQueueTbl[0].recycleTarget)
#define gWaveOutHdrHandles (gWaveOutHdrs[16])
#define DAT_00486980 (*(unsigned short *)((char *)gEventQueue + 0x10))
#define DAT_00486ea9 (gDibColorTable[0].rgbGreen)
#define DAT_00486eaa (gDibColorTable[0].rgbRed)
#define DAT_00489dcc (gFindData.cFileName[0])
#define DAT_00489f00 (gSoundSlotTbl[0].name[0])
#define DAT_00489f10 (gSoundSlotTbl[0].flag)
#define DAT_00489f1c (gSoundSlotTbl[0].aux)
#define DAT_00489f20 (gSoundSlotTbl[0].handle)
#define DAT_00489f2e (gSoundSlotTbl[0].templateKey)
#define DAT_00489f51 (*(int *)&gSoundSlotTbl[0].gain.a)
#define DAT_00489f55 (*(int *)((char *)&gSoundSlotTbl[0].gain + 4))
#define DAT_00489f59 (*(int *)((char *)&gSoundSlotTbl[0].gain + 8))
#define DAT_00489f5d (*(short *)((char *)&gSoundSlotTbl[0].gain + 12))
#define DAT_00489f5f (*(byte *)((char *)&gSoundSlotTbl[0].gain + 14))
#define DAT_00489f60 (gSoundSlotTbl[1].key)
#define DAT_00489f68 (gSoundSlotTbl[1].name[0])
#define DAT_00489f84 (gSoundSlotTbl[1].aux)
#define DAT_00489f88 (gSoundSlotTbl[1].handle)
#define DAT_00489fc8 (gSoundSlotTbl[2].key)
#define DAT_00489fd0 (gSoundSlotTbl[2].name[0])
#define DAT_00489fec (gSoundSlotTbl[2].aux)
#define DAT_00489ff0 (gSoundSlotTbl[2].handle)
#define DAT_0048a030 ((byte *)&gSoundSlotTbl[3])
#define DAT_0048a038 (DAT_0048a030[8])
#define DAT_0048a054 (*(int *)&DAT_0048a030[0x24])
#define DAT_0048a058 (*(int *)&DAT_0048a030[0x28])
#define DAT_0048a0f8 (*(int *)&gLatchedMethodFrame[0x18])
#define DAT_0048a149 (*(byte *)&gScratchStr[1])
#define DAT_0048a2c4 (*(int *)&gScrollSceneRect.y2)
#define DAT_0048a459 (*(byte *)&gScratchStr2[1])
#define DAT_0048a572 (gChannels[0].hResource)
#define gChannelB (gChannels[1])
#define DAT_0048a604 (gChannels[1].hResource)
#define gChannelC (gChannels[2])
#define DAT_0048a696 (gChannels[2].hResource)
#define DAT_0048a74e (gStage.flag1)
#define DAT_0048a750 (gStage.flag2)
#define DAT_0048a752 (gStage.w06)
#define DAT_0048a754 (gStage.ctx)
#define DAT_0048a758 (gStage.page)
#define DAT_0048a75c (gStage.prevPage)
#define DAT_0048a760 (gStage.hChild)
#define DAT_0048a764 (gStage.selector)
#define DAT_0048a768 (gStage.name[0])
#define DAT_0048a77a (gSet.w02)
#define DAT_0048a77c (gSet.w04)
#define DAT_0048a77e (gSet.w06)
#define DAT_0048a780 (gSet.i08)
#define DAT_0048a784 (gSet.p0c)
#define DAT_0048a788 (gSet.i10)
#define DAT_0048a78c (gSet.w14)
#define DAT_0048a78e (gSet.w16)
#define DAT_0048a790 (gSet.w18)
#define gCameraHeight (gSet.cameraHeight)
#define DAT_0048a794 (gSet.w1c)
#define DAT_0048a796 (gSet.w1e)
#define DAT_0048a798 (gSet.w20)
#define DAT_0048a79a (gSet.w22)
#define DAT_0048a79c (gSet.w24)
#define DAT_0048a79e (gSet.w26)
#define DAT_0048a7a0 (gSet.w28)
#define DAT_0048a7a2 (gSet.w2a)
#define gCurrentDeg (gSet.currentDeg)
#define gPlayerX (gSet.playerX)
#define gPlayerY (gSet.playerY)
#define gPlayerZ (gSet.playerZ)
#define gCameraAngle (gSet.cameraAngle)
#define gCameraX (gSet.cameraX)
#define gCameraY (gSet.cameraY)
#define gCameraZ (gSet.cameraZ)
#define DAT_0048a7b4 (gSet.name[0])
#define gSceneName (gSet.sceneName[0])
#define gViewName (gSet.viewName[0])
#define DAT_0048a7e4 (gSet.i6c)
#define gWaypointTable (gSet.waypointTable)
#define DAT_0048a7ec (gSet.i74)
#define gSceneCount (gSet.sceneCount)
#define gSceneTable (gSet.sceneTable)
#define DAT_0048a7fa (gPuppet.w02)
#define DAT_0048a7fc (gPuppet.i04)
#define DAT_0048a804 (gPuppet.i0c)
#define DAT_0048a808 (gPuppet.p10)
#define DAT_0048a80c (gPuppet.p14)
#define DAT_0048a810 (gPuppet.p18)
#define DAT_0048a814 (gPuppet.p1c)
#define DAT_0048a818 (gPuppet.p20)
#define DAT_0048a81c (gPuppet.name)
#define DAT_0048a882 (gClipRect[1])
#define DAT_0048a884 (gClipRect[2])
#define DAT_0048a886 (gClipRect[3])
#define DAT_0048a9c4 (*(CRITICAL_SECTION *)&gPrimaryHeap[5])
#define DAT_0048a9dc (gPrimaryHeap[11])
#define DAT_0048ab41 (gTextEntryBuf[1])
#define DAT_0048af10 (gBevel.rows[0].name)
#define DAT_0048cd48 (gMovieVoice.name[0])

#ifdef _MSC_VER
/* The VC++ build's regions: Ghidra-split structs as one object each;
 * every member is an lvalue into it. DF_R_<base> says the region is in force. */
#define DF_R_DAT_00460cd0 1
#undef gWaveOutFramesPerBuffer
#define gWaveOutFramesPerBuffer (*(int *)((char *)gWaveOutFramesPerBuffer_r + 0))
#undef gWaveOutHdrs
#define gWaveOutHdrs (*(int (*)[32])((char *)gWaveOutFramesPerBuffer_r + 4))
#define DF_R_DAT_00486e80 1
#undef gBitmapHeader
#define gBitmapHeader (*(BITMAPINFOHEADER *)((char *)gBitmapHeader_r + 0))
#undef DAT_00486e84
#define DAT_00486e84 (*(int *)((char *)gBitmapHeader_r + 4))
#undef DAT_00486e88
#define DAT_00486e88 (*(int *)((char *)gBitmapHeader_r + 8))
#undef DAT_00486e8c
#define DAT_00486e8c (*(short *)((char *)gBitmapHeader_r + 12))
#undef DAT_00486e8e
#define DAT_00486e8e (*(short *)((char *)gBitmapHeader_r + 14))
#undef DAT_00486e90
#define DAT_00486e90 (*(int *)((char *)gBitmapHeader_r + 16))
#undef DAT_00486e94
#define DAT_00486e94 (*(int *)((char *)gBitmapHeader_r + 20))
#undef DAT_00486e98
#define DAT_00486e98 (*(int *)((char *)gBitmapHeader_r + 24))
#undef DAT_00486e9c
#define DAT_00486e9c (*(int *)((char *)gBitmapHeader_r + 28))
#undef DAT_00486ea0
#define DAT_00486ea0 (*(int *)((char *)gBitmapHeader_r + 32))
#undef DAT_00486ea4
#define DAT_00486ea4 (*(int *)((char *)gBitmapHeader_r + 36))
#define DF_R_DAT_00487540 1
#undef gPal16
#define gPal16 (*(PAL16ENTRY (*)[256])((char *)DAT_00487540_r + 10))
#define DF_R_DAT_00487d48 1
#undef gLogPal
#define gLogPal (*(LOGPALETTE *)((char *)gLogPal_r + 0))
#undef DAT_00487d4a
#define DAT_00487d4a (*(int *)((char *)gLogPal_r + 2))
#undef gPalEntries
#define gPalEntries (*(PALETTEENTRY (*)[256])((char *)gLogPal_r + 4))
#define DF_R_DAT_0048af08 1
#define gBevel (*(BevelTable *)gBevelCount_r)
#undef gBevelCount
#define gBevelCount (gBevel.count)
#undef DAT_0048af0a
#define DAT_0048af0a (gBevel.selected)
#undef gBevelTable
#define gBevelTable (*(int (*)[325])gBevel.rows)
#endif
#endif /* DF_GLOBALS_H */
