#include "bzccnet.h"
#include <stdio.h>
#include <string.h>

//~ #define DATA_DBG_PRINT

BOOL TCP_to_BRI(BufferedReaderInfo *bri, KeyValueInfo *pKvi) {
	bri->received = recv(bri->ConnectSocket, bri->buffer, RECV_BUFLEN, 0);
	
	if (bri->received == SOCKET_ERROR) {
		int e = WSAGetLastError();
		switch (e) {
			case WSAETIMEDOUT:
				snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("WinSock: 'recv' connection timed out (Socket Error %d)\n"), e);
			break;
			
			default:
				snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("WinSock: 'recv' Socket Error %d\n"), e);
		}
		return FALSE;
	}
	
	if (bri->received < 0) {
		snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("WinSock: Unknown Socket Error ('recv' return value < 0 but not SOCKET_ERROR)\n"));
		return FALSE;
	}
	
	return TRUE; // bri->received will indicate how much data has been put into bri->buffer
}

BOOL Read_to_BRI(BufferedReaderInfo *bri, KeyValueInfo *pKvi) {
	if (!ReadFile(bri->hFile, bri->buffer, RECV_BUFLEN, &bri->received, NULL)) {
		snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("WINAPI: 'ReadFile' Error %d\n"), GetLastError());
		return FALSE;
	}
	
	return TRUE; // bri->received will indicate how much data has been put into bri->buffer
}

#define JSON_WORD_WHITELIST ".-!@#$%^&*+=\0"
BOOL is_word_char(const char c) {
	if (isalnum(c)) {
		return TRUE;
	}
	
	for (int i=0; JSON_WORD_WHITELIST[i]; i+=1) {
		if (c == JSON_WORD_WHITELIST[i]) {
			return TRUE;
		}
	}
	
	return FALSE;
}

