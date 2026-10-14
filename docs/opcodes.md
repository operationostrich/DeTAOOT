# Script Opcode Reference

Flat mapping of DreamFactory script opcodes to their C handler functions in `src/scriptcmd.c` (action/query/compute commands) and `src/scriptprop.c` (property pipeline).

For binary wire format and low-level opcodes (literals, operators, control flow) see [script-format.md](script-format.md).

This page is the **decode-status** view: which handler owns each opcode and whether its
body is trusted. For what a command *means* to a script -- signature, parameters,
return value, the state it writes, and how the game's own 845 scripts call it -- see
the game's own scripts, which DFET (a third-party extractor) reads as source.

**Decode status legend:** Decoded = full body in src/. Stub = empty or placeholder. TBD = not yet located.

## Where the command names come from

Every command name on this page is read out of `TI.EXE`, not inferred. The
engine carries its own command table as data — a run of 6-byte
`{uint32 nameVA, uint16 commandID}` records in `.data` (file offsets
`0x5b990`–`0x5c14c`), each pointing at the command's source-text name. 354 IDs
recover from it. The table was extracted from the shipped binary, and this page's join to the C was
re-checked against the engine's jump tables.

The join to our C is verified the same way rather than assumed.
`DispatchActionCommand` dispatches through two jump tables of handler pointers
(`0x0043d420`, 110 entries for 12001–12110; `0x0043d5d8`, 55 entries for the
16xxx setters). Each entry points at a thunk that marshals four arguments and
CALLs the real handler, so the handler address is the first `E8 rel32` in the
thunk. Following all 165 gives opcode → handler address from the binary, and
**all 165 match our dispatch switch and address tags exactly**. The chain
opcode → name → address → our function therefore contains no inference. The
tool re-runs that check; a mismatch means the switch has drifted from `TI.EXE`
and every name downstream of it is suspect.

## 16xxx is one band, dispatched twice

A 16xxx opcode appears in **both** dispatchers, and that is not a duplication
in our decode — it is the language's arity rule. `propview(me)` reads,
`propview(me, "x")` writes. `DispatchScriptCommand` holds the getter half and
`DispatchActionCommand` the setter half, under the same command ID. The
"Extended Action Commands (16xxx)" and "Query Commands (16xxx)" sections below
are those two halves.

---

## Action Commands (12xxx) — `DispatchActionCommand`

Dispatched by `DispatchActionCommand` (`0x0043c2c0`). Opcodes 12001–12110 (basic range) and extended opcodes in the 16xxx slot (see Extended Actions below). All are void — they perform side effects and do not return a value to the script.

