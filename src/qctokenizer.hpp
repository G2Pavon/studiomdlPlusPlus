// tokenizer.h
#pragma once

#include "cmdlib.hpp"

constexpr int MAXTOKEN = 512;

bool g_endofscript;
char *script_buffer;

void LoadScriptFile(char *filename);

bool GetToken(bool crossline, char* token); // Modified
bool TokenAvailable(void);