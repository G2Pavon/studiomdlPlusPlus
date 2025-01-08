#include "cmdlib.h"
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
#include <direct.h>
#endif

#ifdef WIN32
#define PATHSEPARATOR(c) ((c) == '\\' || (c) == '/')
#else // WIN32
#include <unistd.h>
#define PATHSEPARATOR(c) ((c) == '/')
#endif // WIN32

// set these before calling CheckParm
int myargc;
char **myargv;

char com_token[1024];
qboolean com_eof;
qboolean archive;
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

// only printf if in verbose mode
qboolean verbose = false;

/*qdir will hold the path up to the quake directory, including the slash

  f:\quake\
  /raid/quake/

gamedir will hold qdir + the game directory (id1, id2, etc)*/

char qproject[1024] = {'\0'};
char qdir[1024] = {'\0'};
char gamedir[1024] = {'\0'};

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
	char archivename[1024];

	expanded = ExpandPath(path);

	if (archive)
	{
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

//  BYTE ORDER FUNCTIONS
#ifdef _SGI_SOURCE
#define __BIG_ENDIAN__
#endif

#ifdef __BIG_ENDIAN__
#else
#endif

//=======================================================

// FIXME: byte swap?

// this is a 16 bit, non-reflected CRC using the polynomial 0x1021
// and the initial and final xor values shown below...  in other words, the
// CCITT standard CRC used by XMODEM

#define CRC_INIT_VALUE 0xffff
#define CRC_XOR_VALUE 0x0000

static unsigned short crctable[256] =
	{
		0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
		0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
		0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
		0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
		0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
		0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
		0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
		0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
		0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
		0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
		0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
		0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
		0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
		0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
		0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
		0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
		0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
		0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
		0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
		0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
		0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
		0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
		0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
		0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
		0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
		0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
		0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
		0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
		0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
		0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
		0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0};

//=============================================================================

/*
============
CreatePath
============
*/
void CreatePath(char *path)
{
	char *ofs, c;

	for (ofs = path + 1; *ofs; ofs++)
	{
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