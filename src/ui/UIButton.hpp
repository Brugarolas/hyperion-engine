/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_V2_UI_BUTTON_HPP
#define HYPERION_V2_UI_BUTTON_HPP

#include <ui/UIObject.hpp>

namespace hyperion::v2 {

class UIStage;

// UIButton

class HYP_API UIButton : public UIObject
{
public:
    UIButton(ID<Entity> entity, UIStage *stage, NodeProxy node_proxy);
    UIButton(const UIButton &other)                 = delete;
    UIButton &operator=(const UIButton &other)      = delete;
    UIButton(UIButton &&other) noexcept             = delete;
    UIButton &operator=(UIButton &&other) noexcept  = delete;
    virtual ~UIButton() override                    = default;

protected:
    virtual Handle<Material> GetMaterial() const override;
};

} // namespace hyperion::v2

#endif