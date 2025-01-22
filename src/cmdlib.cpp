#include <cstdint>
#include <cstdarg>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>

#define PATHSEPARATOR(c) ((c) == '/')

#include "cmdlib.hpp"

bool g_archive;
char g_archivedir[1024];

// For abnormal program terminations
void error(char *error, ...)
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
char *expand_path(char *path) // FIXME: use filesystem
{
	char *psz;
	static char full[1024];
	if (!qdir)
		error("expand_path called without qdir set");
	if (path[0] == '/' || path[0] == '\\' || path[1] == ':')
		return path;
	psz = strstr(path, qdir);
	if (psz)
		std::strcpy(full, path);
	else
		sprintf(full, "%s%s", qdir, path);

	return full;
}

char *expand_path_and_archive(char *path)
{
	char *expanded;

	expanded = expand_path(path);

	if (g_archive == true)
	{
		char archivename[1024];
		sprintf(archivename, "%s/%s", g_archivedir, path);
		q_copy_file(expanded, archivename);
	}
	return expanded;
}

void q_mkdir(char *path)
{
	if (mkdir(path, 0777) != -1)
		return;
	if (errno != EEXIST)
		error("mkdir %s: %s", path, strerror(errno));
}

// returns -1 if not present
int file_time(char *path)
{
	struct stat buf;

	if (stat(path, &buf) == -1)
		return -1;

	return buf.st_mtime;
}

//  MISC FUNCTIONS

int file_length(FILE *f)
{
	int pos;
	int end;

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}

FILE *safe_open_write(char *filename)
{
	FILE *f;

	f = fopen(filename, "wb");

	if (!f)
		error("Error opening %s: %s", filename, strerror(errno));

	return f;
}

FILE *safe_open_read(char *filename)
{
	FILE *f;

	f = fopen(filename, "rb");

	if (!f)
		error("Error opening %s: %s", filename, strerror(errno));

	return f;
}

void safe_read(FILE *f, void *buffer, int count)
{
	if (fread(buffer, 1, count, f) != (size_t)count)
		error("File read failure");
}

void safe_write(FILE *f, void *buffer, int count)
{
	if (fwrite(buffer, 1, count, f) != (size_t)count)
		error("File read failure");
}

int load_file(char *filename, void **bufferptr)
{
	FILE *f;
	int length;
	void *buffer;

	f = safe_open_read(filename);
	length = file_length(f);
	buffer = malloc(length + 1);
	((char *)buffer)[length] = 0;
	safe_read(f, buffer, length);
	fclose(f);

	*bufferptr = buffer;
	return length;
}

void save_file(char *filename, void *buffer, int count)
{
	FILE *f;

	f = safe_open_write(filename);
	safe_write(f, buffer, count);
	fclose(f);
}

void strip_extension(char *path)
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

void extract_filebase(char *path, char *dest)
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
create_path
============
*/
void create_path(char *path)
{
	char *ofs;

	for (ofs = path + 1; *ofs; ofs++)
	{
		char c;
		c = *ofs;
		if (c == '/' || c == '\\')
		{ // create the directory
			*ofs = 0;
			q_mkdir(path);
			*ofs = c;
		}
	}
}

// Used to archive source files
void q_copy_file(char *from, char *to)
{
	void *buffer;
	int length;

	length = load_file(from, &buffer);
	create_path(to);
	save_file(to, buffer, length);
	free(buffer);
}