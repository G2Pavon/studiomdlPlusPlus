#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "format/mdl.hpp"

namespace mdl
{
struct TextureData
{
    StudioTexture header{};
    std::vector<std::byte> data;

    [[nodiscard]] std::string name() const;
    [[nodiscard]] int pixel_data_size() const;
    [[nodiscard]] int palette_size() const;
    [[nodiscard]] int expected_data_size() const;
};

struct MeshInfo
{
    int mesh_index = 0;
    StudioMesh mesh{};
    int family0_texture_index = -1;
    std::string family0_texture_name;
};

struct SubmodelInfo
{
    int global_index = 0;
    int bodypart_index = 0;
    int local_index = 0;
    StudioBodyPart bodypart{};
    StudioModel model{};
    std::string bodypart_name;
    std::string model_name;
    std::vector<MeshInfo> meshes;
};

struct MdlDocument
{
    std::filesystem::path source_path;
    std::vector<std::byte> bytes;
    StudioHeader header{};
    std::vector<StudioBodyPart> bodyparts;
    std::vector<StudioModel> models;
    std::vector<std::vector<StudioMesh>> meshes;
    std::vector<TextureData> textures;
    std::vector<std::vector<std::int16_t>> skin_families;
    std::vector<SubmodelInfo> submodels;
};
} // namespace mdl
