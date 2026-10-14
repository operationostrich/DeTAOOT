/* save_props_shops.c -- TI.EXE 0x00413830..0x00419830: savegame / opengame, the shop file
 * and its props (openshopfile and the prop* commands), and the prop-row tables.
 * Mirrors cast_actors.c's cast / actor family one to one: OpenShopNode <->
 * OpenCastNode, InitPropEntry <-> InitActorEntry, CompleteShopOpen <-> CompleteCastOpen, and so
 * on. Prop rows are 0x9e bytes, two bytes shorter than actor rows, so their field offsets sit two
 * lower (set name +0x5e, star name +0x6e, name +0x4e). */

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stddef.h>
#include <mmsystem.h>
#include "df_protos.h"
#include "topics/scriptcmd.h"
#include "topics/scene.h"
#include "topics/scriptquery.h"
#include "topics/hotspot_raster.h"
#include "topics/renderer.h"
#include "topics/sound.h"

static short __cdecl state_transition_20840(byte *bootRelOut, byte *pascal31FileOut);
void __cdecl WriteSaveGameFile(byte *containerPathDesc, byte *auxNamePascal);

/* the declarations the prop-row tables need */
#include "topics/scriptprop.h"
#if DF_TRACE_ENABLED
/* PORT-HELPER: trace filter, true for the three prop rows (life, light, navarrow) whose draw
 * state the traces follow. */
static int DfTraceIsWatchedRow(const byte *nm)
{
    return (nm[0] == 4 && memcmp(nm + 1, "life", 4) == 0)
        || (nm[0] == 5 && memcmp(nm + 1, "light", 5) == 0)
        || (nm[0] == 8 && memcmp(nm + 1, "navarrow", 8) == 0);
}
#endif


#include "topics/savestate.h"

/* Script command savegame (opcode 12077). Evaluates one argument, the save entry's display name,
 * into a local buffer, seeds the Save As dialog with the default filename (kStrSaveGame), shows it
 * (PickSaveFileName, a GetSaveFileName wrapper: non-zero means a file was chosen) and then writes
 * the container with WriteSaveGameFile. Returns 0 on both paths; 0x2d while a puppet is active. */
short __cdecl
ActionCmd_SaveGame(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip) /* 0x00413830 */
{
    short   u;
    /* Frame is 0x108 = 8 + 256: the evaluated argument at ESP+0 and a 256-byte
     * descriptor buffer at ESP+8. */
    short  evaluated[4];    /* ESP+0x00 */
    byte   nameBuf[256];    /* ESP+0x08 */

    if (gPuppet.active != 0)
        return 0x2d;
    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)evaluated, pSubSkip);
    if (u != 0)
        return u;

    /* 0x0041387d: the evaluated argument is resolved into the LOCAL buffer at ESP+8, the save
     * entry display name ("Titanic 1.0" in the menu script), and that buffer, not gScratchStr2,
     * is what the transition body receives. The port skipped this call. */
    DF_TRACE("savegame: evaluated type=%d, resolving descriptor", (int)evaluated[0]);
    u = TakeScriptString(evaluated, nameBuf);
    DF_TRACE("savegame: descriptor -> err=%d name='%.*s'", (int)u,
             (int)nameBuf[0], (const char *)nameBuf + 1);
    if (u != 0)
        return u;

    /* 0x00413894: seed the Save As dialog with its default filename. Also skipped by the port. */
    CopyPascalString((byte *)UpdatePascalLength((char *)kStrSaveGame), gScratchStr2);
    DF_TRACE("savegame: default filename seeded = '%.*s'",
             (int)gScratchStr2[0], (const char *)gScratchStr2 + 1);

    /* TEST AX,AX / JZ (0x004138c1): the picker is a GetSaveFileName wrapper, so NON-zero means
     * the user chose a file and the save proceeds. The port had it inverted: it saved only on
     * cancel, running the whole container write against a path the picker had never filled in
     * (the same defect was fixed in ActionCmd_OpenGame; the save twin was missed). */
    DF_TRACE("savegame: opening Save As dialog");
    u = PickSaveFileName((char *)gScratchStr,
                                            (char *)gScratchStr2);
    DF_TRACE("savegame: picker -> %d (non-zero = file chosen)", (int)u);
    if (u != 0) {
        DF_TRACE_ENTER();
        WriteSaveGameFile(gScratchStr, nameBuf);
        DF_TRACE_LEAVE();
        DF_TRACE("savegame: container write returned");
    }

    return 0;   /* 0x004138d8: XOR AX,AX -- both paths return success */
}


/* Script command opengame (opcode 12078): prompts for a saved-game file (PickOpenFileName:
 * non-zero means a file was chosen), loads it with ReadSaveGameFile and re-establishes the engine
 * state with RestoreSavedGameState. */
short __cdecl
ActionCmd_OpenGame(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip) /* 0x004138f0 */
{
    short   u;
    short  nameDesc[4];  /* local_218 */
    int    tagFilter[4];  /* local_210 */
    byte   saveName[256];  /* local_200 */
    byte   savePath[256];  /* local_100 */

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)nameDesc, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(nameDesc, (byte *)saveName);
    if (u != 0)
        return u;

    DF_TRACE("opengame: expected save name='%.*s'", (int)saveName[0], (const char *)saveName + 1);
    tagFilter[0] = 0x5254444f; /* resource tag filter */
    /* 0x00413957 pushes a fifth argument (1); the callee (0x00420ec0) never
     * reads it, but the push is part of the call's bytes (cdecl, harmless). */
    u = PickOpenFileName((char *)savePath, (char *)&gScratchStr, 0, tagFilter, 1);
    DF_TRACE("opengame: picker -> %d path='%.*s' file='%.*s'", (int)(short)u,
             (int)savePath[0], (const char *)savePath + 1,
             (int)gScratchStr[0], (const char *)gScratchStr + 1);
    if (u != 0) {          /* non-zero = a file was chosen */
        u = ReadSaveGameFile(savePath, saveName);
        DF_TRACE("opengame: ReadSaveGameFile -> %d (non-zero = refused, see msgbox)", (int)(short)u);
        if (u != 0)
            return 0;
        DF_TRACE_ENTER();
        RestoreSavedGameState();   /* 0x00414b00 -- post-load fixup pass */
        DF_TRACE_LEAVE();
        DF_TRACE("opengame: post-load fixup returned");
    }
    return 0;
}


/* The Save As dialog for ActionCmd_SaveGame: a wrapper over PickSaveFileName. */
static short __cdecl
state_transition_20840(byte *bootRelOut, byte *pascal31FileOut)
{
    return PickSaveFileName((char *)bootRelOut, (char *)pascal31FileOut);
}
/* Creates the save container (BootstrapContainerFile 'ODTR' / 'TRFD'), builds the 0x1310-byte save
 * image (node table, stage name, CD name, the nine descriptors, the CLUT) and writes it, plus clones
 * of the channel block and of every object table (actors, casts, props, shops, tracks ...), as
 * children. Called by ActionCmd_SaveGame.
 *
 * Port note: two parameters. 0x004139a0 does __chkstk(0x1430) and reads its real arguments past
 * the scratch block (in_stack_00001434 / 1438), which is why Ghidra shows stack reads and invents
 * param_1..param_7 for what are really locals (class 2, MSVC stack borrowing). The call site
 * (0x004138c6) pushes exactly two. pSceneNode is a local that OpenContainerNode fills, not a
 * parameter: the port had promoted it to the first parameter, so the caller passed the script
 * context where nothing belonged and shifted both real arguments along by one. */
void __cdecl
WriteSaveGameFile(byte *containerPathDesc,
                                        byte *auxNamePascal) /* 0x004139a0 */
{
    /* Frame (0x1430 bytes after __chkstk, from 0x004139a0's disassembly):
     *   +0x10 hRes  +0x14 pSceneNode  +0x18 sceneWordAux  +0x1c uMeth
     *   +0x20 vHead +0x24 uProp       +0x28 uObj
     *   +0x2c scratch: the 0x104-byte node record {node, name[0x100]} and,
     *         from scratch+0x104, the 0x1310-byte save image header. */
    /* 0x00413c41: the live table handle goes into hRes, CloneResource replaces it with a CLONE,
     * and the CLONE is what gets written (SetupRenderObjectResource uninstalls whatever it
     * writes, so the live table would be destroyed). The port once passed CloneResource a NULL
     * local: GetResObjectSize(NULL) raised fatal 227 on the first table. */
    int           *hRes;
    int           *pSceneNode;   /* filled by OpenContainerNode below */
    /* 0x00413b1d: the template's aux out-value goes into the same frame slot
     * every SetupRenderObjectResource below writes its child id to --
     * one variable, never initialised (both callees only write it). */
    uint32_t       sceneWordAux;
    uint32_t       uMeth;
    void          *vHead;
    uint32_t       uProp;
    uint32_t       uObj;
    unsigned char  scratch[0x1414];
    int           *node;
    byte          *blob;
    int           *pChild;
    uint32_t       size;
    int            off;
    void          *raw;
    short          count;
    short          i;
    short          err;
    typedef struct { uint32_t a, b; } Pair8;

#if DF_FIX_SAVE_SCRATCH_ZEROED
    /* The exe only probes the 0x1430-byte block (__chkstk) and writes the save image
     * into it as it goes: every byte not written is whatever was on the stack. Zeroing
     * it makes the save file deterministic. include/df_fixes.h. */
    memset(scratch, 0, sizeof(scratch));
#endif

    DeleteResourceFile(containerPathDesc);
    err = BootstrapContainerFile(containerPathDesc, 100, 0x5254444fU, 0x44465254U);
    if (err != 0) {
        ShowEngineMessageBoxFromPascalString((byte *)UpdatePascalLength((char *)kStrSaveCreateError));
        return;
    }

    err = OpenContainerNode(containerPathDesc, &pSceneNode);
    if (err != 0)
        FatalError(err, 0x10e0);

    count = 0;
    GetGlobalRenderNodeListHead(&vHead);
    hRes = (int *)AllocResObject(0x1310);
    for (node = (int *)vHead; node != NULL; node = *(int **)((char *)node + 0x540)) {
        count++;
        /* 0x00413a5b: the node pointer and its name form one 0x104-byte record,
         * copied whole below. */
        *(int **)scratch = node;
        CopyPascalString((byte *)node + 0x24, scratch + 4);
        off = (int)count * 0x104;
        ResizeResObject(hRes, off + 0x1310);
        blob = (byte *)GetResDataPtr(hRes);
        memcpy(blob + off + 0x120c, scratch, 0x104);
        UnlockResObject(hRes);
    }

    /* `MOVSX EAX,BX / MOV [ESP+0x143c],EAX` (0x00413ad2): the node count is the last dword of
     * the save image, blob+0x130c, where GetRenderNodeDescriptorBySelector reads the table
     * length on load. The port once dropped the store: every save carried a count of 0 and
     * loading any save fataled 0/0x1127 ("Fatal error at line 4391"). */
    *(int *)(scratch + 0x1410) = count;

    CopyPascalString(auxNamePascal, scratch + 0x104);
    CopyPascalString(gCurrentCdName, scratch + 0x204);
    for (i = 0; i <= 8; i++)
        GetSearchPathSlot(i, scratch + 0x304 + (int)i * 0x100);

    GetPlayingThemeName(gScratchStr, (int *)(scratch + 0xc04), (int *)&sceneWordAux);
    GetPlayingSoundNameByPriority(gScratchStr, (uint32_t *)(scratch + 0xc08),
                                          (uint32_t *)(scratch + 0xc0c));

    blob = (byte *)GetResDataPtr((int *)gActiveClut);
    for (i = 0; i < 0x100; i++) {
        ((Pair8 *)(scratch + 0xc10))[i] = ((Pair8 *)(blob + 8))[i];
    }
    UnlockResObject((int *)gActiveClut);

    raw = (void *)GetResDataPtr(hRes);
    memcpy(raw, scratch + 0x104, 0x1310);
    UnlockResObject(hRes);
    err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
    if (err != 0)
        goto hydrate_fail_cleanup;

    hRes = (int *)AllocResObject(0x312);
    raw = (void *)GetResDataPtr(hRes);
    memcpy(raw, &gChannels, 0x312);
    UnlockResObject(hRes);
    err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
    if (err != 0)
        goto hydrate_fail_cleanup;

    hRes = (int *)gActorTable;
    CloneResource((void **)&hRes);
    err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
    if (err != 0)
        goto hydrate_fail_cleanup;

    hRes = (int *)gCastTable;
    CloneResource((void **)&hRes);
    err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
    if (err != 0)
        goto hydrate_fail_cleanup;

    hRes = (int *)gPropTable;
    CloneResource((void **)&hRes);
    err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
    if (err != 0)
        goto hydrate_fail_cleanup;

    hRes = (int *)gShopTable;
    CloneResource((void **)&hRes);
    err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
    if (err != 0)
        goto hydrate_fail_cleanup;

    hRes = (int *)gTrackTable;
    CloneResource((void **)&hRes);
    err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
    if (err != 0)
        goto hydrate_fail_cleanup;

    i = 0;
    blob = (byte *)GetResDataPtr((int *)gTrackTable);
    for (; i < gTrackCount; i++) {
        size = *(short *)(blob + 4) * 0x68;
        hRes = (int *)AllocResObject(size);
        raw = (void *)GetResDataPtr(hRes);
        _memcpy(raw, *(void **)(blob + 0xa), size);
        UnlockResObject(hRes);
        err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
        if (err != 0)
            goto slot2_fail;

        size = *(short *)(blob + 6) * 0x68;
        hRes = (int *)AllocResObject(size);
        raw = (void *)GetResDataPtr(hRes);
        _memcpy(raw, *(void **)(blob + 0xe), size);
        UnlockResObject(hRes);
        err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
        if (err != 0)
            goto slot2_fail;

        size = *(short *)(blob + 8) * 0x68;
        hRes = (int *)AllocResObject(size);
        raw = (void *)GetResDataPtr(hRes);
        _memcpy(raw, *(void **)(blob + 0x12), size);
        UnlockResObject(hRes);
        err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
        if (err != 0)
            goto slot2_fail;

        blob += 0x28;
    }
    UnlockResObject((int *)gTrackTable);

    hRes = (int *)gGlobalsNode;
    CloneResource((void **)&hRes);
    err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
    if (err != 0)
        goto hydrate_fail_cleanup;

    /* 0x00413ee8 re-reads the ORIGINAL table, not the clone just consumed. */
    hRes = *(int **)((byte *)GetResDataPtr((int *)gGlobalsNode) + 0x10);
    UnlockResObject((int *)gGlobalsNode);
    CloneResource((void **)&hRes);
    err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
    if (err != 0)
        goto hydrate_fail_cleanup;

    GetScriptBindingTableBasePointers(&uProp, &uMeth, &uObj);

    hRes = (int *)AllocResObject(0x540);
    raw = (void *)GetResDataPtr(hRes);
    _memcpy(raw, (void *)uProp, 0x540);
    UnlockResObject(hRes);
    err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
    if (err != 0)
        goto hydrate_fail_cleanup;

    hRes = (int *)AllocResObject(0x4a0);
    raw = (void *)GetResDataPtr(hRes);
    _memcpy(raw, (void *)uObj, 0x4a0);
    UnlockResObject(hRes);
    err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
    if (err != 0)
        goto hydrate_fail_cleanup;

    hRes = (int *)AllocResObject(0x6e0);
    raw = (void *)GetResDataPtr(hRes);
    _memcpy(raw, (void *)uMeth, 0x6e0);
    UnlockResObject(hRes);
    err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
    if (err != 0)
        goto hydrate_fail_cleanup;

    /* 16 method rows of 0x6e bytes, walked through uMeth itself (0x004140a2). */
    for (i = 0; i < 0x10; i++) {
        if (*(short *)uMeth != 0) {
            pChild = *(int **)(uMeth + 0x12);
            if (pChild != NULL) {
                hRes = pChild;
                CloneResource((void **)&hRes);
                err = SetupRenderObjectResource(pSceneNode, &sceneWordAux, hRes);
                if (err != 0)
                    goto hydrate_fail_cleanup;
            }
        }
        uMeth += 0x6e;
    }

    if (DestroyRenderNode(pSceneNode) == 0)
        return;

hydrate_fail_cleanup:
    ShowEngineMessageBoxFromPascalString((byte *)UpdatePascalLength((char *)kStrSaveWriteError));
    DestroyRenderNode(pSceneNode);
    DeleteResourceFile(containerPathDesc);
    return;

    /* Failure exit from inside the gTrackTable walk (0x004140fc): release the table lock taken
     * by GetResDataPtr above, then run the common cleanup. As a macro this once had a
     * forward-declaration block spliced in after its `\` continuation, which made its body
     * unconditional code at function entry: UnlockResObject on an unlocked handle, fatal 0xea on
     * every save. */
slot2_fail:
    UnlockResObject((int *)gTrackTable);
    goto hydrate_fail_cleanup;
}


/* The inverse of WriteSaveGameFile: reads the container, checks the saved name against the expected
 * one (kStrSaveVersionError), installs the CLUT and viewport, restores the channel block and the
 * object tables and binds the property tables. Called by ActionCmd_OpenGame. */
