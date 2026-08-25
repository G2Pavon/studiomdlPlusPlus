#include "windows/inspector_window.hpp"

#include <algorithm>

#include "imgui.h"

#include "format/mdl.hpp"
#include "gui_strings.hpp"

namespace gui
{
namespace
{
ImVec4 background_color(TexturePreview::BackgroundMode mode)
{
    switch (mode)
    {
    case TexturePreview::BackgroundMode::Black:
        return ImVec4(0.f, 0.f, 0.f, 1.f);
    case TexturePreview::BackgroundMode::White:
        return ImVec4(1.f, 1.f, 1.f, 1.f);
    case TexturePreview::BackgroundMode::Transparent:
        return ImVec4(0.f, 0.f, 0.f, 0.f);
    case TexturePreview::BackgroundMode::Checkerboard:
    default:
        return ImVec4(0.16f, 0.16f, 0.18f, 1.f);
    }
}
} // namespace

void InspectorWindow::draw(EditorState &state, TexturePreview &preview)
{
    if (!ImGui::Begin(strings::kWindowInspector))
    {
        ImGui::End();
        return;
    }

    const auto *document = state.document();
    if (!document)
    {
        ImGui::TextUnformatted("Open a model to inspect textures and selection details.");
        ImGui::End();
        return;
    }

    int texture_index = state.selection().texture_index;
    if (texture_index < 0 && state.selection().submodel_index >= 0 && state.selection().mesh_index >= 0)
    {
        const auto &submodel = document->submodels[static_cast<std::size_t>(state.selection().submodel_index)];
        if (state.selection().mesh_index < static_cast<int>(submodel.meshes.size()))
        {
            texture_index = submodel.meshes[static_cast<std::size_t>(state.selection().mesh_index)].family0_texture_index;
        }
    }

    if (texture_index < 0 || texture_index >= static_cast<int>(document->textures.size()))
    {
        ImGui::TextUnformatted("Select a texture or mesh to inspect.");
        ImGui::End();
        return;
    }

    const auto &texture = document->textures[static_cast<std::size_t>(texture_index)];
    preview.ensure_uploaded(texture);

    ImGui::Text("Name: %s", texture.name().c_str());
    ImGui::Text("Texture index: %d", texture_index);
    ImGui::Text("Size: %d x %d", texture.header.width, texture.header.height);
    ImGui::Text("Format: Indexed 8-bit BMP palette");
    ImGui::Text("Flags: 0x%X", texture.header.flags);
    ImGui::Text("Colors: 256");

    const auto usages = state.texture_usage(texture_index);
    if (!usages.empty())
    {
        ImGui::Separator();
        ImGui::TextUnformatted("Usage:");
        for (const auto &usage : usages)
        {
            ImGui::BulletText("%s", usage.c_str());
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Preview");
    float zoom = preview.zoom();
    if (ImGui::SliderFloat("Zoom", &zoom, 0.25f, 8.0f, "%.0f%%", ImGuiSliderFlags_None))
    {
        preview.set_zoom(zoom);
    }

    const char *backgrounds[] = {"Checkerboard", "Transparent", "Black", "White"};
    int background_index = static_cast<int>(preview.background_mode());
    if (ImGui::Combo("Background", &background_index, backgrounds, IM_ARRAYSIZE(backgrounds)))
    {
        preview.set_background_mode(static_cast<TexturePreview::BackgroundMode>(background_index));
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float draw_width = std::min(available.x, static_cast<float>(preview.width()) * preview.zoom());
    const float draw_height = std::min(available.y - 8.0f, static_cast<float>(preview.height()) * preview.zoom());
    if (draw_width > 0 && draw_height > 0)
    {
        ImGui::ColorButton(
            "bg",
            background_color(preview.background_mode()),
            ImGuiColorEditFlags_NoTooltip,
            ImVec2(draw_width, draw_height));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - draw_height);
        ImGui::Image(
            reinterpret_cast<void *>(static_cast<uintptr_t>(preview.texture_id())),
            ImVec2(draw_width, draw_height));
    }

    ImGui::End();
}
} // namespace gui
