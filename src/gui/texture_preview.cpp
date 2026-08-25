#include "texture_preview.hpp"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <algorithm>
#include <cstring>

#include "format/mdl.hpp"

namespace gui
{
namespace
{
std::size_t make_cache_key(const mdl::TextureData &texture)
{
    std::size_t hash = static_cast<std::size_t>(texture.header.width) * 1315423911u;
    hash ^= static_cast<std::size_t>(texture.header.height) << 1U;
    hash ^= static_cast<std::size_t>(texture.header.flags) << 2U;
    if (!texture.data.empty())
    {
        hash ^= static_cast<std::size_t>(texture.data.size()) << 3U;
        hash ^= static_cast<std::size_t>(texture.data.front());
        hash ^= static_cast<std::size_t>(texture.data.back()) << 4U;
    }
    return hash;
}
} // namespace

TexturePreview::TexturePreview() = default;

TexturePreview::~TexturePreview()
{
    clear();
}

void TexturePreview::clear()
{
    if (texture_id_ != 0)
    {
        glDeleteTextures(1, &texture_id_);
        texture_id_ = 0;
    }
    width_ = 0;
    height_ = 0;
    cache_key_ = 0;
    rgba_.clear();
}

void TexturePreview::ensure_uploaded(const mdl::TextureData &texture)
{
    const auto key = make_cache_key(texture);
    if (texture_id_ != 0 && key == cache_key_)
    {
        return;
    }
    upload_rgba(texture);
    cache_key_ = key;
}

unsigned int TexturePreview::texture_id() const
{
    return texture_id_;
}

int TexturePreview::width() const
{
    return width_;
}

int TexturePreview::height() const
{
    return height_;
}

float TexturePreview::zoom() const
{
    return zoom_;
}

void TexturePreview::set_zoom(float value)
{
    zoom_ = std::clamp(value, 0.25f, 8.0f);
}

TexturePreview::BackgroundMode TexturePreview::background_mode() const
{
    return background_mode_;
}

void TexturePreview::set_background_mode(BackgroundMode mode)
{
    background_mode_ = mode;
}

void TexturePreview::upload_rgba(const mdl::TextureData &texture)
{
    clear();

    width_ = texture.header.width;
    height_ = texture.header.height;
    const auto pixel_count = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    rgba_.resize(pixel_count * 4U, 255U);

    const auto *indexed = reinterpret_cast<const std::uint8_t *>(texture.data.data());
    const auto *palette = reinterpret_cast<const std::uint8_t *>(texture.data.data() + pixel_count);

    for (std::size_t i = 0; i < pixel_count; ++i)
    {
        const auto color_index = indexed[i];
        const auto pal = static_cast<std::size_t>(color_index) * 3U;
        rgba_[i * 4U + 0U] = palette[pal + 0U];
        rgba_[i * 4U + 1U] = palette[pal + 1U];
        rgba_[i * 4U + 2U] = palette[pal + 2U];
        rgba_[i * 4U + 3U] =
            ((texture.header.flags & STUDIO_NF_MASKED) != 0 && color_index == 255U) ? 0U : 255U;
    }

    glGenTextures(1, &texture_id_);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width_,
        height_,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgba_.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}
} // namespace gui
