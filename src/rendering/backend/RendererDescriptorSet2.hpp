#ifndef HYPERION_V2_BACKEND_RENDERER_DESCRIPTOR_SET2_HPP
#define HYPERION_V2_BACKEND_RENDERER_DESCRIPTOR_SET2_HPP

#include <core/Name.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/ArrayMap.hpp>
#include <core/lib/FixedArray.hpp>
#include <core/lib/Range.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <util/Defines.hpp>
#include <Types.hpp>
#include <HashCode.hpp>


namespace hyperion {
namespace renderer {

struct DescriptorSetDeclaration;

enum class DescriptorSetElementType : uint32
{
    UNSET,
    UNIFORM_BUFFER,
    UNIFORM_BUFFER_DYNAMIC,
    STORAGE_BUFFER,
    STORAGE_BUFFER_DYNAMIC,
    IMAGE,
    IMAGE_STORAGE,
    SAMPLER,
    TLAS,
    MAX
};

constexpr uint descriptor_set_element_type_to_buffer_type[uint(DescriptorSetElementType::MAX)] = {
    0,                                                          // UNSET
    (1u << uint(GPUBufferType::CONSTANT_BUFFER)),               // UNIFORM_BUFFER
    (1u << uint(GPUBufferType::CONSTANT_BUFFER)),               // UNIFORM_BUFFER_DYNAMIC
    (1u << uint(GPUBufferType::STORAGE_BUFFER))
        | (1u << uint(GPUBufferType::ATOMIC_COUNTER))
        | (1u << uint(GPUBufferType::STAGING_BUFFER))
        | (1u << uint(GPUBufferType::INDIRECT_ARGS_BUFFER)),    // STORAGE_BUFFER
    
    (1u << uint(GPUBufferType::STORAGE_BUFFER))
        | (1u << uint(GPUBufferType::ATOMIC_COUNTER))
        | (1u << uint(GPUBufferType::STAGING_BUFFER))
        | (1u << uint(GPUBufferType::INDIRECT_ARGS_BUFFER)),    // STORAGE_BUFFER_DYNAMIC
    0,                                                          // IMAGE
    0,                                                          // IMAGE_STORAGE
    0,                                                          // SAMPLER
    (1u << uint(GPUBufferType::ACCELERATION_STRUCTURE_BUFFER))  // ACCELERATION_STRUCTURE
};

struct DescriptorSetLayoutElement
{
    DescriptorSetElementType    type = DescriptorSetElementType::UNSET;
    uint                        binding = -1; // has to be set
    uint                        count = 1; // Set to -1 for bindless
    uint                        size = -1;

    bool IsBindless() const
        { return count == ~0u; }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(type);
        hc.Add(binding);
        hc.Add(count);
        hc.Add(size);

        return hc;
    }
};

template <SizeType Sz>
static inline uint32 GetDescriptorSetElementTypeMask(FixedArray<DescriptorSetElementType, Sz> types)
{
    uint32 mask = 0;

    for (const auto &type : types) {
        mask |= 1 << uint32(type);
    }

    return mask;
}

enum DescriptorSlot : uint32
{
    DESCRIPTOR_SLOT_NONE,
    DESCRIPTOR_SLOT_SRV,
    DESCRIPTOR_SLOT_UAV,
    DESCRIPTOR_SLOT_CBUFF,
    DESCRIPTOR_SLOT_SSBO,
    DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE,
    DESCRIPTOR_SLOT_SAMPLER,
    DESCRIPTOR_SLOT_MAX
};

struct DescriptorDeclaration
{
    DescriptorSlot  slot = DESCRIPTOR_SLOT_NONE;
    String          name;
    uint            count = 1;
    uint            size = -1;
    bool            is_dynamic = false;
    uint            index = ~0u;
};

struct DescriptorSetDeclaration
{
    uint                                                            set_index = ~0u;
    Name                                                            name = Name::invalid;
    FixedArray<Array<DescriptorDeclaration>, DESCRIPTOR_SLOT_MAX>   slots = { };

