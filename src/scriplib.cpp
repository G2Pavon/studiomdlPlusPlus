// scriplib.c
#include "cmdlib.h"
#include "scriplib.h"

typedef struct
{
	char filename[1024];
	char *buffer, *script_p, *end_p;
	int line;
} script_t;

#define MAX_INCLUDES 8
script_t scriptstack[MAX_INCLUDES];
script_t *script;
int scriptline;

char token[MAXTOKEN];
qboolean endofscript;
qboolean tokenready;

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

	endofscript = qfalse;
	tokenready = qfalse;
}

qboolean EndOfScript(qboolean crossline)
{
	if (!crossline)
		Error("Line %i is incomplete\n", scriptline);

	if (!strcmp(script->filename, "memory buffer"))
	{
		endofscript = qtrue;
		return qfalse;
	}

	free(script->buffer);
	if (script == scriptstack + 1)
	{
		endofscript = qtrue;
		return qfalse;
	}
	script--;
	scriptline = script->line;
	printf("returning to %s\n", script->filename);
	return GetToken(crossline);
}

qboolean GetToken(qboolean crossline)
{
	char *token_p;

	if (tokenready) // is a token already waiting?
	{
		tokenready = qfalse;
		return qtrue;
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

	if (!strcmp(token, "$include"))
	{
		GetToken(qfalse);
		AddScriptToStack(token);
		return GetToken(crossline);
	}

	return qtrue;
}

// turns true if there is another token on the line
qboolean TokenAvailable(void)
{
	char *search_p;

	search_p = script->script_p;

	if (search_p >= script->end_p)
		return qfalse;

	while (*search_p <= 32)
	{
		if (*search_p == '\n')
			return qfalse;
		search_p++;
		if (search_p == script->end_p)
			return qfalse;
	}

	if (*search_p == ';')
		return qfalse;

	return qtrue;
}