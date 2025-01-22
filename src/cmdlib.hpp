#pragma once

#include <cstdio>

extern bool g_archive;

int file_length(FILE *f);
int file_time(char *path);

void q_mkdir(char *path);

extern char qdir[1024];
char *expand_path(char *path); // from scripts
char *expand_path_and_archive(char *path);

void error(char *error, ...);

FILE *safe_open_write(char *filename);
FILE *safe_open_read(char *filename);
void safe_read(FILE *f, void *buffer, int count);
void safe_write(FILE *f, void *buffer, int count);

int load_file(char *filename, void **bufferptr);
void save_file(char *filename, void *buffer, int count);

void strip_extension(char *path);

void extract_filebase(char *path, char *dest);

void create_path(char *path);
void q_copy_file(char *from, char *to);

extern bool g_archive;
extern char g_archivedir[1024];