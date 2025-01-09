// scriplib.c

#include <cstring>
#include <cstdlib>

#include "cmdlib.h"
#include "scriplib.h"

struct script_t
{
	char filename[1024];
	char *buffer, *script_p, *end_p;
	int line;
};

constexpr int MAX_INCLUDES = 8;
script_t scriptstack[MAX_INCLUDES];
script_t *script;
int scriptline;

char token[MAXTOKEN];
bool endofscript;
bool tokenready;

void AddScriptToStack(char *filename)
{
	int size;

	script++;
	if (script == &scriptstack[MAX_INCLUDES])
		Error("script file exceeded MAX_INCLUDES");
	strcpy(script->filename, ExpandPath(filename));

	size = LoadFile(script->filename, (void **)&script->buffer);

	printf("entering %s\n", script->filename);

	script->line = 1;

	script->script_p = script->buffer;
	script->end_p = script->buffer + size;
}

void LoadScriptFile(char *filename)
{
	script = scriptstack;
	AddScriptToStack(filename);

	endofscript = false;
	tokenready = false;
}

bool EndOfScript(bool crossline)
{
	if (!crossline)
		Error("Line %i is incomplete\n", scriptline);

	if (!std::strcmp(script->filename, "memory buffer"))
	{
		endofscript = true;
		return false;
	}

	free(script->buffer);
	if (script == scriptstack + 1)
	{
		endofscript = true;
		return false;
	}
	script--;
	scriptline = script->line;
	printf("returning to %s\n", script->filename);
	return GetToken(crossline);
}

bool GetToken(bool crossline)
{
	char *token_p;

	if (tokenready) // is a token already waiting?
	{
		tokenready = false;
		return true;
	}

	if (script->script_p >= script->end_p)
		return EndOfScript(crossline);

	// Skip spaces and comments
	while (*script->script_p <= 32)
	{
		if (script->script_p >= script->end_p)
			return EndOfScript(crossline);

		if (*script->script_p++ == '\n')
		{
			if (!crossline)
				Error("Line %i is incomplete\n", scriptline);
			scriptline = script->line++;
		}

		// Check for comment (semicolon or line comment)
		if (*script->script_p == ';' || *script->script_p == '#' ||
			(*script->script_p == '/' && *((script->script_p) + 1) == '/'))
		{
			if (!crossline)
				Error("Line %i is incomplete\n", scriptline);

			while (*script->script_p != '\n')
			{
				if (script->script_p >= script->end_p)
					return EndOfScript(crossline);
				script->script_p++;
			}

			// Continue skipping spaces after the comment
			if (script->script_p < script->end_p)
				continue;
			return EndOfScript(crossline);
		}
	}

	if (script->script_p >= script->end_p)
		return EndOfScript(crossline);

	// Copy token
	token_p = token;

	if (*script->script_p == '"')
	{
		// Quoted token
		script->script_p++;
		while (*script->script_p != '"')
		{
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p)
				break;
			if (token_p == &token[MAXTOKEN])
				Error("Token too large on line %i\n", scriptline);
		}
		script->script_p++;
	}
	else // Regular token
	{
		while (*script->script_p > 32 && *script->script_p != ';')
		{
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p)
				break;
			if (token_p == &token[MAXTOKEN])
				Error("Token too large on line %i\n", scriptline);
		}
	}

	*token_p = 0;

	if (!std::strcmp(token, "$include"))
	{
		GetToken(false);
		AddScriptToStack(token);
		return GetToken(crossline);
	}

	return true;
}

// turns true if there is another token on the line
bool TokenAvailable(void)
{
	char *search_p;

	search_p = script->script_p;

	if (search_p >= script->end_p)
		return false;

	while (*search_p <= 32)
	{
		if (*search_p == '\n')
			return false;
		search_p++;
		if (search_p == script->end_p)
			return false;
	}

	if (*search_p == ';')
		return false;

	return true;
}