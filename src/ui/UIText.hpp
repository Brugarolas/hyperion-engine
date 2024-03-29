#ifndef HYPERION_V2_UI_TEXT_H
#define HYPERION_V2_UI_TEXT_H

#include <ui/UIObject.hpp>
#include <ui/UIScene.hpp>

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/Handle.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/Texture.hpp>

#include <math/Transform.hpp>
#include <math/Vector2.hpp>

#include <scene/Scene.hpp>

#include <core/Containers.hpp>
#include <GameCounter.hpp>

#include <Types.hpp>

namespace hyperion {

class SystemEvent;
class InputManager;

} // namespace hyperion

namespace hyperion::v2 {

class FontMap
{
public:
    FontMap(
        const Handle<Texture> &texture,
        Extent2D char_size
    ) : m_texture(texture),
        m_char_size(char_size)
    {
        int x_position = 0;
        int y_position = 0;

        const uint num_chars_per_row = NumCharsPerRow();
        const uint num_chars_per_col = NumCharsPerCol();

        for (int ch = 32; ch < 256; ch++) {
            m_char_texture_coords[ch] = Vec2i {
                x_position,
                y_position
            };

            if (x_position == num_chars_per_row - 1) {
                x_position = 0;

                if (y_position == num_chars_per_col - 1) {
                    break;
                }

                y_position++;
            } else {
                x_position++;
            }
        }
    }

    FontMap(const FontMap &other)
        : m_texture(other.m_texture),
          m_char_size(other.m_char_size),
          m_char_texture_coords(other.m_char_texture_coords)
    {
    }

    FontMap &operator=(const FontMap &other)
    {
        m_texture = other.m_texture;
        m_char_size = other.m_char_size;
        m_char_texture_coords = other.m_char_texture_coords;

        return *this;
    }

    FontMap(FontMap &&other) noexcept
        : m_texture(std::move(other.m_texture)),
          m_char_size(other.m_char_size),
          m_char_texture_coords(std::move(other.m_char_texture_coords))
    {
    }

    FontMap &operator=(FontMap &&other) noexcept
    {
        m_texture = std::move(other.m_texture);
        m_char_size = other.m_char_size;
        m_char_texture_coords = std::move(other.m_char_texture_coords);

        return *this;
    }

    ~FontMap() = default;

    Handle<Texture> &GetTexture()
        { return m_texture; }

    const Handle<Texture> &GetTexture() const
        { return m_texture; }

    uint NumCharsPerRow() const
    {
        if (!m_texture || m_char_size.width == 0) {
            return 0;
        }

        return m_texture->GetExtent().width / m_char_size.width;
    }

    uint NumCharsPerCol() const
    {
        if (!m_texture || m_char_size.height == 0) {
            return 0;
        }

        return m_texture->GetExtent().height / m_char_size.height;
    }

    Vec2f GetCharOffset(char ch) const
    {
        return Vec2f(m_char_texture_coords[ch]) / Vec2f(NumCharsPerCol(), NumCharsPerRow());
    }

    Vec2f GetScaling() const
    {
        if (!m_texture) {
            return Vec2f::zero;
        }

        const Extent3D extent = m_texture->GetExtent();

        return Vec2f(m_char_size) / Vec2f(float(extent.width), float(extent.height));
    }

private:
    Handle<Texture>         m_texture;
    Extent2D                m_char_size;
    FixedArray<Vec2i, 256>  m_char_texture_coords;
};

class UIText : public UIObject
{
    static Handle<Mesh> BuildTextMesh(const FontMap &font_map, const String &text);

public:
    UIText(ID<Entity> entity, UIScene *ui_scene);
    UIText(const UIText &other)                 = delete;
    UIText &operator=(const UIText &other)      = delete;
    UIText(UIText &&other) noexcept             = delete;
    UIText &operator=(UIText &&other) noexcept  = delete;
    virtual ~UIText() override                  = default;

    virtual void Init() override;

    const String &GetText() const
        { return m_text; }

    void SetText(const String &text);

    const RC<FontMap> &GetFontMap() const
        { return m_font_map; }

    void SetFontMap(RC<FontMap> font_map);

protected:
    virtual Handle<Material> GetMaterial() const override;

    void UpdateMesh(bool update_material = false);

    String      m_text;
    RC<FontMap> m_font_map;
};

} // namespace hyperion::v2

#endif