#ifndef HYPERION_V2_ENGINE_H
#define HYPERION_V2_ENGINE_H

#include <Config.hpp>

#include <asset/Assets.hpp>

#include <rendering/PostFX.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/DeferredSystem.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/DefaultFormats.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RenderCommand.hpp>
#include <rendering/debug/DebugDrawer.hpp>
#include <rendering/Material.hpp>
#include <rendering/FinalPass.hpp>
#include <scene/World.hpp>

#include <Threads.hpp>
#include <TaskSystem.hpp>

#include <core/lib/FlatMap.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/ObjectPool.hpp>

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <system/CrashHandler.hpp>

#include <util/shader_compiler/ShaderCompiler.hpp>

#include <Types.hpp>

#include <mutex>

namespace hyperion::v2 {

using renderer::Instance;
using renderer::Device;
using renderer::Semaphore;
using renderer::SemaphoreChain;
using renderer::Image;

class Engine;
class Framebuffer;
class Game;
class GameThread;

extern Engine               *g_engine;
extern AssetManager         *g_asset_manager;
extern ShaderManagerSystem  *g_shader_manager;
extern MaterialCache        *g_material_system;
extern SafeDeleter          *g_safe_deleter;

struct DebugMarker
{
    CommandBuffer *command_buffer = nullptr;
    const char * const name = "<Unnamed debug marker>";
    bool is_ended = false;

    DebugMarker(CommandBuffer *command_buffer, const char *marker_name)
        : command_buffer(command_buffer),
          name(marker_name)
    {
        if (command_buffer != nullptr) {
            command_buffer->DebugMarkerBegin(name);
        }
    }

    DebugMarker(const DebugMarker &other) = delete;
    DebugMarker &operator=(const DebugMarker &other) = delete;

    DebugMarker(DebugMarker &&other) noexcept = delete;
    DebugMarker &operator=(DebugMarker &&other) noexcept = delete;

    ~DebugMarker()
    {
        MarkEnd();
    }

    void MarkEnd()
    {
        if (is_ended) {
            return;
        }

        if (command_buffer != nullptr) {
            command_buffer->DebugMarkerEnd();
        }

        is_ended = true;
    }
};

class GlobalDescriptorSetManager
{
public:
    GlobalDescriptorSetManager(Engine *engine);
    GlobalDescriptorSetManager(const GlobalDescriptorSetManager &)                  = delete;
    GlobalDescriptorSetManager &operator=(const GlobalDescriptorSetManager &)       = delete;
    GlobalDescriptorSetManager(GlobalDescriptorSetManager &&) noexcept              = delete;
    GlobalDescriptorSetManager &operator=(GlobalDescriptorSetManager &&) noexcept   = delete;
    ~GlobalDescriptorSetManager();

    void Initialize(Engine *engine);

    void AddDescriptorSet(Name name, const DescriptorSet2Ref &ref);
    DescriptorSet2Ref GetDescriptorSet(Name name) const;

private:
    HashMap<Name, DescriptorSet2Ref>    m_descriptor_sets;
    mutable Mutex                       m_mutex;
};

class Engine
{
#ifdef HYP_DEBUG_MODE
    static constexpr bool use_debug_layers = true;
#else
    static constexpr bool use_debug_layers = false;
#endif

public:
    HYP_FORCE_INLINE static Engine *Get()
        { return g_engine; }

    Engine();
    ~Engine();

    bool InitializeGame(Game *game);

    const RC<Application> &GetApplication() const
        { return m_application; }

    HYP_FORCE_INLINE
    Instance *GetGPUInstance() const
        { return m_instance.Get(); }

    HYP_FORCE_INLINE
    Device *GetGPUDevice() const
        { return m_instance ? m_instance->GetDevice() : nullptr; }
    
    HYP_FORCE_INLINE
    DeferredRenderer &GetDeferredRenderer()
        { return m_deferred_renderer; }
    
    HYP_FORCE_INLINE
    const DeferredRenderer &GetDeferredRenderer() const
        { return m_deferred_renderer; }
    
    HYP_FORCE_INLINE
    DeferredSystem &GetDeferredSystem()
        { return m_render_list_container; }
    
    HYP_FORCE_INLINE
    const DeferredSystem &GetDeferredSystem() const
        { return m_render_list_container; }
    
    HYP_FORCE_INLINE
    RenderState &GetRenderState()
        { return render_state; }

    HYP_FORCE_INLINE
    const RenderState &GetRenderState() const
        { return render_state; }
    
    HYP_FORCE_INLINE
    ShaderGlobals *GetRenderData() const
        { return m_render_data.Get(); }
    
    HYP_FORCE_INLINE
    PlaceholderData *GetPlaceholderData() const
        { return m_placeholder_data.Get(); }
    
    HYP_FORCE_INLINE
    ObjectPool &GetObjectPool()
        { return m_object_pool; }
    
    HYP_FORCE_INLINE
    const ObjectPool &GetObjectPool() const
        { return m_object_pool; }
    
    HYP_FORCE_INLINE
    const Handle<World> &GetWorld() const
        { return m_world; }
    
