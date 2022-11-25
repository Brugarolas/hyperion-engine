#include "BLAS.hpp"
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::Result;

struct RENDER_COMMAND(CreateBLAS) : RenderCommandBase2
{
    renderer::BottomLevelAccelerationStructure *blas;

    RENDER_COMMAND(CreateBLAS)(renderer::BottomLevelAccelerationStructure *blas)
        : blas(blas)
    {
    }

    virtual Result operator()()
    {
        return blas->Create(Engine::Get()->GetDevice(), Engine::Get()->GetInstance());
    }
};

struct RENDER_COMMAND(DestroyBLAS) : RenderCommandBase2
{
    renderer::BottomLevelAccelerationStructure *blas;

    RENDER_COMMAND(DestroyBLAS)(renderer::BottomLevelAccelerationStructure *blas)
        : blas(blas)
    {
    }

    virtual Result operator()()
    {
        return blas->Destroy(Engine::Get()->GetDevice());
    }
};

BLAS::BLAS(
    IDBase entity_id,
    Handle<Mesh> &&mesh,
    Handle<Material> &&material,
    const Transform &transform
) : EngineComponentBase(),
    m_entity_id(entity_id),
    m_mesh(std::move(mesh)),
    m_material(std::move(material)),
    m_transform(transform)
{
}

BLAS::~BLAS()
{
    Teardown();
}

void BLAS::SetMesh(Handle<Mesh> &&mesh)
{
    // TODO: thread safety

    m_mesh = std::move(mesh);

    if (!m_blas.GetGeometries().empty()) {
        auto size = static_cast<UInt>(m_blas.GetGeometries().size());

        while (size) {
            m_blas.RemoveGeometry(--size);
        }
    }

    if (m_mesh) {
        Engine::Get()->InitObject(m_mesh);

        auto material_id = m_material
            ? m_material->GetID()
            : Material::empty_id;
        
        m_blas.AddGeometry(std::make_unique<AccelerationGeometry>(
            m_mesh->BuildPackedVertices(),
            m_mesh->BuildPackedIndices(),
            m_entity_id.ToIndex(),
            material_id.ToIndex()
        ));
    }
}

void BLAS::SetMaterial(Handle<Material> &&material)
{
    // TODO: thread safety

    m_material = std::move(material);

    if (IsInitCalled()) {
        auto material_id = m_material
            ? m_material->GetID()
            : Material::empty_id;
        
        const auto material_index = material_id.ToIndex();

        if (!m_blas.GetGeometries().empty()) {
            for (auto &geometry : m_blas.GetGeometries()) {
                if (!geometry) {
                    continue;
                }

                geometry->SetMaterialIndex(material_index);
            }

            m_blas.SetFlag(AccelerationStructureFlagBits::ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);
        }
    }
}

void BLAS::SetTransform(const Transform &transform)
{
    // TODO: thread safety

    m_transform = transform;

    if (IsInitCalled()) {
        m_blas.SetTransform(m_transform.GetMatrix());
    }
}

void BLAS::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    UInt material_index = 0;

    if (Engine::Get()->InitObject(m_material)) {
        material_index = m_material->GetID().ToIndex();
    }

    AssertThrow(Engine::Get()->InitObject(m_mesh));
    
    m_blas.SetTransform(m_transform.GetMatrix());
    m_blas.AddGeometry(std::make_unique<AccelerationGeometry>(
        m_mesh->BuildPackedVertices(),
        m_mesh->BuildPackedIndices(),
        m_entity_id.ToIndex(),
        material_index
    ));

    RenderCommands::Push<RENDER_COMMAND(CreateBLAS)>(&m_blas);

    HYP_FLUSH_RENDER_QUEUE();

    SetReady(true);

    OnTeardown([this]() {
        SetReady(false);

        RenderCommands::Push<RENDER_COMMAND(DestroyBLAS)>(&m_blas);

        HYP_FLUSH_RENDER_QUEUE();
    });
}

void BLAS::Update()
{
    // no-op
}

void BLAS::UpdateRender(
    
    Frame *frame,
    bool &out_was_rebuilt
)
{
#if 0 // TopLevelAccelerationStructure does this work here.
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (!NeedsUpdate()) {
        return;
    }
    
    HYPERION_ASSERT_RESULT(m_wrapped.UpdateStructure(Engine::Get()->GetInstance(), out_was_rebuilt));
#endif
}

} // namespace hyperion::v2