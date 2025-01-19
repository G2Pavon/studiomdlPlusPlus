// scriplib.h
#pragma once

#include "cmdlib.h"

constexpr int MAXTOKEN = 512;

extern char *script_p;
extern int g_scriptline;
extern bool g_endofscript;

void LoadScriptFile(char *filename);

bool GetToken(bool crossline,char* token); // Modified
bool TokenAvailable(void);