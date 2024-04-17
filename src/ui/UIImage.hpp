/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_UI_IMAGE_HPP
#define HYPERION_UI_IMAGE_HPP

#include <ui/UIObject.hpp>

#include <rendering/Texture.hpp>

namespace hyperion {

class UIStage;

#pragma region UIImage

class HYP_API UIImage : public UIObject
{
public:
    UIImage(ID<Entity> entity, UIStage *stage, NodeProxy node_proxy);
    UIImage(const UIImage &other)                   = delete;
    UIImage &operator=(const UIImage &other)        = delete;
    UIImage(UIImage &&other) noexcept               = delete;
    UIImage &operator=(UIImage &&other) noexcept    = delete;
    virtual ~UIImage() override                     = default;

    const Handle<Texture> &GetTexture() const
        { return m_texture; }

    void SetTexture(Handle<Texture> texture);

protected:
    virtual Handle<Material> GetMaterial() const override;

    Handle<Texture> m_texture;
};

#pragma endregion UIImage

} // namespace hyperion

#endif