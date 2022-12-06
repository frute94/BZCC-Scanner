#include "bzccb64.h"
#include <string.h>

// Returns string length or 0 on failure
int _b64decode(const char *b64, char *output, int output_size) {
	const char B64index[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 63, 62, 62, 63, 52, 53, 54, 55,
	56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6,
	7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0,
	0, 0, 0, 63, 0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};
	int b64_len, pad, out_len, len;
	
	b64_len = strlen(b64);
	if (!b64_len || b64_len % 4) {
		output[0] = 0; // invalid b64
		return 0;
	}
	
	pad = 0;
	for (int i=b64_len-1; i>0; i--) {
		if (b64[i] != '=') {
			break;
		}
		pad += 1;
	}
	
	out_len = ((b64_len + 4) / 4 - pad) * 4;
	len = 0;
	
	for (int i=0; i<out_len; i+=4) {
		int n = B64index[b64[i]] << 18 | B64index[b64[i + 1]] << 12 | B64index[b64[i + 2]] << 6 | B64index[b64[i + 3]];
		if (len + 3 >= output_size) {
			output[len-1] = '\0';
			return 0; // buffer overflow
		}
		output[len++] = n >> 16;
		output[len++] = n >> 8 & 0xFF;
		output[len++] = n & 0xFF;
	}
	
	if (len + 1 >= output_size) {
		output[len-1] = '\0';
		return 0; // buffer overflow
	}
	
	for (int i=len; i<output_size; i++) {
		output[i] = '\0';
	}
	
	return len; // return value is 0 on failure
}

// Sets output string to failure string on failure instead of 0-length string
int b64decode(const char *b64, char *output, int output_size) {
	const int maxlen = strlen(BAD_B64);
	int result = _b64decode(b64, output, output_size);
	if (!result) {
		for (int i=0; i<maxlen; i++) {
			if (i+1 >= output_size) {
				return 0;
			}
			output[i] = BAD_B64[i];
		}
		output[maxlen] = 0;
	}
	
	return result;
}
