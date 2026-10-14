/* keywords_strings.c -- TI.EXE 0x004212a0..0x00422060: the script keywords, the method-path
 * tokenizer and the Pascal-string helpers. */

#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include "df_protos.h"
#include "topics/renderer.h"
#include "topics/methodpath.h"
#include "topics/strutil.h"
#include "topics/multimedia.h"

static undefined2 LookupScriptKeyword(byte *pName);

/* Turns every script keyword name into a Pascal string in place, once at
 * startup: the default list (index -1), then 'a'..'z'. The port called this
 * RegisterAllNodeClasses and walked the lists with an 8-byte stride; both
 * records are 6 bytes (df_keywords.h). */
void __stdcall ConvertKeywordTablesToPascal(void)  /* 0x004212a0 */
{
    short         iGroup;
    KeywordList   cur;
    KeywordEntry *pEntry;
    short         nEntries;

    for (iGroup = -1; iGroup < 0x1a; iGroup++) {
        if (iGroup < 0)
            cur = g_keywordDefault;
        else
            cur = g_keywordByLetter[iGroup];
        pEntry = cur.pList;
        for (nEntries = cur.count; nEntries > 0; nEntries--) {
            ConvertCStringToPascalInPlace(pEntry->pName);
            pEntry++;
        }
    }
}


/* Convert C string to Pascal format in-place (length byte + shifted body). */
void __cdecl ConvertCStringToPascalInPlace(char *pName)  /* 0x00421310 */
{
    char  c;
    short s;
    s = 0;
    c = pName[0];
    while (c != '\0') {
        s = (short)(s + 1);
        c = pName[s];                   /* movsx: the index is the signed short */
    }
    memmove(pName + 1, pName, (size_t)s);
    pName[0] = (char)(unsigned char)s;
}



/* Tokenizes a method-path source into 8-byte records {typ, mid, tail} in
 * *phTokens, with the quoted strings and unknown identifiers in a string pool
 * (InternPascalStringInPool, which reuses an identical string) appended after the
 * records; each pool offset is rebased to be relative to its record.
 * Returns 0x26 with *pErrOffset = the failing offset on a malformed token.
 *
 * Rewritten from Ghidra 2026-09-30: the port had its own pool, record and
 * Pascal-copy helpers (no string reuse, different growth), a bounds check
 * before a two-character operator the exe does not make, and copied the pool
 * only when non-empty. The exe copies it unconditionally, lock-balanced. */
