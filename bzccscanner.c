#include "bzccnet.h" // apparently winsock2.h must be included before windows.h; which is done in bzccnet, the network portion of this software.
#include "bzccscanner.h"
#include "bzccb64.h"
#include "bzccsettings.h"

#include "resource.h"
#include <commctrl.h>
#include <windowsx.h>
#include <math.h>
#include <stdio.h>

BOOL UserIDIsValid(const TCHAR *user_id) {
	for (DWORD i=0; user_id[i]; i++) {
		if (!isalnum(user_id[i]) || i >= USER_ID_LEN) {
			return FALSE;
		}
	}
	return TRUE;
}

int GetKeyIdentifier(const char *str, const KeyToIdentifier *key2id) {
	for (int i=0; key2id[i].str[0]; i++) {
		if (!strcmpi(key2id[i].str, str)) {
			return key2id[i].id;
		}
	}
	
	return 0; // Unknown
}

const TCHAR* GetGameTypeDescription(int id) {
	for (int i=0; GAMETYPES[i].description[0]; i++) {
		if (i == id) {
			return GAMETYPES[i].description;
		}
	}
	
	return GAMETYPES[0].description;
}

int split_raw_mod_ids(const char *mm_string, unsigned long int *mod_ids, int max_mods) {
	const int mod_id_strlen = 16;
	char mod_id_string[mod_id_strlen];
	int mod_index = 0, len = 0;
	ZeroMemory(mod_id_string, mod_id_strlen);
	
	for (int i=0; mm_string[i]; i++) {
		if (mm_string[i] == ';') {
			if (mod_index+1 >= max_mods) {
				return 0; // buffer overflow
			}
			mod_ids[mod_index++] = atol(mod_id_string);
			ZeroMemory(mod_id_string, mod_id_strlen);
			len = 0;
			continue;
		}
		
		if (isspace(mm_string[i] && len == 0)) {
			continue;
		}
		
		if (isdigit(mm_string[i])) {
			if (len+1 >= mod_id_strlen) {
				return 0; // buffer overflow, invalid mod id string
			}
			
			mod_id_string[len++] = mm_string[i];
			continue;
		}
		
		return 0; // invalid character
	}
	
	// Add last item not broken by ';'
	if (mod_index+1 >= max_mods) {
		return 1; // buffer overflow
	}
	mod_ids[mod_index++] = atoi(mod_id_string);
	
	return mod_index;
}

int ProcessSessionInfo(KeyValueInfo *pKvi, void *pCustomData) {
	WindowControls *pWc = (WindowControls*)pCustomData;
	TCHAR tmpmsg[64];
	char decoded[512];
	
	if (pKvi->status == KVI_STATUS_COMPLETE || pKvi->status == KVI_STATUS_ERROR) {
		EndScan(pWc);
		
		if (pKvi->status == KVI_STATUS_ERROR) {
			if (pWc->iContinueScan) { // Error was not caused by user cancelling
				MessageBox(pWc->hMain, pKvi->err, TEXT("Error"), MB_OK | MB_ICONERROR);
			}
		}
		
		return 1; // In this case, return value does not matter, because processing is already completed or terminated.
	}
	
	if (!pWc->iContinueScan) {
		// User canceled scan.
		// This callback function will be called one more time with KVI_STATUS_ERROR status.
		return 1;
	}
	
	if (pKvi->server_index >= MAX_SERVERS || pKvi->player_index >= MAX_PLAYERS) {
		printf("ERROR: Player or server index out of range.\n");
		return 1;
	}
	
	if (pKvi->server_index >= pWc->scan_count_servers) {
		if (pKvi->server_index >= pWc->scan_count_servers+1) {
			printf("ERROR: Data skipped over server index.\n");
			return 1;
		}
		
		pWc->scan_count_servers += 1;
		pWc->scan_count_players = 0;
		pWc->scan_game = &(pWc->gameinfo[pKvi->server_index]);
		ZeroMemory(pWc->scan_game, sizeof(GameInfo)); // New server begin
	}
	
	if (pKvi->is_player) {
		if (pWc->scan_game == NULL) {
			// No known server to add player info to
			return 1;
		}
		
		if (pKvi->player_index >= pWc->scan_count_players) {
			if (pKvi->player_index >= pWc->scan_count_players+1) {
				printf("ERROR: Data skipped over player index.\n");
				return 1;
			}
			
			pWc->scan_count_players += 1;
			pWc->scan_player = &(pWc->scan_game->playerinfo[pKvi->player_index]);
			ZeroMemory(pWc->scan_player, sizeof(PlayerInfo)); // New player begin
		}
		
		switch (GetKeyIdentifier(pKvi->key_str, PLAYER_KEY_TO_ID)) {
			case PLAYERKEY_NAME:
				b64decode(pKvi->value_str, decoded, 512);
				snwprintf(pWc->scan_player->ingame_name, PLAYER_NAME_LEN, TEXT("%s"), decoded);
			break;
			
			case PLAYERKEY_USERID:
				snwprintf(pWc->scan_player->user_id, USER_ID_LEN, TEXT("%s"), pKvi->value_str);
			break;
			
			case PLAYERKEY_TEAM:
				pWc->scan_player->team = atoi(pKvi->value_str);
			break;
			
			case PLAYERKEY_KILLS:
				pWc->scan_player->kills = atoi(pKvi->value_str);
			break;
		}
		
		if (pKvi->status == KVI_STATUS_LAST) {
			// No need to flush data - UI info is set when user clicks on a server.
			pWc->scan_game->player_count += 1;
		}
	} else {
		switch (GetKeyIdentifier(pKvi->key_str, SERVER_KEY_TO_ID)) {
			case SERVERKEY_NAME:
				b64decode(pKvi->value_str, decoded, 512);
				snwprintf(pWc->scan_game->name, SERVER_NAME_LEN, TEXT("%s"), decoded);
			break;
			
			case SERVERKEY_MAP:
				snwprintf(pWc->scan_game->map, SERVER_MAP_LEN, TEXT("%s.bzn"), pKvi->value_str);
			break;
			
			case SERVERKEY_VERSION:
				snwprintf(pWc->scan_game->version, SERVER_VER_LEN, TEXT("%s"), pKvi->value_str);
			break;
			
			case SERVERKEY_HOSTMSG:
				snwprintf(pWc->scan_game->msg, SERVER_HOSTMSG_LEN, TEXT("%s"), pKvi->value_str);
			break;
			
			case SERVERKEY_MODSTRING:
				split_raw_mod_ids(pKvi->value_str, pWc->scan_game->mod_id, MAX_MODS);
			break;
			
			case SERVERKEY_PLAYERLIMIT:
				pWc->scan_game->playerlimit = atoi(pKvi->value_str);
			break;
			
			case SERVERKEY_TPS:
				pWc->scan_game->tps = atoi(pKvi->value_str);
			break;
			
			case SERVERKEY_PWD:
				pWc->scan_game->pwd = TRUE;
			break;
			
			case SERVERKEY_MAX_PING:
				pWc->scan_game->max_ping = atoi(pKvi->value_str);
			break;
			
			case SERVERKEY_GAMETIME:
				pWc->scan_game->gametime = atoi(pKvi->value_str);
			break;
			
			case SERVERKEY_GAMETYPE:
				pWc->scan_game->gametype = atoi(pKvi->value_str);
			break;
			
			case SERVERKEY_TIMELIMIT:
				pWc->scan_game->timelimit = atoi(pKvi->value_str);
			break;
			
			case SERVERKEY_KILLLIMIT:
				pWc->scan_game->killlimit = atoi(pKvi->value_str);
			break;
		}
		
		if (pKvi->status == KVI_STATUS_LAST) {
			snwprintf(pWc->scan_game->mod_name, SERVER_NAME_LEN, TEXT("%lu"), pWc->scan_game->mod_id[0]); // TODO - Get mod name
			GameInfoToRow(pWc->hGameList, pWc->scan_game); // New server flush
			if (pWc->remember_sort && pWc->last_col_sort_index >= 0) {
				ListView_SortItems(pWc->hGameList, CompareFunc, pWc);
			}
			
			// It is possible to select a player while scanning for sessions.
			if (!pWc->pSelectedPlayerInfo) {
				pWc->timestamp_result = ((float)(GetTimestamp() - pWc->timestamp)) / 10000000;
				pWc->total_player_count += pWc->scan_count_players;
				SetGeneralStatus(pWc);
			}
		}
	}
	
	return 0;
}

DWORD64 GetTimestamp() {
	ULARGE_INTEGER bigint;
	SYSTEMTIME systime;
	FILETIME ftime;
	GetSystemTime(&systime);
	SystemTimeToFileTime(&systime, &ftime);
	bigint.LowPart=ftime.dwLowDateTime;
	bigint.HighPart=ftime.dwHighDateTime;
	return (DWORD64)bigint.QuadPart;
}

