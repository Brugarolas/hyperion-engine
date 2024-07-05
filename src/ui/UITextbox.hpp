/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_TEXTBOX_HPP
#define HYPERION_UI_TEXTBOX_HPP

#include <ui/UIPanel.hpp>
#include <ui/UIText.hpp>

#include <core/containers/String.hpp>

#include <core/memory/RefCountedPtr.hpp>

namespace hyperion {

class HYP_API UITextbox : public UIPanel
{
public:
    UITextbox(UIStage *stage, NodeProxy node_proxy);
    UITextbox(const UITextbox &other)                   = delete;
    UITextbox &operator=(const UITextbox &other)        = delete;
    UITextbox(UITextbox &&other) noexcept               = delete;
    UITextbox &operator=(UITextbox &&other) noexcept    = delete;
    virtual ~UITextbox() override                       = default;

    /*! \brief Gets the text value of the textbox.
     * 
     * \return The text value of the textbox. */
    HYP_FORCE_INLINE
    const String &GetText() const
        { return m_text; }

    /*! \brief Sets the text value of the textbox.
     * 
     * \param text The text to set. */
    void SetText(const String &text);

    virtual void Init() override;

protected:
    String      m_text;
    RC<UIText>  m_text_element;
};


} // namespace hyperion

#endif