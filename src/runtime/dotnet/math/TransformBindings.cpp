#include <math/Transform.hpp>
#include <runtime/dotnet/math/ManagedMathTypes.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;
using namespace hyperion::v2;

extern "C" {
    ManagedMatrix4 Transform_UpdateMatrix(ManagedTransform transform)
    {
        Transform t(transform);
        t.UpdateMatrix();
        return t.GetMatrix();
    }
}