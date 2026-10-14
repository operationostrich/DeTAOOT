/* df_fixes.h -- registry of every DELIBERATE difference from TI.EXE.
 *
 * Force-included everywhere (see Makefile DEFS). Each DF_FIX_* switch guards
 * code that knowingly departs from the original binary because the original
 * behaviour is broken on a modern OS. With a switch at 0 the guarded site
 * compiles to the decoded original.
 *
 *   #if DF_FIX_NAME
 *       ...port behaviour...
 *   #else
 *       ...original lines, when the fix replaced rather than added...
 *   #endif
 *
 * Rules (an audit script once checked them; keep to them):
 *   - every switch used in src/ is defined here, and every switch here is used;
 *   - always `#if DF_FIX_X`, never `#ifdef` -- a switch set to 0 is still
 *     defined. -Wundef turns a misspelt name into a warning instead of a
 *     silently disabled fix;
 *   - a function with no original address lives inside a DF_FIX block.
 *
 * One fix off:      mingw32-make debug EXTRA_DEFS=-DDF_FIX_NAME=0
 * All fixes off:    mingw32-make nofix   (compile check only; the result is
 *                   not expected to run -- several fixes exist because the
 *                   original hangs or deadlocks on a modern desktop)
 *
 * This is also the seam for a future byte-matching build: that configuration
 * is simply DF_FIXES_OFF under the original compiler (VC++ 4.2).
 *
 * Decode bugs do NOT belong here. If the port differs from the binary by
 * accident, fix the decode.
 */
#ifndef DF_FIXES_H
#define DF_FIXES_H

#ifdef DF_FIXES_OFF
#define DF_FIX_DEFAULT 0
#else
#define DF_FIX_DEFAULT 1
#endif

/* 0x0044BD60 MovieFadeIn / 0x0044BCE0 ...Reverse.
 * The original fades by AnimatePalette alone, which recoloured every pixel on
 * an 8bpp screen. On true-colour it changes nothing already drawn, so the
 * port re-blits the whole scene window on each fade step. */
#ifndef DF_FIX_CLUT_FADE_REBLIT
#define DF_FIX_CLUT_FADE_REBLIT DF_FIX_DEFAULT
#endif

/* 0x0041bb40 AppInit. Port-added "-skipdisplaycheck" flag: skips
 * CheckDisplayColourDepth, whose modal box blocks every unattended run on a
 * desktop that is never 8bpp. The real TI.EXE gets past it via a Windows
 * compatibility shim (DWM8And16BitMitigation) a fresh build does not receive.
 * Inert unless the flag is passed. */
#ifndef DF_FIX_SKIP_DISPLAY_CHECK
#define DF_FIX_SKIP_DISPLAY_CHECK DF_FIX_DEFAULT
#endif

/* 0x0041b8c0 RestoreSavedWindowPositions (split out of RestoreDisplayMode,
 * 0x0041b880, which tail-jumps into it). The original MoveWindow()s every top-level
 * window on the desktop; today that blocks forever on any window whose thread
 * is not pumping messages. Restore only this process's own windows. */
#ifndef DF_FIX_OWN_WINDOWS_ONLY
#define DF_FIX_OWN_WINDOWS_ONLY DF_FIX_DEFAULT
#endif

/* 0x0041b6e0 ManageSystemPalette(0). The original restores the 25 system
 * colours unconditionally. On a true-colour display the acquire branch never
 * saves them (GetSystemPaletteUse returns SYSPAL_ERROR), so the restore wrote
 * 25 zero COLORREFs into the user's live theme. Restore only what was saved. */
#ifndef DF_FIX_SYSCOLOR_RESTORE_GUARD
#define DF_FIX_SYSCOLOR_RESTORE_GUARD DF_FIX_DEFAULT
#endif

