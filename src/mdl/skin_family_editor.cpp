#include "mdl/skin_family_editor.hpp"

#include <algorithm>
#include <stdexcept>

#include "mdl/texture_importer.hpp"
#include "utils/cmdlib.hpp"

namespace mdl
{
namespace
{
bool textures_equal(const TextureData &lhs, const TextureData &rhs)
{
    return lhs.header.width == rhs.header.width &&
           lhs.header.height == rhs.header.height &&
           lhs.data == rhs.data;
}

std::string base_name(const std::filesystem::path &path)
{
    return path.filename().string();
}
} // namespace

AddSkinResult SkinFamilyEditor::add_skin(MdlDocument &document, const AddSkinOptions &options)
{
    if (options.copy_skin_index < 0 ||
        options.copy_skin_index >= static_cast<int>(document.skin_families.size()))
    {
        throw std::runtime_error(
            "Error: copy skin index " + std::to_string(options.copy_skin_index) + " is out of range.");
    }

    if (options.submodel_index < 0 ||
        options.submodel_index >= static_cast<int>(document.submodels.size()))
    {
        throw std::runtime_error(
            "Error: submodel index " + std::to_string(options.submodel_index) +
            " is out of range. Available range: 0.." +
            std::to_string(std::max(0, static_cast<int>(document.submodels.size()) - 1)) + ".");
    }

    const auto imported = TextureImporter::import_bmp(options.bmp_path);
    auto &submodel = document.submodels[static_cast<std::size_t>(options.submodel_index)];

    std::vector<int> matching_skinrefs;
    for (const auto &mesh : submodel.meshes)
    {
        if (case_insensitive_compare(mesh.family0_texture_name, options.target_texture))
        {
            matching_skinrefs.push_back(mesh.mesh.skinref);
        }
    }

    if (matching_skinrefs.empty())
    {
        throw std::runtime_error(
            "Error: texture \"" + options.target_texture + "\" is not used by submodel " +
            std::to_string(options.submodel_index) + ".");
    }

    std::sort(matching_skinrefs.begin(), matching_skinrefs.end());
    matching_skinrefs.erase(
        std::unique(matching_skinrefs.begin(), matching_skinrefs.end()), matching_skinrefs.end());

    if (matching_skinrefs.size() > 1 && !options.all_matches)
    {
        throw std::runtime_error(
            "Error: texture \"" + options.target_texture +
            "\" matches multiple meshes in submodel " + std::to_string(options.submodel_index) +
            ".\nUse --all-matches to update all matching skin references.");
    }

    int texture_index = -1;
    for (std::size_t index = 0; index < document.textures.size(); ++index)
    {
        if (case_insensitive_compare(document.textures[index].name(), imported.name))
        {
            if (!textures_equal(document.textures[index], imported.texture))
            {
                throw std::runtime_error(
                    "Error: texture \"" + imported.name +
                    "\" already exists in the model with different content. Rename the BMP and try again.");
            }
            texture_index = static_cast<int>(index);
            break;
        }
    }

    if (texture_index == -1)
    {
        texture_index = static_cast<int>(document.textures.size());
        document.textures.push_back(imported.texture);
    }

    auto new_family = document.skin_families[static_cast<std::size_t>(options.copy_skin_index)];
    for (const auto skinref : matching_skinrefs)
    {
        new_family[static_cast<std::size_t>(skinref)] = static_cast<std::int16_t>(texture_index);
    }
    document.skin_families.push_back(std::move(new_family));

    AddSkinResult result;
    result.new_skin_index = static_cast<int>(document.skin_families.size()) - 1;
    result.texture_index = texture_index;
    result.output_path = options.output_path;
    result.updated_skinrefs = matching_skinrefs;
    return result;
}
} // namespace mdl
