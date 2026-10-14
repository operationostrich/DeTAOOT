/*
 * trace.c -- implementation of the DF_TRACE runtime log (see include/df_trace.h).
 *
 * Entirely empty unless DF_TRACE is defined at compile time.
 */

#include "df_types.h"

#ifdef DF_TRACE

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

int g_dfTraceDepth = 0;
int g_dfTraceRender = 0;

#define DF_TRACE_MAX_LINES_DEFAULT 3000000
static int s_maxLines = DF_TRACE_MAX_LINES_DEFAULT;
#define DF_TRACE_MAX_LINES s_maxLines

static FILE *s_fp;
static int   s_init;
static int   s_lines;
static char  s_prev[512];
static int   s_prevRepeat;
static DWORD s_t0;
static char  s_path[MAX_PATH];
static int   s_segment;   /* 0 = first file, incremented on each rotation */

__attribute__((no_instrument_function))
static void DfTraceOpen(void)
{
    const char *path;

    s_init = 1;

    /* Per-frame render-walk traces are opt-in: they emit a line per table
     * row per frame and exhaust DF_TRACE_MAX_LINES long before a session
     * reaches whatever is being investigated. */
    {
        const char *r = getenv("DF_TRACE_RENDER");
        g_dfTraceRender = (r != NULL && r[0] != '\0' && r[0] != '0');
    }

    {
        const char *cap = getenv("DF_TRACE_MAX_LINES");
        if (cap != NULL && atoi(cap) > 0)
            s_maxLines = atoi(cap);
    }

    path = getenv("DF_TRACE_LOG");
    if (path == NULL || path[0] == '\0')
        path = "df_trace.log";
    strncpy(s_path, path, sizeof(s_path) - 1);
    s_path[sizeof(s_path) - 1] = '\0';

    s_fp = fopen(path, "w");
    s_t0 = GetTickCount();
    if (s_fp)
    {
        fprintf(s_fp, "=== df_trace start (pid %lu, render traces %s) ===\n",
                (unsigned long)GetCurrentProcessId(),
                g_dfTraceRender ? "ON" : "off (set DF_TRACE_RENDER=1)");
        fflush(s_fp);
    }
}

/* Cap reached: move this file aside as "<path>.1" and start a fresh one, so
 * the newest lines are always in <path>. Only one generation is kept; the
 * previous .1 is overwritten. On any failure tracing simply stops, as the
 * old hard cap did. */
__attribute__((no_instrument_function))
static void DfTraceRotate(void)
{
    char prev[MAX_PATH + 4];

    _snprintf(prev, sizeof(prev) - 1, "%s.1", s_path);
    prev[sizeof(prev) - 1] = '\0';

    fprintf(s_fp, "=== df_trace line cap (%d) reached, rotating -- earlier "
                  "lines are now in %s ===\n", DF_TRACE_MAX_LINES, prev);
    fclose(s_fp);
    s_fp = NULL;

    remove(prev);
    if (rename(s_path, prev) != 0)
        return;

    s_fp = fopen(s_path, "w");
    if (s_fp == NULL)
        return;

    s_lines = 0;
    s_segment++;
    fprintf(s_fp, "=== df_trace continues (segment %d, pid %lu, %lu ms in; "
                  "earlier lines in %s) ===\n",
            s_segment, (unsigned long)GetCurrentProcessId(),
            (unsigned long)(GetTickCount() - s_t0), prev);
    fflush(s_fp);
}

/* Emit the pending repeat count for the previous line, if any. */
__attribute__((no_instrument_function))
static void DfTraceFlushRepeat(void)
{
    if (s_prevRepeat > 1)
        fprintf(s_fp, "        ... previous line repeated %d more times\n",
                s_prevRepeat - 1);
    s_prevRepeat = 0;
}

__attribute__((no_instrument_function))
void DfTracePrintf(const char *fmt, ...)
{
    char    line[512];
    int     n;
    va_list ap;

    if (!s_init)
        DfTraceOpen();
    if (s_fp == NULL)
        return;

    n = 0;
    if (g_dfTraceDepth > 0)
    {
        int pad = g_dfTraceDepth * 2;
        if (pad > 40) pad = 40;
        memset(line, ' ', (size_t)pad);
        n = pad;
    }

    va_start(ap, fmt);
    _vsnprintf(line + n, sizeof(line) - (size_t)n - 1, fmt, ap);
    va_end(ap);
    line[sizeof(line) - 1] = '\0';

    /* Collapse consecutive identical lines. */
    if (strcmp(line, s_prev) == 0)
    {
        s_prevRepeat++;
        return;
    }
    DfTraceFlushRepeat();

    strcpy(s_prev, line);
    s_prevRepeat = 1;
    s_lines++;

    fprintf(s_fp, "[%6lu] %s\n", (unsigned long)(GetTickCount() - s_t0), line);
    fflush(s_fp);

    if (s_lines >= DF_TRACE_MAX_LINES)
        DfTraceRotate();
}

