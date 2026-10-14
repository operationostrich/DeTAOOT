/* SceneRect, the EXISTING QuickDraw Rect {y1, x1, y2, x2} (8 bytes), now also visible to the units that
 * name gScrollRect (0x0048a2c0) but do not include topics/gameloop.h (debugger, scene_channel_scripts, messages_channel). New header
 * include/topics/scenerect.h, #included mid-file in those three units (above the first function): 2 symbols for the
 * typedef + 1 for the include = 3 each. gameloop.h keeps its own definition behind the same guard (its 8 includers and channel_messages / movie_player
 * see no change at all, so the move cannot cost 2 symbols in EVERY unit).
 * gScrollRect stays `short[4]` (storage and the `&gScrollRect` / `(short *)&gScrollRect` call sites unchanged);
 * the typed view is gScrollSceneRect, used for the field reads: [0]=y1 [1]=x1 [2]=y2 [3]=x2. */
#ifndef DF_SCENERECT_T
#define DF_SCENERECT_T
typedef struct { short y1, x1, y2, x2; } SceneRect;
#define gScrollSceneRect (*(SceneRect *)gScrollRect)
#endif
