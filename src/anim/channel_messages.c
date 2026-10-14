/* channel_messages.c -- TI.EXE 0x00431640..0x00431ee0: channel message dispatch and the game loop. */

#include <windows.h>
#include <mmsystem.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "df_protos.h"
#include "topics/input.h"
#include "topics/gameloop.h"
#include "topics/scene.h"
#include "topics/hotspot_raster.h"
#include "topics/reset_channel.h"
#include "topics/scriptcmd.h"
#include "topics/savestate.h"

/* Forwards (msg, pParam) to the handler for the channel's type code, the first short of the
 * channel buffer: 1 = scene, 2 = messages, 3 = debugger. Any other type is ignored.
 * Message codes seen in RunGameLoop: 0 activate, 1 deactivate, 2 set data, 3 toggle,
 * 4 end scene, 5/6 enter/leave render lock, 7 set position, 8 idle, 9 reset, 0xb commit,
 * 0xf set param, 0x10 flush. */
void __cdecl DispatchChannelMessage(short *pChannel, short msg, byte *pParam)  /* 0x00431640 */
{
    DF_TRACE("chanMsg: type=%d msg=%d", (int)*pChannel, (int)msg);
    switch (*pChannel) {
    case 1:
        SceneChannelHandler((int)pChannel, msg, pParam);
        break;
    case 2:
        MessagesChannelHandler((int)pChannel, msg, (int *)pParam);
        break;
    case 3:
        DebuggerChannelHandler((int)pChannel, msg, (int *)pParam);
        break;
    }
}


/* Selects the channel's scene state as the current port, then sends it message 5
 * (enter render lock). Always paired with LeaveSceneRenderLock on the same channel. */
void __cdecl EnterSceneRenderLock(int pChannel)  /* 0x004316a0 */
{
    QdSetPort((int *)(pChannel + 0x12));
    DispatchChannelMessage((short *)pChannel, 5, (byte *)0);
}


/* Sends the channel message 6 (leave render lock); the counterpart of EnterSceneRenderLock.
 *
 * Port note: the original takes no declared parameter and reads its caller's first stack
 * argument, which only works with MSVC's frame layout. GCC lays frames out differently (a live
 * SIGSEGV), so pChannel is an explicit parameter; every call site has it from its Enter. */
void __cdecl LeaveSceneRenderLock(int pChannel)  /* 0x004316c0 */
{
    DispatchChannelMessage((short *)pChannel, 6, (byte *)0);
}


/* The game's main loop, run from WinMain. The outer loop restarts the session while
 * gGameLoopState is 2 (new game / retry); the inner loop is one pass per frame and runs while
 * the state is 0. Each frame pumps messages, resets the three channels, then routes one polled
 * event by type or, after 30 ticks of 60 Hz quiet (half a second), sends the channels the
 * idle message.
 *
 * Port note: DF_FIX_GAMELOOP_SEED_SCENESTATE seeds frame.sceneState, which the original passes
 * to QdSetPort uninitialised; GCC's stack garbage crashed there (port-bug-classes.md section 8). */