short __cdecl TokenizeScriptSource(byte *pSource, int srcLen, int **phTokens, int *pErrOffset)  /* 0x00421340 */
{
    struct mpath8 T;        /* esp+0x10: the record being built */
    int      nTok;          /* +0x18 */
    int     *hPool;         /* +0x1c */
    int      tokCap;        /* +0x20 */
    int      poolUsed;      /* +0x24 */
    uint32_t poolCap;       /* +0x28 */
    byte     pasc[0x100];   /* +0x2c */
    byte    *p;
    byte    *pe;
    byte    *pStart;
    short    len;
    short    tabs;
    int      i;
    int      recBytes;
    int      delta;
    struct mpath8 *E;
    byte    *pSrc;

    poolUsed = 0;
    poolCap  = 0x200;
    hPool    = (int *)AllocResObject(0x200);
    nTok     = 0;
    tokCap   = 100;
    *phTokens = (int *)AllocResObject(800);
    p  = pSource;
    pe = srcLen + p;

    while (p < pe) {
        if (*p == 0x20) {
            while (p < pe && *p == 0x20)
                p++;
        } else if (*p == 0x22) {
            p++;
            pStart = p;
            while (p < pe && *p != 0x22 && *p != 0xd)
                p++;
            len = (short)(p - pStart);
            if (pe <= p || len > 0xff || *p == 0xd) {
                FreeResObject(*phTokens);
                FreeResObject(hPool);
                *pErrOffset = pStart - pSource;
                return 0x26;
            }
            CopyBytesToPascalString(pStart, len, pasc);
            T.typ  = 3;
            T.mid  = InternPascalStringInPool((uint32_t *)&hPool, &poolUsed, &poolCap, pasc);
            T.tail = 0;
            AppendEntryPairToBuffer(phTokens, &nTok, &tokCap, (uint32_t *)&T);
            p++;
        } else if (*p >= 0x30 && *p <= 0x39) {
            pStart = p;
            while (p < pe && *p >= 0x30 && *p <= 0x39)
                p++;
            len = (short)(p - pStart);
            if (len > 0xff) {
                FreeResObject(*phTokens);
                FreeResObject(hPool);
                *pErrOffset = pStart - pSource;
                return 0x26;
            }
            CopyBytesToPascalString(pStart, len, pasc);
            T.typ  = 4;
            T.mid  = (int)ParseLeadingIntFromPascalString(pasc);
            T.tail = 0;
            AppendEntryPairToBuffer(phTokens, &nTok, &tokCap, (uint32_t *)&T);
        } else if (*p == 0xd) {
            p++;
            T.mid  = 0;
            T.typ  = 6;
            T.tail = 0;
            tabs   = 0;
            while (p < pe && *p == 0x9) {
                tabs++;
                p++;
            }
            T.mid = tabs;
            AppendEntryPairToBuffer(phTokens, &nTok, &tokCap, (uint32_t *)&T);
        } else if (MPath_TwoCharLeadIn((char *)p) != 0) {
            /* "!=", ">=", "<=" */
            pasc[0] = 2;
            pasc[1] = *p;
            p += 2;
            pasc[2] = p[-1];
            T.typ = LookupScriptKeyword(pasc);
            if (T.typ == 1) {
                FreeResObject(*phTokens);
                FreeResObject(hPool);
                *pErrOffset = p - pSource;
                return 0x26;
            }
            T.mid  = 0;
            T.tail = 0;
            AppendEntryPairToBuffer(phTokens, &nTok, &tokCap, (uint32_t *)&T);
        } else if (MPath_IsIdentBreakChar(p) != 0) {
            /* one-character punctuation or operator */
            p++;
            pasc[0] = 1;
            pasc[1] = p[-1];
            T.typ = LookupScriptKeyword(pasc);
            if (T.typ == 1) {
                FreeResObject(*phTokens);
                FreeResObject(hPool);
                *pErrOffset = p - pSource;
                return 0x26;
            }
            T.mid  = 0;
            T.tail = 0;
            AppendEntryPairToBuffer(phTokens, &nTok, &tokCap, (uint32_t *)&T);
        } else {
            /* identifier: a keyword, or (typ 1) a name kept in the pool */
            pStart = p;
            p++;
            while (p < pe && MPath_IsIdentBreakChar(p) == 0)
                p++;
            len = (short)(p - pStart);
            if (len > 0xff) {
                FreeResObject(*phTokens);
                FreeResObject(hPool);
                *pErrOffset = pStart - pSource;
                return 0x26;
            }
            CopyBytesToPascalString(pStart, len, pasc);
            T.typ = LookupScriptKeyword(pasc);
            T.mid = 0;
            if (T.typ == 1) {
                T.typ = 5;
                T.mid = InternPascalStringInPool((uint32_t *)&hPool, &poolUsed, &poolCap, pasc);
            }
            T.tail = 0;
            AppendEntryPairToBuffer(phTokens, &nTok, &tokCap, (uint32_t *)&T);
        }
    }

    T.typ  = 0;
    T.tail = 0;
    T.mid  = 0;
    AppendEntryPairToBuffer(phTokens, &nTok, &tokCap, (uint32_t *)&T);

    /* rebase each pool offset to its record: the pool follows the records */
    recBytes = nTok * 8;
    E = (struct mpath8 *)GetResDataPtr(*phTokens);
    i = 0;
    if (nTok > 0) {
        delta = recBytes;
        do {
            T = *E;
            if (T.typ == 3)
                T.mid += delta;
            if (T.typ == 5)
                T.mid += delta;
            *E = T;
            delta -= 8;
            E++;
            i++;
        } while (i < nTok);
    }
    UnlockResObject(*phTokens);
    ResizeResObject(*phTokens, poolUsed + recBytes);
    i = GetResDataPtr(*phTokens);
    pSrc = (byte *)GetResDataPtr(hPool);
    memmove((void *)(i + recBytes), pSrc, poolUsed);   /* the exe CALLs the CRT copy (0x0044e440) */
    UnlockResObject(hPool);
    UnlockResObject(*phTokens);
    FreeResObject(hPool);
    return 0;
}



/* Writes count as the Pascal length byte into pDst[0], then copies count + 1 bytes from pSrc into
 * pDst[1..]; the extra byte past the declared length may carry a null terminator. */
