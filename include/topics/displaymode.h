#ifndef DF_TOPIC_DISPLAYMODE_H
#define DF_TOPIC_DISPLAYMODE_H

#include <windows.h>
#include <string.h>

typedef BOOL (WINAPI *PFNENUMDS)(LPCSTR, DWORD, void *);
typedef LONG (WINAPI *PFNCHANGEDS)(void *, DWORD);
typedef BOOL (WINAPI *PFNEXITWIN)(UINT, DWORD);

#endif
