#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include "editor_state.hpp"

namespace gui
{
class AddSkinDialog
{
public:
    void open(EditorState &state);
    void draw(EditorState &state, const std::function<std::filesystem::path()> &browse_bmp);

private:
    bool open_ = false;
    int submodel_index_ = -1;
    int mesh_index_ = 0;
    int copy_skin_family_ = 0;
    std::filesystem::path bmp_path_;
    std::string result_reference_;
    int result_texture_index_ = -1;
    int result_skin_index_ = -1;
};
} // namespace gui
