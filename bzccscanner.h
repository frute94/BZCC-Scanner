#pragma once
#define UNICODE
#include <windows.h>
#include "bzcchtable.h"

// Settings loaded & saved (these are the hardcoded defaults if setting does not exist)
#define USE_LOCAL_NAMES FALSE
#define REMEMBER_SIZE FALSE
#define AUTO_CREATE_NICKS FALSE
#define AUTO_SORT TRUE
#define AUTO_SCAN FALSE
#define AUTO_SCAN_DELAY 60
#define MIN_SCAN_DELAY 1

#define HTTP_HOST TEXT("battlezone99mp.webdev.rebellion.co.uk")
#define HTTP_PORT 80
#define HTTP_FILE TEXT("lobbyServer")

#define DEFAULT_WINDOW_WIDTH 760
#define DEFAULT_WINDOW_HEIGHT 420
#define PREFS_WINDOW_WIDTH 552
#define PREFS_WINDOW_HEIGHT 200
#define RDC_WINDOW_WIDTH 400
#define RDC_WINDOW_HEIGHT 152
// ------------------

// Names
#define DEFAULT_TITLE TEXT("BZCC Game Sessions")
#define BUTTON_TEXT_EDIT_NICK TEXT("Nick")
#define BUTTON_TEXT_STEAM_WWW TEXT("Profile")
#define BUTTON_TEXT_JOIN_GAME TEXT("Join")
#define BUTTON_TEXT_SCAN_REFRESH TEXT("&Refresh")
#define BUTTON_TEXT_USELOCALNICKS TEXT("&Show Local Nicks")
#define BUTTON_TEXT_SCAN_STOP TEXT("&Stop")
#define BUTTON_TEXT_RENDLG_OK TEXT("&Set")
#define BUTTON_TEXT_RENDLG_CANCEL TEXT("&Cancel")
#define BUTTON_TEXT_RENDLG_DELETE TEXT("&Remove")
#define DEFAULT_TITLE_RENAME_DLG_NEW TEXT("Add Nickname")
#define DEFAULT_TITLE_RENAME_DLG TEXT("Change Nickname")
#define MENU_TEXT_PREFS TEXT("&Preferences")
#define MENU_TEXT_EXIT TEXT("&Exit")
#define STATIC_TEXT_PLAYERLIST TEXT(" Player List")
#define STATIC_TEXT_SHELLMAP TEXT(" Game Map")
#define STATIC_TEXT_HOSTMSG TEXT(" Host Message")
#define STATIC_TEXT_GAMESTATS TEXT(" Game Stats")
#define COLUMN_TEXT_PASSWORDED_GAME TEXT("(P)")

// Application specific messages
#define WM_APP_TRAY WM_APP+101
#define WM_APP_INIT WM_APP+102
#define WM_APP_REFRESH WM_APP+103
#define WM_APP_EXIT WM_APP+104
#define WM_APP_RENAME_DLG WM_APP+105
#define WM_APP_PREFS_DLG WM_APP+106
#define WM_APP_CLOSE_RENAME_DLG WM_APP+108
#define WM_RDC_OK WM_APP+201
#define WM_RDC_DELETE WM_APP+202
#define WM_RDC_SELALL WM_APP+203

// TODO: Implement ANSI version of snwprintf (would need to convert %ls to %s in some cases)
#ifdef UNICODE
#define tstrlen wcslen
#define tstrcmp wcscmp
#define ttoi _wtoi
#else
#define tstrlen strlen
#define tstrcmp strcmp
#define ttoi atoi
#endif

#define SERVER_VER_LEN 16
#define SERVER_NAME_LEN 64
#define SERVER_MAP_LEN 32
#define SERVER_HOSTMSG_LEN 256
#define PLAYER_NAME_LEN 64
#define USER_ID_LEN 32

#define HOSTNAME_LEN 128

#define COLUMN_COUNT 7
#define MAX_SERVERS 128
#define MAX_PLAYERS 16
#define MAX_MODS 32

