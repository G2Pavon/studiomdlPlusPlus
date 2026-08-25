#include "cmdlib.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif


void error(const std::string &message)
{
    throw std::runtime_error("ERROR: " + message);
}

std::string path_to_utf8(const std::filesystem::path &path)
{
#ifdef _WIN32
    return path.u8string();
#else
    return path.string();
#endif
}

std::FILE *path_fopen(const std::filesystem::path &path, const char *mode)
{
#ifdef _WIN32
    std::wstring wmode;
    while (*mode)
    {
        wmode.push_back(static_cast<wchar_t>(*mode++));
    }
    return _wfopen(path.c_str(), wmode.c_str());
#else
    return std::fopen(path.c_str(), mode);
#endif
}

std::vector<std::filesystem::path> get_native_args(int argc, char **argv)
{
    std::vector<std::filesystem::path> args;
#ifdef _WIN32
    int wide_argc = 0;
    wchar_t **wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    if (wide_argv != nullptr)
    {
        args.reserve(wide_argc);
        for (int i = 0; i < wide_argc; ++i)
        {
            args.emplace_back(wide_argv[i]);
        }
        LocalFree(wide_argv);
        return args;
    }
#endif
    args.reserve(argc);
    for (int i = 0; i < argc; ++i)
    {
        args.emplace_back(argv[i]);
    }
    return args;
}

static int file_length(std::ifstream &file)
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
        error("Error opening " + path_to_utf8(filename));
    return file;
}

static void safe_read(std::ifstream &file, void *buffer, std::size_t count)
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
        error("Error opening " + path_to_utf8(filename));

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

std::string to_lowercase(const std::string &str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });
    return result;
}

std::string extension_to_lowercase(const std::string &filename)
{
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos)
    {
        return filename;
    }

    std::string extension = filename.substr(dot_pos);
    std::string name = filename.substr(0, dot_pos);
    extension = to_lowercase(extension);

    return name + extension;
}
