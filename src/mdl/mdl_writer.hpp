#pragma once

#include <filesystem>

#include "mdl/mdl_document.hpp"

namespace mdl
{
class MdlWriter
{
public:
    static void write(const MdlDocument &document, const std::filesystem::path &path);
};
} // namespace mdl