    HYP_FORCE_INLINE
    Configuration &GetConfig()
        { return m_configuration; }
    
    HYP_FORCE_INLINE
    const Configuration &GetConfig() const
        { return m_configuration; }
    
    HYP_FORCE_INLINE
    ShaderCompiler &GetShaderCompiler()
        { return m_shader_compiler; }
    
    HYP_FORCE_INLINE
    const ShaderCompiler &GetShaderCompiler() const
        { return m_shader_compiler; }
    
    HYP_FORCE_INLINE
    DebugDrawer &GetDebugDrawer()
        { return m_debug_drawer; }
    
    HYP_FORCE_INLINE
    const DebugDrawer &GetDebugDrawer() const
        { return m_debug_drawer; }
    
    HYP_FORCE_INLINE
    InternalFormat GetDefaultFormat(TextureFormatDefault type) const
        { return m_texture_format_defaults.At(type); }
    
    HYP_FORCE_INLINE
    const FinalPass &GetFinalPass() const
        { return m_final_pass; }
    
    HYP_FORCE_INLINE
    const DescriptorTableRef &GetGlobalDescriptorTable() const
        { return m_global_descriptor_table; }
    
    HYP_FORCE_INLINE
    MaterialDescriptorSetManager &GetMaterialDescriptorSetManager()
        { return m_material_descriptor_set_manager; }
    
    HYP_FORCE_INLINE
    const MaterialDescriptorSetManager &GetMaterialDescriptorSetManager() const
        { return m_material_descriptor_set_manager; }

    Handle<RenderGroup> CreateRenderGroup(
        const RenderableAttributeSet &renderable_attributes
    );
    
    /*! \brief Create a RenderGroup using defined set of DescriptorSets. The result will not be cached. */
    Handle<RenderGroup> CreateRenderGroup(
        const Handle<Shader> &shader,
        const RenderableAttributeSet &renderable_attributes,
        DescriptorTableRef descriptor_table
    );

    // /*! \brief Create a RenderGroup, using implied DescriptorSets from the Shader's CompiledShaderBatch Definition */
    // Handle<RenderGroup> CreateRenderGroup(
    //     const Handle<Shader> &shader,
    //     const RenderableAttributeSet &renderable_attributes
    // );

    void AddRenderGroup(Handle<RenderGroup> &render_group);

    bool IsRenderLoopActive() const
        { return m_is_render_loop_active; }

    void Initialize(RC<Application> application);
    void Compile();
    void RequestStop();

    void PreFrameUpdate(Frame *frame);
    void RenderDeferred(Frame *frame);

    void RenderNextFrame(Game *game);

    ShaderCompiler m_shader_compiler;

    RenderState render_state;

    AtomicVar<bool> m_stop_requested;

    UniquePtr<GameThread> game_thread;

    template <class T, class ... Args>
    Handle<T> CreateObject(Args &&... args)
    {
        auto &container = GetObjectPool().GetContainer<T>();

        const uint index = container.NextIndex();

        container.ConstructAtIndex(
            index,
            std::forward<Args>(args)...
        );

        return Handle<T>(ID<T>::FromIndex(index));
    }

    template <class T>
    Handle<T> CreateObject()
    {
        auto &container = GetObjectPool().GetContainer<T>();

        const uint index = container.NextIndex();
        container.ConstructAtIndex(index);

        return Handle<T>(ID<T>::FromIndex(index));
    }

    template <class T>
    bool InitObject(Handle<T> &handle)
    {
        if (!handle) {
            return false;
        }

        if (!handle->GetID()) {
            return false;
        }

        handle->Init();

        return true;
    }

    void FinalizeStop();

private:
    void ResetRenderState(RenderStateMask mask);
    void UpdateBuffersAndDescriptors(Frame *frame);

    void FindTextureFormatDefaults();

    void AddRenderGroupInternal(Handle<RenderGroup> &, bool cache);

    RC<Application>                                         m_application;
    
    UniquePtr<Instance>                                     m_instance;

    UniquePtr<PlaceholderData>                              m_placeholder_data;

    DescriptorTableRef                                      m_global_descriptor_table;

    MaterialDescriptorSetManager                            m_material_descriptor_set_manager;

    HashMap<TextureFormatDefault, InternalFormat>           m_texture_format_defaults;

    DeferredRenderer                                        m_deferred_renderer;
    DeferredSystem                                          m_render_list_container;
    FlatMap<RenderableAttributeSet, Handle<RenderGroup>>    m_render_group_mapping;
    std::mutex                                              m_render_group_mapping_mutex;

    UniquePtr<ShaderGlobals>                                m_render_data;

    ObjectPool                                              m_object_pool;

    Handle<World>                                           m_world;
    
    Configuration                                           m_configuration;

    DebugDrawer                                             m_debug_drawer;

    FinalPass                                               m_final_pass;

    CrashHandler                                            m_crash_handler;

    bool                                                    m_is_stopping { false };
    bool                                                    m_is_render_loop_active { false };
};

} // namespace hyperion::v2

#endif