    // is this a reference to a global descriptor set declaration?
    bool                                                            is_reference = false;

    DescriptorSetDeclaration()                                                      = default;

    DescriptorSetDeclaration(uint set_index, Name name, bool is_reference = false)
        : set_index(set_index),
          name(name),
          is_reference(is_reference)
    {
    }

    DescriptorSetDeclaration(const DescriptorSetDeclaration &other)                 = default;
    DescriptorSetDeclaration &operator=(const DescriptorSetDeclaration &other)      = default;
    DescriptorSetDeclaration(DescriptorSetDeclaration &&other) noexcept             = default;
    DescriptorSetDeclaration &operator=(DescriptorSetDeclaration &&other) noexcept  = default;
    ~DescriptorSetDeclaration()                                                     = default;

    Array<DescriptorDeclaration> &GetSlot(DescriptorSlot slot)
    {
        AssertThrow(slot < DESCRIPTOR_SLOT_MAX && slot > DESCRIPTOR_SLOT_NONE);

        return slots[uint(slot) - 1];
    }

    const Array<DescriptorDeclaration> &GetSlot(DescriptorSlot slot) const
    {
        AssertThrow(slot < DESCRIPTOR_SLOT_MAX && slot > DESCRIPTOR_SLOT_NONE);

        return slots[uint(slot) - 1];
    }

    void AddDescriptorDeclaration(renderer::DescriptorDeclaration decl)
    {
        AssertThrow(decl.slot != DESCRIPTOR_SLOT_NONE && decl.slot < DESCRIPTOR_SLOT_MAX);

        decl.index = uint(slots[uint(decl.slot) - 1].Size());
        slots[uint(decl.slot) - 1].PushBack(std::move(decl));
    }

    /*! \brief Calculate a flat index for a Descriptor that is part of this set.
        Returns -1 if not found */
    uint CalculateFlatIndex(DescriptorSlot slot, const String &name) const;

    DescriptorDeclaration *FindDescriptorDeclaration(const String &name) const;
};

class DescriptorTableDeclaration
{
public:
    DescriptorTableDeclaration()                                                    = default;
    DescriptorTableDeclaration(const DescriptorTableDeclaration &)                  = default;
    DescriptorTableDeclaration &operator=(const DescriptorTableDeclaration &)       = default;
    DescriptorTableDeclaration(DescriptorTableDeclaration &&) noexcept              = default;
    DescriptorTableDeclaration &operator=(DescriptorTableDeclaration &&) noexcept   = default;
    ~DescriptorTableDeclaration()                                                   = default;

    DescriptorSetDeclaration *FindDescriptorSetDeclaration(Name name) const;
    DescriptorSetDeclaration *AddDescriptorSetDeclaration(DescriptorSetDeclaration descriptor_set);

    Array<DescriptorSetDeclaration> &GetElements()
        { return m_elements; }

    const Array<DescriptorSetDeclaration> &GetElements() const
        { return m_elements; }

private:
    Array<DescriptorSetDeclaration> m_elements;

public:
    struct DeclareSet
    {
        DeclareSet(DescriptorTableDeclaration *table, uint set_index, Name name)
        {
            AssertThrow(table != nullptr);

            if (table->m_elements.Size() <= set_index) {
                table->m_elements.Resize(set_index + 1);
            }

            DescriptorSetDeclaration decl;
            decl.set_index = set_index;
            decl.name = name;
            table->m_elements[set_index] = std::move(decl);
        }
    };

