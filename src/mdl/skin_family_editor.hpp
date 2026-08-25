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
    std::filesystem::path output_path;
    std::vector<int> updated_skinrefs;
};

class SkinFamilyEditor
{
public:
    static AddSkinResult add_skin(MdlDocument &document, const AddSkinOptions &options);
};
} // namespace mdl