/* 0x0041deb0 FreeDisplayLibrary. The source reverts the display mode here when
 * gIsDisplayModeChanged says it was changed, and the March 1997 disc build still does, but
 * our GOG TI.EXE has the JZ at 0x0041dec1 hex-patched to JMP (74 -> eb), so the
 * call to RestoreDisplayMode is skipped. Windows 95 undid a dynamic
 * ChangeDisplaySettings when the process ended; modern Windows keeps it.
 * The match build compiles to the UNPATCHED bytes and applies the patch to a
 * copy after linking (match/gog_patches.py),
 * so both arms of this switch are the source's call: the port at 1 and the
 * original source agree, and only GOG's post-build patch differs. */
#ifndef DF_FIX_REVERT_DISPLAY_MODE
#define DF_FIX_REVERT_DISPLAY_MODE DF_FIX_DEFAULT
#endif

/* 0x00432e20 PromptUntilDescriptorVolumePresent. The source puts up the "insert
 * the CD" prompt loop (and ejects the drive) when the descriptor's volume is
 * absent; GOG's TI.EXE NOPs the jz into it at 0x00432e59 (74 1a -> 90 90), so
 * an absent volume counts as present. The port runs from an installed copy
 * with no disc and keeps GOG's behaviour; at 0 the source's loop compiles (the
 * match build), and link_build.py re-applies GOG's patch after linking. */
#ifndef DF_FIX_GOG_NO_CD_CHECK
#define DF_FIX_GOG_NO_CD_CHECK DF_FIX_DEFAULT
#endif

/* 0x00435700 FadeCLUTToBlack / 0x00435750 ...Interpolation.
 * The source paces each palette-fade step on the 60 Hz tick: after
 * InterpolateCLUT it spins on GetTick60Hz until the step's deadline passes.
 * GOG's TI.EXE overwrites that spin (13 bytes at 0x00435732 and 0x00435782)
 * with `PresentBackBufferRect(gClipRect)`: no wait, and a call that
 * re-applies the channel-B hotspot scroll state each step. The intent is not
 * documented;, so this keeps what the port has always run. At 0 the source's
 * spin compiles (the match build; the exe's saved-EBP clobber by the patch's
 * push is not reproduced), and link_build.py re-applies GOG's bytes. */
#ifndef DF_FIX_GOG_CLUT_FADE_NO_WAIT
#define DF_FIX_GOG_CLUT_FADE_NO_WAIT DF_FIX_DEFAULT
#endif

/* 0x00435cf0 DestroyChannelViewport + every walk of the viewport table
 * gViewportTable. The original frees a viewport and leaves its table slot
 * dangling (verified: no function in TI.EXE ever clears a slot), then keeps
 * dereferencing it -- survivable in 1996 because freed blocks stayed mapped
 * inside the engine's own pool. The port clears the slot and probes each slot
 * before use. One switch for both halves: clearing to NULL without the probes
 * would crash.
 * NOTE: display_ui.c (the window procedures) records a non-NULL dangling slot from some OTHER teardown
 * path that was never root-caused. This guard may be hiding a decode bug;
 * building with this switch at 0 is the way to go looking for it. */
#ifndef DF_FIX_VIEWPORT_SLOT_GUARD
#define DF_FIX_VIEWPORT_SLOT_GUARD DF_FIX_DEFAULT
#endif
#if DF_FIX_VIEWPORT_SLOT_GUARD
#define DF_VIEWPORT_SLOT_OK(p, n) IsPointerReadable((p), (n))   /* display_ui.c */
#else
#define DF_VIEWPORT_SLOT_OK(p, n) 1
#endif

/* 0x00406ed0 WaveOutCallback, 0x00406f50 DequeueWaveOutBuffer,
 * 0x00406fe0 SubmitWaveOutBuffer, 0x004011a0 OpenSoundDevice. Win9x winmm
 * ran CALLBACK_FUNCTION completions on the caller's thread, so the original has no lock around the
 * waveOut ring counters. Modern winmm uses a worker thread, which races them.
 * The lock is a dedicated one, not the mixer's: see sound.c. */
#ifndef DF_FIX_WAVE_RING_LOCK
#define DF_FIX_WAVE_RING_LOCK DF_FIX_DEFAULT
#endif

