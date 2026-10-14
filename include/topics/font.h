#ifndef DF_TOPIC_FONT_H
#define DF_TOPIC_FONT_H

#include <windows.h>
#include <string.h>

/*
 * FileWriterCtx is a helper struct used by WriteToFileWithRetry / CloseContainerFile to
 * write a buffer to an open file handle in a thread-safe manner.
 * The address of local_36c (the first field) is passed as the context pointer.
 */
typedef struct {
    HANDLE           hFile;         /* open file handle */
    DWORD            reserved0;     /* initialised to 0 */
    CRITICAL_SECTION cs;            /* InitializeCriticalSection called before use */
    DWORD            reserved1;     /* initialised to 0 */
} FileWriterCtx;

#endif
