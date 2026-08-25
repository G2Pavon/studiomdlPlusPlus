#pragma once

#include <cstdint>
#include <vector>

#include "mdl/mdl_document.hpp"

namespace gui
{
class TexturePreview
{
public:
    enum class BackgroundMode
    {
        Checkerboard,
        Transparent,
        Black,
        White
    };

    TexturePreview();
    ~TexturePreview();

    void clear();
    void ensure_uploaded(const mdl::TextureData &texture);

    [[nodiscard]] unsigned int texture_id() const;
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] float zoom() const;
    void set_zoom(float value);
    [[nodiscard]] BackgroundMode background_mode() const;
    void set_background_mode(BackgroundMode mode);

private:
    void upload_rgba(const mdl::TextureData &texture);

    unsigned int texture_id_ = 0;
    int width_ = 0;
    int height_ = 0;
    std::size_t cache_key_ = 0;
    float zoom_ = 1.0f;
    BackgroundMode background_mode_ = BackgroundMode::Checkerboard;
    std::vector<std::uint8_t> rgba_;
};
} // namespace gui