short __cdecl ReadSaveGameFile(byte *pPath, byte *pExpectedName)  /* 0x00414110 */
{
  /* The exe copies each channel (146 bytes) and the whole save image (0x312)
   * as one record: rep movsd + movsw. */
  typedef struct { char b[146]; } ChanImg;
  typedef struct { char b[0x312]; } SaveImg;
  uint32_t byteCount;
  short status;
  short loopStatus;
  uint32_t childIdx;
  byte *pStr;
  short detachStatus;
  short *pImage;
  void *pChildData;
  int pTrack;
  int *hChild;  /* local_82c */
  uint32_t *hSaveNode;  /* local_828 */
  short *pBindRows;  /* local_824 */
  /* volatile: the exe keeps this save in its stack slot ([esp+0x1c]) while
   * the bootfile node stays in edx; only a memory-resident local gives
   * that frame (0x82c) */
  volatile uint32_t savedOriginY;  /* local_820 */
  int      local_81f_bootNode;
  void *pBindBlock540;  /* local_81c */
  void *pBindBlock4a0;  /* local_818 */
  short viewRect [4];  /* local_814 */
  ChanImg savedChan0;  /* local_80c */
  ChanImg savedChan1;  /* local_778 */
  ChanImg savedChan2;  /* local_6e4 */
  byte stageName [256];  /* local_650 */
  uint32_t nodeBuf [340];  /* local_550 */

  childIdx = ReadContainerIntoNode(pPath, nodeBuf);
  DF_TRACE("loadgame: ReadContainerIntoNode('%.*s') -> %d",
           (int)pPath[0], (const char *)pPath + 1, (int)(short)childIdx);
  if ((short)childIdx != 0) {
    pStr = (byte *)UpdatePascalLength(&kStrSaveInvalidError);
    ShowEngineMessageBoxFromPascalString(pStr);
    return 1;
  }
  FlushAndCloseNodeStorage(nodeBuf);
  status = OpenContainerNode(pPath, &hSaveNode);
  if (status != 0) {
    FatalError(status, 0x10f2);
  }
  status = AttachRenderChild(hSaveNode, 0, &gPropDescriptorTable);
  if (status != 0) {
    FatalError(status, 0x10f3);
  }
  CloneResource(&gPropDescriptorTable);
  detachStatus = DetachRenderChild((int)hSaveNode, 0);
  if (detachStatus != 0) {
    FatalError(detachStatus, 0x10f4);
  }
  pStr = (byte *)GetResDataPtr(gPropDescriptorTable);
  detachStatus = PascalStringsEqualNoCase(pExpectedName, pStr);
  DF_TRACE("loadgame: header name='%.*s' expected='%.*s' match=%d stage='%.*s'",
           (int)pStr[0], (const char *)pStr + 1,
           (int)pExpectedName[0], (const char *)pExpectedName + 1, (int)detachStatus,
           (int)pStr[0x100], (const char *)pStr + 0x101);
  if (detachStatus == 0) {
    UnlockResObject(gPropDescriptorTable);
    FreeResObject(gPropDescriptorTable);
    status = DestroyRenderNode(hSaveNode);
    if (status != 0) {
      FatalError(status, 0x10f5);
    }
    pStr = (byte *)UpdatePascalLength(&kStrSaveVersionError);
    ShowEngineMessageBoxFromPascalString(pStr);
    return 1;
  }
  CopyPascalString(pStr + 0x100, stageName);
  UnlockResObject(gPropDescriptorTable);
  InstallCursor(gCursor);
  QdSetPort(&gChannels[1].hResource);
  GetSceneRect(&gChannels[1].hResource, viewRect);
  QdPenNormal();
  SetSceneNodeColorFromPalette(0);
  QdPaintRect(viewRect);
  StripBootPrefixFromSavedDescriptors();
  ShutdownSceneResource(0);
  SetCurrentCdName(stageName);
  DF_TRACE("loadgame: stage '%.*s' committed, a990=%d (non-zero = abort)",
           (int)stageName[0], (const char *)stageName + 1, (int)gGameLoopState);
  if (gGameLoopState != 0) {
    InitSceneResource(0);
    return 1;
  }
  status = AttachRenderChild(hSaveNode, 1, &hChild);
  if (status != 0) {
    FatalError(status, 0x10f6);
  }
  pImage = (short *)GetResDataPtr(hChild);
  savedChan0 = *(ChanImg *)&gChannels[0];
  savedChan1 = *(ChanImg *)&gChannels[1];
  /* Ground truth 0x00414110 saves BOTH dwords (mov ecx,[0x48a86a]; mov edx,[0x48a86e]) and writes
   * both back after the channel states are restored. Ghidra shows only the 0x48a86a read and
   * drops the 0x48a86e read and both stores, so the port kept an orphaned save and neither
   * restore. 0x48a86e is the live bootfile node; letting the saved image overwrite it left the
   * SAVING process's pointer in place and the first idle() after a load called through it
   * (sendtoboot -> AcquireMethodHandle -> AttachRenderChild, SIGSEGV). Harmless for our own saves
   * only because our pool reproduces the same addresses run to run; a save from any other
   * process crashed on load. */
  savedOriginY = *(uint32_t *)&gSceneOriginY;
  local_81f_bootNode = gBootNode;
  savedChan2 = *(ChanImg *)&gChannels[2];
  DF_TRACE("loadImage: before restore gBootNode=%p src[0x30e]=0x%08x src[0]=0x%08x",
           (void *)(intptr_t)gBootNode,
           (unsigned)*(uint32_t *)((char *)pImage + 0x30e),
           (unsigned)*(uint32_t *)pImage);
  *(SaveImg *)gChannels = *(SaveImg *)pImage;
  DF_TRACE("loadImage: after restore gBootNode=%p (0 expected if the image covers it)",
           (void *)(intptr_t)gBootNode);
  *(ChanImg *)&gChannels[0] = savedChan0;
  *(ChanImg *)&gChannels[1] = savedChan1;
  *(uint32_t *)&gSceneOriginY = savedOriginY;   /* mov [0x48a86a],ecx */
  gBootNode = local_81f_bootNode;        /* mov [0x48a86e],edx */
  *(ChanImg *)&gChannels[2] = savedChan2;
  UnlockResObject(hChild);
  detachStatus = DetachRenderChild((int)hSaveNode, 1);
  if (detachStatus != 0) {
    FatalError(detachStatus, 0x10f7);
  }
  status = AttachRenderChild(hSaveNode, 2, &hChild);
  if (status != 0) {
    FatalError(status, 0x10f8);
  }
  CloneResource(&hChild);
  gActorTable = hChild;
  detachStatus = DetachRenderChild((int)hSaveNode, 2);
  if (detachStatus != 0) {
    FatalError(detachStatus, 0x10f9);
  }
  status = AttachRenderChild(hSaveNode, 3, &hChild);
  if (status != 0) {
    FatalError(status, 0x10fa);
  }
  CloneResource(&hChild);
  gCastTable = hChild;
  detachStatus = DetachRenderChild((int)hSaveNode, 3);
  if (detachStatus != 0) {
    FatalError(detachStatus, 0x10fb);
  }
  status = AttachRenderChild(hSaveNode, 4, &hChild);
  if (status != 0) {
    FatalError(status, 0x10fc);
  }
  CloneResource(&hChild);
  gPropTable = hChild;
  detachStatus = DetachRenderChild((int)hSaveNode, 4);
  if (detachStatus != 0) {
    FatalError(detachStatus, 0x10fd);
  }
  status = AttachRenderChild(hSaveNode, 5, &hChild);
  if (status != 0) {
    FatalError(status, 0x10fe);    /* exe: push 0x10fe (was a copy of child 4's 0x10fc) */
  }
  CloneResource(&hChild);
  gShopTable = hChild;
  detachStatus = DetachRenderChild((int)hSaveNode, 5);
  if (detachStatus != 0) {
    FatalError(detachStatus, 0x10ff);    /* exe: push 0x10ff (was 0x10fd) */
  }
  status = AttachRenderChild(hSaveNode, 6, &hChild);
  if (status != 0) {
    FatalError(status, 0x1100);
  }
  CloneResource(&hChild);
  gTrackTable = hChild;
  detachStatus = DetachRenderChild((int)hSaveNode, 6);
  if (detachStatus != 0) {
    FatalError(detachStatus, 0x1101);
  }
  status = 0;
  childIdx = 7;
  pTrack = GetResDataPtr(gTrackTable);
  if (0 < gTrackCount) {
    do {
      loopStatus = AttachRenderChild(hSaveNode, childIdx, &hChild);
      if (loopStatus != 0) {
        FatalError(loopStatus, 0x1102);
      }
      byteCount = *(short *)(pTrack + 4) * 0x68;
      /* the buffer pointer is stored whole (mov [ebx+0xa],eax); the port
       * had routed it through the short status local, keeping 16 bits */
      *(void **)(pTrack + 10) = AllocEngineBuffer(byteCount);
      pChildData = (void *)GetResDataPtr(hChild);
      memmove(*(void **)(pTrack + 10), pChildData, byteCount);
      UnlockResObject(hChild);
      detachStatus = DetachRenderChild((int)hSaveNode, childIdx);
      if (detachStatus != 0) {
        FatalError(detachStatus, 0x1103);
      }
      childIdx++;
      loopStatus = AttachRenderChild(hSaveNode, childIdx, &hChild);
      if (loopStatus != 0) {
        FatalError(loopStatus, 0x1104);
      }
      byteCount = *(short *)(pTrack + 6) * 0x68;
      *(void **)(pTrack + 0xe) = AllocEngineBuffer(byteCount);
      pChildData = (void *)GetResDataPtr(hChild);
      memmove(*(void **)(pTrack + 0xe), pChildData, byteCount);
      UnlockResObject(hChild);
      detachStatus = DetachRenderChild((int)hSaveNode, childIdx);
      if (detachStatus != 0) {
        FatalError(detachStatus, 0x1105);
      }
      childIdx++;
      loopStatus = AttachRenderChild(hSaveNode, childIdx, &hChild);
      if (loopStatus != 0) {
        FatalError(loopStatus, 0x1106);
      }
      byteCount = *(short *)(pTrack + 8) * 0x68;
      *(void **)(pTrack + 0x12) = AllocEngineBuffer(byteCount);
      pChildData = (void *)GetResDataPtr(hChild);
      memmove(*(void **)(pTrack + 0x12), pChildData, byteCount);
      UnlockResObject(hChild);
      detachStatus = DetachRenderChild((int)hSaveNode, childIdx);
      if (detachStatus != 0) {
        FatalError(detachStatus, 0x1107);
      }
      status = status + 1;
      childIdx++;
      pTrack = pTrack + 0x28;
    } while (status < gTrackCount);
  }
  UnlockResObject(gTrackTable);
  UninstallResourcePair(gGlobalsNode);
  status = AttachRenderChild(hSaveNode, childIdx, &hChild);
  if (status != 0) {
    FatalError(status, 0x1108);
  }
  CloneResource(&hChild);
  gGlobalsNode = hChild;
  detachStatus = DetachRenderChild((int)hSaveNode, childIdx);
  if (detachStatus != 0) {
    FatalError(detachStatus, 0x1109);
  }
  childIdx++;
  status = AttachRenderChild(hSaveNode, childIdx, &hChild);
  if (status != 0) {
    FatalError(status, 0x110a);
  }
  CloneResource(&hChild);
  *(int **)(GetResDataPtr(gGlobalsNode) + 0x10) = hChild;
  UnlockResObject(gGlobalsNode);
  detachStatus = DetachRenderChild((int)hSaveNode, childIdx);
  if (detachStatus != 0) {
    FatalError(detachStatus, 0x110b);
  }
  childIdx++;
  GetScriptBindingTableBasePointers(&pBindBlock540, &pBindRows, &pBindBlock4a0);
  status = AttachRenderChild(hSaveNode, childIdx, &hChild);
  if (status != 0) {
    FatalError(status, 0x110c);
  }
  pChildData = (void *)GetResDataPtr(hChild);
  memmove(pBindBlock540, pChildData, 0x540);
  UnlockResObject(hChild);
  detachStatus = DetachRenderChild((int)hSaveNode, childIdx);
  if (detachStatus != 0) {
    FatalError(detachStatus, 0x110d);
  }
  childIdx++;
  status = AttachRenderChild(hSaveNode, childIdx, &hChild);
  if (status != 0) {
    FatalError(status, 0x110e);
  }
  pChildData = (void *)GetResDataPtr(hChild);
  memmove(pBindBlock4a0, pChildData, 0x4a0);
  UnlockResObject(hChild);
  detachStatus = DetachRenderChild((int)hSaveNode, childIdx);
  if (detachStatus != 0) {
    FatalError(detachStatus, 0x110f);
  }
  childIdx++;
  status = AttachRenderChild(hSaveNode, childIdx, &hChild);
  if (status != 0) {
    FatalError(status, 0x1110);
  }
  pChildData = (void *)GetResDataPtr(hChild);
  memmove(pBindRows, pChildData, 0x6e0);
  UnlockResObject(hChild);
  detachStatus = DetachRenderChild((int)hSaveNode, childIdx);
  if (detachStatus != 0) {
    FatalError(detachStatus, 0x1111);
  }
  status = 0x10;
  childIdx++;
  do {
    if ((*pBindRows != 0) && (*(int *)(pBindRows + 9) != 0)) {
      loopStatus = AttachRenderChild(hSaveNode, childIdx, &hChild);
      if (loopStatus != 0) {
        FatalError(loopStatus, 0x1112);
      }
      CloneResource(&hChild);
      *(int **)(pBindRows + 9) = hChild;
      detachStatus = DetachRenderChild((int)hSaveNode, childIdx);
      if (detachStatus != 0) {
        FatalError(detachStatus, 0x1113);
      }
      childIdx = childIdx + 1;
    }
    pBindRows = pBindRows + 0x37;
    status = status + -1;
  } while (status != 0);
  status = DestroyRenderNode(hSaveNode);
  if (status != 0) {
    FatalError(status, 0x1114);
  }
  DF_TRACE("loadgame: all tables restored (last child %u), handing to fixup", (unsigned)childIdx);
  return 0;
}


/* The post-load fixup of ActionCmd_OpenGame: re-resolves every table the container just loaded
 * (cast, descriptors, scenes and their node lists), re-opens each saved render node by its saved
 * selector (ClaimPendingRenderNodeBySavedSelector), re-links actors and props to their nodes
 * (FindRenderNodeByDescriptor), re-initialises the animation slots and the sound channels and
 * restores the set and stage subsystems. Loading without it leaves every record pointing at
 * pre-load handles. */
void __stdcall RestoreSavedGameState(void)  /* 0x00414B00 */
{
  typedef struct { uint32_t w[0x1a]; } Node68;
  short status;
  short pan;
  short flag;
  short cnt;
  short kind;
  int tableData;
  int i;
  uint32_t *puVar6;
  int iVar12;
  int *pEntry;
  int pRow;
  int *pSeg;
  uint32_t *pSavedClut;
  uint32_t *pNodeA;
  byte *pSearchPath;
  int j;
  uint32_t *pOutNode;  /* local_28 */
  int *hSegments;  /* local_20 */
  int *hEntries;  /* local_1c */
  int *hTrackInfo;  /* local_18 */
  int trackIdx;  /* local_14 */
  int channelNode;  /* local_10 */
  int savedKeyC;  /* local_c */
  int themeNode;  /* local_8 */
  uint32_t hChildOut;  /* local_4 */
  int k;
  uint32_t *pN;

  tableData = GetResDataPtr(gPropDescriptorTable);
  j = 1;
  pSearchPath = (byte *)(tableData + 0x300);
  do {
    SetSearchPathSlot((short)j, pSearchPath);
    pSearchPath = pSearchPath + 0x100;
    j = j + 1;
  } while (j <= 8);
  j = *(int *)(tableData + 0xb00);
  iVar12 = *(int *)(tableData + 0xb04);
  savedKeyC = *(int *)(tableData + 0xb08);
  puVar6 = (uint32_t *)GetResDataPtr(gActiveClut) + 2;
  pSavedClut = (uint32_t *)(tableData + 0xb0c);
  tableData = 0x100;
  do {
    *(POINT *)puVar6 = *(POINT *)pSavedClut;
    pSavedClut = pSavedClut + 2;
    puVar6 = puVar6 + 2;
  } while (--tableData != 0);
  UnlockResObject(gActiveClut);
  ReloadCLUT(gActiveClut);
  UnlockResObject(gPropDescriptorTable);
  pEntry = (int *)GetResDataPtr(gCastTable);
  i = 0;
  if (0 < gCastCount) {
    do {
      *pEntry = ClaimPendingRenderNodeBySavedSelector(*pEntry, 0x4343444f);   /* 'CCDO' */
      status = AttachRenderChild((int *)*pEntry, 0, pEntry + 1);
      if (status != 0) {
        FatalError(status, 0x1115);
      }
      pEntry = pEntry + 7;
      i = i + 1;
    } while (i < gCastCount);
  }
  UnlockResObject(gCastTable);
  pRow = GetResDataPtr(gActorTable);
  i = 0;
  if (0 < gActorCount) {
    do {
      *(int *)(pRow + 2) = FindRenderNodeByDescriptor(*(int *)(pRow + 2));
      status = AttachRenderChild(*(int **)(pRow + 2), *(uint32_t *)(pRow + 10),
                                (int *)(pRow + 6));
      if (status != 0) {
        FatalError(status, 0x1116);
      }
      *(int *)(pRow + 0x2e) = NextRenderNodeSerial();
      pRow = pRow + 0xa0;
      i = i + 1;
    } while (i < gActorCount);
  }
  UnlockResObject(gActorTable);
  pEntry = (int *)GetResDataPtr(gShopTable);
  i = 0;
  if (0 < gShopCount) {
    do {
      *pEntry = ClaimPendingRenderNodeBySavedSelector(*pEntry, 0x5044444f);   /* 'PDDO' */
      status = AttachRenderChild((int *)*pEntry, 0, pEntry + 1);
      if (status != 0) {
        FatalError(status, 0x1117);
      }
      pEntry = pEntry + 7;
      i = i + 1;
    } while (i < gShopCount);
  }
  UnlockResObject(gShopTable);
  pRow = GetResDataPtr(gPropTable);
  i = 0;
  if (0 < gPropCount) {
    do {
      *(int *)(pRow + 2) = FindRenderNodeByDescriptor(*(int *)(pRow + 2));
      status = AttachRenderChild(*(int **)(pRow + 2), *(uint32_t *)(pRow + 10),
                                (int *)(pRow + 6));
      if (status != 0) {
        FatalError(status, 0x1118);
      }
      *(int *)(pRow + 0x2c) = NextRenderNodeSerial();
      pRow = pRow + 0x9e;
      i = i + 1;
    } while (i < gPropCount);
  }
  UnlockResObject(gPropTable);
  pEntry = (int *)GetResDataPtr(gTrackTable);
  i = 0;
  if (0 < gTrackCount) {
    do {
      *pEntry = ClaimPendingRenderNodeBySavedSelector(*pEntry, 0x534f4e47);   /* 'SONG' */
      pEntry = pEntry + 10;
      i = i + 1;
    } while (i < gTrackCount);
  }
  UnlockResObject(gTrackTable);
  channelNode = FindRenderNodeByDescriptor(iVar12);
  themeNode = FindRenderNodeByDescriptor(j);
  MoveResObjectToHeapBottom(gTrackTable);
  pEntry = (int *)GetResDataPtr(gTrackTable);
  trackIdx = 0;
  if (0 < gTrackCount) {
    do {
      status = AttachRenderChild((int *)*pEntry, 0, (int *)&hTrackInfo);
      if (status != 0) {
        FatalError(status, 0x1119);
      }
      MoveResObjectToHeapBottom(hTrackInfo);
      tableData = GetResDataPtr(hTrackInfo);
      status = AttachRenderChild((int *)*pEntry, *(uint32_t *)(tableData + 0x1c), (int *)&hSegments);
      if (status != 0) {
        FatalError(status, 0x111a);
      }
      MoveResObjectToHeapBottom(hSegments);
      pSeg = (int *)GetResDataPtr(hSegments);
      j = 0;
      pNodeA = *(uint32_t **)((int)pEntry + 0xe);
      iVar12 = (int)pSeg + 0x10e;
      if (0 < *(short *)((int)pEntry + 6)) {
        do {
          status = GetSoundNodeVolume((int)pNodeA);
          pan = GetSoundNodePan((int)pNodeA);
          InitSoundNodeEntryFromResource((uint32_t *)*pEntry, *(uint32_t *)(iVar12 + 4), (byte *)(iVar12 + 10), pNodeA);
          SetSoundNodeVolume((byte *)pNodeA, status);
          SetSoundNodePan((byte *)pNodeA, pan);
          j = j + 1;
          pNodeA = pNodeA + 0x1a;
          iVar12 = iVar12 + 0x1a;
        } while (j < *(short *)((int)pEntry + 6));
      }
      j = 0;
      pOutNode = *(uint32_t **)((int)pEntry + 0x12);
      cnt = 0;
      if (0 < (short)pSeg[1]) {
        do {
          status = *(short *)((int)pSeg + j * 2 + 6);
          if (0 < status) {
            iVar12 = *(int *)((int)pSeg + 0x10a);
            if (status <= iVar12) {
              puVar6 = pOutNode + 0x1a;
              if ((short)pSeg[1] - j == 1) {
                pRow = *pSeg;
                if (iVar12 <= pRow) {
                  pRow = iVar12 + -1;
                }
                if (pRow < 0) {
                  pRow = 0;
                }
                puVar6 = (uint32_t *)(pRow * 0x68 + *(int *)((int)pEntry + 0x12));
              }
              *(Node68 *)pOutNode = ((Node68 *)*(int *)((int)pEntry + 0xe))[status - 1];
              cnt = cnt + 1;
              pOutNode[6] = (uint32_t)puVar6;
              pOutNode = pOutNode + 0x1a;
            }
          }
          j = j + 1;
        } while (j < (short)pSeg[1]);
      }
      *(short *)(pEntry + 2) = cnt;
      UnlockResObject(hSegments);
      status = DetachRenderChild(*pEntry, *(int *)(tableData + 0x1c));
      if (status != 0) {
        FatalError(status, 0x111b);
      }
      status = AttachRenderChild((int *)*pEntry, *(uint32_t *)(tableData + 0x20), (int *)&hEntries);
      if (status != 0) {
        FatalError(status, 0x111c);
      }
      MoveResObjectToHeapBottom(hEntries);
      j = GetResDataPtr(hEntries);
      k = 0;
      j = j + 8;
      pN = *(uint32_t **)((int)pEntry + 10);
      if (0 < (short)pEntry[1]) {
        do {
          if ((short)pN[0xb] != 1) {
            status = GetSoundNodeVolume((int)pN);
            pan = GetSoundNodePan((int)pN);
            flag = GetSoundNodeLoop((int)pN);
            kind = (short)pN[0xb];
            InitSoundNodeEntryFromResource((uint32_t *)*pEntry, *(uint32_t *)(j + 4), (byte *)(j + 10), pN);
            *pN = k + 1;
            *(short *)(pN + 0xb) = kind;
            SetSoundNodeVolume((byte *)pN, status);
            SetSoundNodePan((byte *)pN, pan);
            SetSoundNodeLoop((int)pN, flag);
          }
          j = j + 0x1a;
          pN = pN + 0x1a;
          k = k + 1;
        } while (k < (short)pEntry[1]);
      }
      UnlockResObject(hEntries);
      status = DetachRenderChild(*pEntry, *(int *)(tableData + 0x20));
      if (status != 0) {
        FatalError(status, 0x111d);
      }
      UnlockResObject(hTrackInfo);
      status = DetachRenderChild(*pEntry, 0);
      if (status != 0) {
        FatalError(status, 0x111e);
      }
      pEntry = pEntry + 10;
      trackIdx = trackIdx + 1;
    } while (trackIdx < gTrackCount);
  }
  UnlockResObject(gTrackTable);
  AssignSoundNodeTrackIndices();
  pEntry = (int *)GetResDataPtr(gTrackTable);
  i = 0;
  if (0 < gTrackCount) {
    do {
      pSeg = *(int **)((int)pEntry + 10);
      j = 0;
      if (0 < (short)pEntry[1]) {
        do {
          if ((*pEntry == channelNode) && (pSeg[9] == savedKeyC)) {
            AssignSoundChannel(pSeg);
          }
          pSeg = pSeg + 0x1a;
          j = j + 1;
        } while (j < (short)pEntry[1]);
      }
      if (*pEntry == themeNode) {
        PlayThemeList(*(uint32_t **)((int)pEntry + 0x12));
      }
      pEntry = pEntry + 10;
      i = i + 1;
    } while (i < gTrackCount);
  }
  UnlockResObject(gTrackTable);
  if (gSet.active != 0) {
    DAT_0048a780 = (uint32_t *)ClaimPendingRenderNodeBySavedSelector((int)DAT_0048a780, 0x5343444f);   /* 'SCDO' */
    status = AttachRenderChild(DAT_0048a780, 0, &DAT_0048a784);
    if (status != 0) {
      FatalError(status, 0x111f);
    }
    status = AttachRenderChild(DAT_0048a780, DAT_0048a788, &hChildOut);
    if (status != 0) {
      FatalError(status, 0x1120);
    }
    status = AttachRenderChild(DAT_0048a780, DAT_0048a7ec, &gSceneTable);
    if (status != 0) {
      FatalError(status, 0x1121);
    }
    status = AttachRenderChild(DAT_0048a780, DAT_0048a7e4, &gWaypointTable);
    if (status != 0) {
      FatalError(status, 0x1122);
    }
    CommitSetViewport();
    ShowSetView(&gSceneName, &gViewName);
  }
  if (gStage.active != 0) {
    DAT_0048a754 = (uint32_t *)ClaimPendingRenderNodeBySavedSelector((int)DAT_0048a754, 0x464c4154);   /* 'FLAT' */
    status = AttachRenderChild(DAT_0048a754, 0, &DAT_0048a760);
    if (status != 0) {
      FatalError(status, 0x1123);
    }
    MoveResObjectToHeapTop(DAT_0048a760);
    GetResDataPtr(DAT_0048a760);
    UnlockResObject(DAT_0048a760);
    DAT_0048a75c = 0xffffffff;
    /* One parameter: NavigateToStagePage (0x00446d80) reads only [ESP+0xfc]; the `unaff_EBP`
     * second argument was a Ghidra register artifact. */
    NavigateToStagePage(DAT_0048a758);
  }
  gPuppet.active = 0;
  FreeResObject(gPropDescriptorTable);
  FlushEventQueue();
  BumpSceneOverlayModeFloor(2);
  return;
}


