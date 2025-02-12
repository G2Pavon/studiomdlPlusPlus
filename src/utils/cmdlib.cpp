// cmdlib.cpp
#include "cmdlib.hpp"
#include <cstdarg>
#include <cstring>
#include <iostream>

void error(const std::string &message)
{
    throw std::runtime_error("ERROR: " + message);
}

int file_length(std::ifstream &file)
{
    file.seekg(0, std::ios::end);
    int length = file.tellg();
    file.seekg(0, std::ios::beg);
    return length;
}

std::unique_ptr<std::ofstream> safe_open_write(const std::filesystem::path &filename)
{
    auto file = std::make_unique<std::ofstream>(filename, std::ios::binary);
    if (!file || !file->is_open())
        error("Error opening " + filename.string());
    return file;
}

void safe_read(std::ifstream &file, void *buffer, std::size_t count)
{
    if (!file.read(reinterpret_cast<char *>(buffer), count))
        error("File read failure");
}

void safe_write(std::ofstream &file, const void *buffer, std::size_t count)
{
    if (!file.write(reinterpret_cast<const char *>(buffer), count))
        error("File write failure");
}

std::vector<char> load_file(const std::filesystem::path &filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
        error("Error opening " + filename.string());

    int length = file_length(file);
    std::vector<char> buffer(length + 1, '\0');
    safe_read(file, buffer.data(), length);
    return buffer;
}

std::string strip_extension(const std::string &filename)
{
    return std::filesystem::path(filename).stem().string();
}

bool case_insensitive_compare(const std::string &str1, const std::string &str2)
{
    if (str1.size() != str2.size())
    {
        return false;
    }
    return std::equal(str1.begin(), str1.end(), str2.begin(),
                      [](char c1, char c2)
                      {
                          return std::tolower(static_cast<unsigned char>(c1)) == std::tolower(static_cast<unsigned char>(c2));
                      });
}

bool case_insensitive_n_compare(const std::string &str1, const std::string &str2, size_t n)
{
    if (str1.size() < n || str2.size() < n)
    {
        return false;
    }
    return std::equal(str1.begin(), str1.begin() + n, str2.begin(),
                      [](char c1, char c2)
                      {
                          return std::tolower(static_cast<unsigned char>(c1)) == std::tolower(static_cast<unsigned char>(c2));
                      });
}

void trim_newline_carriage(char *str)
{
    char *p = str;
    while (*p)
        p++;
    while (p > str && (p[-1] == '\n' || p[-1] == '\r'))
        *(--p) = '\0';
}