| Opcode | Keyword / Handler | Decode Status | Notes |
|---|---|---|---|
| 12001 | `ActionCmd_Message` (`0x0043d890`) | Decoded | Evaluates one arg → `TakeScriptString` → `DAT_0048ac40` |
| 12002 | `ActionCmd_HideCursor` (`0x0043da30`) | Decoded | Calls `QdHideCursor`; no args |
| 12003 | `ActionCmd_ShowCursor` (`0x0043e090`) | Decoded | Calls `QdShowCursor`; no args |
| 12004 | `ActionCmd_Delay` (`0x0043e220`) | Decoded | One int arg → `SleepTicks(value)` |
| 12005 | `ActionCmd_MakeLoop` (`0x004424e0`) | Decoded | 3 args → descriptor/name validation → `InstallLoopEntry` |
| 12006 | `ActionCmd_WalkToStar` (`0x00442a90`) | Decoded | `EvalTwoScriptArgs` → `LookupActorEntry` → `BuildWalkToStarEntry` or `BuildWalkOnPathEntry` |
| 12007 | `ActionCmd_MakeCricket` (`0x00443cc0`) | Decoded | Two-pass `EvalThreeScriptArgs`; builds spatial-object entry → `InstallCricketEntry` |
| 12009 | `ActionCmd_VisualEffect` (`0x0043da50`) | Decoded | Cue ID 0x5DC1–0x5DD5; one int arg 1–1000; → `BumpSceneOverlayModeFloor(2)` + dispatch |
| 12010 | `ActionCmd_StopWalk` (`0x0043e310`) | Decoded | One arg → `RemoveWalkEntry` |
| 12011 | `ActionCmd_StopLoop` (`0x0043e370`) | Decoded | Two args → `RemoveLoopEntry` |
| 12012 | `ActionCmd_StopCricket` (`0x0043e3f0`) | Decoded | One arg → `RemoveCricketEntry` |
| 12013 | `ActionCmd_OpenCastFile` (`0x0040d9a0`) | Decoded | arg → `OpenCastNode` → slot-1 table append |
| 12014 | `ActionCmd_CloseCastFile` (`0x0040f990`) | Decoded | arg → `LookupCastEntryByName` → teardown + compact |
| 12015 | `ActionCmd_ActorScript` (`0x0040fdd0`) | Decoded | arg → `LookupActorEntry` → prepend separator → `AppendPascalString` |
| 12016 | `ActionCmd_SendToActor` (`0x00410010`) | Decoded | → `ScriptCmd_SendToActorFx` (see also opcode 20084) |
| 12017 | `ActionCmd_PlayMovie` (`0x0043e5c0`) | Decoded | arg → `PlayMovie` (`reset_channel.c`) |
| 12018 | `ActionCmd_OpenPuppetFile` (`0x0043ea00`) | Decoded | guard DAT_0048a7f8; arg → `OpenPuppetNode` → `SendOpenPuppetMessage` |
| 12032 | `OpenSetNode` (`0x00407780`) | Decoded | `opensetfile`; attach playfield on DAT_0048a780; fills state shorts/ints |
| 12033 | `SetPreShutdownStep` (`0x00407fe0`) | Decoded | `closesetfile`; build scratch + `RunSceneMessage` |
| 12066 | `ActionCmd_SendToPainting` (`0x00409500`) | Decoded | Thin wrapper to `ScriptCmd_SendToPaintingFx` |
| 12068 | `ActionCmd_SendToButton` (`0x00446010`) | Decoded | Thin wrapper to `ScriptCmd_SendToButtonFx` |
| 12071 | `quit` | TBD | Not yet located in scriptcmd.c |
| 12082 | `ActionCmd_MixClut` (`0x0043dbc0`) | Decoded | Two CLUT args + three ints; blend 8 bytes/entry → `ReloadCLUT` |
| 12100 | `ActionCmd_WalkOnPath` (`0x00442cc0`) | Decoded | `EvalThreeScriptArgs` + two descriptors → `BuildWalkOnPathEntry` or mismatch entry |
| 12101 | `ActionCmd_WalkToXyz` (`0x00442bc0`) | Decoded | `EvalFourScriptArgs` → three type-4 coords → `BuildWalkToXyzEntry` |
| 12102 | `ActionCmd_WalkOnRoad` (`0x00442e20`) | Decoded | Same shape as 12100 → `BuildWalkOnRoadEntry` |
| 12103 | `ActionCmd_WalkOnFrames` (`0x00442f80`) | Decoded | Same shape as 12100 → `BuildWalkOnFramesEntry` |
| 12104 | `ActionCmd_ActorHide` (`0x0040eff0`) | Decoded | `EvalTwoScriptArgs` → `LookupActorEntry`; adjusts ref short |
| 12105 | `ActionCmd_PropHide` (`0x00417060`) | Decoded | Like 12104 but `LookupPropEntry`; slot-2 state table |
| 12106 | `ActionCmd_Reboot` (`0x0042b2a0`) | Decoded | Sets `DAT_0048a990=2` (restart outer game loop) |
| 12107 | `ActionCmd_ActorLock` (`0x0040ed20`) | Decoded | `EvalTwo` + `LookupActorEntry`; clear or set child binding |
| 12108 | `ActionCmd_PropLock` (`0x00416d70`) | Decoded | `EvalTwo` + `LookupPropEntry`; release or register slot-2 binding |
| 12109 | `ActionCmd_ShopLock` (`0x00416e20`) | Decoded | `EvalTwo` + `FUN_00418a60`; walk DAT_0048a734 matching key |
| 12110 | `ActionCmd_CastLock` (`0x0040edd0`) | Decoded | `EvalTwo` + `LookupCastEntryByName`; clear or set |

---

## Extended Action Commands (16xxx dispatch via DispatchActionCommand)

These 16xxx opcodes are handled by `DispatchActionCommand` — they are **not** the same as the 16xxx query commands dispatched by `DispatchScriptCommand`.

