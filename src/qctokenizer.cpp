// scriplib.cpp

#include <cstring>
#include <cstdlib>

#include "cmdlib.hpp"
#include "qctokenizer.hpp"


char *script_p;
int g_scriptline;
bool g_tokenready;
char *script_end_p;

bool g_endofscript;
char* script_buffer;



void LoadScriptFile(char *filename)
{
    int size;

    size = LoadFile(filename, (void **)&script_buffer);

    printf("processing %s\n", filename);

    g_scriptline = 1;
    script_p = script_buffer;
    script_end_p = script_buffer + size;
    g_endofscript = false;
    g_tokenready = false;
}

bool GetToken(bool crossline, char* token)
{
    char *token_p;

    if (g_tokenready) // is a token already waiting?
    {
        g_tokenready = false;
        return true;
    }

    if (script_p >= script_end_p)
    {
        g_endofscript = true;
        return false; // End of script
    }

    // Skip spaces and comments
    while (*script_p <= 32 || *script_p == ';' || *script_p == '#' || (*script_p == '/' && *(script_p + 1) == '/'))
    {
        if (script_p >= script_end_p)
        {
            g_endofscript = true;
            return false; // End of script
        }

        if (*script_p++ == '\n')
        {
            if (!crossline)
                Error("Line %i is incomplete\n", g_scriptline);
            g_scriptline++;
        }

        // Check for comment (semicolon or line comment)
        if (*script_p == ';' || *script_p == '#' || (*script_p == '/' && *(script_p + 1) == '/'))
        {
            while (*script_p != '\n')
            {
                if (script_p >= script_end_p)
                {
                    g_endofscript = true;
                    return false; // End of script
                }
                script_p++;
            }
        }
    }

    // Copy token
    token_p = token;

    if (*script_p == '"')
    {
        // Quoted token
        script_p++;
        while (*script_p != '"')
        {
            *token_p++ = *script_p++;
            if (script_p == script_end_p)
                break;
            if (token_p == &token[MAXTOKEN])
                Error("Token too large on line %i\n", g_scriptline);
        }
        script_p++;
    }
    else // Regular token
    {
        while (*script_p > 32 && *script_p != ';' && *script_p != '\n') // Added '\n' to delimiters
        {
            *token_p++ = *script_p++;
            if (script_p == script_end_p)
                break;
            if (token_p == &token[MAXTOKEN])
                Error("Token too large on line %i\n", g_scriptline);
        }
    }

    *token_p = 0;
    return true;
}

// turns true if there is another token on the line
bool TokenAvailable(void)
{
    char *search_p = script_p;

    if (search_p >= script_end_p)
        return false;

    while (*search_p <= 32)
    {
        if (*search_p == '\n')
            return false;
        search_p++;
        if (search_p == script_end_p)
            return false;
    }

    if (*search_p == ';')
        return false;

    return true;
}