typedef struct PlayerInfo {
	TCHAR local_name[PLAYER_NAME_LEN];
	TCHAR ingame_name[PLAYER_NAME_LEN]; // "n"
	TCHAR user_id[USER_ID_LEN]; // "i"
	int team; // "t"
	int kills; // "k"
	int deaths; // "d"
	int score; // "s"
	
	BOOL quitting;
	DWORD unique_identifier;
} PlayerInfo;

typedef struct GameInfo {
	TCHAR version[SERVER_VER_LEN]; // "v"
	TCHAR name[SERVER_NAME_LEN]; // "n"
	TCHAR map[SERVER_MAP_LEN]; // "m"
	TCHAR msg[SERVER_HOSTMSG_LEN]; // "h"
	BOOL pwd;
	
	unsigned long int mod_id[MAX_MODS];
	TCHAR mod_name[SERVER_NAME_LEN]; // TODO - just a string of the int for now
	
	int max_ping;
	int server_ping;
	int gametime;
	int playerlimit;
	int timelimit;
	int killlimit;
	int gametype;
	int tps;
	
	int player_count;
	
	PlayerInfo playerinfo[MAX_PLAYERS];
	
	BOOL quitting;
	DWORD unique_identifier;
} GameInfo;

typedef struct RenameDlgControls {
	BOOL submit;
	
	HWND hParent;
	HWND hMessage;
	HWND hInput;
	HWND hDelete;
	HWND hOk;
	HWND hCancel;
	
	PlayerInfo *pPlayer; // Pointer to player who's attribute is being modified.
} RenameDlgControls;

typedef struct WindowControls {
	BOOL minimized;
	HANDLE hScan; int iContinueScan; BOOL quitting;
	GameInfo gameinfo[MAX_SERVERS];
	GameInfo gameinfo_prev[MAX_SERVERS]; int gameinfo_prev_count;
	
	HWND hMain;
	HWND hInDlg; // Set to a handle if currently pending results from a pop-up dialog.
	HWND hRenameDlg;
	RenameDlgControls *pRdc;
	
	HWND hPlayerListText;
	HWND hPlayerList;
	HWND hShellMapText;
	HWND hShellMap;
	HWND hHostMessageText;
	HWND hHostMessage;
	HWND hServerInfoText;
	HWND hServerInfo;
	
	HWND hPrefsDlg;
	HWND hAutoCreateNicks;
	HWND hRememberSort;
	HWND hAutoScan;
	HWND hAutoScanTime;
	HWND hAutoScanTimeApply;
	HWND hRememberWindow;
	HWND hHostAddr;
	HWND hPortNum;
	HWND hServFile;
	HWND hHostApply;
	
	HWND hLocalNames;
	HWND hSetName;
	HWND hURL;
	HWND hJoin;
	HWND hRefresh;
	HWND hPrefs;
	
	HWND hGameList;
	
	// ---
	int last_col_sort_index;
	BOOL reverse_sort;
	BOOL remember_sort;
	
	BOOL use_local_names;
	
	BOOL remember_size;
	int width, height, column_width[COLUMN_COUNT];
	
	BOOL auto_nick_save;
	
	BOOL auto_scan_request_while_busy;
	BOOL auto_scan;
	DWORD auto_scan_delay;
	UINT timer_id;
	
	TCHAR host_name[HOSTNAME_LEN];
	DWORD host_port;
	TCHAR host_file[HOSTNAME_LEN];
	
	GameInfo *pSelectedGameInfo;
	PlayerInfo *pSelectedPlayerInfo;
	
	int scan_count_servers;
	GameInfo *scan_game;
	int scan_count_players;
	PlayerInfo *scan_player;
	
	HWND hStatusBar;
	int total_player_count;
	DWORD64 timestamp;
	float timestamp_result;
	
	HT_Table *pHtLocalNames;
	HMENU hMenu;
	NOTIFYICONDATA *pTray;
} WindowControls;

// Characters are not unicode because they are bytes from network/file
typedef struct KeyToIdentifier {const char *str;int id;} KeyToIdentifier;