| Opcode | Handler | Notes |
|---|---|---|
| Ext 16002 | `ActionCmd_ActorDeg` (`0x0040e6d0`) | Patch `named+0x88` low byte |
| Ext 16003 | `ActionCmd_ActorXyz` (`0x0040f140`) | `EvalFourScriptArgs` + `RemoveWalkEntry` |
| Ext 16004 | `ActionCmd_ActorXy` (`0x0040f510`) | `EvalThreeScriptArgs` + `RemoveWalkEntry` |
| Ext 16005 | `ActionCmd_ActorIs3d` (`0x0040f6f0`) | Clear `named+0x78` if second arg == 0 |
| Ext 16006 | `ActionCmd_ActorStar` (`0x0040e390`) | `FindWaypointNodePosition` → pack coords to `named+0x84/+0x88` |
| Ext 16007 | `ActionCmd_SetVisible` (`0x00408860`) | Guard DAT_0048a778; word → DAT_0048a77a; `BumpSceneOverlayModeFloor(1)` |
| Ext 16008 | `ActionCmd_StageVisible` (`0x00445a10`) | Guard DAT_0048a74c; navigate page or clear viewport |
| Ext 16009 | `ActionCmd_Path` (`0x0043d8f0`) | Second arg = slot index 1–8 → `SetSearchPathSlot` |
| Ext 16010 | `ActionCmd_Result` (`0x0043d9f0`) | → `DAT_0048a890` |
| Ext 16011 | `ActionCmd_CurrentView` (`0x004088d0`) | Guard DAT_0048a778; trigger dispatch path |
| Ext 16012 | `ActionCmd_ActorDist` (`0x0040e770`) | Patch `named+0x7a` |
| Ext 16013 | `ActionCmd_PropDist` (`0x004167c0`) | Patch `state+0x7a` |
| Ext 16014 | `ActionCmd_ActorPose` (`0x0040e550`) | `named+0x7e = named+0x7c - 1` |
| Ext 16015 | `ActionCmd_PropVisible` (`0x00416f00`) | First short of state row → 0 or 1 |
| Ext 16016 | `ActionCmd_PropDeg` (`0x00416720`) | Low byte → `state+0x88` |
| Ext 16017 | `ActionCmd_PropXyz` (`0x00417340`) | `EvalFourScriptArgs`; `state+0x8E=1` |
| Ext 16018 | `ActionCmd_PropXy` (`0x00417570`) | `state+0x7a` → −1; `state+0x8E=0` |
| Ext 16019 | `ActionCmd_PropIs3d` (`0x00417740`) | If second==0 and `state+0x7a > −1` → `−1` |
| Ext 16020 | `ActionCmd_PropStar` (`0x004163f0`) | `LookupPropEntry`; coords to `+0x84/+0x88` |
| Ext 16021 | (dup of 16006) | Reuses `ActionCmd_ActorStar` |
| Ext 16022 | `ActionCmd_Framerate` (`0x0043def0`) | type 4 → DAT_0048a71e clamped 0..0x3c |
| Ext 16023 | `ActionCmd_ActorSpeed` (`0x0040e970`) | `named+0x7a` = max(value, 1) |
| Ext 16024 | `ActionCmd_ActorScale` (`0x0040ea20`) | `named+0x76` = max(value, 0) |
| Ext 16025 | `ActionCmd_PropView` (`0x004165a0`) | `state+0x80 = state+0x7e - 1` |
| Ext 16026 | `ActionCmd_PropSpeed` (`0x004169c0`) | `state+0x7c` = max(value, 1) |
| Ext 16027 | (dup of 16020) | Reuses `ActionCmd_PropStar` |
| Ext 16028 | `ActionCmd_PropScale` (`0x00416a70`) | `state+0x78` = max(value, 0) |
| Ext 16029 | `ActionCmd_CurrentScene` (`0x00407c00`) | Guard DAT_0048a778; trigger dispatch or property |
| Ext 16030 | `ActionCmd_Variable` (`0x0043e620`) | `EvalTwo` + `LookupPropertyByName` → `WritePropertyValue` |
| Ext 16031 | `ActionCmd_CurrentDeg` (`0x00408a20`) | type 4 → `FindViewNameNearestAngle` + trigger dispatch |
| Ext 16032 | `ActionCmd_PropOwner` (`0x00415f40`) | Copy second name to scratch |
| Ext 16033 | `ActionCmd_WaveVolume` (`0x0043d980`) | type 4 slot index 0–9 → `RunSlotIndexScriptPollIfActive` |
| Ext 16034 | `ActionCmd_CurrentCd` (`0x0042b2c0`) | → `ValidateCurrentCdName` / `SetCurrentCdName` |
| Ext 16035 | `ActionCmd_CameraHi` (`0x0043d7e0`) | type 4 → DAT_0048a792 low 16 bits |
| Ext 16036 | `ActionCmd_ActorTurn` (`0x004108b0`) | `named+0x80` = max(value, 1) |
| Ext 16037 | `ActionCmd_MenuVisible` (`0x0043d6c0`) | type 2: non-zero → `RestoreMenuBar`; zero → `HideMenuBar` |
| Ext 16038 | `ActionCmd_SoundPan` (`0x0044cff0`) | descriptor → node; second arg → `SetSoundNodePan` |
| Ext 16039 | `ActionCmd_SoundLoop` (`0x0044d0c0`) | descriptor → node; second arg → `SetSoundNodeLoop` |
| Ext 16040 | `ActionCmd_SoundVol` (`0x0044cd20`) | descriptor → node; second arg → `SetSoundNodeVolume` |
| Ext 16041 | `ActionCmd_ThemeVol` (`0x0044cdf0`) | `FindTrackByName`; two ptr/count pairs → `SetSoundNodeVolume` each |
| Ext 16042 | `ActionCmd_PropValue` (`0x00416000`) | `LookupPropEntry`; commit |
| Ext 16043 | `ActionCmd_ActorOwner` (`0x004109f0`) | ≤15-char Pascal → `named+144` |

