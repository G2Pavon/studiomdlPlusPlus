#include "dialogs/add_skin_dialog.hpp"

#include <algorithm>
#include <cstring>
#include <functional>

#include "imgui.h"

#include "gui_strings.hpp"
#include "mdl/texture_importer.hpp"

namespace gui
{
void AddSkinDialog::open(EditorState &state)
{
    open_ = true;
    const auto &selection = state.selection();
    submodel_index_ = selection.submodel_index >= 0 ? selection.submodel_index : 0;
    mesh_index_ = selection.mesh_index >= 0 ? selection.mesh_index : 0;
    copy_skin_family_ = 0;
    bmp_path_.clear();
    result_reference_.clear();
    result_texture_index_ = -1;
    result_skin_index_ = -1;
    ImGui::OpenPopup(strings::kDialogAddSkin);
}

void AddSkinDialog::draw(EditorState &state, const std::function<std::filesystem::path()> &browse_bmp)
{
    if (!open_)
    {
        return;
    }

    const auto *document = state.document();
    if (!document)
    {
        open_ = false;
        return;
    }

    if (ImGui::BeginPopupModal(strings::kDialogAddSkin, &open_, ImGuiWindowFlags_AlwaysAutoResize))
    {
        std::vector<const char *> submodels;
        submodels.reserve(document->submodels.size());
        std::vector<std::string> submodel_labels;
        for (const auto &submodel : document->submodels)
        {
            submodel_labels.push_back(
                std::to_string(submodel.global_index) + " - " + submodel.model_name);
        }
        for (const auto &label : submodel_labels)
        {
            submodels.push_back(label.c_str());
        }
        ImGui::Combo("Submodel", &submodel_index_, submodels.data(), static_cast<int>(submodels.size()));

        const auto &submodel = document->submodels[static_cast<std::size_t>(submodel_index_)];
        std::vector<const char *> meshes;
        std::vector<std::string> mesh_labels;
        for (const auto &mesh : submodel.meshes)
        {
            mesh_labels.push_back(
                "Mesh " + std::to_string(mesh.mesh_index) +
                " - " + mesh.family0_texture_name);
        }
        for (const auto &label : mesh_labels)
        {
            meshes.push_back(label.c_str());
        }
        if (mesh_index_ >= static_cast<int>(meshes.size()))
        {
            mesh_index_ = 0;
        }
        ImGui::Combo("Target mesh", &mesh_index_, meshes.data(), static_cast<int>(meshes.size()));
        ImGui::Text("Target texture: %s", submodel.meshes[static_cast<std::size_t>(mesh_index_)].family0_texture_name.c_str());

        ImGui::SliderInt(
            "Copy skin family",
            &copy_skin_family_,
            0,
            static_cast<int>(document->skin_families.size()) - 1);

        char path_buffer[1024]{};
        const auto path_string = bmp_path_.string();
        std::memcpy(path_buffer, path_string.c_str(), std::min(path_string.size(), sizeof(path_buffer) - 1));
        ImGui::InputText("New BMP", path_buffer, sizeof(path_buffer));
        bmp_path_ = path_buffer;
        ImGui::SameLine();
        if (ImGui::Button("Browse"))
        {
            const auto selected = browse_bmp();
            if (!selected.empty())
            {
                bmp_path_ = selected;
            }
        }

        const int output_skin_index = static_cast<int>(document->skin_families.size());
        ImGui::Text("Output skin index: %d", output_skin_index);

        bool compatible = false;
        std::string status = "Select an indexed 8-bit BMP.";
        int imported_width = 0;
        int imported_height = 0;
        if (!bmp_path_.empty() && std::filesystem::exists(bmp_path_))
        {
            try
            {
                const auto imported = mdl::TextureImporter::import_bmp(bmp_path_);
                imported_width = imported.texture.header.width;
                imported_height = imported.texture.header.height;
                compatible = true;
                status = "Compatible";
                ImGui::Text("Name: %s", imported.name.c_str());
                ImGui::Text("Format: Indexed 8-bit");
                ImGui::Text("Size: %d x %d", imported_width, imported_height);
                ImGui::Text("Colors: 256");
                ImGui::Text("File size: %llu bytes", static_cast<unsigned long long>(std::filesystem::file_size(bmp_path_)));
            }
            catch (const std::exception &exception)
            {
                status = exception.what();
            }
        }
        ImGui::Text("Status: %s", status.c_str());

        if (!result_reference_.empty())
        {
            ImGui::Separator();
            ImGui::TextUnformatted("Skin successfully added");
            ImGui::Text("Texture index: %d", result_texture_index_);
            ImGui::Text("Skin index: %d", result_skin_index_);
            ImGui::Text("Reference: %s", result_reference_.c_str());
            if (ImGui::Button("Copy reference"))
            {
                ImGui::SetClipboardText(result_reference_.c_str());
            }
        }

        ImGui::BeginDisabled(!compatible);
        if (ImGui::Button("Add skin"))
        {
            AddSkinRequest request;
            request.submodel_index = submodel_index_;
            request.mesh_index = mesh_index_;
            request.copy_skin_family = copy_skin_family_;
            request.bmp_path = bmp_path_;
            if (state.add_skin(request))
            {
                result_skin_index_ = static_cast<int>(state.document()->skin_families.size()) - 1;
                result_texture_index_ = state.selection().texture_index;
                result_reference_ = state.make_reference(submodel_index_, result_skin_index_);
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Close"))
        {
            open_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
} // namespace gui
