/*
 * globals.c -- the single linker-visible definition of each global, its TI.EXE address beside it.
 * match/globals.tsv lists the identifier the byte-match build knows each one by. A global added,
 * renamed or retyped here is changed in include/df_globals.h too.
 */

#include "df_types.h"
#include "df_fixes.h"

int kUnityGain = 1;  /* 0x0045b030 */
int kUnityGainWord1 = 0x100;  /* 0x0045b034 */
int kUnityGainWord2 = 0x10000;  /* 0x0045b038 */
int gWaveOutOpen;  /* 0x0045b060 */
int gWaveAltDeviceOpen;  /* 0x0045b064 */
int gMixerVolumeReady;  /* 0x0045b068 */
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrRight[7] = "\0right";  /* 0x0045b128 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrLeft[6] = "\0left";  /* 0x0045b130 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrStrait[8] = "\0strait";  /* 0x0045b138 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaOpensceneCall[16] = "\0\", openscene()";  /* 0x0045b140 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuote[3] = "\0\"";  /* 0x0045b150 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaClosesceneCall[17] = "\0\", closescene()";  /* 0x0045b154 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrOpensetCall[11] = "\0openset()";  /* 0x0045b168 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrClosesetCall[12] = "\0closeset()";  /* 0x0045b174 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSystemColon[10] = "\0System: ";  /* 0x0045b180 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSceneMessage[15] = "\0Scene Message";  /* 0x0045b18c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrPostScript[13] = "\0Post Script";  /* 0x0045b19c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSetScriptColon[14] = "\0Set Script: ";  /* 0x0045b1ac */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSceneScriptColon[16] = "\0Scene Script: ";  /* 0x0045b1bc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSetMessage[13] = "\0Set Message";  /* 0x0045b1cc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrMoving[8] = "\0Moving";  /* 0x0045b1dc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrNone[6] = "\0None";  /* 0x0045b1e4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrPaintScriptColon[16] = "\0Paint Script: ";  /* 0x0045b1ec */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrCommaEveryoneClose[14] = "\0,\"everyone\")";  /* 0x0045b1fc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrAddPlayerOpen[12] = "\0addplayer(";  /* 0x0045b20c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrGetmemberid[13] = "\0getmemberid";  /* 0x0045b218 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrGetmembercount[16] = "\0getmembercount";  /* 0x0045b228 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrEveryone[10] = "\0everyone";  /* 0x0045b238 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrGetourid[10] = "\0getourid";  /* 0x0045b244 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrConnect[9] = "\0connect";  /* 0x0045b250 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrNetwork[9] = "\0network";  /* 0x0045b25c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrDisconnect[12] = "\0disconnect";  /* 0x0045b268 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSendmessage[13] = "\0sendmessage";  /* 0x0045b274 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrRParen[3] = "\0)";  /* 0x0045b284 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrComma[3] = "\0,";  /* 0x0045b288 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrLParen[3] = "\0(";  /* 0x0045b28c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrJoingroup[11] = "\0joingroup";  /* 0x0045b290 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrLeavegroup[12] = "\0leavegroup";  /* 0x0045b29c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteRParen[4] = "\0\")";  /* 0x0045b2a8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrCommaQuote[4] = "\0,\"";  /* 0x0045b2ac */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrDelPlayerOpen[12] = "\0delplayer(";  /* 0x0045b2b0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrJoinedSpace[9] = "\0joined ";  /* 0x0045b2bc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrLeftSpace[7] = "\0left ";  /* 0x0045b2c8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrPlayerSpace[9] = "\0player ";  /* 0x0045b2d0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrDisconnectedCall[16] = "\0disconnected()";  /* 0x0045b2dc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrDisconnected[14] = "\0disconnected";  /* 0x0045b2ec */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrGetnotify[11] = "\0getnotify";  /* 0x0045b2fc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrGetmessage[12] = "\0getmessage";  /* 0x0045b308 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrGetmessagesender[18] = "\0getmessagesender";  /* 0x0045b314 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrGetmessagegroup[17] = "\0getmessagegroup";  /* 0x0045b328 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrClearAll[11] = "\0Clear All";  /* 0x0045b33c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrError[7] = "\0Error";  /* 0x0045b348 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrGo[4] = "\0Go";  /* 0x0045b350 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrStepOut[10] = "\0Step Out";  /* 0x0045b354 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrStepIn[9] = "\0Step In";  /* 0x0045b360 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrStep[6] = "\0Step";  /* 0x0045b36c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrScripts[9] = "\0Scripts";  /* 0x0045b374 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSCRIPTS[9] = "\0SCRIPTS";  /* 0x0045b380 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrLocals[8] = "\0Locals";  /* 0x0045b38c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrLOCALS[8] = "\0LOCALS";  /* 0x0045b394 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrGlobals[9] = "\0Globals";  /* 0x0045b39c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrGLOBALS[9] = "\0GLOBALS";  /* 0x0045b3a8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrCR[3] = "\0\r";  /* 0x0045b3b4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrFalse[7] = "\0False";  /* 0x0045b3b8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrTrue[6] = "\0True";  /* 0x0045b3c0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrEquals[5] = "\0 = ";  /* 0x0045b3c8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrFalseLower[7] = "\0false";  /* 0x0045b3d0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrTrueLower[6] = "\0true";  /* 0x0045b3d8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrAll[5] = "\0all";  /* 0x0045b3e0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrCustom[8] = "\0custom";  /* 0x0045b3e8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaOpenactorCall[16] = "\0\", openactor()";  /* 0x0045b3f0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaOpencastCall[15] = "\0\", opencast()";  /* 0x0045b400 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrActor[7] = "\0actor";  /* 0x0045b410 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaClosecastCall[16] = "\0\", closecast()";  /* 0x0045b418 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaCloseactorCall[17] = "\0\", closeactor()";  /* 0x0045b428 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrActorScriptColon[16] = "\0Actor Script: ";  /* 0x0045b43c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrCastScriptColon[15] = "\0Cast Script: ";  /* 0x0045b44c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrActorMessage[15] = "\0Actor Message";  /* 0x0045b45c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrCastMessage[14] = "\0Cast Message";  /* 0x0045b46c */
#endif
int gTrigPlaneDecodeMode = 3;  /* 0x0045b47c */
int DAT_0045b480 = 2;  /* 0x0045b480 */
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSaveGame[10] = "\0SaveGame";  /* 0x0045b484 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSaveWriteError[75] = "\0Error writing to save game file.  Be sure the disk has enough free space.";  /* 0x0045b490 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSaveCreateError[75] = "\0Unable to create save game file.  Be sure the disk isn't write protected.";  /* 0x0045b4dc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSaveVersionError[60] = "\0This saved game is from a different version of this title.";  /* 0x0045b528 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSaveInvalidError[38] = "\0This is not a valid saved game file.";  /* 0x0045b564 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaOpenpropCall[15] = "\0\", openprop()";  /* 0x0045b58c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaOpenshopCall[15] = "\0\", openshop()";  /* 0x0045b59c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrProp[6] = "\0prop";  /* 0x0045b5ac */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaCloseshopCall[16] = "\0\", closeshop()";  /* 0x0045b5b4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaClosepropCall[16] = "\0\", closeprop()";  /* 0x0045b5c4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrPropScriptColon[15] = "\0Prop Script: ";  /* 0x0045b5d4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrShopScriptColon[15] = "\0Shop Script: ";  /* 0x0045b5e4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrPropMessage[14] = "\0Prop Message";  /* 0x0045b5f4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrShopMessage[14] = "\0Shop Message";  /* 0x0045b604 */
#endif
int gGamePaused;  /* 0x0045b618 */
int kSysPalIDs[25] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};  /* 0x0045b620 */
COLORREF kGameSysColors[25] = {16777215, 16777215, 0, 0, 16777215, 16777215, 0, 0, 0, 16777215, 0, 0, 16777215, 0, 16777215, 16777215, 0, 0, 0, 16777215, 0, 0, 0, 16777215, 0};  /* 0x0045b688 */
int gPalOwnerState;  /* 0x0045b6ec */
double gGammaR = 0.65;  /* 0x0045b6f0 */
double gGammaG = 0.65;  /* 0x0045b6f8 */
double gGammaB = 0.65;  /* 0x0045b700 */
int gGammaTablesBuilt;  /* 0x0045b708 */
int gSavedWindowCount;  /* 0x0045b70c */
HMODULE gUser32Lib;  /* 0x0045b710 */
int gJoy1PrevButtons;  /* 0x0045b714 */
int gJoy2PrevButtons;  /* 0x0045b718 */
DWORD gJoyRepeatTime;  /* 0x0045b71c */
DWORD gWinNTBuild;  /* 0x0045b720 */
HANDLE gVxdDevice;  /* 0x0045b724 */
int gCursorShowDepth;  /* 0x0045b728 */
char gVolumeLabelCache[256];  /* 0x0045b730 */
char gVolumePathCache[256];  /* 0x0045b830 */
int gVolumeCacheNext;  /* 0x0045b930 */
int gMsgPumpDepth;  /* 0x0045b934 */
DWORD gLastPollTime;  /* 0x0045b938 */
byte kStipplePattern[8] = {0xaa,0x55,0xaa,0x55,0xaa,0x55,0xaa,0x55};  /* 0x0045b940 */
int gOpenFileNameInited;  /* 0x0045b948 */
#if DF_FIX_WRITABLE_PASCAL_LITERALS
const char kStrEmpty[4] = "";  /* 0x0045bbf8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSpace[3] = "\0 ";  /* 0x0045d684 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrTab[3] = "\0\t";  /* 0x0045d688 */
#endif
int gDisplayAttachLoaderRunning;  /* 0x0045d68c */
int gDirSearchHandle;  /* 0x0045d690 */
int gAttachLoaderRunning;  /* 0x0045d694 */
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrTestfile[10] = "\0testfile";  /* 0x0045d698 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrAllFiles[5] = "\0*.*";  /* 0x0045d6a4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrColonTestfile[11] = "\0:testfile";  /* 0x0045d6ac */
#endif
short gSoundOn;  /* 0x0045d6b8 */
char kAudioDeltaTable1[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -7, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -5, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};  /* 0x0045d6c0 */
char kAudioDeltaTable2[256] = {0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1};  /* 0x0045d7c0 */
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrEmptyPascal[4] = "\0";  /* 0x0045d8c0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrFlat[6] = "\0flat";  /* 0x0045d8c4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrButton[8] = "\0button";  /* 0x0045d8cc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrScene[7] = "\0scene";  /* 0x0045d8d4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrPainting[10] = "\0painting";  /* 0x0045d8dc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrUnknown[9] = "\0unknown";  /* 0x0045d8e8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrLogic[7] = "\0logic";  /* 0x0045d8f4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrNumber[8] = "\0number";  /* 0x0045d8fc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrText[6] = "\0text";  /* 0x0045d904 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrBootfile[10] = "\0bootfile";  /* 0x0045d90c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrRuntime[9] = "\0Runtime";  /* 0x0045d918 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrIdleCall[8] = "\0idle()";  /* 0x0045d924 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrBootCall[8] = "\0boot()";  /* 0x0045d92c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrMenustateCall[13] = "\0menustate()";  /* 0x0045d934 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrMenuSelectOpenQuote[14] = "\0menuselect(\"";  /* 0x0045d944 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrRightarrow[12] = "\0rightarrow";  /* 0x0045d954 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrLeftarrow[11] = "\0leftarrow";  /* 0x0045d960 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrDownarrow[11] = "\0downarrow";  /* 0x0045d96c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrUparrow[9] = "\0uparrow";  /* 0x0045d978 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrKeyDownOpenQuote[11] = "\0keydown(\"";  /* 0x0045d984 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrKeyRepeatOpenQuote[13] = "\0keyrepeat(\"";  /* 0x0045d990 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrMouseDownOpen[12] = "\0mousedown(";  /* 0x0045d9a0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrBootScript[13] = "\0Boot Script";  /* 0x0045d9ac */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrServerScript[15] = "\0Server Script";  /* 0x0045d9bc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrBootMessage[14] = "\0Boot Message";  /* 0x0045d9cc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrServerMessage[16] = "\0Server Message";  /* 0x0045d9dc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrRBracket[3] = "\0]";  /* 0x0045d9ec */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrWIN[5] = "\0WIN";  /* 0x0045d9f0 */
#endif
byte kCaseFoldTable[256] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255};  /* 0x0045da58 */
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrBlack[7] = "\0Black";  /* 0x0045db58 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSystem[8] = "\0System";  /* 0x0045db60 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrPluginsColon[10] = "\0plugins:";  /* 0x0045db78 */
#endif
int gRenderSlotCounter;  /* 0x0045db88 */
void *gRenderNodeList;  /* 0x0045db8c */
int gPendingRenderNodeList;  /* 0x0045db90 */
short  (*gMemoryPressureCallback)(void);  /* 0x0045db94 */
int gNodeDestroyHook;  /* 0x0045db98 */
int *gCurrentParentNode;  /* 0x0045db9c */
int gChildSerialCounter;  /* 0x0045dba0 */
int DAT_0045dba4;  /* 0x0045dba4 */
short kReadBufSizeTable[22] = {19, 53, 89, 131, 173, 223, 263, 311, 359, 409, 457, 503, 569, 613, 659, 719, 769, 827, 881, 941, 997, 1021};  /* 0x0045dba8 */
int gScanLockInited;  /* 0x0045dbd4 */
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrOK[4] = "\0OK";  /* 0x0045dbd8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrMessageBox[13] = "\0Message Box";  /* 0x0045dbdc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrTokenizationError[20] = "\0tokenization error";  /* 0x0045dbec */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrWatch[7] = "\0watch";  /* 0x0045dc00 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrArrow[7] = "\0arrow";  /* 0x0045dc08 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrPuppet[8] = "\0puppet";  /* 0x0045dc10 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrStage[7] = "\0stage";  /* 0x0045dc18 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSet[5] = "\0set";  /* 0x0045dc20 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrBlackLower[7] = "\0black";  /* 0x0045dc28 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrSystemLower[8] = "\0system";  /* 0x0045dc30 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrCurrent[9] = "\0current";  /* 0x0045dc38 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaOpenpuppetCall[17] = "\0\", openpuppet()";  /* 0x0045dc44 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaClosepuppetCall[18] = "\0\", closepuppet()";  /* 0x0045dc58 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrPuppetMessage[16] = "\0Puppet Message";  /* 0x0045dc6c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrPuppetScriptColon[17] = "\0Puppet Script: ";  /* 0x0045dc7c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrPuppetVoice[14] = "\0Puppet Voice";  /* 0x0045dc90 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrIdle4[8] = "\0idle 4";  /* 0x0045dca0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrIdle3[8] = "\0idle 3";  /* 0x0045dca8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrIdle2[8] = "\0idle 2";  /* 0x0045dcb0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrIdle1[8] = "\0idle 1";  /* 0x0045dcb8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrPlayerVoice[14] = "\0Player Voice";  /* 0x0045dcc0 */
#endif
byte kDeltaPixelMagnitudes[16] = {8, 8, 8, 8, 8, 8, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0};  /* 0x0045dcd0 */
byte kDirectionAngleTable[512] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31};  /* 0x0045dce0 */
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrEmptyCall[4] = "\0()";  /* 0x0045dee0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaSpace[5] = "\0\", ";  /* 0x0045dee4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrWalktostar[12] = "\0walktostar";  /* 0x0045deec */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrWalktoxyz[11] = "\0walktoxyz";  /* 0x0045def8 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrWalkonpath[12] = "\0walkonpath";  /* 0x0045df04 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrResume[8] = "\0resume";  /* 0x0045df10 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrWalkonroad[12] = "\0walkonroad";  /* 0x0045df18 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrWalkonframes[14] = "\0walkonframes";  /* 0x0045df24 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrDefer[7] = "\0defer";  /* 0x0045df34 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaEndwalkCall[14] = "\0\", endwalk()";  /* 0x0045df3c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaEndturnCall[14] = "\0\", endturn()";  /* 0x0045df4c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaOpenflatCall[15] = "\0\", openflat()";  /* 0x0045df5c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrQuoteCommaCloseflatCall[16] = "\0\", closeflat()";  /* 0x0045df6c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrOpenstageCall[13] = "\0openstage()";  /* 0x0045df7c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrClosestageCall[14] = "\0closestage()";  /* 0x0045df8c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrStageScriptColon[16] = "\0Stage Script: ";  /* 0x0045df9c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrFlatScriptColon[15] = "\0Flat Script: ";  /* 0x0045dfac */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrButtonScriptColon[17] = "\0Button Script: ";  /* 0x0045dfbc */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrFlatMessage[14] = "\0Flat Message";  /* 0x0045dfd0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrStageMessage[15] = "\0Stage Message";  /* 0x0045dfe0 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrNoPrevFrame[29] = "\0Can't go to previous frame.";  /* 0x0045e1c4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrNoNextFrame[25] = "\0Can't go to next frame.";  /* 0x0045e1e4 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrNoPopFrame[28] = "\0Can't pop any more frames.";  /* 0x0045e200 */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrNoPushFrame[29] = "\0Can't push any more frames.";  /* 0x0045e21c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrAposDot[4] = "\0'.";  /* 0x0045e23c */
#endif
#if DF_FIX_WRITABLE_PASCAL_LITERALS
char kStrJumpToFrameError[24] = "\0Can't jump to frame: '";  /* 0x0045e240 */
#endif
int gMixRingFillFrames;  /* 0x00460870 */
int gWaveFlushLatencySum;  /* 0x00460874 */
int gWaveBufferActive[4];  /* 0x00460878 */
int gMixRingTailCursor;  /* 0x00460888 */
int gWaveFeedLatencyCount;  /* 0x0046088c */
int gMixRingBase;  /* 0x00460890 */
int gMixRingWriteCursor;  /* 0x00460894 */
int gWaveBufferFormatQueue[4];  /* 0x00460898 */
int gWaveBufferLoopGatePending[4];  /* 0x004608a8 */
int gMixBufferCount;  /* 0x004608b8 */
int gMixChunkFrames;  /* 0x004608bc */
int gMixSampleRate;  /* 0x004608c0 */
int gMixBytesPerSample;  /* 0x004608c4 */
int gMixSampleChannels;  /* 0x004608c8 */
int gMixFrameBytes;  /* 0x004608cc */
int gMixFrameShift;  /* 0x004608d0 */
int gMixSilenced;  /* 0x004608d4 */
int gMixQueueTarget;  /* 0x004608d8 */
int gMixFlushMask;  /* 0x004608dc */
int gSoundBufferDescs[30];  /* 0x004608e0 */
int gSoundBufferDescCount;  /* 0x00460958 */
int gSoundVxdHeapSize;  /* 0x0046095c */
int gSoundHeap[12];  /* 0x00460960 */
int gWaveDeviceOps;  /* 0x00460990 */
int gMixFramesPending;  /* 0x00460994 */
int gWaveFeedLatencySum;  /* 0x00460998 */
unsigned char gWaveHdrQueues[200];  /* 0x004609a0 */
int gWaveFlushLatencyCount;  /* 0x00460a68 */
CRITICAL_SECTION gSoundMixerLock;  /* 0x00460a70 */
int gSoundMixerLocked;  /* 0x00460a88 */
int gWaveBindsPending;  /* 0x00460a8c */
int gMixRingReadCursor;  /* 0x00460a90 */
int gMixRingEnd;  /* 0x00460a94 */
int gMixFramesQueuedTotal;  /* 0x00460a98 */
int gScriptWorkQueue[80];  /* 0x00460aa0 */
short gScriptWorkQueueTop;  /* 0x00460be0 */
#ifdef _MSC_VER
short gStringSlotUsed[20];  /* 0x00460be8 */
#else
short gStringSlotUsed[64];  /* 0x00460be8 */
#endif
void *gStringSlots;  /* 0x00460c10 */
void *gMixerGetControlDetailsFn;  /* 0x00460c18 */
int gMixerControlDetails[6];  /* 0x00460c20 */
void *gMixerGetLineInfoFn;  /* 0x00460c38 */
int gMixerVolumeMin;  /* 0x00460c3c */
int gMixerVolumeMax;  /* 0x00460c40 */
int gWaveOutSyncPlaySlot;  /* 0x00460c44 */
int gWaveOutOnWinNT;  /* 0x00460c48 */
int gWaveOutSyncInFlight;  /* 0x00460c4c */
void *gMixerGetNumDevsFn;  /* 0x00460c50 */
void *gMixerGetIDFn;  /* 0x00460c54 */
void *gMixerGetLineControlsFn;  /* 0x00460c58 */
int DAT_00460c60;  /* 0x00460c60 */
void *DAT_00460c64;  /* 0x00460c64 */
void *DAT_00460c68;  /* 0x00460c68 */
void *DAT_00460c6c;  /* 0x00460c6c */
void *DAT_00460c9c;  /* 0x00460c9c */
int gWaveOutSampleRate;  /* 0x00460cb8 */
int gWaveOutBlockAlign;  /* 0x00460cbc */
int gWaveOutInFlight;  /* 0x00460cc0 */
int gWaveOutPlaySlot;  /* 0x00460cc4 */
int gWaveOutWriteSlot;  /* 0x00460cc8 */
int gWaveOutUseGetPosition;  /* 0x00460ccc */
#ifdef _MSC_VER
int gWaveOutFramesPerBuffer_r[33];  /* 0x00460cd0 */
#else
int gWaveOutFramesPerBuffer;  /* 0x00460cd0 */
#endif
#ifndef _MSC_VER
int gWaveOutHdrs[33];  /* 0x00460cd4 */
#endif
int gWaveOutDeviceId;  /* 0x00460d54 */
int gWaveOut;  /* 0x00460d58 */
int gWaveOutQueueTarget;  /* 0x00460d5c */
int gWaveOutRingSize;  /* 0x00460d60 */
int gWaveOutBufferBytes;  /* 0x00460d64 */
int gWaveOutFlushMask;  /* 0x00460d68 */
int gWaveOutResetPending;  /* 0x00460d6c */
int gWinmmLib;  /* 0x00460d70 */
int gMixerVolumeValue;  /* 0x00460d74 */
void *gMixerSetControlDetailsFn;  /* 0x00460d7c */
int DAT_00460d80;  /* 0x00460d80 */
int DAT_00460d84;  /* 0x00460d84 */
void *gMixer;  /* 0x00460d8c */
int gWaveOutOnWin32s;  /* 0x00460d90 */
void *gMixerCloseFn;  /* 0x00460d94 */
void *gMixerOpenFn;  /* 0x00460d98 */
int gWaveOutOnWin95;  /* 0x00460d9c */
short gNetworkOn;  /* 0x00460da0 */
int gNetOurId;  /* 0x00460da4 */
int gNetDirEntry;  /* 0x00460da8 */
short gDebuggerStepMode;  /* 0x00460dc0 */
int gDebuggerWindowRefCount;  /* 0x00460dc4 */
short gDebuggerWatchCount;  /* 0x00460dc8 */
int gDebuggerCaretOffset;  /* 0x00460dcc */
int gDebuggerToken;  /* 0x00460dd0 */
short gDebuggerBreakpointCount;  /* 0x00460dd4 */
void *gDebuggerBreakpoints;  /* 0x00460dd8 */
short gDebuggerTargetDepth;  /* 0x00460ddc */
int gDebuggerLocalsNode;  /* 0x00460de0 */
short gDebuggerPane;  /* 0x00460de4 */
int gDebuggerFrame;  /* 0x00460de8 */
short gDebuggerDepth;  /* 0x00460dec */
void *gDebuggerListing;  /* 0x00460df0 */
void *gDebuggerWatches;  /* 0x00460df4 */
short gDebuggerRunMode;  /* 0x00460df8 */
int gNamedEntryCacheIndex;  /* 0x00460dfc */
int gTrigZoneBand;  /* 0x00460e00 */
int gTrigSrcToDstStore[1026];  /* 0x00460e08 */
#ifdef _MSC_VER
int gTrigPlaneSpanBufs[8];  /* 0x00461e10 */
#else
int gTrigPlaneSpanBufs[9];  /* 0x00461e10 */
#endif
int gTrigPlaneSpanBuf8;  /* 0x00461e30 */
int gScanNodeStore[10240];  /* 0x00461e38 */
int gTrigPlaneSpanCounts[8];  /* 0x0046be38 */
int gTrigDstWidth;  /* 0x0046be58 */
byte gTrigPlaneSpanStore8[4096];  /* 0x0046be60 */
char gTrigPlaneSpanStore[65536];  /* 0x0046ce60 */
int gTrigBandListLen;  /* 0x0047ce60 */
int gTrigHitSpanCount;  /* 0x0047ce64 */
int gTrigZoneRowOffset;  /* 0x0047ce68 */
int gTrigScanRow;  /* 0x0047ce6c */
int gTrigBandOriginX;  /* 0x0047ce70 */
int gTrigDstToSrcMap;  /* 0x0047ce74 */
int gTrigDstRows;  /* 0x0047ce78 */
int gScanNodeCount;  /* 0x0047ce7c */
int gTrigDstToSrcStore[4097];  /* 0x0047ce80 */
int gScanNodeBase;  /* 0x00480e84 */
int gTrigHitSpanStore[2048];  /* 0x00480e88 */
int gTrigDstStride;  /* 0x00482e88 */
int gTrigPlaneSpans[8];  /* 0x00482e90 */
int gTrigPixelRowBase;  /* 0x00482eb0 */
byte gTrigPixelRows[8192];  /* 0x00482eb8 */
int gTrigHitSpans;  /* 0x00484eb8 */
int gTrigSrcRows;  /* 0x00484ebc */
int gTrigPixelRowsPtr;  /* 0x00484ec0 */
int gTrigZoneMapData;  /* 0x00484ec4 */
int gTrigClipRight;  /* 0x00484ec8 */
int gTrigBandListStore[1024];  /* 0x00484ed0 */
int gTrigSrcToDstMap;  /* 0x00485ed0 */
int *gTrigBandList;  /* 0x00485ed4 */
int gTrigSrcWidth;  /* 0x00485ed8 */
int gTrigClipLeft;  /* 0x00485edc */
int gTrigMirrored;  /* 0x00485ee0 */
void *gPropDescriptorTable;  /* 0x00485ee4 */
int gStateEntryCacheIndex;  /* 0x00485ee8 */
char gOomPoolMinFormat[40];  /* 0x00485ef0 */
int gJoystickPresent;  /* 0x00485f18 */
char gOomPageFileFormat[40];  /* 0x00485f20 */
int gTextEntryPrompt;  /* 0x00485f48 */
HPALETTE gPalette;  /* 0x00485f4c */
int gNoEjectFlag;  /* 0x00485f50 */
int gBackBufferDC;  /* 0x00485f54 */
DWORD gTotalPhysRam;  /* 0x00485f58 */
short gGammaSlopeG[256];  /* 0x00485f60 */
HCURSOR gWatchCursor;  /* 0x00486160 */
int gBackBufferSavedObject;  /* 0x00486164 */
int gGraphicsLibrary;  /* 0x00486168 */
int gTextDialogReplyDesc;  /* 0x0048616c */
char gOpenFileInitialDir[256];  /* 0x00486170 */
int gJoystickId;  /* 0x00486270 */
SIZE_T gPoolSizeBytes;  /* 0x00486274 */
int gEventQueueCount;  /* 0x00486278 */
int gAlternateDisplayMode;  /* 0x0048627c */
int gSavedCursorShowDepth;  /* 0x00486280 */
int gTextDialogStatus;  /* 0x00486284 */
int gSystemFont;  /* 0x00486288 */
int gDefaultPalette;  /* 0x0048628c */
int gNoJoystickFlag;  /* 0x00486290 */
int gNoDisplayChangeFlag;  /* 0x00486294 */
int gPreferredDisplayMode;  /* 0x00486298 */
void *gWinGSetDIBColorTableProc;  /* 0x0048629c */
char gOutOfMemPoolSizeFmt[40];  /* 0x004862a0 */
int gJoystickRangeY;  /* 0x004862c8 */
void *gPoolBase;  /* 0x004862d0 */
int gJoystickRangeX;  /* 0x004862d4 */
char gSaveFileFilter[128];  /* 0x004862d8 */
int gIsAppActive;  /* 0x00486358 */
OPENFILENAMEA gOpenFileName;  /* 0x00486360 */
void *gEnumDisplaySettingsProc;  /* 0x004863ac */
char gOutOfMemUsableFmt[40];  /* 0x004863b0 */
int gIsWinGBackend;  /* 0x004863d8 */
int gMainWindow;  /* 0x004863dc */
char gOpenFileFilter[128];  /* 0x004863e0 */
HWND gCoverWindow;  /* 0x00486460 */
int gStereoSoundFlag;  /* 0x00486464 */
char gOpenFilePath[256];  /* 0x00486468 */
short gGammaSlopeR[256];  /* 0x00486568 */
char gAppTitle[0x40];  /* 0x00486768 */
int gWhiteBrush;  /* 0x004867a8 */
int gNullPen;  /* 0x004867e0 */
char gStringResourceBuf[384];  /* 0x004867e8 */
int gCoverWindowWasVisible;  /* 0x00486968 */
GameEvent gEventQueue[32];  /* 0x00486970 */
#ifdef _MSC_VER
void * gViewportTable[16];  /* 0x00486bb0 */
#else
void * gViewportTable[32];  /* 0x00486bb0 */
#endif
int gIsDisplayModeChanged;  /* 0x00486bf0 */
int gIs386Cpu;  /* 0x00486bf4 */
char gOutOfMemAvailPhysFmt[40];  /* 0x00486bf8 */
DWORD gJoystickReleaseThreshold;  /* 0x00486c20 */
UINT gJoystickCenterX;  /* 0x00486c24 */
int gIsSceneBitmapSelected;  /* 0x00486c28 */
short gIsMenuVisible;  /* 0x00486c2c */
byte *gNavCommandTable;  /* 0x00486c50 */
DWORD gPoolReserveBytes;  /* 0x00486c54 */
int gExitWindowsExProc;  /* 0x00486c58 */
short gGammaSlopeB[256];  /* 0x00486c60 */
UINT gJoystickCenterY;  /* 0x00486e60 */
int gIsWin95OrNT4;  /* 0x00486e64 */
int gAccel;  /* 0x00486e68 */
DWORD gAvailPhysBytes;  /* 0x00486e6c */
SIZE_T gPoolAllocatedBytes;  /* 0x00486e70 */
DWORD gJoystickDeflectThreshold;  /* 0x00486e74 */
int gNavCommandCount;  /* 0x00486e78 */
int gThePortDesc[32];  /* 0x004872a8 */
short gJoystickDirection;  /* 0x00487328 */
void *gChangeDisplaySettingsProc;  /* 0x0048732c */
int gNullBrush;  /* 0x00487330 */
short gGammaBaseB[256];  /* 0x00487338 */
DWORD gPoolMinBytes;  /* 0x00487538 */
int gCreateDIBSectionProc;  /* 0x0048753c */
#ifdef _MSC_VER
int DAT_00487540_r[514];  /* 0x00487540 */
#else
PAL16ENTRY gPal16[256];  /* 0x0048754a */
#endif
int gWinGCreateDCProc;  /* 0x0048814c */
HMENU gMainMenu;  /* 0x00488150 */
char gOutOfMemProbeSizeFmt[40];  /* 0x004881d8 */
HINSTANCE gInstance;  /* 0x00488264 */
char gTempFontPath[260];  /* 0x00488268 */
int gViewportCount;  /* 0x0048836c */
#ifdef _MSC_VER
int gViewportWasVisible[16];  /* 0x00488370 */
#else
int gViewportWasVisible[32];  /* 0x00488370 */
#endif
void *gWinGBitBltProc;  /* 0x004883b0 */
int gIsWin31;  /* 0x004883b4 */
int gSound16BitFlag;  /* 0x004883b8 */
SIZE_T gProbedUsableBytes;  /* 0x004883bc */
DWORD gAvailPageFileBytes;  /* 0x004883c0 */
DWORD gPoolMaxBytes;  /* 0x004883c4 */
int *gThePort;  /* 0x004883c8 */
short gGammaBaseG[256];  /* 0x004883d0 */
int gBlackPen;  /* 0x004885d0 */
int gCustomPaletteFlag;  /* 0x004885d4 */
int gBlackBrush;  /* 0x004885d8 */
void *gSetDIBColorTableProc;  /* 0x004885dc */
int gIsWin95;  /* 0x004885e0 */
#ifdef _MSC_VER
SavedWndPos gSavedWindowPositions[256];  /* 0x004885e8 */
#else
SavedWndPos gSavedWindowPositions[255];  /* 0x004885e8 */
#endif
short gGammaBaseR[256];  /* 0x004899e8 */
char gOpenFileDefExt[32];  /* 0x00489be8 */
int gWinGCreateBitmapProc;  /* 0x00489c08 */
COLORREF gSavedSysColors[25];  /* 0x00489c10 */
short gAttachSlots[124];  /* 0x00489c78 */
int gAttachWaitCount;  /* 0x00489d70 */
CRITICAL_SECTION gAttachLock;  /* 0x00489d78 */
void *gAttachEvent;  /* 0x00489d90 */
short DAT_00489d94;  /* 0x00489d94 */
WIN32_FIND_DATAA gFindData;  /* 0x00489da0 */
#ifdef _MSC_VER
void * gAttachWaitList[4];  /* 0x00489ee0 */
#else
void * gAttachWaitList[32];  /* 0x00489ee0 */
#endif
int gSound16Bit;  /* 0x00489ef0 */
int gSoundSlots[104];  /* 0x00489ef8 */
int gSoundStereo;  /* 0x0048a098 */
int gScriptHeap[12];  /* 0x0048a0a0 */
int gSceneChannelRefCount;  /* 0x0048a0d0 */
int gMethodCacheCapacity;  /* 0x0048a0d4 */
void *gSearchPathTable;  /* 0x0048a0d8 */
byte gLatchedMethodFrame[78];  /* 0x0048a0e0 */
short gBootScriptRan;  /* 0x0048a130 */
short gMethodFrameLatched;  /* 0x0048a134 */
int gMethodCacheCount;  /* 0x0048a138 */
int gMethodCacheClock;  /* 0x0048a13c */
void *gMethodCache;  /* 0x0048a140 */
byte gScratchStr[256];  /* 0x0048a148 */
HotspotBuf gHotspotBuf;  /* 0x0048a248 */
short gScrollRect[4];  /* 0x0048a2c0 */
void *gCursorAlt;  /* 0x0048a2c8 */
byte gSavedHotspotBuf[128];  /* 0x0048a2d0 */
SceneViewport gSceneViewport;  /* 0x0048a350 */
byte gCurrentCdName[256];  /* 0x0048a358 */
byte gScratchStr2[256];  /* 0x0048a458 */
int gTrigAnimBufSize;  /* 0x0048a874 */
HCURSOR gCursor;  /* 0x0048a878 */
short gClipRect[4];  /* 0x0048a880 */
void *gTrigAnimBuf;  /* 0x0048a888 */
byte gScriptResult[256];  /* 0x0048a890 */
short gGameLoopState;  /* 0x0048a990 */
byte *gEditVarValue;  /* 0x0048a994 */
byte *gEditVarRecord;  /* 0x0048a998 */
byte *gEditVarText;  /* 0x0048a99c */
int DAT_0048a9a0;  /* 0x0048a9a0 */
int DAT_0048a9a4;  /* 0x0048a9a4 */
int gCompactSegCount;  /* 0x0048a9a8 */
int gPrimaryHeap[12];  /* 0x0048a9b0 */
int gPartitionSwapTemp[4];  /* 0x0048a9e0 */
void *gCompactWorkBuf;  /* 0x0048a9f0 */
int DAT_0048a9f4;  /* 0x0048a9f4 */
int DAT_0048a9f8;  /* 0x0048a9f8 */
int DAT_0048a9fc;  /* 0x0048a9fc */
short gClutReady;  /* 0x0048aa00 */
void *gActiveClut;  /* 0x0048aa08 */
unsigned int gRandTable[55];  /* 0x0048aa10 */
short gRandIndex;  /* 0x0048aaec */
void *gSecondaryClut;  /* 0x0048aaf4 */
void *gPrimaryClut;  /* 0x0048ab00 */
void *gPluginDirectory;  /* 0x0048ab04 */
int gPluginDirectoryCount;  /* 0x0048ab08 */
int **gCurrentParentData;  /* 0x0048ab10 */
int *gCurrentChildListData;  /* 0x0048ab14 */
CRITICAL_SECTION gScanLock;  /* 0x0048ab18 */
int gScanLockBusy;  /* 0x0048ab30 */
int gAnimChannelRefCount;  /* 0x0048ab38 */
byte gTextEntryBuf[256];  /* 0x0048ab40 */
byte gMessageStr[256];  /* 0x0048ac40 */
int gActiveTriggerSel;  /* 0x0048ad40 */
HotspotBuf gTrigHotspotCopy;  /* 0x0048ad48 */
short gTriggerOverlayValid;  /* 0x0048adc0 */
void *gTransitionRects;  /* 0x0048adc4 */
int gSinTableRes;  /* 0x0048adc8 */
HotspotBuf gTrigBufScene;  /* 0x0048add0 */
short gTriggerState;  /* 0x0048ae48 */
short gViewCos;  /* 0x0048ae4c */
short gOverlayRequest;  /* 0x0048ae50 */
int gTriggerFrame;  /* 0x0048ae54 */
int gCosTable;  /* 0x0048ae58 */
short gViewSin;  /* 0x0048ae5c */
short gTransitionRectCount;  /* 0x0048ae60 */
SceneViewport gTrigViewport;  /* 0x0048ae68 */
void *gCalcRows;  /* 0x0048ae70 */
int gCosTableRes;  /* 0x0048ae74 */
HotspotBuf gTrigBufOverlay;  /* 0x0048ae78 */
int gSinTable;  /* 0x0048aef0 */
short gCalcRowCount;  /* 0x0048aef4 */
int gLastTickTime;  /* 0x0048aef8 */
short gQueuedVoiceIds[4];  /* 0x0048af00 */
short gTrackInterrupted;  /* 0x0048b420 */
short gQueuedVoiceCount;  /* 0x0048b424 */
int gBevelTableSnapshot[326];  /* 0x0048b428 */
byte gWalkTable[1760];  /* 0x0048b970 */
byte gCricketTable[1184];  /* 0x0048c050 */
byte gLoopTable[1344];  /* 0x0048c4f0 */
int gObjectUpdateCount;  /* 0x0048ca30 */
#ifdef _MSC_VER
int gWhileStack[20];  /* 0x0048ca38 */
#else
int gWhileStack[32];  /* 0x0048ca38 */
#endif
byte gForStack[280];  /* 0x0048ca88 */
int DAT_0048cb;  /* 0x0048cb */
short gForDepth;  /* 0x0048cba0 */
short gWhileDepth;  /* 0x0048cba4 */
short gMovieNavDepth;  /* 0x0048cba8 */
int gMoviePollTick;  /* 0x0048cbac */
int gMovieEntryFrame;  /* 0x0048cbb0 */
int gMovieLastFrame;  /* 0x0048cbb4 */
int gMovieVoiceEndTick;  /* 0x0048cbb8 */
int *gMovieTemplateData;  /* 0x0048cbbc */
int gMovieNode;  /* 0x0048cbc0 */
int gMovieVoiceRows;  /* 0x0048cbc4 */
short gMovieCellSize;  /* 0x0048cbc8 */
short gMovieSoundPhase;  /* 0x0048cbcc */
short gMovieClutDirty;  /* 0x0048cbd0 */
byte *gMovieFrame;  /* 0x0048cbd4 */
int gMovieVoiceRowCount;  /* 0x0048cbd8 */
int gMovieFrameDueTick;  /* 0x0048cbdc */
int gMovieStartTick;  /* 0x0048cbe0 */
byte gMovieName[16];  /* 0x0048cbe8 */
int *gMovieHeader;  /* 0x0048cbf8 */
int gMovieChannel;  /* 0x0048cbfc */
int gMovieChannelRows;  /* 0x0048cc00 */
int gMovieTemplateRows;  /* 0x0048cc04 */
short gMovieStallCount;  /* 0x0048cc08 */
byte gMovieHotspotBuf[120];  /* 0x0048cc10 */
byte gMovieSavedHotspotBuf[128];  /* 0x0048cc88 */
int DAT_0048cd;  /* 0x0048cd */
int gMovieFrameIndex;  /* 0x0048cd08 */
int gMovieTemplateCount;  /* 0x0048cd0c */
short gMovieVoiceActive;  /* 0x0048cd10 */
void *gMovieNavHistory;  /* 0x0048cd14 */
int gMovieChildBase;  /* 0x0048cd18 */
byte gMovieVoiceName[16];  /* 0x0048cd20 */
short gMovieSoundArmed;  /* 0x0048cd30 */
short gMovieNavState;  /* 0x0048cd34 */
int gMovieTimedJumps;  /* 0x0048cd38 */
void *gMovieClut;  /* 0x0048cd3c */
byte gMovieVoiceRow[104];  /* 0x0048cd40 */
int gMovieVoiceTable;  /* 0x0048cda8 */
int gMovieNextFrame;  /* 0x0048cdac */
short gMovieRenderPhase;  /* 0x0048cdb0 */
void *gGlobalsNode;  /* 0x0048cdb8 */