/* Re-opens one render node recorded in a saved game. `selector` is the saved node's SELECTOR (the
 * first dword of its entry in the saved node table that WriteSaveGameFile wrote), looked up with
 * GetRenderNodeDescriptorBySelector. fileType is the four-character type each caller in
 * RestoreSavedGameState pushes; it is only forwarded to ValidateDescriptorPath, which never reads
 * it.
 *
 * Port note: the port called GetSearchPathSlot instead and declared the parameter `short`, so a
 * saved node pointer arrived truncated (-31328) and that accessor's range check fataled, "Fatal
 * error at line 4300" on every attempt to open a save (class 14). */
int __cdecl ClaimPendingRenderNodeBySavedSelector(int selector, int fileType) /* 0x00415370 */
{
    short u;
    short        err;
    int          ctx;

    GetRenderNodeDescriptorBySelector(selector, gScratchStr);
    TruncatePathToLeaf(gScratchStr);
    u = ValidateDescriptorPath(gScratchStr, fileType);
    if (u != 0)
        FatalError(u, 0x1124);
    StoreRenderNodeDescriptorBySelector(selector, gScratchStr);
    err = TakeMatchingPendingRenderNode(gScratchStr, &ctx);
    if (err != 0)
        FatalError(err, 0x1125);
    return ctx;
}


/* Resolves a saved node selector to the live render node: gets the saved descriptor, encodes it
 * into gScratchStr and walks the global node list (stride 0x540) for the node whose name (+0x24)
 * matches. Fatal 0x1126 if there is none. */
int __cdecl FindRenderNodeByDescriptor(int selector)  /* 0x00415400 */
{
    int hHead;
    int hNode;     /* the walk runs in a register (esi), apart from the out-param local */

    if (selector == 0)
        return 0;
    GetGlobalRenderNodeListHead((int **)&hHead);
    GetRenderNodeDescriptorBySelector(selector, &gScratchStr);
    for (hNode = hHead; hNode != 0; hNode = *(int *)(hNode + 0x540)) {
        if (PascalStringsEqualNoCase(&gScratchStr, (byte *)(hNode + 0x24)) != 0)
            return hNode;
    }
    FatalError(0, 0x1126);
    return 0;
}


/* Looks the selector up in the saved node table (gPropDescriptorTable) and copies its Pascal
 * descriptor to pDescOut. */
void __cdecl GetRenderNodeDescriptorBySelector(int selector, byte *pDescOut)  /* 0x00415480 */
{
    int  iBase;
    int  i;
    int  iCount;
    int *pEntry;

    iBase  = GetResDataPtr(gPropDescriptorTable);
    pEntry = (int *)(iBase + 0x1310);
    /* the shape of StoreRenderNodeDescriptorBySelector: i zeroed before the
     * count is read, both in the for initialiser */
    for (i = 0, iCount = *(int *)(iBase + 0x130c); i < iCount; pEntry += 0x41, i++) {
        if (*pEntry == selector) {
            CopyPascalString((byte *)(pEntry + 1), pDescOut);
            UnlockResObject(gPropDescriptorTable);
            return;
        }
    }
    UnlockResObject(gPropDescriptorTable);
    FatalError(0, 0x1127);
}




/* Copies pOutDesc INTO the saved node table entry whose first dword is the selector (the table is
 * at gPropDescriptorTable + 0x1310, stride 0x104): the caller writes the normalised, validated
 * descriptor back. The exe does this at 0x0041555a (push entry+4, push param_2, CopyPascalString);
 * the twin reader is GetRenderNodeDescriptorBySelector.
 *
 * Port note: the old name said the opposite and an earlier edit had swapped the arguments to fit it, which
 * overwrote the caller's normalised descriptor with the raw saved one (class 14). */
void __cdecl StoreRenderNodeDescriptorBySelector(int slotIndex, byte *pOutDesc)  /* 0x00415500 */
{
  int iBase;
  int iCount;
  int i;
  int *pEntry;

  iBase = GetResDataPtr(gPropDescriptorTable);
  pEntry = (int *)(iBase + 0x1310);
  /* i zeroed before the count is read, and the count read on the loop's own source
   * line: with the read on a line of its own VC++ 4.2 folds i = 0 into the entry
   * test (test ecx,ecx where the exe has cmp ecx,edx) */
  for (i = 0, iCount = *(int *)(iBase + 0x130c); i < iCount; pEntry += 0x41, i++) {
    if (*pEntry == slotIndex) {
      CopyPascalString(pOutDesc, (byte *)(pEntry + 1));
      UnlockResObject(gPropDescriptorTable);
      return;
    }
  }
  UnlockResObject(gPropDescriptorTable);
  FatalError(0, 0x1128);
}


/* Strips descriptor 0 (the boot path) from the eight saved descriptors 1..8 of the save image's
 * class table (gPropDescriptorTable + 0x200, nine 256-byte slots): any case-folded prefix match
 * between entry 0 and entries 1-8 is removed (StripMatchingPascalPrefix). */
void __stdcall StripBootPrefixFromSavedDescriptors(void) /* 0x00415580 */
{
    int   pTable, idx;
    short next;

    next = 1;
    pTable = (int)GetResDataPtr(gPropDescriptorTable);
    do {
        idx = (int)next;
        next = next + 1;
        StripMatchingPascalPrefix(((PropDescriptorTable *)pTable)->names[0], ((PropDescriptorTable *)pTable)->names[idx]);
    } while (next <= 8);
    UnlockResObject(gPropDescriptorTable);
}


/* If pPrefix is a prefix (case-folded) of pStr, removes that prefix from
 * pStr in place, leaving only the suffix. */
void __cdecl StripMatchingPascalPrefix(byte *pPrefix, byte *pStr) /* 0x004155D0 */
{
    /* both lengths are 16-bit signed locals (movzx bp / movzx bx, cmp / jl),
     * adjusted in place: the tail length, then the source offset */
    short  prefixLen;
    short  len;

    prefixLen = *pPrefix;
    len = *pStr;
    if (len >= prefixLen) {
        if (CompareRawByteSlicesCaseFolded(pPrefix + 1, pStr + 1, prefixLen) != 0) {
            len -= prefixLen;
            prefixLen++;
            GetSearchPathSlot(0, &gScratchStr);
            memmove(pStr + 1, pStr + prefixLen, len);
            *pStr = (byte)len;
            AppendPascalString(pStr, &gScratchStr);
            CopyPascalString(&gScratchStr, pStr);
        }
    }
}


/* Script command openshopfile (opcode 12056): as ActionCmd_OpenCastFile, for the shop file.
 * Validates the descriptor, parses it with OpenShopNode into a 7-int shop row appended to
 * gShopTable (stride 0x1c) and expands gPropTable (stride 0x9e) with one 158-byte row per item of
 * the source data's array at +0x938 / +0x93c (InitPropEntry). */
short __cdecl ActionCmd_OpenShopFile(int pCtx, short *pArgBlock,  /* 0x00415650 */
                                         int *pTable, int *pSubSkip)
{
    /* Both rows are copied as whole structs (rep movsd 7; 0x27 + movsw,
     * 0x00415650). */
    typedef struct { int v[7]; } Row7;
    typedef struct { char b[0x9e]; } Row158;
    short           status;
    int            nSrcEntries;
    int            iSlot2Base;
    int            iSrcData;
    int            i;
    unsigned int  *pSrcArray;
    int            stateEntry[40];  /* 0x9E-byte (158-byte) scratch for slot-2 entries */
    short          args[4];
    int            slotEntry[7];    /* 7-int (0x1C-byte) slot-3 entry;
                                       slotEntry[0]=ctxHandle, slotEntry[1]=srcHandle */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)args, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(args, (short *)gScratchStr2);
    if (status != 0)
        return status;

    status = ValidateDescriptorPath(gScratchStr2, 0x5044444f)   /* 'PDDO' */;
    if (status != 0)
        return status;

    status = OpenShopNode(gScratchStr2, slotEntry);
    if (status != 0)
        return status;

    gShopCount++;
    ResizeResObject(gShopTable, (unsigned int)(gShopCount * 0x1c));
    ((Row7 *)GetResDataPtr(gShopTable))[gShopCount - 1] = *(Row7 *)slotEntry;

    UnlockResObject(gShopTable);

    iSrcData    = GetResDataPtr((void *)slotEntry[1]);
    nSrcEntries = *(int *)(iSrcData + 0x938);
    pSrcArray   = (unsigned int *)(iSrcData + 0x93c);

    ResizeResObject(gPropTable, (unsigned int)((gPropCount + nSrcEntries) * 0x9e));
    iSlot2Base = GetResDataPtr(gPropTable);

    DF_TRACE("OpenShopFile: ctxH=%08x srcH=%08x srcData=%08x nSrc=%d "
             "slot2Base=%08x count=%d",
             slotEntry[0], slotEntry[1], iSrcData, nSrcEntries,
             iSlot2Base, (int)gPropCount);

    /* Ground truth guards this loop with `if (0 < iVar3)`, not `!= 0`: a
     * negative count must skip the loop entirely, not run it ~4 billion times
     * writing 158 bytes each. */
    for (i = 0; i < nSrcEntries; i++)
    {
        InitPropEntry(slotEntry, *pSrcArray, stateEntry);
        *(Row158 *)(iSlot2Base + gPropCount * 0x9e) = *(Row158 *)stateEntry;
        pSrcArray += 4;   /* stride: 4 ints = 16 bytes per source array entry */

        gPropCount++;
    }

    UnlockResObject(gPropTable);
    UnlockResObject((void *)slotEntry[1]);
    /* 0x004157f4: the registration dispatch's status is the handler's result. */
    return CompleteShopOpen(slotEntry[0]);
}


/* Opens a shop node, the twin of OpenCastNode: validates the child magic 0x40000, copies the Pascal
 * name into the 7-int shop row and detaches / reattaches the selected child. Returns 0, or 0x38
 * when the child is not a shop file. Every other failure is FatalError 0xfce..0xfd3, as in the exe
 * (the port had returned those codes instead, a deviation with no DF_FIX switch). */
short __cdecl OpenShopNode(byte *pDesc, int *pEntry)  /* 0x00415810 */
{
    int   *pChildH;
    short  status;
    int   *pRes;
    short  detachStatus;
    int    hOut;  /* local_4 */

    status = TakeMatchingPendingRenderNode(pDesc, pEntry);
    if (status != 0)
        FatalError(status, 0xFCE);
    pChildH = pEntry + 1;
    status = AttachRenderChild((int *)*pEntry, 0, (int **)pChildH);
    if (status != 0)
        FatalError(status, 0xFCF);
    pRes = (int *)GetResDataPtr((int *)*pChildH);
    if (*pRes != 0x40000) {
        UnlockResObject((int *)*pChildH);
        detachStatus = DetachRenderChild(*pEntry, 0u);
        if (detachStatus != 0)
            FatalError(detachStatus, 0xFD0);
        status = DeferRenderNodeClose(*pEntry);
        if (status != 0)
            FatalError(status, 0xFD1);
        return 0x38;
    }
    /* pRes is int *: +0x24A is byte 0x928. A byte-pointer +0x24A read a header field
     * holding 0xFF and tripped AppendPascalString's 255-byte FatalError. */
    CopyPascalString((byte *)(pRes + 0x24A), (byte *)(pEntry + 3));
    pEntry[2] = pRes[0x249];
    UnlockResObject((int *)pEntry[1]);   /* pEntry[1], not *pChildH: the exe loads it before the pEntry[2] store */
    status = AttachRenderChild((int *)*pEntry, (uint)pEntry[2], (int **)&hOut);
    if (status != 0)
        FatalError(status, 0xFD2);
    detachStatus = DetachRenderChild(*pEntry, (unsigned int)pEntry[2]);
    if (detachStatus != 0)
        FatalError(detachStatus, 0xFD3);
    return 0;
}


/* Fills a 0x9e-byte prop row (pOut) from shop child `sel` of the shop entry pEntry (pEntry[0] is
 * the context handle): attaches the render child, copies three Pascal-string fields from the node
 * data, allocates an alias handle, optionally looks up the 3D position
 * (FindWaypointNodePosition) and sets the frame-count field via ReadChildUshortAt0x70. Then it
 * attaches and immediately detaches the node at (ctx, data[+0x26]) to validate the secondary
 * selector. The twin of InitActorEntry. */
short __cdecl InitPropEntry(int *pEntry, unsigned int sel, int *pOut)
                                                                    /* 0x00415950 */
{
    /* pOut is a SHORT pointer in ground truth (`undefined2 *param_3`): every
     * `p[N]` below is byte offset 2*N (an earlier transcription doubled the
     * indices and wrote ~300 bytes into a 160-byte stack buffer).
     * Returns 0 (xor ax,ax at 0x00415b26). The position block is written
     * through a pointer to +0x18 formed with the other zero stores, the
     * context read once, and the child-count case tested first. */
    typedef struct { int a, b; } Pos8;
    short      status;
    int        pRow;
    short     *p;
    short     *pPos;
    unsigned int slot;
    int       *hRes;
    short      saved;
    /* local_c / local_8 are one region filled by FindWaypointNodePosition
     * (MSVC stack adjacency); hOut is separate. */
    Pos8       pos;
    int        hOut;  /* local_4 */

    p = (short *)pOut;
    /* the context is stored, then read back through the record for the
     * attach (one load into ecx in the exe) */
    *(int **)&p[1] = (int *)*pEntry;
    status = AttachRenderChild(*(int **)&p[1], sel, (int **)&p[3]);
    if (status != 0)
        FatalError(status, 0xfd4);

    *(unsigned int *)&p[5] = sel;
    p[0]    = 0;
    p[9]    = 0;
    p[0xb]  = 0;
    p[0xa]  = 0;
    p[0x12] = 4;
    p[0x13] = (short)0xffff;
    *(int *)&p[0x14] = 1000;
    p[0xd]  = 0;
    hRes = *(int **)&p[3];   /* the exe loads the handle here, between the zero stores */
    pPos    = &p[0xc];
    p[0xe]  = 0;
    p[0xf]  = 0;
    p[0xc]  = 0;

    pRow = GetResDataPtr(hRes);
    *(int *)&p[7] = *(int *)(pRow + 0x26);
    slot = NextRenderNodeSerial();
    ((short *)pOut)[0x25] = 0;
    ((short *)pOut)[0x26] = 0;
    *(unsigned int *)&((short *)pOut)[0x16] = slot;
    *(int *)&((short *)pOut)[0x18] = 0;
    *(int *)&((short *)pOut)[0x23] = 0;

    CopyPascalString((byte *)(pRow + 0x2a), (byte *)&p[0x27]);  /* byte 78  */
    CopyPascalString((byte *)(pRow + 0x4a), (byte *)&p[0x37]);  /* byte 110 */
    CopyPascalString((byte *)(pRow + 0x3a), (byte *)&p[0x2f]);  /* byte 94  */
    CopyPascalString((byte *)UpdatePascalLength(kStrNone), (byte *)&p[0x47]);  /* byte 142 */

    /* GT: save the short at +0x18, do an 8-byte write there, then restore
     * the low short -- so p[0xd..0xf] change. */
    if ((short)PascalStringsEqualNoCase((byte *)&p[0x2f], &DAT_0048a7b4) != 0 &&
        (short)FindWaypointNodePosition((byte *)&p[0x37], (short *)&pos) != 0) {
        saved = *pPos;
        *(Pos8 *)&((short *)pOut)[0xc] = pos;
        ((short *)pOut)[9] = 1;
        *pPos = saved;
    }

    if (*(int *)(pRow + 0x5a) > 0) {
        pRow += 0x5e;
        CopyPascalString((byte *)(pRow + 0x10), (byte *)&p[0x3f]);
        status = (short)ReadChildUshortAt0x70(*(int **)&p[1], *(unsigned int *)pRow);
        p[0x11] = status;
        status--;
    } else {
        CopyPascalString((byte *)UpdatePascalLength(kStrNone), (byte *)&p[0x3f]);  /* byte 126 */
        status = 0;
        p[0x11] = 0;
    }
    p[0x10] = status;

    UnlockResObject(*(int **)&p[3]);

    status = AttachRenderChild(*(int **)&p[1], *(unsigned int *)&p[7], &hOut);
    if (status != 0)
        FatalError(status, 0xfd5);

    status = DetachRenderChild(*(int *)&p[1], *(int *)&p[7]);
    if (status != 0)
        FatalError(status, 0xfd6);
    return 0;
}


/* Script command propinstance (opcode 12076): two arguments, a prop and an alias name (at most 15
 * characters). If the name is already in gPropTable it returns success; otherwise it appends one
 * 0x9e-byte row: a copy of the prop's row with the new name at +0x4e and a fresh handle
 * (NextRenderNodeSerial), AttachRenderChild (FatalError 0xfd7). The twin of
 * ActionCmd_ActorInstance. */
short __cdecl ActionCmd_PropInstance(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00415b40 */
{
    short          evalStatus;
    short           status;
    short          arg1Result[4];
    short          arg2Result[4];
    int            stateEntry[40];   /* 0x9E-byte slot-2 row scratch */
    int            lookupOut[40];
    typedef struct { byte b[0x9e]; } Row158;   /* copied as one object (rep movsd 0x27 + movsw) */

    evalStatus = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)arg1Result, (int *)arg2Result, pSubSkip);
    if (evalStatus != 0)
        return evalStatus;

    status = TakeScriptString(arg1Result, gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(gScratchStr, stateEntry);
    if (status != 0)
        return status;

    status = TakeScriptString(arg2Result, gScratchStr2);
    if (status != 0)
        return status;

    if (gScratchStr2[0] >= 0x10)
        return 0x1a;

    status = LookupPropEntry(gScratchStr2, lookupOut);
    if (status == 0)
        return 0;

    CopyPascalString(&gScratchStr2, (byte *)stateEntry + 0x4e);
    *(undefined4 *)((byte *)stateEntry + 0x2c) = NextRenderNodeSerial();
    *(undefined4 *)((byte *)stateEntry + 0x30) = 0;

    /* Parent = the handle STORED at +2 (`local_140._2_4_`, 0x00415b40), not
     * the field's address -- the same slip as ActionCmd_ActorInstance. */
    evalStatus = AttachRenderChild(*(int **)((byte *)stateEntry + 2),
                              *(unsigned int *)((byte *)stateEntry + 10),
                              (int **)((byte *)stateEntry + 6));
    if (evalStatus != 0)
        FatalError(evalStatus, 0xfd7);

    gPropCount++;
    ResizeResObject(gPropTable, gPropCount * 0x9e);
    ((Row158 *)GetResDataPtr(gPropTable))[gPropCount - 1] = *(Row158 *)stateEntry;
    UnlockResObject(gPropTable);
    return 0;
}


/* Script command propdelete (opcode 12094): as ActionCmd_ActorDelete on gPropTable (0x9e-byte
 * rows): TeardownPropEntry, then the last row is moved over the removed one. */
short __cdecl
ActionCmd_PropDelete(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x00415cd0 */
{
    typedef struct { short w[0x4f]; } StateRow9E;
    short    u;
    short    notFound;
    int      base;
    short    a[4];
    StateRow9E entry;
    int      i;
    int      p;

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)a, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a, gScratchStr);
    if (u != 0)
        return u;

    notFound = 1;
    base = GetResDataPtr(gPropTable);
    i = 0;
    if (0 < gPropCount) {
        p = base;
        do {
            entry = *(StateRow9E *)p;
            /* the prop name is the row's +0x4e field (0x00415d61 LEA EAX,
             * [ESP+0x72]); the port compared the row's first bytes, so
             * propdelete never found a prop */
            if (PascalStringsEqualNoCase((byte *)&entry + 0x4e, gScratchStr)) {
                TeardownPropEntry((int)&entry);
                gPropCount--;
                *(StateRow9E *)(base + i * 0x9e) = *(StateRow9E *)(base + gPropCount * 0x9e);
                notFound = 0;
                break;
            }
            p += 0x9e;
            i++;
        } while (i < gPropCount);
    }
    UnlockResObject(gPropTable);
    if (notFound)
        return 10;
    ResizeResObject(gPropTable, (unsigned int)(gPropCount * 0x9e));
    return 0;
}


/* Statement half of propset (opcode 16027): as ActionCmd_ActorSet on a prop row (write back with
 * WriteBackPropEntry). Sets the prop's set name (+0x5e) and, when that is the open set, places it
 * at its star. Its sibling opcode 16020 (ActionCmd_PropStar) carries the star name. */
