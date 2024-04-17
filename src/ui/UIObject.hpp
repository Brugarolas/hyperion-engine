/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_V2_UI_OBJECT_HPP
#define HYPERION_V2_UI_OBJECT_HPP

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <core/lib/Delegate.hpp>

#include <scene/Node.hpp>
#include <scene/NodeProxy.hpp>
#include <scene/Scene.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/FullScreenPass.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class UIStage;

// Helper function to get the scene from a UIStage
template <class UIStageType>
static inline Scene *GetScene(UIStageType *stage)
{
    AssertThrow(stage != nullptr);

    return stage->GetScene().Get();
}

// UIObject

enum UIObjectAlignment : uint32
{
    UI_OBJECT_ALIGNMENT_TOP_LEFT        = 0,
    UI_OBJECT_ALIGNMENT_TOP_RIGHT       = 1,

    UI_OBJECT_ALIGNMENT_CENTER          = 2,

    UI_OBJECT_ALIGNMENT_BOTTOM_LEFT     = 3,
    UI_OBJECT_ALIGNMENT_BOTTOM_RIGHT    = 4
};

using UIObjectFocusState = uint32;

enum UIObjectFocusStateBits : UIObjectFocusState
{
    UI_OBJECT_FOCUS_STATE_NONE          = 0x0,
    UI_OBJECT_FOCUS_STATE_HOVER         = 0x1,
    UI_OBJECT_FOCUS_STATE_PRESSED       = 0x2,
    UI_OBJECT_FOCUS_STATE_TOGGLED       = 0x4
};

using UIObjectBorderFlags = uint32;

enum UIObjectBorderFlagBits : UIObjectBorderFlags
{
    UI_OBJECT_BORDER_NONE   = 0x00,
    UI_OBJECT_BORDER_TOP    = 0x01,
    UI_OBJECT_BORDER_LEFT   = 0x02,
    UI_OBJECT_BORDER_BOTTOM = 0x04,
    UI_OBJECT_BORDER_RIGHT  = 0x08,
    UI_OBJECT_BORDER_ALL    = UI_OBJECT_BORDER_TOP | UI_OBJECT_BORDER_LEFT | UI_OBJECT_BORDER_BOTTOM | UI_OBJECT_BORDER_RIGHT
};

struct UIObjectSize
{
    using Flags = uint32;

    enum FlagBits : Flags
    {
        GROW     = 0x04,

        PIXEL    = 0x10,
        PERCENT  = 0x20,

        DEFAULT  = PIXEL
    };

    UIObjectSize()
        : flags { DEFAULT, DEFAULT },
          value(0, 0)
    {
    }

    UIObjectSize(Vec2i value)
        : flags { DEFAULT, DEFAULT },
          value(value)
    {
    }

    UIObjectSize(Vec2i value, Flags flags)
        : flags { flags, flags },
          value(value)
    {
        ApplyDefaultFlags();
    }

    /*! \brief Construct by only providing flags. Used primarily for the DYNAMIC type. */
    UIObjectSize(Flags flags)
        : flags { flags, flags },
          value(0, 0)
    {
        ApplyDefaultFlags();
    }

    /*! \brief Construct by providing specific flags for each axis. */
    UIObjectSize(Pair<int, Flags> x, Pair<int, Flags> y)
        : flags { x.second, y.second },
          value(x.first, y.first)
    {
        ApplyDefaultFlags();
    }

