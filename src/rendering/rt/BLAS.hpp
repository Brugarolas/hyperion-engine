/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BLAS_HPP
#define HYPERION_BLAS_HPP

#include <core/Base.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <math/Transform.hpp>

namespace hyperion {

using renderer::BottomLevelAccelerationStructure;
using renderer::AccelerationStructureFlagBits;
using renderer::Frame;

class Entity;

class HYP_API BLAS : public BasicObject<STUB_CLASS(BLAS)>
{
public:
    BLAS(
        ID<Entity> entity_id,
        Handle<Mesh> mesh,
        Handle<Material> material,
        const Transform &transform
    );
    BLAS(const BLAS &other)             = delete;
    BLAS &operator=(const BLAS &other)  = delete;
    ~BLAS();
    
    const BLASRef &GetInternalBLAS() const
        { return m_blas; }
    
    const Handle<Mesh> &GetMesh() const
        { return m_mesh; }

    void SetMesh(Handle<Mesh> mesh);
    
    const Handle<Material> &GetMaterial() const
        { return m_material; }

    void SetMaterial(Handle<Material> material);

    const Transform &GetTransform() const
        { return m_transform; }

    void SetTransform(const Transform &transform);

    void Init();
    void Update();
    void UpdateRender(
        Frame *frame,
        bool &out_was_rebuilt
    );

private:
    void SetNeedsUpdate()
        { m_blas->SetFlag(AccelerationStructureFlagBits::ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING); }

    bool NeedsUpdate() const
        { return bool(m_blas->GetFlags()); }

    ID<Entity>          m_entity_id;
    Handle<Mesh>        m_mesh;
    Handle<Material>    m_material;
    Transform           m_transform;
    BLASRef             m_blas;
};

} // namespace hyperion

#endif