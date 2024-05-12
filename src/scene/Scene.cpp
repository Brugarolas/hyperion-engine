/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Scene.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/systems/VisibilityStateUpdaterSystem.hpp>
#include <scene/ecs/systems/EntityDrawDataUpdaterSystem.hpp>
#include <scene/ecs/systems/EntityMeshDirtyStateSystem.hpp>
#include <scene/ecs/systems/WorldAABBUpdaterSystem.hpp>
#include <scene/ecs/systems/LightVisibilityUpdaterSystem.hpp>
#include <scene/ecs/systems/ShadowMapUpdaterSystem.hpp>
#include <scene/ecs/systems/EnvGridUpdaterSystem.hpp>
#include <scene/ecs/systems/AnimationSystem.hpp>
#include <scene/ecs/systems/SkySystem.hpp>
#include <scene/ecs/systems/AudioSystem.hpp>
#include <scene/ecs/systems/BLASUpdaterSystem.hpp>
#include <scene/ecs/systems/PhysicsSystem.hpp>
#include <scene/ecs/systems/TerrainSystem.hpp>
#include <scene/ecs/systems/ScriptSystem.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/ReflectionProbeRenderer.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <math/Halton.hpp>

#include <Engine.hpp>

// #define HYP_VISIBILITY_CHECK_DEBUG
// #define HYP_DISABLE_VISIBILITY_CHECK