short __cdecl
ActionCmd_PropSet(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00415e20 */
{
    short  s1;
    typedef struct { int w[2]; } Pos8;
    short  u, u3;
    short  a1[4], a2[4];
    int    state[40];
    short  pos[4];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    u = TakeScriptString(a2, (byte *)gScratchStr2);
    if (u != 0)
        return u;
    if (gScratchStr2[0] >= 0x10)
        return 0x1a;
    /* The row is local_a0 and Ghidra's other locals are fields of it: local_42 = row+0x5e (set
     * name), local_32 = row+0x6e (node name written by propxyz), local_88 = +0x18, local_84 =
     * +0x1c, local_8e = +0x12. The port had copied the set name to a scratch buffer, compared
     * row+0x50 instead of the set name, looked the node up by the set name, and stored the result
     * at +0x84/+0x88/+0x8e (the local NAMES used as offsets, past the end of the 0x9e-byte row). */
    CopyPascalString((byte *)gScratchStr2, (byte *)state + 0x5e);
    u3 = PascalStringsEqualNoCase((byte *)state + 0x5e, (byte *)&DAT_0048a7b4);
    if (u3 != 0) {
        u3 = FindWaypointNodePosition((byte *)state + 0x6e, pos);
        if (u3 != 0) {
            /* the whole 8-byte record over +0x18, then the first word put
             * back: row+0x1a/+0x1c/+0x1e = pos[1..3], row+0x18 keeps its value */
            short keep = *(short *)((char *)state + 0x18);
            *(Pos8 *)((char *)state + 0x18) = *(Pos8 *)pos;
            *(short *)((char *)state + 0x18) = keep;
        }
    }
    *(short *)((char *)state + 0x12) = 1;
    DF_TRACE("propset: prop='%.*s' set='%.*s' node='%.*s' cur='%.*s' matched=%d found=%d xyz=(%d,%d,%d)",
             (int)((byte *)gScratchStr)[0], (const char *)gScratchStr + 1,
             (int)gScratchStr2[0], (const char *)gScratchStr2 + 1,
             (int)*((byte *)state + 0x6e), (const char *)state + 0x6f,
             (int)((byte *)&DAT_0048a7b4)[0], (const char *)&DAT_0048a7b4 + 1,
             (int)(short)u3 != 0, (int)(short)u3,
             (int)*(short *)((char *)state + 0x1a), (int)*(short *)((char *)state + 0x1c),
             (int)*(short *)((char *)state + 0x1e));
    WriteBackPropEntry(state);
    return 0;
}


/* Statement half of propowner (opcode 16032): stores the owner name, a Pascal string, at row +0x8e.
 * Both script arguments are descriptors (the prop name, then the owner name); there is no type-4
 * check, and the out-parameter order is the reverse of PropDeg / ActorValue (out1 = ESP'+0, out2 =
 * ESP'+8 at 0x00415f4a). */
short __cdecl
ActionCmd_PropOwner(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00415F40 */
{
    short  s1;
    short  u;
    short  a1[4], a2[4];
    /* Ground truth copies the owner name to record + 0x8e (0x00415fd3 LEA EAX,[ESP+0x9e], record
     * at ESP'+0x10), and 0x8e + 18 = 160 = sizeof(state): it is the record's tail, not a
     * throwaway `scratch` (which made propowner a silent no-op). */
    int    state[40];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    u = TakeScriptString(a2, (byte *)gScratchStr2);
    if (u != 0)
        return u;
    if (gScratchStr2[0] >= 0x10)
        return 0x1a;
    CopyPascalString((byte *)gScratchStr2, (byte *)state + 0x8e);
    WriteBackPropEntry(state);
    return 0;
}


/* Statement half of propvalue (opcode 16042): stores the second argument, an int, in the prop row
 * at +0x46. */
short __cdecl
ActionCmd_PropValue(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00416000 */
{
    short  s1;
    short   u;
    short  a1[4], a2[4];
    int    skip;
    int    state[40];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    if (a2[0] != 4)
        return 0x0e;
    /* 0x0041607b..0x00416083: MOV EAX,[ESP+0x2] (the second argument's int payload) / MOV
     * [ESP+0x56],EAX: with the row base at [ESP+0x10] this is a dword at row +0x46, the field
     * ScriptCmd_PropValue reads back. The store was missing (Ghidra's decompile omits it; only
     * the disassembly shows it), exactly as in ActionCmd_ActorValue, so every `propvalue (x, v)`
     * committed an unchanged row. */
    *(int *)((byte *)state + 0x46) = *(int *)(a2 + 1);
    WriteBackPropEntry(state);
    return 0;
}


/* Statement half of propzclip (opcode 16052): the second argument must be type 4; its low word is
 * stored in the row at +0x4a. */
short __cdecl
ActionCmd_PropZclip(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x004160a0 */
{
    short s1;
    short  u;
    short a1[4], a2[4];
    
    int   state[40];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    if (a2[0] != 4)   /* CMP word ptr [ESP],4 is local_b0 = ARG2's tag, not the row */
        return 0x0E;
    /* MOV AX,[ESP+2]; MOV [ESP+0x5a],AX: arg2 payload -> row byte +0x4a (0xa0-0x56). The
     * port never performed this store, so propzclip() was a no-op even when it passed. */
    *(short *)((char *)state + 0x4a) = (short)*(int *)(a2 + 1);
    WriteBackPropEntry(state);
    return 0;
}


/* Value half of propset (opcode 16027): returns the prop's set name, the Pascal string at row +94. */
short ScriptCmd_PropSet(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00416140 */
                               short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    /* One 0x9e-byte prop row, not a split. Ghidra names the field local as if it were a separate
     * object; its byte offset is (0xa0 - local number), and LookupPropEntry fills the whole
     * 158-byte row. Declaring the buffer at the FIELD OFFSET let that write run off the end of
     * the stack object (class 3, as in ScriptCmd_ActorSet). The other ScriptCmd_Prop* readers
     * below have the same single record. */
    byte  entryRec[160];   /* Pascal string field at byte offset 94 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, (unsigned int *)entryRec);
    if (status != 0)
        return status;

    return AllocScriptString(entryRec + 94, pResult);   /* its status is the result */
}


/* Script command propexists (opcode 20024): the same shape as ScriptCmd_ActorExists, using
 * LookupPropEntry. Returns a bool: any lookup failure is "no". */
short __cdecl ScriptCmd_PropExists(int *pCtx, short *pArgBlock,  /* 0x004161c0 */
                                            int *pTable, short *pResult,
                                            int *pSkipCount)
{
    short              status;
    unsigned int      lookupResult;
    short             argResult[4];
    unsigned int      dataBuffer[40];   /* 160 bytes: the exe's frame is 0xa8 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, (byte *)&gScratchStr);
    if (status != 0)
        return status;

    lookupResult = LookupPropEntry((byte *)&gScratchStr, dataBuffer);
    /* 0x0041621b: any failure is "no", not only status 10 */
    if ((short)lookupResult != 0)
        *(int *)(pResult + 1) = 0;
    else
        *(int *)(pResult + 1) = 1;
    pResult[3] = 0;
    *pResult   = 2;
    return 0;
}


/* Value half of propowner (opcode 16032): returns the owner name, the Pascal string at row +142. */
short ScriptCmd_PropOwner(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00416250 */
                               short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    byte  entryRec[160];   /* Pascal string field at byte offset 142 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, (unsigned int *)entryRec);
    if (status != 0)
        return status;

    return AllocScriptString(entryRec + 142, pResult);   /* its status is the result */
}


/* Value half of propvalue (opcode 16042): returns the dword at row +70 (0x46) as a type-4 int
 * (0x00416330: MOV ECX,[ESP+0x4e], row base at [ESP+0x8]); the field ActionCmd_PropValue writes. */
short ScriptCmd_PropValue(int *pCtx, short *pArgBlock, int *pTable,  /* 0x004162d0 */
                            short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    int   stateRow[40];    /* dword field at byte offset 70 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, (unsigned int *)stateRow);
    if (status != 0)
        return status;

    *(int *)(pResult + 1) = *(int *)((char *)stateRow + 70);
    *pResult = 4;   /* type: int */
    pResult[3] = 0;
    return 0;
}


/* Value half of propzclip (opcode 16052): returns the short at row +74 as a type-4 int. */
short ScriptCmd_PropZclip(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00416360 */
                            short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    int   stateRow[40];    /* short field at byte offset 74 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, (unsigned int *)stateRow);
    if (status != 0)
        return status;

    *pResult = 4;   /* type: int */
    *(int *)(pResult + 1) = (int)*(short *)((char *)stateRow + 74);
    pResult[3] = 0;
    return 0;
}


/* Statement half of propstar (opcode 16020): sets the prop's star (node) name, row +0x6e, from the
 * second argument (a name of at most 15 characters). ActionCmd_PropSet with the copy target
 * swapped. */
short __cdecl ActionCmd_PropStar(uint32_t pCtx, int pArgBlock, int *pTable, int *pSubSkip) /* 0x004163F0 */
{
    /* ActionCmd_PropSet with the copy target swapped. One 0x9e-byte row (LookupPropEntry writes
     * all of it); fields: +0x12 is-3D flag, +0x18..+0x1f node position, +0x5e set name (compared
     * against the current scene), +0x6e star name (the argument, and what the position is looked
     * up by). */
    typedef struct { int w[2]; } Pos8;
    short    s1;
    short    u, u3;
    short    a1[4], a2[4];
    int      state[40];
    short    pos[4];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    u = TakeScriptString(a2, (byte *)gScratchStr2);
    if (u != 0)
        return u;
    if (gScratchStr2[0] >= 0x10)   /* length byte, not the address */
        return 0x1a;
    CopyPascalString((byte *)gScratchStr2, (byte *)state + 0x6e);
    u3 = PascalStringsEqualNoCase((byte *)state + 0x5e, (byte *)&DAT_0048a7b4);
    if (u3 != 0) {
        u3 = FindWaypointNodePosition((byte *)state + 0x6e, pos);
        if (u3 != 0) {
            /* the whole 8-byte record over +0x18, then the first word put
             * back: row+0x1a/+0x1c/+0x1e = pos[1..3] */
            short keep = *(short *)((char *)state + 0x18);
            *(Pos8 *)((char *)state + 0x18) = *(Pos8 *)pos;
            *(short *)((char *)state + 0x18) = keep;
        }
    }
    *(short *)((char *)state + 0x12) = 1;
    WriteBackPropEntry(state);
    return 0;
}


/* Value half of propstar (opcode 16020): returns the star name, the Pascal string at row +110. */
short ScriptCmd_PropStar(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00416520 */
                               short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    byte  entryRec[160];   /* Pascal string field at byte offset 110 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, (unsigned int *)entryRec);
    if (status != 0)
        return status;

    return AllocScriptString(entryRec + 110, pResult);   /* its status is the result */
}


/* Statement half of propview (opcode 16025): sets the prop's view, the sprite binding named at
 * +0x7e, with its frame count (+0x22, from SetPropFrameCountForPose) and frame counter (+0x20 =
 * count - 1). */
short __cdecl
ActionCmd_PropView(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x004165A0 */
{
    short  s1;
    short  u, u3;
    short  a1[4], a2[4];
    int    state[40];
    short  c;

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    u = TakeScriptString(a2, (byte *)gScratchStr2);
    if (u != 0)
        return u;
    if (gScratchStr2[0] >= 0x10)
        return 0x1a;
    u3 = SetPropFrameCountForPose(state, (byte *)gScratchStr2);
    if (u3 != 0)
        return 10;
    /* `local_80 = local_7e - 1`: both are fields INSIDE the row at local_a0, byte 0xa0-0x7e =
     * 0x22 (the ushort SetPropFrameCountForPose just wrote at +34) and 0xa0-0x80 = 0x20. The port
     * took the Ghidra local NAMES as byte offsets (+0x7e / +0x80), scribbling 96 bytes further
     * into the row (class 3). */
    c = *(short *)((char *)state + 0x22);
    *(short *)((char *)state + 0x20) = c - 1;
    /* local_22[34] is ALSO inside the row: ebp-0x22 = row+0x7e, the view name that
     * LookupStateSpriteFrameGeometryByAngle compares against each binding's name to choose the
     * sprite. Copied to a scratch local, propview changed the view index but never the binding,
     * so every prop kept its default view. */
    CopyPascalString((byte *)gScratchStr2, (byte *)state + 0x7e);
    DF_TRACE("propview: prop='%.*s' view='%.*s' idx=%d",
             (int)((byte *)gScratchStr)[0], (const char *)gScratchStr + 1,
             (int)gScratchStr2[0], (const char *)gScratchStr2 + 1, (int)(c - 1));
    WriteBackPropEntry(state);
    return 0;
}


/* Value half of propview (opcode 16025): returns the view name, the Pascal string at row +126. */
short ScriptCmd_PropView(int *pCtx, short *pArgBlock, int *pTable,  /* 0x004166a0 */
                               short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    byte  entryRec[160];   /* Pascal string field at byte offset 126 */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, (unsigned int *)entryRec);
    if (status != 0)
        return status;

    return AllocScriptString(entryRec + 126, pResult);   /* its status is the result */
}


/* Statement half of propdeg (opcode 16016): the second script argument must be type 4; its low byte
 * is stored at row +0x18. */
short __cdecl
ActionCmd_PropDeg(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00416720 */
{
    short  s1;
    short  u;
    short  a1[4], a2[4];
    int    state[40];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    if (a2[0] != 4)
        return 0x0e;
    /* 0x004167a8 MOV word ptr [ESP+0x2c],AX -- record at ESP'+0x10 with one
     * pending PUSH, so the field is record + 0x18, not +0x88. */
    DF_TRACE("dial: propdeg SET '%.*s' = %d (raw %d)",
             (int)(unsigned char)((byte *)gScratchStr)[0],
             (const char *)&((byte *)gScratchStr)[1], *(int *)&a2[1] & 0xff, *(int *)&a2[1]);
    *(ushort *)((char *)state + 0x18) = (ushort)(*(int *)&a2[1] & 0xff);
    WriteBackPropEntry(state);
    return 0;
}


/* Statement half of propdist (opcode 16013): for a 2-D prop (is-3D word +0x12 zero) and a negative
 * value, stores it as the render-order key (+0x26). */
short __cdecl
ActionCmd_PropDist(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x004167C0 */
{
    short  s1;
    short   u;
    int    skip, ival;
    short  a1[4], a2[4];
    int    state[40];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    if (a2[0] != 4)
        return 0x0e;
    /* `if ((local_8e == 0) && (local_ae < 0)) local_7a = (short)local_ae;` -- local_8e and
     * local_7a are FIELDS of the 0x9e-byte row at local_a0, not separate locals: +0x12 is the
     * is-3D flag CalcPropRenderParams reads as param_4[9], and +0x26 is param_4[0x13], the
     * render-order key that it copies to the scratch row's +0x0C and
     * SortCalcScratchRowsByHotspotBand sorts on (descending, signed). The port tested the
     * ARGUMENT's high half instead of the row flag and used the local name 0x7a as the offset
     * (class 3), so the key was never written, every row kept the builder's 0xffff default and
     * the sort was a no-op: the menu-bar halo ("light", which gets no propdist and so keeps -1)
     * painted over the life ring ("life", given a negative distance), and the next click
     * hit-tested as "light". Only negative values are stored, by design. */
    if (*(short *)((char *)state + 0x12) == 0 && *(int *)&a2[1] < 0)
        *(short *)((char *)state + 0x26) = (short)*(int *)&a2[1];
    WriteBackPropEntry(state);
    (void)u;
    return 0U;
}


/* Value half of propdist (opcode 16013): parallel to ScriptCmd_ActorDist, with LookupPropEntry and
 * CalcPropRenderParams. The stored key (row +0x26) is returned as-is for a 2-D prop; otherwise the
 * calculated distance. */
short ScriptCmd_PropDist(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00416870 */
                                short *pResult, int *pSkipCount)
{
    short   uVar;
    short  argResult[4];
    /* Two records, both of which Ghidra splits (0x00416870): local_c4[12] + local_b8 are the
     * 36-byte calc output, local_b8 its short index 6, the render-order key; local_a0[4] +
     * local_8e + local_7a are the 0x9e-byte row, whose fields sit at +0x12 (is-3D) and +0x26 (the
     * propdist value). LookupPropEntry fills all 158 bytes of it. */
    short  calcOut[18];    /* local_c4 -- 36 bytes; [6] is local_b8 */
    int    stateRow[40];   /* local_a0 -- the full state row */

    uVar = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                              (int *)argResult, pSkipCount);
    if (uVar != 0)
        return uVar;

    uVar = TakeScriptString(argResult, &gScratchStr);
    if (uVar != 0)
        return uVar;

    uVar = LookupPropEntry(&gScratchStr, (unsigned int *)stateRow);
    if (uVar != 0)
        return uVar;

    {
        short *pData = (short *)stateRow;

        /* 0x004168d4: as ScriptCmd_ActorDist -- 3D case first, and
         * 0x00418d90 really takes (row, out) only. */
        if (pData[9] != 0) {
            if (CalcPropRenderParams(pData, calcOut) != 0)
                *(int *)(pResult + 1) = (int)calcOut[6];
            else
                *(int *)(pResult + 1) = 32000;
        } else {
            /* not 3D: the stored value is returned as-is */
            *(int *)(pResult + 1) = (int)pData[19];
        }
    }
    pResult[3] = 0;
    *pResult = 4;   /* type: int */
    return 0;
}


/* Value half of propdeg (opcode 16016): returns the short at row +0x18, the field ActionCmd_PropDeg
 * writes, as a type-4 int.
 *
 * Port note: it read +0x12 (the pose flag propxy clears), so `propdeg "x"` answered 0 forever and
 * the radio tuning knob computed every drag delta against 0. */
short ScriptCmd_PropDeg(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00416930 */
                           short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    short dataBuffer[80];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, dataBuffer);
    if (status != 0)
        return status;

    DF_TRACE("dial: propdeg GET '%.*s' -> %d",
             (int)(unsigned char)gScratchStr[0], (const char *)&gScratchStr[1],
             (int)dataBuffer[12]);
    *pResult = 4;   /* type: int */
    *(int *)(pResult + 1) = (int)dataBuffer[12]   /* +0x18 */;
    pResult[3] = 0;
    return 0;
}


/* Statement half of propspeed (opcode 16026): stores the second argument, clamped to at least 1,
 * as a short in the prop row at +0x24. */
short __cdecl
ActionCmd_PropSpeed(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x004169C0 */
{
    short  s1;
    short   u;
    int    skip, ival;
    short  a1[4], a2[4];
    int    state[40];
    short  v7c;

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    if (a2[0] != 4)
        return 0x0e;
    *(short *)((char *)state + 0x24) = (short)*(int *)&a2[1];
    if (*(short *)((char *)state + 0x24) < 1)
        *(short *)((char *)state + 0x24) = 1;   /* 0x004169c0: local_7c = ebp-0x7c = record +0x24 */
    /* ONE store: the former second store at +0x7c took Ghidra's local NAME as a byte offset and
     * landed inside the prop's name field at +0x6e (see ActionCmd_ActorSpeed). */
    WriteBackPropEntry(state);
    return 0U;
}


/* Statement half of propscale (opcode 16028): stores the second argument, clamped to at least 0,
 * as a dword in the prop row at +0x28. */
short __cdecl
ActionCmd_PropScale(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00416A70 */
{
    short  s1;
    short   u;
    int    skip, ival;
    short  a1[4], a2[4];
    int    state[40];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    if (a2[0] != 4)
        return 0x0e;
    *(int *)((char *)state + 0x28) = *(int *)&a2[1];
    if (*(int *)((char *)state + 0x28) < 0)
        *(int *)((char *)state + 0x28) = 0;   /* 0x00416a70: local_78 = ebp-0x78 = record +0x28 */
    /* ONE store; the former +0x78 twin was the local's name, not an offset. */
    WriteBackPropEntry(state);
    return 0U;
}


/* Value half of propscale (opcode 16028): returns the dword at row +40 as a type-4 int. */
short ScriptCmd_PropScale(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00416b20 */
                            short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    short dataBuffer[80];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, dataBuffer);
    if (status != 0)
        return status;

    *(int *)(pResult + 1) = *(int *)(dataBuffer + 20);  /* dword at byte 40 */
    *pResult = 4;   /* type: int */
    pResult[3] = 0;
    return 0;
}


/* Value half of propspeed (opcode 16026): returns the short at row +36, sign-extended, as a type-4
 * int. */
short ScriptCmd_PropSpeed(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00416bb0 */
                            short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    short dataBuffer[80];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, dataBuffer);
    if (status != 0)
        return status;

    *pResult = 4;   /* type: int */
    *(int *)(pResult + 1) = (int)dataBuffer[18];
    pResult[3] = 0;
    return 0;
}


/* Script command propwarm (opcode 12086): looks the prop up and re-attaches its render children
 * (ReattachStateRowRenderChildren). */
short __cdecl
ActionCmd_PropWarm(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x00416c40 */
{
    short  u;
    short a[4];
    int   stateRow[40];  /* local_a0 */

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)a, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a, gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry(gScratchStr, stateRow);
    if (u != 0)
        return u;
    ReattachStateRowRenderChildren(stateRow);
    return 0U;
}


/* Script command shopwarm (opcode 12087): looks the shop up by name and re-attaches the render
 * children of every prop of that shop (rows whose handle at +2 matches). */
short __cdecl
ActionCmd_ShopWarm(int pCtx, short *pArgBlock, int *pTable, int *pSubSkip)  /* 0x00416cc0 */
{
    /* The exe returns ax unwidened like every handler; byte-exact (2 bytes of
     * loop-compare orientation, as ActionCmd_CastWarm) once this and its
     * df_protos.h prototype are `short` -- the prototype still says `uint`. */
    short u;
    short a[4];
    int   slot3[7];
    int  *row;
    int   iEntry;

    u = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)a, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a, gScratchStr);
    if (u != 0)
        return u;
    u = LookupShopEntryByName(gScratchStr, slot3);
    if (u != 0)
        return u;
    row = (int *)GetResDataPtr(gPropTable);
    for (iEntry = 0; iEntry < gPropCount; iEntry++) {
        if (*(int *)((char *)row + 2) == slot3[0])
            ReattachStateRowRenderChildren(row);
        row = (int *)((char *)row + 0x9e);
    }
    UnlockResObject(gPropTable);
    return 0;
}


/* Script command proplock (opcode 12108): a name and a bool. True warms the prop's render children
 * (WarmPropRenderChildren), false unwarms them, and the row is written back. */
short __cdecl
ActionCmd_PropLock(int pCtx, int pArgBlock, int *pTable, int *pSubSkip) /* 0x00416D70 */
{
    short  s1;
    short  u;
    short  a1[4], a2[4];
    int    skip, second, stateBuf[40];
    int   *pState;

    pState = stateBuf;
    s1     = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u      = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    if (a2[0] != 2)     /* the flag's tag (0x00416dc1 CMP [ESP],2); a1 is the descriptor, type 3 */
        return 0x0E;
    u = LookupPropEntry((byte *)gScratchStr, pState);
    if (u != 0)
        return u;
    if (*(int *)&a2[1] != 0)
        WarmPropRenderChildren((int)pState);
    else
        UnwarmPropRenderChildren((int)pState);
    WriteBackPropEntry(pState);
    return 0;
}


/* Script command shoplock (opcode 12109): as proplock, for every prop of the named shop. */
short __cdecl
ActionCmd_ShopLock(int pCtx, int pArgBlock, int *pTable, int *pSubSkip) /* 0x00416E20 */
{
    int    k;
    short  u;
    short a1[4];
    short a2[4];
    int    row[7];
    char  *p;

    u = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    if (a2[0] != 2)     /* the flag's tag (0x00416e70); a1 is the descriptor, type 3 */
        return 0x0E;
    u = LookupShopEntryByName((byte *)gScratchStr, row);
    if (u != 0)
        return u;
    p = (char *)GetResDataPtr((void *)gPropTable);
    for (k = 0; k < gPropCount; k++) {
        /* the state row's slot-3 handle is the int at BYTE +2 (0x00416eba
         * CMP [ESI+0x2]); the port had read iVar3[2], byte +8 */
        if (*(int *)(p + 2) == row[0]) {
            if (*(int *)&a2[1] != 0)
                WarmPropRenderChildren((int)p);
            else
                UnwarmPropRenderChildren((int)p);
        }
        p += 0x9E;
    }
    UnlockResObject((int *)gPropTable);
    return 0;
}


/* Statement half of propvisible (opcode 16015): stores a bool, 0 or 1, in the prop row's first
 * word. (The actor twin, ActionCmd_ActorVisible, is opcode 16001.) */
short __cdecl
ActionCmd_PropVisible(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00416F00 */
{
    short  s1;
    short   u;
    int    skip;
    short  a1[4], a2[4];
    short *pHead;
    int    state[40];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    if (a2[0] != 2)
        return 0x0e;
    pHead = (short *)state;
    if (a2[1] != 0) {                   /* test / je: the set case first */
        pHead[0] = 1;
    } else {
        if (pHead[0] > 0)
            pHead[0] = 0;
    }
    DF_TRACE("propvisible: prop='%.*s' -> %d", (int)((byte *)gScratchStr)[0],
             (const char *)gScratchStr + 1, (int)*(short *)state);
    WriteBackPropEntry(state);
    return 0U;
}


/* Value half of propvisible (opcode 16015): a bool, whether the prop's first word is above 0. */
short ScriptCmd_PropVisible(int *pCtx, short *pArgBlock, int *pTable,  /* 0x00416fc0 */
                             short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    short dataBuffer[80];  /* 160-byte result from LookupPropEntry */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, dataBuffer);
    if (status != 0)
        return status;

    if (dataBuffer[0] > 0) {            /* cmp 0 / jle, as the exe branches */
        *(int *)(pResult + 1) = 1;
    } else {
        *(int *)(pResult + 1) = 0;
    }
    pResult[3] = 0;
    *pResult = 2;   /* type: short (boolean) */
    return 0;
}


/* Script command prophide (opcode 12105): as actorhide for props; adjusts the visible count of the
 * named prop, or of every prop for the name "all". */
short __cdecl
ActionCmd_PropHide(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00417060 */
{
    int    i;
    short  u;
    short a1[4];
    short a2[4];
    short  state80[80];
    short *pRow0;

    u = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    if (a2[0] != 2)     /* the flag's tag (0x004170ba); a1 is the descriptor, type 3 */
        return 0x0E;
    u = LookupPropEntry((byte *)gScratchStr, (int *)state80);
    if (u != 0) {
        /* Name not in table: only proceed if the requested name is the global wildcard. */
        if (PascalStringsEqualNoCase(
                (byte *)UpdatePascalLength((char *)&kStrAll), (byte *)gScratchStr) == 0)
            return u;
        pRow0 = (short *)GetResDataPtr((void *)gPropTable);
        for (i = 0; i < gPropCount; i++) {
            if (*(int *)&a2[1] != 0)
                pRow0[0]--;
            else if (++pRow0[0] > 0)
                pRow0[0] = 1;
            pRow0 = (short *)((char *)pRow0 + 0x9E);
        }
        UnlockResObject((int *)gPropTable);
        return 0;
    }
    if (*(int *)&a2[1] != 0)
        state80[0]--;
    else if (++state80[0] > 0)
        state80[0] = 1;
    WriteBackPropEntry((int *)state80);
    return 0;
}


/* Script command pointinprop (opcode 20040): returns a bool, whether a point hits the prop's drawn
 * image. The same shape and calc-output field map as ScriptCmd_PointInActor. */
short __cdecl ScriptCmd_PointInProp(int *pCtx, int pArgBlock, int *pTable,
                                                  short *pResult, int *pSkipCount) /* 0x004171B0 */
{
    short           sVar;
    short           a1[4], a2[4];
    short           row160[80];
    short           ccMix[18];   /* the 36-byte calc output */
    int             packedPt;
    int            *hChild;

    /* Same shape as ScriptCmd_PointInActor: skip count passed straight
     * through, default result filled after the argument checks. */
    sVar = EvalTwoScriptArgs((int)pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSkipCount);
    if (sVar != 0)
        return sVar;
    sVar = TakeScriptString(a1, gScratchStr);
    if (sVar != 0)
        return sVar;
    sVar = LookupPropEntry((byte *)gScratchStr, (int *)row160);
    if (sVar != 0)
        return sVar;
    if (a2[0] != 4)
        return 0xE;

    *pResult              = 2;
    packedPt              = *(int *)((char *)a2 + 2);
    *(int *)(pResult + 1) = 0;
    pResult[3]            = 0;

    if (CalcPropRenderParams(row160, ccMix) != 0
        && PointInSceneRect(*(ScenePointYX *)&packedPt, ccMix + 8)) {
    /* Same calc-output field map as ScriptCmd_PointInActor (0x0040f380);
     * 0x004171b0 is its state-row twin. */
    sVar = AttachRenderChild(*(int **)((byte *)ccMix + 0x18),
                             *(unsigned int *)((byte *)ccMix + 0x1c), (int **)&hChild);
    if (sVar != 0)
        FatalError(sVar, 0xfd8);
    if (ccMix[6] < 0)
        *(int *)(pResult + 1) = ProbeSpriteHotspotStripHit(hChild, *(int *)ccMix, ccMix[5], *(ScenePointYX *)&packedPt);
    else
        *(int *)(pResult + 1) = ProbeScaledHotspotStripHit(hChild, *(int *)ccMix, ccMix[5],
                                    *(ScenePointYX *)((byte *)ccMix + 0x20), *(ScenePointYX *)&packedPt);
    sVar = DetachRenderChild(*(int *)((byte *)ccMix + 0x18), *(unsigned int *)((byte *)ccMix + 0x1c));
    if (sVar != 0)
        FatalError(sVar, 0xfd9);
    }
    return 0;
}


/* Statement half of propxyz (opcode 16017): a prop and three ints. Stores x, y, z at +0x1a /
 * +0x1c / +0x1e, sets the is-3D word (+0x12) to 1 and copies the wildcard name into the row at
 * +0x6e. The shape of ActionCmd_ActorXyz. */
short __cdecl
ActionCmd_PropXyz(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00417340 */
{
    /* four separate 8-byte typed values (frame: x +0, y +8, z +0x10,
     * desc +0x18, the row +0x20), each type tested with its own return --
     * the shape of ActionCmd_ActorXyz */
    short  s1;
    short   u;
    short  o2[4];
    short  o3[4];
    short  o4[4];
    short  o1[4];
    int    state[40];

    s1 = EvalFourScriptArgs(pCtx, pArgBlock, pTable, o1, o2, o3, o4, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(o1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    if (o2[0] != 4)
        return 0x0e;
    if (o3[0] != 4)
        return 0x0e;
    if (o4[0] != 4)
        return 0x0e;
    /* 0x004173f7..0x0041742f: x -> +0x1a, y -> +0x1c, z -> +0x1e, +0x12 = 1,
     * wildcard name copied INTO the row at +0x6e. */
    *(short *)((char *)state + 0x1a) = (short)*(int *)(o2 + 1);
    *(short *)((char *)state + 0x1c) = (short)*(int *)(o3 + 1);
    *(short *)((char *)state + 0x1e) = (short)*(int *)(o4 + 1);
    *(short *)((char *)state + 0x12) = 1;
    CopyPascalString((byte *)UpdatePascalLength((char *)&kStrCustom), (byte *)state + 0x6e);
    WriteBackPropEntry(state);
    return 0;
}


/* Value half of propxyz (opcode 16017): like ScriptCmd_ActorXyz on a prop row. Index 1-4 selects
 * the field (x, y, z, or x and y packed). */
short ScriptCmd_PropXyz(int *pCtx, int pArgBlock, int *pTable,  /* 0x00417450 */
                       short *pResult, int *pSkipCount) 
{
    short sVar;
    short  status;
    short argResult1[4];
    short argResult2[4];
    short dataBuffer[80];
    short packed[2];       /* case 4: two words packed into one int */

    sVar = EvalTwoScriptArgs(pCtx, pArgBlock, pTable,
                       (undefined4 *)argResult1, (undefined4 *)argResult2, pSkipCount);
    status = (uint)(unsigned short)sVar;
    if (sVar != 0)
        return status;

    status = TakeScriptString(argResult1, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, dataBuffer);
    if (status != 0)
        return status;

    if (argResult2[0] != 4)
        return 0x0e;

    *pResult = 4;
    pResult[3] = 0;
    switch (*(int *)(argResult2 + 1) - 1) {
    case 0:
        *(int *)(pResult + 1) = (int)dataBuffer[13];
        return 0;
    case 1:
        *(int *)(pResult + 1) = (int)dataBuffer[14];
        return 0;
    case 2:
        *(int *)(pResult + 1) = (int)dataBuffer[15];
        return 0;
    case 3:
        /* the exe builds the pair in a short[2] local and reads it back as a dword */
        packed[1] = dataBuffer[13];
        packed[0] = dataBuffer[14];
        *(int *)(pResult + 1) = *(int *)packed;
        return 0;
    default:
        return 0x0e;
    }
}


/* Statement half of propxy (opcode 16018): a prop and a 2-D position. Stores x at +0x16 and y at
 * +0x14, clears the is-3D word (+0x12) and sets the short at +0x26 to -1 if it is not negative. */
short __cdecl
ActionCmd_PropXy(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00417570 */
{
    short  u;
    short  name[4];
    short  xArg[4];
    short  yArg[4];
    int    state[40];

    u = EvalThreeScriptArgs(pCtx, pArgBlock, pTable, (int *)name, (int *)xArg, (int *)yArg, pSubSkip);
    if (u != 0)
        return u;
    u = TakeScriptString(name, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    if (xArg[0] != 4)
        return 0x0e;
    if (yArg[0] != 4)
        return 0x0e;
    /* 0x00417607..0x00417628: x -> +0x16, y -> +0x14; if short +0x26 >= 0 set -1;
     * +0x12 = 0. The old `state + 0x7a` test was local_7a's NAME as an offset. */
    DF_TRACE("propxy: name='%.*s' x=%d y=%d (row had y=%d x=%d) types=%d,%d",
             (int)((byte *)gScratchStr)[0], (const char *)gScratchStr + 1,
             *(int *)(xArg + 1), *(int *)(yArg + 1),
             (int)*(short *)((char *)state + 0x14), (int)*(short *)((char *)state + 0x16),
             (int)xArg[0], (int)yArg[0]);
    *(short *)((char *)state + 0x16) = (short)*(int *)(xArg + 1);
    *(short *)((char *)state + 0x14) = (short)*(int *)(yArg + 1);
    if (*(short *)((char *)state + 0x26) >= 0)
        *(short *)((char *)state + 0x26) = -1;
    *(short *)((char *)state + 0x12) = 0;
    WriteBackPropEntry(state);
    return 0;
}


/* Value half of propxy (opcode 16018): like ScriptCmd_ActorXy on a prop row. Index 1 / 2 / 3
 * returns the high word, the low word or the whole 32-bit position. */
short ScriptCmd_PropXy(int *pCtx, int pArgBlock, int *pTable,  /* 0x00417650 */
                      short *pResult, int *pSkipCount) 
{
    short sVar;
    short  status;
    short argResult1[4];
    short argResult2[4];
    short dataBuffer[80];

    sVar = EvalTwoScriptArgs(pCtx, pArgBlock, pTable,
                       (undefined4 *)argResult1, (undefined4 *)argResult2, pSkipCount);
    status = (uint)(unsigned short)sVar;
    if (sVar != 0)
        return status;

    status = TakeScriptString(argResult1, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, dataBuffer);
    if (status != 0)
        return status;

    if (argResult2[0] != 4)
        return 0x0e;

    *pResult = 4;
    pResult[3] = 0;

    switch (*(int *)(argResult2 + 1)) {   /* a compare chain on one load: a switch */
    case 1:
        *(int *)(pResult + 1) = (int)dataBuffer[11];
        return 0;
    case 2:
        *(int *)(pResult + 1) = (int)dataBuffer[10];
        return 0;
    case 3:
        *(int *)(pResult + 1) = *(int *)(dataBuffer + 10);
        return 0;
    }
    return 0x0e;
}


/* Statement half of propis3d (opcode 16019): a bool stored in the prop's is-3D word (+0x12). If it
 * is 0, the short at +0x26 is forced to -1 unless it is already negative. */
short __cdecl
ActionCmd_PropIs3d(int pCtx, int pArgBlock, int *pTable, int *pSubSkip)  /* 0x00417740 */
{
    short  s1;
    short   u;
    int    skip;
    short  a1[4], a2[4];
    int    state[40];

    s1 = EvalTwoScriptArgs(pCtx, pArgBlock, pTable, (int *)a1, (int *)a2, pSubSkip);
    u  = (uint)(unsigned short)s1;
    if (s1 != 0)
        return u;
    u = TakeScriptString(a1, (byte *)gScratchStr);
    if (u != 0)
        return u;
    u = LookupPropEntry((byte *)gScratchStr, state);
    if (u != 0)
        return u;
    if (a2[0] != 2)
        return 0x0e;
    /* The flag is STORED at row+0x12 (MOV [ESP+0x22],AX, the twin of
     * ActionCmd_ActorIs3d's), which Ghidra's decompile drops. Then, when
     * it is 0, row+0x26 (local_7a, the field ActionCmd_PropXy forces to -1; +0x7a
     * was the local's NAME as an offset) is forced to -1 unless already negative. */
    *(short *)((char *)state + 0x12) = (short)*(int *)&a2[1];
    if (*(short *)((char *)state + 0x12) == 0 && *(short *)((char *)state + 0x26) >= 0)
        *(short *)((char *)state + 0x26) = -1;
    WriteBackPropEntry(state);
    return 0;
}


/* Value half of propis3d (opcode 16019): returns the short at row +18 as a type-2 result. */
short ScriptCmd_PropIs3d(int *pCtx, short *pArgBlock, int *pTable,  /* 0x004177f0 */
                             short *pResult, int *pSkipCount)
{
    short  status;
    short argResult[4];
    short dataBuffer[80];

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)argResult, pSkipCount);
    if (status != 0)
        return status;

    status = TakeScriptString(argResult, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(&gScratchStr, dataBuffer);
    if (status != 0)
        return status;

    *pResult = 2;   /* type: short */
    *(int *)(pResult + 1) = (int)dataBuffer[9];
    pResult[3] = 0;
    return 0;
}


#undef PSS_SHOPNAME
#undef PSS_ROWNAME

/* Not a handler: called only from ActionCmd_OpenShopFile. After a shop and its props are
 * registered, builds `"<shop>", openshop()` and runs it as a sendtoshop (RunShopMessage), then
 * for every prop row whose context pointer (+2) belongs to this shop builds `"<prop>",
 * openprop()` and runs it as a sendtoprop. That is how each prop's own script places it (propxy)
 * when its shop opens. Returns the first failing lookup or dispatch status, else 0. The twin of
 * CompleteCastOpen. */
short __cdecl CompleteShopOpen(int param_1)  /* 0x00417880 */
{
    /* Returns the first failing lookup/dispatch status, else 0 (0x00417880
     * ends in `and eax,0xffff0000`; ActionCmd_OpenShopFile returns its ax). */
    /* One 0x9E-byte slot-2 row copied whole (rep movsd 0x27 + movsw). */
    typedef struct { char b[0x9e]; } Rec158;
    int    ownerKey;
    short  status;
    int    i;
    int    shopEntry[7];  /* local_bc */
    Rec158 propRow;  /* local_a0 */
    int   *pNode;

    ownerKey = *(int *)((char *)(UINT_PTR)(unsigned)param_1 + 0x530);
    status = LookupShopEntryByNode(param_1, shopEntry);
    if (status != 0)
        return status;

    DF_TRACE("CompleteShopOpen: entryName len=%d '%.*s'",
             (int)((byte *)&shopEntry[3])[0], (int)((byte *)&shopEntry[3])[0],
             (char *)&shopEntry[3] + 1);

    CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);   /* the destination is pushed before the lookup call */
    AppendPascalString((byte *)&shopEntry[3], &gScratchStr);
    AppendPascalString((byte *)UpdatePascalLength(kStrQuoteCommaOpenshopCall), &gScratchStr);   /* ", openshop() */
    status = RunShopMessage(&gScratchStr);
    DF_TRACE("CompleteShopOpen: shop script '%.*s' -> status=0x%x (ctx=%p ctx+530=%d)",
             (int)gScratchStr[0], (const char *)&gScratchStr + 1, status,
             (void *)(intptr_t)param_1, ownerKey);
    if (status != 0)
        return status;

    /* 0x00417880 has no null checks on the row table or on the row's
     * context pointer (the port had both, with no DF_FIX switch). */
    for (i = 0; i < gPropCount; i++) {
            propRow = ((Rec158 *)GetResDataPtr(gPropTable))[i];
            UnlockResObject((int *)gPropTable);
            /* Ground truth (0x00417880 / 0x00417ca0): `propRow._2_4_`, the context pointer at
             * row +2 where InitPropEntry stores it. Reading +0 took the visibility short and half
             * the pointer, so no row ever matched and the per-prop registration script, where each
             * prop places itself with propxy, never ran: the menu-bar props (life, navarrow, ...)
             * sat at anchor (0,0). */
            pNode = *(int **)((char *)&propRow + 2);
            DF_TRACE("CompleteShopOpen: row %d ctxPtr=%p want=%d name='%.*s'",
                     i, (void *)pNode, ownerKey,
                     (int)(byte)propRow.b[0x4E], (char *)&propRow + 0x4F);
            if (*(int *)((char *)pNode + 0x530) == ownerKey) {
                CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
                AppendPascalString((byte *)&propRow + 0x4E, &gScratchStr);
                AppendPascalString((byte *)UpdatePascalLength(kStrQuoteCommaOpenpropCall), &gScratchStr);   /* ", openprop() */
                status = RunPropMessage(&gScratchStr);
                if (status != 0)
                    return status;
            }
    }
    return 0;
}


/* Script command closeshopfile (opcode 12057): as ActionCmd_CloseCastFile for the shop file.
 * Finds the shop (LookupShopEntryByName), pre-shuts it down (ShopPreShutdownStep), resolves its
 * node (FindRenderNodeByContextValue; returns early if there is none), removes every prop row
 * whose handle at +2 is that shop (TeardownPropEntry, compacting gPropTable in place), calls
 * CloseShopNode and removes the shop row by moving the last row into its position. */
short __cdecl ActionCmd_CloseShopFile(int pCtx, short *pArgBlock,  /* 0x004179e0 */
                                           int *pTable, int *pSubSkip)
{
    /* The slot-2 row (0x9E bytes) and slot-3 entry (0x1C) are copied as whole
     * structs: rep movsd 0x27 + movsw / rep movsd 7. */
    typedef struct { char b[0x9e]; } Rec158;
    typedef struct { int d[7]; } Rec28;
    Rec158 *piRow;          /* row being examined, captured pre-decrement */
    int    resolvedHandle;  /* node handle from FindRenderNodeByContextValue */
    int    slot3Idx;        /* slot-3 table index from FindShopEntryIndexByNode */
    short  args[4];
    Rec158 stateEntry;      /* copy of one slot-2 state entry */
    int    slotEntry[7];    /* slot-3 context entry from LookupShopEntryByName / LookupShopEntryByNode */
    short   status;
    Rec158 *rows;
    int    iEntry;
    int    contextValue;    /* saved before the pre-shutdown scan */
    Rec28  *p3;

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable,
                                (int *)args, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(args, &gScratchStr);
    if (status != 0)
        return status;

    status = LookupShopEntryByName(gScratchStr, slotEntry);
    DF_TRACE("closeShop: LookupShopEntryByName('%.*s') -> err=%d",
             (int)gScratchStr[0], (const char *)gScratchStr + 1, (int)status);
    if (status != 0)
        return status;

    /* Ground truth reads the context value BEFORE the pre-shutdown scan (`iVar2 = *(int
     * *)(local_c4[0] + 0x530);` precedes the call) and passes the saved copy to
     * FindRenderNodeByContextValue; the port re-read it afterwards. */
    contextValue = *(int *)(slotEntry[0] + 0x530);

    status = ShopPreShutdownStep(slotEntry[0]);
    DF_TRACE("closeShop: ShopPreShutdownStep -> err=%d", (int)status);
    if (status != 0)
        return status;

    /* Resolve context node; 0 = not found -- nothing to unregister */
    status = FindRenderNodeByContextValue(contextValue, &resolvedHandle);
    DF_TRACE("closeShop: FindRenderNodeByContextValue(%08x) -> %d",
             (unsigned)contextValue, (int)status);
    if (status == 0)
        return 0;

    status = LookupShopEntryByNode(resolvedHandle, slotEntry);
    DF_TRACE("closeShop: LookupShopEntryByNode -> err=%d", (int)status);
    if (status != 0)
        return status;

    status = FindShopEntryIndexByNode(slotEntry[0], &slot3Idx);
    DF_TRACE("closeShop: FindShopEntryIndexByNode -> err=%d", (int)status);
    if (status != 0)
        return status;

    rows = (Rec158 *)GetResDataPtr(gPropTable);
    for (iEntry = 0; iEntry < gPropCount; iEntry++)
    {
        piRow = &rows[iEntry];   /* GT puVar6 */
        stateEntry = *piRow;

        /* Match: context handle at byte offset +2 of state entry == slotEntry[0] */
        if (*(int *)((char *)&stateEntry + 2) == slotEntry[0])
        {
            TeardownPropEntry((int)&stateEntry);
            iEntry--;
            gPropCount--;

            /* Compact: overwrite THIS slot with the last one. The exe captures the row pointer
             * (puVar6) BEFORE the index is decremented and copies into that; the port recomputed
             * the destination from the already-decremented iEntry, clobbered the PREVIOUS row and
             * left the matched one in place, so the next iteration matched it again and shut it
             * down twice: a double DetachRenderChild, "Bad pack use count" (FatalError 0xFDA in
             * TeardownPropEntry). The decrement exists only so the trailing iEntry++ re-examines
             * this index, which now holds the moved-in last row. */
            *piRow = rows[gPropCount];
        }
    }

    UnlockResObject(gPropTable);
    ResizeResObject(gPropTable, (unsigned int)(gPropCount * 0x9e));

    CloseShopNode(slotEntry);

    gShopCount--;
    p3 = (Rec28 *)GetResDataPtr(gShopTable);
    p3[slot3Idx] = p3[gShopCount];   /* last entry into the target slot */

    UnlockResObject(gShopTable);
    ResizeResObject(gShopTable, (unsigned int)(gShopCount * 0x1c));
    return 0;
}


/* Unregisters the prop's property and object script bindings (name at +0x4e), unwarms its render
 * children and detaches the child at the selector stored at byte +10 of the 0x9e-byte row. The
 * twin of TeardownActorEntry. */
void __cdecl TeardownPropEntry(int pEntry)  /* 0x00417c00 */
{
    short s2;
    byte *p = (byte *)pEntry;

    RemoveLoopEntry((byte *)UpdatePascalLength((char *)kStrProp), p + 0x4E);
    UnwarmPropRenderChildren(pEntry);
    s2 = DetachRenderChild(*(int *)(p + 2), (unsigned int)*(int *)(p + 10));
    if (s2 != 0)
        FatalError(s2, 0xFDA);
}


/* Detaches child 0 and registers the shop's render node for finalisation. The twin of
 * CloseCastNode. */
void __cdecl CloseShopNode(int *pEntry)  /* 0x00417c50 */
{
    short s2;
    short s1;

    s2 = DetachRenderChild(*pEntry, 0u);
    if (s2 != 0)
        FatalError(s2, 0xFDB);
    s1 = DeferRenderNodeClose(*pEntry);
    if (s1 != 0)
        FatalError(s1, 0xFDC);
}


/* Runs `"<prop>", closeprop()` for each prop of the shop (RunPropMessage), then `"<shop>",
 * closeshop()` (RunShopMessage): the twin of CastPreShutdownStep. Finds the props by the +0x530
 * key of pNode; the prefixes are kStrPropScriptColon / kStrShopScriptColon. */
short __cdecl ShopPreShutdownStep(int pNode)  /* 0x00417ca0 */
{
    /* One 0x9E-byte prop row copied whole (rep movsd 0x27 + movsw). Byte-exact with a `short`
     * return type (the exe returns ax unwidened, as ActionCmd_CloseShopFile reads it);
     * df_protos.h still declares it `uint`, so the returns below sign-extend until that
     * prototype changes. 0x00417ca0 has no null checks on the row table or the row's context
     * pointer, and no memset (the port had all three). */
    typedef struct { char b[0x9e]; } Rec158;
    int         ownerKey;
    int         i;
    short       status;
    int         shopNode;  /* local_c0 */
    /* Two records, each of which Ghidra splits into a truncated array plus a separately named
     * field (ground truth 0x00417ca0): shopEntry[3] + local_b0[16] is ONE 28-byte shop entry (7
     * ints, name at +12), and propRow[78] + local_52[82] ONE 160-byte prop row (name at +0x4e).
     * Ported as separate locals and then memset to zero, both names came out empty and the
     * composed script paths were prefix + "" + ", closeprop()" / ", closeshop()": a path with no
     * object name is exactly the "No such object" (error 10) the radio panel OK button raised.
     * The exe has no memsets here. */
    int         shopEntry[7];  /* local_bc */
    Rec158      propRow;  /* local_a0 */

    ownerKey = *(int *)((char *)(UINT_PTR)(unsigned)pNode + 0x530);
    for (i = 0; i < gPropCount; i++) {
            propRow = ((Rec158 *)GetResDataPtr(gPropTable))[i];
            UnlockResObject((int *)gPropTable);
            /* the context pointer at row +2, as in CompleteShopOpen */
            if (*(int *)(*(int *)((char *)&propRow + 2) + 0x530) == ownerKey) {
                CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
                AppendPascalString((byte *)&propRow + 0x4e, &gScratchStr);
                AppendPascalString((byte *)UpdatePascalLength(kStrQuoteCommaClosepropCall), &gScratchStr);   /* ", closeprop() */
                status = RunPropMessage(&gScratchStr);
                if (status != 0)
                    return status;
            }
    }

    status = FindRenderNodeByContextValue(ownerKey, &shopNode);
    if (status == 0)
        return 0;
    status = LookupShopEntryByNode(shopNode, shopEntry);
    if (status != 0)
        return status;
    CopyPascalString((byte *)UpdatePascalLength(&kStrQuote), &gScratchStr);
    AppendPascalString((byte *)shopEntry + 12, &gScratchStr);
    AppendPascalString((byte *)UpdatePascalLength(kStrQuoteCommaCloseshopCall), &gScratchStr);   /* ", closeshop() */
    return RunShopMessage(&gScratchStr);
}


/* Script command propscript (opcode 12054). Evaluates one argument, checks the prop exists
 * (LookupPropEntry), builds "Prop Script: <name>" in gScratchStr2 and finalises it with
 * ClearScriptResult. Like ActionCmd_CastScript. */
short __cdecl ActionCmd_PropScript(int pCtx, short *pArgBlock,  /* 0x00417e20 */
                                           int *pTable, int *pSubSkip)
{
    short  status;
    short args[4];
    int   stateData[40];   /* 160-byte output from LookupPropEntry */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)args, pSubSkip);
    if (status == 0) {
        status = TakeScriptString(args, gScratchStr);
        if (status == 0) {
            status = LookupPropEntry(gScratchStr, stateData);
            if (status == 0) {
                CopyPascalString((byte *)UpdatePascalLength(kStrPropScriptColon), gScratchStr2);  /* "Prop Script: " */
                AppendPascalString(gScratchStr, gScratchStr2);
                ClearScriptResult(*(int *)((char *)stateData + 2), *(int *)((char *)stateData + 0xe),
                                     gScratchStr2, 0);   /* 0x00417eaa, loads made with 8 bytes still pushed */
                return 0;
            }
        }
    }
    return status;
}


/* Script command shopscript (opcode 12058): as propscript, validating with LookupShopEntryByName
 * and building "Shop Script: <name>". */
short __cdecl ActionCmd_ShopScript(int pCtx, short *pArgBlock,  /* 0x00417ed0 */
                                           int *pTable, int *pSubSkip)
{
    short  status;
    byte *pbVar2;
    byte *pbVar3;
    short args[4];
    int   slotEntry[7];   /* slot-3 entry output from LookupShopEntryByName */

    status = ProcessScriptBlock(pCtx, pArgBlock, pTable, (int *)args, pSubSkip);
    if (status != 0)
        return status;

    status = TakeScriptString(args, gScratchStr);
    if (status != 0)
        return status;

    status = LookupShopEntryByName(gScratchStr, slotEntry);
    if (status != 0)
        return status;

    CopyPascalString((byte *)UpdatePascalLength(kStrShopScriptColon), gScratchStr2);  /* "Shop Script: " */
    AppendPascalString(gScratchStr, gScratchStr2);
    ClearScriptResult(slotEntry[0], slotEntry[2], gScratchStr2, 0);   /* 0x00417f51 */
    status = 0;
    return status;
}


/* Tokenizes a script string and runs it through sendtoprop in a frame named "Prop Message": the
 * prop flavour of the message runners (TickMethodEntry case 2). Like DispatchServerMessageString
 * (0x0042a860) it compiles the text through WaitHeapThenResolveMethodPath and builds a
 * MethodCallFrame around the result, but the compiled block goes to ActionCmd_SendToProp, not to
 * ActionCmd_SendToServer.
 *
 * Port note: the port routed both this and RunShopMessage through the server dispatcher, so
 * `"inven.shp", openshop()` and every `"<prop>", openprop()` went to the wrong recipient and no
 * prop ever ran its registration script. */
short __cdecl RunPropMessage(byte *pCmd)  /* 0x00417f80 */
{
    int            *ph;
    int             skipOut;
    int             posErr;
    MethodCallFrame frame;
    short           st;
    int            *pData;

    st = WaitHeapThenResolveMethodPath(pCmd + 1, (uint)*pCmd, &ph, &posErr);
    if (st != 0)
        return st;
    frame.pData      = pData = (int *)GetResDataPtr(ph);
    frame.isLast     = 1;
    frame.saved0x14c = 0xffffffffu;
    CopyPascalString((byte *)UpdatePascalLength(kStrPropMessage), frame.buf0);
    CopyPascalString((byte *)UpdatePascalLength(kStrPropMessage), frame.buf2);
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), frame.buf1);
    st = (short)ActionCmd_SendToProp((int)&frame, (int)pData, (int *)0, &skipOut);
    UnlockResObject(ph);
    FreeResObject(ph);
    return st;
}


/* Script command sendtoprop (opcode 12055): a thin wrapper over ScriptCmd_SendToPropFx with a NULL
 * result pointer. */
short __cdecl ActionCmd_SendToProp(int pCtx, int pArgBlock,  /* 0x00418060 */
                                        int *pTable, int *pSubSkip)
{
    return ScriptCmd_SendToPropFx((int *)pCtx, pArgBlock,
                                pTable, NULL, pSubSkip);
}


/* Body of sendtoprop (opcodes 12055 / 20088). One script argument and a 0xfb4 separator; the
 * argument is resolved to a prop (LookupPropEntry) and its shop (LookupShopEntryByNode). Three
 * method frames are run through EvalMethodCall: "Prop Script: " (context from row +2, selector
 * from +0x0e), "Shop Script: " (the same context, selector from the shop entry[2]) and "System: " /
 * "Post Script" (the bootfile, selector 2). pResult goes to EvalMethodCall; the wrapper passes NULL. */
short __cdecl ScriptCmd_SendToPropFx(int *pCtx, int pArgBlock,  /* 0x00418080 */
                                   int *pTable, void *pResult,
                                   int *pSubSkip)
{
    int            start;           /* the parameter walks; this keeps its start */
    short          status;
    int            skipCount;       /* both evaluators' skip count (one slot) */
    short          args[4];
    /* Three chained method-call frames (Prop Script -> Shop Script -> System / Post Script) as
     * MethodCallFrame[3], not ~24 separate locals: EvalMethodCall walks frame[0] -> frame[1] ->
     * frame[2] by 0x4e (class 3; the frames sit at ebp-0x1a8, ebp-0x15a, ebp-0x10c). Each frame
     * is filled through a pointer, as in ScriptCmd_SendToBootFx (byte-exact). */
    MethodCallFrame f[3];
    MethodCallFrame *pf;
    int            slot3Entry[7];   /* LookupShopEntryByNode output: [0]=ctxPtr,[1]=stateHandle,[2]=shopSel,[3..6]=name */
    int            stateData[40];   /* 160-byte output of LookupPropEntry
                                     * ctxPtr at byte offset 2; selector at byte offset 0x0e;
                                     * Pascal name at byte offset 0x4e (= local_52 in Ghidra) */
    int           *h;

    start = pArgBlock;
    status = ProcessScriptBlock((int)pCtx, (short *)pArgBlock, pTable,
                               (int *)args, &skipCount);
    if (status != 0)
        return status;

    pArgBlock += skipCount * 8;
    if (*(short *)pArgBlock != 0xfb4)   /* expect 0xfb4 separator */
        return 0x1c;
    pArgBlock += 8;                     /* advance past 8-byte separator */

    status = TakeScriptString(args, gScratchStr);
    if (status != 0)
        return status;

    status = LookupPropEntry(gScratchStr, stateData);
    if (status != 0)
        return status;

    status = LookupShopEntryByNode(*(int *)((byte *)stateData + 2), slot3Entry);
    if (status != 0)
        return status;

    pf = &f[0];
    pf->pOwner     = (int *)*(int *)((byte *)stateData + 2);
    pf->selector   = *(uint32_t *)((byte *)stateData + 0x0e);
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(kStrPropScriptColon), pf->buf1);   /* "Prop Script: " */
    CopyPascalString((byte *)stateData + 0x4e, pf->buf0);
    CopyPascalString((byte *)stateData + 0x4e, pf->buf2);
    pf->isLast = 0;

    pf = &f[1];
    pf->pOwner     = (int *)*(int *)((byte *)stateData + 2);
    pf->selector   = (uint32_t)slot3Entry[2];
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(kStrShopScriptColon), pf->buf1);   /* "Shop Script: " */
    CopyPascalString((byte *)&slot3Entry[3], pf->buf0);  /* name in slot-3 entry */
    CopyPascalString((byte *)stateData + 0x4e, pf->buf2);
    pf->isLast = 0;

    pf = &f[2];
    pf->pOwner     = (int *)gBootNode;
    pf->selector   = 2u;
    pf->hMethod    = h = AcquireMethodHandle((int *)pf->pOwner, 2u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h);
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), pf->buf1);   /* "System: " */
    CopyPascalString((byte *)UpdatePascalLength(kStrPostScript), pf->buf0);   /* "Post Script" */
    CopyPascalString((byte *)stateData + 0x4e, pf->buf2);
    pf->isLast = 1;

    status = EvalMethodCall((short *)&f[0], (short *)pCtx, (short *)pArgBlock,
                           pTable, pResult, &skipCount);

    UnlockResObject(f[0].hMethod);
    ReleaseMethodHandle((int)f[0].hMethod);
    UnlockResObject(f[1].hMethod);
    ReleaseMethodHandle((int)f[1].hMethod);
    UnlockResObject(f[2].hMethod);
    ReleaseMethodHandle((int)f[2].hMethod);

    pArgBlock += skipCount * 8;
    *pSubSkip = (pArgBlock - start) >> 3;
    return status;
}


