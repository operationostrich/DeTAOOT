/* the save image 0x0048a560..0x0048a872 (0x312 = 786 bytes) and the three subsystem handle
 * structs inside it. New header include/topics/savestate.h, #included mid-file (above the first function,
 * after the preamble) by the 13 units that name one of its members: wave_device_scene_cmds, waypoint_paths,
 * cast_actors, save_props_shops, script_dispatch, scene_channel_scripts, channel_messages,
 * trigger_overlay_actions, puppet_transitions, script_bindings, stage, interpreter, track_sound_cmds.
 * Cost: 5 typedefs = 10 C1 symbols + 1 for the #include = 11 per user unit.
 * The engine copies the range as ONE block (0xc4 dwords + 1 short, transition body 0x004139a0), so the storage
 * stays `byte g_saveStateImage[0x312]`; gSave is a view. pack(1): sceneFrame / frameRate sit at 2 mod 4.
 * Every flat name below stays a macro over the struct (they are the script-visible variable names, 1294 uses):
 * `#define gSceneFrame (gSave.sceneFrame)`. Offsets are not asserted in this header
 * (a typedef in a header is a symbol). Names marked ? are placeholders for unnamed fields (offset in the name). */
#pragma pack(push, 1)
typedef struct SaveSlotRef { void *table; int count; } SaveSlotRef;      /* 8: gActorTable/Count .. gTrackTable/Count */

typedef struct StageState {          /* 44 = 0x2c at 0x0048a74c (image +0x1ec) */
    short active;                    /* +0x00 `*(short *)gStage`: 0 = no stage open (error 0xb) */
    short flag1;                     /* +0x02 DAT_0048a74e  (stage.c header: "flag1") */
    short flag2;                     /* +0x04 DAT_0048a750 */
    short w06;                       /* +0x06 DAT_0048a752 */
    int   ctx;                       /* +0x08 DAT_0048a754 "stage ctx"; +0x530 holds the scene handle */
    int   page;                      /* +0x0c DAT_0048a758 current page */
    int   prevPage;                  /* +0x10 DAT_0048a75c */
    void *hChild;                    /* +0x14 DAT_0048a760 "child handle" (GetResDataPtr/UnlockResObject) */
    int   selector;                  /* +0x18 DAT_0048a764 */
    byte  name[16];                  /* +0x1c DAT_0048a768 Pascal string */
} StageState;                        /* 0x2c */

typedef struct SetState {            /* 128 = 0x80 at 0x0048a778 (image +0x218) */
    short active;                    /* +0x00 `*(short *)gSet` */
    short w02, w04, w06;             /* +0x02 DAT_0048a77a (set "ready") .. +0x06 */
    int   i08;                       /* +0x08 DAT_0048a780 */
    void *p0c;                       /* +0x0c DAT_0048a784 */
    int   i10;                       /* +0x10 DAT_0048a788 */
    short w14, w16, w18;             /* +0x14 DAT_0048a78c, a78e, a790 */
    short cameraHeight;              /* +0x1a gCameraHeight */
    short w1c, w1e, w20, w22, w24, w26, w28, w2a;   /* +0x1c DAT_0048a794 .. +0x2a DAT_0048a7a2 */
    short currentDeg;                /* +0x2c gCurrentDeg */
    short playerX, playerY, playerZ; /* +0x2e gPlayerX/Y/Z */
    short cameraAngle;               /* +0x34 */
    short cameraX, cameraY, cameraZ; /* +0x36 */
    byte  name[16];                  /* +0x3c DAT_0048a7b4 ('currentset' Pascal) */
    byte  sceneName[16];             /* +0x4c gSceneName */
    byte  viewName[16];              /* +0x5c gViewName */
    int   i6c;                       /* +0x6c DAT_0048a7e4 */
    int   waypointTable;             /* +0x70 gWaypointTable */
    int   i74;                       /* +0x74 DAT_0048a7ec */
    int   sceneCount;                /* +0x78 gSceneCount */
    void *sceneTable;                /* +0x7c gSceneTable */
} SetState;                          /* 0x80 */

typedef struct PuppetState {         /* 52 = 0x34 at 0x0048a7f8 (image +0x298) */
    short active;                    /* +0x00 `gPuppet[0]`: the scene-transition guard */
    short w02;                       /* +0x02 DAT_0048a7fa */
    int   i04;                       /* +0x04 DAT_0048a7fc */
    byte  unk08[4];                  /* +0x08 no name anywhere (0x0048a800) */
    int   i0c;                       /* +0x0c DAT_0048a804 */
    void *p10, *p14, *p18, *p1c, *p20;   /* +0x10 DAT_0048a808 .. +0x20 DAT_0048a818 */
    byte  name[16];                  /* +0x24 DAT_0048a81c ('currentpuppet' Pascal) */
} PuppetState;                       /* 0x34 */

typedef struct SaveState {           /* 0x312 = 786 */
    ChannelState channels[3];        /* +0x000 (0x92 each) */
    short visualEffectDepth;         /* +0x1b6 gVisualEffectDepth */
    short keyAborts;                 /* +0x1b8 gKeyAborts */
    int   sceneFrame;                /* +0x1ba gSceneFrame (2 mod 4) */
    int   frameRate;                 /* +0x1be gFrameRate */
    short actionFrameFlags;          /* +0x1c2 gActionFrameFlags */
    SaveSlotRef slot[5];             /* +0x1c4 actor, cast, prop, shop, track */
    StageState  stage;               /* +0x1ec */
    SetState    set;                 /* +0x218 */
    PuppetState puppet;              /* +0x298 */
    short clutSyncFirst, clutSyncEnd, textColor, textHighlightColor, textFont, textSize,
          puppetParam7, puppetNoTimeout, speakTicksPerChar, textIndent;   /* +0x2cc .. +0x2de */
    short puppetGrab;                /* +0x2e0 gPuppetGrab */
    byte  puppetBase[32];            /* +0x2e2 gPuppetBase (Pascal) */
    short sceneW, sceneH;            /* +0x302 */
    short overlayW, overlayH;        /* +0x306 */
    short sceneOriginY, sceneOriginX;/* +0x30a */
    int   bootNode;                  /* +0x30e gBootNode */
} SaveState;
#pragma pack(pop)
#define gSave (*(SaveState *)g_saveStateImage)
