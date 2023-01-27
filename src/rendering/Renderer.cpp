#include "Renderer.hpp"
#include <scene/Entity.hpp>
#include <Engine.hpp>
#include <Constants.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>

namespace hyperion::v2 {

using renderer::Result;

#pragma region Render commands

struct RENDER_COMMAND(CreateGraphicsPipeline) : RenderCommand
{
    renderer::GraphicsPipeline *pipeline;
    renderer::ShaderProgram *shader_program;
    renderer::RenderPass *render_pass;
    Array<renderer::FramebufferObject *> framebuffers;
    Array<Array<renderer::CommandBuffer *>> command_buffers;
    RenderableAttributeSet attributes;

    RENDER_COMMAND(CreateGraphicsPipeline)(
        renderer::GraphicsPipeline *pipeline,
        renderer::ShaderProgram *shader_program,
        renderer::RenderPass *render_pass,
        Array<renderer::FramebufferObject *> &&framebuffers,
        Array<Array<renderer::CommandBuffer *>> &&command_buffers,
        const RenderableAttributeSet &attributes
    ) : pipeline(pipeline),
        shader_program(shader_program),
        render_pass(render_pass),
        framebuffers(std::move(framebuffers)),
        command_buffers(std::move(command_buffers)),
        attributes(attributes)
    {
    }

    virtual Result operator()()
    {
        renderer::GraphicsPipeline::ConstructionInfo construction_info {
            .vertex_attributes = attributes.mesh_attributes.vertex_attributes,
            .topology          = attributes.mesh_attributes.topology,
            .cull_mode         = attributes.material_attributes.cull_faces,
            .fill_mode         = attributes.material_attributes.fill_mode,
            .blend_mode        = attributes.material_attributes.blend_mode,
            .depth_test        = bool(attributes.material_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_TEST),
            .depth_write       = bool(attributes.material_attributes.flags & MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_DEPTH_WRITE),
            .shader            = shader_program,
            .render_pass       = render_pass,
            .stencil_state     = attributes.stencil_state
        };

        for (renderer::FramebufferObject *framebuffer : framebuffers) {
            construction_info.fbos.push_back(framebuffer);
        }

        for (UInt i = 0; i < max_frames_in_flight; i++) {
            for (UInt j = 0; j < UInt(command_buffers[i].Size()); j++) {
                HYPERION_BUBBLE_ERRORS(command_buffers[i][j]->Create(
                    Engine::Get()->GetGPUInstance()->GetDevice(),
                    Engine::Get()->GetGPUInstance()->GetGraphicsCommandPool(j)
                ));
            }
        }

        return pipeline->Create(
            Engine::Get()->GetGPUDevice(),
            std::move(construction_info),
            &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
        );
    }
};

struct RENDER_COMMAND(DestroyGraphicsPipeline) : RenderCommand
{
    renderer::GraphicsPipeline *pipeline;

    RENDER_COMMAND(DestroyGraphicsPipeline)(renderer::GraphicsPipeline *pipeline)
        : pipeline(pipeline)
    {
    }

    virtual Result operator()()
    {
        return pipeline->Destroy(Engine::Get()->GetGPUDevice());
    }
};

#pragma endregion

RenderGroup::RenderGroup(
    Handle<Shader> &&shader,
    const RenderableAttributeSet &renderable_attributes
) : EngineComponentBase(),
    m_pipeline(std::make_unique<renderer::GraphicsPipeline>()),
    m_shader(std::move(shader)),
    m_renderable_attributes(renderable_attributes)
{
}

RenderGroup::RenderGroup(
    Handle<Shader> &&shader,
    const RenderableAttributeSet &renderable_attributes,
    const Array<const DescriptorSet *> &used_descriptor_sets
) : EngineComponentBase(),
    m_pipeline(std::make_unique<renderer::GraphicsPipeline>(used_descriptor_sets)),
    m_shader(std::move(shader)),
    m_renderable_attributes(renderable_attributes)
{
}

RenderGroup::~RenderGroup()
{
    Teardown();
}

void RenderGroup::RemoveFramebuffer(ID<Framebuffer> id)
{
    const auto it = m_fbos.FindIf([&](const auto &item) {
        return item->GetID() == id;
    });

    if (it == m_fbos.End()) {
        return;
    }
    
    m_fbos.Erase(it);
}

void RenderGroup::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    // create our indirect renderer
    // will be created with some initial size.
    m_indirect_renderer.Create();