void __cdecl CopyBytesToPascalString(undefined1 *pSrc, short count, undefined1 *pDst)  /* 0x004218B0 */
{
  undefined1 ch;

  /* the exe: inc the destination, store the count at -1, then `while (count >= 0)`
   * with the source re-read from its parameter slot after every byte store */
  *pDst++ = (char)count;
  if (count >= 0) {                 /* test dx,dx / jl, then dec / jns */
    do {
      ch = *pSrc;
      pSrc = pSrc + 1;
      *pDst = ch;
      pDst = pDst + 1;
      count = count + -1;
    } while (count >= 0);
  }
  return;
}


/* Returns 1 when the byte does NOT continue an identifier (charset 0-9 a-z A-Z), 0 while it does.
 *
 * Port note: the original code set only AX and Ghidra's decompile preserved the upper 16 bits of
 * the caller's pointer. Call sites check the full 32-bit result against 0, so the garbage upper
 * word broke tokenization ("boot" became four single-character tokens). */
short __cdecl MPath_IsIdentBreakChar(byte *pChar)  /* 0x004218E0 */
{
  byte c = *pChar;

  if (c >= '0' && c <= '9')             /* cmp 0x30 / jb, cmp 0x39 / ja */
    return 0;
  if (c >= 'a' && c <= 'z')
    return 0;
  if (c >= 'A' && c <= 'Z')
    return 0;
  return 1;
}


/* Low word 1 if p starts a two-char comparison operator: "!=", ">=", "<=". */
short MPath_TwoCharLeadIn(char *p)  /* 0x00421910 */
{
    if (p[1] == '=') {                  /* three separate `mov ax,1; ret` blocks in the exe */
        if (p[0] == '!')
            return 1;
        if (p[0] == '>')
            return 1;
        if (p[0] == '<')
            return 1;
    }
    return 0;
}


/* Walks a script entry table and assembles its text as Pascal strings in a newly allocated
 * res-object, one entry per token up to tokenLimit (the entry count); returns the count. Token
 * types: 0 end, 3 string, 4 uint, 5 value, 6 loop; any other is named through
 * LookupScriptKeywordName. */
int __cdecl FormatScriptTokensAsText(int *hTokens, int **phText, int tokenLimit) /* 0x00421940 */
{
    short    tokType;
    short   *pTok;
    int      charCount;
    int      bufLen;
    uint32_t bufCap;
    short    k;
    byte     tokenText[256];  /* local_100 */

    /* The lookups are written inline as the first argument: the exe pushes
     * the other three before calling UpdatePascalLength. */
    bufCap = 0x2000;
    charCount  = 0;
    bufLen = 0;
    *phText = AllocResObject(0x2000);
    pTok = (short *)GetResDataPtr(hTokens);
    do {
        while (1) {
            tokType = *pTok;
            if (tokType == 0)
                goto done;   /* the exit is written last: same code (layout follows the
                              * flow graph), but " 	" is then named before "  ", and a
                              * file's literals land in reverse order of first mention */
            if (tokType != 6) break;
            if (0 < tokenLimit) charCount++;
            AppendPascalStringToBuffer((byte *)UpdatePascalLength(&kStrCR), phText, &bufLen, &bufCap);
            k = 0;
            if (*(int *)(pTok + 1) <= 0)
                goto next;
            do {
                if (0 < tokenLimit) charCount++;
                AppendPascalStringToBuffer((byte *)UpdatePascalLength(&kStrTab), phText, &bufLen, &bufCap);
                k++;
            } while ((int)k < *(int *)(pTok + 1));
            tokenLimit--; pTok += 4;
        }
        if (tokType == 3) {
            if (0 < tokenLimit) charCount++;
            AppendPascalStringToBuffer((byte *)UpdatePascalLength(&kStrQuote), phText, &bufLen, &bufCap);
            CopyPascalStringFromInlineDescriptor(pTok, tokenText);
            if (0 < tokenLimit) charCount += (uint32_t)tokenText[0];
            AppendPascalStringToBuffer(tokenText, phText, &bufLen, &bufCap);
            if (0 < tokenLimit) charCount++;
            AppendPascalStringToBuffer((byte *)UpdatePascalLength(&kStrQuote), phText, &bufLen, &bufCap);
            if (IsScriptDelimiterOpcode(pTok[4]) != 0) goto next;
            if (0 < tokenLimit) charCount++;
            AppendPascalStringToBuffer((byte *)UpdatePascalLength(&kStrSpace), phText, &bufLen, &bufCap);
            tokenLimit--; pTok += 4;
        } else if (tokType == 5) {
            CopyPascalStringFromInlineDescriptor(pTok, tokenText);
            if (0 < tokenLimit) charCount += (uint32_t)tokenText[0];
            AppendPascalStringToBuffer(tokenText, phText, &bufLen, &bufCap);
            if (IsScriptDelimiterOpcode(pTok[4]) != 0) goto next;
            if (0 < tokenLimit) charCount++;
            AppendPascalStringToBuffer((byte *)UpdatePascalLength(&kStrSpace), phText, &bufLen, &bufCap);
            tokenLimit--; pTok += 4;
        } else if (tokType == 4) {
            FormatUintAsPascalDecimalString(*(uint32_t *)(pTok + 1), (char *)tokenText);
            if (0 < tokenLimit) charCount += (uint32_t)tokenText[0];
            AppendPascalStringToBuffer(tokenText, phText, &bufLen, &bufCap);
            if (IsScriptDelimiterOpcode(pTok[4]) != 0) goto next;
            if (0 < tokenLimit) charCount++;
            AppendPascalStringToBuffer((byte *)UpdatePascalLength(&kStrSpace), phText, &bufLen, &bufCap);
            tokenLimit--; pTok += 4;
        } else {
            LookupScriptKeywordName(tokType, tokenText);
            if (0 < tokenLimit) charCount += (uint32_t)tokenText[0];
            AppendPascalStringToBuffer(tokenText, phText, &bufLen, &bufCap);
            if (IsScriptDelimiterOpcode(*pTok) == 0) {
                if (IsScriptDelimiterOpcode(pTok[4]) == 0) {
                    if (0 < tokenLimit) charCount++;
                    AppendPascalStringToBuffer((byte *)UpdatePascalLength(&kStrSpace), phText, &bufLen, &bufCap);
                }
            }
next:       /* the exe's shared tail (0x00421c53) */
            tokenLimit--; pTok += 4;
        }
    } while (1);
done:
    UnlockResObject(hTokens);
    if (bufLen == 0)
        AppendPascalStringToBuffer((byte *)UpdatePascalLength(&kStrSpace), phText, &bufLen, &bufCap);
    ResizeResObject(*phText, bufLen);
    return charCount;
}



