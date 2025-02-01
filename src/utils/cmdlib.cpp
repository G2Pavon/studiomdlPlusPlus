#include <cstdarg>
#include <cstring>
#include <string>

#include "cmdlib.hpp"

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

//  MISC FUNCTIONS

int file_length(FILE *f)
{
	int pos = ftell(f);
	fseek(f, 0, SEEK_END);
	int end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}

FILE *safe_open_write(char *filename)
{
	FILE *f = fopen(filename, "wb");
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
	FILE *f = fopen(filename, "rb");
	if (!f)
		error("Error opening %s: %s", filename, strerror(errno));
	int length = file_length(f);
	void *buffer = malloc(length + 1);
	((char *)buffer)[length] = 0;
	safe_read(f, buffer, length);
	fclose(f);

	*bufferptr = buffer;
	return length;
}

std::string strip_extension(const std::string& filename) {
    size_t dotPosition = filename.find_last_of('.');
    if (dotPosition == std::string::npos) {
        return filename;
    }
    return filename.substr(0, dotPosition);
}