#define READING_HTTP_HEADER 0
#define READING_HTTP_HEADER_NAME 1
#define READING_HTTP_HEADER_VALUE 2
#define READING_JSON 3
#define EXPECT_ROOT_START 0 // Expect '{' only.
#define EXPECT_KEY_NAME 1 // Names will only be quoted string values.
#define EXPECT_KEY_SEP 2 // Expect ':' after name string ends.
#define EXPECT_VALUE 3 // Expect start of array, object, non-string value, or string value.
#define EXPECT_READ_VALUE 4 // Value is string/number/bool/null, not an object or array.
#define EXPECT_NEXT 5 // Expect control character, such as next value indicator ',', array/object end, etc... 
#define EXPECT_ROOT_END 6 // Expect '}' only.
int parse_data(
	BufferedReaderInfo *bri,
	BOOL using_http,
	int (*ProcessDataCallback)(KeyValueInfo*, void*),
	void *pCustomData,
	KeyValueInfo *pKvi
) {
	char name[GENERIC_BUFLEN], value[GENERIC_BUFLEN], c;
	DWORD content_length;
	int name_len, value_len;
	int state, j_state, depth;
	int server_slot, player_slot;
	char json_name[JSON_NAME_LEN][MAX_DEPTH];
	BOOL is_array[MAX_DEPTH], in_quote, value_complete;
	BOOL in_escape;
	DWORD json_len;
	
	state = using_http ? READING_HTTP_HEADER : READING_JSON;
	name_len = value_len = 0;
	ZeroMemory(name, GENERIC_BUFLEN);
	ZeroMemory(value, GENERIC_BUFLEN);
	content_length = 0;
	
	j_state = EXPECT_ROOT_START;
	in_quote = value_complete = in_escape = FALSE;
	ZeroMemory(json_name, JSON_NAME_LEN*MAX_DEPTH);
	depth = 0;
	json_len = 0;
	
	server_slot = player_slot = 0;
	ZeroMemory(pKvi, sizeof(KeyValueInfo));
	
    while(1) {
		if (!using_http) {
			if (!Read_to_BRI(bri, pKvi)) {
				return 1;
			}
		} else {
			if (!TCP_to_BRI(bri, pKvi)) {
				return 1;
			}
		}
		
        if (bri->received == 0) {
			// Graceful end of strema
			break;
		}
		
		for (int i=0; i<bri->received; i+=1) {
			c = bri->buffer[i];
			
			if (state == READING_JSON) {
				json_len += 1;
				if (content_length && json_len >= content_length) {
					return 0;
				}
				
				if (j_state == EXPECT_ROOT_START) {
					if (c == '{') {
						#ifdef DATA_DBG_PRINT
						printf("Root object found.\n{\n");
						#endif
						j_state = EXPECT_KEY_NAME;
						is_array[depth] = FALSE;
						depth += 1;
						continue;
					}
					
					if (isspace(c)) {
						continue;
					}
					
					snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Expected start of root object, not '%c' at position %d"), c, json_len);
					return 1;
				}
				
				if (j_state == EXPECT_KEY_NAME) {
					if (in_quote) {
						if (c == '"') {
							in_quote = FALSE;
							j_state = EXPECT_KEY_SEP;
							continue;
						}
						
						if (c == '\n' || c == '\r') {
							snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Unexpected line break at position %d"), json_len);
							return 1;
						}
						
						if (name_len+1 >= GENERIC_BUFLEN) {
							snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Buffer Overflow - JSON key name > buffer size (%d bytes)"), GENERIC_BUFLEN);
							return 1;
						}
						
						name[name_len] = c;
						name_len += 1;
						continue;
					}
					
					if (c == '"') {
						in_quote = TRUE;
						continue;
					}
					
					if (isspace(c)) {
						continue;
					}
					
					if (c == '}') {
						j_state = EXPECT_NEXT; // Execution continues below to (j_state == EXPECT_NEXT)...
					} else {
						snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Expected start of key name, not '%c' at position %d"), c, json_len);
						return 1;
					}
				}
				
				if (j_state == EXPECT_KEY_SEP) {
					if (c == ':') {
						j_state = EXPECT_VALUE;
						continue;
					}
					
					if (isspace(c)) {
						continue;
					}
					
					snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Expected ':', not '%c' at position %d"), c, json_len);
					return 1;
				}
				
				if (j_state == EXPECT_VALUE) {
					if (c == '"') {
						in_quote = TRUE;
						value_complete = FALSE;
						j_state = EXPECT_READ_VALUE;
						continue;
					}
					
					if (is_word_char(c)) {
						value_complete = FALSE;
						value[value_len] = c;
						value_len += 1;
						j_state = EXPECT_READ_VALUE;
						continue;
					}
					
					if (c == '[' || c == '{') {
						// Start of array or object
						if (c == '[') {
							j_state = EXPECT_VALUE;
							is_array[depth] = TRUE;
							#ifdef DATA_DBG_PRINT
							printf("%d [\n", depth);
							#endif
						} else {
							j_state = EXPECT_KEY_NAME;
							is_array[depth] = FALSE;
							#ifdef DATA_DBG_PRINT
							printf("%d {\n", depth);
							#endif
						}
						
						if (depth+1 >= MAX_DEPTH) {
							snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Exceeded JSON object depth (%d) at position %d"), MAX_DEPTH, json_len);
							return 1;
						}
						
						memmove((void*)json_name[depth], (const void*)name, JSON_NAME_LEN);
						depth += 1;
						ZeroMemory(name, name_len);
						name_len = 0;
						
						continue;
					}
					
					if (isspace(c)) {
						continue;
					}
					
					if (c == ',' || c == '}' || c == ']') {
						j_state = EXPECT_NEXT; // Execution continues below to (j_state == EXPECT_NEXT)...
					} else {
						snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Expected start of value, array, or object, not '%c' at position %d"), c, json_len);
						return 1;
					}
				}
				
				if (j_state == EXPECT_READ_VALUE) {
					if (in_quote) {
						if (!in_escape) {
							if (c == '"') {
								in_quote = FALSE;
								value_complete = TRUE;
								pKvi->status = KVI_STATUS_NORMAL;
								pKvi->server_index = server_slot;
								pKvi->player_index = player_slot;
								strncpy(pKvi->key_str, name, 32);
								strncpy(pKvi->value_str, value, 512);
								
								if (depth == 3) {
									pKvi->is_player = FALSE;
									if (ProcessDataCallback(pKvi, pCustomData) != 0) {
										return 1;
									}
								} else {
									if (depth == 5) {
										pKvi->is_player = TRUE;
										if (ProcessDataCallback(pKvi, pCustomData) != 0) {
											return 1;
										}
									}
								}
								
								ZeroMemory(name, name_len);
								ZeroMemory(value, value_len);
								name_len = value_len = 0;
								
								j_state = EXPECT_NEXT;
								continue;
							}
							
							if (c == '\n' || c == '\r') {
								snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Unexpected line break at position %d"), json_len);
								return 1;
							}
							
							if (c == '\\') {
								in_escape = TRUE;
								continue;
							}
						} else {
							switch(c) {
							// as-is cases
							case '"':
							case '\\':
							break;
							
							case 'r':
								c = '\r';
							break;
							
							case 'n':
								c = '\n';
							break;
							
							case 't':
								c = '\t';
							break;
							
							default:
								snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Invalid escape sequence \\%c at position %d"), c, json_len);
								return 1;
							}
							in_escape = FALSE;
						}
						
						if (value_len+1 >= GENERIC_BUFLEN) {
							snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Buffer Overflow - JSON value > buffer size (%d bytes)"), GENERIC_BUFLEN);
							return 1;
						}
						
						value[value_len] = c;
						value_len += 1;
						continue;
					}
					
					if (is_word_char(c)) {
						if (value_len+1 >= GENERIC_BUFLEN) {
							snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Buffer Overflow - JSON value > buffer size (%d bytes)"), GENERIC_BUFLEN);
							return 1;
						}
						
						value[value_len] = c;
						value_len += 1;
						continue;
					}
					
					if (isspace(c)) {
						// non-string value broken by whitespace
						if (value_len > 0) {
							value_complete = TRUE;
						}
						
						continue;
					}
					
					if (c == ',' || c == '}' || c == ']') {
						value_complete = TRUE;
						pKvi->status = c == '}' ? KVI_STATUS_LAST : KVI_STATUS_NORMAL;
						pKvi->server_index = server_slot;
						pKvi->player_index = player_slot;
						strncpy(pKvi->key_str, name, 32);
						strncpy(pKvi->value_str, value, 512);
						
						if (depth == 3) {
							pKvi->is_player = FALSE;
							if (ProcessDataCallback(pKvi, pCustomData) != 0) {
								return 1;
							}
						} else {
							if (depth == 5) {
								pKvi->is_player = TRUE;
								if (ProcessDataCallback(pKvi, pCustomData) != 0) {
									return 1;
								}
							}
						}
						
						ZeroMemory(name, name_len);
						ZeroMemory(value, value_len);
						name_len = value_len = 0;
						
						j_state = EXPECT_NEXT; // Execution continues below to (j_state == EXPECT_NEXT)...
					} else {
						snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Unexpected Symbol '%c' reading value \"%s\" at position %d"), c, value, json_len);
						return 1;
					}
				}
				
				if (j_state == EXPECT_NEXT) {
					if (is_array[depth-1]) { // Assuming depth >= 1
						if (c == ',') {
							ZeroMemory(name, name_len);
							name_len = 0;
							j_state = EXPECT_VALUE;
							continue;
						}
						
						if (c == ']') {
							depth -= 1;
							#ifdef DATA_DBG_PRINT
							printf("]\n");
							#endif
							if (depth <= 0) {
								j_state = EXPECT_ROOT_END;
							} else {
								j_state = is_array[depth] ? EXPECT_VALUE : EXPECT_KEY_NAME;
							}
							continue;
						}
					} else {
						if (c == ',') {
							ZeroMemory(name, name_len);
							name_len = 0;
							ZeroMemory(value, value_len);
							name_len = value_len = 0;
							j_state = EXPECT_KEY_NAME;
							continue;
						}
						
						if (c == '}') {
							if (depth == 3) {
								// unnamed objects found at this depth level are assumed to be server info JSON objects
								server_slot += 1;
								player_slot = 0;
							} else {
								if (depth == 5) {
									player_slot += 1;
								}
							}
							
							depth -= 1;
							#ifdef DATA_DBG_PRINT
							printf("}\n");
							#endif
							if (depth <= 0) {
								j_state = EXPECT_ROOT_END;
							} else {
								j_state = is_array[depth-1] ? EXPECT_VALUE : EXPECT_KEY_NAME;
							}
							continue;
						}
					}
					
					if (isspace(c)) {
						continue;
					}
				}
				
				if (j_state == EXPECT_ROOT_END) {
					if (isspace(c)) {
						continue;
					}
					
					if (c == '}') {
						#ifdef DATA_DBG_PRINT
						printf("} (root)\n");
						#endif
						// Done, no more data expected.
						return 0;
					}
					
					snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Expected end of JSON '}' but got '%c' at position %d"), c, json_len);
					return 1;
				}
				
				snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("JSON: Unexpected Symbol '%c' at position %d"), c, json_len);
				return 1;
			}
			
			if (state == READING_HTTP_HEADER) {
				if (name_len > 0 && c == '\n' && name[name_len-1] == '\r') {
					state = READING_HTTP_HEADER_NAME;
					name[name_len-1] = '\0';
					if (strcmpi(name + strlen(name) - 6, "200 OK")) {
						snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("HTTP: Expected 200 OK but got '%s'"), name);
						return 1;
					}
					
					#ifdef DATA_DBG_PRINT
					printf(">%s\n", name);
					#endif
					ZeroMemory(name, name_len);
					name_len = 0;
					continue;
				}
				
				if (name_len+1 >= GENERIC_BUFLEN) {
					snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("HTTP: Buffer Overflow - HTTP Header Name > buffer size (%d bytes)"), GENERIC_BUFLEN);
					return 1;
				}
				
				name[name_len] = c;
				name_len += 1;
				continue;
			}
			
			if (state == READING_HTTP_HEADER_NAME) {
				if (c == '\r') {
					if (name_len+1 >= GENERIC_BUFLEN) {
						snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("HTTP: Buffer Overflow - HTTP Header Name > buffer size (%d bytes)"), GENERIC_BUFLEN);
						return 1;
					}
					
					name[name_len] = c;
					name_len += 1;
					continue;
				}
				
				if (name_len > 0 && c == '\n' && name[name_len-1] == '\r') {
					state = READING_JSON;
					ZeroMemory(name, name_len);
					name_len = 0;
					continue;
				}
				
				if (isspace(c) && name_len == 0) {
					continue;
				}
				
				if (c == ':') {
					state = READING_HTTP_HEADER_VALUE;
					continue;
				}
				
				if (name_len+1 >= GENERIC_BUFLEN) {
					snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("HTTP: Buffer Overflow - HTTP Header Name > buffer size (%d bytes)"), GENERIC_BUFLEN);
					return 1;
				}
				
				name[name_len] = c;
				name_len += 1;
				continue;
			}
			
			if (state == READING_HTTP_HEADER_VALUE) {
				if (value_len > 0 && c == '\n' && value[value_len-1] == '\r') {
					state = READING_HTTP_HEADER_NAME;
					value[value_len-1] = '\0';
					if (strcmpi(name, "Content-Length") == 0) {
						content_length = atoi(value); // Is 0 if error
					}
					
					#ifdef DATA_DBG_PRINT
					printf(">%s: %s\n", name, value);
					#endif
					ZeroMemory(value, value_len);
					ZeroMemory(name, name_len);
					value_len = name_len = 0;
					continue;
				}
				
				if (isspace(c) && value_len == 0) {
					continue;
				}
				
				if (value_len+1 >= GENERIC_BUFLEN) {
					snwprintf(pKvi->err, ERRMSG_BUFLEN, TEXT("HTTP: Buffer Overflow - HTTP Header Value > buffer size (%d bytes)"), GENERIC_BUFLEN);
					return 1;
				}
				
				value[value_len] = c;
				value_len += 1;
				continue;
			}
		}
    }
	
	return 0; // Graceful end
}