/* True for script structure delimiters: 0xfb2 block open, 0xfb3 close, 0xfb4 separator. */
short __cdecl IsScriptDelimiterOpcode(short opcode) /* 0x00421cc0 */
{
    if (opcode == (short)0xfb4)
        return 1;
    if (opcode == (short)0xfb2)
        return 1;
    if (opcode == (short)0xfb3)
        return 1;
    return 0;
}


/* Returns the keyword id of the Pascal token, or 1 when it is not a keyword.
 * A token starting with a letter is looked up in that letter's list, anything
 * else in the default list (punctuation and operators); both from
 * src/keyword_tables.c. The comparison ignores case (kCaseFoldTable folds).
 * The port had this as "LookupPropertySendDispatch", unused, reading five zero
 * globals, while the tokenizer called a static stand-in that knew only the
 * punctuation. */
static undefined2 LookupScriptKeyword(byte *pName)  /* 0x00421CF0 */
{
  KeywordList   cur;
  byte          c;
  short         n;
  KeywordEntry *pEnt;

  c = kCaseFoldTable[pName[1]];
  if (c >= 'a' && c <= 'z')
    cur = g_keywordByLetter[c - 'a'];
  else
    cur = g_keywordDefault;
  n = cur.count;
  pEnt = cur.pList;
  for (; n > 0; n--, pEnt++)
    if (PascalStringsEqualNoCase((byte *)pEnt->pName, pName) != 0)
      return (undefined2)pEnt->id;
  return 1;
}



/* Keyword id -> name: searches the default keyword list, then the letter lists
 * from 'z' down to 'a' (src/keyword_tables.c), and copies the matching Pascal
 * name into pNameOut; "?" and -1 when no keyword has that id (the port returned 0).
 * The port read the lists
 * through three unrelated zero globals, so it always answered "?". */
