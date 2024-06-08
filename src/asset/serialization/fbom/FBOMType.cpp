/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/serialization/fbom/FBOMType.hpp>
#include <asset/serialization/fbom/FBOMBaseTypes.hpp>

namespace hyperion::fbom {

FBOMType::FBOMType()
    : name("UNSET"),
      size(0),
      extends(nullptr)
{
}

FBOMType::FBOMType(const ANSIStringView &name, SizeType size)
    : name(name),
      size(size),
      extends(nullptr)
{
}

FBOMType::FBOMType(const ANSIStringView &name, SizeType size, const FBOMType &extends)
    : name(name),
      size(size),
      extends(new FBOMType(extends))
{
}

FBOMType::FBOMType(const FBOMType &other)
    : name(other.name),
      size(other.size),
      extends(nullptr)
{
    if (other.extends != nullptr) {
        extends = new FBOMType(*other.extends);
    }
}

FBOMType &FBOMType::operator=(const FBOMType &other)
{
    if (extends != nullptr) {
        delete extends;
    }

    name = other.name;
    size = other.size;
    extends = nullptr;

    if (other.extends != nullptr) {
        extends = new FBOMType(*other.extends);
    }

    return *this;
}

FBOMType::FBOMType(FBOMType &&other) noexcept
    : name(std::move(other.name)),
      size(other.size),
      extends(other.extends)
{
    other.size = 0;
    other.extends = nullptr;
}

FBOMType &FBOMType::operator=(FBOMType &&other) noexcept
{
    if (extends != nullptr) {
        delete extends;
    }

    name = std::move(other.name);
    size = other.size;
    extends = other.extends;

    other.size = 0;
    other.extends = nullptr;

    return *this;
}

FBOMType::~FBOMType()
{
    if (extends != nullptr) {
        delete extends;
    }
}

FBOMType FBOMType::Extend(const FBOMType &object) const
{
    return FBOMObjectType(object.name, *this);
}

bool FBOMType::IsOrExtends(const ANSIStringView &name) const
{
    if (this->name == name) {
        return true;
    }

    if (extends == nullptr || extends->IsUnset()) {
        return false;
    }

    return extends->IsOrExtends(name);
}

bool FBOMType::Is(const FBOMType &other, bool allow_unbounded) const
{
    if (name != other.name) {
        return false;
    }

    if (!allow_unbounded) {
        if (size != other.size) {
            return false;
        }
    }

    if (extends != nullptr) {
        if (!other.extends) {
            return false;
        }

        return extends->Is(*other.extends, allow_unbounded);
    }

    return true;
}

bool FBOMType::IsOrExtends(const FBOMType &other, bool allow_unbounded) const
{
    if (Is(other, allow_unbounded)) {
        return true;
    }

    return Extends(other, allow_unbounded);

    // if (*this == other) {
    //     return true;
    // }

    // if (allow_unbounded && (IsUnbouned() || other.IsUnbouned())) {
    //     if (name == other.name && extends == other.extends) { // don't size check
    //         return true;
    //     }
    // }

    // return Extends(other);
}

bool FBOMType::Extends(const FBOMType &other, bool allow_unbounded) const
{
    if (extends == nullptr || extends->IsUnset()) {
        return false;
    }

    if (extends->Is(other)) {
        return true;
    }

    return extends->Extends(other, allow_unbounded);
}

String FBOMType::ToString() const
{
    String str = String(name) + " (" + String::ToString(size) + ") ";

    if (extends != nullptr && !extends->IsUnset()) {
        str += "[" + extends->ToString() + "]";
    }

    return str;
}

} // namespace hyperion::fbom
