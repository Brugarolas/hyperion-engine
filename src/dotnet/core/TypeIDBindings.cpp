/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <system/Debug.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {
HYP_EXPORT void TypeID_FromString(const char *type_name, TypeID *out_type_id)
{
    *out_type_id = TypeID::FromString(type_name);
}
} // extern "C"