int ScanHTTP(const char *host, const char *port, const char *server_file, int timeoutms, int (*ProcessDataCallback)(KeyValueInfo*, void*), void *pCustomData) {
	KeyValueInfo kvi;
	char http[512];
	BufferedReaderInfo bri;
	TCHAR errormsg[ERRMSG_BUFLEN];
	WSADATA wsaData;
	SOCKET ConnectSocket;
	struct addrinfo hints, *result, *ptr;
	int i, state;
	
	snprintf(
		http,
		512,
		"GET /%s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"User-Agent: %s\r\n"
		"Accept-Encoding: gzip, deflate\r\n"
		"Accept: */*\r\n"
		"Connection: keep-alive\r\n\r\n"
		,
		server_file,
		host,
		USER_AGENT
	);
	
	ZeroMemory(&bri, sizeof(bri));
	bri.hFile = INVALID_HANDLE_VALUE;
	printf("Connecting to %s:%s/%s...\n", host, port, server_file);
	
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		snwprintf(kvi.err, ERRMSG_BUFLEN, TEXT("WinSock: Failed to initialize with error %d"), WSAGetLastError());
		kvi.status = KVI_STATUS_ERROR;
		ProcessDataCallback(&kvi, pCustomData);
		return 1;
	}
	
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	
	i = getaddrinfo(host, port, &hints, &result);
	if (i != 0) {
		snwprintf(kvi.err, ERRMSG_BUFLEN, TEXT("WinSock: 'getaddrinfo' call failed with error %d"), i);
		WSACleanup();
		kvi.status = KVI_STATUS_ERROR;
		ProcessDataCallback(&kvi, pCustomData);
		return 1;
	}
	
	for(ptr=result; ptr != NULL; ptr=ptr->ai_next) {
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			snwprintf(kvi.err, ERRMSG_BUFLEN, TEXT("WinSock: 'socket' call failed with error %d"), WSAGetLastError());
			freeaddrinfo(result);
			WSACleanup();
			kvi.status = KVI_STATUS_ERROR;
			ProcessDataCallback(&kvi, pCustomData);
			return 1;
		}
		
		if (connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		
		break; // successfully connected
	}
	
	int recvtimeout = timeoutms; // RECV timeout in milliseconds
	setsockopt(ConnectSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvtimeout, sizeof(recvtimeout));
	freeaddrinfo(result);
	
	if (ConnectSocket == INVALID_SOCKET) {
		snwprintf(kvi.err, ERRMSG_BUFLEN, TEXT("WinSock: Failed to connect to host %s:%s"), host, port);
		WSACleanup();
		kvi.status = KVI_STATUS_ERROR;
		ProcessDataCallback(&kvi, pCustomData);
		return 1;
	}
	
	// Send GET request
	for (int i=0; i<(int)strlen(http); i=send(ConnectSocket, http, (int)strlen(http)-i, 0)) {
		if (i == SOCKET_ERROR) {
			snwprintf(kvi.err, ERRMSG_BUFLEN, TEXT("WinSock: 'send' call failed with error %d"), WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			kvi.status = KVI_STATUS_ERROR;
			ProcessDataCallback(&kvi, pCustomData);
			return 1;
		}
	}
	
	bri.ConnectSocket = ConnectSocket;
	
	i = parse_data(&bri, TRUE, ProcessDataCallback, pCustomData, &kvi);
	closesocket(bri.ConnectSocket);
	WSACleanup();
	
	kvi.status = i == 0 ? KVI_STATUS_COMPLETE : KVI_STATUS_ERROR;
	ProcessDataCallback(&kvi, pCustomData);
	
	return i;
}