/* As RunPropMessage, with sendtoshop and a "Shop Message" frame (ActionCmd_SendToShop). */
short __cdecl RunShopMessage(byte *pCmd) /* 0x004183b0 */
{
    int            *ph;
    int             skipOut;
    int             posErr;
    MethodCallFrame frame;
    short            status;
    short           sendStatus;
    int             rowBase;

    status = WaitHeapThenResolveMethodPath(pCmd + 1, (uint)*pCmd, &ph, &posErr);
    if (status != 0)
        return status;

    rowBase          = GetResDataPtr(ph);
    frame.isLast     = 1;
    frame.saved0x14c = 0xffffffffu;
    frame.pData      = (void *)rowBase;

    CopyPascalString((byte *)UpdatePascalLength(kStrShopMessage), frame.buf0);
    CopyPascalString((byte *)UpdatePascalLength(kStrShopMessage), frame.buf2);
    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), frame.buf1);

    sendStatus = (short)ActionCmd_SendToShop((int)&frame, rowBase, (int *)0, &skipOut);
    UnlockResObject(ph);
    FreeResObject(ph);
    return sendStatus;
}


/* Script command sendtoshop (opcode 12059): a thin wrapper over ScriptCmd_SendToShopFx with a NULL
 * result pointer. */
short __cdecl ActionCmd_SendToShop(int pCtx, int pArgBlock,  /* 0x00418490 */
                                        int *pTable, int *pSubSkip)
{
    return ScriptCmd_SendToShopFx((int *)pCtx, pArgBlock,
                                pTable, NULL, pSubSkip);
}