#ifdef _MSC_VER
BITMAPINFOHEADER gBitmapHeader_r[1];  /* 0x00486e80 */
RGBQUAD gDibColorTable[256];  /* 0x00486ea8 */
#else
/* 0x00486e80..0x004872a8: copied as one image by the engine; keep it byte-exact. 16-aligned to preserve the original mod-16 phase. */
#ifdef __GNUC__
byte g_hotspotBitmapInfo[0x428] __attribute__((aligned(16)));
#else
byte g_hotspotBitmapInfo[0x428];
#endif
#endif

/* 0x0048a560..0x0048a872: copied as one image by the engine; keep it byte-exact. 16-aligned to preserve the original mod-16 phase. */
#ifdef __GNUC__
byte g_saveStateImage[0x312] __attribute__((aligned(16)));
#else
byte g_saveStateImage[0x312];  /* 0x0048a560 */
#endif

#ifdef _MSC_VER
short gBevelCount_r[652];  /* 0x0048af08 */
#else
/* 0x0048af08..0x0048b420: copied as one image by the engine; keep it byte-exact. 16-aligned to preserve the original mod-16 phase. */
#ifdef __GNUC__
byte g_textEntryTable[0x518] __attribute__((aligned(16)));
#else
byte g_textEntryTable[0x518];
#endif
#endif

#ifdef _MSC_VER
int gLogPal_r[257];  /* 0x00487d48 */
#else
/* 0x00487d48..0x0048814c: copied as one image by the engine; keep it byte-exact. 16-aligned to preserve the original mod-16 phase. */
#ifdef __GNUC__
byte g_logPalImage[0x404] __attribute__((aligned(16)));
#else
byte g_logPalImage[0x404];
#endif
#endif