#define PLAYERKEY_NAME 1
#define PLAYERKEY_USERID 2
#define PLAYERKEY_TEAM 3
#define PLAYERKEY_KILLS 4
#define PLAYERKEY_SCORE 5
#define PLAYERKEY_DEATHS 6
const KeyToIdentifier PLAYER_KEY_TO_ID[] = {
	{"n", PLAYERKEY_NAME},
	{"i", PLAYERKEY_USERID},
	{"t", PLAYERKEY_TEAM},
	{"k", PLAYERKEY_KILLS},
	{"s", PLAYERKEY_SCORE},
	{"d", PLAYERKEY_DEATHS},
	{"", 0} // Null-terminator
};

#define SERVERKEY_NAME 1
#define SERVERKEY_MAP 2
#define SERVERKEY_VERSION 3
#define SERVERKEY_HOSTMSG 4
#define SERVERKEY_PLAYERLIMIT 5
#define SERVERKEY_MAX_PING 6
#define SERVERKEY_MODSTRING 7
#define SERVERKEY_TPS 8
#define SERVERKEY_PWD 9
#define SERVERKEY_GAMETIME 10
#define SERVERKEY_GAMETYPE 11
#define SERVERKEY_TIMELIMIT 12
#define SERVERKEY_KILLLIMIT 13
#define SERVERKEY_PING 14
#define SERVERKEY_G 15
const KeyToIdentifier SERVER_KEY_TO_ID[] = {
	{"n", SERVERKEY_NAME},
	{"m", SERVERKEY_MAP},
	{"v", SERVERKEY_VERSION},
	{"gtm", SERVERKEY_GAMETIME},
	{"mu", 0}, // ?
	{"gt", SERVERKEY_GAMETYPE},
	{"gtd", 0}, // ? seems to be some kind of bitfield
	{"pg", SERVERKEY_PING}, // Ping to master server?
	{"pgm", SERVERKEY_MAX_PING},
	{"t", 0}, // ?
	{"mm", SERVERKEY_MODSTRING},
	{"pm", SERVERKEY_PLAYERLIMIT},
	{"tps", SERVERKEY_TPS},
	{"si", 0}, // ?
	{"h", SERVERKEY_HOSTMSG},
	{"k", SERVERKEY_PWD},
	{"ti", SERVERKEY_TIMELIMIT},
	{"ki", SERVERKEY_KILLLIMIT},
	{"g", SERVERKEY_G}, // Some kind of crypto challenge? Seems to be unique
	{"", 0} // Null-terminator
};

typedef struct GameTypeList {
	int id;
	const TCHAR *description;
} GameTypeList;

const GameTypeList GAMETYPES[] = {
	{0, TEXT("Unknown")},
	{1, TEXT("Deathmatch")},
	{2, TEXT("Strategy")},
	{2, NULL}
};

#define COL_GAMENAME 0
#define COL_PASSWORD 1
#define COL_PING 2
#define COL_PLAYERS 3
#define COL_MAP 4
#define COL_GAMETYPE 5
#define COL_GAMEMOD 6
typedef struct ColumnDescription {
	const TCHAR *text;
	int width;
} ColumnDescription;

// Values based on DEFAULT_WINDOW_WIDTH & HEIGHT
const ColumnDescription COLUMNS[COLUMN_COUNT] = {
	{TEXT("Game Name"), 196},
	{TEXT("PWD"), 48},
	{TEXT("Ping"), 64},
	{TEXT("Players"), 74},
	{TEXT("Map Name"), 96},
	{TEXT("Game Type"), 78},
	{TEXT("Mod Name"), 76}
};

#define WINDOW_CLASS_NAME TEXT("BZCCSessionScanner")
#define WINDOW_CLASS_NAME_REN TEXT("BZCCRenameDlg")
#define WINDOW_CLASS_NAME_PREFS TEXT("BZCCPrefsDlg")

DWORD64 GetTimestamp();

void SetGeneralStatus(WindowControls *pWc);
void SetPlayerStatus(WindowControls *pWc);

int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
DWORD WINAPI ScanThread(LPVOID void_pWc);

void UpdateContextMenu(WindowControls *pWc);
int EndScan(WindowControls *pWc);
int BeginScan(WindowControls *pWc);

BOOL GameInfoToRow(HWND hLV, GameInfo *pGameInfo);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
