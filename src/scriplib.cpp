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
script_t g_scriptstack[MAX_INCLUDES];
script_t *g_script;
int g_scriptline;

char g_token[MAXTOKEN];
bool g_endofscript;
bool g_tokenready;

void AddScriptToStack(char *filename)
{
	int size;

	g_script++;
	if (g_script == &g_scriptstack[MAX_INCLUDES])
		Error("script file exceeded MAX_INCLUDES");
	std::strcpy(g_script->filename, ExpandPath(filename));

	size = LoadFile(g_script->filename, (void **)&g_script->buffer);

	printf("entering %s\n", g_script->filename);

	g_script->line = 1;

	g_script->script_p = g_script->buffer;
	g_script->end_p = g_script->buffer + size;
}

void LoadScriptFile(char *filename)
{
	g_script = g_scriptstack;
	AddScriptToStack(filename);

	g_endofscript = false;
	g_tokenready = false;
}

bool EndOfScript(bool crossline)
{
	if (!crossline)
		Error("Line %i is incomplete\n", g_scriptline);

	if (!std::strcmp(g_script->filename, "memory buffer"))
	{
		g_endofscript = true;
		return false;
	}

	free(g_script->buffer);
	if (g_script == g_scriptstack + 1)
	{
		g_endofscript = true;
		return false;
	}
	g_script--;
	g_scriptline = g_script->line;
	printf("returning to %s\n", g_script->filename);
	return GetToken(crossline);
}

bool GetToken(bool crossline)
{
	char *token_p;

	if (g_tokenready) // is a token already waiting?
	{
		g_tokenready = false;
		return true;
	}

	if (g_script->script_p >= g_script->end_p)
		return EndOfScript(crossline);

	// Skip spaces and comments
	while (*g_script->script_p <= 32)
	{
		if (g_script->script_p >= g_script->end_p)
			return EndOfScript(crossline);

		if (*g_script->script_p++ == '\n')
		{
			if (!crossline)
				Error("Line %i is incomplete\n", g_scriptline);
			g_scriptline = g_script->line++;
		}

		// Check for comment (semicolon or line comment)
		if (*g_script->script_p == ';' || *g_script->script_p == '#' ||
			(*g_script->script_p == '/' && *((g_script->script_p) + 1) == '/'))
		{
			if (!crossline)
				Error("Line %i is incomplete\n", g_scriptline);

			while (*g_script->script_p != '\n')
			{
				if (g_script->script_p >= g_script->end_p)
					return EndOfScript(crossline);
				g_script->script_p++;
			}

			// Continue skipping spaces after the comment
			if (g_script->script_p < g_script->end_p)
				continue;
			return EndOfScript(crossline);
		}
	}

	if (g_script->script_p >= g_script->end_p)
		return EndOfScript(crossline);

	// Copy token
	token_p = g_token;

	if (*g_script->script_p == '"')
	{
		// Quoted token
		g_script->script_p++;
		while (*g_script->script_p != '"')
		{
			*token_p++ = *g_script->script_p++;
			if (g_script->script_p == g_script->end_p)
				break;
			if (token_p == &g_token[MAXTOKEN])
				Error("Token too large on line %i\n", g_scriptline);
		}
		g_script->script_p++;
	}
	else // Regular token
	{
		while (*g_script->script_p > 32 && *g_script->script_p != ';')
		{
			*token_p++ = *g_script->script_p++;
			if (g_script->script_p == g_script->end_p)
				break;
			if (token_p == &g_token[MAXTOKEN])
				Error("Token too large on line %i\n", g_scriptline);
		}
	}

	*token_p = 0;

	if (!std::strcmp(g_token, "$include"))
	{
		GetToken(false);
		AddScriptToStack(g_token);
		return GetToken(crossline);
	}

	return true;
}

// turns true if there is another token on the line
bool TokenAvailable(void)
{
	char *search_p;

	search_p = g_script->script_p;

	if (search_p >= g_script->end_p)
		return false;

	while (*search_p <= 32)
	{
		if (*search_p == '\n')
			return false;
		search_p++;
		if (search_p == g_script->end_p)
			return false;
	}

	if (*search_p == ';')
		return false;

	return true;
}