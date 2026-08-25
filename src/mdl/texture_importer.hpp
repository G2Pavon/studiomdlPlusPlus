#pragma once

#include <filesystem>

#include "mdl/mdl_document.hpp"

namespace mdl
{
struct ImportedTexture
{
    std::string name;
    TextureData texture;
};

class TextureImporter
{
public:
    static ImportedTexture import_bmp(const std::filesystem::path &path);
};
} // namespace mdl
