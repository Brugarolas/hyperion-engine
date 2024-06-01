/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_STRUCTS_HPP
#define HYPERION_BACKEND_RENDERER_STRUCTS_HPP

#include <core/containers/String.hpp>
#include <core/Defines.hpp>
#include <util/EnumOptions.hpp>
#include <math/Extent.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion::renderer {

struct alignas(16) PackedVertex
{
    float32 position_x,
            position_y,
            position_z,
            normal_x,
            normal_y,
            normal_z,
            texcoord0_x,
            texcoord0_y;
};

static_assert(sizeof(PackedVertex) == sizeof(float32) * 8);

enum class DatumType : uint32
{
    UNSIGNED_BYTE,
    SIGNED_BYTE,
    UNSIGNED_SHORT,
    SIGNED_SHORT,
    UNSIGNED_INT,
    SIGNED_INT,
    FLOAT
};

enum class FaceCullMode : uint32
{
    NONE,
    BACK,
    FRONT
};

enum class FillMode : uint32
{
    FILL,
    LINE
};

enum class Topology : uint32
{
    TRIANGLES,
    TRIANGLE_FAN,
    TRIANGLE_STRIP,

    LINES,

    POINTS
};

enum class StencilMode : uint8
{
    NONE,
    FILL,
    OUTLINE
};

enum class BlendModeFactor : uint32
{
    NONE,

    ONE,
    ZERO,
    SRC_COLOR,
    SRC_ALPHA,
    DST_COLOR,
    DST_ALPHA,
    ONE_MINUS_SRC_COLOR,
    ONE_MINUS_SRC_ALPHA,
    ONE_MINUS_DST_COLOR,
    ONE_MINUS_DST_ALPHA,

    MAX
};

static_assert(uint32(BlendModeFactor::MAX) <= 15, "BlendModeFactor enum too large to fit in 4 bits");

struct BlendFunction
{
    uint32  value;

    BlendFunction()
        : BlendFunction(BlendModeFactor::ONE, BlendModeFactor::ZERO)
    {
    }

    BlendFunction(BlendModeFactor src, BlendModeFactor dst)
        : value((uint32(src) << 0) | (uint32(dst) << 4) | (uint32(src) << 8) | (uint32(dst) << 12))
    {
    }

    BlendFunction(BlendModeFactor src_color, BlendModeFactor dst_color, BlendModeFactor src_alpha, BlendModeFactor dst_alpha)
        : value((uint32(src_color) << 0) | (uint32(dst_color) << 4) | (uint32(src_alpha) << 8) | (uint32(dst_alpha) << 12))
    {
    }

    BlendFunction(const BlendFunction &other)                   = default;
    BlendFunction &operator=(const BlendFunction &other)        = default;
    BlendFunction(BlendFunction &&other) noexcept               = default;
    BlendFunction &operator=(BlendFunction &&other) noexcept    = default;
    ~BlendFunction()                                            = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    BlendModeFactor GetSrcColor() const
        { return BlendModeFactor(value & 0xF); }