BOOL GameInfoToRow(HWND hLV, GameInfo *pGameInfo) {
	int player_count = 0;
	const int len = 32;
	TCHAR tmpstr[32];
	
	LVITEM lvi;
	lvi.mask = LVIF_PARAM | LVIF_TEXT;
	lvi.cchTextMax = len;
	lvi.iItem = 0;
	lvi.lParam = (LPARAM)pGameInfo;
	
	lvi.iSubItem = 0;
	lvi.pszText = pGameInfo->name;
	if (ListView_InsertItem(hLV, &lvi) == -1) {
		return FALSE;
	}
	
	lvi.mask = LVIF_TEXT;
	
	lvi.iSubItem += 1;
	lvi.pszText = pGameInfo->pwd ? COLUMN_TEXT_PASSWORDED_GAME : TEXT("");
	ListView_SetItem(hLV, &lvi);
	
	lvi.iSubItem += 1;
	snwprintf(tmpstr, len, TEXT("")); // TODO - Get ping
	lvi.pszText = tmpstr;
	ListView_SetItem(hLV, &lvi);
	
	lvi.iSubItem += 1;
	snwprintf(tmpstr, len, TEXT("%d/%d"), pGameInfo->player_count, pGameInfo->playerlimit);
	lvi.pszText = tmpstr;
	ListView_SetItem(hLV, &lvi);
	
	lvi.iSubItem += 1;
	lvi.pszText = pGameInfo->map;
	ListView_SetItem(hLV, &lvi);
	
	lvi.iSubItem += 1;
	snwprintf(tmpstr, len, TEXT("%ls"), GetGameTypeDescription(pGameInfo->gametype));
	lvi.pszText = tmpstr;
	ListView_SetItem(hLV, &lvi);
	
	lvi.iSubItem += 1;
	lvi.pszText = pGameInfo->mod_name;
	ListView_SetItem(hLV, &lvi);
	return TRUE;
}

BOOL UpdatePlayerList(WindowControls *pWc) {
	TCHAR tmp[PLAYER_NAME_LEN];
	int sel = LB_ERR;
	int iTmp = ListBox_GetCount(pWc->hPlayerList);
	PlayerInfo *pPlayerInfo;
	GameInfo *pGameInfo;
	
	if (!pWc) {
		return FALSE;
	}
	
	pGameInfo = pWc->pSelectedGameInfo;
	
	// Find which item is selected
	if (iTmp != LB_ERR && iTmp == pGameInfo->player_count) {
		for (int i=0; i<iTmp; i++) {
			if (ListBox_GetSel(pWc->hPlayerList, i)) {
				sel = i;
				break;
			}
		}
	}
	
	ListBox_ResetContent(pWc->hPlayerList);
	
	for (int i=0; i<(pGameInfo->player_count); i++) {
		pPlayerInfo = &(pGameInfo->playerinfo[i]);
		
		// Find or import local names
		if (!pPlayerInfo->local_name[0] && pPlayerInfo->user_id[0] && pWc->pHtLocalNames) {
			DWORD crc32 = CalcCRC32(pPlayerInfo->user_id);
			const TCHAR *str_from_ht = HT_Find_crc32(pWc->pHtLocalNames, crc32);
			if (str_from_ht) {
				snwprintf(pPlayerInfo->local_name, PLAYER_NAME_LEN, TEXT("%ls"), str_from_ht);
			} else {
				// Try loading nick externally
				if (LoadNick(pPlayerInfo->user_id, tmp, PLAYER_NAME_LEN)) {
					snwprintf(pPlayerInfo->local_name, PLAYER_NAME_LEN, TEXT("%ls"), tmp);
					HT_New(pWc->pHtLocalNames, crc32, pPlayerInfo->local_name);
				} else {
					// No local nick for this player's steam or GOG id is available.
					if (pWc->auto_nick_save) {
						// Use this player's ingame name as their nick name (permanently saves)
						snwprintf(pPlayerInfo->local_name, PLAYER_NAME_LEN, TEXT("%ls"), pPlayerInfo->ingame_name);
						SaveNick(pPlayerInfo->user_id, pPlayerInfo->local_name);
						HT_New(pWc->pHtLocalNames, crc32, pPlayerInfo->local_name);
					}
				}
			}
		}
		
		if (pWc->use_local_names && pPlayerInfo->local_name[0]) {
			snwprintf(tmp, PLAYER_NAME_LEN, TEXT("\u2713 %ls"), pPlayerInfo->local_name); // Add little checkbox at start of name
			iTmp = ListBox_AddString(pWc->hPlayerList, tmp);
		} else {
			iTmp = ListBox_AddString(pWc->hPlayerList, pPlayerInfo->ingame_name);
		}
		
		if (iTmp == LB_ERR) {
			ListBox_ResetContent(pWc->hPlayerList);
			return FALSE;
		}
		
		if (iTmp == LB_ERRSPACE) {
			// return FALSE; // Don't think this is possible
		}
		
		ListBox_SetItemData(pWc->hPlayerList, 0, (LPARAM)pPlayerInfo);
	}
	
	if (sel != LB_ERR) {
		if (ListBox_SetCurSel(pWc->hPlayerList, sel) == LB_ERR) {
			// return FALSE; // There was a problem restoring the original selection.
		}
	}
	
	return TRUE;
}

int EndScan(WindowControls *pWc) {
	// printf("END SCAN\n");
	
	if (pWc->hScan) {
		CloseHandle(pWc->hScan);
		pWc->hScan = NULL;
	}
	
	// User tried to quit during scan
	if (pWc->quitting) {
		SendMessage(pWc->hMain, WM_CLOSE, 0, 0);
		return 1;
	}
	
	UpdateContextMenu(pWc);
	Button_SetText(pWc->hRefresh, BUTTON_TEXT_SCAN_REFRESH);
	Button_Enable(pWc->hRefresh, TRUE);
	
	if (pWc->auto_scan) {
		pWc->timer_id = SetTimer(pWc->hMain, IDT_REFRESH, pWc->auto_scan_delay * 1000, NULL);
	}
	
	return 0;
}

int BeginScan(WindowControls *pWc) {
	// printf("BEGIN SCAN\n");
	
	if (pWc->timer_id) {
		KillTimer(pWc->hMain, pWc->timer_id);
		pWc->timer_id = 0;
	}
	
	pWc->iContinueScan = 1;
	pWc->pSelectedGameInfo = NULL;
	pWc->pSelectedPlayerInfo = NULL;
	
	if (!pWc->remember_sort) {
		pWc->last_col_sort_index = -1;
		pWc->reverse_sort = FALSE;
	}
	
	pWc->timestamp_result = 0.0;
	pWc->timestamp = GetTimestamp();
	pWc->total_player_count = 0;
	SetGeneralStatus(pWc);
	
	ListBox_ResetContent(pWc->hPlayerList);
	ListView_DeleteAllItems(pWc->hGameList);
	
	Edit_SetText(pWc->hHostMessage, TEXT(""));
	Edit_SetText(pWc->hShellMap, TEXT(""));
	Edit_SetText(pWc->hServerInfo, TEXT(""));
	
	SetWindowText(pWc->hMain, DEFAULT_TITLE);
	
	Button_SetText(pWc->hRefresh, BUTTON_TEXT_SCAN_STOP);
	Button_Enable(pWc->hJoin, FALSE);
	Button_Enable(pWc->hURL, FALSE);
	Button_Enable(pWc->hSetName, FALSE);
	
	pWc->hScan = CreateThread(
		NULL,
		0,
		ScanThread, // Function
		(LPVOID)pWc, // Pointer passed to thread function
		0, // Flags
		NULL // Pointer to DWORD thread ID
	);
	
	if (pWc->hScan == NULL) {
		printf("Failed to create thread.\n");
		Button_SetText(pWc->hRefresh, BUTTON_TEXT_SCAN_REFRESH);
	}
	
	UpdateContextMenu(pWc);
	
	return 0;
}

void SetGeneralStatus(WindowControls *pWc) {
	TCHAR tmpmsg[64];
	
	snwprintf(tmpmsg, 64, TEXT("Scan time %.3f seconds"), pWc->timestamp_result);
	SendMessage(pWc->hStatusBar, SB_SETTEXT, 0, (LPARAM)tmpmsg);
	
	snwprintf(tmpmsg, 64, TEXT("%d Games\t%d Players"), pWc->scan_count_servers, pWc->total_player_count);
	SendMessage(pWc->hStatusBar, SB_SETTEXT, 1, (LPARAM)tmpmsg);
	
	snwprintf(tmpmsg, 64, TEXT("%ls:%d/%ls"), pWc->host_name, pWc->host_port, pWc->host_file);
	SendMessage(pWc->hStatusBar, SB_SETTEXT, 2|SBT_POPOUT, (LPARAM)tmpmsg);
}

void SetPlayerStatus(WindowControls *pWc) {
	TCHAR tmpmsg[64];
	PlayerInfo *pPlayerInfo = pWc->pSelectedPlayerInfo;
	if (!pPlayerInfo) {
		return;
	}
	
	switch (pPlayerInfo->user_id[0]) {
		case 'S': // Steam
			Button_Enable(pWc->hURL, TRUE);
			snwprintf(tmpmsg, 64, TEXT("Steam: [%ls]"), pPlayerInfo->user_id+1);
		break;
		
		case 'G': // GOG
			Button_Enable(pWc->hURL, FALSE);
			snwprintf(tmpmsg, 64, TEXT("GOG ID: [%ls]"), pPlayerInfo->user_id+1);
		break;
		
		default:
			Button_Enable(pWc->hURL, FALSE);
			snwprintf(tmpmsg, 64, TEXT("Unknown ID: [%ls]"), pPlayerInfo->user_id);
	}
	
	SendMessage(pWc->hStatusBar, SB_SETTEXT, 0, (LPARAM)tmpmsg);
	
	snwprintf(tmpmsg, 64, TEXT("%ls"), pPlayerInfo->ingame_name);
	SendMessage(pWc->hStatusBar, SB_SETTEXT, 1, (LPARAM)tmpmsg);
	
	if (pPlayerInfo->local_name[0]) {
		snwprintf(tmpmsg, 64, TEXT("Nick: %ls"), pPlayerInfo->local_name);
	} else {
		snwprintf(tmpmsg, 64, TEXT(""));
	}
	SendMessage(pWc->hStatusBar, SB_SETTEXT, 2, (LPARAM)tmpmsg);
}

