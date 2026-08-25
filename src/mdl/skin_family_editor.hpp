#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "mdl/mdl_document.hpp"

namespace mdl
{
struct AddSkinOptions
{
    std::filesystem::path model_path;
    std::filesystem::path bmp_path;
    std::filesystem::path output_path;
    int submodel_index = 0;
    int copy_skin_index = 0;
    std::string target_texture;
    bool all_matches = false;
    bool in_place = false;
    bool backup = false;
    bool verbose = false;
    bool force = false;
};

struct AddSkinResult
{
    int new_skin_index = -1;
    int texture_index = -1;
    bool texture_added = false;
    std::filesystem::path output_path;
    std::vector<int> updated_skinrefs;
};

struct RemoveSkinResult
{
    int removed_skin_index = -1;
    std::vector<std::int16_t> removed_family;
};

struct ReplaceTextureResult
{
    int texture_index = -1;
    TextureData previous_texture;
    TextureData current_texture;
    std::vector<std::string> usage_descriptions;
};

class SkinFamilyEditor
{
public:
    static AddSkinResult add_skin(MdlDocument &document, const AddSkinOptions &options);
    static RemoveSkinResult remove_skin_family(MdlDocument &document, int skin_index);
    static ReplaceTextureResult replace_texture(
        MdlDocument &document,
        int texture_index,
        const std::filesystem::path &bmp_path,
        bool keep_name);
    static std::vector<std::string> describe_texture_usage(
        const MdlDocument &document,
        int texture_index);
};
} // namespace mdl
