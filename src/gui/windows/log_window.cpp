#include "windows/log_window.hpp"

#include "imgui.h"

#include "gui_strings.hpp"

namespace gui
{
namespace
{
ImVec4 color_for(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Success:
        return ImVec4(0.35f, 0.85f, 0.45f, 1.0f);
    case LogLevel::Warning:
        return ImVec4(0.93f, 0.76f, 0.28f, 1.0f);
    case LogLevel::Error:
        return ImVec4(0.93f, 0.35f, 0.35f, 1.0f);
    case LogLevel::Info:
    default:
        return ImVec4(0.80f, 0.84f, 0.90f, 1.0f);
    }
}
} // namespace

void LogWindow::draw(EditorState &state)
{
    if (!ImGui::Begin(strings::kWindowLog))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear"))
    {
        state.clear_log();
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy"))
    {
        ImGui::SetClipboardText(state.copyable_log().c_str());
    }
    ImGui::Separator();

    for (const auto &entry : state.log())
    {
        ImGui::TextColored(color_for(entry.level), "%s", entry.message.c_str());
    }

    ImGui::End();
}
} // namespace gui