    struct DeclareDescriptor
    {
        DeclareDescriptor(DescriptorTableDeclaration *table, Name set_name, DescriptorSlot slot_type, const String &descriptor_name, uint count = 1)
        {
            AssertThrow(table != nullptr);

            uint set_index = ~0u;

            for (uint i = 0; i < table->m_elements.Size(); ++i) {
                if (table->m_elements[i].name == set_name) {
                    set_index = i;
                    break;
                }
            }

            AssertThrowMsg(set_index != ~0u, "Descriptor set %s not found", set_name.LookupString());

            DescriptorSetDeclaration &decl = table->m_elements[set_index];
            AssertThrow(decl.set_index == set_index);
            AssertThrow(slot_type > 0 && slot_type < decl.slots.Size());

            const uint slot_index = decl.slots[uint(slot_type) - 1].Size();

            DescriptorDeclaration descriptor_decl;
            descriptor_decl.index = slot_index;
            descriptor_decl.slot = slot_type;
            descriptor_decl.name = descriptor_name;
            descriptor_decl.count = count;
            decl.slots[uint(slot_type) - 1].PushBack(descriptor_decl);
        }
    };
};

extern DescriptorTableDeclaration *g_static_descriptor_table_decl;

namespace platform {

template <PlatformType PLATFORM>
class Device;
    
template <PlatformType PLATFORM>
class Instance;

template <PlatformType PLATFORM>
class DescriptorSet2;

template <PlatformType PLATFORM>
class ImageView;

template <PlatformType PLATFORM>
class Sampler;

template <PlatformType PLATFORM>
class TopLevelAccelerationStructure;

template <PlatformType PLATFORM>
class Frame;

template <PlatformType PLATFORM>
class CommandBuffer;

template <PlatformType PLATFORM>
class GraphicsPipeline;

template <PlatformType PLATFORM>
class ComputePipeline;

template <PlatformType PLATFORM>
class RaytracingPipeline;

template <class T>
struct DescriptorSetElementTypeInfo
{
    static_assert(resolution_failure<T>, "Implementation missing for this type");
};

template <>
struct DescriptorSetElementTypeInfo<GPUBuffer<Platform::VULKAN>>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::UNIFORM_BUFFER))
        | (1u << uint32(DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC))
        | (1u << uint32(DescriptorSetElementType::STORAGE_BUFFER))
        | (1u << uint32(DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC));
};

template <>
struct DescriptorSetElementTypeInfo<ImageView<Platform::VULKAN>>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::IMAGE))
        | (1u << uint32(DescriptorSetElementType::IMAGE_STORAGE));
};

template <>
struct DescriptorSetElementTypeInfo<Sampler<Platform::VULKAN>>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::SAMPLER));
};

template <>
struct DescriptorSetElementTypeInfo<TopLevelAccelerationStructure<Platform::VULKAN>>
{
    static constexpr uint32 mask = (1u << uint32(DescriptorSetElementType::TLAS));
};

template <PlatformType PLATFORM>
class DescriptorSetLayout
{
public:
    DescriptorSetLayout(const DescriptorSetDeclaration &decl);
    DescriptorSetLayout(const DescriptorSetLayout &other)                   = default;
    DescriptorSetLayout &operator=(const DescriptorSetLayout &other)        = default;
    DescriptorSetLayout(DescriptorSetLayout &&other) noexcept               = default;
    DescriptorSetLayout &operator=(DescriptorSetLayout &&other) noexcept    = default;
    ~DescriptorSetLayout()                                                  = default;

    DescriptorSet2Ref<PLATFORM> CreateDescriptorSet() const;

    Name GetName() const
        { return m_name; }

    const ArrayMap<String, DescriptorSetLayoutElement> &GetElements() const
        { return m_elements; }

    void AddElement(const String &name, DescriptorSetElementType type, uint binding, uint count, uint size = -1)
        { m_elements.Insert(name, DescriptorSetLayoutElement { type, binding, count, size }); }

    const DescriptorSetLayoutElement *GetElement(const String &name) const
    {
        const auto it = m_elements.Find(name);

        if (it == m_elements.End()) {
            return nullptr;
        }

        return &it->second;
    }

    const Array<String> &GetDynamicElements() const
        { return m_dynamic_elements; }

    HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &it : m_elements) {
            hc.Add(it.first.GetHashCode());
            hc.Add(it.second.GetHashCode());
        }

        return hc;
    }