/* Body of sendtoshop (opcodes 12059 / 20089). One script argument and a 0xfb4 separator; the
 * argument is resolved with LookupShopEntryByName. Two method frames are run through
 * EvalMethodCall: "Shop Script: " (context shopEntry[0], selector shopEntry[2]) and "System: " /
 * "Post Script" (the bootfile, selector 2). ScriptCmd_SendToPropFx without the middle "Prop
 * Script: " frame. pResult goes to EvalMethodCall; the wrapper passes NULL. */
short __cdecl ScriptCmd_SendToShopFx(int *pCtx, int pArgBlock,  /* 0x004184b0 */
                                   int *pTable, void *pResult,
                                   int *pSubSkip)
{
    int            start;           /* the parameter walks; this keeps its start */
    short          status;
    int            skipCount;       /* both evaluators' skip count (one slot) */
    short          args[4];
    /* Same layout as ScriptCmd_SendToCastFx (0x004184b0): the shop entry is ONE int[7] (it was
     * split into slot3Entry[2] + shopSel + slot3Name[16], as in CompleteCastOpen), and the two
     * method-call frames are an array, not ~14 locals; they sit at ebp-0x9c / ebp-0x4e
     * (class 3). */
    int            slot3Entry[7];   /* [0]=ctxHandle, [2]=selector, [3..6]=name */
    MethodCallFrame f[2];
    int           *h1, *h2;
    MethodCallFrame *pf;   /* frames filled through a pointer, as ScriptCmd_SendToBootFx */

    start = pArgBlock;
    status = ProcessScriptBlock((int)pCtx, (short *)pArgBlock, pTable,
                               (int *)args, &skipCount);
    if (status != 0)
        return status;

    pArgBlock += skipCount * 8;
    if (*(short *)pArgBlock != 0xfb4)   /* expect 0xfb4 separator */
        return 0x1c;
    pArgBlock += 8;                     /* advance past 8-byte separator */

    status = TakeScriptString(args, gScratchStr);
    if (status != 0)
        return status;

    status = LookupShopEntryByName(gScratchStr, slot3Entry);
    if (status != 0)
        return status;

    pf = &f[0];
    pf->pOwner     = (int *)slot3Entry[0];
    pf->selector   = (uint32_t)slot3Entry[2];
    pf->hMethod    = h1 = AcquireMethodHandle((int *)pf->pOwner, pf->selector);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h1);

    CopyPascalString((byte *)UpdatePascalLength(kStrShopScriptColon), pf->buf1);   /* "Shop Script: " */
    CopyPascalString((byte *)&slot3Entry[3], pf->buf0);   /* slot3Entry[3..6] = name at +12 */
    CopyPascalString((byte *)&slot3Entry[3], pf->buf2);
    pf->isLast = 0;

    pf = &f[1];
    pf->pOwner     = (int *)gBootNode;
    pf->selector   = 2u;
    pf->hMethod    = h2 = AcquireMethodHandle((int *)pf->pOwner, 2u);
    pf->saved0x14c = ((int *)pf->pOwner)[0x14c];
    pf->pData      = (void *)GetResDataPtr(h2);

    CopyPascalString((byte *)UpdatePascalLength(kStrSystemColon), pf->buf1);   /* "System: " */
    CopyPascalString((byte *)UpdatePascalLength(kStrPostScript), pf->buf0);   /* "Post Script" */
    CopyPascalString((byte *)&slot3Entry[3], pf->buf2);
    pf->isLast = 1;

    status = EvalMethodCall((short *)&f[0], (short *)pCtx, (short *)pArgBlock,
                           pTable, pResult, &skipCount);

    UnlockResObject(f[0].hMethod);
    ReleaseMethodHandle((int)f[0].hMethod);
    UnlockResObject(f[1].hMethod);
    ReleaseMethodHandle((int)f[1].hMethod);

    pArgBlock += skipCount * 8;
    *pSubSkip = (pArgBlock - start) >> 3;
    return status;
}


