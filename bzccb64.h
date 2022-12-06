#pragma once

int _b64decode(const char *b64, char *output, int output_size);

// Writes BAD_B64 to output on failure
#define BAD_B64 "B64 DECODING ERROR"
int b64decode(const char *b64, char *output, int output_size);
