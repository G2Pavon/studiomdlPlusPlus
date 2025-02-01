// cmdlib.hpp
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <stdexcept>
#include <fstream>
#include <memory>

int file_length(std::ifstream& file);
[[noreturn]] void error(const std::string& message);

std::unique_ptr<std::ofstream> safe_open_write(const std::filesystem::path& filename);
void safe_read(std::ifstream& file, void* buffer, std::size_t count);
void safe_write(std::ofstream& file, const void* buffer, std::size_t count);

std::vector<char> load_file(const std::filesystem::path& filename);
std::string strip_extension(const std::string& filename);