---

## Query Commands (16xxx) — `DispatchScriptCommand`

Single-value getters dispatched by `DispatchScriptCommand`. Results are returned to the script. Decoded bodies are in `src/scriptquery.c`.

| Opcode | Handler (`scriptquery.c` unless noted) | Notes |
|---|---|---|
| 16011 | `ScriptCmd_ActorStar` | Opcode 16011; resource string at data byte 0 |
| 16017 | `ScriptCmd_PropXyz` | Reads descriptor + int index 1–4 via `EvalTwoScriptArgs` + `FUN_00418900` |
| 16018 | `ScriptCmd_PropXy` | Reads descriptor + int sub-index 1–3; returns hi/lo/full word |
| 16019 | `ScriptCmd_PropIs3d` | → `dataBuffer[9]` as type-2 short |
| 16020 | `ScriptCmd_PropStar` | → Pascal string at data byte 108 |
| 16021 | `ScriptCmd_ActorSet` | → Pascal string at data byte 96 |
| 16023 | `ScriptCmd_ActorSpeed` | → `dataBuffer[19]` as type-4 int |
| 16024 | `ScriptCmd_ActorScale` | → 32-bit value at data byte 42 |
| 16025 | `ScriptCmd_PropView` | → Pascal string at data byte 124 |
| 16026 | `ScriptCmd_PropSpeed` | → `dataBuffer[18]` as type-4 int |
| 16027 | `ScriptCmd_PropSet` | → Pascal string at data byte 94 |
| 16028 | `ScriptCmd_PropScale` | → dword at data byte 40 |
| 16030 | `ScriptCmd_Variable` | Reads descriptor → `LookupPropertyByName` + `ReadPropertyValue` |
| 16032 | `ScriptCmd_PropOwner` | → Pascal string at data byte 142 |
| 16034 | — | `currentcd` — TBD |
| 16036 | `ScriptCmd_ActorTurn` | → `dataBuffer[16]` as type-4 int |
| 16038 | `ScriptCmd_SoundPan` | `ResolveNodeFromScript` → `FUN_00425190` result |
| 16039 | `ScriptCmd_SoundLoop` | `ResolveNodeFromScript` → `FUN_00425290` bool |
| 16040 | `ScriptCmd_SoundVol` | `ResolveNodeFromScript` → `FUN_00425090` result |
| 16041 | `ScriptCmd_ThemeVol` | descriptor → `FUN_0044d9f0`; if loaded → `FUN_00425090` |
| 16042 | `ScriptCmd_PropValue` | → dword at data byte 68 |
| 16043 | `ScriptCmd_ActorOwner` | → Pascal string at data byte 144 |
| 16044 | `ScriptCmd_ActorValue` | → dword at data byte 72 |
| 16051 | `ScriptCmd_ActorZclip` | → short at data byte 76 |
| 16052 | `ScriptCmd_PropZclip` | → short at data byte 74 |
| 16054 | `ScriptCmd_ThemePan` | descriptor → `FindTrackByName`; if loaded → `FUN_00425190` |

