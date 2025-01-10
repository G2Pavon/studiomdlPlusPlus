#pragma once

#include <cstdio>

extern bool archive;

typedef unsigned char byte;

int filelength(FILE *f);
int FileTime(char *path);

void Q_mkdir(char *path);

extern char qdir[1024];
char *ExpandPath(char *path); // from scripts
char *ExpandPathAndArchive(char *path);

void Error(char *error, ...);

FILE *SafeOpenWrite(char *filename);
FILE *SafeOpenRead(char *filename);
void SafeRead(FILE *f, void *buffer, int count);
void SafeWrite(FILE *f, void *buffer, int count);

int LoadFile(char *filename, void **bufferptr);
void SaveFile(char *filename, void *buffer, int count);

void StripExtension(char *path);

void ExtractFileBase(char *path, char *dest);

void CreatePath(char *path);
void QCopyFile(char *from, char *to);

extern bool archive;
extern char archivedir[1024];