void __stdcall RunGameLoop(void)  /* 0x004316e0 */
{
    /* Declaration order is the exe's stack order (first declared = lowest). */
    GameEvent  ev;                /* local_b0..local_a2 -- current event          */
    short      sceneRect[4];      /* local_9c..local_96 -- viewport rect {y1,x1,y2,x2} */
    NodeFrame  frame;             /* local_94 + local_82 -- ONE 146-byte object */
    int        deadline;
    int        delta;
    short      r;
    short      key;

    ConvertKeywordTablesToPascal();
    ConvertMessageNamesToPascal();

    do
    {
        gGameLoopState = 0;
        BeginGameSession();

        deadline = GetTick60Hz() + 0x1e;

        do
        {
            PumpMessages();

            r = FlushAllRenderNodes();
            if (r != 0)
                FatalError(r, 0x113a);

            /* The exe passes channel B's own resource handle here, not the frame's scene state. */
            QdSetPort((int *)&gChannels[1].hResource);
#if DF_FIX_GAMELOOP_SEED_SCENESTATE
            memcpy(frame.sceneState, gThePortDesc, sizeof(frame.sceneState));
#endif
            GetSceneCursorPos((int)&gChannelB, (short *)&ev.pNode);
            GetSceneRect((int *)&gChannels[1].hResource, sceneRect);

            if (PointInSceneRect(*(ScenePointYX *)&ev.pNode, sceneRect) == 0)
                QdInitCursor();

            DispatchChannelMessage((short *)&gChannels, 9, (byte *)0);
            DispatchChannelMessage((short *)&gChannelB, 9, (byte *)0);
            DispatchChannelMessage((short *)&gChannelC, 9, (byte *)0);

            /* the exe pushes -1: a `short` mask */
            if (PollEventQueue(-1, (unsigned int *)&ev) != 0)
            {
                deadline = GetTick60Hz() + 0x1e;

                DF_TRACE("gameloop ev: type=%d field_2=%u mods=0x%x",
                         (int)ev.eventType, (unsigned)ev.field_2,
                         (unsigned)ev.modifiers);
                switch (ev.eventType)
                {
                case 1:   /* click; HitTestHotspot returns the action code */
                    r = HitTestHotspot(*(unsigned int *)&ev.pNode, (NodeDesc *)&frame);
                    DF_TRACE("click: scenePt=(y=%d,x=%d) hitTest=%d nodeType=%d",
                             (int)((short *)&ev.pNode)[0], (int)((short *)&ev.pNode)[1],
                             (int)r, (int)frame.nodeType);
                    switch (r)
                    {
                    case 1:
                        DispatchMenuStateInteraction();
                        QdInitCursor();
                        DispatchNavCommand(ResolveNavCommandId(&ev, 1));
                        break;

                    case 2:
                        NavigateBack((NodeDesc *)&frame, &ev);
                        break;

                    case 3:
                        /* Byte match: the block sits here and case 5 jumps in, which puts it
                         * after case 5's node-type test as in the exe. */
                        if (IsInteractionAllowed((NodeDesc *)&frame) != 0)
                        {
                        ENTER_SUBVIEW:
                            QdSetPort(frame.sceneState);
                            MapScreenPointToNode((NodeDesc *)&frame, (short *)&ev.pNode);
                            if (CheckSubviewEntry((NodeDesc *)&frame, &ev) == 0)
                                DispatchChannelMessage(&frame.nodeType, 0, ev.pNode);
                        }
                        break;

                    case 4:
                        if (IsInteractionAllowed((NodeDesc *)&frame) != 0)
                            SecondaryNodeAction((NodeDesc *)&frame, &ev);
                        break;

                    case 5:
                        if (IsInteractionAllowed((NodeDesc *)&frame) != 0)
                        {
                            if (frame.nodeType == 1)
                            {
                                goto ENTER_SUBVIEW;
                            }
                            else
                            {
                                /* delta: low 16 = dx, high 16 = dy. The window rect is read into
                                 * sceneRect, its far corner moved by delta, and sent as message 7. */
                                delta = GetScrollDelta((NodeDesc *)&frame, &ev);
                                if (delta != 0)
                                {
                                    GetChannelWindowRect((int)&frame, sceneRect);
                                    sceneRect[3] = sceneRect[1] + (short)delta;
                                    sceneRect[2] = (short)(delta >> 16) + sceneRect[0];
                                    DispatchChannelMessage(&frame.nodeType, 7, (byte *)sceneRect);
                                    QdSetPort(frame.sceneState);
                                    InvalidateSceneRect((short *)&gScrollRect);
                                }
                            }
                        }
                        break;

                    case 6:
                        if (IsInteractionAllowed((NodeDesc *)&frame) != 0)
                            HideHitNodeWindow((NodeDesc *)&frame, &ev);
                        break;

                    case 7:
                        if (IsInteractionAllowed((NodeDesc *)&frame) != 0)
                        {
                            MapScreenPointToNode((NodeDesc *)&frame, (short *)&ev.pNode);
                            PerformNodeOperation((NodeDesc *)&frame, &ev, 7);
                        }
                        break;

                    case 8:
                        if (IsInteractionAllowed((NodeDesc *)&frame) != 0)
                        {
                            MapScreenPointToNode((NodeDesc *)&frame, (short *)&ev.pNode);
                            PerformNodeOperation((NodeDesc *)&frame, &ev, 8);
                        }
                        break;
                    }
                    break;

                case 3:
                    if ((ev.modifiers & 0x1fa0) != 0)
                    {
                        DispatchMenuStateInteraction();
                        DispatchNavCommand(ResolveNavCommandId(&ev, 0));
                    }
                    else if (FindActiveHotspot((NodeDesc *)&frame) != 0)
                    {
                        /* For a type-2 node and key 0x1c..0x1f channel B's whole 146-byte
                         * state replaces the frame: descriptor and scene state are one
                         * object (see NodeFrame). */
                        key = (short)*(int *)&ev.field_2 & 0xff;
                        if (frame.nodeType == 2 && key >= 0x1c && key <= 0x1f)
                            frame = *(NodeFrame *)&gChannelB;
                        DispatchChannelMessage(&frame.nodeType, 2, (byte *)(int)key);
                    }
                    break;

                case 5:   /* same logic as case 3, message 0xf */
                    if ((ev.modifiers & 0x1fa0) == 0 && FindActiveHotspot((NodeDesc *)&frame) != 0)
                    {
                        key = (short)*(int *)&ev.field_2 & 0xff;
                        if (frame.nodeType == 2 && key >= 0x1c && key <= 0x1f)
                            frame = *(NodeFrame *)&gChannelB;
                        DispatchChannelMessage(&frame.nodeType, 0xf, (byte *)(int)key);
                    }
                    break;

                case 6:
                    r = FindEventViewportDesc((NodeDesc *)&frame, &ev);
                    DF_TRACE("paint-ev: FindEventViewportDesc=%d", (int)r);
                    if (r == 0)
                    {
                        /* the exe passes the frame here, where the other caller of the
                         * Save/Restore pair passes gMovieChannel; both hooks are empty */
                        QdSetPort(frame.sceneState);
                        SaveNavigationState(&frame);
                        DispatchChannelMessage(&frame.nodeType, 1, (byte *)0);
                        RestoreNavigationState(&frame);
                    }
                    break;

                case 7:   /* quit requested */
                    HandleQuitRequest(&ev);
                    break;

                case 8:   /* message 3 with the modifier bit 0 as its parameter */
                    if (FindEventViewportDesc((NodeDesc *)&frame, &ev) == 0)
                    {
                        if ((ev.modifiers & 1) != 0)
                            DispatchChannelMessage(&frame.nodeType, 3, (byte *)1);
                        else
                            DispatchChannelMessage(&frame.nodeType, 3, (byte *)0);
                    }
                    break;

                case 0x17:   /* purpose not yet understood */
                    HandleEvent23(&ev);
                    break;

                case 0x2a:   /* resets the scroll rect and flushes (0x10) all three channels */
                    ResetSceneRectToScreen((short *)&gScrollRect);
                    DispatchChannelMessage((short *)&gChannels, 0x10, (byte *)0);
                    DispatchChannelMessage((short *)&gChannelB, 0x10, (byte *)0);
                    DispatchChannelMessage((short *)&gChannelC, 0x10, (byte *)0);
                    break;
                }
            }
            else
            {
                if ((int)GetTick60Hz() >= deadline)   /* signed compare */
                {
                    DispatchChannelMessage((short *)&gChannels, 8, (byte *)0);
                    DispatchChannelMessage((short *)&gChannelB, 8, (byte *)0);
                    DispatchChannelMessage((short *)&gChannelC, 8, (byte *)0);
                }
            }

        } while (gGameLoopState == 0);

        EndGameSession();

    } while (gGameLoopState == 2);
}