/* Dump script bytecode entries: each is 4 shorts (8 bytes), printed as
 * "<index>:[a b c d]". Stops early on the 0xfb3 block terminator or a NUL
 * entry so a short arg block does not print a page of trailing garbage. */
__attribute__((no_instrument_function))
void DfTraceTokens(const char *label, const short *p, int maxEntries)
{
    char line[480];
    int  i, n = 0;

    if (p == NULL)
    {
        DfTracePrintf("%s <null>", label);
        return;
    }

    for (i = 0; i < maxEntries && n < (int)sizeof(line) - 40; i++)
    {
        n += _snprintf(line + n, sizeof(line) - (size_t)n - 1,
                       "%d:[%d %d %d %d] ", i,
                       p[i * 4], p[i * 4 + 1], p[i * 4 + 2], p[i * 4 + 3]);
        if (p[i * 4] == (short)0xfb3 || p[i * 4] == 0)
            break;
    }
    line[sizeof(line) - 1] = '\0';
    DfTracePrintf("%s %s", label, line);
}

/* -----------------------------------------------------------------------
 * Function-entry coverage (-finstrument-functions on the trace build).
 *
 * GCC calls __cyg_profile_func_enter at the top of every instrumented
 * function. Each distinct entry address is recorded once in an open-addressed
 * set and written to df_coverage.txt (next to $DF_TRACE_LOG). It is flushed
 * on normal shutdown, from FatalError before ExitProcess, and every
 * DF_COV_FLUSH_EVERY new entries: a SIGSEGV reaches none of the exit paths,
 * so without the periodic flush a crash run would leave nothing.
 * Resolve the addresses with nm to see which functions have never run.
 * --------------------------------------------------------------------- */
#define DF_COV_SLOTS        16384   /* power of two; ~1341 functions live here */
#define DF_COV_FLUSH_EVERY  2000
static void *s_cov[DF_COV_SLOTS];
static int   s_covCount;
static int   s_covOverflow;
static int   s_covSinceFlush;

__attribute__((no_instrument_function))
void DfCoverageWrite(void)
{
    char        path[MAX_PATH];
    const char *log;
    FILE       *fp;
    int         i;
    char       *slash;

    log = getenv("DF_TRACE_LOG");
    if (log == NULL || log[0] == '\0')
        log = "df_trace.log";
    strncpy(path, log, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    slash = strrchr(path, '\\');
    if (slash == NULL) slash = strrchr(path, '/');
    if (slash) slash[1] = '\0'; else path[0] = '\0';
    strncat(path, "df_coverage.txt", sizeof(path) - strlen(path) - 1);

    fp = fopen(path, "w");
    if (fp == NULL)
        return;
    /* The exe relocates at load (ASLR); record the base so the tool can
     * rebase these runtime addresses onto nm's link-time ones. */
    fprintf(fp, "# base %p\n", (void *)GetModuleHandleA(NULL));
    fprintf(fp, "# %d distinct function entry addresses%s\n", s_covCount,
            s_covOverflow ? " (TABLE OVERFLOWED -- raise DF_COV_SLOTS)" : "");
    for (i = 0; i < DF_COV_SLOTS; i++)
        if (s_cov[i] != NULL)
            fprintf(fp, "%p\n", s_cov[i]);
    fclose(fp);
    s_covSinceFlush = 0;
}

__attribute__((no_instrument_function))
void __cyg_profile_func_enter(void *fn, void *callsite)
{
    unsigned h = ((unsigned)(uintptr_t)fn >> 2) & (DF_COV_SLOTS - 1);
    unsigned n;
    (void)callsite;
    for (n = 0; n < DF_COV_SLOTS; n++)
    {
        void *cur = s_cov[h];
        if (cur == fn)
            return;
        if (cur == NULL)
        {
            s_cov[h] = fn;
            s_covCount++;
            if (++s_covSinceFlush >= DF_COV_FLUSH_EVERY)
                DfCoverageWrite();
            return;
        }
        h = (h + 1) & (DF_COV_SLOTS - 1);
    }
    s_covOverflow = 1;
}

__attribute__((no_instrument_function))
void __cyg_profile_func_exit(void *fn, void *callsite)
{
    (void)fn; (void)callsite;
}

__attribute__((no_instrument_function))
void DfTraceShutdown(void)
{
    DfCoverageWrite();
    if (s_fp)
    {
        DfTraceFlushRepeat();
        fprintf(s_fp, "=== df_trace end ===\n");
        fclose(s_fp);
        s_fp = NULL;
    }
}

#endif /* DF_TRACE */