/* Script command countprops (opcode 20031): returns gPropCount as a type-4 int. */
short __cdecl ScriptCmd_CountProps(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x004186F0 */
{
    int count;

    *pSkipCount = 0;
    count = gPropCount;
    *pResult = 4;
    *(int *)(pResult + 1) = count;
    pResult[3] = 0;
    return 0;
}


/* Script command countshops (opcode 20033): returns gShopCount as a type-4 int. */
short __cdecl ScriptCmd_CountShops(int *pCtx, short *pArgBlock, int *pTable, short *pResult, int *pSkipCount) /* 0x00418720 */
{
    *pSkipCount = 0;
    *(int *)(pResult + 1) = gShopCount;
    *pResult = 4;
    pResult[3] = 0;
    return 0;
}


/* Script command indextoprop (opcode 20032): one int argument, a 1-based index into gPropTable.
 * Copies that prop row and returns the prop's own name through AllocScriptString; the shop name
 * goes to the shared result() slot. */
short __cdecl ScriptCmd_IndexToProp(int *pCtx, short *pArgBlock, int *pTable,
                                                      short *pResult, int *pSkipCount) /* 0x00418750 */
{
    /* the 0x9E-byte slot-2 row, copied as one object (rep movsd 0x27 + movsw) */
    typedef struct { byte b[0x9e]; } Row158;
    short    st;
    short   argBuf[4];
    int     slot3Entry[7];
    Row158  stateRow;

    st = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)argBuf, pSkipCount);
    if (st != 0)
        return st;

    if (argBuf[0] != 4)
        return 0x0e;

    if (*(int *)(argBuf + 1) < 1 || gPropCount < *(int *)(argBuf + 1))
        return 10;

    /* the product is a pointer before the -0x9e (disp32 in the exe's lea) */
    stateRow = *(Row158 *)((char *)(*(int *)(argBuf + 1) * 0x9e) - 0x9e + GetResDataPtr(gPropTable));
    UnlockResObject(gPropTable);

    st = LookupShopEntryByNode(*(int *)(stateRow.b + 2), slot3Entry);
    if (st != 0)
        return st;

    /* The two halves go to two different places and are NOT concatenated: the shop (file) name
     * lands in the shared result() slot (0x00418819, CopyPascalString(&slot3Entry[3], 0x48a890))
     * and the value the command answers is the prop's own name at record +0x4e (AllocScriptString
     * at 0x00418831). Scripts enumerate one file's props out of the game-wide table by comparing
     * result() against a file name, so folding the file into the return value made that
     * comparison never match. */
    CopyPascalString((byte *)&slot3Entry[3], gScriptResult);

    return AllocScriptString(stateRow.b + 0x4e, pResult);
}


/* Script command indextoshop (opcode 20034): one int argument, a 1-based index into gShopTable;
 * returns the shop's name. */
short __cdecl ScriptCmd_IndexToShop(int *pCtx, short *pArgBlock, int *pTable,
                                                      short *pResult, int *pSkipCount) /* 0x00418850 */
{
    /* the 28-byte table row, copied as one object (rep movsd, ecx = 7) */
    typedef struct { int w[7]; } Row28;
    short  st;
    short  argBuf[4];
    Row28  row;

    st = ProcessScriptBlock((int)pCtx, pArgBlock, pTable, (int *)argBuf, pSkipCount);
    if (st != 0)
        return st;
    if (argBuf[0] != 4)
        return 0x0e;
    if (*(int *)(argBuf + 1) < 1 || gShopCount < *(int *)(argBuf + 1))
        return 10;
    /* the product is a pointer before the -0x1c: the exe keeps the displacement
     * in the lea instead of folding it into the multiply */
    row = *(Row28 *)(GetResDataPtr(gShopTable) + (char *)(*(int *)(argBuf + 1) * 0x1c) - 0x1c);
    UnlockResObject(gShopTable);
    return AllocScriptString((byte *)&row.w[3], pResult);
}


/* Finds the prop row named pName in gPropTable (stride 0x9e, name at +0x4e), ignoring case, trying
 * the last matching index (gStateEntryCacheIndex) first, and copies the row (0x27 ints and a short)
 * to pRowOut. Returns 0, or 10 if there is no such prop. Every Prop* handler starts with it; the
 * twin of LookupActorEntry. */
short __cdecl LookupPropEntry(byte *pName, unsigned int *pRowOut)  /* 0x00418900 */
{
    /* the 0x9E-byte row, copied whole (rep movsd 0x27 + movsw) */
    typedef struct { char b[0x9e]; } Rec158;
    Rec158 *rows;
    Rec158 *p;
    int     i;

    rows = (Rec158 *)GetResDataPtr(gPropTable);

    if (gStateEntryCacheIndex < gPropCount) {
        p = &rows[gStateEntryCacheIndex];
        if (PascalStringsEqualNoCase(pName, (byte *)p + 0x4e) != 0) {
            *(Rec158 *)pRowOut = *p;
            UnlockResObject(gPropTable);
            return 0;
        }
    }

    for (i = 0; i < gPropCount; rows++, i++) {
        if (PascalStringsEqualNoCase(pName, (byte *)rows + 0x4e) != 0) {
            gStateEntryCacheIndex = i;
            *(Rec158 *)pRowOut = *rows;
            UnlockResObject(gPropTable);
            return 0;
        }
    }

    UnlockResObject(gPropTable);
    return 10;
}


/* Writes the 0x9e-byte prop row back to gPropTable at the index cached by LookupPropEntry, after
 * verifying the key at row +0x2c. Fatal 0xFDD if the index is out of range, 0xFDE on a key
 * mismatch. The twin of WriteBackActorEntry. */
void __cdecl
WriteBackPropEntry(int *pRow)  /* 0x004189e0 */
{
    /* the 0x9e-byte state row, copied as one object (rep movsd 0x27 + movsw) */
    typedef struct { short w[0x4f]; } StateRow9E;
    byte *pDst;

    pDst = (byte *)GetResDataPtr((void *)gPropTable);
    if (gStateEntryCacheIndex >= gPropCount)
        FatalError(0, 0x0FDD);
    pDst += gStateEntryCacheIndex * 0x9E;
    if (*(int *)(pDst + 0x2c) != pRow[0xB])
        FatalError(0, 0x0FDE);
    *(StateRow9E *)pDst = *(StateRow9E *)pRow;
    UnlockResObject((int *)gPropTable);
}


/* Finds the shop row in gShopTable (stride 7 ints, count gShopCount) whose name (entry[3])
 * equals pDesc, ignoring case (PascalStringsEqualNoCase), and copies its 7 ints to pEntryOut.
 * Returns 0, or 10 if there is none. */
short __cdecl LookupShopEntryByName(byte *pDesc, int *pEntryOut)  /* 0x00418a60 */
{
    /* the 7-int entry as one object: `rep movsd`, ecx = 7, is a struct assignment.
     * Local on purpose: a file-scope typedef enters every later test file's symbol
     * table and moves the 1996 compiler's register allocation there. */
    typedef struct { int w[7]; } Slot3EntryRec;
    int *pEntry;
    int  i;

    pEntry = (int *)GetResDataPtr(gShopTable);
    for (i = 0; i < gShopCount; pEntry += 7, i++) {   /* add esi,0x1c before inc ebx */
        if ((short)PascalStringsEqualNoCase(pDesc, (byte *)(pEntry + 3)) != 0) {
            *(Slot3EntryRec *)pEntryOut = *(Slot3EntryRec *)pEntry;
            UnlockResObject((int *)gShopTable);
            return 0;
        }
    }
    UnlockResObject((int *)gShopTable);
    return 10;
}


/* Walks gShopTable (the layout of LookupShopEntryByNode) and writes the index of the row whose node
 * is ctxHandle. Returns 0, or 10 if there is none. */
short __cdecl FindShopEntryIndexByNode(int ctxHandle, int *pIdxOut)  /* 0x00418ae0 */
{
    int *pEnt;
    int  i;

    pEnt = (int *)GetResDataPtr((int *)gShopTable);
    for (i = 0; i < gShopCount; pEnt += 7, i++) {
        if (*pEnt == ctxHandle) {
            *pIdxOut = i;
            UnlockResObject((int *)gShopTable);
            return 0;
        }
    }
    UnlockResObject((int *)gShopTable);
    return 10;
}


/* Finds the shop row whose first field (the shop's render node) equals ctxHandle and copies its 7
 * ints to pEntryOut. Returns 0, or 10 if there is none. */
short __cdecl LookupShopEntryByNode(int ctxHandle, int *pEntryOut)  /* 0x00418b40 */
{
    typedef struct { int w[7]; } Slot3EntryRec;
    int *pEnt;
    int  i;

    pEnt = (int *)GetResDataPtr((int *)gShopTable);
    for (i = 0; i < gShopCount; pEnt += 7, i++) {
        if (*pEnt == ctxHandle) {
            *(Slot3EntryRec *)pEntryOut = *(Slot3EntryRec *)pEnt;
            UnlockResObject((int *)gShopTable);
            return 0;
        }
    }
    UnlockResObject((int *)gShopTable);
    return 10;
}


/* The prop twin of PlaceActorsAtWaypoints: for every prop row whose set is the current set and
 * whose waypoint name is in the set's waypoint table, copies the waypoint's xyz into the row.
 * Called right after it from the set open path. */
void __stdcall PlacePropsAtWaypoints(void)  /* 0x00418BB0 */
{
  /* One 8-byte record, exactly as in PlaceActorsAtWaypoints above. */
  typedef struct { int w[2]; } Pos8;
  int    i;
  byte  *row;
  short  keep;
  short  pos[4];

  row = (byte *)GetResDataPtr(gPropTable);
  for (i = 0; i < gPropCount; row += 0x9e, i++) {
    if (*(short *)(row + 0x12) != 0
        && PascalStringsEqualNoCase(row + 0x5e, &DAT_0048a7b4) != 0
        && FindWaypointNodePosition(row + 0x6e, pos) != 0) {
      keep = *(short *)(row + 0x18);
      *(Pos8 *)(row + 0x18) = *(Pos8 *)pos;
      *(short *)(row + 0x18) = keep;
    }
  }
  UnlockResObject(gPropTable);
}


/* The prop twin of WalkActorsCalcRender over gPropTable (gPropCount rows of 0x9e bytes): calls
 * CalcPropRenderParams on each row and, in the live mode, AppendCalcScratchRow (gCalcRows),
 * AddDirtyRect and commits the rects to the row (shorts +0x18..+0x22). */
void __cdecl WalkPropsCalcRender(short mode) /* 0x00418c40 */
{
    typedef struct { short s[79]; } Slot2Row158;   /* copied whole: `rep movsd` 0x27 + movsw */
    typedef struct { short s[4]; } Rect8;
    int           idx;
    short        *psRow;
    short         u;
    Slot2Row158   rowCopy;
    /* calcOut is Ghidra's local_c4 (ebp-0xc4); every other "local" in the ground truth is a field
     * of it: calcDirty = [4] (set by the calc when anything changed), calcAux = [5], calcRectTL =
     * [8..9], calcRectBR = [10..11], calcGeomWord = [14..15], calcSizePair = [16..17]. As in
     * WalkActorsCalcRender, the port had calcDirty as its own variable and the dirty gate never
     * fired after the first frame (class 3). */
    short         calcOut[18];
#define calcDirty (calcOut[4])
#define calcAux (calcOut[5])
#define calcRectTL (*(int *)(calcOut + 8))
#define calcRectBR (*(int *)(calcOut + 10))
#define calcGeomWord (*(int *)(calcOut + 14))
#define calcSizePair (*(int *)(calcOut + 16))

    psRow = (short *)(intptr_t)GetResDataPtr(gPropTable);
    DF_TRACE("walkSlot2: mode=%d rows=%d table=%p", (int)mode,
             (int)gPropCount, (void *)psRow);
#if DF_TRACE_ENABLED
    {
        static int dumped = 0;
        if (!dumped && gPropCount >= 72) {
            int k; dumped = 1;
            for (k = 0; k < gPropCount; k++) {
                const byte *r = (const byte *)psRow + k * 0x9e;
                DF_TRACE("  row[%d]: name='%.*s' set='%.*s' node='%.*s' view='%.*s' vis=%d node=(%d,%d)",
                         k, r[0x4e], (const char *)r + 0x4f, r[0x5e], (const char *)r + 0x5f,
                         r[0x6e], (const char *)r + 0x6f, r[0x7e], (const char *)r + 0x7f,
                         (int)*(const short *)r, (int)*(const short *)(r + 0x14), (int)*(const short *)(r + 0x16));
            }
        }
    }
#endif

    /* no NULL test on the table in the exe (the port had added one) */
    {
        for (idx = 0; idx < gPropCount; idx++) {
            if (mode != 0) {
                psRow[0x10]++;
                if (psRow[0x11] <= psRow[0x10])
                    psRow[0x10] = 0;

                u = CalcPropRenderParams(psRow, calcOut);
#if DF_TRACE_ENABLED
                /* Why a row does or does not draw, logged ONLY when its calc status or dirty flag
                 * changes (a per-frame line for 78 rows would bury the log): calc=0 means
                 * CalcPropRenderParams rejected the row (inactive, or its view frame did not
                 * resolve), so the walk clears its saved rect and nothing is drawn; dirty=0 means
                 * it resolved but matched its saved state. A row that is clickable but never
                 * painted shows up as a stuck calc=0 or dirty=0. */
                {
                    static signed char sPrev[128];
                    int state = (u != 0 ? 2 : 0) | (calcOut[4] != 0 ? 1 : 0);
                    if (idx < 128 && sPrev[idx] != (signed char)state) {
                        const byte *r = (const byte *)psRow;
                        sPrev[idx] = (signed char)state;
                        DF_TRACE("slot2row[%d] '%.*s' calc=%d dirty=%d "
                                 "active=%d view='%.*s' vis=%d frame=%d",
                                 idx, r[0x4e], (const char *)r + 0x4f,
                                 (int)u, (int)calcOut[4], (int)psRow[0],
                                 r[0x7e], (const char *)r + 0x7f,
                                 (int)*(const short *)r, (int)psRow[0xc]);
                    }
                }
#endif
                /* Only rows that actually produced a calc row are worth logging: all 72 fire every
                 * frame, and the inactive ones alone exhausted the 400k line cap before a session
                 * got past the apartment. Trace on the event, not per frame. */
                if (u != 0)
                    DF_TRACE_RENDER("  slot2row[%d]: calc=%d", idx, (int)u);
                if (u != 0) {
                    AppendCalcScratchRow((unsigned int *)calcOut);
                    if (calcDirty != 0) {
                        if (*(int *)(psRow + 0x18) != 0)
                            AddDirtyRect(psRow + 0x1a);
                        AddDirtyRect(calcOut + 8);
                        DF_TRACE("  dirtyRect: row %d rect=(%d,%d,%d,%d)", idx,
                                 (int)calcOut[8], (int)calcOut[9], (int)calcOut[10], (int)calcOut[11]);
                        *(Rect8 *)(psRow + 0x1a) = *(Rect8 *)(calcOut + 8);   /* calcRectTL/calcRectBR */
                        *(int *)(psRow + 0x18) = calcGeomWord;
                        *(int *)(psRow + 0x1e) = *(int *)(calcOut + 0);
                        *(int *)(psRow + 0x20) = calcSizePair;
                        psRow[0x22]            = calcAux;
                    }
                } else {
                    if (*(int *)(psRow + 0x18) != 0)
                        AddDirtyRect(psRow + 0x1a);
                    /* Ground truth clears shorts [0x18] and [0x19] with ONE dword store (`mov
                     * dword ptr [ebp+0x30],0`); two 4-byte stores ran on into short [0x1a], the
                     * first word of the saved rect this row hands to AddDirtyRect next pass. */
                    *(int *)(psRow + 0x18) = 0;
                }
            } else {
                rowCopy = *(Slot2Row158 *)psRow;
                u = CalcPropRenderParams(rowCopy.s, calcOut);
                if (u != 0) {
                    calcDirty = 1;
                    AppendCalcScratchRow((unsigned int *)calcOut);
                }
            }
            psRow += 0x4f;
        }
    }

    UnlockResObject(gPropTable);
#undef calcDirty
#undef calcAux
#undef calcRectTL
#undef calcRectBR
#undef calcGeomWord
#undef calcSizePair
}

/* The prop twin of CalcActorRenderParams: the same structure over a 0x9e-byte row, using
 * LookupStateSpriteFrameGeometryByAngle, with every pRow field index one lower (0x14 -> 0x13, 0x26
 * -> 0x25, 0x2f against 0x30 for the string, ...). Derived from the actor version by offset
 * substitution and shaped to the exe the same way ; see CalcActorRenderParams. */
short CalcPropRenderParams(short *pRow, short *pCalc)  /* 0x00418d90 */
{
    short         relDist;  /* local_bc */
    short         sVar4;
    short         dist;
    short         sVar11;
    short         extentA;
    short         extentB;
    short         angle;
    short         camAngle;
    short        *psVar1;
    short         rowAngle;
    short         bearing;
    int           serial;
    int           w;
    int w_a;
    short         pt[2];
    int           h;
    short         clipRect[4];  /* local_ac */
    short         frameGeom[22];          /* 44 bytes @ ebp-0xa4, one geometry record */
    short         vp[60];                 /* 120 bytes @ ebp-0x78: viewport pair, then the size pair */
#define frameGeomHead  ((int *)frameGeom)      /* +0x00 */
#define frameMinA  (frameGeom[9])          /* +0x12 */
#define frameMinB  (frameGeom[10])         /* +0x14 */
#define frameMaxA  (frameGeom[11])         /* +0x16 */
#define frameMaxB  (frameGeom[12])         /* +0x18 */
#define frameOriginA  (frameGeom[13])         /* +0x1a */
#define frameOriginB  (frameGeom[14])         /* +0x1c */
#define frameScale  (frameGeom[21])         /* +0x2a */

    if (*pRow <= 0) {
        DF_TRACE_RENDER("    calcState: bail row[0]=%d (<1, inactive)", (int)*pRow);
        return 0;
    }

    if (pRow[9] != 0) {
        /* AVI / video path */
        DF_TRACE("    calcState: AVI path (row[9]=%d)", (int)pRow[9]);
        if ((gSet.active == 0) || (DAT_0048a77a == 0))
            return 0;
        if (PascalStringsEqualNoCase((byte *)(pRow + 0x2f), &DAT_0048a7b4) == 0)
            return 0;

        camAngle = gCameraAngle;
        psVar1 = pRow + 0xc;
        rowAngle = *psVar1 - gCurrentDeg;
        /* 0x48a7ac is a field of the camera record (gSet[0x34]); passing the literal
         * dereferenced a 1996 address that is unmapped in this process. */
        bearing = camAngle - ComputeAngleBetweenPoints((int)psVar1, (int)(intptr_t)&gCameraAngle);
        angle  = (rowAngle + bearing) & 0xff;
        dist  = ProjectSetPointToScreen((int)psVar1, pt);
        relDist = dist - pRow[0x25];
        if (relDist < 0)
            relDist = 0;

        /* Near/far cull. An actor close to the camera has the SMALLEST dist, so a
         * wrong near limit drops exactly the foreground figures while distant ones
         * keep drawing -- the grand-staircase steward missing while someone at the
         * end of the corridor renders. */
        if (dist <= (short)(DAT_0048a78c / 4) || relDist > DAT_0048a790) {
            DF_TRACE("stateCull: CULL '%.*s' dist=%d near=%d d=%d far=%d (a78c=%d selfNear=%d)",
                     (int)*((byte *)pRow + 0x4e), (const char *)pRow + 0x4f,
                     (int)dist, (int)(short)(DAT_0048a78c / 4), (int)relDist,
                     (int)DAT_0048a790, (int)DAT_0048a78c, (int)pRow[0x25]);
            return 0;
        }
        DF_TRACE("stateCull: draw '%.*s' dist=%d near=%d far=%d d=%d",
                 (int)*((byte *)pRow + 0x4e), (const char *)pRow + 0x4f,
                 (int)dist, (int)(short)(DAT_0048a78c / 4), (int)DAT_0048a790, (int)relDist);

        DF_TRACE("    calcState: AVI lookupGeom angle=%d (rowAngle=%d camAngle=%d bearing=%d)",
                 (int)angle, (int)rowAngle, (int)camAngle, (int)bearing);
        if (!LookupStateSpriteFrameGeometryByAngle((int)pRow, angle, frameGeomHead))
            return 0;

        /* The divisors below are the record's own extents. The exe divides by them
         * with no guard, so a degenerate frame is an integer divide-by-zero, not a
         * wrong picture. */
        DF_TRACE("    calcState: geom dx=%d dy=%d dist=%d scale=%d%s",
                 (int)(short)(frameMaxB - frameMinB), (int)(short)(frameMaxA - frameMinA),
                 (int)dist, (int)frameScale,
                 ((short)(frameMaxA - frameMinA) == 0 || (short)(frameMaxB - frameMinB) == 0
                  || dist == 0) ? "  <<< ZERO DIVISOR" : "");

        sVar11 = (short)((*(int *)(pRow + 0x14) * (int)frameScale) / 1000);
        extentA = frameMaxA - frameMinA;
        sVar4 = frameMaxB - frameMinB;
        extentB = (short)((sVar11 * sVar4) / (int)dist);
        h      = extentA;
        extentA = (short)((sVar11 * extentA) / (int)dist);
        sVar4 = (short)(((int)extentB * (int)frameOriginB) / sVar4);
        psVar1 = pCalc + 8;
        sVar11 = pt[0] - (short)(((int)extentA * (int)frameOriginA) / h);
        sVar4  = pt[1] - sVar4;
        psVar1[0] = sVar11;
        psVar1[1] = sVar4;
        psVar1[2] = sVar11 + extentA;
        psVar1[3] = sVar4 + extentB;
        CopyTrigOverlayBufferAndViewportPair(vp, clipRect);
        if (IntersectSceneRects(clipRect, pCalc + 8, (int *)(pCalc + 8)) == 0)
            return 0;
        pCalc[6]      = dist;
        pCalc[7]      = relDist / DAT_0048a78c;
        *(int *)pCalc = *(int *)pt;
        pRow[0x13]   = dist;
    } else {
        /* Sprite/animation path */
        if (!LookupStateSpriteFrameGeometryByAngle((int)pRow, pRow[0xc], frameGeomHead)) {
            DF_TRACE("    calcState: bail lookupGeom failed (angle=%d)", (int)pRow[0xc]);
            return 0;
        }
        extentA = frameMaxA - frameMinA;
        extentB = frameMaxB - frameMinB;
        sVar11 = pRow[10] - frameOriginA;
        pCalc[8]   = sVar11;
        sVar4  = pRow[0xb] - frameOriginB;
        pCalc[9]   = sVar4;
        pCalc[10]  = sVar11 + extentA;
        pCalc[0xb] = sVar4 + extentB;
        CopyTrigSceneBufferAndViewportPair(vp, clipRect);
        DF_TRACE("    calcState: rect=(%d,%d,%d,%d) clip=(%d,%d,%d,%d)",
                 (int)pCalc[8], (int)pCalc[9], (int)pCalc[10], (int)pCalc[0xb],
                 (int)clipRect[0], (int)clipRect[1], (int)clipRect[2], (int)clipRect[3]);
        if (IntersectSceneRects(clipRect, pCalc + 8, (int *)(pCalc + 8)) == 0) {
            DF_TRACE("    calcState: bail rect/clip do not intersect");
            return 0;
        }
        pCalc[6]      = pRow[0x13];
        DF_TRACE_RENDER("    stateRow: node=(%d,%d) hot=(%d,%d) size=%dx%d row9=%d rowC=%d "
                 "row13=%d row1e=%d row1f=%d name='%.8s'",
                 (int)pRow[10], (int)pRow[11],
                 (int)frameOriginA, (int)frameOriginB,
                 (int)(frameMaxA - frameMinA), (int)(frameMaxB - frameMinB),
                 (int)pRow[9], (int)pRow[0xc], (int)pRow[0x13],
                 (int)pRow[0x1e], (int)pRow[0x1f],
                 (const char *)((byte *)pRow + 0x4f));   /* row name is the Pascal string at +0x4e */
        *(int *)pCalc = *(int *)(pRow + 10);
    }

    /* The size pair goes out through the first dword of the viewport blob. */
    psVar1 = vp;
    vp[1] = extentB;
    vp[0] = extentA;
    *(int *)(pCalc + 0xc) = *(int *)(pRow + 1);       /* int at byte offset +2 */
    *(int *)(pCalc + 0xe) = frameGeomHead[0];
    serial = *(int *)(pRow + 0x16);
    pCalc[4] = 0;
    pCalc[5] = 0;
    *(int *)(pCalc + 2) = serial;
    *(int *)(pCalc + 0x10) = *(int *)psVar1;

    /* Dirty check: set pCalc[4]=1 if any cached value changed */
    if (*(int *)(pRow + 0x18) != frameGeomHead[0] ||     /* frame data at byte +0x32 */
        pCalc[1]    != pRow[0x1f] ||
        pRow[0x1e] != *pCalc       ||
        pCalc[0x11] != pRow[0x21] ||
        pRow[0x20] != pCalc[0x10]       ||
        pRow[0x22] != 0) {
        pCalc[4] = 1;
    }
    return 1;
}



