# Channel / Message System

Architecture reference for the DreamFactory channel and message-passing system.

---

## Overview

The engine uses three persistent **input channels** to route game events to subsystems. Each channel has a type, a 146-byte state buffer, and a resource handle. Channels are initialised per-scene and torn down by `EndGameSession`.

| Channel | Type | State buffer | Init function | Subsystem |
|---|---|---|---|---|
| A | 2 (animation) | `0x0048a560` | `InitMessagesChannel` (`0x00438ed0`) | Anim text overlay |
| B | 1 (scene-node) | `0x0048a5f2` | `InitSceneNodeChannel` | Scene interaction |
| C | 3 (multimedia) | `0x0048a684` | `InitDebugger` (`0x0040cbd0`) | Playback / UI |

Each state buffer is 146 bytes (`0x92`). The resource handle sits at offset `+0x12`; if non-zero when a channel is re-initialised, the engine triggers a fatal error (double-init guard).

See [structs.md](structs.md) for the partial `ChannelState` layout.

---

## Message Dispatch

### DispatchChannelMessage (`0x00431640`)

The central router called each game-loop tick (from `RunGameLoop`). Routes an event by channel type:

- **Type 1** (scene-node / channel B): forwards to the scene-node channel handler
- **Type 2** (animation / channel A): forwards to `MessagesChannelHandler` (`0x00438a10`)
- **Type 3** (multimedia / channel C): forwards to `DebuggerChannelHandler` (`0x004405c0`)

Each handler receives `(pChannel, msg, pParam)`.

### SetChannelWindowState (`0x00435b60`)

Hides or shows the viewport window associated with a channel. Called when transitioning between scenes or suspending the UI for modal dialogs (see `modalui.c`).

---

## Channel Initialisation Sequence (per scene)

```
BeginGameSession()
  ├── InitMessagesChannel()   -- channel A: 37-px-tall viewport strip; 436 px wide
  ├── InitSceneNodeChannel()   -- channel B: "Runtime" resource; clears DAT_0048a0d0
  └── InitDebugger()  -- channel C: playback viewport (200 px tall, offset right of centre)
```

Each init:
1. Checks resource handle at `+0x12` is NULL (fatal if non-zero)
2. Allocates/locks a resource object into `+0x12`
3. Sets up the viewport rectangle
4. Installs subsystem-specific state

---

## Animation Channel (Type 2)

**Source:** `src/anim.c`

Handler: `MessagesChannelHandler` (`0x00438a10`) — handles 10 message cases:

| Message | Action |
|---|---|
| activate | Enable animation display |
| click | Handle click on anim overlay |
| CLUT | Apply colour look-up table |
| end scene | Tear down anim state |
| init ref | Initialise resource reference |
| teardown ref | Release resource reference |
| viewport | Update viewport rect |
| info | Query info |
| centre | Centre overlay |
| (other) | No-op |

The animation system supports up to 10 simultaneous slots (`DAT_0048a0d4 = 10`). Each slot has a 256-byte descriptor entry in the descriptor table (`DAT_0048a0d8`). Compositing scratch area: 409,600 bytes from secondary heap (`DAT_0048a0a0`).

---

## Scene-Node Channel (Type 1)

**Source:** `src/scene.c`, `src/scriptcmd.c`

The scene-node channel handles player interaction with the rendered scene (hotspot hit-testing, script dispatch). It is driven by events from `PollEventQueue` → `RunGameLoop` and by the script engine via opcode 12017 (`playmovie` / `ResetChannel`).

Key state: `DAT_0048a0d0` (scene-node interaction state, cleared by `InitSceneNodeChannel`).

---

## Multimedia Channel (Type 3)

**Source:** `src/multimedia.c`

Handler: `DebuggerChannelHandler` (`0x004405c0`) — handles 12 message cases including playback start/stop, UI layout, and playlist management.

Key globals:
| Address | Name | Notes |
|---|---|---|
| 0x00460da0 | network session flag | Checked by opcodes 20110–20114 |
| 0x00460dcc | current playback index | Init to -1 (none) |
| 0x00460dec | playback counter | |
| 0x00460df8 | playback state | |
| 0x00460de4 | playback mode | Init to 2 |
| 0x00460c10 | descriptor slot table base | 20 entries × 256 bytes |

The playback viewport is 200 px tall, offset right of centre. The script work queue (`DAT_00460aa0`) holds 40 entries × 8 bytes; max 39 pending (`DAT_00460be0`).

---

## Event Types (routed via RunGameLoop)

Events pulled from `PollEventQueue` are dispatched by type in `RunGameLoop`:

| Type | Meaning |
|---|---|
| 1 | Input event (keyboard / mouse) |
| 3 | Scene-node event |
| 5 | Channel message (activate) |
| 6 | Channel message (deactivate) |
| 7 | Timer / tick event |
| 8 | Sound event |
| 0x17 (23) | Network event |
| 0x2a (42) | Unknown |

---

## Modal UI Interaction

When a modal dialog is shown (e.g. `MessageBox`, `DialogBoxParam`), the game suspends the channel system via `SuspendGameUiForModalDialog` (`0x00420580`):

1. Hides all viewports (`SetChannelWindowState`)
2. Shows the cover window
3. Releases palette to Windows
4. Hides cursor

Restored by `RestoreGameUiAfterModalDialog` (`0x00420660`) in reverse order.

See `src/modalui.c`.

---

## Reset Channel (opcode 12017 — `playmovie`)

`ActionCmd_PlayMovie` → `PlayMovie` (`reset_channel.c`, `0x004486a0`):

1. `ValidateDescriptorPathOfType` (type `'MOVE'`) + `MoveResObjectToHeapBottom` on each of the four children
2. `RunMovieFrameActions` — the main user-interaction loop for display-attach rows
3. Helpers: `FindMovieFrameIndexByName`, `ScheduleMovieTimedJumps`, `ShowMovieFrame`, `PretouchMovieFrameRange`

This is the primary mechanism for scene-to-scene transitions with interactive hotspot overlays (MOV playback with clickable regions).
