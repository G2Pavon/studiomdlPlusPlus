// qc.cpp
#include "cmdlib.hpp"
#include "qc.hpp"
#include <iostream>


std::vector<char> qc_script_buffer;
char* qc_stream_p = nullptr;
char* qc_stream_end_p = nullptr;
bool token_ready = false;
bool end_of_qc_file = false;
int qc_line_number = 0;

void load_qc_file(const std::filesystem::path& filename)
{
    qc_script_buffer = load_file(filename);
    qc_stream_p = qc_script_buffer.data();
    qc_stream_end_p = qc_stream_p + qc_script_buffer.size();
    qc_line_number = 1;
    end_of_qc_file = false;
    token_ready = false;
    std::cout << "Processing " << filename << "\n";
}

bool get_token(bool crossline, std::string& token)
{
    if (token_ready)
    {
        token_ready = false;
        return true;
    }

    if (qc_stream_p >= qc_stream_end_p)
    {
        end_of_qc_file = true;
        return false;
    }
    
    while (*qc_stream_p <= 32 || *qc_stream_p == ';' || *qc_stream_p == '#' || (*qc_stream_p == '/' && *(qc_stream_p + 1) == '/'))
    {
        if (qc_stream_p >= qc_stream_end_p)
        {
            end_of_qc_file = true;
            return false;
        }
        if (*qc_stream_p++ == '\n')
        {
            if (!crossline)
                error("Line " + std::to_string(qc_line_number) + " is incomplete");
            qc_line_number++;
        }
        if (*qc_stream_p == ';' || *qc_stream_p == '#' || (*qc_stream_p == '/' && *(qc_stream_p + 1) == '/'))
        {
            while (*qc_stream_p != '\n' && qc_stream_p < qc_stream_end_p)
                qc_stream_p++;
        }
    }
    
    token.clear();
    
    if (*qc_stream_p == '"')
    {
        qc_stream_p++;
        while (*qc_stream_p != '"' && qc_stream_p < qc_stream_end_p)
            token.push_back(*qc_stream_p++);
        qc_stream_p++;
    }
    else
    {
        while (*qc_stream_p > 32 && *qc_stream_p != ';' && *qc_stream_p != '\n' && qc_stream_p < qc_stream_end_p)
            token.push_back(*qc_stream_p++);
    }
    
    return true;
}

bool token_available()
{
    char* search_p = qc_stream_p;
    while (*search_p <= 32)
    {
        if (*search_p == '\n')
            return false;
        search_p++;
        if (search_p == qc_stream_end_p)
            return false;
    }
    return *search_p != ';';
}
