// cmdlib.hpp
#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

int file_length(std::ifstream &file);
void error(const std::string &message);

std::unique_ptr<std::ofstream> safe_open_write(const std::filesystem::path &filename);
void safe_read(std::ifstream &file, void *buffer, std::size_t count);
void safe_write(std::ofstream &file, const void *buffer, std::size_t count);

std::vector<char> load_file(const std::filesystem::path &filename);

// string manipulation
std::string strip_extension(const std::string &filename);
bool case_insensitive_compare(const std::string &str1, const std::string &str2);
bool case_insensitive_n_compare(const std::string &str1, const std::string &str2, size_t n);
void trim_newline_carriage(char *str);
std::string to_lowercase(const std::string &str);
std::string extension_to_lowercase(const std::string &filename);