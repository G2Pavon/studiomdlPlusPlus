#pragma once

#include <filesystem>

#include "mdl/mdl_document.hpp"

namespace mdl
{
class MdlReader
{
public:
    static MdlDocument read(const std::filesystem::path &path);
};
} // namespace mdl
