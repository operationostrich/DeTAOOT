/*
 * df_trace.h -- opt-in runtime tracing for live debugging.
 *
 * Force-included into every translation unit by the Makefile (see DEFS), the
 * same way df_types.h / df_globals.h are, so trace points can be dropped into
 * any source file without adding an #include.
 *
 * Compiled out entirely unless DF_TRACE is defined, so trace points may stay
 * in the source permanently:
 *
 *     mingw32-make trace        -> build/titanic_trace.exe (DF_TRACE on)
 *     mingw32-make debug        -> build/titanic_dbg.exe   (DF_TRACE off)
 *
 * The log file is $DF_TRACE_LOG, or "df_trace.log" in the process's cwd.
 * Consecutive identical lines are collapsed into a repeat count -- the script
 * interpreter's idle loop otherwise produces megabytes per minute.
 *
 * The log ROTATES at DF_TRACE_MAX_LINES (src/trace.c): the newest lines are
 * always in $DF_TRACE_LOG, the previous generation in "<log>.1". Anything
 * older is gone, so a very long session still loses its beginning.
 *
 * PER-FRAME RENDER TRACES ARE OFF BY DEFAULT. The scene walk emits a line per
 * table row per frame; left on, those alone took roughly half the line cap and
 * twice truncated a session before it reached the thing being debugged. They
 * use DF_TRACE_RENDER and turn on with DF_TRACE_RENDER=1 in the environment.
 * Use them when chasing what draws; leave them off for script behaviour.
 */
#ifndef DF_TRACE_H
#define DF_TRACE_H

#ifdef DF_TRACE

/* Trace-only source blocks test this, never `#ifdef DF_TRACE`: once this header has
 * defined the DF_TRACE() macro, `#ifdef DF_TRACE` is true in every build (and in the
 * byte-match harness, which defines the macro as a no-op). */
#define DF_TRACE_ENABLED 1

void DfTracePrintf(const char *fmt, ...);
void DfTraceTokens(const char *label, const short *p, int maxEntries);
void DfTraceShutdown(void);
/* Flush the function-entry coverage set to df_coverage.txt (trace build). */
void DfCoverageWrite(void);
extern int g_dfTraceDepth;
/* Non-zero when DF_TRACE_RENDER=1 was set in the environment. */
extern int g_dfTraceRender;

/* Emit one trace line (printf-style), indented by the current nesting depth. */
#define DF_TRACE(...)     DfTracePrintf(__VA_ARGS__)
/* Dump up to maxEntries script bytecode entries (4 shorts each) from p. */
#define DF_TRACE_TOKENS(label, p, n) DfTraceTokens((label), (const short *)(p), (n))
/* Per-frame render-walk trace: suppressed unless DF_TRACE_RENDER=1. */
#define DF_TRACE_RENDER(...) \
    do { if (g_dfTraceRender) DfTracePrintf(__VA_ARGS__); } while (0)
#define DF_TRACE_ENTER()  ((void)(++g_dfTraceDepth))
#define DF_TRACE_LEAVE()  ((void)(--g_dfTraceDepth))
#define DF_COVERAGE_FLUSH() DfCoverageWrite()

#else

#define DF_TRACE_ENABLED 0
#define DF_TRACE(...)     ((void)0)
#define DF_TRACE_TOKENS(label, p, n) ((void)0)
#define DF_TRACE_RENDER(...) ((void)0)
#define DF_TRACE_ENTER()  ((void)0)
#define DF_TRACE_LEAVE()  ((void)0)
#define DF_COVERAGE_FLUSH() ((void)0)

#endif /* DF_TRACE */

#endif /* DF_TRACE_H */