DWORD WINAPI ScanThread(LPVOID void_pWc) {
	WindowControls *pWc = (WindowControls*)void_pWc;
	pWc->scan_count_servers = pWc->scan_count_players = 0;
	
	if (!pWc->host_name[0]) {
		// No host name, treat host_file as file on disk.
		// This is what I used for debugging the parser.
		return ScanFile(pWc->host_file, ProcessSessionInfo, (void*)pWc);
	} else {
		// Server scan
		char host_bytes[128], port_bytes[8], file_bytes[128];
		snprintf(host_bytes, 128, "%ls", pWc->host_name);
		snprintf(port_bytes, 8, "%d", pWc->host_port);
		snprintf(file_bytes, 128, "%ls", pWc->host_file);
		return ScanHTTP(host_bytes, port_bytes, file_bytes, 8000, ProcessSessionInfo, (void*)pWc);
	}
	
	return 0;
}

int WndResize(WindowControls *pWc, int w, int h) {
	const int m = 4; // Absolute Pixel Margin
	const int xoffh = 10; // Header text offset
	
	const int h_top_text_row = 20;
	
	const int w_boxes = w/4;
	const int h_boxes = h/2 - m;
	
	const int w_localnames = 150;
	const int x_buttons_row = 5; // Starting position of all buttons
	const int y_buttons_row = h_boxes + h_top_text_row + m;
	const int h_buttons = 25;
	
	const int h_statusbar = 25;
	
	const int y_gamelist = y_buttons_row + h_buttons + m*2;
	const int h_gamelist = h - y_gamelist - h_statusbar + m;
	
	const int wh_shellmap = min(w_boxes, h_boxes);
	const int x_shellmap = w_boxes + (w_boxes - wh_shellmap) / 2;
	const int w_shellmap_text = max(wh_shellmap, w_boxes - x_shellmap);
	
	for (int i=0; i<7; i++) {
		ListView_GetColumnWidth(pWc->hGameList, i);
	}
	
	MoveWindow(pWc->hPlayerListText, xoffh, 0, w_boxes - m - xoffh, h_top_text_row, TRUE);
	MoveWindow(pWc->hShellMapText, x_shellmap+xoffh, 0, w_shellmap_text - m*2 - xoffh, h_top_text_row, TRUE);
	MoveWindow(pWc->hHostMessageText, w_boxes*2+xoffh, 0, w_boxes - m*3 - xoffh, h_top_text_row, TRUE);
	MoveWindow(pWc->hServerInfoText, w_boxes*3+xoffh - m, 0, w_boxes - xoffh, h_top_text_row, TRUE);
	
	MoveWindow(pWc->hPlayerList, 0, h_top_text_row, w_boxes - m, h_boxes, TRUE);
	MoveWindow(pWc->hShellMap, x_shellmap, h_top_text_row, wh_shellmap - m*2, wh_shellmap, TRUE);
	MoveWindow(pWc->hHostMessage, w_boxes*2, h_top_text_row, w_boxes - m*3, h_boxes, TRUE);
	MoveWindow(pWc->hServerInfo, w_boxes*3 - m, h_top_text_row, w_boxes, h_boxes, TRUE);
	
	MoveWindow(pWc->hLocalNames, x_buttons_row, y_buttons_row, w_localnames, h_buttons, TRUE);
	
	const HWND hButtons[4] = {pWc->hSetName, pWc->hURL, pWc->hJoin, pWc->hRefresh};
	const int max_sizes[4] = {65, 100, 65, 150};
	const int min_sizes[4] = {50, 70, 60, 80};
	const int margin_sizes[4] = {m, m*2, m*4, m*2}; // Variable margin between buttons
	const int w_button = (w - m*2 - w_localnames) / 4; // Default width without min/max constraints
	int x_button = x_buttons_row + w_localnames + m;
	int w_this_button;
	for (int i=0; i<4; i++) {
		w_this_button = min(max(w_button, min_sizes[i]), max_sizes[i]);
		MoveWindow(
			hButtons[i],
			x_button,
			y_buttons_row,
			w_this_button - margin_sizes[i],
			h_buttons,
			TRUE
		);
		
		x_button += w_this_button + margin_sizes[i];
	}
	const int w_prefs = 32;
	const int x_prefs = max(x_button, w-w_prefs-m);
	MoveWindow(pWc->hPrefs, x_prefs, y_buttons_row, w_prefs, 28, TRUE);
	
	MoveWindow(pWc->hGameList, 0, y_gamelist, w, h_gamelist, TRUE);
	
	int new_width;
	DWORD preceding_w = 0;
	float ratio = (float)w / (float)pWc->width; // Percent of last width when resizing started
	for (int i=0; i<COLUMN_COUNT; i++) {
		ListView_SetColumnWidth(pWc->hGameList, i, (int)round((float)pWc->column_width[i] * ratio));
	}
	
	const int ptr_w_statusbar[3] = {w*0.30, w*0.60, -1};
	MoveWindow(pWc->hStatusBar, 0, h - h_statusbar, w, h_statusbar, TRUE);
	SendMessage(pWc->hStatusBar, SB_SETPARTS, 3, (LPARAM)ptr_w_statusbar);
	
	// Some reason the text was getting chopped up during some resize operations, this should fix.
	RECT force_redraw = {0, 0, w, h_top_text_row};
	RedrawWindow(pWc->hMain, &force_redraw, NULL, RDW_INVALIDATE);
	RedrawWindow(pWc->hShellMap, NULL, NULL, RDW_INVALIDATE);
	RedrawWindow(pWc->hGameList, NULL, NULL, RDW_INVALIDATE);
	
	return 0;
}

int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
	const BOOL use_fallback = FALSE;
	WindowControls *pWc = (WindowControls*)lParamSort;
	GameInfo *gGameInfo1 = (GameInfo*)lParam1;
	GameInfo *gGameInfo2 = (GameInfo*)lParam2;
	const TCHAR *alpha_cmp_1 = gGameInfo1->name;
	const TCHAR *alpha_cmp_2 = gGameInfo2->name;
	const TCHAR *substr_1 = NULL, *substr_2 = NULL;
	int int_1, int_2, iResult;
	BOOL reverse = pWc->reverse_sort;
	BOOL int_compare = FALSE;
	
	switch (pWc->last_col_sort_index) {
		case COL_PASSWORD:
			int_compare = TRUE;
			int_1 = (int)gGameInfo1->pwd;
			int_2 = (int)gGameInfo2->pwd;
		break;
		
		case COL_PING:
			int_compare = TRUE;
			int_1 = gGameInfo1->max_ping;
			int_2 = gGameInfo2->max_ping;
		break;
		
		case COL_PLAYERS:
			int_compare = TRUE;
			int_1 = gGameInfo1->player_count;
			int_2 = gGameInfo2->player_count;
		break;
		
		case COL_MAP:
			substr_1 = gGameInfo1->map;
			substr_2 = gGameInfo2->map;
		break;
		
		case COL_GAMETYPE:
			substr_1 = GetGameTypeDescription(gGameInfo1->gametype);
			substr_2 = GetGameTypeDescription(gGameInfo2->gametype);
		break;
		
		case COL_GAMEMOD:
			substr_1 = gGameInfo1->mod_name;
			substr_2 = gGameInfo2->mod_name;
		break;
	}
	
	if (int_compare) {
		iResult = int_1 - int_2;
		
		if (iResult) {
			return reverse ? -iResult : iResult;
		}
		
		if (!use_fallback) {
			return 0;
		}
	} else {
		if (substr_1 && substr_2) {
			iResult = CompareString(LOCALE_CUSTOM_DEFAULT, 0x00, substr_1, -1, substr_2, -1);
			if (iResult) {
				iResult -= 2;
				return reverse ? -iResult : iResult;
			}
			
			if (!use_fallback) {
				return 0;
			}
		}
	}
	
	iResult = CompareString(LOCALE_CUSTOM_DEFAULT, 0x00, alpha_cmp_1, -1, alpha_cmp_2, -1);
	
	if (iResult) {
		// "To maintain the C runtime convention of comparing strings,
		// the value 2 can be subtracted from a nonzero return value.
		// Then, the meaning of <0, ==0, and >0 is consistent with the C runtime."
		iResult -= 2;
	}
	
	return reverse ? -iResult : iResult;
}

