// cmdlib.cpp
#include "cmdlib.hpp"
#include <iostream>
#include <cstdarg>
#include <cstring>

[[noreturn]] void error(const std::string& message)
{
    throw std::runtime_error("ERROR: " + message);
}

int file_length(std::ifstream& file)
{
    file.seekg(0, std::ios::end);
    int length = file.tellg();
    file.seekg(0, std::ios::beg);
    return length;
}

std::unique_ptr<std::ofstream> safe_open_write(const std::filesystem::path& filename)
{
    auto file = std::make_unique<std::ofstream>(filename, std::ios::binary);
    if (!file || !file->is_open())
        error("Error opening " + filename.string());
    return file;
}

void safe_read(std::ifstream& file, void* buffer, std::size_t count)
{
    if (!file.read(reinterpret_cast<char*>(buffer), count))
        error("File read failure");
}

void safe_write(std::ofstream& file, const void* buffer, std::size_t count)
{
    if (!file.write(reinterpret_cast<const char*>(buffer), count))
        error("File write failure");
}

std::vector<char> load_file(const std::filesystem::path& filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
        error("Error opening " + filename.string());
    
    int length = file_length(file);
    std::vector<char> buffer(length + 1, '\0');
    safe_read(file, buffer.data(), length);
    return buffer;
}

std::string strip_extension(const std::string& filename)
{
    return std::filesystem::path(filename).stem().string();
}