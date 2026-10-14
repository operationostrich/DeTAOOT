#ifndef DF_TOPIC_METHODPATH_H
#define DF_TOPIC_METHODPATH_H

#include <windows.h>
#include <string.h>
#include <ctype.h>

#ifndef CONCAT22
#define CONCAT22(h, l) \
    ((unsigned int)(((unsigned int)(h) & 0xFFFF) << 16) | (unsigned int)((unsigned short)(l)))
#endif

typedef unsigned char byte;
typedef unsigned int  uint;

#include "df_keywords.h"
#pragma pack(push, 1)
struct mpath8 { short typ; int mid; short tail; };
#pragma pack(pop)
#endif
