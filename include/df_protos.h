/* Prototypes of every non-static function, taken from its definition, sorted by address.
 * A new function's prototype goes here by hand, in its place. */
#ifndef DF_PROTOS_H
#define DF_PROTOS_H

#include "df_port_types.h"

void __cdecl MixChannels8x1(unsigned char *src, unsigned char *dst, int n);  /* 0x00401000 */
void __cdecl MixChannels8x2(unsigned char *src, unsigned char *dst, int n);  /* 0x00401020 */
void __cdecl MixChannels8x3(unsigned char *src, unsigned char *dst, int n);  /* 0x00401050 */
void __cdecl MixChannels8x4(unsigned char *src, unsigned char *dst, int n);  /* 0x00401080 */
void __cdecl MixChannels16x1(short *src, short *dst, int n);  /* 0x004010c0 */
void __cdecl MixChannels16x2(short *src, short *dst, int n);  /* 0x004010f0 */
void __cdecl MixChannels16x3(short *src, short *dst, int n);  /* 0x00401120 */
void __cdecl MixChannels16x4(short *src, short *dst, int n);  /* 0x00401160 */
int __cdecl OpenSoundDevice(int nBuffers, int nChannels, int sampleRate, int bytesPerSample);  /* 0x004011a0 */
void __cdecl AllocInitWaveOutHeaderForBufferIdx(int bufIdx);  /* 0x004013c0 */
uint32_t __cdecl InitWaveOutHdrPayload(uint32_t *pPayload, int *pFormat);  /* 0x00401420 */
unsigned int __stdcall CloseSoundDevice(void);  /* 0x00401720 */
void __cdecl FreeWaveOutHeaderChainForBufferIdx(int bufIdx);  /* 0x004017d0 */
void __cdecl FreeWaveOutHdrDecodeSlots(int *pHdrPlus4);  /* 0x00401810 */
unsigned int __cdecl LoadWaveDataOntoSoundChannel(int bufIdx, void *pWaveData);  /* 0x00401860 */
void __cdecl BindWaveFormatToSoundChannel(int bufIdx, void *pFmtIn, int isFirst);  /* 0x004018c0 */
void __cdecl ClearSoundChannelPendingDescriptors(int bufIdx);  /* 0x00401930 */
int __cdecl QueueWaveDataOntoSoundChannel(int ch, uint32_t *pData, int isFirst);  /* 0x00401970 */
void __cdecl QueueWaveFormatOnSoundChannel(int ch, uint32_t *pData, uint32_t isFirst);  /* 0x004019c0 */
unsigned int __cdecl SilenceSoundChannel(int bufIdx);  /* 0x00401a50 */
int __cdecl IsSoundChannelIdle(int bufIdx);  /* 0x00401aa0 */
int __cdecl QuerySoundChannelIdle(int bufIdx);  /* 0x00401ab0 */
int __cdecl QueryWaveHdrQueuedByteDelta(int *pHdrPlus4);  /* 0x00401b70 */
void __cdecl DrainWaveBufferToMixBudget(int bufIdx);  /* 0x00401b80 */
void __cdecl AdjustWaveHdrWriteCursor(int *pHdr, int delta);  /* 0x00401c80 */
void __cdecl UpdateWaveHdrReadPosition(int *pBody, int position);  /* 0x00401ca0 */
unsigned int __cdecl SetSoundChannelLoopGateFlags(int bufIdx, int enable);  /* 0x00401d40 */
unsigned int __cdecl SetSoundChannelGain(int bufIdx, void *pGain);  /* 0x00401d90 */
void __cdecl ApplySoundChannelGain(int bufIdx, void *pExtra);  /* 0x00401db0 */
void __cdecl GetWaveOutHdrBaseGain(int *pQueueHeadPlus4, int *pScratch16);  /* 0x00401e20 */
int TryResolveWaveMixSlotForVoiceSource(int sourceId, int *pOutSlot);  /* 0x00401eb0 */
int __cdecl MatchSoundNodeChannelKey(int *pNode, int key);  /* 0x00401f90 */
unsigned int __cdecl SetSoundMixerSilenced(unsigned int silenced);  /* 0x00401fd0 */
int __cdecl PackVolumePairToGainTriple(int *pTriple, double xs, double ys);  /* 0x00401ff0 */
int __cdecl MultiplyGainTriples(GainTriple *pOut, GainTriple *pFmt, GainTriple *pScr);  /* 0x00402120 */
unsigned int __cdecl RegisterSoundCodec(int *pDesc);  /* 0x004022f0 */
int __stdcall LockSoundMixer(void);  /* 0x00402350 */
int __cdecl UnlockSoundMixer(unsigned int pos);  /* 0x00402370 */
void __cdecl PumpSoundMixer(uint flushRequest);  /* 0x004023f0 */
void __cdecl FillWaveSilence(void *pDst, int nFrames);  /* 0x004027c0 */
void __cdecl ConsumePlayedFramesFromWaveHdrQueue(int bufIdx, int byteBudget);  /* 0x00402840 */
void __cdecl RecycleOrTeardownWaveHdrChain(int bufIdx);  /* 0x004029f0 */
void __cdecl ResetWaveHdrChannelPendingCount(int pChannel);  /* 0x00402a90 */
void __cdecl SetWaveHdrRecycleFlag(int bufIdx, int flag);  /* 0x00402aa0 */
void __cdecl AdvanceWaveMixTick(int targetFrames);  /* 0x00402af0 */
void __cdecl RenderWaveHdrQueueToMixBuffer(char *pSrc, int bufIdx, int nFrames);  /* 0x00402c30 */
int __cdecl RenderWaveHdrFrames(uint32_t *pHdr, char *pSrc, int nFrames, uint32_t bIsHead, int *pAnchor);  /* 0x00402d70 */
void __cdecl DecodeMediaFrameSegmentDirect(int *pState, int iPos, int nFrames, char *pDst, int *pAnchor);  /* 0x00402e30 */
void __cdecl ConvertAudioBlockToOutputFormat(byte *param_1,uint32_t param_2,int param_3,int param_4,char *param_5, uint32_t param_6,int param_7,int param_8,int param_9,int *param_10,uint32_t param_11 );  /* 0x00402ef0 */
void __cdecl DecodeMediaFrameSegmentBuffered(int *pState, int iPos, int nFrames, char *pDst, int *pAnchor);  /* 0x004053e0 */
void __cdecl RewindWaveHdrNode(uint32_t *pHdr);  /* 0x004055a0 */
void __cdecl RenderSoundChannelToMixRing(int bufIdx, int framesPending);  /* 0x004055e0 */
void __cdecl CopyMixBufferSegment(void *pMixWriteCursor, int nFrames);  /* 0x00405650 */
void __cdecl AppendPendingDescriptorsToWaveHdrQueue(int bufIdx);  /* 0x00405710 */
void __cdecl AppendWaveOutHdrEntryToQueue(int bufIdx, int *pPayload);  /* 0x00405780 */
void __stdcall ClearAllSoundChannelPendingDescriptors(void);  /* 0x00405810 */
void __stdcall ServiceSoundMixer(void);  /* 0x00405840 */
void __cdecl InvokeWaveDeviceSetVolume(int level);  /* 0x00405860 */
void __cdecl InvokeWaveDeviceQueryVolume(void *pOut);  /* 0x00405880 */
void __stdcall AllocScriptStringSlots(void);  /* 0x004058a0 */
void __stdcall FreeScriptStringSlots(void);  /* 0x004058d0 */
void __stdcall ResetScriptStringSlots(void);  /* 0x004058e0 */
short __cdecl AllocScriptString(byte *pDesc, short *pResult);  /* 0x00405900 */
short __cdecl TakeScriptString(short *pChannel, byte *pOut);  /* 0x00405960 */
short __cdecl ExtractInstrIdentifier(short *pInstr, byte *pOut);  /* 0x004059e0 */
short __cdecl ProcessScriptBlock(char *pCtx, short *pInstr, int *pTable, int *pResultOut, int *pCountOut);  /* 0x00405a10 */
short __cdecl ResolveQueuedOperation(short queueIdx, short opcode);  /* 0x00405c50 */
short __cdecl GetInstructionPriority(short opcode);  /* 0x00406150 */
short __cdecl EvalScriptTerm(char *pCtx, short *pInstr, int *pTable, short *pResult, int *pSkipCount);  /* 0x004061e0 */
short __cdecl ExtractInstrString(short *pInstr, byte *pOut);  /* 0x004065f0 */
int __cdecl OpenWaveDevice(int **ppDevice, int sampleRate, int bytesPerSample, int nChannels, int *pWaveFmt, int *pBufSize, unsigned int *pExtra);  /* 0x00406620 */
int __cdecl ClassifyWaveOutDeviceVersionCompat(void);  /* 0x004066a0 */
void __cdecl InitWaveOutMixerVolumeControl(HWAVEOUT hwo);  /* 0x00406af0 */
undefined4 __stdcall InitializeMixerControlAPIs(void);  /* 0x00406cb0 */
void __stdcall UnloadWinmm(void);  /* 0x00406dc0 */
void __cdecl ReportMMError(MMRESULT mmResult, int tag);  /* 0x00406dd0 */
uint32_t __cdecl DequeueWaveOutBuffer(uint32_t *pHdrOut, int markReset);  /* 0x00406f50 */
uint32_t __stdcall SubmitWaveOutBuffer(void);  /* 0x00406fe0 */
void __cdecl ProcessWaveOutMessages(void);  /* 0x004070d0 */
uint32_t __stdcall WaveOutDispatchNoOp(void);  /* 0x00407130 */
unsigned int __cdecl GetWaveOutQueuedFrames(int *pQueuedFrames);  /* 0x00407140 */
unsigned int __cdecl QueryWaveOutVolumeLinear9(unsigned int *pLevel);  /* 0x00407220 */
unsigned int __cdecl QueryMixerVolumeLevel9(unsigned int *pLevelOut);  /* 0x004072d0 */
int __cdecl SetWaveOutVolumeLevel(int level);  /* 0x00407360 */
int __cdecl SetMixerVolumeLevel9(int level);  /* 0x004073f0 */
unsigned int __cdecl CloseWaveDevice(int hDevice);  /* 0x00407450 */
int __stdcall ReleaseDirectSound(void);  /* 0x00407480 */
uint32_t __stdcall CloseDirectSoundBuffer(void);  /* 0x004074a0 */
void __cdecl CloseMixerVolumeControl(void);  /* 0x004074f0 */
undefined4 __stdcall CleanupAudioTimerEvent(void);  /* 0x00407520 */
short __cdecl ActionCmd_OpenSetFile(char *pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00407620 */
short __cdecl OpenSetNode(byte *pDesc, short *pState, byte *pOpt1, byte *pOpt2);  /* 0x00407780 */
short __cdecl ActionCmd_CloseSetFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00407ab0 */
void __cdecl CloseSetNode(short *pState);  /* 0x00407b30 */
short __cdecl ActionCmd_CurrentScene(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00407c00 */
short __stdcall SendOpenSceneMessage(void);  /* 0x00407e50 */
short __cdecl SendCloseSceneMessage(void);  /* 0x00407ec0 */
short __stdcall CompleteSetInit(void);  /* 0x00407f30 */
short __stdcall SetPreShutdownStep(void);  /* 0x00407fe0 */
short __cdecl RunSceneMessage(byte *pDesc);  /* 0x00408090 */
short __cdecl ActionCmd_SendToScene(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00408170 */
short __cdecl ScriptCmd_SendToSceneFx(char *pCtx, int pArgBlock, int *pTable, void *pResult, int *pSubSkip);  /* 0x00408190 */
short __cdecl RunSetMessage(byte *pDesc);  /* 0x004084b0 */
short __cdecl ActionCmd_SendToSet(int pCtx, short *pArgBlock, int *pTable, int *pSkipOut);  /* 0x00408590 */
undefined2 __cdecl ScriptCmd_SendToSetFx(void *pCtxB, short *pInstr, int *pTable, void *pResult, int *pSkipOut);  /* 0x004085b0 */
short __cdecl ActionCmd_SetScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00408770 */
short __cdecl ActionCmd_SceneScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x004087e0 */
short __cdecl ActionCmd_SetVisible(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00408860 */
short __cdecl ActionCmd_CurrentView(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x004088d0 */
short __cdecl ActionCmd_CurrentDeg(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00408a20 */
short __cdecl ScriptCmd_RoadAhead(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00408b60 */
short ScriptCmd_SetVisible(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00408c30 */
short ScriptCmd_CurrentView(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00408c70 */
short ScriptCmd_CurrentDeg(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00408ce0 */
short __cdecl ScriptCmd_CurrentSet(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00408d20 */
short __cdecl ScriptCmd_PointInSet(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00408d90 */
short ScriptCmd_CurrentScene(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00408e50 */
short __cdecl ScriptCmd_CountScenes(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00408e90 */
short __cdecl ScriptCmd_IndexToScene(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00408ed0 */
short __cdecl ScriptCmd_CountPaintings(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00408f90 */
short __cdecl ScriptCmd_IndexToPainting(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004090d0 */
short ScriptCmd_PointInPainting(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00409290 */
short __cdecl ActionCmd_PaintingScript(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x004093c0 */
short __cdecl ActionCmd_SendToPainting(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00409500 */
short __cdecl ScriptCmd_SendToPaintingFx(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSubSkip);  /* 0x00409520 */
ushort __cdecl FindPaintingAtPoint(int packedPt, byte *pDescOut);  /* 0x004099a0 */
short __cdecl ScriptCmd_CountViews(int pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00409ac0 */
short __cdecl ScriptCmd_IndexToView(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00409b90 */
short __cdecl ActionCmd_SetParam(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00409d20 */
short ScriptCmd_SetParam(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00409dd0 */
short __cdecl EvalSceneRowArg(int pCtx, short *pArgBlock, int *pTable, int *pSkipCount, void *pRowOut);  /* 0x00409e70 */
short __cdecl FindSceneRowByName(byte *pDesc, void *pOut);  /* 0x00409ee0 */
short __cdecl FindWaypointNodePosition(byte *pName, short *pPosOut);  /* 0x00409f70 */
short __cdecl FindWaypointPathBetween(byte *pName1, byte *pName2, int **ppHandleOut);  /* 0x0040a060 */
short __cdecl FindWaypointPathToNode(byte *pName, int namedEntry, int **ppHandleOut);  /* 0x0040a180 */
void __cdecl RotateWaypointPathToNearestEntry(int pPos, int *hPath);  /* 0x0040a290 */
uint32_t __cdecl CloneRenderChildResource(uint32_t selector);  /* 0x0040a470 */
void __cdecl ReverseWaypointPathDirection(int *hPath);  /* 0x0040a4e0 */
void __cdecl BuildAnimWaypointPath(uint32_t selector, int **phPath);  /* 0x0040a5b0 */
uint __cdecl ResolveWalkPathSelectorForView(byte *pName1, byte *pName2);  /* 0x0040a760 */
void __stdcall SelectViewByNameOrNearestAngle(void);  /* 0x0040a910 */
short __cdecl SceneHasViewNamed(byte *pA, byte *pB);  /* 0x0040aae0 */
short __cdecl FindViewNameNearestAngle(short kind, byte *pOutBuf);  /* 0x0040abc0 */
short __cdecl LookupViewRow(byte *pDescA, byte *pDescB, void *pOutRow46);  /* 0x0040acc0 */
short __cdecl LookupNamedRowInChild(uint32_t selector, byte *pOut, int *pRowOut);  /* 0x0040add0 */
short __cdecl ActionCmd_NetConnect(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040aec0 */
short __cdecl ActionCmd_NetDisconnect(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040b060 */
short __cdecl ActionCmd_NetBroadcast(char *pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040b0b0 */
short __cdecl ActionCmd_NetJoinGroup(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040b270 */
short __cdecl ActionCmd_NetLeaveGroup(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040b2f0 */
short NetPollEvents(void);  /* 0x0040b370 */
short __stdcall NetPollNotification(void);  /* 0x0040b390 */
short __stdcall NetPollMessage(void);  /* 0x0040b610 */
short __cdecl ScriptCmd_NetworkOn(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040b660 */
short __cdecl ScriptCmd_NetCountMembers(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040b690 */
short __cdecl ScriptCmd_NetIndexToMember(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040b730 */
short __cdecl ScriptCmd_NetOurId(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040b7c0 */
short __cdecl ScriptCmd_NetMessageSender(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040b800 */
short __cdecl ScriptCmd_NetMessageGroup(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040b860 */
short __cdecl NetPluginCallStr(byte *pStr, uint *pOut);  /* 0x0040b8d0 */
short __cdecl NetPluginCallStrGetStr(byte *pCmd, byte *pResultBuf);  /* 0x0040b940 */
short __cdecl NetPluginCallStrInt(byte *pStr, int idx, uint *pOut);  /* 0x0040b9e0 */
short __cdecl NetPluginCallStrStr(byte *pNameA, byte *pNameB, uint32_t *pOut);  /* 0x0040ba60 */
short __cdecl NetPluginCallStrStrStr(byte *pNameA, byte *pNameB, byte *pNameC, uint32_t *pOut);  /* 0x0040bae0 */
void __cdecl DebuggerChannelHandler(int pChannel, short msg, int *pParam);  /* 0x0040bb60 */
void __cdecl DispatchDebuggerKey(int pChannel, short cmd);  /* 0x0040beb0 */
void __cdecl HandleDebuggerClick(int pChannel, ScenePointYX param);  /* 0x0040c240 */
void __cdecl DrawDebuggerButtons(int pChannel);  /* 0x0040c660 */
void __cdecl InvalidateDebuggerListing(short param);  /* 0x0040c910 */
void __stdcall DrawDebuggerListing(void);  /* 0x0040c960 */
void __cdecl DrawButton(byte *pLabel, short *pRect);  /* 0x0040cac0 */
void __cdecl ComputeButtonRect(short mode, short phase, short *pRect);  /* 0x0040cb50 */
void __stdcall ShutdownDebugger(void);  /* 0x0040cba0 */
void __stdcall InitDebugger(void);  /* 0x0040cbd0 */
void __stdcall ResetDebuggerState(void);  /* 0x0040cd10 */
void __stdcall RebuildDebuggerListing(void);  /* 0x0040cd50 */
void __stdcall ScrollDebuggerListingToCaret(void);  /* 0x0040cd90 */
int __cdecl CountDebuggerListingLines(void);  /* 0x0040cde0 */
int __cdecl GetDebuggerListingLineAtOffset(int byteOffset);  /* 0x0040ce30 */
int __cdecl GetDebuggerListingLineOffset(int lineIdx);  /* 0x0040cea0 */
int __cdecl GetScriptTokenIndexForLine(int lineIdx);  /* 0x0040cf20 */
void __cdecl ListNodePropertiesToDebugger(int *pHandle);  /* 0x0040cf60 */
void __stdcall ListScriptSourceToDebugger(void);  /* 0x0040d130 */
void __cdecl AppendToDebuggerListing(byte *pStr);  /* 0x0040d1c0 */
short __cdecl IsDebuggerLineMarked(int rowOffset);  /* 0x0040d220 */
short __cdecl ClearBreakpointAtLine(int lineIdx);  /* 0x0040d380 */
void __cdecl RemoveBreakpoint(int bpIdx);  /* 0x0040d3d0 */
int __cdecl FindBreakpoint(int pDataBase, int pSlotByte, ushort *pOut);  /* 0x0040d450 */
ushort __cdecl TrackButtonPress(short *pRect);  /* 0x0040d4e0 */
void __cdecl EditDebuggerVariable(int *pScopeNode, int lineIdx);  /* 0x0040d570 */
short __cdecl ActionCmd_OpenCastFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040d9a0 */
short __cdecl OpenCastNode(byte *pDesc, int *pRow);  /* 0x0040db50 */
void __cdecl InitActorEntry(int *pNodeRef, uint32_t selector, short *pActor);  /* 0x0040dc90 */
short __cdecl ActionCmd_ActorInstance(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040de80 */
short __cdecl ActionCmd_ActorDelete(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040e010 */
short __cdecl ActionCmd_ActorSet(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040e150 */
short ScriptCmd_ActorSet(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040e280 */
short __cdecl ScriptCmd_ActorExists(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040e300 */
short __cdecl ActionCmd_ActorStar(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040e390 */
short ScriptCmd_ActorStar(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040e4d0 */
short __cdecl ActionCmd_ActorPose(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040e550 */
short ScriptCmd_ActorPose(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040e650 */
short __cdecl ActionCmd_ActorDeg(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040e6d0 */
short __cdecl ActionCmd_ActorDist(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040e770 */
short ScriptCmd_ActorDist(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040e820 */
short ScriptCmd_ActorDeg(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040e8e0 */
short __cdecl ActionCmd_ActorSpeed(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040e970 */
short __cdecl ActionCmd_ActorScale(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040ea20 */
short ScriptCmd_ActorScale(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040ead0 */
short ScriptCmd_ActorSpeed(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040eb60 */
short __cdecl ActionCmd_ActorWarm(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040ebf0 */
short __cdecl ActionCmd_CastWarm(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040ec70 */
short __cdecl ActionCmd_ActorLock(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040ed20 */
short __cdecl ActionCmd_CastLock(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040edd0 */
short __cdecl ActionCmd_ActorVisible(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040eeb0 */
short ScriptCmd_ActorVisible(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040ef50 */
short __cdecl ActionCmd_ActorHide(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040eff0 */
short __cdecl ActionCmd_ActorXyz(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040f140 */
short ScriptCmd_ActorXyz(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040f260 */
short __cdecl ScriptCmd_PointInActor(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040f380 */
short __cdecl ActionCmd_ActorXy(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040f510 */
short ScriptCmd_ActorXy(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040f600 */
short __cdecl ActionCmd_ActorIs3d(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040f6f0 */
short ScriptCmd_ActorIs3d(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0040f7a0 */
short __cdecl CompleteCastOpen(int pNode);  /* 0x0040f830 */
short __cdecl ActionCmd_CloseCastFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040f990 */
void __cdecl TeardownActorEntry(int pEntry);  /* 0x0040fba0 */
void __cdecl CloseCastNode(int *pNodeRef);  /* 0x0040fc00 */
short __cdecl CastPreShutdownStep(int pNode);  /* 0x0040fc50 */
short __cdecl ActionCmd_ActorScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040fdd0 */
short __cdecl ActionCmd_CastScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0040fe80 */
short __cdecl RunActorMessage(byte *pPascalDesc);  /* 0x0040ff30 */
short __cdecl ActionCmd_SendToActor(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00410010 */
short __cdecl ScriptCmd_SendToActorFx(int *pCtx, int pArgBlock, int *pTable, void *pResult, int *pSubSkip);  /* 0x00410030 */
short __cdecl RunCastMessage(byte *pCmd);  /* 0x00410360 */
short __cdecl ActionCmd_SendToCast(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00410440 */
short __cdecl ScriptCmd_SendToCastFx(int *pCtx, int pArgBlock, int *pTable, void *pResult, int *pSubSkip);  /* 0x00410460 */
short ScriptCmd_CountActors(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004106a0 */
short __cdecl ScriptCmd_CountCasts(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004106d0 */
short ScriptCmd_IndexToActor(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00410700 */
short __cdecl ScriptCmd_IndexToCast(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00410800 */
short __cdecl ActionCmd_ActorTurn(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x004108b0 */
short ScriptCmd_ActorTurn(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00410960 */
short __cdecl ActionCmd_ActorOwner(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x004109f0 */
short __cdecl ActionCmd_ActorValue(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00410ab0 */
short __cdecl ActionCmd_ActorZclip(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00410b50 */
short ScriptCmd_ActorOwner(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00410bf0 */
short ScriptCmd_ActorValue(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00410c70 */
short ScriptCmd_ActorZclip(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00410d00 */
short LookupActorEntry(byte *pName, int *pDataOut);  /* 0x00410d90 */
void __cdecl WriteBackActorEntry(int *pEntryData);  /* 0x00410e70 */
short __cdecl LookupCastEntryByName(byte *pName, int *pOut);  /* 0x00410ee0 */
short __cdecl FindCastEntryIndexByNode(int handle, int *pIdxOut);  /* 0x00410f60 */
short __cdecl LookupCastEntryByNode(int entryValue, int *pOut);  /* 0x00410fc0 */
void __stdcall PlaceActorsAtWaypoints(void);  /* 0x00411030 */
void __cdecl WalkActorsCalcRender(short mode);  /* 0x004110c0 */
short CalcActorRenderParams(short *pActor, short *pCalc);  /* 0x00411210 */
short __cdecl LookupSpriteFrameGeometryByAngle(int pActor, short angle, int *pFrameOut);  /* 0x00411570 */
unsigned short __cdecl ReadNamedRowChildUshort(int *pCtx, unsigned int childId);  /* 0x004116e0 */
short __cdecl SetActorFrameCountForPose(int *pNamed, byte *pSecond);  /* 0x00411760 */
short __cdecl GetActorNameByBindingKey(int bindingKey, byte *pDescOut);  /* 0x004117f0 */
void __cdecl ReattachSlot0RowRenderChildren(int *pRow);  /* 0x00411860 */
void __cdecl WarmActorRenderChildren(int *pRow);  /* 0x004119e0 */
void __cdecl UnwarmActorRenderChildren(int *pRow);  /* 0x00411b40 */
uint32_t __cdecl CloneActorFrameImage(byte *pActorName, byte *pViewName, short angle, undefined2 animIdx);  /* 0x00411cb0 */
void __stdcall InitRenderState(void);  /* 0x00411e00 */
void __stdcall ShutdownRenderState(void);  /* 0x00411e80 */
void __cdecl ComputeSpriteFrameGeometry(int *hSprite, uint32_t packedPos, short flipV, short *pRectOut);  /* 0x00411e90 */
void RasterStampChildIntoSceneHotspotBuf(int *hSprite, uint32_t packedTopLeft, int pDstDesc, short flipV);  /* 0x00411f10 */
short ProbeSpriteHotspotStripHit(int *hSprite, int packedPos, short flipV, ScenePointYX pt);  /* 0x00412140 */
short ProbePackedHotspotStrip(byte *pSliceData, short spanDim, short probeCoord);  /* 0x00412260 */
void __cdecl RasterStampChildIntoTrigSceneHotspotBuf(int *hSprite, uint32_t packedTopLeft, int pDstDesc, short flipV, short zoneBand, ScenePointYX stampSize);  /* 0x00412310 */
void __stdcall InitTrigHotspotBresenhamTables(void);  /* 0x004126a0 */
void WalkTrigHotspotRasterBandSteep(byte *pRleCursor, int scanBase);  /* 0x00412790 */
void __stdcall BuildTrigScanBandList(void);  /* 0x00412940 */
void WalkTrigHotspotRasterBandDualPlane(byte *pRleCursor, int scanBase);  /* 0x00412df0 */
void WalkTrigHotspotRasterBandShallow(byte *pRleCursor, int scanBase);  /* 0x004134e0 */
short ProbeScaledHotspotStripHit(int *hSprite, int packedPos, short flipV, ScenePointYX scale, ScenePointYX pt);  /* 0x004135d0 */
short __cdecl ProbeTriggerOverlayHotspotAtPackedPoint(short *pTrigOverlayDims, short zoneBand, ScenePointYX pt);  /* 0x00413780 */
short __cdecl ActionCmd_SaveGame(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00413830 */
short __cdecl ActionCmd_OpenGame(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x004138f0 */
void __cdecl WriteSaveGameFile(byte *containerPathDesc, byte *auxNamePascal);  /* 0x004139a0 */
short __cdecl ReadSaveGameFile(byte *pPath, byte *pExpectedName);  /* 0x00414110 */
void __stdcall RestoreSavedGameState(void);  /* 0x00414b00 */
int __cdecl ClaimPendingRenderNodeBySavedSelector(int selector, int fileType);  /* 0x00415370 */
int __cdecl FindRenderNodeByDescriptor(int selector);  /* 0x00415400 */
void __cdecl GetRenderNodeDescriptorBySelector(int selector, byte *pDescOut);  /* 0x00415480 */
void __cdecl StoreRenderNodeDescriptorBySelector(int slotIndex, byte *pOutDesc);  /* 0x00415500 */
void __stdcall StripBootPrefixFromSavedDescriptors(void);  /* 0x00415580 */
void __cdecl StripMatchingPascalPrefix(byte *pPrefix, byte *pStr);  /* 0x004155d0 */
short __cdecl ActionCmd_OpenShopFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00415650 */
short __cdecl OpenShopNode(byte *pDesc, int *pEntry);  /* 0x00415810 */
short __cdecl InitPropEntry(int *pEntry, unsigned int sel, int *pOut);  /* 0x00415950 */
short __cdecl ActionCmd_PropInstance(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00415b40 */
short __cdecl ActionCmd_PropDelete(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00415cd0 */
short __cdecl ActionCmd_PropSet(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00415e20 */
short __cdecl ActionCmd_PropOwner(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00415f40 */
short __cdecl ActionCmd_PropValue(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00416000 */
short __cdecl ActionCmd_PropZclip(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x004160a0 */
short ScriptCmd_PropSet(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00416140 */
short __cdecl ScriptCmd_PropExists(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004161c0 */
short ScriptCmd_PropOwner(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00416250 */
short ScriptCmd_PropValue(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004162d0 */
short ScriptCmd_PropZclip(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00416360 */
short __cdecl ActionCmd_PropStar(uint32_t pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x004163f0 */
short ScriptCmd_PropStar(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00416520 */
short __cdecl ActionCmd_PropView(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x004165a0 */
short ScriptCmd_PropView(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004166a0 */
short __cdecl ActionCmd_PropDeg(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00416720 */
short __cdecl ActionCmd_PropDist(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x004167c0 */
short ScriptCmd_PropDist(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00416870 */
short ScriptCmd_PropDeg(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00416930 */
short __cdecl ActionCmd_PropSpeed(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x004169c0 */
short __cdecl ActionCmd_PropScale(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00416a70 */
short ScriptCmd_PropScale(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00416b20 */
short ScriptCmd_PropSpeed(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00416bb0 */
short __cdecl ActionCmd_PropWarm(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00416c40 */
short __cdecl ActionCmd_ShopWarm(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00416cc0 */
short __cdecl ActionCmd_PropLock(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00416d70 */
short __cdecl ActionCmd_ShopLock(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00416e20 */
short __cdecl ActionCmd_PropVisible(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00416f00 */
short ScriptCmd_PropVisible(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00416fc0 */
short __cdecl ActionCmd_PropHide(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00417060 */
short __cdecl ScriptCmd_PointInProp(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004171b0 */
short __cdecl ActionCmd_PropXyz(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00417340 */
short ScriptCmd_PropXyz(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00417450 */
short __cdecl ActionCmd_PropXy(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00417570 */
short ScriptCmd_PropXy(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00417650 */
short __cdecl ActionCmd_PropIs3d(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00417740 */
short ScriptCmd_PropIs3d(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004177f0 */
short __cdecl CompleteShopOpen(int param_1);  /* 0x00417880 */
short __cdecl ActionCmd_CloseShopFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x004179e0 */
void __cdecl TeardownPropEntry(int pEntry);  /* 0x00417c00 */
void __cdecl CloseShopNode(int *pEntry);  /* 0x00417c50 */
short __cdecl ShopPreShutdownStep(int pNode);  /* 0x00417ca0 */
short __cdecl ActionCmd_PropScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00417e20 */
short __cdecl ActionCmd_ShopScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00417ed0 */
short __cdecl RunPropMessage(byte *pCmd);  /* 0x00417f80 */
short __cdecl ActionCmd_SendToProp(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00418060 */
short __cdecl ScriptCmd_SendToPropFx(int *pCtx, int pArgBlock, int *pTable, void *pResult, int *pSubSkip);  /* 0x00418080 */
short __cdecl RunShopMessage(byte *pCmd);  /* 0x004183b0 */
short __cdecl ActionCmd_SendToShop(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00418490 */
short __cdecl ScriptCmd_SendToShopFx(int *pCtx, int pArgBlock, int *pTable, void *pResult, int *pSubSkip);  /* 0x004184b0 */
short __cdecl ScriptCmd_CountProps(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004186f0 */
short __cdecl ScriptCmd_CountShops(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00418720 */
short __cdecl ScriptCmd_IndexToProp(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00418750 */
short __cdecl ScriptCmd_IndexToShop(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00418850 */
short __cdecl LookupPropEntry(byte *pName, unsigned int *pRowOut);  /* 0x00418900 */
void __cdecl WriteBackPropEntry(int *pRow);  /* 0x004189e0 */
short __cdecl LookupShopEntryByName(byte *pDesc, int *pEntryOut);  /* 0x00418a60 */
short __cdecl FindShopEntryIndexByNode(int ctxHandle, int *pIdxOut);  /* 0x00418ae0 */
short __cdecl LookupShopEntryByNode(int ctxHandle, int *pEntryOut);  /* 0x00418b40 */
void __stdcall PlacePropsAtWaypoints(void);  /* 0x00418bb0 */
void __cdecl WalkPropsCalcRender(short mode);  /* 0x00418c40 */
short CalcPropRenderParams(short *pRow, short *pCalc);  /* 0x00418d90 */
short __cdecl LookupStateSpriteFrameGeometryByAngle(int pRow, short angle, int *pFrameOut);  /* 0x004190f0 */
unsigned short __cdecl ReadChildUshortAt0x70(int *pCtx, unsigned int childId);  /* 0x00419260 */
short __cdecl SetPropFrameCountForPose(int *pState, byte *pSecondName);  /* 0x004192e0 */
short __cdecl GetPropNameByBindingKey(int bindingKey, byte *pDescOut);  /* 0x00419370 */
void __cdecl ReattachStateRowRenderChildren(int *pRow);  /* 0x004193e0 */
void __cdecl WarmPropRenderChildren(int pEntry);  /* 0x00419560 */
void __cdecl UnwarmPropRenderChildren(int pEntry);  /* 0x004196c0 */
uint32_t __cdecl ClonePropFrameImage(byte *pPropName, byte *pViewName, short angle, undefined2 animIdx);  /* 0x00419830 */
void __cdecl CloneResource(void **ppCLUT);  /* 0x00419980 */
void __cdecl ComputeAlignedRectSize(short *pRect, int *pMetricOut, unsigned short *pWidthOut);  /* 0x004199f0 */
char * __cdecl UpdatePascalLength(char *pDescriptor);  /* 0x00419a30 */
void __cdecl ZeroBlock(unsigned int *ptr, unsigned int size);  /* 0x00419a50 */
void __cdecl CommitInterpolatedCLUTPalette(int pRawData);  /* 0x00419a70 */
void __cdecl FlushDIBColorTable(HDC hdc, UINT iStart, UINT cEntries, const RGBQUAD *prgbq);  /* 0x00419ba0 */
void __stdcall PaletteUpdateFinalizer(void);  /* 0x00419bd0 */
void SetPaletteEntry16(int idx, int r, int g, int b);  /* 0x00419be0 */
void __cdecl BuildGammaTables(void);  /* 0x00419d10 */
void __cdecl SyncLogicalPalette(int animate);  /* 0x00419e00 */
void __cdecl ApplyCLUTToPalette(int pClut);  /* 0x00419e60 */
LPCSTR __cdecl GetStringResource(UINT stringId);  /* 0x00419f80 */
void __cdecl WordWrapText(char *pSrc, char *pDst);  /* 0x0041a090 */
short __stdcall InitJoystick(void);  /* 0x0041a100 */
unsigned short __stdcall PollJoystickButtons(void);  /* 0x0041a290 */
short __stdcall PollJoystickDirection(void);  /* 0x0041a2c0 */
void __cdecl EnqueueInputEvent(int isRepeat, int eventType, int buttonId, unsigned int *pPayload, int extraParam);  /* 0x0041a450 */
short __cdecl DequeueGameEvent(unsigned short typeMask, unsigned int *pEventOut);  /* 0x0041a630 */
short FindQueueEntry(unsigned short typeMask, unsigned int *pEventOut);  /* 0x0041a6e0 */
void __stdcall PollJoystickInput(void);  /* 0x0041a750 */
void __cdecl AdjustRgbGammaScales(int decAll, int tweakR, int tweakG, int tweakB);  /* 0x0041b2a0 */
void __cdecl HandleActivateAppMessage(BOOL fActive);  /* 0x0041b480 */
void __cdecl ManageSystemPalette(int acquire);  /* 0x0041b6e0 */
void __stdcall SaveSystemColors(void);  /* 0x0041b750 */
void __stdcall ApplyGameSysColors(void);  /* 0x0041b780 */
void __stdcall ApplyDisplayMode(void);  /* 0x0041b7a0 */
void __stdcall SaveWindowPositions(void);  /* 0x0041b820 */
void __stdcall RestoreDisplayMode(void);  /* 0x0041b880 */
int __cdecl ShowYesNoDialog(const char *pszTitle, LPCSTR pszMessage);  /* 0x0041b950 */
void __stdcall ShowEngineAboutDialog(void);  /* 0x0041b9b0 */
void __cdecl ShowFatalDialog(LPCSTR pszTitle, char *pszMessage);  /* 0x0041bb00 */
void __cdecl AppInit(void *pInitArgs);  /* 0x0041bb40 */
void __stdcall InitializeEngineState(void);  /* 0x0041c440 */
void __stdcall SubsystemCleanup(void);  /* 0x0041c460 */
int __stdcall InitGraphicsBackend(void);  /* 0x0041c470 */
HDC __cdecl CreateBackBufferDC(void);  /* 0x0041c580 */
void __cdecl SetPaletteEntry(int idx, unsigned short r, unsigned short g, unsigned short b);  /* 0x0041c5a0 */
void __cdecl RevertDisplayMode(const char *pszCallerMsg);  /* 0x0041c670 */
void __cdecl InitDisplayMode(int nWidth, int nHeight, unsigned int nBpp);  /* 0x0041c730 */
void __cdecl ShowInfoDialog(LPCSTR pszTitle, char *pszMessage);  /* 0x0041c930 */
int __stdcall CheckDisplayColourDepth(void);  /* 0x0041c970 */
int __cdecl ShowOKCancelBox(LPCSTR pszTitle, char *pszMessage);  /* 0x0041cb40 */
int __stdcall CheckSoundCardCapability(void);  /* 0x0041cb90 */
int __stdcall CheckWindowsVersion(void);  /* 0x0041cd90 */
BOOL __stdcall IsWindowsNT(void);  /* 0x0041cfa0 */
int __stdcall CheckFileHandles(void);  /* 0x0041d020 */
int __stdcall DetectCPUType(void);  /* 0x0041d0d0 */
void __cdecl ManageEmbeddedFont(int install);  /* 0x0041d130 */
BOOL __stdcall InitMemoryManager(void);  /* 0x0041d3f0 */
int __cdecl AllocMemoryPool(int doAllocate);  /* 0x0041d510 */
int __stdcall QueryVxDAvailMem(void);  /* 0x0041d6e0 */
int __stdcall OpenVxDDevice(void);  /* 0x0041d720 */
void * __cdecl AllocVxDBlock(unsigned int cbBytes);  /* 0x0041d970 */
INT_PTR CALLBACK OutOfMemoryDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);  /* 0x0041d9d0 */
void __stdcall Shutdown(void);  /* 0x0041dd30 */
void __stdcall FreeGraphicsLibrary(void);  /* 0x0041dea0 */
void __stdcall FreeDisplayLibrary(void);  /* 0x0041deb0 */
void __cdecl FreeVxDBlock(void *pBlock);  /* 0x0041dee0 */
unsigned int __stdcall GetTick60Hz(void);  /* 0x0041df10 */
void __cdecl SleepTicks(int nTicks);  /* 0x0041df30 */
void __cdecl InstallCursor(HCURSOR hCursor);  /* 0x0041df70 */
void __stdcall QdInitCursor(void);  /* 0x0041e050 */
void __stdcall QdShowCursor(void);  /* 0x0041e090 */
void __stdcall QdHideCursor(void);  /* 0x0041e0b0 */
void __cdecl ShowChannelWindow(int pChannel);  /* 0x0041e0d0 */
void __cdecl HideChannelWindow(int pChannel);  /* 0x0041e0f0 */
void __cdecl ResizeChannelWindow(short *pRect, int pChannel);  /* 0x0041e110 */
void __cdecl GetChannelWindowRect(int pChannel, short *pOut);  /* 0x0041e190 */
void __cdecl PackUncPathAsPascal31(char *uncPath, char *pascalOut);  /* 0x0041e220 */
void __cdecl GetResourcePath(byte *pDescriptor, byte *pszOut);  /* 0x0041e260 */
void __cdecl ResolveFilePathByVolume(char *pVolumeName, char *pszOut, int flag);  /* 0x0041e380 */
void __cdecl EnumerateDriveLettersForMounted(DWORD bufSize, char *pBuf);  /* 0x0041e600 */
void __cdecl ConvertColonToBackslash(byte *pPascalIn, byte *pOut);  /* 0x0041e6a0 */
void __cdecl ResolveColonPathToDosPath(byte *pDescriptor, byte *pszOut);  /* 0x0041e6e0 */
void __cdecl ComposeVolumeDescriptorFromPath(char *pPath, char *pOut);  /* 0x0041e800 */
void __cdecl GetVolumeLabelForPath(char *drivePath, char *labelOut);  /* 0x0041e900 */
void __stdcall PumpMessages(void);  /* 0x0041eaa0 */
SIZE_T __stdcall QueryTotalSystemMemory(void);  /* 0x0041ebe0 */
int __stdcall GetCallBudget(void);  /* 0x0041ec00 */
void __cdecl FatalError(short errCode, short errSub);  /* 0x0041ec10 */
short __stdcall IsMouseDown(void);  /* 0x0041f020 */
short __stdcall IsMouseStillDown(void);  /* 0x0041f040 */
void __cdecl GetSceneCursorPos(int sceneOverride, short *pOut);  /* 0x0041f090 */
short __cdecl PollEventQueue(short typeMask, unsigned int *pEventOut);  /* 0x0041f100 */
short __cdecl PeekEventQueue(ushort eventMask, uint pEventOut);  /* 0x0041f120 */
void __stdcall FlushEventQueue(void);  /* 0x0041f140 */
void __cdecl InitHotspotBitmapState(int *pFields);  /* 0x0041f190 */
void __cdecl CleanupHotspotBitmaps(int pBufInternal);  /* 0x0041f210 */
void __cdecl ApplyHotspotBitmapGdiState(HDC hdc, void *pState);  /* 0x0041f250 */
void * __cdecl QdSetPortOffscreen(int *pStateData);  /* 0x0041f330 */
void * __cdecl QdSetPortWindow(int hNode);  /* 0x0041f370 */
void __cdecl QdReleasePort(void *pRef);  /* 0x0041f390 */
void __cdecl PrepareSceneNodeHdcForVectorDraw(int *pPort);  /* 0x0041f400 */
void __cdecl RefreshHotspotBitmapFontIfNeeded(unsigned *pBitmapState);  /* 0x0041f600 */
void __cdecl QdApplyPendingClip(void *pNode);  /* 0x0041f6b0 */
void __cdecl InitHotspotBuffer(short *pBuf, unsigned short w, short h);  /* 0x0041f700 */
HBITMAP __cdecl AllocateHotspotPixelBuffer(HDC, BITMAPINFO *, void **);  /* 0x0041f780 */
void __cdecl DestroyHotspotBuffer(int pBuf);  /* 0x0041f7c0 */
void __cdecl InvalidateSceneRect(short *pRect);  /* 0x0041f7e0 */
void __cdecl MapScreenPointToNode(NodeDesc *pNode, short *pPoint);  /* 0x0041f860 */
void __cdecl SaveNavigationState(void *ctx);  /* 0x0041f8c0 */
void __cdecl RestoreNavigationState(void *ctx);  /* 0x0041f8d0 */
void __cdecl QdSetOrigin(int pChannel, short width, short height);  /* 0x0041f8e0 */
void __cdecl CopyHotspotBufState(unsigned int *pSrc, unsigned int *pDst);  /* 0x0041f920 */
void __cdecl TeardownSceneRefs(void *pSavedHotspot);  /* 0x0041f940 */
void __cdecl QdSetPort(int *pSceneState);  /* 0x0041f950 */
void __cdecl QdCopyBits(int *pSrcBits, int *pDstBits, short *pSrcRect, short *pDstRect);  /* 0x0041f9b0 */
void __cdecl RenderSceneDisplayBuffer(HDC hdcDest, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, int isDisplayNode, void *pPixels);  /* 0x0041fbb0 */
void __cdecl GetSceneRect(int *pSceneState, short *pOut);  /* 0x0041fd40 */
void __cdecl SetSceneNodeColorFromPalette(short idx);  /* 0x0041fdc0 */
void __cdecl QdPenSize(short size);  /* 0x0041fe10 */
void __stdcall QdPenNormal(void);  /* 0x0041fe30 */
void __cdecl QdClipRect(short *pRect);  /* 0x0041fe90 */
void __cdecl QdPenPat(int pPattern);  /* 0x0041fef0 */
void __cdecl QdPenMode(short mode);  /* 0x0041ff40 */
void __cdecl QdTextFont(short font);  /* 0x0041ff80 */
void __cdecl QdTextSize(short size);  /* 0x0041ffa0 */
void __cdecl QdMoveTo(short x, short y);  /* 0x0041ffc0 */
void __cdecl QdLineTo(short param_1, short param_2);  /* 0x0041ffe0 */
void __cdecl QdEraseRect(short *pRect);  /* 0x004200d0 */
void __cdecl QdInvertRect(short *pRect);  /* 0x00420170 */
void __cdecl QdFrameRect(short *pRect);  /* 0x004201f0 */
void __cdecl QdFrameRectStippled(short *pRect);  /* 0x00420280 */
void __cdecl QdPaintRect(short *pRect);  /* 0x004202c0 */
void __cdecl QdDrawString(byte *pPascalStr);  /* 0x004203e0 */
short __cdecl QdStringWidth(byte *pPascalStr);  /* 0x00420460 */
void __stdcall RestoreMenuBar(void);  /* 0x004204d0 */
void __stdcall HideMenuBar(void);  /* 0x00420530 */
void __stdcall SuspendGameUiForModalDialog(void);  /* 0x00420580 */
void __stdcall RestoreGameUiAfterModalDialog(void);  /* 0x00420660 */
void __cdecl CenterDialogInMainWindow(HWND hDlg);  /* 0x00420780 */
short __cdecl PickSaveFileName(char *bootRelativeOut, char *pascal31FileOut);  /* 0x00420840 */
void __stdcall InitOpenFileNameStaticOnce(void);  /* 0x00420990 */
void __cdecl SetOpenFileInitialDirToModuleDir(DWORD cbBuf, char *dirBuf);  /* 0x00420a20 */
void __cdecl ApplyOpenFileNameTitleFilterPair(char *titleFilterScratch);  /* 0x00420a80 */
int __stdcall NormalizeOpenFileSelectionAfterOk(void);  /* 0x00420b50 */
short __cdecl PickOpenFileName(char *bootRelativeOut, char *pascal31FileOut, unsigned int unusedAux, int *pAssetKindFourcc, int unusedArg5);  /* 0x00420ec0 */
INT_PTR CALLBACK TextEntryDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);  /* 0x00421040 */
short __cdecl RunTextEntryDialog(void *pDescA, void *pDescB);  /* 0x00421160 */
void ShowEngineMessageBoxFromPascalString(byte *pPascal);  /* 0x004211b0 */
short __cdecl RunDescriptorYesNoDialog(void *pDesc);  /* 0x00421220 */
void __stdcall ConvertKeywordTablesToPascal(void);  /* 0x004212a0 */
void __cdecl ConvertCStringToPascalInPlace(char *pName);  /* 0x00421310 */
short __cdecl TokenizeScriptSource(byte *pSource, int srcLen, int **phTokens, int *pErrOffset);  /* 0x00421340 */
void __cdecl CopyBytesToPascalString(undefined1 *pSrc, short count, undefined1 *pDst);  /* 0x004218b0 */
short __cdecl MPath_IsIdentBreakChar(byte *pChar);  /* 0x004218e0 */
short MPath_TwoCharLeadIn(char *p);  /* 0x00421910 */
int __cdecl FormatScriptTokensAsText(int *hTokens, int **phText, int tokenLimit);  /* 0x00421940 */
short __cdecl IsScriptDelimiterOpcode(short opcode);  /* 0x00421cc0 */
short __cdecl LookupScriptKeywordName(short keywordId, byte *pNameOut);  /* 0x00421d90 */
int __cdecl InternPascalStringInPool(uint32_t *phPool, int *pUsed, uint32_t *pCap, byte *pStr);  /* 0x00421e50 */
short __cdecl ComparePascalStringsEqual(byte *pA, byte *pB);  /* 0x00421f40 */
void __cdecl AppendEntryPairToBuffer(int **phBuf, int *pCount, int *pCap, uint32_t *pEntry);  /* 0x00421f90 */
void __cdecl AppendPascalStringToBuffer(byte *pPascal, int **phBuf, int *pLen, uint32_t *pCap);  /* 0x00421ff0 */
void __cdecl CopyPascalStringFromInlineDescriptor(void *pInlineDesc, byte *pDst);  /* 0x00422060 */
void __stdcall InitDisplayAttachSlots(void);  /* 0x00422080 */
void __stdcall ShutdownDisplayAttachSlots(void);  /* 0x00422090 */
short __cdecl ResolveDisplayAttachSlot(void *pCtx, unsigned int chanId, int **ppHandleOut);  /* 0x004220a0 */
short __cdecl ActivateDisplayAttachSlot(void *pCtx, unsigned int chanId);  /* 0x004221a0 */
void __cdecl DrainDisplayAttachSlotsForContext(int pCtx);  /* 0x00422260 */
short __cdecl BootstrapContainerFile(byte *pDescriptor, int childCount, unsigned int tagA, unsigned int tagB);  /* 0x00422310 */
short __cdecl DeleteResourceFile(byte *pName);  /* 0x004223f0 */
short __cdecl ReadContainerIntoNode(byte *pDescriptor, int *pNode);  /* 0x00422400 */
short FlushAndCloseNodeStorage(int *pNode);  /* 0x00422580 */
short __cdecl FlushContainerNodeState(int *pNode);  /* 0x004225b0 */
short QueryChildAllocSize(int *pNode, unsigned int childId, int *pSizeOut);  /* 0x00422650 */
short OpenChildNode(int *pNode, unsigned int childId, int **pHandleOut);  /* 0x004226b0 */
short WriteChildDataToNode(int *pNode, unsigned int childId, int *pHandle);  /* 0x00422740 */
short __cdecl AddChildResourceToNode(int *pNode, uint32_t *pChildId, int *hResource);  /* 0x004228f0 */
short __cdecl DeleteChildFromNode(uint32_t *pNode, uint32_t childId);  /* 0x00422a20 */
short ReadBlockHeader(int *pNode, unsigned int fileOffset, int *pOut);  /* 0x00422ad0 */
short WriteBlockHeader(int *pNode, unsigned int fileOffset, const void *pHeader);  /* 0x00422b00 */
short __cdecl AllocateChildId(int *pNode, unsigned int *pIdOut);  /* 0x00422b30 */
short __cdecl EnsureChildTableCapacity(int *pNode);  /* 0x00422bb0 */
int AllocNodeBlock(int *pNode, int alignedSize, int *pFilePosOut);  /* 0x00422cf0 */
short MoveNodeBlock(int *pNode, unsigned int srcOff, unsigned int dstOff, unsigned int alignedSize);  /* 0x00422dd0 */
short __cdecl ShiftFileRegionToHigherOffset(uint32_t *pHandle, int srcOffset, uint dstOffset, uint length);  /* 0x00422eb0 */
short __cdecl CompactNodeBlockStore(int *pNode);  /* 0x00422fa0 */
void FreeNodeBlock(int *pNode, unsigned int filePos, unsigned int alignedSize);  /* 0x004230f0 */
void CoalesceFreeBlocks(int *pNode);  /* 0x004231c0 */
void RemoveFreeBlockEntry(int *pNode, short index);  /* 0x00423250 */
short __cdecl GetChildBlockOffset(int *pNode, unsigned int childIdx, int *pFileOffsetOut);  /* 0x004232b0 */
short UpdateChildBlockOffset(int *pNode, unsigned int childIdx, int fileOffset);  /* 0x00423490 */
short __cdecl LoadOffsetPage(int *pNode, int *pSlot);  /* 0x00423660 */
short __cdecl FlushOffsetPage(int *pNode, int *pEntry);  /* 0x004236a0 */
int ComputeAlignedBlockSize(int dataSize);  /* 0x004236e0 */
short __cdecl ReadFileBlock(ContainerFileHandle *pHandle, void *pDest, unsigned int size);  /* 0x00423700 */
int __cdecl PromptDiskRetry(const char *pTitle, const char *pMessage);  /* 0x00423830 */
short __cdecl WriteToFileWithRetry(int *pCtx, const void *pSrc, DWORD cb);  /* 0x004238a0 */
short __cdecl OpenContainerFile(byte *pDescriptor, ContainerFileHandle *pHandle);  /* 0x004239b0 */
short __cdecl CloseContainerFile(void *pHandle);  /* 0x00423a40 */
short __cdecl OpenContainerFileCreateAlways(byte *pDescriptor, unsigned int unusedParam2, unsigned int unusedParam3, ContainerFileHandle *pHandle);  /* 0x00423a70 */
short __cdecl FlushNodeFileBuffers(int *pNode);  /* 0x00423ae0 */
short __cdecl SeekContainerFile(void *pHandle, unsigned int fileOffset);  /* 0x00423b30 */
short __cdecl CheckContainerFileExists(byte *pDescriptor, int  *pContainerOut, int  *pLocalOut);  /* 0x00423b70 */
short __cdecl TryDeleteResourceFile(byte *pName);  /* 0x00423bd0 */
short __cdecl GetActualFileSize(ContainerFileHandle *pHandle, DWORD *pSizeOut);  /* 0x00423c10 */
short __cdecl ProbeContainerFile(byte            *pDescriptor, short           *pOpenError, int             *pContainerOut, unsigned short  *pIsReadOnly);  /* 0x00423c60 */
short __cdecl GetResourceDiskFreeSpace(byte *pDescriptor, uint32_t *pFreeBytes);  /* 0x00423cf0 */
short __cdecl OpenDirectorySearch(byte *pDesc);  /* 0x00423df0 */
short __cdecl ReadNextDirectoryEntry(byte *pOut);  /* 0x00423e80 */
short __cdecl CheckDescriptorVolumeResolvable(byte *pDesc, short *pFlagOut);  /* 0x00423f00 */
ushort __cdecl IsDisplayAttachLoaderRunning(int pCtx);  /* 0x00423f90 */
short __cdecl StartDisplayAttachLoaderThread(void);  /* LINK-DISCARDED: not in TI.EXE */
short __cdecl StopDisplayAttachLoaderThread(void);  /* LINK-DISCARDED: not in TI.EXE */
short __cdecl SetupDisplayAttachSlotRecord(int *pNode, unsigned int childId, int *pSlot);  /* 0x00423fa0 */
short __cdecl UnregisterDisplayAttachWaitRecord(int *payloadPtr);  /* 0x00424060 */
short __cdecl PollDisplayAttachSlotCompletion(void *payloadRaw, short *pDoneOut, short *pAuxOut, int **ppResourceOut);  /* 0x00424100 */
void __cdecl InitSoundBufferRegionDesc(int *pDesc, int *pOut);  /* 0x004241e0 */
void __cdecl DecodeAudioBuffer(char *pDst, int *pState);  /* 0x00424240 */
void __stdcall ShutdownSoundSystem(void);  /* 0x00424340 */
void __stdcall InitSoundSystem(void);  /* 0x00424370 */
short __stdcall PollWaveDeviceIfSoundActive(void);  /* 0x004244e0 */
void __cdecl SetWaveVolumeIfDeviceReady(short level);  /* 0x00424510 */
void __cdecl TeardownVoicePlaybackRow(int pVoiceRow);  /* 0x00424530 */
void __cdecl InitSoundNodeEntryFromResource(unsigned int *pGroup, unsigned int nodeId, byte *pName, unsigned int *pEntry);  /* 0x004245b0 */
void __cdecl PackGainTripleFromVolumePan(int *pOut, short gainByte, short curveByte);  /* 0x004246f0 */
void __cdecl InitSoundNodeEntry(uint32_t pGroup, uint32_t id, byte *pName, uint32_t *pEntry);  /* 0x004247b0 */
void __stdcall ClearInactiveSoundChannelTemplateKeys(void);  /* 0x004247f0 */
short __stdcall AreSoundChannelsIdle(void);  /* 0x00424820 */
short __stdcall CheckSoundChannelStopped(void);  /* 0x00424850 */
void __cdecl SilenceSoundChannelsByHandle(int handle);  /* 0x00424880 */
void __cdecl StopSoundChannels(short bCh1, short bCh2, short bCh3, short bCh0);  /* 0x004248e0 */
void __cdecl GetPlayingThemeName(byte *pNamedDescBuf, int *pRowCtx, int *pAux);  /* 0x00424950 */
void __cdecl GetPlayingVoiceName(byte *pNamedDescBuf, int *pA, int *pB);  /* 0x004249d0 */
void __cdecl GetPlayingSoundName1(byte *pNameBuf, uint32_t *pRowCtx, uint32_t *pAux);  /* 0x00424a50 */
void __cdecl GetPlayingSoundName2(byte *pNameBuf, uint32_t *pRowCtx, uint32_t *pAux);  /* 0x00424ad0 */
void __cdecl GetPlayingSoundNameByPriority(byte *pNameBuf, uint32_t *pRowCtx, uint32_t *pAux);  /* 0x00424b50 */
void __cdecl PlayThemeList(void *pTarget);  /* 0x00424be0 */
void __cdecl AssignSoundChannel(int *pEntry);  /* 0x00424cb0 */
void __cdecl ReplaceSoundChannel(int *pEntry);  /* 0x00424da0 */
void __cdecl UpgradeSoundChannels(int *pEntry);  /* 0x00424e90 */
void __cdecl ForceAssignSoundChannels(int *pEntry);  /* 0x00424f60 */
void __cdecl AssignPrimarySoundChannel(int *pEntry);  /* 0x00425010 */
unsigned int __cdecl GetSoundNodeKey(unsigned int *pEntry);  /* 0x00425070 */
void __cdecl SetSoundNodeKey(unsigned int *pEntry, unsigned int val);  /* 0x00425080 */
short __cdecl GetSoundNodeVolume(int nodeCtx);  /* 0x00425090 */
void __cdecl SetSoundNodeVolume(byte *pEntry, short volume);  /* 0x004250a0 */
short __cdecl GetSoundNodePan(int nodeCtx);  /* 0x00425190 */
void __cdecl SetSoundNodePan(byte *pEntry, short pan);  /* 0x004251a0 */
short __cdecl GetSoundNodeLoop(int nodeCtx);  /* 0x00425290 */
void __cdecl SetSoundNodeLoop(int pEntry, short loop);  /* 0x004252a0 */
void __cdecl DeactivateSoundNodeEntry(int pEntry);  /* 0x00425340 */
short __cdecl IsVoicePlaybackStillActive(int pVoiceRow);  /* 0x00425390 */
short __cdecl DispatchScriptCommand(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00425400 */
short __cdecl ScriptCmd_StringWidth(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00426cc0 */
short __cdecl ScriptCmd_Sqrt(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00426d60 */
short __cdecl ScriptCmd_CalcMod(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00426dd0 */
short __cdecl ScriptCmd_WalkDest(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00426e40 */
short ScriptCmd_MenuVisible(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00426eb0 */
short ScriptCmd_KeyAborts(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00426ee0 */
short ScriptCmd_Frame(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00426f10 */
short __cdecl ScriptCmd_StarXyz(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00426f40 */
short __cdecl ScriptCmd_SceneXyz(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427050 */
short __cdecl ScriptCmd_CalcDeg(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427150 */
short __cdecl ScriptCmd_CalcDist(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427200 */
short __cdecl ScriptCmd_CalcTurn(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004272a0 */
short __cdecl ScriptCmd_HitTest(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427350 */
short ScriptCmd_CameraHi(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427690 */
short ScriptCmd_MachineType(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004276c0 */
short ScriptCmd_MachineSpeed(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004276f0 */
short ScriptCmd_FileExists(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427720 */
short ScriptCmd_ActionFrame(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004277e0 */
short ScriptCmd_TextDialog(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427870 */
short ScriptCmd_QuestionDialog(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427910 */
short ScriptCmd_IndexToGlobal(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427980 */
short ScriptCmd_CountGlobals(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427a10 */
short ScriptCmd_Type(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427a60 */
short ScriptCmd_Path(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427b10 */
short ScriptCmd_WaveVolume(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427b90 */
short ScriptCmd_Result(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427bc0 */
short ScriptCmd_Random(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427be0 */
short ScriptCmd_PointX(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427c60 */
short ScriptCmd_PointY(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427cc0 */
short ScriptCmd_MakePoint(int *pCtx, int pArgBlock, int *pTable, int *pResult, int *pSkipCount);  /* 0x00427d20 */
short ScriptCmd_Button(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427da0 */
short ScriptCmd_Mouse(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427dd0 */
short ScriptCmd_Tick(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427e10 */
short ScriptCmd_StillDown(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427e40 */
short ScriptCmd_CountLoops(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427e70 */
short ScriptCmd_CountWalks(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427ee0 */
short ScriptCmd_CountCrickets(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427f10 */
short ScriptCmd_IndexToLoop(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427f40 */
short ScriptCmd_IndexToCricket(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00427ff0 */
short __cdecl ScriptCmd_IndexToWalk(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428070 */
short ScriptCmd_PauseLoop(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004280f0 */
short ScriptCmd_PauseWalk(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428190 */
short ScriptCmd_PauseCricket(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428210 */
short ScriptCmd_IsWalk(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428290 */
short ScriptCmd_IsLoop(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428300 */
short ScriptCmd_IsCricket(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428390 */
short __cdecl ScriptCmd_StringToNum(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428400 */
short __cdecl ScriptCmd_NumToString(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428480 */
short __cdecl ScriptCmd_FreeMem(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004284f0 */
short __cdecl ScriptCmd_SysMem(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428520 */
short __cdecl ScriptCmd_HeapSize(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428550 */
short ScriptCmd_Framerate(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428580 */
short ScriptCmd_Variable(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004285b0 */
short __cdecl ScriptCmd_FindWord(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428680 */
short __cdecl ScriptCmd_SubString(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428820 */
short __cdecl ScriptCmd_StringLength(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428900 */
short __cdecl ScriptCmd_PutWord(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428970 */
short __cdecl ScriptCmd_OptionKey(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428c00 */
short __cdecl ScriptCmd_ShiftKey(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428c30 */
short __cdecl ScriptCmd_CommandKey(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428c60 */
short __cdecl ScriptCmd_CalcVectX(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428c90 */
short __cdecl ScriptCmd_CalcVectY(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428d10 */
short __cdecl ScriptCmd_CameraXyz(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428d90 */
short __cdecl ScriptCmd_PlayerXyz(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00428e70 */
short __cdecl EvalTwoScriptArgs(char *pCtx, int pArgBlock, int *pTable, int *pResult1, int *pResult2, int *pSkipOut);  /* 0x00428f50 */
short __cdecl EvalThreeScriptArgs(char *pCtx, int pArgBlock, int *pTable, int *out1, int *out2, int *out3, int *pSkipOut);  /* 0x00428fe0 */
short __cdecl EvalFourScriptArgs(char *pCtx, int pArgBlock, int *pTable, void *pOut1, void *pOut2, void *pOut3, void *pOut4, int *pOffset);  /* 0x004290a0 */
void __cdecl InitSceneResource(short mode);  /* 0x004291a0 */
void __cdecl ShutdownSceneResource(short mode);  /* 0x004294a0 */
void __stdcall InitSceneNodeChannel(void);  /* 0x00429840 */
void __cdecl GetSearchPathSlot(short index, byte *pOut);  /* 0x004298f0 */
void __cdecl SetSearchPathSlot(short index, unsigned char *pState);  /* 0x00429940 */
short __cdecl ValidateDescriptorPath(byte *pDesc, int fileType);  /* 0x00429990 */
void __cdecl SceneChannelHandler(int pChannel, short msg, byte *pParam);  /* 0x00429b70 */
void __cdecl DispatchMenuSelectScriptCall(int pChannel, byte *pParam);  /* 0x0042a0c0 */
void __cdecl DispatchKeyDownScriptCall(short param);  /* 0x0042a140 */
void __cdecl DispatchKeyRepeatScriptCall(short keyCode);  /* 0x0042a250 */
void __cdecl DispatchMouseDownScriptCall(uint packedXY);  /* 0x0042a360 */
short __cdecl ActionCmd_BootScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0042a3e0 */
short __cdecl ActionCmd_PostScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0042a410 */
short __cdecl ActionCmd_ServerScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0042a440 */
short __cdecl DispatchBootMessageString(byte *pName);  /* 0x0042a470 */
short __cdecl ActionCmd_SendToBoot(int *pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0042a550 */
short __cdecl ScriptCmd_SendToBootFx(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSubSkip);  /* 0x0042a570 */
short __cdecl ActionCmd_SendToPost(int pCtx, short *pArgBlock, int *pTable, int *pSkipOut);  /* 0x0042a740 */
short __cdecl ScriptCmd_SendToPostFx(void *pCtxB, short *pInstr, int *pTable, void *pResult, int *pSkipOut);  /* 0x0042a760 */
short __cdecl DispatchServerMessageString(byte *pStr);  /* 0x0042a860 */
short __cdecl ActionCmd_SendToServer(int pCtx, short *pArgBlock, int *pTable, int *pSkipOut);  /* 0x0042a940 */
short __cdecl ScriptCmd_SendToServerFx(void *pCtxB, short *pInstr, int *pTable, void *pResult, int *pSkipOut);  /* 0x0042a960 */
void __cdecl LatchFailedMethodFrame(void *pCtx);  /* 0x0042ab20 */
void __stdcall ShowScriptErrorContinueDialog(void);  /* 0x0042ab60 */
void __stdcall InitScriptRuntime(void);  /* 0x0042ac00 */
void __stdcall ShutdownScriptRuntime(void);  /* 0x0042ac80 */
short __cdecl WaitHeapThenResolveMethodPath(byte *pData, uint len, int **ppHandleOut, int *pPosErr);  /* 0x0042ad10 */
void __stdcall ReapMarkedMethodCacheEntries(void);  /* 0x0042ad70 */
int * __cdecl AcquireMethodHandle(int *pCtx, unsigned int selector);  /* 0x0042adb0 */
void __cdecl ReleaseMethodHandle(int hHandle);  /* 0x0042af80 */
void __cdecl ClearScriptResult(int owner, int kind, byte *pName, int flags);  /* 0x0042aff0 */
void __stdcall EvictLRUMethodCacheEntry(void);  /* 0x0042b010 */
short __cdecl FindMethodCacheEntryByKey(int keyA, int keyB, int *pIdxOut);  /* 0x0042b090 */
short __cdecl FindMethodCacheEntry(int key,int *pIdxOut);  /* 0x0042b100 */
void __cdecl EvictMethodCacheEntry(int slotIdx);  /* 0x0042b160 */
short __cdecl FindInvalidatedMethodCacheEntry(int *pOut);  /* 0x0042b1c0 */
short __cdecl InvalidateCachedMethodsForNode(void *pNode);  /* 0x0042b240 */
short __cdecl ActionCmd_Reboot(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0042b2a0 */
short __cdecl ActionCmd_CurrentCd(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0042b2c0 */
short __cdecl ValidateCurrentCdName(byte *pName);  /* 0x0042b340 */
void __cdecl SetCurrentCdName(byte *pCdName);  /* 0x0042b610 */
short __cdecl IsSlotEntryOnCurrentVolume(int pSlotEntry);  /* 0x0042b6f0 */
void __cdecl TruncatePathToVolume(byte *pDesc);  /* 0x0042b730 */
short ScriptCmd_CurrentCd(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0042b760 */
void __cdecl TruncatePathToLeaf(byte *pBuf);  /* 0x0042b780 */
void __cdecl DecodeHotspotPlaneRowForward(byte *pRle, int deltaPx, int planeIdx);  /* 0x0042b7d0 */
void BlitScaledTrigHotspotSpans(int dstBase);  /* 0x0042b8a0 */
void __cdecl DecodeHotspotPlaneRow(byte *pRle, int deltaPx, int planeIdx);  /* 0x00430e70 */
void __cdecl DecodeHotspotPlaneRowBackward(byte *pRle, int deltaPx, int planeIdx);  /* 0x00430f70 */
void ForwardHotspotRleBlitRow(uint8_t *pbRle, short skip, short count, short width, uint8_t *pbDst, uint8_t *pbPrev);  /* 0x00431140 */
void BackwardHotspotRleBlitRow(uint8_t *pbRle, short skip, short count, short width, uint8_t *pbDst, uint8_t *pbPrev);  /* 0x00431280 */
void BackwardHotspotRleBlitRow386(uint8_t *pbRle, short skip, short count, short width, uint8_t *pbDst, uint8_t *pbPrev);  /* 0x00431470 */
void __cdecl DispatchChannelMessage(short *pChannel, short msg, byte *pParam);  /* 0x00431640 */
void __cdecl EnterSceneRenderLock(int pChannel);  /* 0x004316a0 */
void __cdecl LeaveSceneRenderLock(int pChannel);  /* 0x004316c0 */
void __stdcall RunGameLoop(void);  /* 0x004316e0 */
void __stdcall DispatchMenuStateInteraction(void);  /* 0x00431cc0 */
void __stdcall BeginGameSession(void);  /* 0x00431ce0 */
void __stdcall EndGameSession(void);  /* 0x00431de0 */
short __cdecl ValidateDescriptorPathOfType(byte *pDesc, int fileType);  /* 0x00431ea0 */
void __cdecl OrInputFlagsForDisplayAttachTrackRow(short flagBits);  /* 0x00431ec0 */
void __cdecl NoOpDisplayAttachBeforeUserInteraction(int rowIdx);  /* 0x00431ed0 */
void __cdecl NoOpAfterDisplayAttachVoice(int hotspotIdx);  /* 0x00431ee0 */
short __cdecl ActionCmd_ExportClut(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00431ef0 */
void __cdecl CopyMachineTypeName(void *pBuf);  /* 0x00431f00 */
DWORD __stdcall QueryProcessorType(void);  /* 0x00431f20 */
short __cdecl FindEventViewportDesc(NodeDesc *pDesc, GameEvent *pEv);  /* 0x00431f70 */
short __cdecl IsInteractionAllowed(NodeDesc *pNode);  /* 0x00431fc0 */
void __cdecl HandleQuitRequest(GameEvent *pEv);  /* 0x00431fd0 */
void __cdecl HandleEvent23(GameEvent *pEv);  /* 0x00431fe0 */
short __cdecl CheckSubviewEntry(NodeDesc *pNode, GameEvent *pEv);  /* 0x00431ff0 */
int __cdecl GetScrollDelta(NodeDesc *pNode, GameEvent *pEv);  /* 0x00432000 */
void __cdecl HideHitNodeWindow(NodeDesc *pNode, GameEvent *pEv);  /* 0x00432010 */
void __cdecl SecondaryNodeAction(NodeDesc *pNode, GameEvent *pEv);  /* 0x00432020 */
void __cdecl NavigateBack(NodeDesc *pNode, GameEvent *pEv);  /* 0x00432030 */
unsigned int __cdecl ResolveNavCommandId(GameEvent *pEv, short flag);  /* 0x00432040 */
void __cdecl PerformNodeOperation(NodeDesc *pNode, GameEvent *pEv, short op);  /* 0x004320b0 */
void __cdecl ComposeModuleDirBootPath(char *pState);  /* 0x004320c0 */
void __cdecl GetModuleDirectory(DWORD bufLen, char *pBuf);  /* 0x00432140 */
void __cdecl ResetSceneRectToScreen(short *pState);  /* 0x004321a0 */
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR     lpCmdLine, int       nShowCmd);  /* 0x004321d0 */
void __cdecl DispatchNavCommand(unsigned int menuId);  /* 0x00432210 */
void __cdecl OpenAnimWindow(int *ppWindowRec);  /* 0x00432290 */
void __cdecl OpenMultimediaWindow(int *ppWindowRec);  /* 0x00432340 */
uint __cdecl InvokeEntryCallback(void *pEntry, DfPluginMsg *pMsg);  /* 0x004323f0 */
short __cdecl TryBindSceneDirectoryDll(byte *pDirPrefixDesc, byte *pEntryNameDesc, void *pEntryStruct);  /* 0x00432410 */
short __cdecl FinalizeDirectoryEntry(void *pEntry);  /* 0x004324f0 */
void __cdecl DrawPlaybackPositionMarker(short y);  /* 0x00432510 */
int __cdecl ShowEditVariableDialog(uint32_t pEntry,uint32_t pValue,uint32_t pName);  /* 0x00432570 */
INT_PTR CALLBACK EditVariableDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);  /* 0x004325b0 */
void __cdecl CenterWindowRelative(HWND hWnd);  /* 0x00432d70 */
short __cdecl PromptUntilDescriptorVolumePresent(byte *pDesc, short *pFlagOut);  /* 0x00432e20 */
void __stdcall EjectCdDrive(void);  /* 0x00432fa0 */
void __stdcall EjectCdViaMci(void);  /* 0x00433070 */
char __stdcall GetFirstCdRomDriveLetter(void);  /* 0x004330d0 */
void __cdecl InitVxDHeap(unsigned int size, unsigned int align);  /* 0x004331c0 */
void __stdcall AllocDescriptorPage(void);  /* 0x004332a0 */
void __stdcall ShutdownVxDHeap(void);  /* 0x004333d0 */
void __cdecl InitHeap(void *pvPool, unsigned int cbPool, unsigned int cbBlockMax);  /* 0x00433460 */
void __stdcall ShutdownHeap(void);  /* 0x00433500 */
void __cdecl AllocSecondaryHeap(int *pDesc, unsigned int cbSize);  /* 0x00433550 */
void __cdecl FreeSecondaryHeap(int *pDesc);  /* 0x004335b0 */
void __cdecl SetActiveHeap(int *pHeapDesc);  /* 0x00433620 */
void __stdcall RestorePrimaryHeap(void);  /* 0x00433640 */
int __stdcall GetHeapDescPtr(void);  /* 0x00433660 */
unsigned int __stdcall GetLargestFreeBlockSize(void);  /* 0x00433680 */
unsigned int __cdecl GetHeapFreeCapacity(int heapDesc);  /* 0x004336e0 */
int __stdcall GetHeapFreeBytes(void);  /* 0x00433700 */
int __cdecl TryAllocHeapSpace(uint32_t cbSize);  /* 0x00433760 */
void __cdecl SplitHeapBlock(char *pBlock, int cbSize);  /* 0x00433860 */
void __cdecl FreeHeapBlock(int heapDesc, int *pBlock);  /* 0x00433930 */
void __cdecl InsertFreeBlock(int heapDesc, int pBlock);  /* 0x004339e0 */
void __cdecl CompactHeap(int heapDesc, int cbNeeded);  /* 0x00433a30 */
void __cdecl LinkDescToBlock(int *pDesc, int pBlock);  /* 0x00433d90 */
void __cdecl SortSegmentTable(int lo, int hi);  /* 0x00433dc0 */
int __cdecl FindSegmentPivot(int lo, int hi);  /* 0x00433e10 */
int __cdecl PartitionSegmentTable(int lo, int hi, int pivotVal);  /* 0x00433e60 */
void __cdecl ConsolidateSegment(int heapDesc, int *pEntry);  /* 0x00433f30 */
void __cdecl PushHeapContext(int *pDesc);  /* 0x00434100 */
void __stdcall PopHeapContext(void);  /* 0x00434150 */
DWORD __cdecl GetResObjectSize(void *pHandle);  /* 0x004341a0 */
int *__cdecl ResolveResBlock(int *pHandle);  /* 0x004341d0 */
void __cdecl ResizeResObject(int *pHandle, unsigned int newSize);  /* 0x00434200 */
int * __cdecl LockResDescriptor(int *pHandle);  /* 0x00434360 */
int __cdecl TryResizeBlock(int heapDesc, int *pBlock, unsigned int newSize);  /* 0x004343a0 */
void __cdecl ShrinkHeapBlock(int pHeap, int pBlock, int newSize);  /* 0x004344b0 */
void * __cdecl AllocResObject(DWORD cbData);  /* 0x004345e0 */
int * __stdcall AllocDescriptor(void);  /* 0x004346c0 */
void __cdecl FreeResObject(int *pHandle);  /* 0x004346e0 */
void __cdecl ReleaseDescriptor(int *pDesc);  /* 0x00434730 */
int __cdecl GetResDataPtr(int *pHandle);  /* 0x00434780 */
void __cdecl UnlockResObject(int *pHandle);  /* 0x004347b0 */
int *__cdecl GetLockedResDataPtr(int *pHandle);  /* 0x004347f0 */
void __cdecl MoveResObjectToHeapTop(int *pHandle);  /* 0x00434830 */
int *__cdecl RelocateBlockToHighestFit(int *pDesc);  /* 0x004348b0 */
void __cdecl MoveResObjectToHeapBottom(int *pHandle);  /* 0x00434a20 */
int *__cdecl RelocateBlockToLowestFit(int *pDesc);  /* 0x00434aa0 */
void * __cdecl AllocEngineBuffer(DWORD size);  /* 0x00434b30 */
void FreeEngineBuffer(int pBlock);  /* 0x00434c70 */
void __cdecl FatalErrorWithCode(short errCode);  /* 0x00434cd0 */
void __stdcall InitCLUTPalettes(void);  /* 0x00434cf0 */
void __stdcall FreeCLUTPalettes(void);  /* 0x00434d90 */
ushort TrackHotspotRectPress(short *pRect);  /* 0x00434dd0 */
short IntersectSceneRects(short *pRectA, short *pRectB, int *pOutRect);  /* 0x00434e70 */
short AreSceneRectsIdentical(short *rA, short *rB);  /* 0x00434f30 */
short __cdecl PointInSceneRect(ScenePointYX pt, const short *pRect);  /* 0x00434f80 */
void UnionSceneRects(short *pRectA, short *pRectB, unsigned int *pOutRect);  /* 0x00434fc0 */
short IsSceneRectEmpty(const short *r);  /* 0x00435030 */
void OffsetSceneRect(short *rect, short dy, short dx);  /* 0x00435060 */
void __cdecl InsetSceneRect(short *pRect, short dx, short dy);  /* 0x00435080 */
void __cdecl MapRectangleLinearShort(short *pRect, short *pFromFrame, short *pToFrame);  /* 0x004350a0 */
short __cdecl PascalStringsEqualNoCase(byte *pA, byte *pB);  /* 0x004351a0 */
short __cdecl CompareRawByteSlicesCaseFolded(byte *pA, byte *pB, unsigned short byteCount);  /* 0x00435200 */
void __cdecl AppendPascalString(byte *pSrc, byte *pDst);  /* 0x00435250 */
void __cdecl CopyPascalString(byte *pSrc, byte *pDst);  /* 0x004352b0 */
void __cdecl EncodePascalString(char *pCStr, undefined1 *pPascal);  /* 0x004352d0 */
void __cdecl PascalStringToCString(byte *pPascal, void *pCStr);  /* 0x00435300 */
void __cdecl SeedRandomTable(unsigned int seed);  /* 0x00435330 */
int __cdecl RandomUpTo(uint limit);  /* 0x00435380 */
uint __stdcall GenerateNextPseudoRandomValue(void);  /* 0x004353a0 */
void __cdecl FormatUintAsPascalDecimalString(unsigned int value, char *pPascalOut);  /* 0x00435450 */
unsigned short __cdecl IntSqrt(unsigned int x);  /* 0x004354c0 */
void __cdecl InterpolateCLUT(int *pFrom, int *pTo, int step, int total);  /* 0x00435520 */
void __cdecl FadeCLUTToBlack(int *hClut, short stepCount);  /* 0x00435700 */
void __cdecl FadeCLUTFromBlack(int *hClut,short stepCount);  /* 0x00435750 */
void __cdecl LoadStringResourceAsPascal(short baseId, short offsetId, char *pPascal);  /* 0x004357a0 */
short __stdcall IsEscapeKeyDown(void);  /* 0x00435800 */
short __stdcall IsMultimediaSequentialModifierKeyDown(void);  /* 0x00435820 */
short __stdcall IsSoundSequentialModifierKeyDown(void);  /* 0x00435840 */
short __stdcall IsMusicSequentialModifierKeyDown(void);  /* 0x00435860 */
unsigned int __cdecl ParseLeadingIntFromPascalString(byte *pPascal);  /* 0x00435880 */
void * __cdecl LoadResourceByTag(unsigned int tag, byte *pDesc);  /* 0x004358e0 */
void * __cdecl LoadEmbeddedResource(LPCSTR pName, unsigned int tag);  /* 0x00435980 */
void * __cdecl GetResourceHandle(unsigned int tag, short id);  /* 0x00435a60 */
void __cdecl ReleaseResource(int *pHandle, int tag);  /* 0x00435ae0 */
void __cdecl FetchHotspotDesc(HWND *pHwnd, NodeDesc *pOut);  /* 0x00435b00 */
void __cdecl SetChannelWindowState(void *pHwnd, short *pState);  /* 0x00435b60 */
short __cdecl FindActiveHotspot(NodeDesc *pOut);  /* 0x00435bc0 */
short __cdecl HitTestHotspot(unsigned int packedYX, NodeDesc *pNodeDesc);  /* 0x00435c20 */
void __cdecl DestroyChannelViewport(int pChannel);  /* 0x00435cf0 */
void __cdecl FlushChannelDisplay(int pChannel);  /* 0x00435d50 */
void __cdecl RefreshChannelState(int pChannel, int *pParam);  /* 0x00435d60 */
void __cdecl CreateSceneViewport(void *pUnused, void **ppOut, int unused3);  /* 0x00435d70 */
void __cdecl SetChannelScrollMax(int pChannel, short hMax, short vMax);  /* 0x00435e00 */
void __cdecl SetChannelScrollMin(int pChannel, short hMin, short vMin);  /* 0x00435e70 */
void __cdecl GetChannelScrollPos(int pChannel, short *pHOut, short *pVOut);  /* 0x00435ee0 */
void __cdecl SetChannelScrollPos(int pChannel, short hPos, short vPos);  /* 0x00435f10 */
void __cdecl PostViewportSetAction(int pChannel);  /* 0x00435f40 */
void __cdecl ReloadCLUT(int *pSrcClut);  /* 0x00435f50 */
void __cdecl InitCLUT(int *hClut);  /* 0x00435ff0 */
void __stdcall LoadPluginDirectory(void);  /* 0x004360a0 */
void __stdcall UnloadPluginDirectory(void);  /* 0x00436240 */
short __cdecl ScriptCmd_PluginExists(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00436330 */
short __cdecl ActionCmd_Plugin(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x004363b0 */
short __cdecl ScriptCmd_PluginFx(char *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSubSkip);  /* 0x004363f0 */
short __cdecl DispatchPluginEngineRequest(DfPluginMsg *pMsg);  /* 0x00436700 */
void __cdecl CheckPluginArgTypes(DfPluginMsg *pMsg, short k0, short k1, short k2, short k3);  /* 0x004373a0 */
short __cdecl LookupPluginByDescriptor(byte *pDesc, unsigned int *pEntryCtxOut);  /* 0x00437410 */
short __stdcall FlushAllRenderNodes(void);  /* 0x00437490 */
void __stdcall AcquireRenderScanLock(void);  /* 0x004374f0 */
void __stdcall ReleaseRenderScanLock(void);  /* 0x00437530 */
void __cdecl SetRenderParentContext(int *pNodeHandle);  /* 0x00437610 */
void __cdecl ReleaseRenderParentContext(int ctx);  /* 0x00437640 */
void __cdecl GetGlobalRenderNodeListHead(void **ppHead);  /* 0x004377c0 */
short __cdecl ReserveHeapSpace(int size, short *pSucceeded);  /* 0x004377d0 */
short __cdecl EvictCachedChildrenForSpace(int *pAmount);  /* 0x004378d0 */
int __stdcall NextRenderNodeSerial(void);  /* 0x00437ac0 */
short __cdecl OpenContainerNode(byte *pDescriptor, int **ppNode);  /* 0x00437ad0 */
int * __cdecl AllocChildTable(short requestedSize);  /* 0x00437bd0 */
short __cdecl FindRenderNodeByContextValue(int nodeVal, int *pOut);  /* 0x00437ca0 */
void __cdecl SetNodeDestroyHook(int pHandler);  /* 0x00437cf0 */
short __cdecl DestroyRenderNode(void *pNode);  /* 0x00437d00 */
short __cdecl HasRenderChild(int *pCtx, unsigned int chanId);  /* 0x00437e80 */
short __cdecl AttachRenderChildHandle(int *pCtx, unsigned int chanId, int *pRes);  /* 0x00437f40 */
short __cdecl AttachRenderChild(int *pParent, unsigned int childId, int *pHandleOut);  /* 0x00438160 */
short __cdecl DetachRenderChild(int pParent, int childId);  /* 0x004383c0 */
short __cdecl ReleaseRenderChild(int pCtx, int childId);  /* 0x00438470 */
short __cdecl SetChildRenderData(int pCtx, int childId, uint32_t renderData);  /* 0x00438530 */
short __cdecl DestroyChildRenderEntry(uint32_t *pCtx, uint32_t childId);  /* 0x004385f0 */
short __cdecl SetupRenderObjectResource(int *pNode, uint32_t *pChildId, int *hResource);  /* 0x00438690 */
short MatchPendingRenderNodeDescriptor(byte *pDescriptor);  /* 0x004388b0 */
short __cdecl TakeMatchingPendingRenderNode(byte *pDesc, int *pCtxOut);  /* 0x004388f0 */
short __cdecl DeferRenderNodeClose(int pCtx);  /* 0x00438960 */
short __stdcall DestroyAllRenderNodes(void);  /* 0x004389d0 */
void __cdecl MessagesChannelHandler(int pChannel, short msg, int *pParam);  /* 0x00438a10 */
void __cdecl HandleMessagesKey(short key);  /* 0x00438c90 */
void __cdecl HandleMessagesHit(int *pParam);  /* 0x00438d10 */
void __stdcall DrawMessagesWindow(void);  /* 0x00438d20 */
void __cdecl FormatScriptArgAsDescriptor(short *pArg, byte *pOut);  /* 0x00438dc0 */
void __stdcall InitMessagesChannel(void);  /* 0x00438ed0 */
void __cdecl RunMessagesInputLine(byte *pLine);  /* 0x00438fc0 */
short __cdecl InvokeMessagesMethodDispatchKernel(int pCtx, short *pInstr, int *pTable, short *pResult);  /* 0x004390e0 */
void __cdecl BumpSceneOverlayModeFloor(short mode);  /* 0x00439320 */
void __stdcall InitSetView(void);  /* 0x00439340 */
void __stdcall ShutdownSetView(void);  /* 0x004394f0 */
void __stdcall CommitSetViewport(void);  /* 0x00439570 */
void __stdcall ResetSetMoveState(void);  /* 0x004396d0 */
short __stdcall PostTickSceneUpdate(void);  /* 0x00439700 */
void __stdcall ReshowSetViewIfIdle(void);  /* 0x00439750 */
void __cdecl StartSetMove(short moveState);  /* 0x00439790 */
void __cdecl StartSetMoveStraight(void);  /* 0x00439990 */
uint __cdecl GetStraightMoveForCurrentView(void);  /* 0x00439a60 */
void __cdecl ShowSetView(byte *pSceneDesc, byte *pTriggerName);  /* 0x00439c30 */
short __stdcall StepSetMove(void);  /* 0x00439fc0 */
short __stdcall StepStraightSetMove(void);  /* 0x0043a1c0 */
void __stdcall RenderSceneFrame(void);  /* 0x0043a3e0 */
void __stdcall WaitForSceneTick(void);  /* 0x0043a4c0 */
void __cdecl ShowSetFrame(int pFrame, int frameIdx, int totalFrames);  /* 0x0043a4e0 */
void __cdecl WalkBothSlotTablesCalcRender(short mode);  /* 0x0043a720 */
short __cdecl FindPaintingBindingKeyAtPoint(int packedPt, int *pBindingKeyOut);  /* 0x0043a740 */
void CopyTrigSceneBufferAndViewportPair(void *pDstBuf, void *pDstViewport);  /* 0x0043a8a0 */
void CopyTrigOverlayBufferAndViewportPair(void *pDstBuf, void *pDstViewport);  /* 0x0043a8d0 */
int __stdcall GetActiveSetMove(void);  /* 0x0043a900 */
short __cdecl AngleDistToDeltaX(unsigned short frameSlotIndex, short sceneId);  /* 0x0043a910 */
short __cdecl AngleDistToDeltaY(unsigned short frameSlotIndex, short sceneId);  /* 0x0043a940 */
short __cdecl ProjectSetPointToScreen(int pPoint, short *pScreenYX);  /* 0x0043a970 */
void AppendCalcScratchRow(unsigned int *pRow);  /* 0x0043aa40 */
void AddDirtyRect(short *pRect);  /* 0x0043aa90 */
void __stdcall SortCalcScratchRowsByHotspotBand(void);  /* 0x0043ab20 */
void MergeOverlappingDirtyRects(void);  /* 0x0043abb0 */
void SlideDirtyRectSlotsDown(unsigned int *pFrom, unsigned int *pTo, short rectCount);  /* 0x0043ac50 */
void __stdcall MarkScratchRowsOverlappingDirtyRects(void);  /* 0x0043ac80 */
void __stdcall StampCalcScratchRowsToHotspotBufs(void);  /* 0x0043ad00 */
void __cdecl RestoreDirtyRectFromBackdrop(short *transitionRect);  /* 0x0043ae30 */
void __cdecl DispatchVisualEffect(short *pDesc, short value, short cueId);  /* 0x0043b000 */
void __cdecl VisualEffectBarnDoorClose(uint32_t *pRect, short stepCount);  /* 0x0043b1f0 */
void __cdecl VisualEffectBarnDoorOpen(uint32_t *pRect, short stepCount);  /* 0x0043b2d0 */
void __cdecl VisualEffectIrisClose(uint32_t *pRect, short stepCount);  /* 0x0043b3b0 */
void __cdecl VisualEffectIrisOpen(uint32_t *pRect, short stepCount);  /* 0x0043b500 */
void __cdecl VisualEffectScrollDown(uint32_t *pRect, short stepCount);  /* 0x0043b5f0 */
void __cdecl VisualEffectScrollUp(uint32_t *pRect, short stepCount);  /* 0x0043b6a0 */
void __cdecl VisualEffectScrollRight(uint32_t *pRect, short stepCount);  /* 0x0043b750 */
void __cdecl VisualEffectScrollLeft(uint32_t *pRect, short stepCount);  /* 0x0043b800 */
void __cdecl VisualEffectWipeDown(uint32_t *pRect, short stepCount);  /* 0x0043b8b0 */
void __cdecl VisualEffectWipeUp(uint32_t *pRect, short stepCount);  /* 0x0043b940 */
void __cdecl VisualEffectWipeRight(uint32_t *pRect, short stepCount);  /* 0x0043b9d0 */
void __cdecl VisualEffectWipeLeft(uint32_t *pRect, short stepCount);  /* 0x0043ba70 */
void __cdecl VisualEffectVenetian(short *pRect, short stepCount);  /* 0x0043bb10 */
void __cdecl VisualEffectPlain(uint32_t *pRect, short stepCount);  /* 0x0043bbb0 */
void __cdecl VisualEffectTurnRight(uint32_t *pRect, short stepCount);  /* 0x0043bbc0 */
void __cdecl VisualEffectTurnLeft(uint32_t *pRect, short stepCount);  /* 0x0043bca0 */
void __cdecl VisualEffectTurnUp(uint32_t *pRect, short stepCount);  /* 0x0043bd70 */
void __cdecl VisualEffectTurnDown(uint32_t *pRect, short stepCount);  /* 0x0043be70 */
void __cdecl VisualEffectTurnHalfRight(uint32_t *pRect, short stepCount);  /* 0x0043bf70 */
void __cdecl VisualEffectTurnHalfLeft(uint32_t *pRect, short stepCount);  /* 0x0043c080 */
void __cdecl WaitUntilTick(int baseTick, short extraTicks);  /* 0x0043c180 */
void __cdecl BlitBackBufferRect(int *pCoord1, int *pCoord2);  /* 0x0043c1a0 */
void __cdecl BlitScreenRect(uint32_t *pCoord1, uint32_t *pCoord2);  /* 0x0043c230 */
short __cdecl DispatchActionCommand(int pCtx, short *pArgBlock, int *pTable, int *pSkipCount);  /* 0x0043c2c0 */
short __cdecl ActionCmd_MenuVisible(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043d6c0 */
short __cdecl ActionCmd_DrawString(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043d720 */
short __cdecl ActionCmd_CameraHi(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043d7e0 */
short __cdecl ActionCmd_NoteDialog(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043d830 */
short __cdecl ActionCmd_Message(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043d890 */
short __cdecl ActionCmd_Path(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043d8f0 */
short __cdecl ActionCmd_WaveVolume(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043d980 */
short __cdecl ActionCmd_Result(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043d9f0 */
short ActionCmd_HideCursor(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043da30 */
short __cdecl ActionCmd_VisualEffect(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043da50 */
short __cdecl ActionCmd_Clut(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043db50 */
short __cdecl ActionCmd_MixClut(char *pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043dbc0 */
short __cdecl ActionCmd_Framerate(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043def0 */
short __cdecl ActionCmd_ForceUpdate(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043df60 */
short __cdecl ActionCmd_Cursor(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043df70 */
short ActionCmd_ShowCursor(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e090 */
short __cdecl ActionCmd_Debugger(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e0b0 */
short __cdecl ActionCmd_Error(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e0c0 */
short __cdecl ActionCmd_ScreenToBlack(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e0d0 */
short __cdecl ActionCmd_BlackToScreen(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e150 */
short __cdecl ActionCmd_BlackScreen(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e1d0 */
short __cdecl ActionCmd_Delay(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e220 */
short __cdecl ActionCmd_KeyAborts(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e270 */
short __cdecl ActionCmd_Quit(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e2c0 */
short __cdecl ActionCmd_FlushEvents(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e2f0 */
short __cdecl ActionCmd_StopWalk(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e310 */
short __cdecl ActionCmd_StopLoop(int *pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e370 */
short __cdecl ActionCmd_StopCricket(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e3f0 */
short __cdecl ActionCmd_PauseLoop(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e450 */
short __cdecl ActionCmd_PauseWalk(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e4e0 */
short __cdecl ActionCmd_PauseCricket(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e550 */
short __cdecl ActionCmd_PlayMovie(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e5c0 */
short __cdecl ActionCmd_Variable(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043e620 */
short __cdecl ResolveClutFromScriptValue(short *pScriptValue, int **ppHandle);  /* 0x0043e6f0 */
int * __cdecl BuildCLUTFromPaletteData(int pData);  /* 0x0043e990 */
short __cdecl ActionCmd_OpenPuppetFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043ea00 */
short __cdecl OpenPuppetNode(byte *pDesc, short *pSceneGuard);  /* 0x0043eab0 */
short __cdecl ActionCmd_ClosePuppetFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043eec0 */
void __cdecl ClosePuppetNode(short *pGuard);  /* 0x0043ef40 */
short __stdcall SendOpenPuppetMessage(void);  /* 0x0043f070 */
short __stdcall SendClosePuppetMessage(void);  /* 0x0043f0f0 */
short __cdecl ActionCmd_PuppetBevel(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043f170 */
short __cdecl ScriptCmd_PuppetEvent(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0043f260 */
short __cdecl ActionCmd_PuppetSpeak(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043f320 */
short __cdecl ActionCmd_PuppetClear(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043f440 */
short __cdecl ActionCmd_PuppetGrab(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043f470 */
short __cdecl ActionCmd_PuppetSubtitle(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043f4c0 */
short __cdecl ActionCmd_PuppetBase(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043f520 */
short ScriptCmd_PuppetBase(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0043f590 */
short __cdecl RunSendToPuppetCommand(byte *pCmd);  /* 0x0043f5b0 */
short __cdecl ActionCmd_SendToPuppet(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043f690 */
short __cdecl ScriptCmd_SendToPuppetFx(int *pCtx, int pArgBlock, int *pTable, void *pResult, int *pSubSkip);  /* 0x0043f6b0 */
short __cdecl ActionCmd_PuppetScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043f900 */
short __cdecl ScriptCmd_CountPuppets(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0043f9c0 */
short __cdecl ScriptCmd_IndexToPuppet(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSubSkip);  /* 0x0043fa30 */
short __cdecl ScriptCmd_CurrentPuppet(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0043fb20 */
short __cdecl ActionCmd_PuppetVisible(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043fb90 */
short ScriptCmd_PuppetVisible(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0043fbf0 */
short __cdecl ActionCmd_PuppetParam(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043fc30 */
short ScriptCmd_PuppetParam(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0043fd70 */
short __cdecl ActionCmd_PuppetScramble(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0043fe60 */
short __cdecl ScriptCmd_CountBevels(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0043ff40 */
void __cdecl CopyFirstPuppetName(byte *pDest);  /* 0x0043ff80 */
short __cdecl LookupPuppetSelectorByName(byte *pName, unsigned int *pSelectorOut);  /* 0x0043fff0 */
void __stdcall ShowPuppetBasePose(void);  /* 0x004400a0 */
void __cdecl PlayPuppetTrack(short trackId);  /* 0x004401a0 */
short __cdecl TrackHasSubtitleText(int pTrackEntry);  /* 0x00440390 */
void __cdecl AdvanceAnimPlaybackFrame(int *hAnimList, short prevFrame, short frameIdx, short textStrip);  /* 0x00440490 */
void __cdecl RenderAnimFrameForChannel(char *pChanBase, short chanIdx, short frameIdx, uint32_t packedPos);  /* 0x004406c0 */
void __stdcall GrabBackdropIntoChannel0(void);  /* 0x00440780 */
void __cdecl PresentBackBufferRect(short *pScrollRects);  /* 0x004408a0 */
short __cdecl FindPuppetTrackIndexByName(byte *pDesc);  /* 0x004408d0 */
void __cdecl DrawBevelTable(short *pTable);  /* 0x004409b0 */
void __cdecl GetBevelRowRect(short rowIdx, uint32_t *pOutRect);  /* 0x00440ae0 */
void __cdecl SwitchPuppetVariantForTrack(short *pTrackEntry);  /* 0x00440b30 */
int __cdecl GetQuadVariantChildIndexOrFatal(int pData);  /* 0x00440f00 */
void AttachPuppetTrackFrames(int *pTrackSlot, short packedPhase);  /* 0x00440f50 */
short __cdecl FindRowWithMatchingShortField(int pTable, short count, short fieldIdx, short key);  /* 0x00441040 */
void __stdcall DetachPuppetFrames(void);  /* 0x00441090 */
short __cdecl PollPuppetEventInput(uint32_t *pEntryIdOut);  /* 0x00441480 */
void __cdecl RunVoiceSamplePlaybackModal(short playlistRowIdx);  /* 0x00441730 */
short __cdecl FindPlayerVoiceRowIndexByName(byte *pSearchName);  /* 0x00441840 */
short __cdecl ProcessEventKeyInput(short *pEvent, undefined2 *pToggleFlag);  /* 0x00441910 */
void __cdecl DrawSubtitleText(byte *pText);  /* 0x00441a80 */
void CopyHotspotRectBetweenBuffers(HotspotBuf *pSrc, HotspotBuf *pDst, short *pRect);  /* 0x00441c00 */
int __cdecl DecodePageNodeImage(int *pHandle, HotspotBuf *pDst);  /* 0x00441ca0 */
void __cdecl GetScriptBindingTableBasePointers(uint32_t *pPropWords, uint32_t *pMethodWords, uint32_t *pObjectWords);  /* 0x004420c0 */
short TickScriptBindings(void);  /* 0x004420e0 */
void __cdecl RemoveWalkEntry(byte *pNameDesc);  /* 0x004421d0 */
void __cdecl RemoveLoopEntry(byte *pTypeDesc, byte *pNameDesc);  /* 0x00442270 */
void __cdecl RemoveCricketEntry(byte *pNameDesc);  /* 0x00442300 */
short __cdecl IsWalkActive(byte *pNameDesc);  /* 0x00442370 */
short __cdecl IsLoopActive(byte *pTypeDesc, byte *pNameDesc);  /* 0x004423c0 */
short __cdecl IsCricketActive(byte *pNameDesc);  /* 0x00442420 */
void __cdecl CopyWalkDestName(byte *pIn148, byte *pOut458);  /* 0x00442470 */
short __cdecl ActionCmd_MakeLoop(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x004424e0 */
short __cdecl InstallLoopEntry(short *pEntry);  /* 0x00442610 */
short TickLoopEntry(short *pEntry);  /* 0x00442670 */
short __cdecl CountLoopsByType(byte *pTypeDesc);  /* 0x00442790 */
void __cdecl GetLoopNameByIndex(short index, byte *pTypeDesc, byte *pOut);  /* 0x004427d0 */
short __cdecl GetLoopPauseCount(byte *pTypeDesc, byte *pNameDesc);  /* 0x00442840 */
void __cdecl AdjustLoopPauseCount(byte *pTypeDesc, byte *pNameDesc, short resume);  /* 0x004428a0 */
short __cdecl ActionCmd_TurnToDeg(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00442970 */
short __cdecl InstallWalkEntry(short *pNewEntry);  /* 0x00442a30 */
short __cdecl ActionCmd_WalkToStar(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00442a90 */
short __cdecl ActionCmd_WalkToXyz(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00442bc0 */
short __cdecl ActionCmd_WalkOnPath(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00442cc0 */
short __cdecl ActionCmd_WalkOnRoad(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00442e20 */
short __cdecl ActionCmd_WalkOnFrames(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00442f80 */
short __cdecl BuildTurnToDegEntry(ushort nameTag, int pNamed, short *pRow);  /* 0x004430e0 */
short __cdecl BuildWalkToStarEntry(byte *pTargetName, int pNamed, unsigned short *pOutEntry);  /* 0x00443150 */
short __cdecl BuildWalkToXyzEntry(short x, short y, short z, int pNamed, undefined2 *pOutEntry);  /* 0x00443260 */
short __cdecl BuildWalkOnPathEntry(byte *pName1, byte *pName2, int pNamed, unsigned short *pOutEntry);  /* 0x00443380 */
short __cdecl BuildWalkOnRoadEntry(byte *pName1, byte *pName2, int pNamed, unsigned short *pOutEntry);  /* 0x00443490 */
short __cdecl BuildWalkOnFramesEntry(byte *pName1, byte *pName2, int pNamed, unsigned short *pOutEntry);  /* 0x00443570 */
short TickWalkEntry(short *pEntry);  /* 0x00443730 */
short __stdcall CountWalks(void);  /* 0x00443b40 */
void __cdecl GetWalkNameByIndex(short oneBasedIdx, byte *pOutDesc);  /* 0x00443b60 */
short __cdecl GetWalkPauseCount(byte *pNameDesc);  /* 0x00443bc0 */
void __cdecl AdjustWalkPauseCount(byte *pNameDesc, short resume);  /* 0x00443c10 */
short __cdecl ActionCmd_MakeCricket(char *pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00443cc0 */
short __cdecl InstallCricketEntry(short *pNewEntry);  /* 0x00443f70 */
short TickCricketEntry(short *pEntry);  /* 0x00443fc0 */
void __stdcall AssignCricketSoundChannels(void);  /* 0x00444260 */
short __cdecl ComputeStereoPanning(short sourceAngle, short listenerAngle);  /* 0x00444320 */
short __stdcall CountCrickets(void);  /* 0x00444390 */
void __cdecl GetCricketNameByIndex(short index, byte *pOut);  /* 0x004443b0 */
short __cdecl GetCricketPauseCount(byte *pNameDesc);  /* 0x00444410 */
void __cdecl AdjustCricketPauseCount(byte *pNameDesc, short resume);  /* 0x00444460 */
short __cdecl GetPathStartAngle(int *pHandle);  /* 0x00444510 */
short GetDescriptorTypeId(byte *pDesc);  /* 0x00444580 */
void __cdecl GetDescriptorById(short typeId, byte *pOutDesc);  /* 0x00444620 */
short __cdecl ComputeAngleBetweenPoints(int pA, int pB);  /* 0x004446d0 */
short __cdecl ComputeDirectionAngle(short x, short y);  /* 0x00444700 */
short __cdecl ComputeAngularDistance(short a, short b);  /* 0x004448d0 */
short __cdecl SampleCurveKeyframe(int *hCurve, int tickPos, unsigned short *pOut);  /* 0x00444900 */
short __cdecl SampleStepKeyframe(int *hSteps, int stepIdx, unsigned short *pOut);  /* 0x00444ac0 */
short __cdecl StepValueTowardTarget(short current, short target, short delta, short max);  /* 0x00444c10 */
short __cdecl ActionCmd_OpenStageFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00444ca0 */
short __cdecl OpenStageNode(byte *pDesc, short *pHandle);  /* 0x00444d40 */
short __cdecl ActionCmd_CloseStageFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00444f20 */
void __cdecl CloseStageNode(short *pHandle);  /* 0x00444fe0 */
short __cdecl ActionCmd_GotoFlat(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00445040 */
short __stdcall SendOpenFlatMessage(void);  /* 0x00445180 */
short __stdcall SendCloseFlatMessage(void);  /* 0x00445240 */
short __stdcall CompleteStageInit(void);  /* 0x00445300 */
short __stdcall StagePreShutdownStep(void);  /* 0x00445430 */
short __cdecl ScriptCmd_CountFlats(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00445560 */
short __cdecl ScriptCmd_IndexToFlat(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004455c0 */
short __cdecl ScriptCmd_CountButtons(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSubSkip);  /* 0x004456c0 */
short __cdecl ScriptCmd_IndexToButton(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004457d0 */
short __cdecl ScriptCmd_FlatToIndex(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00445950 */
short ScriptCmd_StageVisible(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x004459d0 */
short __cdecl ActionCmd_StageVisible(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00445a10 */
short __cdecl ScriptCmd_CurrentFlat(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00445b00 */
short __cdecl ScriptCmd_CurrentStage(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00445ba0 */
short __cdecl ScriptCmd_PointInStage(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00445c10 */
short __cdecl ScriptCmd_PointInButton(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00445cd0 */
short __cdecl ActionCmd_StageScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00445de0 */
short __cdecl ActionCmd_FlatScript(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x00445e50 */
short __cdecl ActionCmd_ButtonScript(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00445f10 */
short __cdecl ActionCmd_SendToButton(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00446010 */
short __cdecl ScriptCmd_SendToButtonFx(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSubSkip);  /* 0x00446030 */
short __cdecl RunSendToFlatCommand(byte *pCmd);  /* 0x00446460 */
short __cdecl ActionCmd_SendToFlat(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00446540 */
short __cdecl ScriptCmd_SendToFlatFx(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSubSkip);  /* 0x00446560 */
short __cdecl RunSendToStageCommand(byte *pMethod);  /* 0x00446880 */
short __cdecl ActionCmd_SendToStage(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00446960 */
unsigned short __cdecl ScriptCmd_SendToStageFx(int *pCtx, short *pArgBlock, int *pTable, void *pResult, int *pSubSkip);  /* 0x00446980 */
ushort __cdecl FindButtonNameAtPoint(int packedPt, byte *pDescOut);  /* 0x00446b40 */
short __cdecl ActionCmd_StageParam(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x00446c50 */
short ScriptCmd_StageParam(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x00446cf0 */
void __cdecl NavigateToStagePage(int fromPage);  /* 0x00446d80 */
short __cdecl LookupFlatEntry(byte *pName, uint *pOut);  /* 0x00446fd0 */
int __cdecl FlatPageNameToIndex(byte *pName);  /* 0x00447030 */
short __cdecl LookupButtonEntry(uint childId, byte *pName, uint *pOut);  /* 0x004470c0 */
int __cdecl FindButtonIndexByName(int *pNodeHandle, byte *pName);  /* 0x004471b0 */
void __stdcall ConvertMessageNamesToPascal(void);  /* 0x00447240 */
void __cdecl ConvertMessageNameToPascal(char *pName);  /* 0x00447260 */
void __stdcall ResetLoopStacks(void);  /* 0x00447290 */
short __cdecl EvalMethodCall(short *pCtxA, void *pCtxB, short *pInstr, int *pTable, void *pResult, int *pSkipCount);  /* 0x004472a0 */
short __cdecl ExecuteMethodBody(void *pCtx, void *pCtxB, short *pInstr, int *pTable, void *pResult);  /* 0x004473c0 */
short __cdecl BindMethodArguments(int pBlock, int *pScope, int pCtxB, short *pInstr, int *pTable, int *pSkip);  /* 0x00447490 */
short __cdecl InterpretStatements(void *pCtx, short *pInstr, int *pTable, void *pResult);  /* 0x00447640 */
short __cdecl DeclareVariableList(short *pFormalWalk, int *pScope, int *pSkipOut);  /* 0x004480a0 */
short __cdecl UndeclareVariableList(short *pWalk, int *pScope, int *pSkipOut);  /* 0x00448110 */
short __cdecl SkipIfFalseBranchToEndif(short *pInstr, int *pSkipOut);  /* 0x00448170 */
short __cdecl SkipWhileBodyToWend(short *pInstr, int *pSkipOut);  /* 0x004481d0 */
short __cdecl SkipSwitchBodyToEndswitch(short *pInstr, int *pSkipOut);  /* 0x00448230 */
short __cdecl SkipForBodyToNextStatement(short *pInstr, int *pSkipOut);  /* 0x00448290 */
short __cdecl FindElseBranchOrMarkNoElse(short *pInstr, int *pSkipOut);  /* 0x004482f0 */
short __cdecl EvaluateSwitchFirstMatchingCase(int pCtx, short *pInstr, int *pTable, short *pSwitchKey, int *pSkipOut);  /* 0x00448370 */
short __cdecl SkipFallThroughCaseLabels(short *pAfterCaseExpr, int *pWordCountOut);  /* 0x00448550 */
short __cdecl AdvanceToNextMethodBlock(short *pBlock, int *pSkip);  /* 0x004485b0 */
short __cdecl ClassifyMethodIdentifier(short *pInstr);  /* 0x00448620 */
short __cdecl PlayMovie(byte *pMovieName, void *pChannel);  /* 0x004486a0 */
short RunMovieFrameActions(void);  /* 0x00449310 */
int __cdecl FindMovieFrameIndexByName(byte *pName);  /* 0x00449c20 */
short __stdcall WaitForMovieFrameSound(void);  /* 0x00449c80 */
short __stdcall PumpMovieInputOnce(void);  /* 0x00449cc0 */
short __cdecl ProcessMovieKeyInput(short *pEvent, short *pWantPoll);  /* 0x0044a000 */
void __stdcall NoOpDisplayAttachAfterRowT(void);  /* 0x0044a170 */
void __stdcall ScheduleMovieTimedJumps(void);  /* 0x0044a260 */
short __stdcall ShowMovieFrame(void);  /* 0x0044a480 */
int FindMovieVoiceRowIndexByName(byte *pName);  /* 0x0044a910 */
void __stdcall PretouchMovieFrameRange(void);  /* 0x0044a970 */
short __cdecl MovieWipeSidesIn(uint *pRect, short stepCount);  /* 0x0044ae10 */
short __cdecl MovieWipeSidesOut(uint32_t *pRect, short stepCount);  /* 0x0044aef0 */
short __cdecl MovieWipeBoxIn(uint32_t *pRect, short stepCount);  /* 0x0044afe0 */
short __cdecl MovieWipeBoxOut(uint32_t *pRect, short stepCount);  /* 0x0044b130 */
short __cdecl MovieScrollDown(uint32_t *pRect, short stepCount);  /* 0x0044b220 */
short __cdecl MovieScrollUp(uint32_t *pRect, short stepCount);  /* 0x0044b2e0 */
short __cdecl MovieScrollRight(uint32_t *pRect, short stepCount);  /* 0x0044b3a0 */
short __cdecl MovieScrollLeft(uint32_t *pRect, short stepCount);  /* 0x0044b460 */
short __cdecl MovieWipeDown(uint32_t *pRect, short stepCount);  /* 0x0044b520 */
short __cdecl MovieWipeUp(uint32_t *pRect, short stepCount);  /* 0x0044b5c0 */
short __cdecl MovieWipeRight(uint32_t *pRect, short stepCount);  /* 0x0044b660 */
short __cdecl MovieWipeLeft(uint32_t *pRect, short stepCount);  /* 0x0044b700 */
short __cdecl MovieWipeVenetian(short *pRect, short stepCount);  /* 0x0044b7a0 */
short __cdecl MovieWipeRandomBlocks(short *pCtx, short scale);  /* 0x0044b840 */
void __cdecl MovieWipeRandomBlocksStep(byte *pGrid, int gridBytes, short rowWidthMinus1, short colCount, short *pOrigin);  /* 0x0044b930 */
short __cdecl MovieWipeRandomRows(uint *pRect, short stepCount);  /* 0x0044ba50 */
short __cdecl MovieWipeRandomColumns(uint *pRect, short stepCount);  /* 0x0044bb80 */
short __cdecl MovieWipePlain(uint32_t *pRect, short stepCount);  /* 0x0044bcb0 */
short __cdecl MovieFadeOut(uint32_t *pRect, short stepCount);  /* 0x0044bce0 */
short __cdecl MovieFadeIn(uint32_t *pRect, short stepCount);  /* 0x0044bd60 */
short __cdecl MoviePushUp(uint32_t *pRect, short stepCount);  /* 0x0044bde0 */
short __cdecl MoviePushDown(uint32_t *pRect, short stepCount);  /* 0x0044bef0 */
short __cdecl MoviePushLeft(uint32_t *pRect, short stepCount);  /* 0x0044c000 */
short __cdecl MoviePushRight(uint *pRect, short stepCount);  /* 0x0044c110 */
short __cdecl WaitMovieWipeStep(short timeoutTicks);  /* 0x0044c220 */
void __cdecl BlitMovieRect(uint *pPacked, uint *pPacked2);  /* 0x0044c260 */
short __cdecl ActionCmd_OpenTrackFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044c410 */
short __cdecl OpenTrackNode(byte *pDesc, int *pOut);  /* 0x0044c4f0 */
short __cdecl ActionCmd_CloseTrackFile(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044c8a0 */
void __cdecl CloseTrackNode(int *pTrack);  /* 0x0044c9a0 */
short __cdecl ActionCmd_PlayTheme(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044ca80 */
short __cdecl ActionCmd_SingleSound(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044cb00 */
short __cdecl ActionCmd_MultipleSound(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044cb40 */
short __cdecl ActionCmd_DualSound(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044cb80 */
short __cdecl ActionCmd_BothSound(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044cbc0 */
short __cdecl ActionCmd_VoiceSound(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044cc00 */
short __cdecl ActionCmd_HaltTheme(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044cc40 */
short __cdecl ActionCmd_HaltSound(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044cc60 */
short __cdecl ActionCmd_HaltVoice(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044cd00 */
short __cdecl ActionCmd_SoundVol(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044cd20 */
short __cdecl ActionCmd_ThemeVol(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044cdf0 */
short __cdecl ActionCmd_ThemePan(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044cef0 */
short __cdecl ActionCmd_SoundPan(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044cff0 */
short __cdecl ActionCmd_SoundLoop(int pCtx, int pArgBlock, int *pTable, int *pSubSkip);  /* 0x0044d0c0 */
short ScriptCmd_SoundRate(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d170 */
short ScriptCmd_ThemeVol(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d1c0 */
short ScriptCmd_ThemePan(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d260 */
short ScriptCmd_SoundVol(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d300 */
short ScriptCmd_SoundPan(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d360 */
short ScriptCmd_SoundLoop(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d3c0 */
short ScriptCmd_SoundDone(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d420 */
short __cdecl ScriptCmd_VoiceDone(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d450 */
short ScriptCmd_CountSounds(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d480 */
short ScriptCmd_IndexToSound(int *pCtx, int pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d500 */
short ScriptCmd_CountTracks(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d5b0 */
short ScriptCmd_IndexToTrack(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d5e0 */
short ScriptCmd_CurrentSound(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d690 */
short ScriptCmd_CurrentTheme(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d750 */
short ScriptCmd_CurrentVoice(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount);  /* 0x0044d820 */
short __cdecl ResolveNodeFromScript(int pCtx, short *pArgBlock, int *pTable, int *pSkipCount, int *pNodeOut);  /* 0x0044d860 */
short __cdecl FindNodeByDescriptor(byte *pName, uint **ppEntry);  /* 0x0044d8c0 */
short __cdecl FindTrackByName(byte *pName, int *pOut);  /* 0x0044d9f0 */
short __cdecl FindTrackIndexByNode(int entryKey, int *pIdx);  /* 0x0044da70 */
short __cdecl CopyTrackNameByNode(int nodeKey, byte *pOut);  /* 0x0044dad0 */
void __stdcall AssignSoundNodeTrackIndices(void);  /* 0x0044db40 */
void __stdcall TeardownFinishedVoiceNodes(void);  /* 0x0044dbd0 */
void __stdcall InitGlobalsScope(void);  /* 0x0044dc70 */
void __stdcall TeardownGlobalsScope(void);  /* 0x0044dc80 */
int *__cdecl AllocMethodExecutionScopeResource(void);  /* 0x0044dc90 */
short __cdecl ReadPropertyValue(int *pObj, short tag, short *pResult);  /* 0x0044dcf0 */
short __cdecl ReadPropertyNameByTag(int *hProps, short tag, short *pOutValue);  /* 0x0044ddb0 */
short __cdecl WritePropertyValue(int *hProps, short tag, short *pValue);  /* 0x0044de40 */
short __cdecl DeclarePropertyFromBytecodeInstr(int *pScope, byte *pName, short *pTagOut, int instrAddr);  /* 0x0044df40 */
short __cdecl UndeclarePropertyForInstr(int *pScope, byte *pName, int instrAddr);  /* 0x0044e040 */
short __cdecl LookupPropertyByName(int *pObj, byte *pName, short *pTagOut, int instrAddr);  /* 0x0044e120 */
void __cdecl UninstallResourcePair(void *pHandle);  /* 0x0044e1f0 */
void __cdecl CopyPropertyHeapStringToBuffer(int propTableDataAsInt, int heapByteOffset, byte *pDst);  /* 0x0044e220 */
int __cdecl InternPascalStringInPropertyHeap(int propTableDataAsInt, byte *pSrc);  /* 0x0044e270 */
void __cdecl CompactPropertyStringHeap(short *pTable);  /* 0x0044e2e0 */
void CALLBACK WaveOutCallback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);  /* 0x00406ed0 */
int __cdecl IsPointerReadable(const void *p, size_t size);  /* port helper */
UINT_PTR __stdcall OpenFileNameModalHook(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);  /* port helper */
void __cdecl RestorePaletteAfterModalCover(void);  /* port helper */
void __cdecl RestoreSavedWindowPositions(void);  /* port helper */
#if DF_FIX_WAVE_RING_LOCK
void WaveRingLockDelete(void);  /* port helper */
void WaveRingLockInit(void);  /* port helper */
#endif
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR     lpCmdLine, int       nShowCmd);  /* port helper */
LRESULT CALLBACK WndProc_DFCOVER(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);  /* port helper */
LRESULT CALLBACK WndProc_DFWINDOW(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);  /* port helper */

#endif
