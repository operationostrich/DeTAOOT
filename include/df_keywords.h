/*
 * df_keywords.h -- the script language's reserved-keyword tables (src/keyword_tables.c).
 *
 * Both records are six bytes in TI.EXE ({pointer, short}, no padding): the readers step
 * them by 6, so the packing is load-bearing.
 */
#ifndef DF_KEYWORDS_H
#define DF_KEYWORDS_H

#pragma pack(push, 2)
typedef struct {
    char  *pName;   /* C string in the image, Pascal after ConvertKeywordTablesToPascal */
    short  id;      /* keyword id / script opcode */
} KeywordEntry;

typedef struct {
    KeywordEntry *pList;
    short         count;
} KeywordList;
#pragma pack(pop)

/* the readers step both records by 6 */
typedef char df_keyword_record_size_check[(sizeof(KeywordEntry) == 6 && sizeof(KeywordList) == 6) ? 1 : -1];

extern KeywordList g_keywordDefault;        /* 0x0045d5e0 -- punctuation and operators */
extern KeywordList g_keywordByLetter[26];   /* 0x0045d5e8 -- one list per initial 'a'..'z' */

#endif /* DF_KEYWORDS_H */
