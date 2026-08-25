#include "gui_theme.hpp"

#include "imgui.h"

namespace gui
{
void apply_gui_theme()
{
    ImGui::StyleColorsDark();
    auto &style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.CellPadding = ImVec2(6.0f, 4.0f);

    auto &colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.0f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.13f, 0.15f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.13f, 0.15f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.24f, 0.26f, 0.31f, 0.8f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.17f, 0.18f, 0.21f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.29f, 0.36f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.35f, 0.50f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.15f, 0.18f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.24f, 0.33f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.23f, 0.34f, 0.48f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.40f, 0.57f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.32f, 0.50f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.40f, 0.61f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.17f, 0.28f, 0.43f, 1.0f);
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.17f, 0.20f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.28f, 0.40f, 1.0f);
    colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.32f, 0.50f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.67f, 0.92f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.67f, 0.92f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.18f, 0.49f, 0.74f, 1.0f);
}
} // namespace gui
