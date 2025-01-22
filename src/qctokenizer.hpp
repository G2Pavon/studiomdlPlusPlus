#pragma once

#include "cmdlib.hpp"
#include <string>

extern bool end_of_qc_file;
extern char *qc_script_buffer;

void LoadScriptFile(char *filename);

bool GetToken(bool crossline, std::string &token);
bool TokenAvailable(void);
