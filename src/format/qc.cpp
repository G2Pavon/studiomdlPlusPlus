#include <cstring>
#include <cstdlib>
#include <string>

#include "utils/cmdlib.hpp"
#include "format/qc.hpp"

int g_num_renamebone = 0;
int g_num_hitgroups = 0;
int g_num_mirrored = 0;
int g_num_animation = 0;
int g_num_texturegroup = 0;
int g_num_hitboxes = 0;
int g_num_bonecontroller = 0; 
int g_num_attachments = 0;
int g_num_sequence = 0;
int g_num_submodels = 0;
int g_num_bodygroup = 0;


char *qc_stream_p;     // Pointer to the current position in the QC file's data stream
int qc_line_number;    // Current line number in the QC file
bool token_ready;      // Flag indicating if a token is ready
char *qc_stream_end_p; // Pointer to the end of the QC file's data stream

bool end_of_qc_file;    // Flag indicating if we've reached the end of the QC file
char *qc_script_buffer; // Buffer holding the entire QC file data

void load_qc_file(char *filename)
{
    int size;

    size = load_file(filename, (void **)&qc_script_buffer);

    printf("processing %s\n", filename);

    qc_line_number = 1;
    qc_stream_p = qc_script_buffer;
    qc_stream_end_p = qc_script_buffer + size;
    end_of_qc_file = false;
    token_ready = false;
}

bool get_token(bool crossline, std::string &token)
{
    if (token_ready) // is a token already waiting?
    {
        token_ready = false;
        return true;
    }

    if (qc_stream_p >= qc_stream_end_p)
    {
        end_of_qc_file = true;
        return false; // End of script
    }

    // Skip spaces and comments
    while (*qc_stream_p <= 32 || *qc_stream_p == ';' || *qc_stream_p == '#' || (*qc_stream_p == '/' && *(qc_stream_p + 1) == '/'))
    {
        if (qc_stream_p >= qc_stream_end_p)
        {
            end_of_qc_file = true;
            return false; // End of script
        }

        if (*qc_stream_p++ == '\n')
        {
            if (!crossline)
                error("Line %i is incomplete\n", qc_line_number);
            qc_line_number++;
        }

        // Check for comment (semicolon or line comment)
        if (*qc_stream_p == ';' || *qc_stream_p == '#' || (*qc_stream_p == '/' && *(qc_stream_p + 1) == '/'))
        {
            while (*qc_stream_p != '\n')
            {
                if (qc_stream_p >= qc_stream_end_p)
                {
                    end_of_qc_file = true;
                    return false; // End of script
                }
                qc_stream_p++;
            }
        }
    }

    // Clear the string to store the new token
    token.clear();

    if (*qc_stream_p == '"')
    {
        // Quoted token
        qc_stream_p++;
        while (*qc_stream_p != '"')
        {
            token.push_back(*qc_stream_p++); // add characters
            if (qc_stream_p == qc_stream_end_p)
                break;
        }
        qc_stream_p++; // Skip the closing quote
    }
    else // Regular token
    {
        while (*qc_stream_p > 32 && *qc_stream_p != ';' && *qc_stream_p != '\n') // Added '\n' to delimiters
        {
            token.push_back(*qc_stream_p++); // Add characters to the token string
            if (qc_stream_p == qc_stream_end_p)
                break;
        }
    }

    return true;
}

// turns true if there is another token on the line
bool token_available(void)
{
    char *search_p = qc_stream_p;

    if (search_p >= qc_stream_end_p)
        return false;

    while (*search_p <= 32)
    {
        if (*search_p == '\n')
            return false;
        search_p++;
        if (search_p == qc_stream_end_p)
            return false;
    }

    if (*search_p == ';')
        return false;

    return true;
}
