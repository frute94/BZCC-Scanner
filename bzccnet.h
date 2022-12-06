#pragma once

#define UNICODE
#include <winsock2.h> // Must be included before windows.h
#include <ws2tcpip.h>
#include <windows.h>

#define USER_AGENT "BZCC Scanner 1.0"
#define RECV_BUFLEN 1024
#define GENERIC_BUFLEN 1024 // "mm" mod list string can often be over 300 bytes
#define ERRMSG_BUFLEN 512

#define MAX_DEPTH 6
#define JSON_NAME_LEN 8

#define KVI_STATUS_NORMAL 0
#define KVI_STATUS_LAST 1
#define KVI_STATUS_COMPLETE 2
#define KVI_STATUS_ERROR 3
typedef struct KeyValueInfo {
	// Status Values: 
	// KVI_STATUS_NORMAL: New data (both key & value strings) received
	// KVI_STATUS_LAST: New data received is the last key & value for the block
	// KVI_STATUS_COMPLETE: No more data, parsing completed gracefully.
	// KVI_STATUS_ERROR: Parsing canceled or error occurred. A descirption of this error will be written to err.
	int status;
	TCHAR err[ERRMSG_BUFLEN];
	
	BOOL is_player;
	char key_str[32];
	char value_str[512];
	int server_index;
	int player_index;
} KeyValueInfo;

typedef struct BufferedReaderInfo {
	WSADATA wsaData;
	SOCKET ConnectSocket;
	HANDLE hFile;
	
	char buffer[RECV_BUFLEN];
	DWORD received;
} BufferedReaderInfo;

// As data is received in buffers, once a key-value pair is establed from JSON data, the callback function ProcessDataCallback
// is called. Passed into it is 2 pointers: A pointer to a KeyValueInfo struct about the info being processed, and a custom
// user-provided pointer that can be used in the function.
// If callback function returns non-zero processing will cease.
int ScanFile(const TCHAR *filepath, int (*ProcessDataCallback)(KeyValueInfo*, void*), void *pCustomData);
int ScanHTTP(const char *host, const char *port, const char *file, int timeoutms, int (*ProcessDataCallback)(KeyValueInfo*, void*), void *pCustomData);