---

## Computed-Value Commands (20xxx) — `DispatchScriptCommand`

Return a computed result to the script. Decoded bodies are primarily in `src/scriptcmd.c`.

| Opcode | Handler | Notes |
|---|---|---|
| 20018 | `ScriptCmd_CountPaintings` | `EvalTwoScriptArgs` + `LookupViewRow`; optional child attach; first dword of child resource |
| 20019 | `ScriptCmd_CountScenes` | Guard DAT_0048a778; DAT_0048a7f0 as type-4 |
| 20020 | `ScriptCmd_IndexToScene` | One type-4 row index; DAT_0048a7f4 row (0x2a bytes) → `AllocScriptString` |
| 20022 | `ScriptCmd_IndexToPainting` | `EvalThreeArgs`; indexed row (9 ints) → string |
| 20025 | `ScriptCmd_StringToNum` | Leading decimal int from Pascal descriptor name |
| 20026 | `ScriptCmd_NumToString` | type-4 → decimal Pascal string |
| 20027 | `ScriptCmd_FreeMem` | `GetHeapFreeBytes` as type-4 |
| 20028 | `ScriptCmd_PuppetEvent` | Requires DAT_0048a7f8+7fa; clears/appends path table; copies 0x146 dwords |
| 20029 | `ScriptCmd_CountCasts` | DAT_0048a730 (slot-1 count) as type-4 |
| 20030 | `ScriptCmd_IndexToCast` | 1-based index → slot-1 Pascal name at [3] |
| 20031 | `ScriptCmd_CountProps` | DAT_0048a738 as type-4 |
| 20032 | `ScriptCmd_IndexToProp` | slot-2 row (0x9E bytes); slot-3 name + state name composite |
| 20033 | `ScriptCmd_CountShops` | DAT_0048a740 as type-4 |
| 20034 | `ScriptCmd_IndexToShop` | slot-3 row (7 ints); Pascal name at [3] |
| 20035 | `ScriptCmd_CountFlats` | Music guard; flat buffer count |
| 20036 | `ScriptCmd_IndexToFlat` | 1-based index → 0x2e-byte row → Pascal at +0x1e |
| 20037 | `ScriptCmd_FlatToIndex` | descriptor → `FlatPageNameToIndex` + 1 |
| 20038 | `ScriptCmd_CurrentFlat` | Current flat row → `AllocScriptString` on +0x1e |
| 20039 | `ScriptCmd_PointInActor` | `LookupActorEntry` → `CalcActorRenderParams` → hotspot probes |
| 20040 | `ScriptCmd_PointInProp` | `LookupPropEntry` → `CalcPropRenderParams` → hotspot probe |
| 20041 | `ScriptCmd_PointInSet` | Sound AVI active check; `CopyTrigOverlayBufferAndViewportPair` + `PointInSceneRect` |
| 20042 | `ScriptCmd_PointInStage` | Music guard; `CopyTrigSceneBufferAndViewportPair` + `PointInSceneRect` |
| 20043 | `ScriptCmd_PointInButton` | Stage guard; `LookupFlatEntry` + `LookupButtonEntry` vs button rect |
| 20044 | `ScriptCmd_CountButtons` | `LookupFlatEntry`; flat child dword at GetResDataPtr(child)+0x404 |
| 20045 | `ScriptCmd_IndexToButton` | `EvalTwoScriptArgs`; 1-based button row → 0x20 bytes → `AllocScriptString` |
| 20046 | `ScriptCmd_CountPuppets` | Guard DAT_0048a7f8; copies 6 dwords from DAT_0048a808 header; low short of [5] |
| 20047 | `ScriptCmd_IndexToPuppet` | Guard; 1-based row → 40-byte row → `AllocScriptString` |
| 20048 | `ScriptCmd_CurrentStage` | Music/stage active → `AllocScriptString(DAT_0048a768)` + normalize |
| 20049 | `ScriptCmd_CurrentPuppet` | DAT_0048a7f8 → `AllocScriptString(DAT_0048a81c)` + normalize |
| 20053 | `ScriptCmd_CurrentSet` | DAT_0048a778 → `AllocScriptString(DAT_0048a7b4)` + normalize |
| 20054 | `ScriptCmd_FindWord` | `EvalThreeScriptArgs`; `CompareRawByteSlicesCaseFolded` occurrence scan → `AllocScriptString` |
| 20055 | `ScriptCmd_SubString` | `EvalTwoScriptArgs` + two descriptors; first match → 1-based index, or -1 |
| 20056 | `ScriptCmd_StringLength` | `TakeScriptString` → first 4 bytes of DAT_0048a148 |
| 20057 | `ScriptCmd_PutWord` | `EvalFourScriptArgs`; K-th needle replacement in haystack |
| 20058 | `ScriptCmd_OptionKey` | `IsMultimediaSequentialModifierKeyDown` (Shift) |
| 20059 | `ScriptCmd_ShiftKey` | `IsMusicSequentialModifierKeyDown` (Shift) |
| 20060 | `ScriptCmd_CommandKey` | `IsSoundSequentialModifierKeyDown` (Ctrl) |
| 20061 | `ScriptCmd_CalcVectX` | `EvalTwoScriptArgs` → `AngleDistToDeltaX` |
| 20062 | `ScriptCmd_CalcVectY` | `EvalTwoScriptArgs` → `AngleDistToDeltaY` |
| 20063 | `ScriptCmd_CameraXyz` | Guard DAT_0048a778; selector 1–4 → scroll pair |
| 20064 | `ScriptCmd_PlayerXyz` | Guard; selector maps DAT_0048a7a6/a8/aa |
| 20070 | `ScriptCmd_HitTest` | `FindPaintingBindingKeyAtPoint` → entry or viewport descriptor |
| 20071 | `ScriptCmd_CalcDeg` | `EvalTwoScriptArgs` → `ComputeAngleBetweenPoints` |
| 20072 | `ScriptCmd_CalcTurn` | `EvalThreeScriptArgs` → `StepValueTowardTarget` with 0xff modulus |
| 20073 | `ScriptCmd_StarXyz` | `EvalTwoScriptArgs` → `FindWaypointNodePosition`; selector picks coord |
| 20074 | `ScriptCmd_Frame` | DAT_0048a71a tick counter as type-4 |
| 20076 | `ScriptCmd_IndexToTrack` | 1-based index into DAT_0048a744 (stride 0x28) → `AllocScriptString` from +0x1e |
| 20077 | `ScriptCmd_CurrentSound` | type-4 opcode 1/2/3 → FUN_00424a50/ad0/b50 + `AllocScriptString` |
| 20078 | `ScriptCmd_CurrentVoice` | `GetPlayingVoiceName` + `AllocScriptString` |
| 20079 | `ScriptCmd_CurrentTheme` | Variant 1/2 → `GetPlayingThemeName` |
| 20080 | `ScriptCmd_SoundRate` | `FindNodeByDescriptor` → `*(uint *)(*(*(node+0x1c))+0x1c)` |
| 20081 | `ScriptCmd_CalcDist` | `EvalTwoScriptArgs`; `IntSqrt` on squared XY deltas |
| 20082 | `ScriptCmd_CalcMod` | `EvalTwoScriptArgs`; both type-4 → `a % b` |
| 20084 | `ScriptCmd_SendToActorFx` | `LookupActorEntry` + `LookupCastEntryByNode`; triple `EvalMethodCall` (also dispatched by 12016) |
| 20096 | `ScriptCmd_SceneXyz` | `EvalTwoScriptArgs` + `FindSceneRowByName`; selector 1–4 → short X/Y/Z or packed XY |
| 20097 | `ScriptCmd_VoiceDone` | `FUN_00424850` → type-2 bool |
| 20099 | `ScriptCmd_WalkDest` | `TakeScriptString` + `CopyWalkDestName` → `AllocScriptString` |
| 20103 | `ScriptCmd_IndexToWalk` | `CountWalks` + 1-based index → `AllocScriptString` |
| 20107 | `ScriptCmd_CountBevels` | Guard DAT_0048a7f8; DAT_0048af08 as type-4 |
| 20108 | `ScriptCmd_Sqrt` | uint → `IntSqrt` (low 16 bits) as type-4 |
| 20109 | `ScriptCmd_PluginExists` | `LookupPluginByDescriptor` → type-2 bool |
| 20110 | `ScriptCmd_NetworkOn` | DAT_00460da0 as type-2 |
| 20111 | `ScriptCmd_NetCountMembers` | DAT_00460da0 guard; `FUN_0040ba60` prefix |
| 20112 | `ScriptCmd_NetIndexToMember` | `FUN_0040b9e0` + prefix DAT_0045b218 |
| 20113 | `ScriptCmd_NetOurId` | DAT_00460da4 as type-4 |
| 20114 | `ScriptCmd_NetMessageSender` | `NetPluginCallStr(DAT_0045b314)` |
| 20115 | `ScriptCmd_NetMessageGroup` | `FUN_0040b940(DAT_0045b328)` → `AllocScriptString` |
| 20116 | `ScriptCmd_RoadAhead` | `EvalTwoScriptArgs` + `LookupViewRow` + `ResolveWalkPathSelectorForView` |
| 20117 | `ScriptCmd_StringWidth` | `EvalThreeScriptArgs` + `QdStringWidth` |
| 20118 | `ScriptCmd_CountViews` | `FindSceneRowByName` + playfield +0x30 row count |
| 20119 | `ScriptCmd_IndexToView` | `EvalTwoScriptArgs`; 1-based row ×0x2e copy → `AllocScriptString` |
| 20120 | `ScriptCmd_SysMem` | `FUN_0041ebe0` SIZE_T as type-4 |
| 20121 | `ScriptCmd_HeapSize` | `*(GetHeapDescPtr()+0xc)` as type-4 |