    AssertThrow(m_fbos.Any());

    for (auto &fbo : m_fbos) {
        AssertThrow(fbo.IsValid());
        InitObject(fbo);
    }

    AssertThrow(m_shader.IsValid());
    InitObject(m_shader);

    for (UInt i = 0; i < max_frames_in_flight; i++) {
        for (auto &command_buffer : m_command_buffers[i]) {
            command_buffer.Reset(new CommandBuffer(CommandBuffer::Type::COMMAND_BUFFER_SECONDARY));
        }
    }

    OnInit(Engine::Get()->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](...) {
        renderer::RenderPass *render_pass = nullptr;
        
        Array<renderer::FramebufferObject *> framebuffers;
        framebuffers.Reserve(m_fbos.Size());

        for (auto &fbo : m_fbos) {
            if (render_pass == nullptr) {
                render_pass = &fbo->GetRenderPass();
            }

            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                framebuffers.PushBack(&fbo->GetFramebuffer(frame_index));
            }
        }

        Array<Array<renderer::CommandBuffer *>> command_buffers;
        command_buffers.Reserve(m_command_buffers.Size());
        
        for (auto &item : m_command_buffers) {
            Array<renderer::CommandBuffer *> frame_command_buffers;
            frame_command_buffers.Reserve(item.Size());

            for (auto &command_buffer : item) {
                frame_command_buffers.PushBack(command_buffer.Get());
            }

            command_buffers.PushBack(std::move(frame_command_buffers));
        }

        RenderCommands::Push<RENDER_COMMAND(CreateGraphicsPipeline)>(
            m_pipeline.get(),
            m_shader->GetShaderProgram(),
            render_pass,
            std::move(framebuffers),
            std::move(command_buffers),
            m_renderable_attributes
        );
            
        SetReady(true);

        OnTeardown([this]() {
            SetReady(false);

            m_indirect_renderer.Destroy(); // make sure we have the render queue flush at the end of
                                                 // this, as the indirect renderer has a call back that needs to be exec'd
                                                 // before the destructor is called

            m_shader.Reset();

            for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                for (UInt i = 0; i < UInt(m_command_buffers[frame_index].Size()); i++) {
                    Engine::Get()->SafeRelease(std::move(m_command_buffers[frame_index][i]));
                }
            }

            for (auto &fbo : m_fbos) {
                fbo.Reset();
            }

            RenderCommands::Push<RENDER_COMMAND(DestroyGraphicsPipeline)>(m_pipeline.get());
            
            HYP_SYNC_RENDER();
        });
    }));
}

void RenderGroup::CollectDrawCalls()
{
    Threads::AssertOnThread(THREAD_RENDER | THREAD_TASK);

    AssertReady();

    m_indirect_renderer.GetDrawState().Reset();
    m_divided_draw_calls.Clear();

    DrawCallCollection previous_draw_state = std::move(m_draw_state);

    for (EntityDrawProxy &draw_proxy : m_draw_proxies) {
        AssertThrow(draw_proxy.mesh_id.IsValid());

        DrawCallID draw_call_id;

        if constexpr (DrawCall::unique_per_material) {
            draw_call_id = DrawCallID(draw_proxy.mesh_id, draw_proxy.material_id);
        } else {
            draw_call_id = DrawCallID(draw_proxy.mesh_id);
        }

        BufferTicket<EntityInstanceBatch> batch_index = 0;

        if (DrawCall *draw_call = previous_draw_state.TakeDrawCall(draw_call_id)) {
            // take the batch for reuse
            if ((batch_index = draw_call->batch_index)) {
                Engine::Get()->shader_globals->entity_instance_batches.ResetBatch(batch_index);
            }

            draw_call->batch_index = 0;
        }

        m_draw_state.PushDrawCall(batch_index, draw_call_id, draw_proxy);
    }

    previous_draw_state.Reset();

    // register draw calls for indirect rendering
    for (DrawCall &draw_call : m_draw_state.draw_calls) {
        DrawCommandData draw_command_data;
        m_indirect_renderer.GetDrawState().PushDrawCall(draw_call, draw_command_data);
        draw_call.draw_command_index = draw_command_data.draw_command_index;
    }

    m_draw_proxies.Clear();
}

void RenderGroup::PerformOcclusionCulling(Frame *frame, const CullData *cull_data)
{
    if (!use_draw_indirect) {
        return;
    }

    Threads::AssertOnThread(THREAD_RENDER);

    AssertThrow(cull_data != nullptr);

    m_indirect_renderer.ExecuteCullShaderInBatches(
        frame,
        *cull_data
    );
}