private:
    Name                                            m_name;
    ArrayMap<String, DescriptorSetLayoutElement>    m_elements;
    Array<String>                                   m_dynamic_elements;
};

template <PlatformType PLATFORM>
struct DescriptorSetElement
{
    using ValueType = Variant<GPUBufferRef<PLATFORM>, ImageViewRef<PLATFORM>, SamplerRef<PLATFORM>, TLASRef<PLATFORM>>;

    FlatMap<uint, ValueType>    values;
    uint                        buffer_size = 0; // Used for dynamic uniform buffers
    Range<uint>                 dirty_range { };     

    HYP_FORCE_INLINE
    bool IsDirty() const
        { return bool(dirty_range); }
};

template <PlatformType PLATFORM>
class DescriptorSet2
{
public:
    DescriptorSet2(const DescriptorSetLayout<PLATFORM> &layout);
    DescriptorSet2(const DescriptorSet2 &other)                 = delete;
    DescriptorSet2 &operator=(const DescriptorSet2 &other)      = delete;
    DescriptorSet2(DescriptorSet2 &&other) noexcept             = delete;
    DescriptorSet2 &operator=(DescriptorSet2 &&other) noexcept  = delete;
    ~DescriptorSet2();

    const DescriptorSetLayout<PLATFORM> &GetLayout() const
        { return m_layout; }
    
    void SetElement(const String &name, const GPUBufferRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, const GPUBufferRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, uint buffer_size, const GPUBufferRef<PLATFORM> &ref);
    
    void SetElement(const String &name, const ImageViewRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, const ImageViewRef<PLATFORM> &ref);
    
    void SetElement(const String &name, const SamplerRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, const SamplerRef<PLATFORM> &ref);
    
    void SetElement(const String &name, const TLASRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, const TLASRef<PLATFORM> &ref);

    void Bind(const CommandBufferRef<PLATFORM> &command_buffer, const GraphicsPipeline<PLATFORM> *pipeline, uint bind_index) const;
    void Bind(const CommandBufferRef<PLATFORM> &command_buffer, const GraphicsPipeline<PLATFORM> *pipeline, const ArrayMap<String, uint> &offsets, uint bind_index) const;
    void Bind(const CommandBufferRef<PLATFORM> &command_buffer, const ComputePipeline<PLATFORM> *pipeline, uint bind_index) const;
    void Bind(const CommandBufferRef<PLATFORM> &command_buffer, const ComputePipeline<PLATFORM> *pipeline, const ArrayMap<String, uint> &offsets, uint bind_index) const;
    void Bind(const CommandBufferRef<PLATFORM> &command_buffer, const RaytracingPipeline<PLATFORM> *pipeline, uint bind_index) const;
    void Bind(const CommandBufferRef<PLATFORM> &command_buffer, const RaytracingPipeline<PLATFORM> *pipeline, const ArrayMap<String, uint> &offsets, uint bind_index) const;

    DescriptorSet2Ref<PLATFORM> Clone() const;

private:
    DescriptorSetLayout<PLATFORM>                       m_layout;
    ArrayMap<String, DescriptorSetElement<PLATFORM>>    m_elements;
};

