#pragma once

#include <string>

#include "mdl/mdl_document.hpp"

namespace mdl
{
class MdlValidator
{
public:
    static void validate(const MdlDocument &document);
    static void validate_file(const std::filesystem::path &path);
};
} // namespace mdl
