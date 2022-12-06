#pragma once
#define UNICODE
#include <WinDef.h>
#include <winnt.h>
#include <heapapi.h>

#define HT_MAX_STR 64

typedef struct HT_TableEntry HT_TableEntry;
struct HT_TableEntry {
	DWORD crc32;
	TCHAR value[HT_MAX_STR];
	HT_TableEntry *next;
};

typedef struct HT_Table {
	HT_TableEntry *first;
	HT_TableEntry *last;
} HT_Table;

DWORD CalcCRC32(const TCHAR *str);

const TCHAR* HT_Find_crc32(HT_Table *ht, DWORD crc32);

// Returns pointer to mapped string of reference string, or NULL if not found.
const TCHAR* HT_Find(HT_Table *ht, const TCHAR *reference_string);

// Does not check if crc32 exists before adding, check first with HT_Find_crc32.
BOOL HT_New(HT_Table *ht, DWORD crc32, const TCHAR *mapped_string);

// Searches for crc32 of reference string, and either replaces or creates entry with mapped string.
BOOL HT_Set(HT_Table *ht, const TCHAR *reference_string, const TCHAR *mapped_string, DWORD *crc32_out);

BOOL HT_Delete(HT_Table *ht, const TCHAR *reference_string);

// Must be called when done with HT_Table if any calls to HT_New or HT_Set were made.
void HT_Free(HT_Table *ht);

void HT_Display(HT_Table *ht);
