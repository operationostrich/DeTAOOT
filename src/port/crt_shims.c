/*
 * crt_shims.c
 *
 * The C runtime functions the decoded game code calls by their LIBCMT names.
 * TI.EXE linked the Microsoft C runtime (LIBCMT.LIB) statically after the game
 * code, from __ftol at 0x0044e3a0 on; this port links MinGW's runtime instead,
 * so each name here forwards to the equivalent standard function. Nothing else
 * from that region is needed: the decoded LIBCMT internals that used to live in
 * crt_support.c were called by nothing in the game and were removed.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

int __cdecl _sprintf(char *buf, const char *fmt, ...)  /* 0x0044e590 */
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsprintf(buf, fmt, ap);
    va_end(ap);
    return r;
}

int __cdecl _sscanf(const char *str, const char *fmt, ...)  /* 0x0044e870 */
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsscanf(str, fmt, ap);
    va_end(ap);
    return r;
}

/* 0x0044e440 is LIBCMT's memcpy/memmove (one routine, Ghidra: FID_conflict), which
 * copies overlapping buffers correctly; MinGW's memcpy does not promise that. */
void * __cdecl _memcpy(void *dst, const void *src, size_t n)  /* 0x0044e440 */
{
    return memmove(dst, src, n);
}

int __cdecl __strcmpi(const char *a, const char *b)  /* 0x004591b0 */
{
    return _stricmp(a, b);
}

/* __ftol: MSVC float-to-long truncation helper (FPU stack -> EAX:EDX). */
long __cdecl __ftol(double f)  /* 0x0044e3a0 */
{
    return (long)f;
}
