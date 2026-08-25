#pragma once

#include "editor_state.hpp"
#include "texture_preview.hpp"

namespace gui
{
class InspectorWindow
{
public:
    void draw(EditorState &state, TexturePreview &preview);
};
} // namespace gui