namespace hyperion {

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(BindLights) : renderer::RenderCommand
{
    SizeType num_lights;
    Pair<ID<Light>, LightDrawProxy> *lights;

    RENDER_COMMAND(BindLights)(SizeType num_lights, Pair<ID<Light>, LightDrawProxy> *lights)
        : num_lights(num_lights),
          lights(lights)
    {
    }

    virtual Result operator()()
    {
        for (SizeType i = 0; i < num_lights; i++) {
            g_engine->GetRenderState().BindLight(lights[i].first, lights[i].second);
        }

        delete[] lights;

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(BindEnvProbes) : renderer::RenderCommand
{
    Array<Pair<ID<EnvProbe>, EnvProbeType>> items;

    RENDER_COMMAND(BindEnvProbes)(Array<Pair<ID<EnvProbe>, EnvProbeType>> &&items)
        : items(std::move(items))
    {
    }

    virtual Result operator()()
    {
        for (const auto &it : items) {
            g_engine->GetRenderState().BindEnvProbe(it.second, it.first);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

Scene::Scene()
    : Scene(Handle<Camera>::empty, { })
{
}

Scene::Scene(Handle<Camera> camera)
    : Scene(std::move(camera), { })
{
}

Scene::Scene(
    Handle<Camera> camera,
    const InitInfo &info
) : BasicObject(info),
    HasDrawProxy(),
    m_camera(std::move(camera)),
    m_root_node_proxy(new Node("<ROOT>", ID<Entity>::invalid, Transform { }, this)),
    m_environment(new RenderEnvironment(this)),
    m_world(nullptr),
    m_is_non_world_scene(info.flags & InitInfo::SCENE_FLAGS_NON_WORLD),
    m_is_audio_listener(false),
    m_entity_manager(new EntityManager(info.thread_mask, this)),
    m_octree(m_entity_manager, BoundingBox(Vec3f(-250.0f), Vec3f(250.0f))),
    m_mutation_state(DataMutationState::DIRTY)
{
    m_entity_manager->AddSystem<WorldAABBUpdaterSystem>();
    m_entity_manager->AddSystem<EntityMeshDirtyStateSystem>();
    m_entity_manager->AddSystem<EntityDrawDataUpdaterSystem>();
    m_entity_manager->AddSystem<LightVisibilityUpdaterSystem>();
    m_entity_manager->AddSystem<VisibilityStateUpdaterSystem>();
    m_entity_manager->AddSystem<ShadowMapUpdaterSystem>();
    m_entity_manager->AddSystem<EnvGridUpdaterSystem>();
    m_entity_manager->AddSystem<AnimationSystem>();
    m_entity_manager->AddSystem<SkySystem>();
    m_entity_manager->AddSystem<AudioSystem>();
    m_entity_manager->AddSystem<BLASUpdaterSystem>();
    m_entity_manager->AddSystem<PhysicsSystem>();
    m_entity_manager->AddSystem<TerrainSystem>();
    m_entity_manager->AddSystem<ScriptSystem>();

    m_root_node_proxy->SetScene(this);
}

Scene::~Scene()
{
    DebugLog(LogType::Debug, "Destroy scene with ID %u (name: %s) from thread : %s\n", GetID().Value(), *GetName(), *ThreadID::Current().name);
    
    m_octree.SetEntityManager(nullptr);
    m_octree.Clear();

    m_camera.Reset();
    m_tlas.Reset();
    m_environment.Reset();

    if (m_root_node_proxy.IsValid()) {
        m_root_node_proxy->SetScene(nullptr);
    }

    // Move so destruction of components can check GetEntityManager() returns nullptr
    RC<EntityManager> entity_manager = std::move(m_entity_manager);
    entity_manager.Reset();

    HYP_SYNC_RENDER();
}
    
void Scene::Init()
{
    if (IsInitCalled()) {
        return;
    }
    
    BasicObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
    {
        m_environment.Reset();
        // since environment is now reset
        m_is_non_world_scene = true;
    }));

    InitObject(m_camera);
    m_render_list.SetCamera(m_camera);

    if (IsWorldScene()) {
        if (!m_tlas) {
            if (g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported() && HasFlags(InitInfo::SCENE_FLAGS_HAS_TLAS)) {
                CreateTLAS();
            } else {
                SetFlags(InitInfo::SCENE_FLAGS_HAS_TLAS, false);
            }
        }

        InitObject(m_tlas);
        
        m_environment->Init();

        if (m_tlas) {
            m_environment->SetTLAS(m_tlas);
        }
    }

    SetReady(true);
}

void Scene::SetCamera(Handle<Camera> camera)
{
    m_camera = std::move(camera);
    InitObject(m_camera);

    m_render_list.SetCamera(m_camera);
}

void Scene::SetWorld(World *world)
{
    // be cautious
    // Threads::AssertOnThread(ThreadName::THREAD_GAME);

    m_world = world;
}

NodeProxy Scene::FindNodeWithEntity(ID<Entity> entity) const
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertThrow(m_root_node_proxy);
    return m_root_node_proxy->FindChildWithEntity(entity);
}

NodeProxy Scene::FindNodeByName(const String &name) const
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertThrow(m_root_node_proxy);
    return m_root_node_proxy->FindChildByName(name);
}

void Scene::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertReady();
    
    // Rebuild any octants that have had structural changes
    // IMPORTANT: must be ran at start of tick, as pointers to octants' visibility states will be
    // stored on VisibilityStateComponent.
    m_octree.PerformUpdates();
    m_octree.NextVisibilityState();

    if (m_camera.IsValid()) {
        m_camera->Update(delta);

        m_octree.CalculateVisibility(m_camera);

        if (m_camera->GetViewProjectionMatrix() != m_last_view_projection_matrix) {
            m_last_view_projection_matrix = m_camera->GetViewProjectionMatrix();
            m_mutation_state |= DataMutationState::DIRTY;
        }
    }

    m_entity_manager->Update(delta);

    if (IsWorldScene()) {
        // update render environment
        m_environment->Update(delta);
    }

    EnqueueRenderUpdates();
}

void Scene::CollectEntities(
    RenderList &render_list,
    const Handle<Camera> &camera,
    const Optional<RenderableAttributeSet> &override_attributes,
    bool skip_frustum_culling
) const
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME | ThreadName::THREAD_TASK);

    if (!camera) {
        return;
    }

    const ID<Camera> camera_id = camera.GetID();

    const VisibilityStateSnapshot &visibility_state_snapshot = m_octree.GetVisibilityState()->GetSnapshot(camera_id);

    for (auto it : m_entity_manager->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, VisibilityStateComponent>()) {
        auto [entity_id, mesh_component, transform_component, bounding_box_component, visibility_state_component] = it;

        if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE)) {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
            if (!visibility_state_component.visibility_state) {
                continue;
            }

            if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot)) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                DebugLog(
                    LogType::Debug,
                    "Skipping entity #%u for camera #%u due to visibility state being invalid.\n",
                    entity_id.Value(),
                    camera_id.Value()
                );
#endif

                continue;
            }
#endif
        }

        AssertThrow(mesh_component.proxy != nullptr);

        render_list.PushEntityToRender(
            entity_id,
            *mesh_component.proxy
        );
    }

    render_list.UpdateOnRenderThread(
        camera->GetFramebuffer(),
        override_attributes
    );
}

void Scene::CollectDynamicEntities(
    RenderList &render_list,
    const Handle<Camera> &camera,
    const Optional<RenderableAttributeSet> &override_attributes,
    bool skip_frustum_culling
) const
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME | ThreadName::THREAD_TASK);

    if (!camera) {
        return;
    }

    const ID<Camera> camera_id = camera.GetID();
    
    const VisibilityStateSnapshot &visibility_state_snapshot = m_octree.GetVisibilityState()->GetSnapshot(camera_id);

    for (auto it : m_entity_manager->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::DYNAMIC>>()) {
        auto [entity_id, mesh_component, transform_component, bounding_box_component, visibility_state_component, _] = it;

        if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE)) {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
            if (!visibility_state_component.visibility_state) {
                continue;
            }


            if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot)) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                DebugLog(
                    LogType::Debug,
                    "Skipping entity #%u for camera #%u due to visibility state being invalid.\n",
                    entity_id.Value(),
                    camera_id.Value()
                );