static void GetDividedDrawCalls(
    const Array<DrawCall> &draw_calls,
    UInt num_batches,
    Array<Array<DrawCall>> &out_divided_draw_calls
)
{
    out_divided_draw_calls.Resize(num_batches);

    const UInt num_draw_calls = UInt(draw_calls.Size());
    const UInt num_draw_calls_divided = (num_draw_calls + num_batches - 1) / num_batches;

    UInt draw_call_index = 0;

    for (SizeType container_index = 0; container_index < num_async_rendering_command_buffers; container_index++) {
        auto &container = out_divided_draw_calls[container_index];
        container.Reserve(num_draw_calls_divided);

        for (SizeType i = 0; i < num_draw_calls_divided && draw_call_index < num_draw_calls; i++, draw_call_index++) {
            container.PushBack(draw_calls[draw_call_index]);
        }
    }
}

static void BindGlobalDescriptorSets(
    Frame *frame,
    renderer::GraphicsPipeline *pipeline,
    CommandBuffer *command_buffer
)
{
    const UInt frame_index = frame->GetFrameIndex();

    command_buffer->BindDescriptorSets(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        pipeline,
        FixedArray<DescriptorSet::Index, 2> { DescriptorSet::global_buffer_mapping[frame_index], DescriptorSet::scene_buffer_mapping[frame_index] },
        FixedArray<DescriptorSet::Index, 2> { DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL, DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE },
        FixedArray {
            HYP_RENDER_OBJECT_OFFSET(Scene, Engine::Get()->GetRenderState().GetScene().id.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(Light, 0),
            HYP_RENDER_OBJECT_OFFSET(EnvGrid, Engine::Get()->GetRenderState().bound_env_grid.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(EnvProbe, Engine::Get()->GetRenderState().current_env_probe.ToIndex()),
            HYP_RENDER_OBJECT_OFFSET(Camera, Engine::Get()->GetRenderState().GetCamera().id.ToIndex())
        }
    );

#if HYP_FEATURES_BINDLESS_TEXTURES
    /* Bindless textures */
    Engine::Get()->GetGPUInstance()->GetDescriptorPool().Bind(
        Engine::Get()->GetGPUDevice(),
        command_buffer,
        pipeline,
        {
            {.set = DescriptorSet::bindless_textures_mapping[frame_index], .count = 1},
            {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS}
        }
    );
#endif
                    
    Engine::Get()->GetGPUInstance()->GetDescriptorPool().Bind(
        Engine::Get()->GetGPUDevice(),
        command_buffer,
        pipeline,
        {
            {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER, .count = 1},
        }
    );
}

static void BindPerObjectDescriptorSets(
    Frame *frame,
    renderer::GraphicsPipeline *pipeline,
    CommandBuffer *command_buffer,
    UInt batch_index,
    UInt skeleton_index,
    UInt material_index
)
{
    const UInt frame_index = frame->GetFrameIndex();
    
#if HYP_FEATURES_BINDLESS_TEXTURES
    if constexpr (use_indexed_array_for_object_data) {
        command_buffer->BindDescriptorSets(
            Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
            pipeline,
            FixedArray<DescriptorSet::Index, 1> { DescriptorSet::object_buffer_mapping[frame_index] },
            FixedArray<DescriptorSet::Index, 1> { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT },
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Skeleton, skeleton_index),
                UInt32(batch_index * sizeof(EntityInstanceBatch))
            }
        );
    } else {
        command_buffer->BindDescriptorSets(
            Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
            pipeline,
            FixedArray<DescriptorSet::Index, 1> { DescriptorSet::object_buffer_mapping[frame_index] },
            FixedArray<DescriptorSet::Index, 1> { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT },
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Material, material_index),
                HYP_RENDER_OBJECT_OFFSET(Skeleton, skeleton_index),
                UInt32(batch_index * sizeof(EntityInstanceBatch))
            }
        );
    }
