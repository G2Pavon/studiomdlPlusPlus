#include <cstdint>
#include <cstdarg>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>

#ifdef WIN32
#include <direct.h>
#endif

#ifdef WIN32
#define PATHSEPARATOR(c) ((c) == '\\' || (c) == '/')
#else // WIN32
#define PATHSEPARATOR(c) ((c) == '/')
#endif // WIN32

#include "cmdlib.h"

bool archive;
char archivedir[1024];

// For abnormal program terminations
void Error(char *error, ...)
{
	va_list argptr;
	printf("\n************ ERROR ************\n");
	va_start(argptr, error);
	vprintf(error, argptr);
	va_end(argptr);
	printf("\n");
	exit(1);
}

/*qdir will hold the path up to the quake directory, including the slash

  f:\quake\
  /raid/quake/

gamedir will hold qdir + the game directory (id1, id2, etc)*/

char qproject[1024] = {'\0'};
char qdir[1024] = {'\0'};
char *ExpandPath(char *path)
{
	char *psz;
	static char full[1024];
	if (!qdir)
		Error("ExpandPath called without qdir set");
	if (path[0] == '/' || path[0] == '\\' || path[1] == ':')
		return path;
	psz = strstr(path, qdir);
	if (psz)
		strcpy(full, path);
	else
		sprintf(full, "%s%s", qdir, path);

	return full;
}

char *ExpandPathAndArchive(char *path)
{
	char *expanded;

	expanded = ExpandPath(path);

	if (archive == true)
	{
		char archivename[1024];
		sprintf(archivename, "%s/%s", archivedir, path);
		QCopyFile(expanded, archivename);
	}
	return expanded;
}

void Q_mkdir(char *path)
{
#ifdef WIN32
	if (_mkdir(path) != -1)
		return;
#else
	if (mkdir(path, 0777) != -1)
		return;
#endif
	if (errno != EEXIST)
		Error("mkdir %s: %s", path, strerror(errno));
}

// returns -1 if not present
int FileTime(char *path)
{
	struct stat buf;

	if (stat(path, &buf) == -1)
		return -1;

	return buf.st_mtime;
}

//  MISC FUNCTIONS

int filelength(FILE *f)
{
	int pos;
	int end;

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}

FILE *SafeOpenWrite(char *filename)
{
	FILE *f;

	f = fopen(filename, "wb");

	if (!f)
		Error("Error opening %s: %s", filename, strerror(errno));

	return f;
}

FILE *SafeOpenRead(char *filename)
{
	FILE *f;

	f = fopen(filename, "rb");

	if (!f)
		Error("Error opening %s: %s", filename, strerror(errno));

	return f;
}

void SafeRead(FILE *f, void *buffer, int count)
{
	if (fread(buffer, 1, count, f) != (size_t)count)
		Error("File read failure");
}

void SafeWrite(FILE *f, void *buffer, int count)
{
	if (fwrite(buffer, 1, count, f) != (size_t)count)
		Error("File read failure");
}

int LoadFile(char *filename, void **bufferptr)
{
	FILE *f;
	int length;
	void *buffer;

	f = SafeOpenRead(filename);
	length = filelength(f);
	buffer = malloc(length + 1);
	((char *)buffer)[length] = 0;
	SafeRead(f, buffer, length);
	fclose(f);

	*bufferptr = buffer;
	return length;
}

void SaveFile(char *filename, void *buffer, int count)
{
	FILE *f;

	f = SafeOpenWrite(filename);
	SafeWrite(f, buffer, count);
	fclose(f);
}

void DefaultExtension(char *path, char *extension)
{
	char *src;
	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	src = path + strlen(path) - 1;

	while (!PATHSEPARATOR(*src) && src != path)
	{
		if (*src == '.')
			return; // it has an extension
		src--;
	}

	strcat(path, extension);
}

void StripExtension(char *path)
{
	int length;

	length = strlen(path) - 1;
	while (length > 0 && path[length] != '.')
	{
		length--;
		if (path[length] == '/')
			return; // no extension
	}
	if (length)
		path[length] = 0;
}

void ExtractFileBase(char *path, char *dest)
{
	char *src;

	src = path + strlen(path) - 1;

	// back up until a \ or the start
	while (src != path && !PATHSEPARATOR(*(src - 1)))
		src--;

	while (*src && *src != '.')
	{
		*dest++ = *src++;
	}
	*dest = 0;
}

/*
============
CreatePath
============
*/
void CreatePath(char *path)
{
	char *ofs;

	for (ofs = path + 1; *ofs; ofs++)
	{
		char c;
		c = *ofs;
		if (c == '/' || c == '\\')
		{ // create the directory
			*ofs = 0;
			Q_mkdir(path);
			*ofs = c;
		}
	}
}

// Used to archive source files
void QCopyFile(char *from, char *to)
{
	void *buffer;
	int length;

	length = LoadFile(from, &buffer);
	CreatePath(to);
	SaveFile(to, buffer, length);
	free(buffer);
}