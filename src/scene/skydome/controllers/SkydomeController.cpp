#include <scene/skydome/controllers/SkydomeController.hpp>
#include <rendering/render_components/sky/SkydomeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <asset/serialization/fbom/FBOMObject.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

SkydomeController::SkydomeController()
    : Controller()
{
}

void SkydomeController::OnAdded()
{
    m_dome_node = g_asset_manager->Load<Node>("models/cube.obj");
    m_dome_node.Scale(150.0f);

    // m_dome = dome_node[0].GetEntity();

    // if (m_dome) {    
    //     m_dome->SetFlags(Entity::InitInfo::ENTITY_FLAGS_HAS_BLAS, false);
    //     m_dome->SetScale(150.0f);

    //     InitObject(m_dome);
    // }
}

void SkydomeController::OnRemoved()
{
}

void SkydomeController::OnUpdate(GameCounter::TickUnit delta)
{
}

void SkydomeController::OnDetachedFromScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        // m_dome_node.Remove();

        // scene->RemoveEntity(m_dome);

        if (scene->IsWorldScene() && m_skydome_renderer != nullptr) {
            scene->GetEnvironment()->RemoveRenderComponent<SkydomeRenderer>(HYP_NAME(TempSkydomeRenderer0));

            m_skydome_renderer = nullptr;
        }
    }
}

void SkydomeController::OnAttachedToScene(ID<Scene> id)
{
    if (auto scene = Handle<Scene>(id)) {
        // scene->AddEntity(m_dome);
    }

    AssertThrow(m_skydome_renderer == nullptr);

    if (Handle<Scene> scene = Handle<Scene>(id)) {
        if (scene->IsWorldScene()) {
            m_skydome_renderer = scene->GetEnvironment()->AddRenderComponent<SkydomeRenderer>(HYP_NAME(TempSkydomeRenderer0));

            // AssertThrow(m_dome.IsValid());
            // AssertThrow(m_dome->GetMaterial().IsValid());

            Handle<Material> material = CreateObject<Material>();
            material->SetBucket(Bucket::BUCKET_SKYBOX);
            material->SetTexture(Material::TextureKey::MATERIAL_TEXTURE_ALBEDO_MAP, m_skydome_renderer->GetCubemap());
            material->SetFaceCullMode(FaceCullMode::FRONT);
            material->SetIsDepthTestEnabled(false);
            material->SetIsDepthWriteEnabled(false);

            material->SetShader(g_shader_manager->GetOrCreate(
                HYP_NAME(Skybox),
                ShaderProperties(renderer::static_mesh_vertex_attributes)
            ));

            // m_dome->SetMaterial(std::move(material));
        }
    }
}

void SkydomeController::Serialize(fbom::FBOMObject &out) const
{
    out.SetProperty("controller_name", fbom::FBOMString(), Memory::StrLen(controller_name), controller_name);
}

fbom::FBOMResult SkydomeController::Deserialize(const fbom::FBOMObject &in)
{
    return fbom::FBOMResult::FBOM_OK;
}

} // namespace hyperion::v2