#else
    if constexpr (use_indexed_array_for_object_data) {
        command_buffer->BindDescriptorSets(
            Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
            pipeline,
            FixedArray<DescriptorSet::Index, 2> { DescriptorSet::object_buffer_mapping[frame_index], DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, material_index, frame_index) },
            FixedArray<DescriptorSet::Index, 2> { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT, DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES },
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Skeleton, skeleton_index),
                UInt32(batch_index * sizeof(EntityInstanceBatch))
            }
        );
    } else {
        command_buffer->BindDescriptorSets(
            Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
            pipeline,
            FixedArray<DescriptorSet::Index, 2> { DescriptorSet::object_buffer_mapping[frame_index], DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, material_index, frame_index) },
            FixedArray<DescriptorSet::Index, 2> { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT, DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES },
            FixedArray {
                HYP_RENDER_OBJECT_OFFSET(Material, material_index),
                HYP_RENDER_OBJECT_OFFSET(Skeleton, skeleton_index),
                UInt32(batch_index * sizeof(EntityInstanceBatch))
            }
        );
    }
#endif
}

template <bool IsIndirect>
static HYP_FORCE_INLINE void
RenderAll(
    Frame *frame,
    FixedArray<FixedArray<UniquePtr<CommandBuffer>, num_async_rendering_command_buffers>, max_frames_in_flight> &command_buffers,
    UInt &command_buffer_index,
    renderer::GraphicsPipeline *pipeline,
    IndirectRenderer *indirect_renderer,
    Array<Array<DrawCall>> &divided_draw_calls,
    const DrawCallCollection &draw_state,
    const RenderResourceManager &render_resources
)
{
    if (draw_state.draw_calls.Empty()) {
        return;
    }

    const auto &scene_binding = Engine::Get()->GetRenderState().GetScene();
    const ID<Scene> scene_id = scene_binding.id;

    const UInt frame_index = frame->GetFrameIndex();

    const auto num_batches = use_parallel_rendering
        ? MathUtil::Min(UInt(Engine::Get()->task_system.GetPool(TASK_PRIORITY_HIGH).threads.Size()), num_async_rendering_command_buffers)
        : 1u;
    
    GetDividedDrawCalls(
        draw_state.draw_calls,
        num_async_rendering_command_buffers,
        divided_draw_calls
    );

    // rather than using a single integer, we have to set states in a fixed array
    // because otherwise we'd need to use an atomic integer
    FixedArray<UInt, num_async_rendering_command_buffers> command_buffers_recorded_states { };
    
    // always run renderer items as HIGH priority,
    // so we do not lock up because we're waiting for a large process to
    // complete in the same thread
    Engine::Get()->task_system.ParallelForEach(
        TASK_PRIORITY_HIGH,
        num_batches,
        divided_draw_calls,
        [frame, pipeline, indirect_renderer, &command_buffers, &command_buffers_recorded_states, frame_index, &render_resources](const Array<DrawCall> &draw_calls, UInt index, UInt) {
            if (draw_calls.Empty()) {
                return;
            }

            command_buffers[frame_index][index/*(command_buffer_index + batch_index) % static_cast<UInt>(command_buffers.Size())*/]->Record(
                Engine::Get()->GetGPUDevice(),
                pipeline->GetConstructionInfo().render_pass,
                [&](CommandBuffer *secondary) {
                    pipeline->Bind(secondary);

                    BindGlobalDescriptorSets(
                        frame,
                        pipeline,
                        secondary
                    );

                    for (const DrawCall &draw_call : draw_calls) {
                        AssertThrow(draw_call.mesh != nullptr);

                        const EntityInstanceBatch &entity_batch = Engine::Get()->shader_globals->entity_instance_batches.Get(draw_call.batch_index);

                        BindPerObjectDescriptorSets(
                            frame,
                            pipeline,
                            secondary,
                            draw_call.batch_index,
                            draw_call.skeleton_id.ToIndex(),
                            draw_call.material_id.ToIndex()
                        );

#ifdef HYP_DEBUG_MODE
                        // AssertThrowMsg(render_resources.IsUsed(draw_call.mesh_id), "Mesh ID not in used resource map; dangling pointer is possible!");
                        // AssertThrowMsg(render_resources.IsUsed(draw_call.material_id), "Material ID not in used resource map; dangling pointer is possible!");
#endif

                        if constexpr (IsIndirect) {
#ifdef HYP_DEBUG_MODE
                            AssertThrow(draw_call.draw_command_index * sizeof(IndirectDrawCommand) < indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index)->size);
#endif
                            
                            draw_call.mesh->RenderIndirect(
                                secondary,
                                indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index).Get(),
                                draw_call.draw_command_index * sizeof(IndirectDrawCommand)
                            );
                        } else {
                            draw_call.mesh->Render(secondary, entity_batch.num_entities);
                        }
                    }

                    HYPERION_RETURN_OK;
                }
            );

            command_buffers_recorded_states[index] = 1u;
        }
    );

    const auto num_recorded_command_buffers = command_buffers_recorded_states.Sum();

    // submit all command buffers
    for (UInt i = 0; i < num_recorded_command_buffers; i++) {
        command_buffers[frame_index][/*(command_buffer_index + i) % static_cast<UInt>(command_buffers.Size())*/ i]
            ->SubmitSecondary(frame->GetCommandBuffer());
    }

    command_buffer_index = (command_buffer_index + num_recorded_command_buffers) % UInt(command_buffers.Size());
}

