#include "engine.h"

#include <asset/byte_reader.h>
#include <asset/asset_manager.h>

#include "components/post_fx.h"
#include "components/compute.h"

#include <rendering/backend/renderer_features.h>

#include <rendering/camera/ortho_camera.h>

namespace hyperion::v2 {

using renderer::VertexAttributeSet;
using renderer::Attachment;
using renderer::ImageView;
using renderer::FramebufferObject;

Engine::Engine(SystemSDL &_system, const char *app_name)
    : m_instance(new Instance(_system, app_name, "HyperionEngine")),
      shader_globals(nullptr),
      m_octree(BoundingBox(Vector3(-250.0f), Vector3(250.0f))),
      resources(this),
      assets(this),
      m_shadow_renderer(std::make_unique<OrthoCamera>(-50, 50, -50, 50, -50, 50)),
      render_thread_id(std::hash<std::thread::id>{}(std::this_thread::get_id()))
{
    m_octree.m_root = &m_octree_root;
}

Engine::~Engine()
{
    AssertThrowMsg(m_instance == nullptr, "Instance should have been destroyed");
}

void Engine::SetSpatialTransform(Spatial *spatial, const Transform &transform)
{
    AssertThrow(spatial != nullptr);
    
    spatial->UpdateShaderData(this);
}

void Engine::FindTextureFormatDefaults()
{
    const Device *device = m_instance->GetDevice();

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_COLOR,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_BGRA8_SRGB,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8 },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_DEPTH,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_24,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_GBUFFER,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
        )
    );

    m_texture_format_defaults.Set(
        TextureFormatDefault::TEXTURE_FORMAT_DEFAULT_STORAGE,
        device->GetFeatures().FindSupportedFormat(
            std::array{ Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                        Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
        )
    );
}

void Engine::PrepareSwapchain()
{

    auto shader = resources.shaders.Add(std::make_unique<Shader>(
        std::vector<SubShader>{
            {ShaderModule::Type::VERTEX, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/blit_vert.spv").Read()}},
            {ShaderModule::Type::FRAGMENT, {FileByteReader(AssetManager::GetInstance()->GetRootDir() + "/vkshaders/blit_frag.spv").Read()}}
        }
    ));

    shader->Init(this);

    uint32_t iteration = 0;
    
    auto render_pass = resources.render_passes.Add(std::make_unique<RenderPass>(
        renderer::RenderPassStage::PRESENT,
        renderer::RenderPass::Mode::RENDER_PASS_INLINE
    ));

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            m_instance->swapchain->extent,
            m_instance->swapchain->image_format,
            nullptr
        ),
        renderer::RenderPassStage::PRESENT
    ));

    m_render_pass_attachments.push_back(std::make_unique<renderer::Attachment>(
        std::make_unique<renderer::FramebufferImage2D>(
            m_instance->swapchain->extent,
            m_texture_format_defaults.Get(TEXTURE_FORMAT_DEFAULT_DEPTH),
            nullptr
        ),
        renderer::RenderPassStage::PRESENT
    ));
    
    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(m_instance->GetDevice()));
    }

    for (VkImage img : m_instance->swapchain->images) {
        auto fbo = std::make_unique<Framebuffer>(
            m_instance->swapchain->extent,
            render_pass.Acquire()
        );

        renderer::AttachmentRef *color_attachment_ref,
                                *depth_attachment_ref;

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[0]->AddAttachmentRef(
            m_instance->GetDevice(),
            img,
            renderer::Image::ToVkFormat(m_instance->swapchain->image_format),
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D,
            1, 1,
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &color_attachment_ref
        ));

        color_attachment_ref->SetBinding(0);

        fbo->GetFramebuffer().AddRenderPassAttachmentRef(color_attachment_ref);

        HYPERION_ASSERT_RESULT(m_render_pass_attachments[1]->AddAttachmentRef(
            m_instance->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &depth_attachment_ref
        ));

        fbo->GetFramebuffer().AddRenderPassAttachmentRef(depth_attachment_ref);

        depth_attachment_ref->SetBinding(1);

        if (iteration == 0) {
            render_pass->GetRenderPass().AddAttachmentRef(color_attachment_ref);
            render_pass->GetRenderPass().AddAttachmentRef(depth_attachment_ref);

            render_pass.Init();

            m_root_pipeline = std::make_unique<GraphicsPipeline>(
                shader.Acquire(),
                render_pass.Acquire(),
                VertexAttributeSet::static_mesh,
                Bucket::BUCKET_SWAPCHAIN
            );
        }

        m_root_pipeline->AddFramebuffer(resources.framebuffers.Add(std::move(fbo)));

        ++iteration;
    }

    m_root_pipeline->SetTopology(Topology::TRIANGLE_FAN);

    callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](...) {
        m_render_list_container.AddFramebuffersToPipelines(this);
        m_root_pipeline->Init(this);
    });
}