/* Sends channel B the commit message (0xb) that precedes every node activation. */
void __stdcall DispatchMenuStateInteraction(void)  /* 0x00431cc0 */
{
    DispatchChannelMessage((short *)&gChannelB, 0xb, (byte *)0);
}


/* Initialises the per-scene state: cursors, scroll rect, every subsystem, the viewport and the
 * hotspot buffer. Called by RunGameLoop at the top of each outer iteration. */
void __stdcall BeginGameSession(void)  /* 0x00431ce0 */
{
    gCursor    = (HCURSOR)GetResourceHandle(0x43555253u, 0x7d2u);   /* 'CURS' id 2002 */
    gCursorAlt =          GetResourceHandle(0x43555253u, 0x83u);  /* 'CURS' id 131  */

    InstallCursor(gCursor);

    ResetSceneRectToScreen((short *)&gScrollRect);

    InitDisplayAttachSlots();
    InitScriptRuntime();
    InitSceneNodeChannel();
    DF_TRACE("probe1 after InitSceneNodeChannel: chanB+0x12=%p",
             *(void **)((char *)&gChannelB + 0x12));
    InitMessagesChannel();
    InitDebugger();
    InitSceneResource(1);

    gSceneViewport.y = 0;
    gSceneViewport.x = 0;
    gSceneViewport.w = gSceneW;
    gSceneViewport.h = gSceneH;   /* store order is the exe's */
    DF_TRACE("sceneViewport init: {x=%d,y=%d,h=%d,w=%d} (gSceneH=%d gSceneW=%d) @%p",
             (int)gSceneViewport.x, (int)gSceneViewport.y,
             (int)gSceneViewport.h, (int)gSceneViewport.w,
             (int)gSceneH, (int)gSceneW, (void *)&gSceneViewport);

    /* the exe passes the viewport's own fields (the loads it made for the stores) */
    InitHotspotBuffer((short *)&gHotspotBuf, gSceneViewport.w, gSceneViewport.h);
    ZeroBlock(gHotspotBuf.pPx, (int)gSceneW * (int)gSceneH);
    CopyHotspotBufState((unsigned int *)&gHotspotBuf, &gSavedHotspotBuf);

    LoadPluginDirectory();
    AllocScriptStringSlots();
    InitGlobalsScope();
    InitSetView();
    InitRenderState();
    InitSoundSystem();
}


