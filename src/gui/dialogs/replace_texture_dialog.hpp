#pragma once

#include <filesystem>
#include <functional>

#include "editor_state.hpp"

namespace gui
{
class ReplaceTextureDialog
{
public:
    void open(EditorState &state);
    void draw(EditorState &state, const std::function<std::filesystem::path()> &browse_bmp);

private:
    bool open_ = false;
    int texture_index_ = -1;
    bool keep_name_ = true;
    std::filesystem::path bmp_path_;
};
} // namespace gui