template <PlatformType PLATFORM>
class DescriptorSetManager
{
public:
};

} // namespace platform

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererDescriptorSet2.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class DescriptorTable
{
public:
    DescriptorTable(const DescriptorTableDeclaration &decl);
    DescriptorTable(const DescriptorTable &other)                 = default;
    DescriptorTable &operator=(const DescriptorTable &other)      = default;
    DescriptorTable(DescriptorTable &&other) noexcept             = default;
    DescriptorTable &operator=(DescriptorTable &&other) noexcept  = default;
    ~DescriptorTable()                                            = default;

    const DescriptorTableDeclaration &GetDeclaration() const
        { return m_decl; }

    const FixedArray<Array<DescriptorSet2Ref<PLATFORM>>, max_frames_in_flight> &GetSets() const
        { return m_sets; }

    DescriptorSet2Ref<PLATFORM> GetDescriptorSet(Name name, uint frame_index) const
    {
        for (const auto &set : m_sets[frame_index]) {
            if (set->GetLayout().GetName() == name) {
                return set;
            }
        }

        return DescriptorSet2Ref<PLATFORM>::unset;
    }

    Result Create(Device<PLATFORM> *device)
    {
        Result result = Result::OK;

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            for (auto &set : m_sets[frame_index]) {
                result = set->Create(device);

                if (!result) {
                    return result;
                }
            }
        }

        return result;
    }

    Result Destroy(Device<PLATFORM> *device)
    {
        for (auto &it : m_sets) {
            SafeRelease(std::move(it));
        }
        
        m_sets = { };

        return Result::OK;
    }

    void Bind(Frame<PLATFORM> *frame, const GraphicsPipelineRef<PLATFORM> &pipeline, const ArrayMap<Name, ArrayMap<String, uint>> &offsets)
    {
        for (const auto &set : m_sets[frame->GetFrameIndex()]) {
            const Name descriptor_set_name = set->GetLayout().GetName();

            DescriptorSetDeclaration *decl = m_decl.FindDescriptorSetDeclaration(descriptor_set_name);
            AssertThrow(decl != nullptr);

            if (set->GetLayout().GetDynamicElements().Empty()) {
                set->Bind(frame->GetCommandBuffer(), pipeline, decl->set_index);

                continue;
            }

            const auto offsets_it = offsets.Find(descriptor_set_name);
            AssertThrow(offsets_it != offsets.End());

            set->Bind(frame->GetCommandBuffer(), pipeline, offsets_it->second, decl->set_index);
        }
    }

    void Bind(Frame<PLATFORM> *frame, const ComputePipelineRef<PLATFORM> &pipeline, const ArrayMap<Name, ArrayMap<String, uint>> &offsets) const
    {
        for (const auto &set : m_sets[frame->GetFrameIndex()]) {
            const Name descriptor_set_name = set->GetLayout().GetName();

            DescriptorSetDeclaration *decl = m_decl.FindDescriptorSetDeclaration(descriptor_set_name);
            AssertThrow(decl != nullptr);

            if (set->GetLayout().GetDynamicElements().Empty()) {
                set->Bind(frame->GetCommandBuffer(), pipeline, decl->set_index);

                continue;
            }

            const auto offsets_it = offsets.Find(descriptor_set_name);
            AssertThrow(offsets_it != offsets.End());

            set->Bind(frame->GetCommandBuffer(), pipeline, offsets_it->second, decl->set_index);
        }
    }

    void Bind(Frame<PLATFORM> *frame, const RaytracingPipelineRef<PLATFORM> &pipeline, const ArrayMap<Name, ArrayMap<String, uint>> &offsets) const
    {
        for (const auto &set : m_sets[frame->GetFrameIndex()]) {
            const Name descriptor_set_name = set->GetLayout().GetName();

            DescriptorSetDeclaration *decl = m_decl.FindDescriptorSetDeclaration(descriptor_set_name);
            AssertThrow(decl != nullptr);

            if (set->GetLayout().GetDynamicElements().Empty()) {
                set->Bind(frame->GetCommandBuffer(), pipeline, decl->set_index);

                continue;
            }

            const auto offsets_it = offsets.Find(descriptor_set_name);
            AssertThrow(offsets_it != offsets.End());

            set->Bind(frame->GetCommandBuffer(), pipeline, offsets_it->second, decl->set_index);
        }
    }

private:
    DescriptorTableDeclaration                                              m_decl;
    FixedArray<Array<DescriptorSet2Ref<PLATFORM>>, max_frames_in_flight>    m_sets;
};

} // namespace platform

using DescriptorSet2 = platform::DescriptorSet2<Platform::CURRENT>;
using DescriptorSetLayout = platform::DescriptorSetLayout<Platform::CURRENT>;
using DescriptorSetManager = platform::DescriptorSetManager<Platform::CURRENT>;
using DescriptorTable = platform::DescriptorTable<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif