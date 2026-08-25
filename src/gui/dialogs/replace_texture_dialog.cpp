#include "dialogs/replace_texture_dialog.hpp"

#include <algorithm>
#include <cstring>

#include "imgui.h"

#include "gui_strings.hpp"
#include "mdl/texture_importer.hpp"

namespace gui
{
void ReplaceTextureDialog::open(EditorState &state)
{
    texture_index_ = state.selection().texture_index;
    keep_name_ = true;
    bmp_path_.clear();
    open_ = true;
    ImGui::OpenPopup(strings::kDialogReplaceTexture);
}

void ReplaceTextureDialog::draw(EditorState &state, const std::function<std::filesystem::path()> &browse_bmp)
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

    if (texture_index_ < 0)
    {
        texture_index_ = 0;
    }

    if (ImGui::BeginPopupModal(strings::kDialogReplaceTexture, &open_, ImGuiWindowFlags_AlwaysAutoResize))
    {
        std::vector<const char *> labels;
        std::vector<std::string> storage;
        for (std::size_t i = 0; i < document->textures.size(); ++i)
        {
            storage.push_back(std::to_string(i) + " - " + document->textures[i].name());
        }
        for (const auto &item : storage)
        {
            labels.push_back(item.c_str());
        }
        ImGui::Combo("Current texture", &texture_index_, labels.data(), static_cast<int>(labels.size()));

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

        ImGui::Checkbox("Keep name", &keep_name_);

        const auto usages = state.texture_usage(texture_index_);
        ImGui::Separator();
        ImGui::Text("Texture %s is used by:", document->textures[static_cast<std::size_t>(texture_index_)].name().c_str());
        for (const auto &usage : usages)
        {
            ImGui::BulletText("%s", usage.c_str());
        }

        bool compatible = false;
        if (!bmp_path_.empty() && std::filesystem::exists(bmp_path_))
        {
            try
            {
                const auto imported = mdl::TextureImporter::import_bmp(bmp_path_);
                compatible = true;
                if (imported.texture.header.width != document->textures[static_cast<std::size_t>(texture_index_)].header.width ||
                    imported.texture.header.height != document->textures[static_cast<std::size_t>(texture_index_)].header.height)
                {
                    ImGui::TextColored(
                        ImVec4(0.93f, 0.76f, 0.28f, 1.0f),
                        "Warning: new texture dimensions differ from the current texture.");
                }
            }
            catch (const std::exception &exception)
            {
                ImGui::TextColored(ImVec4(0.93f, 0.35f, 0.35f, 1.0f), "%s", exception.what());
            }
        }

        ImGui::BeginDisabled(!compatible);
        if (ImGui::Button("Replace"))
        {
            ReplaceTextureRequest request;
            request.texture_index = texture_index_;
            request.bmp_path = bmp_path_;
            request.keep_name = keep_name_;
            if (state.replace_texture(request))
            {
                open_ = false;
                ImGui::CloseCurrentPopup();
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
