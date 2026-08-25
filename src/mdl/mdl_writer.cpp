#include "mdl/mdl_writer.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

#include "utils/cmdlib.hpp"

namespace mdl
{
namespace
{
constexpr std::size_t kAlignment = 4;

std::size_t align_up(std::size_t value)
{
    return (value + (kAlignment - 1)) & ~(kAlignment - 1);
}

std::size_t first_rebuilt_section_offset(const MdlDocument &document)
{
    std::size_t offset = document.bytes.size();

    const auto consider = [&](int candidate)
    {
        if (candidate >= 0)
        {
            offset = std::min(offset, static_cast<std::size_t>(candidate));
        }
    };

    consider(document.header.textureindex);
    consider(document.header.skinindex);
    consider(document.header.texturedataindex);
    return offset;
}

void write_header(std::vector<std::byte> &buffer, const StudioHeader &header)
{
    if (buffer.size() < sizeof(StudioHeader))
    {
        throw std::runtime_error("Error: output buffer is too small for MDL header.");
    }
    std::memcpy(buffer.data(), &header, sizeof(header));
}

void save_bytes(const std::filesystem::path &path, const std::vector<std::byte> &bytes)
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Error: failed to write \"" + path_to_utf8(path) + "\".");
    }
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file)
    {
        throw std::runtime_error("Error: failed to write \"" + path_to_utf8(path) + "\".");
    }
}
} // namespace

void MdlWriter::write(const MdlDocument &document, const std::filesystem::path &path)
{
    StudioHeader header = document.header;
    const auto preserved_size = first_rebuilt_section_offset(document);
    if (preserved_size < sizeof(StudioHeader) || preserved_size > document.bytes.size())
    {
        throw std::runtime_error("Error: invalid preserved MDL prefix.");
    }

    std::vector<std::byte> buffer(document.bytes.begin(), document.bytes.begin() + preserved_size);
    buffer.resize(align_up(buffer.size()), std::byte{0});

    header.numtextures = static_cast<int>(document.textures.size());
    header.numskinfamilies = static_cast<int>(document.skin_families.size());

    header.textureindex = static_cast<int>(buffer.size());
    const auto texture_table_size =
        static_cast<std::size_t>(header.numtextures) * sizeof(StudioTexture);
    buffer.resize(buffer.size() + texture_table_size, std::byte{0});
    buffer.resize(align_up(buffer.size()), std::byte{0});

    header.skinindex = static_cast<int>(buffer.size());
    const auto skin_table_size =
        static_cast<std::size_t>(header.numskinref) * static_cast<std::size_t>(header.numskinfamilies) *
        sizeof(std::int16_t);
    buffer.resize(buffer.size() + skin_table_size, std::byte{0});
    buffer.resize(align_up(buffer.size()), std::byte{0});

    header.texturedataindex = static_cast<int>(buffer.size());

    std::vector<StudioTexture> texture_headers;
    texture_headers.reserve(document.textures.size());
    for (const auto &texture : document.textures)
    {
        StudioTexture copy = texture.header;
        copy.index = static_cast<int>(buffer.size());
        texture_headers.push_back(copy);
        buffer.insert(buffer.end(), texture.data.begin(), texture.data.end());
    }
    buffer.resize(align_up(buffer.size()), std::byte{0});

    header.length = static_cast<int>(buffer.size());
    std::memcpy(buffer.data() + header.textureindex,
                texture_headers.data(),
                texture_headers.size() * sizeof(StudioTexture));

    auto *skin_table = reinterpret_cast<std::int16_t *>(buffer.data() + header.skinindex);
    std::size_t skin_offset = 0;
    for (const auto &family : document.skin_families)
    {
        std::memcpy(skin_table + skin_offset, family.data(), family.size() * sizeof(std::int16_t));
        skin_offset += family.size();
    }

    write_header(buffer, header);
    save_bytes(path, buffer);
}
} // namespace mdl