/* Tears down what BeginGameSession built, in roughly reverse order, after the inner loop
 * exits. Sends message 4 (end scene) to all three channels. */
void __stdcall EndGameSession(void)  /* 0x00431de0 */
{
    ShutdownSceneResource(1);
    DestroyAllRenderNodes();

    DispatchChannelMessage((short *)&gChannelB, 4, (byte *)0);
    DispatchChannelMessage((short *)&gChannels, 4, (byte *)0);
    DispatchChannelMessage((short *)&gChannelC, 4, (byte *)0);

    DestroyHotspotBuffer((int)&gHotspotBuf);
    TeardownSceneRefs(&gSavedHotspotBuf);   /* the exe pushes it and cleans up 4 bytes; the callee is a bare RET */

    ShutdownRenderState();
    UnloadPluginDirectory();
    FreeScriptStringSlots();
    TeardownGlobalsScope();
    ShutdownSetView();
    ShutdownScriptRuntime();
    ShutdownSoundSystem();
    RestoreMenuBar();
    ShutdownDebugger();

    ReleaseResource((void *)gCursor,    0x43555253u);   /* 'CURS' id 2002; a no-op */
    ReleaseResource(        gCursorAlt, 0x43555253u);   /* 'CURS' id 131; a no-op */
    ShutdownDisplayAttachSlots();   /* the exe tail-jumps here (JMP 0x00422090) */
}



/* Forwards to ValidateDescriptorPath; its only caller passes the type 'MOVE'. */
short __cdecl ValidateDescriptorPathOfType(byte *pDesc, int fileType)  /* 0x00431ea0 */
{
    return ValidateDescriptorPath(pDesc, fileType);
}

void __cdecl OrInputFlagsForDisplayAttachTrackRow(short flagBits) /* 0x00431ec0 */
{
    /* a read-modify-write in memory (or word ptr [..],ax): the word is volatile in the
     * original -- the display-attach thread polls it */
    *(volatile short *)&gActionFrameFlags |= flagBits;
}


/* Both no-ops are `ret` in the exe but their callers push an argument and clean it up
 * (0x00448d7a `add esp,4` after pushing gMovieFrameIndex; seven `push ebx` sites with the
 * hotspot loop index): a cdecl parameter the body never reads. */
void __cdecl NoOpDisplayAttachBeforeUserInteraction(int rowIdx) /* 0x00431ed0 */
{
    (void)rowIdx;
}


void __cdecl NoOpAfterDisplayAttachVoice(int hotspotIdx) /* 0x00431ee0 */
{
    (void)hotspotIdx;
}
