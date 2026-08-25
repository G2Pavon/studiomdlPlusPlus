#include "mdl/texture_importer.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>

#include "format/image/bmp.hpp"
#include "utils/cmdlib.hpp"

namespace mdl
{
namespace
{
struct FreeDeleter
{
    void operator()(void *ptr) const
    {
        std::free(ptr);
    }
};

std::string basename_string(const std::filesystem::path &path)
{
    return path.filename().string();
}
} // namespace

ImportedTexture TextureImporter::import_bmp(const std::filesystem::path &path)
{
    uint8_t *raw_bits = nullptr;
    uint8_t *raw_palette = nullptr;
    int width = 0;
    int height = 0;

    if (load_bmp(path, &raw_bits, &raw_palette, &width, &height) != 0)
    {
        throw std::runtime_error(
            "Error: \"" + basename_string(path) + "\" is not an indexed 8-bit BMP.");
    }

    std::unique_ptr<void, FreeDeleter> bits_guard(raw_bits);
    std::unique_ptr<void, FreeDeleter> palette_guard(raw_palette);

    const auto texture_name = path.filename().string();
    if (texture_name.size() >= sizeof(StudioTexture::name))
    {
        throw std::runtime_error(
            "Error: texture name \"" + texture_name + "\" is too long for GoldSrc MDL.");
    }

    const int padded_width = (width + 3) & ~3;
    TextureData texture;
    std::memset(texture.header.name, 0, sizeof(texture.header.name));
    std::memcpy(texture.header.name, texture_name.c_str(), texture_name.size());
    texture.header.width = width;
    texture.header.height = height;
    texture.header.flags = 0;

    texture.data.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) + 256U * 3U);

    auto *pixels = reinterpret_cast<const std::byte *>(raw_bits);
    for (int row = 0; row < height; ++row)
    {
        const auto src_offset = static_cast<std::size_t>(row) * static_cast<std::size_t>(padded_width);
        const auto dst_offset = static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        std::memcpy(texture.data.data() + dst_offset, pixels + src_offset, static_cast<std::size_t>(width));
    }

    const auto palette_offset = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::memcpy(texture.data.data() + palette_offset, raw_palette, 256U * 3U);

    ImportedTexture imported;
    imported.name = texture_name;
    imported.texture = std::move(texture);
    return imported;
}
} // namespace mdl