#undef local_7a
#undef local_88
#undef local_8a
#undef local_8c
#undef local_8e
#undef local_90
#undef local_92
#undef local_a4

/* The prop variant of LookupSpriteFrameGeometryByAngle: finds, in the binding named by the prop's
 * view, the frame of the current pose whose angle is nearest, and copies its 44-byte frame record
 * to pFrameOut. Returns a short (`(short)(best - 1000) != 0` in ax). */
short __cdecl LookupStateSpriteFrameGeometryByAngle(int pRow, short angle, int *pFrameOut) /* 0x004190f0 */
{
    /* The shape of its twin LookupSpriteFrameGeometryByAngle (0x00411570):
     * frame target +0, best +2, childBase +4, rowWalk +8, childHandle +0xc,
     * the 44-byte frame record copied out whole (rep movsd, ecx = 0xb). The exe
     * returns `(short)(best - 1000) != 0` in ax (`mov ax,cx`): `short`, which
     * its df_protos.h prototype does not say yet. */
    typedef struct { int v[11]; } Frame44;
    short         target;
    short         bestDelta;
    int           childBase;
    int           rowWalk;
    int           childHandle;
    int           parentBase;
    int           bindIdx;
    unsigned int *pBinding;
    Frame44      *pf;
    short         d;
    short         err;

    bestDelta  = 1000;
    parentBase = GetResDataPtr(*(void **)(pRow + 6));
    bindIdx    = 0;
    pBinding     = (unsigned int *)(parentBase + 0x5e);
#if DF_TRACE_ENABLED
    /* One shot per watched row: list every binding the view name is matched
     * against, so a wrong view selection is visible directly. */
    {
        const byte *nm = (const byte *)pRow + 0x4e;
        const byte *vw = (const byte *)pRow + 0x7e;
        static int dumped[3];
        int slot = (nm[0] == 4) ? 0 : ((nm[0] == 5) ? 1 : 2);
        if (DfTraceIsWatchedRow(nm) && !dumped[slot]) {
            unsigned int *pb = pBinding;
            int nb = *(int *)(parentBase + 0x5a);
            int b;
            dumped[slot] = 1;
            DF_TRACE("bindings for '%.*s': want view='%.*s' viewIdx=%d nBindings=%d",
                     (int)nm[0], (const char *)nm + 1,
                     (int)vw[0], (const char *)vw + 1,
                     (int)*(short *)(pRow + 0x20), nb);
            for (b = 0; b < nb && b < 24; b++, pb += 8) {
                const byte *bn = (const byte *)(pb + 4);
                DF_TRACE("  bind[%d]: '%.*s'%s", b, (int)bn[0],
                         (const char *)bn + 1,
                         (bn[0] == vw[0] && memcmp(bn + 1, vw + 1, bn[0]) == 0)
                             ? "   <== MATCH" : "");
            }
        }
    }
#endif
    if (*(int *)(parentBase + 0x5a) > 0) {
        do {
            if (PascalStringsEqualNoCase((byte *)(pBinding + 4), (byte *)(pRow + 0x7e)) != 0) {
                err = AttachRenderChild(*(int **)(pRow + 2), *pBinding, &childHandle);
                if (err != 0)
                    FatalError(err, 0xfdf);
                childBase = GetResDataPtr((void *)(intptr_t)childHandle);
                rowWalk = 0;
                target = *(short *)(childBase + 0x2e + *(short *)(pRow + 0x20) * 2) - 1;
                pf = (Frame44 *)(childBase + 0x76);
#if DF_TRACE_ENABLED
                    {
                        const byte *nm = (const byte *)pRow + 0x4e;
                        const byte *vw = (const byte *)pRow + 0x7e;
                        if (DfTraceIsWatchedRow(nm)) {
                            int k; unsigned int *fr = (unsigned int *)pf;
                            DF_TRACE("frames for '%.*s': view='%.*s' viewIdx=%d viewSel=%d nFrames=%d angle=%d",
                                     (int)nm[0], (const char *)nm + 1,
                                     (int)vw[0], (const char *)vw + 1,
                                     (int)*(short *)(pRow + 0x20),
                                     (int)target + 1, *(int *)(childBase + 0x72), (int)angle);
                            for (k = 0; k < *(int *)(childBase + 0x72) && k < 12; k++, fr += 11) {
                                const short *f = (const short *)fr;
                                DF_TRACE("  frame[%d]: view=%d ang=%d  s[0..21]=%d %d %d %d | %d %d %d %d | %d %d %d %d %d %d %d | %d %d %d %d %d %d %d",
                                         k, (int)f[4], (int)f[20],
                                         f[0],f[1],f[2],f[3], f[4],f[5],f[6],f[7], f[8],f[9],f[10],f[11],f[12],f[13],f[14],
                                         f[15],f[16],f[17],f[18],f[19],f[20],f[21]);
                            }
                        }
                    }
#endif
                if (*(int *)(childBase + 0x72) > 0) {
                    do {
                        if (*(short *)((char *)pf + 8) == target) {
                            d = ComputeAngularDistance(*(short *)((char *)pf + 0x28), angle);
                            if (d < bestDelta) {
                                bestDelta = d;
                                *(Frame44 *)pFrameOut = *pf;
                                if (d <= 0)
                                    break;
                            }
                        }
                        pf++;
                        rowWalk++;
                    } while (rowWalk < *(int *)(childBase + 0x72));
                }
                UnlockResObject((void *)(intptr_t)childHandle);
                err = DetachRenderChild(*(int *)(pRow + 2), *pBinding);
                if (err != 0)
                    FatalError(err, 0xfe0);
                /* Ground truth (0x00411570 / 0x004190f0) leaves the binding loop as soon as a name
                 * matches; falling through let a LATER binding with the same name overwrite
                 * pFrameOut (see LookupSpriteFrameGeometryByAngle). */
                break;
            }
            pBinding += 8;
            bindIdx++;
        } while (bindIdx < *(int *)(parentBase + 0x5a));
    }

    UnlockResObject(*(void **)(pRow + 6));
    return (short)(bestDelta - 1000) != 0;
}


/* Temporarily attaches child childId, reads the ushort at byte +0x70 of its resource data and
 * detaches it. FatalError on failure (0xfe1 attach, 0xfe2 detach), as in the binary. The twin of
 * ReadNamedRowChildUshort. */
unsigned short __cdecl ReadChildUshortAt0x70(int *pCtx, unsigned int childId)  /* 0x00419260 */
{
    short   err;
    int    *hChild;
    int     data;
    unsigned short u;

    err    = AttachRenderChild(pCtx, childId, (int **)&hChild);
    if (err != 0)
        FatalError(err, 0xFE1);
    data = GetResDataPtr(hChild);
    u    = *(unsigned short *)(data + 0x70);
    UnlockResObject(hChild);
    err = DetachRenderChild((int)pCtx, childId);
    if (err != 0)
        FatalError(err, 0xFE2);
    return u;
}


/* Finds the shop binding row whose name is the pose name pSecondName and writes that pose's frame
 * count (ReadChildUshortAt0x70) into the prop row at +0x22. The twin of
 * SetActorFrameCountForPose. */
short __cdecl SetPropFrameCountForPose(int *pState, byte *pSecondName) /* 0x004192e0 */
{
    /* Ground truth (0x004192e0): the row's handle at BYTE +6 and context at
     * BYTE +2 (the port once had +24/+8, int indices). port-bug-classes.md #3 */
    int     i;
    int     base;
    byte   *p;

    base = GetResDataPtr(*(int **)((char *)pState + 6));
    p    = (byte *)(base + 0x5e);
    for (i = 0; i < *(int *)(base + 0x5a); p += 0x20, i++) {
        if (PascalStringsEqualNoCase(p + 0x10, pSecondName) != 0) {
            *(ushort *)((char *)pState + 34) = ReadChildUshortAt0x70(*(int **)((char *)pState + 2), *(uint *)p);
            UnlockResObject(*(int **)((char *)pState + 6));
            return 0;
        }
    }
    UnlockResObject(*(int **)((char *)pState + 6));
    return 1;
}


/* Scans gPropTable for the row whose binding key (+0x2c) equals bindingKey and copies its name
 * (+0x4e) to pDescOut. Returns 0 on a hit, 10 otherwise. The twin of GetActorNameByBindingKey. */
short __cdecl GetPropNameByBindingKey(int bindingKey, byte *pDescOut) /* 0x00419370 */
{
    /* The exe returns ax (`mov ax,0xa` / `xor ax,ax`): `short`, which its
     * df_protos.h prototype does not say yet. */
    int    i;
    byte  *pRow;

    pRow = (byte *)GetResDataPtr(gPropTable);
    for (i = 0; i < gPropCount; pRow += 0x9e, i++) {
        if (*(int *)(pRow + 0x2c) == bindingKey) {
            CopyPascalString(pRow + 0x4e, pDescOut);
            UnlockResObject((int *)gPropTable);
            return 0;
        }
    }
    UnlockResObject((int *)gPropTable);
    return 10u;
}


/* Re-attaches the render children of a prop row: the parent node handle (+2) is used for every
 * attach and detach, with the scope handle at +6 and the child selector at +0xe. The twin of
 * ReattachSlot0RowRenderChildren, with different fatal ids. */
void __cdecl
ReattachStateRowRenderChildren(int *pRow)  /* 0x004193e0 */
{
    /* Declaration order sets the exe's register allocation (byte-exact in the
     * whole unit with this order; the twin 0x00411860 differs in the last three). */
    void  *scopeBase;
    int    hFrameChild;  /* local_4 */
    int    bindIdx;  /* local_8 */
    uint  *pBinding;  /* local_10 */
    int   *hViewChild;  /* local_14 */
    int    frameIdx;
    uint  *pFrame;
    int    viewData;
    short   detachStatus;
    short  attachStatus;

    /* the row is a BYTE address, as in ReattachSlot0RowRenderChildren (parent +2, scope +6,
     * selector +0xe) */
    scopeBase = GetResDataPtr((void *)*(int **)((char *)pRow + 6));
    attachStatus = AttachRenderChild(*(int **)((char *)pRow + 2), *(unsigned int *)((char *)pRow + 0x0E),
                             (int **)&hViewChild);
    if (attachStatus != 0)
        FatalError(attachStatus, (short)0xFE3);

    detachStatus = DetachRenderChild(*(int *)((char *)pRow + 2),
                                   *(unsigned int *)((char *)pRow + 0x0E));
    if (detachStatus != 0)
        FatalError(detachStatus, (short)0xFE4);

    bindIdx = 0;
    pBinding = (uint *)((char *)scopeBase + 0x5E);
    while (bindIdx < *(int *)((char *)scopeBase + 0x5A)) {
        attachStatus = AttachRenderChild(*(int **)((char *)pRow + 2), *pBinding, (int **)&hViewChild);
        if (attachStatus != 0)
            FatalError(attachStatus, (short)0xFE5);

        viewData = GetResDataPtr((void *)hViewChild);
        for (frameIdx = 0, pFrame = (uint *)((char *)viewData + 0x76);
             frameIdx < *(int *)((char *)viewData + 0x72);
             pFrame += 0xB, frameIdx++) {
            attachStatus = AttachRenderChild(*(int **)((char *)pRow + 2), *pFrame, (int **)&hFrameChild);
            if (attachStatus != 0)
                FatalError(attachStatus, (short)0xFE6);

            detachStatus = DetachRenderChild(*(int *)((char *)pRow + 2), *pFrame);
            if (detachStatus != 0)
                FatalError(detachStatus, (short)0xFE7);
        }
        UnlockResObject((int *)hViewChild);

        detachStatus = DetachRenderChild(*(int *)((char *)pRow + 2), *pBinding);
        if (detachStatus != 0)
            FatalError(detachStatus, (short)0xFE8);

        bindIdx++;
        pBinding += 8;
    }
    UnlockResObject(*(int **)((char *)pRow + 6));
}


/* Attaches every binding child and grandchild of the prop's shop resource and sets the +0x4c warm
 * flag: what proplock / shoplock call. The twin of WarmActorRenderChildren. */
void __cdecl WarmPropRenderChildren(int pEntry) /* 0x00419560 */
{
    /* Declared in this order for the exe's register allocation, the inner loop
     * a `for` with both walkers initialised (byte-exact in the whole unit). */
    short   err;
    int     base;
    int    *hGrand;
    int     idx;
    uint   *pct;
    int    *local14;
    int     childData;
    uint   *pu3;
    int     inner;
    short   heapOk;

    if (*(short *)((char *)pEntry + 0x4c) != 0)
        return;

    err = ReserveHeapSpace(200000, &heapOk);
    if (err != 0)
        FatalError(err, 0xFE9);

    if (heapOk == 0)
        return;

    base = GetResDataPtr(*(int **)((char *)pEntry + 6));
    err = AttachRenderChild(*(int **)((char *)pEntry + 2), *(uint *)((char *)pEntry + 0xe), &local14);
    if (err != 0)
        FatalError(err, 0xFEA);

    idx = 0;
    pct = (uint *)(base + 0x5e);
    while (idx < *(int *)(base + 0x5a)) {
        err = AttachRenderChild(*(int **)((char *)pEntry + 2), *pct, &local14);
        if (err != 0)
            FatalError(err, 0xFEB);

        childData = GetResDataPtr(local14);
        for (inner = 0, pu3 = (uint *)(childData + 0x76);
             inner < *(int *)(childData + 0x72);
             pu3 += 0xb, inner++) {
            err = AttachRenderChild(*(int **)((char *)pEntry + 2), *pu3, &hGrand);
            if (err != 0)
                FatalError(err, 0xFEC);
        }
        UnlockResObject(local14);
        idx++;
        pct += 8;
    }
    UnlockResObject(*(int **)((char *)pEntry + 6));
    *(short *)((char *)pEntry + 0x4c) = 1;
}


/* The mirror of WarmPropRenderChildren: detaches the row, then every child's grandchildren and the
 * child itself, and clears the flag at +0x4c. The child is detached twice (0xff0, 0xff1) in the exe.
 *
 * Port note: the port had a guessed one-liner here (RemoveCricketEntry on +0x4e), so prop bindings
 * were never detached (a class 12 body). */
void __cdecl UnwarmPropRenderChildren(int pEntry)   /* 0x004196c0 */
{
    short   err;
    int     base;
    int    *local14;
    int     idx;
    uint   *pct;
    int     childData;      /* declaration order sets the registers: this order is the exe's */
    int    *pu3;
    int     inner;
    short    u;

    if (*(short *)((char *)pEntry + 0x4c) != 0) {
        base = GetResDataPtr(*(int **)((char *)pEntry + 6));
        u = DetachRenderChild(*(int *)((char *)pEntry + 2), *(int *)((char *)pEntry + 0xe));
        if (u != 0)
            FatalError(u, 0xFED);
        idx = 0;
        pct = (uint *)(base + 0x5e);
        if (*(int *)(base + 0x5a) > 0) {
            do {
                err = AttachRenderChild(*(int **)((char *)pEntry + 2), *pct, &local14);
                if (err != 0)
                    FatalError(err, 0xFEE);
                childData = GetResDataPtr(local14);
                inner     = 0;
                pu3       = (int *)(childData + 0x76);
                if (*(int *)(childData + 0x72) > 0) {
                    do {
                        u = DetachRenderChild(*(int *)((char *)pEntry + 2), *pu3);
                        if (u != 0)
                            FatalError(u, 0xFEF);
                        pu3 += 0xb;
                        inner++;
                    } while (inner < *(int *)(childData + 0x72));
                }
                UnlockResObject(local14);
                u = DetachRenderChild(*(int *)((char *)pEntry + 2), *pct);
                if (u != 0)
                    FatalError(u, 0xFF0);
                u = DetachRenderChild(*(int *)((char *)pEntry + 2), *pct);
                if (u != 0)
                    FatalError(u, 0xFF1);
                idx++;
                pct += 8;
            } while (idx < *(int *)(base + 0x5a));
        }
        UnlockResObject(*(int **)((char *)pEntry + 6));
        *(short *)((char *)pEntry + 0x4c) = 0;
    }
}


/* The twin of CloneActorFrameImage: looks the prop up, sets its view and pose, picks the frame
 * nearest the angle and returns a CLONE of that frame's resource (AttachRenderChild, InstallCLUT,
 * detach). */
uint32_t __cdecl ClonePropFrameImage(byte *pPropName, byte *pViewName, short angle, undefined2 animIdx)  /* 0x00419830 */
{
  short attachStatus;
  short status;
  int scopeData;
  uint32_t hClone;  /* local_d0 */
  /* The 0x9e-byte prop row. LookupPropEntry writes all 158 bytes, so the [6] declaration was a
   * 152-byte stack smash. Ghidra names the fields as separate locals; offsets are (0xcc - local
   * number): +0x02 the parent handle, +0x06 the resource handle (it was read UNINITIALISED into
   * GetResDataPtr), +0x20 the caller's parameter (written to a dead local), +0x7e the view name,
   * exactly where LookupStateSpriteFrameGeometryByAngle compares each binding (written to a
   * scratch buffer, the requested view never took effect). The twin of CloneActorFrameImage,
   * whose equivalents sit two bytes higher at +0x22 and +0x80. */
  undefined1 stateRow [160];

#define ISA_RESHANDLE (*(int **)(stateRow + 0x06))
#define ISA_PARAM     (*(undefined2 *)(stateRow + 0x20))
#define ISA_VIEWNAME  ((byte *)(stateRow + 0x7e))
  uint32_t frameGeom [11];  /* local_2c */

  status = LookupPropEntry(pPropName, (uint32_t *)stateRow);
  if (status != 0) {
    return 0;
  }
  if (*pViewName >= 0x10) {
    return 0;
  }
  CopyPascalString(pViewName, ISA_VIEWNAME);
  if (ISA_VIEWNAME[0] == 0) {
    scopeData = GetResDataPtr(ISA_RESHANDLE);
    if (*(int *)(scopeData + 0x5a) <= 0) {
      UnlockResObject(ISA_RESHANDLE);
      return 0;
    }
    CopyPascalString((byte *)(scopeData + 0x6e), ISA_VIEWNAME);
    UnlockResObject(ISA_RESHANDLE);
  }
  ISA_PARAM = animIdx;
  if ((short)LookupStateSpriteFrameGeometryByAngle((int)stateRow, angle, frameGeom) == 0) {
    return 0;
  }
  attachStatus = AttachRenderChild((uint32_t *)(*(uint32_t *)(stateRow + 2)), frameGeom[0], &hClone);
  if (attachStatus != 0) {
    FatalError(attachStatus, 0xff2);
  }
  CloneResource(&hClone);
  status = DetachRenderChild((*(uint32_t *)(stateRow + 2)), frameGeom[0]);
  if (status != 0) {
    FatalError(status, 0xff3);
  }
  return hClone;
}

