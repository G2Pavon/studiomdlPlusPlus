#include "windows/skins_window.hpp"

#include "imgui.h"

#include "gui_strings.hpp"

namespace gui
{
void SkinsWindow::draw(EditorState &state)
{
    if (!ImGui::Begin(strings::kWindowSkins))
    {
        ImGui::End();
        return;
    }

    const auto *document = state.document();
    if (!document)
    {
        ImGui::TextUnformatted("Open a model to inspect skin families.");
        ImGui::End();
        return;
    }

    if (document->skin_families.empty())
    {
        ImGui::TextUnformatted("No skin families.");
        ImGui::End();
        return;
    }

    const auto column_count = static_cast<int>(document->header.numskinref) + 1;
    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollX | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("skins_table", column_count, flags))
    {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn("Skin", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        for (int skinref = 0; skinref < document->header.numskinref; ++skinref)
        {
            const std::string label = "skinref " + std::to_string(skinref);
            ImGui::TableSetupColumn(label.c_str(), ImGuiTableColumnFlags_WidthFixed, 160.0f);
        }
        ImGui::TableHeadersRow();

        for (std::size_t family_index = 0; family_index < document->skin_families.size(); ++family_index)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const bool selected = state.selection().skin_family_index == static_cast<int>(family_index);
            const std::string row_label = std::to_string(family_index);
            if (ImGui::Selectable(row_label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
            {
                state.selection().skin_family_index = static_cast<int>(family_index);
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (family_index > 0 && ImGui::MenuItem("Copy reference"))
                {
                    ImGui::SetClipboardText(
                        state.make_reference(
                                 state.selection().submodel_index >= 0 ? state.selection().submodel_index : 0,
                                 static_cast<int>(family_index))
                            .c_str());
                }
                ImGui::EndPopup();
            }

            const auto &family = document->skin_families[family_index];
            for (std::size_t skinref = 0; skinref < family.size(); ++skinref)
            {
                ImGui::TableNextColumn();
                const auto texture_index = family[skinref];
                const auto &texture = document->textures[static_cast<std::size_t>(texture_index)];
                ImGui::TextUnformatted(texture.name().c_str());
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Texture %d", texture_index);
                }
            }
        }
        ImGui::EndTable();
    }

    ImGui::End();
}
} // namespace gui