void RenderGroup::PerformRendering(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (m_draw_state.draw_calls.Empty()) {
        return;
    }

    RenderAll<false>(
        frame,
        m_command_buffers,
        m_command_buffer_index,
        m_pipeline.get(),
        &m_indirect_renderer,
        m_divided_draw_calls,
        m_draw_state,
        m_render_resources
    );
}

void RenderGroup::PerformRenderingIndirect(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);
    AssertReady();

    if (m_draw_state.draw_calls.Empty()) {
        return;
    }

    RenderAll<true>(
        frame,
        m_command_buffers,
        m_command_buffer_index,
        m_pipeline.get(),
        &m_indirect_renderer,
        m_divided_draw_calls,
        m_draw_state,
        m_render_resources
    );
}

void RenderGroup::Render(Frame *frame)
{
    // perform all ops in one batch
    CollectDrawCalls();
    PerformRendering(frame);
}

void RenderGroup::SetDrawProxies(const Array<EntityDrawProxy> &draw_proxies)
{
    Threads::AssertOnThread(THREAD_RENDER | THREAD_TASK);

    m_draw_proxies = draw_proxies;

    // UpdateDrawableLifetimes();
}

void RenderGroup::SetDrawProxies(Array<EntityDrawProxy> &&draw_proxies)
{
    Threads::AssertOnThread(THREAD_RENDER | THREAD_TASK);

    m_draw_proxies = std::move(draw_proxies);

    // UpdateDrawableLifetimes();
}

void RenderGroup::UpdateDrawableLifetimes()
{
    // TODO! Optimize : It eats at frame rate quite a bit

    RenderResourceManager previous_resources = std::move(m_render_resources);

    // prevent these objects from going out of scope while rendering is happening
    // register any used objects from the drawable / draw call here
    for (const EntityDrawProxy &draw_proxy : m_draw_proxies) {
        m_render_resources.SetIsUsed(
            draw_proxy.mesh_id,
            previous_resources.TakeResourceUsage(draw_proxy.mesh_id),
            true
        );

        m_render_resources.SetIsUsed(
            draw_proxy.material_id,
            previous_resources.TakeResourceUsage(draw_proxy.material_id),
            true
        );

        m_render_resources.SetIsUsed(
            draw_proxy.skeleton_id,
            previous_resources.TakeResourceUsage(draw_proxy.skeleton_id),
            true
        );
    }
}

// Proxied methods

CommandBuffer *RendererProxy::GetCommandBuffer(UInt frame_index)
{
    return m_render_group->m_command_buffers[frame_index].Front().Get();
}

renderer::GraphicsPipeline *RendererProxy::GetGraphicsPipeline()
{
    return m_render_group->m_pipeline.get();
}

void RendererProxy::Bind(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    CommandBuffer *command_buffer = m_render_group->m_command_buffers[frame->GetFrameIndex()].Front().Get();
    AssertThrow(command_buffer != nullptr);

    command_buffer->Begin(Engine::Get()->GetGPUDevice(), m_render_group->m_pipeline->GetConstructionInfo().render_pass);

    m_render_group->m_pipeline->Bind(command_buffer);
}

void RendererProxy::DrawMesh(Frame *frame, Mesh *mesh)
{
    CommandBuffer *command_buffer = m_render_group->m_command_buffers[frame->GetFrameIndex()].Front().Get();
    AssertThrow(command_buffer != nullptr);

    mesh->Render(command_buffer);
}

void RendererProxy::Submit(Frame *frame)
{
    CommandBuffer *command_buffer = m_render_group->m_command_buffers[frame->GetFrameIndex()].Front().Get();
    AssertThrow(command_buffer != nullptr);

    command_buffer->End(Engine::Get()->GetGPUDevice());
    command_buffer->SubmitSecondary(frame->GetCommandBuffer());
}

} // namespace hyperion::v2