int ScanFile(const TCHAR *filepath, int (*ProcessDataCallback)(KeyValueInfo*, void*), void *pCustomData) {
	KeyValueInfo kvi;
	BufferedReaderInfo bri;
	TCHAR errormsg[ERRMSG_BUFLEN];
	int i, state;
	
	ZeroMemory(&bri, sizeof(bri));
	// printf("File \"%ls\"...\n", filepath);
	bri.hFile = CreateFile(
		filepath, // Path to file
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL, // Security Attributes Pointer
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	
	if (bri.hFile == INVALID_HANDLE_VALUE) {
		snwprintf(kvi.err, ERRMSG_BUFLEN, TEXT("WINAPI: CreateFile failed with error %d"), GetLastError());
		kvi.status = KVI_STATUS_ERROR;
		ProcessDataCallback(&kvi, pCustomData);
		return 1;
	}
	
	i = parse_data(&bri, FALSE, ProcessDataCallback, pCustomData, &kvi);
	CloseHandle(bri.hFile);
	
	kvi.status = i == 0 ? KVI_STATUS_COMPLETE : KVI_STATUS_ERROR;
	ProcessDataCallback(&kvi, pCustomData);
	
	return i;
}

// ScanFile(TEXT("./lobbyServer"), PrintKeyValTest, NULL, NULL);
// ScanHTTP("127.0.0.1", "8000", "lobbyServer", PrintKeyValTest, NULL, NULL);
// ScanHTTP("battlezone99mp.webdev.rebellion.co.uk", "80", "lobbyServer", PrintKeyValTest, NULL, NULL);
