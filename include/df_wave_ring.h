/* df_wave_ring.h -- DF_FIX_WAVE_RING_LOCK's lock and macros (see df_fixes.h for why).
 * A header so every file with a ring-touching function sees the same lock:
 * as file-static definitions they could not be shared between files. */
#ifndef DF_WAVE_RING_H
#define DF_WAVE_RING_H

#if DF_FIX_WAVE_RING_LOCK
extern CRITICAL_SECTION g_waveRingLock;
extern int              g_waveRingLockReady;
void WaveRingLockInit(void);
void WaveRingLockDelete(void);
#define WAVE_RING_LOCK()   EnterCriticalSection(&g_waveRingLock)
#define WAVE_RING_UNLOCK() LeaveCriticalSection(&g_waveRingLock)
#else  /* original: no ring lock at all -- every site is bare */
#define WaveRingLockInit()   ((void)0)
#define WaveRingLockDelete() ((void)0)
#define WAVE_RING_LOCK()     ((void)0)
#define WAVE_RING_UNLOCK()   ((void)0)
#endif

#endif /* DF_WAVE_RING_H */
