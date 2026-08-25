#include "windows/model_tree_window.hpp"

#include <string>

#include "imgui.h"

#include "gui_strings.hpp"
#include "utils/cmdlib.hpp"

namespace gui
{
void ModelTreeWindow::draw(EditorState &state)
{
    if (!ImGui::Begin(strings::kWindowModelTree))
    {
        ImGui::End();
        return;
    }

    const auto *document = state.document();
    if (!document)
    {
        ImGui::TextUnformatted("Open a model to inspect its structure.");
        ImGui::End();
        return;
    }

    const auto tree_label =
        path_to_utf8(state.current_path().empty() ? state.original_path() : state.current_path());
    if (ImGui::TreeNode(tree_label.c_str()))
    {
        for (const auto &submodel : document->submodels)
        {
            const std::string label =
                "Bodypart: " + submodel.bodypart_name + " / [" +
                std::to_string(submodel.global_index) + "] " + submodel.model_name;
            if (ImGui::TreeNode(label.c_str()))
            {
                for (const auto &mesh : submodel.meshes)
                {
                    const bool selected =
                        state.selection().submodel_index == submodel.global_index &&
                        state.selection().mesh_index == mesh.mesh_index;
                    const std::string mesh_label =
                        "Mesh " + std::to_string(mesh.mesh_index) +
                        " - skinref " + std::to_string(mesh.mesh.skinref) +
                        " - " + mesh.family0_texture_name;
                    if (ImGui::Selectable(mesh_label.c_str(), selected))
                    {
                        state.selection().submodel_index = submodel.global_index;
                        state.selection().mesh_index = mesh.mesh_index;
                        state.selection().texture_index = mesh.family0_texture_index;
                        state.selection().skinref_index = mesh.mesh.skinref;
                    }
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    ImGui::End();
}
} // namespace gui
