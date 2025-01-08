#ifndef __CMDLIB__
#define __CMDLIB__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

typedef enum
{
	false,
	true
} qboolean;

typedef unsigned char byte;

// the dec offsetof macro doesn't work very well...
#define myoffsetof(type, identifier) ((size_t) & ((type *)0)->identifier)

// set these before calling CheckParm
extern int myargc;
extern char **myargv;

int filelength(FILE *f);
int FileTime(char *path);

void Q_mkdir(char *path);

extern char qdir[1024];
extern char gamedir[1024];
char *ExpandPath(char *path); // from scripts
char *ExpandPathAndArchive(char *path);

void Error(char *error, ...);

FILE *SafeOpenWrite(char *filename);
FILE *SafeOpenRead(char *filename);
void SafeRead(FILE *f, void *buffer, int count);
void SafeWrite(FILE *f, void *buffer, int count);

int LoadFile(char *filename, void **bufferptr);
void SaveFile(char *filename, void *buffer, int count);

void DefaultExtension(char *path, char *extension);
void StripExtension(char *path);

void ExtractFileBase(char *path, char *dest);

extern char com_token[1024];
extern qboolean com_eof;

void CreatePath(char *path);
void QCopyFile(char *from, char *to);

extern qboolean archive;
extern char archivedir[1024];

extern qboolean verbose;

typedef struct
{
	char name[56];
	int filepos, filelen;
} packfile_t;

typedef struct
{
	char id[4];
	int dirofs;
	int dirlen;
} packheader_t;

#endif
