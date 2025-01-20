// tokenizer.h
#pragma once

#include "cmdlib.hpp"

constexpr int MAXTOKEN = 512;

extern bool g_endofscript;
extern char* script_buffer;

void LoadScriptFile(char *filename);

bool GetToken(bool crossline, char* token);
bool TokenAvailable(void);