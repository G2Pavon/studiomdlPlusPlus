#pragma once

#include <filesystem>

#include "format/qc.hpp"

void write_file(std::filesystem::path path, QC &qc);