---

## Transition / Effect Commands (24xxx)

| Opcode | Keyword | Notes |
|---|---|---|
| 24001 | `barndoorclose` | TBD |
| 24002 | `barndooropen` | TBD |
| 24003 | `irisclose` | TBD |
| 24004 | `irisopen` | TBD |
| 24005–24008 | `scroll*` (left/right/up/down) | TBD |
| 24019 | `nodraw` | TBD |

---

## Helper Functions (scriptcmd.c internal)

Not script-callable directly; called by the handlers above.

| Address | Name | Notes |
|---|---|---|
| 0x0040acc0 | `LookupViewRow` | Parse pDescA → child selector; scan 0x2e-byte rows for pDescB match; memcpy 46 bytes |
| 0x0040aae0 | `SceneHasViewNamed` | `FindSceneRowByName` + first-child row scan |
| 0x0040abc0 | `FindViewNameNearestAngle` | Best `FUN_004448d0` angle match into pOutBuf |
| 0x00407ec0 | `SendCloseSceneMessage` | Build DAT_0048a148 from paths + DAT_0048a7c4 → `RunSceneMessage` |
| 0x00407e50 | `SendOpenSceneMessage` | Build DAT_0048a148 + DAT_0048a7c4 → `RunSceneMessage` |
| 0x00439790 | `StartSetMove` | Trigger setup; mode==3 → `StartSetMoveStraight`; `ShowSetFrame` |
| 0x00439a60 | `GetStraightMoveForCurrentView` | Trigger row resolve + return value |
| 0x0043a740 | `FindPaintingBindingKeyAtPoint` | Rebuild render state; traverse DAT_0048ae70; hotspot probe; → bindingKey |
| 0x00439570 | `CommitSetViewport` | Build g_trigViewport; `ComputeAlignedRectSize`; set g_trigBufOverlay |
| 0x00441130 | `RunPuppetEventModalLoop` | The `puppetevent` (20028) modal loop: bevel hotspots + idle tracks; returns the picked row's value, -1 interrupted, -2 timeout |
| 0x00441090 | `DetachPuppetFrames` | Detach children on DAT_0048a7fc playlist rows |
| 0x0042b340 | `ValidateCurrentCdName` | Pascal name validate + conflict scan |
| 0x0042b610 | `SetCurrentCdName` | Walk render nodes; copy to DAT_0048a358 |
| 0x0042b730 | `TruncatePathToVolume` | Placeholder normalization (stub) |
| 0x0042b780 | `TruncatePathToLeaf` | Forwards to `TruncatePathToVolume` |
| 0x0040b8d0 | `NetPluginCallStr` | `InvokeEntryCallback(0x00460da8)` |
| 0x00411760 | `SetActorFrameCountForPose` | Named resource row scan; patch `named+0x24` |
| 0x004192e0 | `SetPropFrameCountForPose` | State row scan + `ReadChildUshortAt0x70`; patch `state+0x22` |
| 0x004377c0 | `GetGlobalRenderNodeListHead` | `*ppHead = g_renderNodeList` |