    UIObjectSize(const UIObjectSize &other)                 = default;
    UIObjectSize &operator=(const UIObjectSize &other)      = default;
    UIObjectSize(UIObjectSize &&other) noexcept             = default;
    UIObjectSize &operator=(UIObjectSize &&other) noexcept  = default;
    ~UIObjectSize()                                         = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Vec2i &GetValue() const
        { return value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Flags GetFlagsX() const
        { return flags[0]; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Flags GetFlagsY() const
        { return flags[1]; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    Flags GetAllFlags() const
        { return flags[0] | flags[1]; }

private:
    Flags   flags[2];
    Vec2i   value;

    template <Flags mask>
    HYP_FORCE_INLINE
    void ApplyDefaultFlagMask()
    { 
        for (int i = 0; i < 2; i++) {
            if (!(flags[i] & mask)) {
                flags[i] |= (DEFAULT & mask);
            }
        }
    }

    HYP_FORCE_INLINE
    void ApplyDefaultFlags()
    {
        ApplyDefaultFlagMask<PIXEL | PERCENT>();
    }
};

class HYP_API UIObject : public EnableRefCountedPtrFromThis<UIObject>
{
public:
    UIObject(ID<Entity> entity, UIStage *parent, NodeProxy node_proxy);
    UIObject(const UIObject &other)                 = delete;
    UIObject &operator=(const UIObject &other)      = delete;
    UIObject(UIObject &&other) noexcept             = delete;
    UIObject &operator=(UIObject &&other) noexcept  = delete;
    virtual ~UIObject();

    virtual void Init();

    [[nodiscard]]
    HYP_FORCE_INLINE
    ID<Entity> GetEntity() const
        { return m_entity; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    UIStage *GetStage() const
        { return m_parent; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsInit() const
        { return m_is_init; }

    Name GetName() const;
    void SetName(Name name);

    Vec2i GetPosition() const;
    void SetPosition(Vec2i position);

    UIObjectSize GetSize() const;
    void SetSize(UIObjectSize size);

    int GetMaxWidth() const;
    void SetMaxWidth(int max_width, UIObjectSize::Flags flags = UIObjectSize::DEFAULT);

    int GetMaxHeight() const;
    void SetMaxHeight(int max_height, UIObjectSize::Flags flags = UIObjectSize::DEFAULT);
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    Vec2i GetActualSize() const
        { return m_actual_size; }

    /*! \brief Get the depth of the UI object
     *
     * The depth of the UI object is used to determine the rendering order of the object in the scene, with higher depth values being rendered on top of lower depth values.
     * If the depth value is set to 0, the depth will be determined by the node's depth in the scene.
     *
     * \return The depth of the UI object
     */
    int GetDepth() const;

    /*! \brief Set the depth of the UI object
     *
     * The depth of the UI object is used to determine the rendering order of the object in the scene, with higher depth values being rendered on top of lower depth values.
     * Set the depth to a value between UIStage::min_depth and UIStage::max_depth. If the depth value is set to 0, the depth will be determined by the node's depth in the scene.
     *
     * \param depth The depth of the UI object
     */
    void SetDepth(int depth);

    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 GetBorderRadius() const
        { return m_border_radius; }

    void SetBorderRadius(uint32 border_radius);

    [[nodiscard]]
    HYP_FORCE_INLINE
    UIObjectBorderFlags GetBorderFlags() const
        { return m_border_flags; }

    void SetBorderFlags(UIObjectBorderFlags border_flags);

    UIObjectAlignment GetOriginAlignment() const;
    void SetOriginAlignment(UIObjectAlignment alignment);

    UIObjectAlignment GetParentAlignment() const;
    void SetParentAlignment(UIObjectAlignment alignment);

    /*! \brief Get the padding of the UI object
     *
     * The padding of the UI object is used to add space around the object's content.
     *
     * \return The padding of the UI object
     */
    [[nodiscard]]
    HYP_FORCE_INLINE
    Vec2i GetPadding() const
        { return m_padding; }

    /*! \brief Set the padding of the UI object
     *
     * The padding of the UI object is used to add space around the object's content.
     *
     * \param padding The padding of the UI object
     */
    void SetPadding(Vec2i padding);

    virtual void AddChildUIObject(UIObject *ui_object);
    virtual bool RemoveChildUIObject(UIObject *ui_object);

    bool HasChildUIObjects() const;

    const NodeProxy &GetNode() const;

    BoundingBox GetLocalAABB() const;
    BoundingBox GetWorldAABB() const;

    virtual void UpdatePosition();
    virtual void UpdateSize();

    [[nodiscard]]
    HYP_FORCE_INLINE
    UIObjectFocusState GetFocusState() const
        { return m_focus_state; }

    void SetFocusState(UIObjectFocusState focus_state);

    Delegate<bool, const UIMouseEventData &>    OnMouseDown;
    Delegate<bool, const UIMouseEventData &>    OnMouseUp;
    Delegate<bool, const UIMouseEventData &>    OnMouseDrag;
    Delegate<bool, const UIMouseEventData &>    OnMouseHover;
    Delegate<bool, const UIMouseEventData &>    OnMouseLeave;
    Delegate<bool, const UIMouseEventData &>    OnClick;

protected:
    /*! \brief Get the shared quad mesh used for rendering UI objects. Vertices are in range: 0..1, with the origin at the top-left corner.
     *
     * \return The shared quad mesh
     */
    static Handle<Mesh> GetQuadMesh();

    virtual Handle<Material> GetMaterial() const;

    const Handle<Mesh> &GetMesh() const;

    const UIObject *GetParentUIObject() const;

    Scene *GetScene() const;

    void SetLocalAABB(const BoundingBox &aabb);

    void UpdateMeshData();
    void UpdateMaterial();

    ID<Entity>          m_entity;
    UIStage             *m_parent;

    Name                m_name;

    Vec2i               m_position;

    UIObjectSize        m_size;
    Vec2i               m_actual_size;

    UIObjectSize        m_max_size;
    Vec2i               m_actual_max_size;

    Vec2i               m_padding;

    int                 m_depth; // manually set depth; otherwise defaults to the node's depth in the scene

    uint32              m_border_radius;
    UIObjectBorderFlags m_border_flags;

    UIObjectAlignment   m_origin_alignment;
    UIObjectAlignment   m_parent_alignment;

private:
    void UpdateActualSizes();
    void ComputeActualSize(const UIObjectSize &size, Vec2i &out_actual_size, bool clamp = false);

    template <class Lambda>
    void ForEachChildUIObject(Lambda &&lambda);

    bool                m_is_init;

    UIObjectFocusState  m_focus_state;

    NodeProxy           m_node_proxy;
};

} // namespace hyperion::v2

#endif