/* 0x0041b480 HandleActivateAppMessage, reactivation branch. The original
 * repaints after an alt-tab restore only through the type-6 paint event,
 * which the main loop alone dequeues; inside a script loop that owns the
 * thread (forceupdate every frame) nothing repaints but the dirty rows, and
 * the window stays black around them. With this on, the restore also raises
 * the scene redraw level to 2 (0x00439320), so the next tick -- main loop or
 * script loop -- merges the full viewport and repaints everything. Whether
 * the original showed the same blackout is untested; see port-bug-classes
 * section 8. Not a decode bug: every function on the path matches TI.EXE. */
#ifndef DF_FIX_REPAINT_ON_ACTIVATE
#define DF_FIX_REPAINT_ON_ACTIVATE DF_FIX_DEFAULT
#endif

/* 0x004248e0 StopSoundChannels. Every call ends in UnlockSoundMixer(1),
 * which flushes the wave device (waveOutReset in 0x00406fe0) and re-mixes
 * from the play cursor. Win9x's reset was near-free; modern winmm blocks
 * ~9 ms (measured: 171 of 207 resets) and plays nothing meanwhile. The
 * radio tuner script calls halttheme on every drag tick (~17 ms), so the
 * static came out as ~8 ms bursts at ~57 Hz -- a high-pitched buzz. With
 * the switch on, a stop whose channels are all already idle returns before
 * the flush; stopping a channel that is actually playing is unchanged. */
#ifndef DF_FIX_SKIP_IDLE_CHANNEL_STOP
#define DF_FIX_SKIP_IDLE_CHANNEL_STOP DF_FIX_DEFAULT
#endif

/* 0x00435880 ParseLeadingIntFromPascalString. The exe copies the Pascal body
 * into a 20-byte stack buffer with no length check; a script value longer
 * than 19 characters overruns the frame. With the switch on the buffer is 32
 * bytes and the copy is clamped to 31. No script in the extracted set comes
 * near either limit, so this is belt and braces, not a fix for a seen crash. */
#ifndef DF_FIX_PARSE_INT_BUFFER
#define DF_FIX_PARSE_INT_BUFFER DF_FIX_DEFAULT
#endif

/* 0x00423c60 ProbeContainerFile (existence probe: original share mode 0),
 * 0x004239b0 OpenContainerFile (read-only fallback: original share mode 1).
 * Both relaxed to FILE_SHARE_READ|FILE_SHARE_WRITE so the probe does not
 * collide with a container handle that is still open -- on a script `reboot`
 * (opcode 12106) and with a second instance of the game during development.
 * SUSPECT, not a proven modern-OS difference: Windows 95 enforced share
 * modes too, and an exclusive probe against the original's own read/write
 * handle would fail there just the same (-> FatalError 0x10c0). So the
 * original must have CLOSED the bootfile container before BeginGameSession
 * re-opens it. EndGameSession matches the binary call for call, which leaves the
 * reference counting under DestroyRenderNode: if the port leaks that
 * handle, this switch is hiding a decode bug. Build with it at 0 and
 * trigger a `reboot` to find out. */
#ifndef DF_FIX_CONTAINER_SHARE_MODES
#define DF_FIX_CONTAINER_SHARE_MODES DF_FIX_DEFAULT
#endif

/* 0x004316e0 RunGameLoop. The exe passes local_82, a stack slot this function
 * never writes, into QdSetPort and the hotspot paths; stale stack contents
 * happened to be harmless there. On the port an event reaching one of those
 * branches on a fresh frame handed garbage to QdSetPortWindow and crashed. With
 * the switch on the slot is seeded from gThePortDesc each frame. The only
 * difference from the original left in the function. */
#ifndef DF_FIX_GAMELOOP_SEED_SCENESTATE
#define DF_FIX_GAMELOOP_SEED_SCENESTATE DF_FIX_DEFAULT
#endif

/* 0x0042a470 DispatchBootMessageString. The exe builds its MethodCallFrame on
 * the stack and fills only the fields it needs; the rest are whatever the stack
 * held. The port zeroes the frame first. No reader of an unset field is known
 * (LatchFailedMethodFrame reads the status at +0x18, which EvalMethodCall writes),
 * so this is belt and braces; the zeroing is the function's only byte
 * difference. */
