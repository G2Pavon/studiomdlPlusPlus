#include "mdl/mdl_reader.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

#include "mdl/mdl_validator.hpp"
#include "utils/cmdlib.hpp"

namespace mdl
{
namespace
{
template <typename T>
T read_struct(const std::vector<std::byte> &bytes, int offset)
{
    if (offset < 0 || static_cast<std::size_t>(offset) + sizeof(T) > bytes.size())
    {
        throw std::runtime_error("Error: structure offset is out of bounds.");
    }

    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

template <typename T>
std::vector<T> read_array(const std::vector<std::byte> &bytes, int offset, int count)
{
    if (count < 0)
    {
        throw std::runtime_error("Error: negative array size in MDL.");
    }

    const std::size_t total = static_cast<std::size_t>(count) * sizeof(T);
    if (offset < 0 || static_cast<std::size_t>(offset) + total > bytes.size())
    {
        throw std::runtime_error("Error: array offset is out of bounds.");
    }

    std::vector<T> values(static_cast<std::size_t>(count));
    if (!values.empty())
    {
        std::memcpy(values.data(), bytes.data() + offset, total);
    }
    return values;
}

std::string read_c_string(const char *buffer, std::size_t size)
{
    const auto *end = static_cast<const char *>(std::memchr(buffer, '\0', size));
    return std::string(buffer, end ? end : buffer + size);
}

std::vector<std::byte> load_bytes(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Error: failed to open \"" + path_to_utf8(path) + "\".");
    }

    file.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> bytes(size);
    if (size > 0)
    {
        file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(size));
    }
    if (!file)
    {
        throw std::runtime_error("Error: failed to read \"" + path_to_utf8(path) + "\".");
    }
    return bytes;
}
} // namespace

std::string TextureData::name() const
{
    return read_c_string(header.name, sizeof(header.name));
}

int TextureData::pixel_data_size() const
{
    return header.width * header.height;
}

int TextureData::palette_size() const
{
    return 256 * 3;
}

int TextureData::expected_data_size() const
{
    return pixel_data_size() + palette_size();
}

MdlDocument MdlReader::read(const std::filesystem::path &path)
{
    MdlDocument document;
    document.source_path = path;
    document.bytes = load_bytes(path);

    if (document.bytes.size() < sizeof(StudioHeader))
    {
        throw std::runtime_error("Error: file is too small to be a GoldSrc MDL.");
    }

    document.header = read_struct<StudioHeader>(document.bytes, 0);

    document.bodyparts = read_array<StudioBodyPart>(
        document.bytes, document.header.bodypartindex, document.header.numbodyparts);

    document.models.reserve(static_cast<std::size_t>(std::max(document.header.numbodyparts, 0)));
    document.meshes.reserve(static_cast<std::size_t>(std::max(document.header.numbodyparts, 0)));

    int global_model_index = 0;
    for (int bodypart_index = 0; bodypart_index < document.header.numbodyparts; ++bodypart_index)
    {
        const auto &bodypart = document.bodyparts[static_cast<std::size_t>(bodypart_index)];
        const auto bodypart_name = read_c_string(bodypart.name, sizeof(bodypart.name));

        auto bodypart_models = read_array<StudioModel>(
            document.bytes, bodypart.modelindex, bodypart.nummodels);
        for (int local_index = 0; local_index < bodypart.nummodels; ++local_index)
        {
            const auto &model = bodypart_models[static_cast<std::size_t>(local_index)];
            auto model_meshes = read_array<StudioMesh>(
                document.bytes, model.meshindex, model.nummesh);

            document.models.push_back(model);
            document.meshes.push_back(model_meshes);

            SubmodelInfo submodel;
            submodel.global_index = global_model_index++;
            submodel.bodypart_index = bodypart_index;
            submodel.local_index = local_index;
            submodel.bodypart = bodypart;
            submodel.model = model;
            submodel.bodypart_name = bodypart_name;
            submodel.model_name = read_c_string(model.name, sizeof(model.name));

            for (int mesh_index = 0; mesh_index < model.nummesh; ++mesh_index)
            {
                MeshInfo mesh_info;
                mesh_info.mesh_index = mesh_index;
                mesh_info.mesh = model_meshes[static_cast<std::size_t>(mesh_index)];
                submodel.meshes.push_back(mesh_info);
            }

            document.submodels.push_back(submodel);
        }
    }

    auto texture_headers = read_array<StudioTexture>(
        document.bytes, document.header.textureindex, document.header.numtextures);
    document.textures.reserve(texture_headers.size());
    for (const auto &texture_header : texture_headers)
    {
        TextureData texture;
        texture.header = texture_header;
        const int size = texture.expected_data_size();
        if (size < 0)
        {
            throw std::runtime_error("Error: texture has invalid dimensions.");
        }
        if (texture.header.index < 0 ||
            static_cast<std::size_t>(texture.header.index) + static_cast<std::size_t>(size) >
                document.bytes.size())
        {
            throw std::runtime_error("Error: texture data offset is out of bounds.");
        }
        texture.data.resize(static_cast<std::size_t>(size));
        if (size > 0)
        {
            std::memcpy(texture.data.data(),
                        document.bytes.data() + texture.header.index,
                        static_cast<std::size_t>(size));
        }
        document.textures.push_back(std::move(texture));
    }

    if (document.header.numskinref < 0 || document.header.numskinfamilies < 0)
    {
        throw std::runtime_error("Error: skin table sizes are invalid.");
    }

    const auto skin_count =
        static_cast<std::size_t>(document.header.numskinref) *
        static_cast<std::size_t>(document.header.numskinfamilies);
    std::vector<std::int16_t> raw_skins;
    raw_skins = read_array<std::int16_t>(
        document.bytes, document.header.skinindex, static_cast<int>(skin_count));
    document.skin_families.resize(static_cast<std::size_t>(document.header.numskinfamilies));
    for (int family = 0; family < document.header.numskinfamilies; ++family)
    {
        auto &skin_family = document.skin_families[static_cast<std::size_t>(family)];
        skin_family.reserve(static_cast<std::size_t>(document.header.numskinref));
        for (int skinref = 0; skinref < document.header.numskinref; ++skinref)
        {
            const auto index = static_cast<std::size_t>(family * document.header.numskinref + skinref);
            skin_family.push_back(raw_skins[index]);
        }
    }

    if (!document.skin_families.empty())
    {
        const auto &family0 = document.skin_families.front();
        for (auto &submodel : document.submodels)
        {
            for (auto &mesh : submodel.meshes)
            {
                if (mesh.mesh.skinref < 0 ||
                    mesh.mesh.skinref >= static_cast<int>(family0.size()))
                {
                    throw std::runtime_error("Error: mesh skinref is out of bounds.");
                }
                mesh.family0_texture_index = family0[static_cast<std::size_t>(mesh.mesh.skinref)];
                if (mesh.family0_texture_index < 0 ||
                    mesh.family0_texture_index >= static_cast<int>(document.textures.size()))
                {
                    throw std::runtime_error("Error: texture index from skin family is out of bounds.");
                }
                mesh.family0_texture_name =
                    document.textures[static_cast<std::size_t>(mesh.family0_texture_index)].name();
            }
        }
    }

    MdlValidator::validate(document);
    return document;
}
} // namespace mdl
