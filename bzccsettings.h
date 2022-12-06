#pragma once
#define UNICODE
#include <windows.h>
#include <stdio.h>

#define REG_KEY_Setting TEXT("Software\\BZCC Scanner")
#define REG_KEY_NICKS TEXT("Software\\BZCC Scanner\\Known Nicks")

// Returns value of registry key on success, otherwise returns default_value.
DWORD LoadSettingDWORD(const TCHAR *value_name, DWORD default_value);

// Returns TRUE on success, otherwise FALSE.
BOOL SaveSettingDWORD(const TCHAR *value_name, DWORD value);

// Returns amount of characters written to output buffer 'out' if successful, otherwise returns 0.
// out_len is the maximum size of the out buffer.
// 0-length strings always return 0.
DWORD LoadSettingString(const TCHAR *value_name, TCHAR *out, DWORD out_len);
DWORD LoadNick(const TCHAR *user_id, TCHAR *out, DWORD out_len);

// Returns TRUE on success, otherwise FALSE.
BOOL SaveSettingString(const TCHAR *value_name, const TCHAR *value);
BOOL SaveNick(const TCHAR *user_id, const TCHAR *nick);

// Returns TRUE on success, otherwise FALSE.
BOOL DeleteSetting(const TCHAR *value_name);
BOOL DeleteNick(const TCHAR *nick);