void UpdateContextMenu(WindowControls *pWc) {
	TCHAR text[32];
	if (pWc->hMenu) {
		DestroyMenu(pWc->hMenu);
	}
	
	pWc->hMenu = CreatePopupMenu();
	AppendMenu(pWc->hMenu, MF_ENABLED | MF_STRING, WM_APP_PREFS_DLG, MENU_TEXT_PREFS);
	AppendMenu(pWc->hMenu, MF_SEPARATOR, 0, NULL);
	if (pWc->hScan) {
		AppendMenu(pWc->hMenu, pWc->iContinueScan ? MF_ENABLED : MF_DISABLED | MF_STRING, WM_APP_REFRESH, BUTTON_TEXT_SCAN_STOP);
	} else {
		AppendMenu(pWc->hMenu, MF_ENABLED | MF_STRING, WM_APP_REFRESH, BUTTON_TEXT_SCAN_REFRESH);
	}
	AppendMenu(pWc->hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(pWc->hMenu, MF_ENABLED | MF_STRING, WM_APP_EXIT, MENU_TEXT_EXIT);
}


LRESULT CALLBACK WndProcPrefsDlg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	WindowControls *pWc = (WindowControls*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	TCHAR tmpbuf[256];
	
	switch (msg) {
		case WM_COMMAND:
			if (!pWc) {
				break;
			}
			
			if (LOWORD(wParam)) {
				break;
			}
			
			switch (HIWORD(wParam)) {
				case EN_CHANGE:
					if ((HWND)lParam == pWc->hAutoScanTime) {
						Button_Enable(pWc->hAutoScanTimeApply, TRUE);
						break;
					}
					
					if (
						(HWND)lParam == pWc->hHostAddr ||
						(HWND)lParam == pWc->hServFile ||
						(HWND)lParam == pWc->hPortNum
					) {
						Button_Enable(pWc->hHostApply, TRUE);
						break;
					}
				break;
				
				case BN_CLICKED:
					if ((HWND)lParam == pWc->hAutoCreateNicks) {
						pWc->auto_nick_save = !(pWc->auto_nick_save);
						SaveSettingDWORD(TEXT("AUTO_CREATE_NICKS"), pWc->auto_nick_save);
						if (pWc->auto_nick_save && pWc->pSelectedPlayerInfo) {
							UpdatePlayerList(pWc);
						}
						break;
					}
					
					if ((HWND)lParam == pWc->hRememberSort) {
						pWc->remember_sort = !(pWc->remember_sort);
						SaveSettingDWORD(TEXT("AUTO_SORT"), pWc->remember_sort);
						if (pWc->remember_sort) {
							SaveSettingDWORD(TEXT("LAST_SORT_INDEX"), (DWORD)pWc->last_col_sort_index);
							SaveSettingDWORD(TEXT("LAST_SORT_REVERSE"), (DWORD)pWc->reverse_sort);
						}
						break;
					}
					
					if ((HWND)lParam == pWc->hRememberWindow) {
						pWc->remember_size = !(pWc->remember_size);
						SaveSettingDWORD(TEXT("REMEMBER_SIZE"), pWc->remember_size);
						break;
					}
					
					if ((HWND)lParam == pWc->hAutoScan) {
						pWc->auto_scan = !(pWc->auto_scan);
						SaveSettingDWORD(TEXT("AUTO_SCAN"), pWc->auto_scan);
						
						if (pWc->auto_scan) {
							pWc->timer_id = SetTimer(pWc->hMain, IDT_REFRESH, pWc->auto_scan_delay * 1000, NULL);
						} else {
							if (pWc->timer_id) {
								KillTimer(pWc->hMain, pWc->timer_id);
								pWc->timer_id = 0;
							}
						}
						break;
					}
					
					if ((HWND)lParam == pWc->hAutoScanTimeApply) {
						Edit_GetText(pWc->hAutoScanTime, tmpbuf, 8);
						pWc->auto_scan_delay = (DWORD)ttoi(tmpbuf);
						if (pWc->auto_scan_delay < MIN_SCAN_DELAY) {
							pWc->auto_scan_delay = MIN_SCAN_DELAY;
							snwprintf(tmpbuf, 8, TEXT("%lu"), pWc->auto_scan_delay);
							Edit_SetText(pWc->hAutoScanTime, tmpbuf);
						}
						
						if (pWc->auto_scan) {
							KillTimer(pWc->hMain, pWc->timer_id);
							pWc->timer_id = SetTimer(pWc->hMain, IDT_REFRESH, pWc->auto_scan_delay * 1000, NULL);
						}
						
						SaveSettingDWORD(TEXT("AUTO_SCAN_DELAY"), pWc->auto_scan_delay);
						Button_Enable(pWc->hAutoScanTimeApply, FALSE);
						break;
					}
					
					if ((HWND)lParam == pWc->hHostApply) {
						Edit_GetText(pWc->hHostAddr, pWc->host_name, HOSTNAME_LEN);
						Edit_GetText(pWc->hPortNum, tmpbuf, 8);
						pWc->host_port = (DWORD)ttoi(tmpbuf);
						Edit_GetText(pWc->hServFile, pWc->host_file, HOSTNAME_LEN);
						
						SaveSettingString(TEXT("HOST"), pWc->host_name);
						SaveSettingDWORD(TEXT("PORT"), pWc->host_port);
						SaveSettingString(TEXT("FILE"), pWc->host_file);
						Button_Enable(pWc->hHostApply, FALSE);
						if (!pWc->pSelectedPlayerInfo) {
							SetGeneralStatus(pWc); // Reflect updated host info in status bar
						}
						break;
					}
				break; 
			}
		break;
		
		case WM_CLOSE:
			ShowWindow(hWnd, SW_HIDE);
		break;
		
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	
	return 0;
}

LRESULT CALLBACK WndProcRenDlg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	RenameDlgControls *pRdc = (RenameDlgControls*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	
	switch (msg) {
		case WM_COMMAND:
			if (!pRdc) {
				break;
			}
			
			switch (LOWORD(wParam)) {
				case IDM_REN_SELALL:
					SetFocus(pRdc->hInput);
					Edit_SetSel(pRdc->hInput, 0, -1);
				break;
				case IDM_REN_DELETE:
					SendMessage(hWnd, WM_RDC_DELETE, 0, 0);
				break;
				case IDM_REN_CLOSE:
					SendMessage(hWnd, WM_CLOSE, 0, 0);
				break;
			}
			
			if (LOWORD(wParam)) {
				break;
			}
			
			switch (HIWORD(wParam)) {
				case BN_CLICKED:
					if ((HWND)lParam == pRdc->hOk) {
						SendMessage(hWnd, WM_RDC_OK, 0, 0);
					}
					
					if ((HWND)lParam == pRdc->hDelete) {
						SendMessage(hWnd, WM_RDC_DELETE, 0, 0);
					}
					
					if ((HWND)lParam == pRdc->hCancel) {
						SendMessage(hWnd, WM_CLOSE, 0, 0);
					}
				break; 
			}
		break;
		
		case WM_RDC_DELETE:
			Static_SetText(pRdc->hInput, TEXT("")); // Submitting empty text means delete nick
		case WM_RDC_OK:
			pRdc->submit = TRUE;
			SendMessage(hWnd, WM_CLOSE, 0, 0);
		break;
		
		case WM_CLOSE:
			ShowWindow(hWnd, SW_HIDE);
			if (pRdc) {
				SendMessage(pRdc->hParent, WM_APP_CLOSE_RENAME_DLG, 0, 0);
			}
		break;
		
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	
	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	WindowControls *pWc = (WindowControls*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	TCHAR tmpmsg[256];
	BOOL menuResult;
	DWORD notifyIconType;
	LVITEM lvi;
	POINT cur;
	LPNMLISTVIEW pnmv;
	RECT rect;
	GameInfo *pGameInfo;
	PlayerInfo *pPlayerInfo;
	int iTmp;
	
	switch (msg) {
		case WM_APP_PREFS_DLG:
			if (!pWc || !pWc->hPrefsDlg) {
				break;
			}
			GetWindowRect(pWc->hMain, &rect);
			MoveWindow(
				pWc->hPrefsDlg,
				rect.left + (rect.right - rect.left)/2 - PREFS_WINDOW_WIDTH/2,
				rect.top + (rect.bottom - rect.top)/2 - PREFS_WINDOW_HEIGHT/2,
				PREFS_WINDOW_WIDTH,
				PREFS_WINDOW_HEIGHT,
				TRUE
			);
			ShowWindow(pWc->hPrefsDlg, SW_NORMAL);
			SetActiveWindow(pWc->hPrefsDlg);
		break;
		
		case WM_APP_RENAME_DLG:
			if (!pWc) {
				break;
			}
			
			pPlayerInfo = pWc->pSelectedPlayerInfo;
			if (pWc->pRdc && !pWc->hInDlg && pPlayerInfo) {
				if (pWc->pSelectedGameInfo) {
					snwprintf(tmpmsg, 256, TEXT("Set nickname for %ls (%ls)"), pPlayerInfo->ingame_name, pPlayerInfo->user_id);
					EnableWindow(pWc->hMain, FALSE);
					pWc->hInDlg = pWc->hRenameDlg;
					
					// List of games and players assumed not to change
					// by the time user submits new value.
					pWc->pRdc->pPlayer = pPlayerInfo;
					
					SetWindowText(pWc->hInDlg, pPlayerInfo->local_name[0] ? DEFAULT_TITLE_RENAME_DLG : DEFAULT_TITLE_RENAME_DLG_NEW);
					Static_SetText(pWc->pRdc->hMessage, tmpmsg);
					Edit_LimitText(pWc->pRdc->hInput, PLAYER_NAME_LEN-1);
					Edit_SetText(pWc->pRdc->hInput, pPlayerInfo->local_name[0] ? pPlayerInfo->local_name : pPlayerInfo->ingame_name);
					SetFocus(pWc->pRdc->hInput);
					Edit_SetSel(pWc->pRdc->hInput, 0, -1);
					Button_Enable(pWc->pRdc->hDelete, pPlayerInfo->local_name[0]);
					
					GetWindowRect(pWc->hMain, &rect);
					MoveWindow(
						pWc->hRenameDlg,
						rect.left + (rect.right - rect.left)/2 - RDC_WINDOW_WIDTH/2,
						rect.top + (rect.bottom - rect.top)/2 - RDC_WINDOW_HEIGHT/2,
						RDC_WINDOW_WIDTH,
						RDC_WINDOW_HEIGHT,
						TRUE
					);
					ShowWindow(pWc->hRenameDlg, SW_NORMAL);
					SetActiveWindow(pWc->hRenameDlg);
				}
			}
		break;
		
		case WM_APP_CLOSE_RENAME_DLG:
			if (!pWc || pWc->hInDlg != pWc->hRenameDlg) {
				break;
			}
			
			EnableWindow(hWnd, TRUE);
			pWc->hInDlg = NULL;
			SetActiveWindow(pWc->hMain);
			
			if (pWc->auto_scan_request_while_busy) {
				if (pWc->auto_scan) {
					// Just reset the timer I guess is the best solution?
					// Another option is to immediately fire the timer when available.
					pWc->timer_id = SetTimer(pWc->hMain, IDT_REFRESH, pWc->auto_scan_delay * 1000, NULL);
				}
				pWc->auto_scan_request_while_busy = FALSE;
			}
			
			if (pWc->pHtLocalNames && pWc->pRdc->submit) {
				pWc->pRdc->submit = FALSE;
				Edit_GetText(pWc->pRdc->hInput, tmpmsg, PLAYER_NAME_LEN);
				pPlayerInfo = pWc->pRdc->pPlayer;
				if (tstrcmp(tmpmsg, pPlayerInfo->local_name)) {
					// Security concern: Input (steam/gog ID key name string) from internet source is written to registry value name on computer.
					// I don't think this poses a serious risk other than maybe flooding the registry with values or values with offensive names.
					if (UserIDIsValid(pPlayerInfo->user_id)) {
						if (!tmpmsg[0]) {
							// Empty name field means delete nick (always use ingame nick even if show local names is on)
							pPlayerInfo->local_name[0] = 0;
							HT_Delete(pWc->pHtLocalNames, pPlayerInfo->user_id);
							DeleteNick(pPlayerInfo->user_id);
						} else {
							snwprintf(pPlayerInfo->local_name, PLAYER_NAME_LEN, TEXT("%ls"), tmpmsg);
							HT_Set(pWc->pHtLocalNames, pPlayerInfo->user_id, tmpmsg, NULL);
							SaveNick(pPlayerInfo->user_id, tmpmsg);
						}
					} else {
						snwprintf(tmpmsg, 256, TEXT("Invalid user ID: %ls\n"), pPlayerInfo->user_id);
						MessageBox(pWc->hMain, tmpmsg, TEXT("Security Error"), MB_OK | MB_ICONERROR);
					}
				}
				
				UpdatePlayerList(pWc);
				SetPlayerStatus(pWc);
			}
		break;
		
		case WM_TIMER:
			if (wParam == IDT_REFRESH) {
				if (pWc->hInDlg == pWc->hRenameDlg) {
					pWc->auto_scan_request_while_busy = TRUE;
					KillTimer(pWc->hMain, pWc->timer_id);
					pWc->timer_id = 0;
				} else {
					SendMessage(hWnd, WM_APP_REFRESH, 0, 0);
				}
			}
		break;
		
		case WM_SYSCOMMAND:
			switch(wParam) {
				// Minimize to tray
				case SC_MINIMIZE:
					ShowWindow(hWnd, SW_HIDE);
					Shell_NotifyIcon(NIM_ADD, pWc->pTray);
					pWc->minimized = TRUE;
				break;
				
				default:
					return DefWindowProc(hWnd, msg, wParam, lParam);
			}
		break;
		
		case WM_COMMAND:
			if (!pWc) {
				break;
			}
			
			// Accelerators
			switch (LOWORD(wParam)) {
				case IDM_PREFERENCES:
					SendMessage(hWnd, WM_APP_PREFS_DLG, 0, 0);
				break;
				case IDM_REFRESH:
					SendMessage(hWnd, WM_APP_REFRESH, 0, 0);
				break;
				case IDM_RENAME:
					SendMessage(hWnd, WM_APP_RENAME_DLG, 0, 0);
				break;
			}
			
			if (LOWORD(wParam)) {
				break;
			}
			
			switch (HIWORD(wParam)) {
				case LBN_SELCHANGE:
					// Player from player list selected
					if ((HWND)lParam == pWc->hPlayerList) {
						pGameInfo = pWc->pSelectedGameInfo;
						iTmp = ListBox_GetCurSel(pWc->hPlayerList);
						if (iTmp >= MAX_PLAYERS) {
							snwprintf(tmpmsg, 256, TEXT("Listbox 0-based selected player index %d exceeds maximum players %d"), iTmp, MAX_PLAYERS);
							MessageBox(pWc->hMain, tmpmsg, TEXT("Error"), MB_OK | MB_ICONERROR);
							iTmp = LB_ERR;
						}
						
						if (iTmp == LB_ERR) {
							pWc->pSelectedPlayerInfo = NULL;
							Button_Enable(pWc->hURL, FALSE);
							break;
						}
						
						pWc->pSelectedPlayerInfo = &(pGameInfo->playerinfo[iTmp]);
						Button_Enable(pWc->hSetName, TRUE);
						SetPlayerStatus(pWc);						
					}
				break;
				
				case BN_CLICKED:
					if ((HWND)lParam == pWc->hLocalNames) {
						pWc->use_local_names = !(pWc->use_local_names);
						SaveSettingDWORD(TEXT("USE_LOCAL_NAMES"), pWc->use_local_names);
						
						if (pWc->pSelectedGameInfo) {
							UpdatePlayerList(pWc);
						}
						break;
					}
					
					if ((HWND)lParam == pWc->hRefresh) {
						SendMessage(hWnd, WM_APP_REFRESH, 0, 0);
						break;
					}
					
					if ((HWND)lParam == pWc->hPrefs) {
						SendMessage(hWnd, WM_APP_PREFS_DLG, 0, 0);
						break;
					}
					
					if ((HWND)lParam == pWc->hJoin) {
						// TODO
						break;
					}
					
					if ((HWND)lParam == pWc->hURL) {
						if (pWc->pSelectedPlayerInfo && pWc->pSelectedPlayerInfo->user_id[0] == 'S') {
							snwprintf(tmpmsg, 256, TEXT("https://steamcommunity.com/profiles/%ls"), pWc->pSelectedPlayerInfo->user_id+1);
							ShellExecute(NULL, TEXT("open"), tmpmsg, NULL, NULL, SW_SHOWNORMAL);
						}
						break;
					}
					
					if ((HWND)lParam == pWc->hSetName) {
						SendMessage(hWnd, WM_APP_RENAME_DLG, 0, 0);
					}
				break;
			}
		break;
		
		case WM_NOTIFY:
			if (!pWc) {
				break;
			}
			
			switch (((NMHDR*)lParam)->code) {
				// User clicked on a column header (sort by header)
				case LVN_COLUMNCLICK:
					iTmp = ((LPNMLISTVIEW)lParam)->iSubItem;
					if (iTmp == pWc->last_col_sort_index) {
						pWc->reverse_sort = !(pWc->reverse_sort);
					} else {
						pWc->reverse_sort = FALSE;
						pWc->last_col_sort_index = iTmp;
					}
					
					if (pWc->remember_sort) {
						SaveSettingDWORD(TEXT("LAST_SORT_INDEX"), (DWORD)pWc->last_col_sort_index);
						SaveSettingDWORD(TEXT("LAST_SORT_REVERSE"), (DWORD)pWc->reverse_sort);
					}
					
					ListView_SortItems(pWc->hGameList, CompareFunc, pWc);
				break;
				
				// User selected a server from the game list
				case LVN_ITEMCHANGED:
					iTmp = ((LPNMLISTVIEW)lParam)->iItem;
					lParam = ((LPNMLISTVIEW)lParam)->lParam;
					
					if (iTmp == -1) {
						break;
					}
					
					if (iTmp >= MAX_SERVERS) {
						printf("ERROR: Server Index %d >= %d\n", iTmp, MAX_SERVERS);
						break;
					}
					
					// User selected a different server
					pGameInfo = (GameInfo*)lParam;
					if (pGameInfo && pGameInfo != pWc->pSelectedGameInfo) {
						TCHAR hostmsg_str[512], modlist_str[256]; DWORD offset = 0;
						pWc->pSelectedGameInfo = pGameInfo;
						pWc->pSelectedPlayerInfo = NULL;
						ListBox_ResetContent(pWc->hPlayerList);
						SetWindowText(pWc->hMain, pGameInfo->name);
						
						Static_SetText(pWc->hShellMap, pGameInfo->map);
						Edit_SetText(pWc->hHostMessage, pGameInfo->msg);
						
						for (int i=0; i<MAX_MODS; i++) {
							if (pGameInfo->mod_id[i] == 0) {
								if (!i) {
									snwprintf(modlist_str, 256, TEXT(""));
								}
								break;
							}
							offset += snwprintf(modlist_str + offset, 256-offset, TEXT("%lu\r\n"), pGameInfo->mod_id[i]);
						}
						
						snwprintf(
							hostmsg_str,
							256,
							TEXT(
								"%ls\r\n" // TODO: Actual map name, not bzn name.
								"%ls\r\n" // Game mode
								"\r\n"
								"Server Version: %ls\r\n"
								"\r\n"
								//~ "Not playing or in shell for %d minutes\r\n" // How to know if in shell or ingame?
								"Playing for %d minutes.\r\n" // How to know if in shell or ingame?
								"TPS: %d\r\n"
								"Mods:\r\n"
								"%ls" // Long list of mods
								"\r\n"
								"Time Limit: %d minutes\r\n"
								"Kill Limit: %d\r\n"
								"Max Ping: %d"
							),
							pGameInfo->map,
							GetGameTypeDescription(pGameInfo->gametype),
							pGameInfo->version,
							pGameInfo->gametime,
							pGameInfo->tps,
							modlist_str,
							pGameInfo->timelimit,
							pGameInfo->killlimit,
							pGameInfo->max_ping
							
						);
						Edit_SetText(pWc->hServerInfo, hostmsg_str);
						Button_Enable(pWc->hURL, FALSE);
						Button_Enable(pWc->hSetName, FALSE);
						Button_Enable(pWc->hJoin, TRUE);
						UpdatePlayerList(pWc);
						SetGeneralStatus(pWc);
					}
				break;
			}
		break;
		
		case WM_ENTERSIZEMOVE:
		case WM_EXITSIZEMOVE:
			// If we update width values constantly in WM_SIZE based at a dynamic percent,
			// then the columns start to noticably lose information from cumulative
			// floating point rounding errors.
			GetClientRect(pWc->hMain, &rect);
			pWc->width = rect.right;
			pWc->height = rect.bottom;
			
			for (int i=0; i<COLUMN_COUNT; i++) {
				pWc->column_width[i] = ListView_GetColumnWidth(pWc->hGameList, i);
			}
		break;
		
		case WM_SIZE:
			WndResize(pWc, LOWORD(lParam), HIWORD(lParam));
		break;
		
		case WM_APP_TRAY:
			if (!pWc) {
				break;
			}
		
			switch (lParam) {
				// Right click on tray icon, show popup menu.
				case WM_RBUTTONDOWN:
					GetCursorPos(&cur);
					SetForegroundWindow(hWnd); // This is so the menu closes when user clicks away from tray/menu area
					menuResult = TrackPopupMenu(pWc->hMenu, TPM_RETURNCMD, cur.x, cur.y, 0, hWnd, NULL);
					switch (menuResult) {
						case WM_APP_PREFS_DLG:
						case WM_APP_REFRESH:
							SendMessage(hWnd, menuResult, 0, 0);
						break;
						case WM_APP_EXIT:
							SendMessage(hWnd, WM_CLOSE, 0, 0);
						break;
					}
				break;
				
				// If minimized, show window.
				case WM_LBUTTONDOWN: // case WM_LBUTTONDBLCLK:
					if (pWc->minimized) {
						Shell_NotifyIcon(NIM_DELETE, pWc->pTray);
						ShowWindow(pWc->hMain, SW_NORMAL);
						pWc->minimized = FALSE;
						SetActiveWindow(pWc->hMain);
					}
				break;
			}
		break;
		
		case WM_APP_REFRESH:
			if (pWc->hInDlg) {
				break;
			}
			
			if (pWc->hScan) {
				// Already scanning
				if (pWc->iContinueScan) {
					pWc->iContinueScan = 0;
					UpdateContextMenu(pWc);
					Button_Enable(pWc->hRefresh, FALSE);
				}
				break;
			}
			
			BeginScan(pWc);
		break;
		
		case WM_CLOSE:
			if (pWc && pWc->hScan) {
				// First try gracefully waiting
				if (!pWc->quitting) {
					pWc->iContinueScan = 0;
					pWc->quitting = TRUE;
					break;
				} else {
					// Force quit after specified ms
					WaitForSingleObject(pWc->hScan, 5000); // 5 second timeout if thread does not close
				}
			}
			
			if (pWc->remember_size) {
				SaveSettingDWORD(TEXT("WIN_WIDTH"), pWc->width);
				SaveSettingDWORD(TEXT("WIN_HEIGHT"), pWc->height);
			}
		case WM_DESTROY:
			PostQuitMessage(0);
		break;
		
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	
	return 0;
}


HWND CreatePreferencesDialog(HWND hParent, WindowControls *pWc) {
	TCHAR tmpbuf[256];
	HWND hWnd;
	
	const int margin = 8;
	const int w = PREFS_WINDOW_WIDTH;
	const int h = PREFS_WINDOW_HEIGHT;
	const int h_opt = 21;
	const int h_opt_total = h_opt + margin;
	const int y_info = margin+h_opt_total*4;
	const int y_http = y_info + 18;
	const int h_host = h_opt;
	const int w_host = 200;
	const int w_http = 40;
	const int w_sep = 6;
	const int w_port = 50;
	const int w_slash = 8;
	const int w_file = 100;
	const int w_apply = 70;
	const int w_autoscan = 190;
	const int w_autoscan_rate = 40;
	const int w_autoscan_sectxt = 65;
	
	pWc-> hPrefsDlg = hWnd = CreateWindowEx(
		0x00, WINDOW_CLASS_NAME_PREFS, TEXT("Preferences"),
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | DS_CENTERMOUSE | DS_SETFOREGROUND,
		CW_USEDEFAULT, CW_USEDEFAULT, w, h,
		hParent, NULL, NULL, NULL
	);
	
	// AUTO_CREATE_NICKS
	pWc->hAutoCreateNicks = CreateWindowEx(
		0x00, WC_BUTTON, TEXT("Automatically Add New Nicks"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_TEXT | BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP,
		margin, margin, w-margin*2, h_opt,
		hWnd, NULL, NULL, NULL
	);
	Button_SetCheck(pWc->hAutoCreateNicks, pWc->auto_nick_save ? BST_CHECKED : BST_UNCHECKED);
	
	// AUTO_SORT
	pWc->hRememberSort = CreateWindowEx(
		0x00, WC_BUTTON, TEXT("Remember Sort"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_TEXT | BS_AUTOCHECKBOX | WS_TABSTOP,
		margin, margin+h_opt_total, w-margin*2, h_opt,
		hWnd, NULL, NULL, NULL
	);
	Button_SetCheck(pWc->hRememberSort, pWc->remember_sort ? BST_CHECKED : BST_UNCHECKED);
	
	// AUTO_SCAN
	pWc->hAutoScan = CreateWindowEx(
		0x00, WC_BUTTON, TEXT("Automatically Scan Every"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_TEXT | BS_AUTOCHECKBOX | WS_TABSTOP,
		margin, margin+h_opt_total*2, w_autoscan, h_opt,
		hWnd, NULL, NULL, NULL
	);
	
	pWc->hAutoScanTime = CreateWindowEx(
		0x00, WC_EDIT, NULL,
		WS_VISIBLE | WS_BORDER | WS_CHILD | ES_NUMBER | WS_TABSTOP,
		margin*2+w_autoscan, margin+h_opt_total*2, w_autoscan_rate, h_opt,
		hWnd, NULL, NULL, NULL
	);
	Edit_LimitText(pWc->hAutoScanTime, 8);
	snwprintf(tmpbuf, 8, TEXT("%lu"), pWc->auto_scan_delay);
	Edit_SetText(pWc->hAutoScanTime, tmpbuf);
	
	CreateWindowEx(
		0x00, WC_STATIC, TEXT("Seconds"),
		WS_CHILD | WS_VISIBLE,
		margin*3+w_autoscan+w_autoscan_rate, margin+h_opt_total*2, w_autoscan_sectxt, h_opt,
		hWnd, NULL, NULL, NULL
	);
	
	pWc->hAutoScanTimeApply = CreateWindowEx(
		0x00, WC_BUTTON, TEXT("Apply"),
		WS_VISIBLE | WS_BORDER | WS_CHILD | WS_TABSTOP,
		margin*4+w_autoscan+w_autoscan_rate + w_autoscan_sectxt, margin+h_opt_total*2, w_apply, h_opt,
		hWnd, NULL, NULL, NULL
	);
	Button_Enable(pWc->hAutoScanTimeApply, FALSE);
	Button_SetCheck(pWc->hAutoScan, pWc->auto_scan ? BST_CHECKED : BST_UNCHECKED);
	
	// REMEMBER_POSITION
	pWc->hRememberWindow = CreateWindowEx(
		0x00, WC_BUTTON, TEXT("Remember Window Position"),
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_TEXT | BS_AUTOCHECKBOX | WS_TABSTOP,
		margin, margin+h_opt_total*3, w-margin*2, h_opt,
		hWnd, NULL, NULL, NULL
	);
	Button_SetCheck(pWc->hRememberWindow, pWc->remember_size ? BST_CHECKED : BST_UNCHECKED);
	
	// Host/port/file text headers
	CreateWindowEx(
		0x00, WC_STATIC, TEXT("Hostname or IP"),
		WS_CHILD | WS_VISIBLE,
		margin*2 + w_http, y_info, w_host, h_host,
		hWnd, NULL, NULL, NULL
	);
	
	CreateWindowEx(
		0x00, WC_STATIC, TEXT("Port"),
		WS_CHILD | WS_VISIBLE,
		margin*4 + w_http + w_host + w_sep, y_info, w_port, h_host,
		hWnd, NULL, NULL, NULL
	);
	
	CreateWindowEx(
		0x00, WC_STATIC, TEXT("JSON File"),
		WS_CHILD | WS_VISIBLE,
		margin*6 + w_http + w_host + w_sep + w_port + w_slash, y_info, w_file, h_host,
		hWnd, NULL, NULL, NULL
	);
	
	// Host/port/file edits
	CreateWindowEx(
		0x00, WC_STATIC, TEXT("http://"),
		WS_CHILD | WS_VISIBLE,
		margin, y_http, w_host, h_host,
		hWnd, NULL, NULL, NULL
	);
	
	pWc->hHostAddr = CreateWindowEx(
		0x00, WC_EDIT, NULL,
		WS_VISIBLE | WS_BORDER | WS_CHILD | ES_LEFT | ES_AUTOHSCROLL | WS_TABSTOP | WS_GROUP,
		margin*2 + w_http, y_http, w_host, h_host,
		hWnd, NULL, NULL, NULL
	);
	Edit_LimitText(pWc->hHostAddr, 64);
	Edit_SetText(pWc->hHostAddr, pWc->host_name);
	
	CreateWindowEx(
		0x00, WC_STATIC, TEXT(":"),
		WS_CHILD | WS_VISIBLE,
		margin*3 + w_http + w_host, y_http, w_sep, h_host,
		hWnd, NULL, NULL, NULL
	);
	
	pWc->hPortNum = CreateWindowEx(
		0x00, WC_EDIT, NULL,
		WS_VISIBLE | WS_BORDER | WS_CHILD | ES_LEFT | ES_NUMBER | WS_TABSTOP,
		margin*4 + w_http + w_host + w_sep, y_http, w_port, h_host,
		hWnd, NULL, NULL, NULL
	);
	Edit_LimitText(pWc->hPortNum, 5);
	snwprintf(tmpbuf, 8, TEXT("%d"), pWc->host_port);
	Edit_SetText(pWc->hPortNum, tmpbuf);
	
	CreateWindowEx(
		0x00, WC_STATIC, TEXT("/"),
		WS_CHILD | WS_VISIBLE,
		margin*5 + w_http + w_host + w_sep + w_port, y_http, w_sep, h_host,
		hWnd, NULL, NULL, NULL
	);
	
	pWc->hServFile = CreateWindowEx(
		0x00, WC_EDIT, NULL,
		WS_VISIBLE | WS_BORDER | WS_CHILD | ES_LEFT | ES_AUTOHSCROLL | WS_TABSTOP,
		margin*6 + w_http + w_host + w_sep + w_port + w_slash, y_http, w_file, h_host,
		hWnd, NULL, NULL, NULL
	);
	Edit_LimitText(pWc->hServFile, 64);
	Edit_SetText(pWc->hServFile, pWc->host_file);
	
	pWc->hHostApply = CreateWindowEx(
		0x00, WC_BUTTON, TEXT("Apply"),
		WS_VISIBLE | WS_BORDER | WS_CHILD | WS_TABSTOP,
		margin*7 + w_http + w_host + w_sep + w_port + w_slash + w_file, y_http, w_apply, h_host,
		hWnd, NULL, NULL, NULL
	);
	Button_Enable(pWc->hHostApply, FALSE);
	
	return hWnd;
}

HWND CreateRenameDialog(HWND hParent, RenameDlgControls *pRdc) {
	TCHAR tmpbuf[256];
	HWND hWnd;
	
	const int margin = 12;
	const int h_txt = 40;
	const int h_btn = 23;
	const int h_input = 21;
	const int w = RDC_WINDOW_WIDTH;
	const int h = RDC_WINDOW_HEIGHT;
	
	pRdc->hParent = hParent;
	pRdc->submit = FALSE;
	
	hWnd = CreateWindowEx(
		0x00, WINDOW_CLASS_NAME_REN, TEXT("Set Nickname"),
		WS_OVERLAPPED | DS_CENTERMOUSE | DS_SETFOREGROUND,
		CW_USEDEFAULT, CW_USEDEFAULT, w, h,
		hParent, NULL, NULL, NULL
	);
	
	pRdc->hMessage = CreateWindowEx(
		0x00, WC_STATIC, TEXT(""),
		WS_VISIBLE | WS_CHILD | SS_CENTER,
		margin, margin, w-margin*2, h_txt,
		hWnd, NULL, NULL, NULL
	);
	
	const int w_shrink = 20;
	pRdc->hInput = CreateWindowEx(
		0x00, WC_EDIT, NULL,
		WS_VISIBLE | WS_BORDER | WS_CHILD | ES_LEFT | WS_TABSTOP | WS_GROUP,
		margin+w_shrink, h_txt+margin, w-w_shrink*2-margin*2, h_input,
		hWnd, NULL, NULL, NULL
	);
	
	const int w_cancel = 120;
	pRdc->hCancel = CreateWindowEx(
		0x00, WC_BUTTON, BUTTON_TEXT_RENDLG_CANCEL,
		WS_VISIBLE | WS_BORDER | WS_CHILD | WS_TABSTOP,
		margin+w_shrink, h_txt+20+margin*2, w_cancel, h_btn,
		hWnd, NULL, NULL, NULL
	);
	
	const int w_delete = 90;
	pRdc->hDelete = CreateWindowEx(
		0x00, WC_BUTTON, BUTTON_TEXT_RENDLG_DELETE,
		WS_VISIBLE | WS_BORDER | WS_CHILD | WS_TABSTOP,
		margin*2+w_cancel+w_shrink, h_txt+20+margin*2, w_cancel, h_btn,
		hWnd, NULL, NULL, NULL
	);
	
	const int w_ok = 80;
	pRdc->hOk = CreateWindowEx(
		0x00, WC_BUTTON, BUTTON_TEXT_RENDLG_OK,
		WS_VISIBLE | WS_BORDER | WS_CHILD | BS_DEFPUSHBUTTON | WS_TABSTOP,
		w-margin-w_ok-w_shrink, h_txt+20+margin*2, w_ok, h_btn,
		hWnd, NULL, NULL, NULL
	);
	
	return hWnd;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	RenameDlgControls rdc;
	WindowControls wc;
	INITCOMMONCONTROLSEX picce;
	NOTIFYICONDATA tray;
	HICON hIconPrefs;
	HWND hWnd;
	HACCEL hAccelMain, hAccelRenDlg;
	WNDCLASSEX wndclass, wndclass_ren, wndclass_prefs;
	HICON system_icon;
	MSG Msg;
	TCHAR errmsg[256];
	HT_Table htLocalNames;
	
	// Icons
	system_icon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
	hIconPrefs = ExtractIcon(hInstance, TEXT("SHELL32.DLL"), 71);
	
	// Key shortcuts for main window
	hAccelMain = LoadAccelerators(hInstance, MAKEINTRESOURCE(2));
	if (!hAccelMain) {
		snwprintf(errmsg, 256, TEXT("Failed to load accelerator GetLastError()=%d"), GetLastError());
		MessageBox(NULL, errmsg, TEXT("LoadAccelerators Error"), MB_OK | MB_ICONERROR);
		return 1;
	}
	
	// Key shortcuts for rename dialog
	hAccelRenDlg = LoadAccelerators(hInstance, MAKEINTRESOURCE(3));
	if (!hAccelRenDlg) {
		snwprintf(errmsg, 256, TEXT("Failed to load accelerator GetLastError()=%d"), GetLastError());
		MessageBox(NULL, errmsg, TEXT("LoadAccelerators Error"), MB_OK | MB_ICONERROR);
		return 1;
	}
	
	// Hash table initialization
	htLocalNames.first = htLocalNames.last = NULL;
	
	// Values that require initialization for WindowControls
	ZeroMemory(&wc, sizeof(WindowControls));
	wc.pRdc = &rdc;
	wc.pHtLocalNames = &htLocalNames;
	SetGeneralStatus(&wc);
	
	// Initial hardcoded defaults (overwritten by settings)
	snwprintf(wc.host_name, HOSTNAME_LEN, TEXT("%ls"), HTTP_HOST);
	snwprintf(wc.host_file, HOSTNAME_LEN, TEXT("%ls"), HTTP_FILE);
	wc.width = DEFAULT_WINDOW_WIDTH;
	wc.height = DEFAULT_WINDOW_HEIGHT;
	
	// Settings
	wc.remember_size = (BOOL)LoadSettingDWORD(TEXT("REMEMBER_SIZE"), REMEMBER_SIZE);
	if (wc.remember_size) {
		wc.width = (int)LoadSettingDWORD(TEXT("WIN_WIDTH"), wc.width);
		wc.height = (int)LoadSettingDWORD(TEXT("WIN_HEIGHT"), wc.height);
	}
	wc.use_local_names = (BOOL)LoadSettingDWORD(TEXT("USE_LOCAL_NAMES"), USE_LOCAL_NAMES);
	wc.auto_nick_save = (BOOL)LoadSettingDWORD(TEXT("AUTO_CREATE_NICKS"), AUTO_CREATE_NICKS);
	wc.remember_sort = (BOOL)LoadSettingDWORD(TEXT("AUTO_SORT"), AUTO_SORT);
	wc.auto_scan = (BOOL)LoadSettingDWORD(TEXT("AUTO_SCAN"), AUTO_SCAN);
	wc.auto_scan_delay = LoadSettingDWORD(TEXT("AUTO_SCAN_DELAY"), AUTO_SCAN_DELAY);
	
	wc.last_col_sort_index = (int)LoadSettingDWORD(TEXT("LAST_SORT_INDEX"), -1);
	wc.reverse_sort = (BOOL)LoadSettingDWORD(TEXT("LAST_SORT_REVERSE"), FALSE);
	
	LoadSettingString(TEXT("HOST"), wc.host_name, HOSTNAME_LEN);
	LoadSettingString(TEXT("FILE"), wc.host_file, HOSTNAME_LEN);
	wc.host_port = LoadSettingDWORD(TEXT("PORT"), HTTP_PORT);
	
	// InitCommonControls();
	picce.dwSize = sizeof(INITCOMMONCONTROLSEX);
	picce.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&picce);
	
	ZeroMemory(&wndclass, sizeof(WNDCLASSEX));
	wndclass.cbSize = sizeof(WNDCLASSEX);
	wndclass.lpszClassName = WINDOW_CLASS_NAME;
	wndclass.lpfnWndProc = WndProc;
	wndclass.hIcon = system_icon;
	wndclass.hInstance = hInstance;
	wndclass.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	if (!RegisterClassEx(&wndclass)) {
		snwprintf(errmsg, 256, TEXT("Failed to register '%ls' window class: GetLastError()=%d"), WINDOW_CLASS_NAME, GetLastError());
		MessageBox(NULL, errmsg, TEXT("RegisterClassEx Error"), MB_OK | MB_ICONERROR);
		return 1;
	}
	
	ZeroMemory(&wndclass_ren, sizeof(WNDCLASSEX));
	wndclass_ren.cbSize = sizeof(WNDCLASSEX);
	wndclass_ren.lpszClassName = WINDOW_CLASS_NAME_REN;
	wndclass_ren.lpfnWndProc = WndProcRenDlg;
	wndclass_ren.hInstance = hInstance;
	wndclass_ren.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wndclass_ren.hCursor = LoadCursor(NULL, IDC_ARROW);
	if (!RegisterClassEx(&wndclass_ren)) {
		snwprintf(errmsg, 256, TEXT("Failed to register '%ls' window class: GetLastError()=%d"), WINDOW_CLASS_NAME_REN, GetLastError());
		MessageBox(NULL, errmsg, TEXT("RegisterClassEx Error"), MB_OK | MB_ICONERROR);
		return 1;
	}
	
	ZeroMemory(&wndclass_prefs, sizeof(WNDCLASSEX));
	wndclass_prefs.cbSize = sizeof(WNDCLASSEX);
	wndclass_prefs.lpszClassName = WINDOW_CLASS_NAME_PREFS;
	wndclass_prefs.lpfnWndProc = WndProcPrefsDlg;
	wndclass_prefs.hInstance = hInstance;
	wndclass_prefs.hIcon = hIconPrefs;
	wndclass_prefs.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wndclass_prefs.hCursor = LoadCursor(NULL, IDC_ARROW);
	if (!RegisterClassEx(&wndclass_prefs)) {
		snwprintf(errmsg, 256, TEXT("Failed to register '%ls' window class: GetLastError()=%d"), WINDOW_CLASS_NAME_PREFS, GetLastError());
		MessageBox(NULL, errmsg, TEXT("RegisterClassEx Error"), MB_OK | MB_ICONERROR);
		return 1;
	}
	
	const int window_width = DEFAULT_WINDOW_WIDTH;
	const int window_height = DEFAULT_WINDOW_HEIGHT;
	const DWORD static_style = WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP;
	
	wc.hMain = hWnd = CreateWindowEx(
		0x00, WINDOW_CLASS_NAME, DEFAULT_TITLE,
		WS_OVERLAPPED | WS_SIZEBOX | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, wc.width, wc.height,
		NULL, NULL, hInstance, NULL
	);
	
	if (hWnd == NULL) {
		snwprintf(errmsg, 256, TEXT("Failed to create main window: GetLastError()=%d"), GetLastError());
		MessageBox(hWnd, errmsg, TEXT("CreateWindowEx Error"), MB_OK | MB_ICONERROR);
		return 1;
	}
	
	// Player list
	wc.hPlayerListText = CreateWindowEx(
		0x00, WC_STATIC, STATIC_TEXT_PLAYERLIST,
		static_style,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	wc.hPlayerList = CreateWindowEx(
		0x00, WC_LISTBOX, TEXT(""),
		WS_VISIBLE | WS_BORDER | WS_CHILD | LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP | WS_GROUP,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	// Shellmap
	wc.hShellMapText = CreateWindowEx(
		0x00, WC_STATIC, STATIC_TEXT_SHELLMAP,
		static_style,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	wc.hShellMap = CreateWindowEx(
		0x00, WC_STATIC, TEXT(""),
		WS_VISIBLE | WS_BORDER | WS_CHILD | SS_CENTER,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	// Host message
	wc.hHostMessageText = CreateWindowEx(
		0x00, WC_STATIC, STATIC_TEXT_HOSTMSG,
		static_style,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	wc.hHostMessage = CreateWindowEx(
		0x00, WC_EDIT, TEXT(""),
		WS_VISIBLE | WS_BORDER | WS_CHILD | ES_MULTILINE | ES_LEFT | ES_READONLY | WS_VSCROLL,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	// Server info
	wc.hServerInfoText = CreateWindowEx(
		0x00, WC_STATIC, STATIC_TEXT_GAMESTATS,
		static_style,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	wc.hServerInfo = CreateWindowEx(
		0x00, WC_EDIT, TEXT(""),
		WS_VISIBLE | WS_BORDER | WS_CHILD | ES_MULTILINE | ES_LEFT | ES_READONLY | WS_VSCROLL,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	// Local player name checkbox
    wc.hLocalNames = CreateWindowEx(
		0x00, WC_BUTTON, BUTTON_TEXT_USELOCALNICKS,
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_TEXT | BS_AUTOCHECKBOX | WS_TABSTOP,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	Button_SetCheck(wc.hLocalNames, wc.use_local_names ? BST_CHECKED : BST_UNCHECKED);
	
	// Nick button
	wc.hSetName = CreateWindowEx(
		0x00, WC_BUTTON, BUTTON_TEXT_EDIT_NICK,
		WS_VISIBLE | WS_CHILD | WS_DISABLED | WS_TABSTOP,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	// URL
	wc.hURL = CreateWindowEx(
		0x00, WC_BUTTON, BUTTON_TEXT_STEAM_WWW,
		WS_VISIBLE | WS_CHILD | WS_DISABLED | WS_TABSTOP,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	// Join button
	wc.hJoin = CreateWindowEx(
		0x00, WC_BUTTON, BUTTON_TEXT_JOIN_GAME,
		WS_CHILD | WS_DISABLED | WS_TABSTOP,
		//~ WS_VISIBLE | WS_CHILD | WS_DISABLED | WS_TABSTOP,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	// Refresh button
    wc.hRefresh = CreateWindowEx(
		0x00, WC_BUTTON, BUTTON_TEXT_SCAN_REFRESH,
		WS_CHILD | WS_VISIBLE | BS_CENTER | BS_PUSHBUTTON | BS_TEXT | WS_TABSTOP,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	// Preferences button
    wc.hPrefs = CreateWindowEx(
		0x00, WC_BUTTON, TEXT(""),
		WS_CHILD | WS_VISIBLE | BS_CENTER | BS_ICON | BS_PUSHBUTTON | BS_TEXT | WS_TABSTOP,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	SendMessage(wc.hPrefs, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIconPrefs);
	
	// Game list table
	wc.hGameList = CreateWindowEx(
		0x00,
		WC_LISTVIEW, TEXT(""),
		WS_VISIBLE | WS_BORDER | WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_NOLABELWRAP | LVS_SHOWSELALWAYS | WS_TABSTOP,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	ListView_SetExtendedListViewStyle(wc.hGameList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
	
	LVCOLUMN lvc;
	float init_column_width_mod = (float)wc.width / (float)DEFAULT_WINDOW_WIDTH;
	lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_MINWIDTH | LVCF_ORDER | LVCF_SUBITEM;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cxMin = 16;
	for (int i=0; i<COLUMN_COUNT; i++) {
		lvc.pszText = (TCHAR*)COLUMNS[i].text;
		lvc.cchTextMax = tstrlen(COLUMNS[i].text);
		lvc.cx = (int)round((float)(COLUMNS[i].width) * init_column_width_mod);
		lvc.cxMin = 16;
		lvc.iOrder = lvc.iSubItem = i;
		ListView_InsertColumn(wc.hGameList, i, &lvc);
	}
	
	// Status bar
	wc.hStatusBar = CreateWindowEx(
		0x00,
		STATUSCLASSNAME,
		TEXT(""),
		WS_VISIBLE | WS_CHILD,
		0, 0, 0, 0,
		hWnd,
		NULL,
		hInstance,
		NULL
	);
	
	// Rename dialog
	wc.hRenameDlg = CreateRenameDialog(hWnd, &rdc);
	SetWindowLongPtr(wc.hRenameDlg, GWLP_USERDATA, (LONG_PTR)&rdc);
	
	// Preferences dialog (must be created after settings are loaded)
	if (!CreatePreferencesDialog(hWnd, &wc)) {
		snwprintf(errmsg, 256, TEXT("Failed to create preferences window."));
		MessageBox(hWnd, errmsg, TEXT("Error"), MB_OK | MB_ICONERROR);
		return 1;
	}
	SetWindowLongPtr(wc.hPrefsDlg, GWLP_USERDATA, (LONG_PTR)&wc);
	
	// Right-click tray icon menu
	UpdateContextMenu(&wc);
	
	// Tray icon setup
	ZeroMemory(&tray, sizeof(NOTIFYICONDATA));
	tray.cbSize = sizeof(NOTIFYICONDATA);
	tray.hIcon = system_icon;
	tray.hWnd = hWnd;
	tray.uID = 1;
	//~ tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_INFO;
	tray.uFlags = NIF_MESSAGE | NIF_ICON;
	tray.uCallbackMessage = WM_APP_TRAY;
	tray.hIcon = system_icon;
	tray.hBalloonIcon = system_icon;
	wc.pTray = &tray;
	snwprintf(tray.szInfo, 256, TEXT("%ls"), TEXT("TESTING"));
	//~ snwprintf(tray.szInfoTitle, 64, TEXT("%ls"), TEXT("TITLE TEST"));
	//~ Shell_NotifyIcon(NIM_ADD, &tray);
	//~ Shell_NotifyIcon(NIF_INFO, &tray);
	
	// Window
	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)&wc);
	SendMessage(hWnd, WM_EXITSIZEMOVE, 0, 0); // Required to initialize size info for resizing
	SendMessage(hWnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(wc.width, wc.height));
	SetGeneralStatus(&wc);
	wc.minimized = FALSE;
	ShowWindow(hWnd, nCmdShow);
	
	if (wc.auto_scan) {
		SendMessage(hWnd, WM_APP_REFRESH, 0, 0);
	}
	
	HACCEL hAccel;
	while (GetMessage(&Msg, NULL, 0, 0) > 0) {
		if (wc.hInDlg == wc.hRenameDlg) {
			hWnd = wc.hInDlg;
			hAccel = hAccelRenDlg;
		} else {
			hWnd = wc.hMain;
			hAccel = hAccelMain;
		}
		
		if (!TranslateAccelerator(hWnd, hAccel, &Msg) && !IsDialogMessage(hWnd, &Msg) && !IsDialogMessage(wc.hPrefsDlg, &Msg)) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
	}
	
	// Cleanup
	if (wc.minimized) {
		Shell_NotifyIcon(NIM_DELETE, wc.pTray);
	}
	
	if (hIconPrefs) {
		DestroyIcon(hIconPrefs);
	}
	
	if (wc.hMenu) {
		DestroyMenu(wc.hMenu);
	}
	
	HT_Free(&htLocalNames);
	
	return 0;
}