    HYP_FORCE_INLINE
    void SetSrcColor(BlendModeFactor src)
        { value |= uint32(src); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    BlendModeFactor GetDstColor() const
        { return BlendModeFactor((value >> 4) & 0xF); }

    HYP_FORCE_INLINE
    void SetDstColor(BlendModeFactor dst)
        { value |= uint32(dst) << 4; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    BlendModeFactor GetSrcAlpha() const
        { return BlendModeFactor((value >> 8) & 0xF); }

    HYP_FORCE_INLINE
    void SetSrcAlpha(BlendModeFactor src)
        { value |= uint32(src) << 8; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    BlendModeFactor GetDstAlpha() const
        { return BlendModeFactor((value >> 12) & 0xF); }

    HYP_FORCE_INLINE
    void SetDstAlpha(BlendModeFactor dst)
        { value |= uint32(dst) << 12; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const BlendFunction &other) const
        { return value == other.value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const BlendFunction &other) const
        { return value != other.value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const BlendFunction &other) const
        { return value < other.value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
        { return HashCode::GetHashCode(value); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    static BlendFunction None()
        { return BlendFunction(BlendModeFactor::NONE, BlendModeFactor::NONE); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    static BlendFunction Default()
        { return BlendFunction(BlendModeFactor::ONE, BlendModeFactor::ZERO); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    static BlendFunction AlphaBlending()
        { return BlendFunction(BlendModeFactor::SRC_ALPHA, BlendModeFactor::ONE_MINUS_SRC_ALPHA, BlendModeFactor::ONE, BlendModeFactor::ZERO); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    static BlendFunction Additive()
        { return BlendFunction(BlendModeFactor::ONE, BlendModeFactor::ONE); }
};

enum class StencilCompareOp : uint8
{
    ALWAYS,
    NEVER,
    EQUAL,
    NOT_EQUAL
};

enum class StencilOp : uint8
{
    KEEP,
    ZERO,
    REPLACE,
    INCREMENT,
    DECREMENT
};

struct StencilFunction
{
    StencilOp           pass_op;
    StencilOp           fail_op;
    StencilOp           depth_fail_op;
    StencilCompareOp    compare_op;
    uint8               mask;
    uint8               value;

    StencilFunction()
        : StencilFunction(StencilOp::KEEP, StencilOp::REPLACE, StencilOp::REPLACE, StencilCompareOp::ALWAYS, 0x0, 0x1)
    {
    }

    StencilFunction(StencilOp pass_op, StencilOp fail_op, StencilOp depth_fail_op, StencilCompareOp compare_op, uint8 mask, uint8 value)
        : pass_op(pass_op),
          fail_op(fail_op),
          depth_fail_op(depth_fail_op),
          compare_op(compare_op),
          mask(mask),
          value(value)
    {
    }

    StencilFunction(const StencilFunction &other)                   = default;
    StencilFunction &operator=(const StencilFunction &other)        = default;
    StencilFunction(StencilFunction &&other) noexcept               = default;
    StencilFunction &operator=(StencilFunction &&other) noexcept    = default;
    ~StencilFunction()                                              = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const StencilFunction &other) const = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const StencilFunction &other) const = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const StencilFunction &other) const
        { return Memory::MemCmp(this, &other, sizeof(StencilFunction)) < 0; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsSet() const
        { return mask != 0x0; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
        { return HashCode::GetHashCode(value); }
};

struct PushConstantData
{
    ubyte   data[128];
    uint    size;

    PushConstantData()
        : data { 0 },
          size(0)
    {
    }

    PushConstantData(const void *ptr, SizeType size)
        : size(size)
    {
        AssertThrowMsg(size <= 128, "Push constant data size exceeds 128 bytes");

        Memory::MemCpy(&data[0], ptr, size);
    }

    template <class T>
    PushConstantData(const T *value)
        : size(sizeof(T))
    {
        static_assert(sizeof(T) <= 128, "Push constant data size exceeds 128 bytes");
        static_assert(std::is_trivial_v<T>, "T must be a trivial type");
        static_assert(std::is_standard_layout_v<T>, "T must be a standard layout type");

        Memory::MemCpy(&data[0], value, sizeof(T));
    }

    PushConstantData(const PushConstantData &other)                 = default;
    PushConstantData &operator=(const PushConstantData &other)      = default;
    PushConstantData(PushConstantData &&other) noexcept             = default;
    PushConstantData &operator=(PushConstantData &&other) noexcept  = default;
    ~PushConstantData()                                             = default;

    const void *Data() const
        { return data; }

    uint Size() const
        { return size; }

    explicit operator bool() const
        { return size != 0; }
};

} // namespace hyperion::renderer

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererStructs.hpp>
#else
#error Unsupported rendering backend
#endif

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>
#include <math/Matrix4.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

struct alignas(16) MeshDescription
{
    uint64 vertex_buffer_address;
    uint64 index_buffer_address;

    uint32 entity_index;
    uint32 material_index;
    uint32 num_indices;
    uint32 num_vertices;
};

using ImageSubResourceFlagBits = uint;

enum ImageSubResourceFlags : ImageSubResourceFlagBits
{
    IMAGE_SUB_RESOURCE_FLAGS_NONE    = 0,
    IMAGE_SUB_RESOURCE_FLAGS_COLOR   = 1 << 0,
    IMAGE_SUB_RESOURCE_FLAGS_DEPTH   = 1 << 1,
    IMAGE_SUB_RESOURCE_FLAGS_STENCIL = 1 << 2
};

/* images */
struct ImageSubResource
{
    ImageSubResourceFlagBits flags = IMAGE_SUB_RESOURCE_FLAGS_COLOR;
    uint32 base_array_layer = 0;
    uint32 base_mip_level = 0;
    uint32 num_layers = 1;
    uint32 num_levels = 1;

    bool operator==(const ImageSubResource &other) const
    {
        return flags == other.flags
            && base_array_layer == other.base_array_layer
            && num_layers == other.num_layers
            && base_mip_level == other.base_mip_level
            && num_levels == other.num_levels;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(flags);
        hc.Add(base_array_layer);
        hc.Add(num_layers);
        hc.Add(base_mip_level);
        hc.Add(num_levels);

        return hc;
    }
};

template <class T>
struct alignas(8) ShaderVec2
{
    union {
        struct { T x, y; };
        T values[2];
    };

    ShaderVec2() = default;
    ShaderVec2(const ShaderVec2 &other) = default;
    ShaderVec2(T x, T y) : x(x), y(y) {}
    ShaderVec2(const Extent2D &extent)
        : x(extent.width),
          y(extent.height)
    {
    }

    ShaderVec2(const Vec2<T> &xy)
        : x(xy.x),
          y(xy.y)
    {
    }

    constexpr T &operator[](uint index) { return values[index]; }
    constexpr const T &operator[](uint index) const { return values[index]; }

    operator Vec2<T>() const { return Vec2<T>(x, y); }
};

static_assert(sizeof(ShaderVec2<float>) == 8);
static_assert(sizeof(ShaderVec2<uint32>) == 8);

// shader vec3 is same size as vec4
template <class T>
struct alignas(16) ShaderVec3
{
    union {
        struct { T x, y, z, _w; };
        T values[4];
    };

    ShaderVec3() = default;
    ShaderVec3(const ShaderVec3 &other) = default;
    ShaderVec3(T x, T y, T z) : x(x), y(y), z(z) {}

    ShaderVec3(const Extent3D &extent)
        : x(extent.width),
          y(extent.height),
          z(extent.depth)
    {
    }

    ShaderVec3(const Vec3<T> &xyz)
        : x(xyz.x),
          y(xyz.y),
          z(xyz.z)
    {
    }

    constexpr T &operator[](uint index) { return values[index]; }
    constexpr const T &operator[](uint index) const { return values[index]; }

    operator Vector3() const { return Vector3(x, y, z); }
};

static_assert(sizeof(ShaderVec3<float>)  == 16);
static_assert(sizeof(ShaderVec3<uint32>) == 16);

template <class T>
struct alignas(16) ShaderVec4
{
    union {
        struct { T x, y, z, w; };
        T values[4];
    };

    ShaderVec4() = default;
    ShaderVec4(const ShaderVec4 &other) = default;
    ShaderVec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
    ShaderVec4(const Vector4 &vec)
        : x(T(vec.x)),
          y(T(vec.y)),
          z(T(vec.z)),
          w(T(vec.w))
    {
    }

    ShaderVec4(const Vec3<T> &xyz, T w)
        : x(xyz.x),
          y(xyz.y),
          z(xyz.z),
          w(w)
    {
    }

    constexpr T &operator[](uint index) { return values[index]; }
    constexpr const T &operator[](uint index) const { return values[index]; }

    operator Vector4() const { return Vector4(float(x), float(y), float(z), float(w)); }
};

static_assert(sizeof(ShaderVec4<float>) == 16);
static_assert(sizeof(ShaderVec4<uint32>) == 16);

struct alignas(16) ShaderMat4
{
    union {
        struct {
            float m00, m01, m02, m03,
                m10, m11, m12, m13,
                m20, m21, m22, m23,
                m30, m31, m32, m33;
        };

        float values[16];
        ShaderVec4<float> rows[4];
    };

    ShaderMat4() = default;
    ShaderMat4(const ShaderMat4 &other) = default;
    ShaderMat4(const Matrix4 &mat)
        : m00(mat[0][0]), m01(mat[0][1]), m02(mat[0][2]), m03(mat[0][3]),
          m10(mat[1][0]), m11(mat[1][1]), m12(mat[1][2]), m13(mat[1][3]),
          m20(mat[2][0]), m21(mat[2][1]), m22(mat[2][2]), m23(mat[2][3]),
          m30(mat[3][0]), m31(mat[3][1]), m32(mat[3][2]), m33(mat[3][3])
    {
    }

    constexpr ShaderVec4<float> &operator[](uint index) { return rows[index]; }
    constexpr const ShaderVec4<float> &operator[](uint index) const { return rows[index]; }

    operator Matrix4() const { return Matrix4(&values[0]); }
};

static_assert(sizeof(ShaderMat4) == 64);

template <SizeType N, class T>
struct PaddedStructValue
{
    alignas(T) ubyte bytes[sizeof(T) + N];
};

template <class T, SizeType Size>
struct ShaderValue : public PaddedStructValue<Size - sizeof(T), T>
{
    static_assert(sizeof(T) <= Size, "T does not fit into required size!");

    ShaderValue()
    {
        new (this->bytes) T();
    }

    ShaderValue(const ShaderValue &other)
    {
        new (this->bytes) T(other.Get());
    }

    ShaderValue &operator=(const ShaderValue &other)
    {
        Get().~T();
        new (this->bytes) T(other.Get());

        return *this;
    }

    ShaderValue(const T &value)
    {
        new (this->bytes) T(value);
    }

    ShaderValue &operator=(const T &value)
    {
        Get().~T();
        new (this->bytes) T(value);

        return *this;
    }

    ShaderValue(ShaderValue &&other) noexcept
    {
        new (this->bytes) T(std::move(other.Get()));
    }

    ShaderValue &operator=(ShaderValue &&other) noexcept
    {
        Get().~T();
        new (this->bytes) T(std::move(other.Get()));

        return *this;
    }

    ShaderValue(T &&value) noexcept
    {
        new (this->bytes) T(std::move(value));
    }

    ShaderValue &operator=(T &&value) noexcept
    {
        Get().~T();
        new (this->bytes) T(std::move(value));

        return *this;
    }

    ~ShaderValue()
    {
        Get().~T();
    }

    T &Get()
    {
        return *reinterpret_cast<T *>(this->bytes);
    }

    const T &Get() const
    {
        return *reinterpret_cast<const T *>(this->bytes);
    }
};

} // namespace renderer
} // namespace hyperion

#endif