void Engine::Initialize()
{
    HYPERION_ASSERT_RESULT(m_instance->Initialize(true));

    FindTextureFormatDefaults();

    shader_globals = new ShaderGlobals(m_instance->GetFrameHandler()->NumFrames());
    
    /* for scene data */
    shader_globals->scenes.Create(m_instance->GetDevice());
    /* for materials */
    shader_globals->materials.Create(m_instance->GetDevice());
    /* for objects */
    shader_globals->objects.Create(m_instance->GetDevice());
    /* for skeletons */
    shader_globals->skeletons.Create(m_instance->GetDevice());



    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->AddSubDescriptor({
            .buffer = shader_globals->scenes.GetBuffers()[0].get(),
            .range = static_cast<uint32_t>(sizeof(SceneShaderData))
        });
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->AddSubDescriptor({
            .buffer = shader_globals->materials.GetBuffers()[0].get(),
            .range = static_cast<uint32_t>(sizeof(MaterialShaderData))
        });


    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
        ->AddSubDescriptor({
            .buffer = shader_globals->objects.GetBuffers()[0].get(),
            .range = static_cast<uint32_t>(sizeof(ObjectShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(2)
        ->AddSubDescriptor({
            .buffer = shader_globals->skeletons.GetBuffers()[0].get(),
            .range = static_cast<uint32_t>(sizeof(SkeletonShaderData))
        });



    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->AddSubDescriptor({
            .buffer = shader_globals->scenes.GetBuffers()[1].get(),
            .range = static_cast<uint32_t>(sizeof(SceneShaderData))
        });
    
    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(0)
        ->AddSubDescriptor({
            .buffer = shader_globals->materials.GetBuffers()[1].get(),
            .range = static_cast<uint32_t>(sizeof(MaterialShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
        ->AddSubDescriptor({
            .buffer = shader_globals->objects.GetBuffers()[1].get(),
            .range = static_cast<uint32_t>(sizeof(ObjectShaderData))
        });

    m_instance->GetDescriptorPool().GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT_FRAME_1)
        ->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(2)
        ->AddSubDescriptor({
            .buffer = shader_globals->skeletons.GetBuffers()[1].get(),
            .range = static_cast<uint32_t>(sizeof(SkeletonShaderData))
        });

    m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS)
        ->AddDescriptor<renderer::ImageSamplerDescriptor>(0);

    m_instance->GetDescriptorPool()
        .GetDescriptorSet(DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS_FRAME_1)
        ->AddDescriptor<renderer::ImageSamplerDescriptor>(0);

    /* for textures */
    shader_globals->textures.Create(this);
    
    callbacks.TriggerPersisted(EngineCallback::CREATE_RENDER_PASSES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_FRAMEBUFFERS, this);

    m_render_list_container.Create(this);
    
    callbacks.TriggerPersisted(EngineCallback::CREATE_SCENES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_TEXTURES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_SHADERS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_SPATIALS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_MESHES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_ACCELERATION_STRUCTURES, this);

    m_running = true;
}

void Engine::Destroy()
{
    m_running = false;

    callbacks.Trigger(EngineCallback::DESTROY_ACCELERATION_STRUCTURES, this);
    callbacks.Trigger(EngineCallback::DESTROY_MESHES, this);
    callbacks.Trigger(EngineCallback::DESTROY_MATERIALS, this);
    callbacks.Trigger(EngineCallback::DESTROY_LIGHTS, this);
    callbacks.Trigger(EngineCallback::DESTROY_SKELETONS, this);
    callbacks.Trigger(EngineCallback::DESTROY_SPATIALS, this);
    callbacks.Trigger(EngineCallback::DESTROY_SHADERS, this);
    callbacks.Trigger(EngineCallback::DESTROY_TEXTURES, this);
    callbacks.Trigger(EngineCallback::DESTROY_VOXELIZER, this);
    callbacks.Trigger(EngineCallback::DESTROY_DESCRIPTOR_SETS, this);
    callbacks.Trigger(EngineCallback::DESTROY_GRAPHICS_PIPELINES, this);
    callbacks.Trigger(EngineCallback::DESTROY_COMPUTE_PIPELINES, this);
    callbacks.Trigger(EngineCallback::DESTROY_RAYTRACING_PIPELINES, this);
    callbacks.Trigger(EngineCallback::DESTROY_SCENES, this);

    game_thread.Join();
    render_scheduler.Flush();

    AssertThrow(m_instance != nullptr);
    (void)m_instance->GetDevice()->Wait();
    
    m_render_list_container.Destroy(this);
    
    m_deferred_renderer.Destroy(this);
    m_shadow_renderer.Destroy(this);

    for (auto &attachment : m_render_pass_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Destroy(m_instance->GetDevice()));
    }

    
    callbacks.Trigger(EngineCallback::DESTROY_FRAMEBUFFERS, this);
    callbacks.Trigger(EngineCallback::DESTROY_RENDER_PASSES, this);

    resources.Destroy(this);

    

    if (shader_globals != nullptr) {
        shader_globals->scenes.Destroy(m_instance->GetDevice());
        shader_globals->objects.Destroy(m_instance->GetDevice());
        shader_globals->materials.Destroy(m_instance->GetDevice());
        shader_globals->skeletons.Destroy(m_instance->GetDevice());

        delete shader_globals;
    }

    m_instance->Destroy();
    m_instance.reset();
}

void Engine::Compile()
{
    m_deferred_renderer.Create(this);
    m_shadow_renderer.Create(this);

    callbacks.TriggerPersisted(EngineCallback::CREATE_SKELETONS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_MATERIALS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_LIGHTS, this);

    for (uint32_t i = 0; i < m_instance->GetFrameHandler()->NumFrames(); i++) {
        /* Finalize skeletons */
        shader_globals->skeletons.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize materials */
        shader_globals->materials.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize per-object data */
        shader_globals->objects.UpdateBuffer(m_instance->GetDevice(), i);

        /* Finalize per-object data */
        shader_globals->scenes.UpdateBuffer(m_instance->GetDevice(), i);
    }

    callbacks.TriggerPersisted(EngineCallback::CREATE_DESCRIPTOR_SETS, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_VOXELIZER, this);

    /* Finalize descriptor pool */
    HYPERION_ASSERT_RESULT(m_instance->GetDescriptorPool().Create(m_instance->GetDevice()));
    
    callbacks.TriggerPersisted(EngineCallback::CREATE_GRAPHICS_PIPELINES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_COMPUTE_PIPELINES, this);
    callbacks.TriggerPersisted(EngineCallback::CREATE_RAYTRACING_PIPELINES, this);
}

void Engine::Stop()
{
    
}

Ref<GraphicsPipeline> Engine::FindOrCreateGraphicsPipeline(
    Ref<Shader> &&shader,
    const VertexAttributeSet &vertex_attributes,
    Bucket bucket
)
{
    auto &render_list_bucket = m_render_list_container.Get(bucket);

    Ref<GraphicsPipeline> found_pipeline;
    
    for (auto &graphics_pipeline : render_list_bucket.graphics_pipelines) {
        if (graphics_pipeline->GetShader() != shader) {
            continue;
        }

        if (!(graphics_pipeline->GetVertexAttributes() & vertex_attributes)) {
            continue;
        }

        found_pipeline = graphics_pipeline.Acquire();

        break;
    }

    if (found_pipeline != nullptr) {
        return std::move(found_pipeline);
    }

    // create a pipeline with the given params
    return AddGraphicsPipeline(std::make_unique<GraphicsPipeline>(
        std::move(shader),
        render_list_bucket.render_pass.Acquire(),
        vertex_attributes,
        bucket
    ));
}

void Engine::ResetRenderBindings()
{
    render_bindings.scene_ids = {};
}

void Engine::UpdateRendererBuffersAndDescriptors(uint32_t frame_index)
{
    shader_globals->scenes.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->objects.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->materials.UpdateBuffer(m_instance->GetDevice(), frame_index);
    shader_globals->skeletons.UpdateBuffer(m_instance->GetDevice(), frame_index);

    shader_globals->textures.ApplyUpdates(this, frame_index);
}

void Engine::RenderShadows(CommandBuffer *primary, uint32_t frame_index)
{
    m_shadow_renderer.Render(this, primary, frame_index);
}

void Engine::RenderDeferred(CommandBuffer *primary, uint32_t frame_index)
{
    m_deferred_renderer.Render(this, primary, frame_index);
}

void Engine::RenderSwapchain(CommandBuffer *command_buffer) const
{
    auto *pipeline = m_root_pipeline->GetPipeline();
    const uint32_t acquired_image_index = m_instance->GetFrameHandler()->GetAcquiredImageIndex();

    m_root_pipeline->GetFramebuffers()[acquired_image_index]->BeginCapture(command_buffer);
    
    pipeline->Bind(command_buffer);

    m_instance->GetDescriptorPool().Bind(
        m_instance->GetDevice(),
        command_buffer,
        pipeline,
        {{
            .set = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL,
            .count = 1
        }}
    );

    /* TMP */
    m_instance->GetDescriptorPool().Bind(
        m_instance->GetDevice(),
        command_buffer,
        pipeline,
        {{
            .set = DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
            .count = 1
        }}
    );

    /* Render full screen quad overlay to blit deferred + all post fx onto screen. */
    PostEffect::full_screen_quad->Render(const_cast<Engine *>(this), command_buffer);
    
    m_root_pipeline->GetFramebuffers()[acquired_image_index]->EndCapture(command_buffer);
}
} // namespace hyperion::v2