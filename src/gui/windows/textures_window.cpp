#include "windows/textures_window.hpp"

#include "imgui.h"

#include "gui_strings.hpp"

namespace gui
{
void TexturesWindow::draw(EditorState &state)
{
    if (!ImGui::Begin(strings::kWindowTextures))
    {
        ImGui::End();
        return;
    }

    const auto *document = state.document();
    if (!document)
    {
        ImGui::TextUnformatted("Open a model to inspect textures.");
        ImGui::End();
        return;
    }

    for (std::size_t i = 0; i < document->textures.size(); ++i)
    {
        const auto &texture = document->textures[i];
        const bool selected = state.selection().texture_index == static_cast<int>(i);
        const std::string label =
            std::to_string(i) + ": " + texture.name() +
            " [" + std::to_string(texture.header.width) + "x" +
            std::to_string(texture.header.height) + "]";
        if (ImGui::Selectable(label.c_str(), selected))
        {
            state.selection().texture_index = static_cast<int>(i);
        }
    }

    ImGui::End();
}
} // namespace gui
