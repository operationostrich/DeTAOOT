/* Declarations for the "gameloop" topic: the preamble and shared definitions of the functions grouped under that name. */
#ifndef DF_TOPIC_GAMELOOP_H
#define DF_TOPIC_GAMELOOP_H
/*
 * Address: 0x004316a0 (EnterSceneRenderLock), 0x004316c0 (LeaveSceneRenderLock),
 *          0x004316e0 (RunGameLoop)
 *
 * Main scene interaction loop. RunGameLoop() is called from WinMain after AppInit.
 *
 * gGameLoopState -- loop control flag
 *   0  = keep running inner loop
 *   1  = exit (return from function)
 *   2  = restart outer loop (reset game state / start over)
 *
 * Event types (ev.eventType)
 * ---------------------------
 *  1    -- hotspot activated (left click / interact)
 *  3    -- navigation / direct transition (e.g. move to room)
 *  5    -- secondary navigation / transition (e.g. right-click move)
 *  6    -- navigation with state query
 *  7    -- quit / close requested
 *  8    -- drag / grab-and-move
 *  0x17 -- special event (purpose TBD)
 *  0x2a -- attract-mode / idle timeout trigger
 *
 * Hotspot action codes returned by HitTestHotspot
 * -----------------------------------------------
 *  1  -- confirm / open -> play animation then navigate
 *  2  -- cancel / back
 *  3  -- enter sub-view (navigate deeper)
 *  4  -- secondary action on node
 *  5  -- pan / scroll (if node type != 1), or enter sub-view (if type == 1)
 *  6  -- tertiary action on node
 *  7  -- set node as active + operation A
 *  8  -- set node as active + operation B (same as 7)
 *
 * Input channel buffers (reset every frame to type 9)
 * ---------------------------------------------------
 *  gChannels  -- channel A input state
 *  gChannelB  -- channel B input state / last-hit-node data
 *  gChannelC  -- channel C input state
 *
 * Key globals
 * -----------
 *  gGameLoopState  -- loop control / scene-result flag
 *  gChannels[1].hResource  -- current scene / position data
 *  gScrollRect  -- current scroll / pan position
 */

#include <windows.h>
#include <string.h>

#include "df_port_types.h"   /* NodeDesc (22 bytes, packed) */

#pragma pack(push, 1)
typedef struct
{
    short nodeType;        /* +0x00  local_94[0] -- channel type code      */
    short data[8];         /* +0x02  local_94[1..8]                        */
    int   sceneState[32];  /* +0x12  local_82[0..31]; sceneState[0] is the
                            *        same 4 bytes as NodeDesc.pNodeObj    */
} NodeFrame;               /* 146 bytes -- local_94 and local_82 together  */
#pragma pack(pop)

/* Compile-time proof of the 146-byte footprint above (gnu99: no _Static_assert,
 * so use the negative-array-size idiom). */
typedef char NodeFrame_is_146_bytes[(sizeof(NodeFrame) == 146) ? 1 : -1];
typedef char NodeDesc_heads_NodeFrame[(sizeof(NodeDesc) == 22) ? 1 : -1];

/* Position / coordinate pair */
typedef struct { short x, y; } ScenePos;

/* Scene-space bounding rectangle (4 shorts = 8 bytes) */
#ifndef DF_SCENERECT_T
#define DF_SCENERECT_T
typedef struct { short y1, x1, y2, x2; } SceneRect;
#define gScrollSceneRect (*(SceneRect *)gScrollRect)
#endif

extern void         *gViewportTable[];         /* 0x00486bb0 -- hotspot node-object pointer array; scene.c */
extern int           gViewportCount;           /* 0x0048836c -- number of active hotspot entries; scene.c */

/* Nav command table (used by ResolveNavCommandId) */
extern byte         *gNavCommandTable;   /* 0x00486c50-base of stride-8 command entries */
extern int           gNavCommandCount;   /* 0x00486e78 -- number of entries */
extern HMENU         gMainMenu;         /* 0x00488150 -- main navigation menu handle */

/* Globals used by InvalidateSceneRect */
extern HWND *g_pSceneHwnd;  /* 0x004872a8 -- ptr to main scene window HWND */
extern int  *gThePort;    /* 0x004883c8-current scene node ref; +0x50=screenX, +0x54=screenY */

#endif
