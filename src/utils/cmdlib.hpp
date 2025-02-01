#pragma once

#include <cstdio>
#include <string>

int file_length(FILE *f);
void error(char *error, ...);

FILE *safe_open_write(char *filename);
void safe_read(FILE *f, void *buffer, int count);
void safe_write(FILE *f, void *buffer, int count);

int load_file(char *filename, void **bufferptr);

std::string strip_extension(const std::string& filename);