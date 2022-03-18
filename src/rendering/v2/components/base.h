#ifndef HYPERION_V2_COMPONENTS_BASE_H
#define HYPERION_V2_COMPONENTS_BASE_H

#include <rendering/backend/renderer_instance.h>

#include <memory>

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;

class Engine;

Device *GetEngineDevice(Engine *engine);

template <class WrappedType>
class EngineComponent {
    template <class ObjectType, class InnerType>
    struct IdWrapper {
        using InnerType_t = InnerType;

        explicit constexpr operator InnerType() const { return value; }
        inline constexpr InnerType GetValue() const { return value; }

        InnerType value;
    };

public:
    using ID = IdWrapper<WrappedType, uint32_t>;

    template <class ...Args>
    EngineComponent(Args &&... args)
        : m_wrapped(std::move(args)...),
          m_is_created(false)
    {}
    
    EngineComponent(EngineComponent &&other) noexcept
        : m_wrapped(std::move(other.m_wrapped)),
          m_is_created(other.m_is_created)
    {
        other.m_is_created = false;
    }

    EngineComponent &operator=(EngineComponent &&other) noexcept
    {
        this->m_wrapped = std::move(other.m_wrapped);
        this->m_is_created = other.m_is_created;
        other.m_is_created = false;

        return *this;
    }

    EngineComponent(const EngineComponent &other) = delete;
    EngineComponent &operator=(const EngineComponent &other) = delete;

    ~EngineComponent()
    {
        if (m_is_created) {
            AssertThrowMsg(
                !m_is_created,
                "Expected wrapped object of type %s to be destroyed before destructor, but it was not nullptr.",
                typeid(WrappedType).name()
            );
        }
    }

    inline WrappedType &Get() { return m_wrapped; }
    inline const WrappedType &Get() const { return m_wrapped; }

    /* Standard non-specialized initialization function */
    template <class ...Args>
    void Create(Engine *engine, Args &&... args)
    {
        const char *wrapped_type_name = typeid(WrappedType).name();

        AssertThrowMsg(
            !m_is_created,
            "Expected wrapped object of type %s to have not already been created, but it was already created.",
            wrapped_type_name
        );

        auto result = m_wrapped.Create(GetEngineDevice(engine), std::move(args)...);
        AssertThrowMsg(result, "Creation of object of type %s failed: %s", wrapped_type_name, result.message);

        m_is_created = true;
    }

    /* Standard non-specialized destruction function */
    template <class ...Args>
    void Destroy(Engine *engine, Args &&... args)
    {
        const char *wrapped_type_name = typeid(WrappedType).name();

        AssertThrowMsg(
            m_is_created,
            "Expected wrapped object of type %s to have been created, but it was not yet created.",
            wrapped_type_name
        );

        auto result = m_wrapped.Destroy(GetEngineDevice(engine), std::move(args)...);
        AssertThrowMsg(result, "Destruction of object of type %s failed: %s", wrapped_type_name, result.message);

        m_is_created = false;
    }

protected:
    WrappedType m_wrapped;
    bool m_is_created;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_COMPONENTS_BASE_H

