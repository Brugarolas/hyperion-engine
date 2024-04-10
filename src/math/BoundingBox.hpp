#ifndef BOUNDING_BOX_H
#define BOUNDING_BOX_H

#include <math/Vector3.hpp>
#include <math/Transform.hpp>
#include <core/lib/FixedArray.hpp>
#include <core/lib/Memory.hpp>
#include <HashCode.hpp>
#include <Types.hpp>

namespace hyperion {

class HYP_API BoundingBox
{
    friend std::ostream &operator<<(std::ostream &out, const BoundingBox &bb);
public:
    static const BoundingBox empty;
    static const BoundingBox infinity;

    BoundingBox();
    BoundingBox(const Vec3f &min, const Vec3f &max);

    BoundingBox(const BoundingBox &other)                   = default;
    BoundingBox &operator=(const BoundingBox &other)        = default;

    BoundingBox(BoundingBox &&other) noexcept               = default;
    BoundingBox &operator=(BoundingBox &&other) noexcept    = default;

    ~BoundingBox()                                          = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Vec3f &GetMin() const
        { return min; }

    HYP_FORCE_INLINE
    void SetMin(const Vec3f &min)
        { this->min = min; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    const Vec3f &GetMax() const
        { return max; }

    HYP_FORCE_INLINE
    void SetMax(const Vec3f &max)
        { this->max = max; }
    
    [[nodiscard]]
    FixedArray<Vec3f, 8> GetCorners() const;

    [[nodiscard]]
    Vec3f GetCorner(uint index) const;
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    Vec3f GetCenter() const
        { return (max + min) * 0.5f; }

    void SetCorners(const FixedArray<Vec3f, 8> &corners);

    void SetCenter(const Vec3f &center);
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    Vec3f GetExtent() const
        { return max - min; }

    void SetExtent(const Vec3f &dimensions);

    float GetRadiusSquared() const;
    float GetRadius() const;

    BoundingBox operator*(float scalar) const;
    BoundingBox &operator*=(float scalar);
    BoundingBox operator/(float scalar) const;
    BoundingBox &operator/=(float scalar);
    BoundingBox operator+(const Vec3f &offset) const;
    BoundingBox &operator+=(const Vec3f &offset);
    BoundingBox operator-(const Vec3f &offset) const;
    BoundingBox &operator-=(const Vec3f &offset);
    BoundingBox operator/(const Vec3f &scale) const;
    BoundingBox &operator/=(const Vec3f &scale);
    BoundingBox operator*(const Vec3f &scale) const;
    BoundingBox &operator*=(const Vec3f &scale);
    BoundingBox operator*(const Transform &transform) const;
    BoundingBox &operator*=(const Transform &transform);
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(const BoundingBox &other) const
        { return min == other.min && max == other.max; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const BoundingBox &other) const
        { return !operator==(other); }

    BoundingBox &Clear();
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Empty() const
        { return Memory::MemCmp(this, &empty, sizeof(BoundingBox)) == 0; }

    BoundingBox &Extend(const Vec3f &vec);
    BoundingBox &Extend(const BoundingBox &bb);
    
    // do the AABB's intersect at all?
    [[nodiscard]]
    bool Intersects(const BoundingBox &other) const;

    // does this AABB completely contain other?
    [[nodiscard]]
    bool Contains(const BoundingBox &other) const;

    [[nodiscard]]
    bool ContainsPoint(const Vec3f &vec) const;
    
    [[nodiscard]]
    float Area() const;
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsFinite() const
        { return MathUtil::IsFinite(min) && MathUtil::IsFinite(max); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsValid() const
        { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(min.GetHashCode());
        hc.Add(max.GetHashCode());

        return hc;
    }

    Vec3f   min;
    Vec3f   max;
};

} // namespace hyperion

#endif
