/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <math/Transform.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {
HYP_EXPORT void Transform_UpdateMatrix(Transform *transform)
{
    transform->UpdateMatrix();
}
} // extern "C"