#ifndef DF_FIX_ZERO_INTERACTION_FRAME
#define DF_FIX_ZERO_INTERACTION_FRAME DF_FIX_DEFAULT
#endif

/* 0x0041bd4e AppInit, 0x0041eaa0 PumpMessages (after a pause). The exe runs at
 * HIGH_PRIORITY_CLASS; on a modern desktop that starves other processes while
 * the game runs. ABOVE_NORMAL instead. */
#ifndef DF_FIX_ABOVE_NORMAL_PRIORITY
#define DF_FIX_ABOVE_NORMAL_PRIORITY DF_FIX_DEFAULT
#endif

/* 0x0041c670 RevertDisplayMode (AppInit, when CheckDisplayColourDepth returns 2).
 * The exe applies the chosen display mode WITH CDS_UPDATEREGISTRY -- a
 * permanent change to the user's desktop -- and offers EWX_REBOOT on
 * DISP_CHANGE_RESTART. The port restores the default mode (NULL, 0) and offers
 * a log-off instead. */
#ifndef DF_FIX_REVERT_TO_DEFAULT_MODE
#define DF_FIX_REVERT_TO_DEFAULT_MODE DF_FIX_DEFAULT
#endif

/* 0x004019c0 QueueWaveFormatOnSoundChannel. On an empty deferred list the exe
 * never sets the head: it stores the new node through its tail local, which on
 * that path is an unwritten stack slot (0x00401a39) -- a write through whatever
 * the stack held, and the entry is lost. The port sets gWaveBufferFormatQueue[ch]. */
#ifndef DF_FIX_SOUND_TEMPLATE_QUEUE_HEAD
#define DF_FIX_SOUND_TEMPLATE_QUEUE_HEAD DF_FIX_DEFAULT
#endif

/* 0x00422310 BootstrapContainerFile. The exe's header buffer is 0x400 bytes on
 * the stack with only 0x200 of them set, and it writes (chunks+2)*0x200 >=
 * 0x600 bytes from it: stack contents go into the new file and the read runs
 * past the frame. The port writes a zero-filled buffer of the full size. */
#ifndef DF_FIX_BOOTSTRAP_HEADER_BUFFER
#define DF_FIX_BOOTSTRAP_HEADER_BUFFER DF_FIX_DEFAULT
#endif

/* The 168 Pascal-string constants ("\0right", "\0Post Script", ...; kStrRight on).
 * In the source they were string literals: TI.EXE's .data holds them among each unit's
 * other literals, 4-aligned, one pooled copy per string (/Gf), and the engine writes
 * their length byte in place (UpdatePascalLength) -- legal in 1996, when VC++ put
 * literals in writable memory. GCC makes literals read-only, so the port defines each as
 * a writable array (src/data/globals.c). With the switch at 0
 * every DAT_ name is a macro for its literal again. */
#ifndef DF_FIX_WRITABLE_PASCAL_LITERALS
#define DF_FIX_WRITABLE_PASCAL_LITERALS DF_FIX_DEFAULT
#endif

/* WriteSaveGameFile (0x004139a0), the save-image writer, builds
 * the image in a 0x1430-byte stack block that the exe only probes (__chkstk) and never
 * clears: the bytes between the fields it writes are stale stack, so two saves of the
 * same state differ on disk. The port zeroes the block first. Behaviour is otherwise identical --
 * the loader reads only the written fields. */
#ifndef DF_FIX_SAVE_SCRATCH_ZEROED
#define DF_FIX_SAVE_SCRATCH_ZEROED DF_FIX_DEFAULT
#endif

/* AppInit (0x0041bb40) copies the command line into a 512-byte stack buffer with an
 * inline strcpy and no length check, then _strlwr; Win95 capped a command line near
 * 127 characters, Windows today allows 32767, so a long one overruns the frame. The
 * port bounds the copy (CopyLowerCmdLine) and widens the 64-byte title scratch
 * (local_240) to 512. */
#ifndef DF_FIX_CMDLINE_BOUNDED_COPY
#define DF_FIX_CMDLINE_BOUNDED_COPY DF_FIX_DEFAULT
#endif

#endif /* DF_FIXES_H */
