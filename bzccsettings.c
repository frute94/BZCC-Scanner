#include "bzccSettings.h"

#ifndef tstrlen
#ifdef UNICODE
#define tstrlen wcslen
#else
#define tstrlen strlen
#endif
#endif

DWORD _LoadSettingDWORD(const TCHAR *key_name, const TCHAR *value_name, DWORD default_value) {
	DWORD value;
	DWORD value_type = REG_DWORD;
	DWORD value_sz = sizeof(DWORD);
	HKEY hKey;
	
	if (RegCreateKeyEx(
		HKEY_CURRENT_USER,
		key_name,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_READ,
		NULL,
		&hKey,
		NULL
	) != ERROR_SUCCESS) {
		return default_value;
	}
	
	if (RegQueryValueEx(
		hKey,
		value_name,
		NULL,
		&value_type,
		(LPBYTE)&value,
		&value_sz
	) != ERROR_SUCCESS) {
		return default_value;
	}
	
	RegCloseKey(hKey);
	return value;
}

BOOL _SaveSettingDWORD(const TCHAR *key_name, const TCHAR *value_name, DWORD value) {
	HKEY hKey;
	if (RegCreateKeyEx(
		HKEY_CURRENT_USER,
		key_name,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE,
		NULL,
		&hKey,
		NULL
	) != ERROR_SUCCESS) {
		return FALSE;
	}
	
	RegSetValueEx(hKey, value_name, 0, REG_DWORD, (BYTE*)&value, sizeof(DWORD));
	RegCloseKey(hKey);
	
	return TRUE;
}

DWORD _LoadSettingString(const TCHAR *key_name, const TCHAR *value_name, TCHAR *out, DWORD out_len) {
	DWORD value;
	DWORD value_type = REG_SZ;
	DWORD value_sz = out_len;
	HKEY hKey;
	
	if (RegCreateKeyEx(
		HKEY_CURRENT_USER,
		key_name,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_READ,
		NULL,
		&hKey,
		NULL
	) != ERROR_SUCCESS) {
		return 0;
	}
	
	if (RegQueryValueEx(
		hKey,
		value_name,
		NULL,
		&value_type,
		(LPBYTE)out,
		&value_sz
	) != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return 0;
	}
	
	RegCloseKey(hKey);
	return value_sz / sizeof(TCHAR) - 1;
}

BOOL _SaveSettingString(const TCHAR *key_name, const TCHAR *value_name, const TCHAR *value) {
	HKEY hKey;
	if (RegCreateKeyEx(
		HKEY_CURRENT_USER,
		key_name,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE,
		NULL,
		&hKey,
		NULL
	) != ERROR_SUCCESS) {
		return FALSE;
	}
	
	if (RegSetValueEx(
		hKey,
		value_name,
		0,
		REG_SZ,
		(LPBYTE)value,
		sizeof(TCHAR)*tstrlen(value)
	) != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return FALSE;
	}
	
	RegCloseKey(hKey);
	return TRUE;
}

BOOL _DeleteSetting(const TCHAR *key_name, const TCHAR *value_name) {
	HKEY hKey;
	if (RegCreateKeyEx(
		HKEY_CURRENT_USER,
		key_name,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_WRITE,
		NULL,
		&hKey,
		NULL
	) != ERROR_SUCCESS) {
		return FALSE;
	}
	
	if (RegDeleteValue(hKey, value_name) != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return FALSE;
	}
	
	RegCloseKey(hKey);
	return TRUE;
}

DWORD LoadSettingDWORD(const TCHAR *value_name, DWORD default_value) {
	return _LoadSettingDWORD(REG_KEY_Setting, value_name, default_value);
}

BOOL SaveSettingDWORD(const TCHAR *value_name, DWORD value) {
	return _SaveSettingDWORD(REG_KEY_Setting, value_name, value);
}

DWORD LoadSettingString(const TCHAR *value_name, TCHAR *out, DWORD out_len) {
	return _LoadSettingString(REG_KEY_Setting, value_name, out, out_len);
}

BOOL SaveSettingString(const TCHAR *value_name, const TCHAR *value) {
	return _SaveSettingString(REG_KEY_Setting, value_name, value);
}

BOOL DeleteSetting(const TCHAR *value_name) {
	return _DeleteSetting(REG_KEY_Setting, value_name);
}

DWORD LoadNick(const TCHAR *user_id, TCHAR *out, DWORD out_len) {
	return _LoadSettingString(REG_KEY_NICKS, user_id, out, out_len);
}

BOOL SaveNick(const TCHAR *user_id, const TCHAR *nick) {
	return _SaveSettingString(REG_KEY_NICKS, user_id, nick);
}

BOOL DeleteNick(const TCHAR *nick) {
	return _DeleteSetting(REG_KEY_NICKS, nick);
}