short __cdecl LookupScriptKeywordName(short keywordId, byte *pNameOut) /* 0x00421D90 */
{
    short         i, n;
    KeywordEntry *pEnt;
    KeywordList   cur;

    pEnt = g_keywordDefault.pList;
    for (i = g_keywordDefault.count; i > 0; i--) {
        if (pEnt->id == keywordId) {
            CopyPascalString((byte *)pEnt->pName, pNameOut);
            return 0;
        }
        pEnt++;
    }
    for (i = 0x19; i >= 0; i--) {
        cur = g_keywordByLetter[i];
        pEnt = cur.pList;
        for (n = cur.count; n > 0; n--) {
            if (pEnt->id == keywordId) {
                CopyPascalString((byte *)pEnt->pName, pNameOut);
                return 0;
            }
            pEnt++;
        }
    }
    *pNameOut = 1;
    pNameOut[1] = 0x3f;
    return -1;
}


/* Interns a Pascal string in the string pool (*phPool, *pUsed bytes used, capacity *pCap): returns
 * the offset of an identical string already in the pool, otherwise grows the res-object (by the
 * string's length plus 0x200 when it is full) and copies the string in at the end. Returns the
 * string's offset. */
int __cdecl InternPascalStringInPool(uint32_t *phPool, int *pUsed, uint32_t *pCap,
                               byte *pStr)  /* 0x00421E50 */
{
  int used;
  short len;
  int off;
  int base;

  used = *pUsed;
  off = 0;
  len = *pStr + 1;
  base = GetResDataPtr((int *)*phPool);
  if (used >= len) do {
    if (ComparePascalStringsEqual((byte *)(base + off), pStr)) {
      UnlockResObject((int *)*phPool);
      return off;
    }
    off += *(byte *)(base + off) + 1;
  } while (off + len <= used);
  UnlockResObject((int *)*phPool);
  *pUsed += len;
  if ((int)*pCap <= *pUsed) {
    *pCap = *pCap + len + 0x200;
    ResizeResObject((int *)*phPool, *pCap);
  }
  CopyPascalString(pStr, (byte *)(GetResDataPtr((int *)*phPool) + used));
  UnlockResObject((int *)*phPool);
  return used;
}


/* Case-sensitive Pascal string equality: compares length bytes then the
 * character data.  Low word 1 if equal, 0 otherwise. */
short __cdecl ComparePascalStringsEqual(byte *pA, byte *pB)  /* 0x00421F40 */
{
  byte *p;
  short len;

  len = *pA++;
  p = pB + 1;
  if (*pB != len) return 0;
  while (len-- != 0) {
    if (*pA++ != *p++) return 0;
  }
  return 1;
}


/* Appends an 8-byte entry to a dynamic buffer (*phBuf, *pCount entries used, capacity *pCap),
 * growing it by 100 entries when it is full. */
void __cdecl AppendEntryPairToBuffer(int **phBuf, int *pCount, int *pCap, uint32_t *pEntry)  /* 0x00421f90 */
{
    int       iRaw;

    if (*pCount >= *pCap) {
        *pCap += 100;
        ResizeResObject(*phBuf, (unsigned int)(*pCap * 8));
    }
    iRaw  = GetResDataPtr(*phBuf);
    /* the 8-byte entry is copied as one struct (both dwords loaded, then stored) */
    *(SceneViewport *)(iRaw + *pCount * 8) = *(SceneViewport *)pEntry;
    UnlockResObject(*phBuf);
    *pCount += 1;
}


/* Appends the body of a Pascal string to a resizable byte buffer (*phBuf, *pLen bytes used,
 * capacity *pCap), growing it by 0x1000 when it is full. */
void __cdecl AppendPascalStringToBuffer(byte *pPascal, int **phBuf, int *pLen, uint32_t *pCap)  /* 0x00421ff0 */
{
    int      pData;         /* the data pointer */
    int      oldLen;

    oldLen = *pLen;
    *pLen += *pPascal;
    if (*pLen >= (int)*pCap) {
        *pCap += 0x1000;
        ResizeResObject(*phBuf, *pCap);
    }
    pData = GetResDataPtr(*phBuf);
    memmove((void *)(pData + oldLen), pPascal + 1, (size_t)*pPascal);
    UnlockResObject(*phBuf);
}


/* Inline DreamFactory descriptor layout: Pascal body begins at base + *(int *)(base + 2).
 * Copies that Pascal string to pDst via CopyPascalString. */
void __cdecl CopyPascalStringFromInlineDescriptor(void *pInlineDesc, byte *pDst) /* 0x00422060 */
{
    byte *base = (byte *)pInlineDesc;
    byte *pSrc = base + *(int *)(base + 2);   /* computed before the pushes in the exe */

    CopyPascalString(pSrc, pDst);
}

