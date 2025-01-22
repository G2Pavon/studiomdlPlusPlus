#pragma once

#include "cmdlib.hpp"
#include <string>

extern bool end_of_qc_file;
extern char *qc_script_buffer;

void load_qc_file(char *filename);

bool get_token(bool crossline, std::string &token);
bool token_available(void);