#endif

                continue;
            }
#endif
        }

        AssertThrow(mesh_component.proxy != nullptr);

        render_list.PushEntityToRender(
            entity_id,
            *mesh_component.proxy
        );
    }

    render_list.UpdateOnRenderThread(
        camera->GetFramebuffer(),
        override_attributes
    );
}

void Scene::CollectStaticEntities(
    RenderList &render_list,
    const Handle<Camera> &camera,
    const Optional<RenderableAttributeSet> &override_attributes,
    bool skip_frustum_culling
) const
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME | ThreadName::THREAD_TASK);

    if (!camera) {
        return;
    }

    const ID<Camera> camera_id = camera.GetID();
    
    const VisibilityStateSnapshot &visibility_state_snapshot = m_octree.GetVisibilityState()->GetSnapshot(camera_id);

    for (auto it : m_entity_manager->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::STATIC>>()) {
        auto [entity_id, mesh_component, transform_component, bounding_box_component, visibility_state_component, _] = it;

        if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE)) {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
            if (!visibility_state_component.visibility_state) {
                continue;
            }


            if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot)) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                DebugLog(
                    LogType::Debug,
                    "Skipping entity #%u for camera #%u due to visibility state being invalid.\n",
                    entity_id.Value(),
                    camera_id.Value()
                );
#endif

                continue;
            }
#endif
        }

        AssertThrow(mesh_component.proxy != nullptr);

        render_list.PushEntityToRender(
            entity_id,
            *mesh_component.proxy
        );
    }

    render_list.UpdateOnRenderThread(
        camera->GetFramebuffer(),
        override_attributes
    );
}

void Scene::EnqueueRenderUpdates()
{
    struct RENDER_COMMAND(UpdateSceneRenderData) : renderer::RenderCommand
    {
        ID<Scene>           id;
        BoundingBox         aabb;
        float               global_timer;
        FogParams           fog_params;
        RenderEnvironment   *render_environment;
        SceneDrawProxy      &draw_proxy;

        RENDER_COMMAND(UpdateSceneRenderData)(
            ID<Scene> id,
            const BoundingBox &aabb,
            float global_timer,
            const FogParams &fog_params,
            RenderEnvironment *render_environment,
            SceneDrawProxy &draw_proxy
        ) : id(id),
            aabb(aabb),
            global_timer(global_timer),
            fog_params(fog_params),
            render_environment(render_environment),
            draw_proxy(draw_proxy)
        {
        }

        virtual Result operator()()
        {
            const uint frame_counter = render_environment->GetFrameCounter();

            draw_proxy.frame_counter = frame_counter;

            SceneShaderData shader_data { };
            shader_data.aabb_max         = Vec4f(aabb.max, 1.0f);
            shader_data.aabb_min         = Vec4f(aabb.min, 1.0f);
            shader_data.fog_params       = Vec4f(float(fog_params.color.Packed()), fog_params.start_distance, fog_params.end_distance, 0.0f);
            shader_data.global_timer     = global_timer;
            shader_data.frame_counter    = frame_counter;
            shader_data.enabled_render_components_mask = render_environment->GetEnabledRenderComponentsMask();
            
            g_engine->GetRenderData()->scenes.Set(id.ToIndex(), shader_data);

            HYPERION_RETURN_OK;
        }
    };

    PUSH_RENDER_COMMAND(
        UpdateSceneRenderData, 
        m_id,
        m_root_node_proxy.GetWorldAABB(),
        m_environment->GetGlobalTimer(),
        m_fog_params,
        m_environment.Get(),
        m_draw_proxy
    );

    m_mutation_state = DataMutationState::CLEAN;
}

bool Scene::CreateTLAS()
{
    AssertThrowMsg(IsWorldScene(), "Can only create TLAS for world scenes");
    AssertIsInitCalled();

    if (m_tlas) {
        // TLAS already exists
        return true;
    }
    
    if (!g_engine->GetConfig().Get(CONFIG_RT_ENABLED)) {
        // cannot create TLAS if RT is not supported.
        SetFlags(InitInfo::SCENE_FLAGS_HAS_TLAS, false);

        return false;
    }

    m_tlas = CreateObject<TLAS>();

    if (IsReady()) {
        InitObject(m_tlas);

        m_environment->SetTLAS(m_tlas);
    }

    SetFlags(InitInfo::SCENE_FLAGS_HAS_TLAS, true);

    return true;
}

} // namespace hyperion
