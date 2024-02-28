#ifndef HYPERION_V2_LIGHTMAP_RENDERER_HPP
#define HYPERION_V2_LIGHTMAP_RENDERER_HPP

#include <core/Base.hpp>
#include <core/lib/Queue.hpp>
#include <core/lib/Mutex.hpp>
#include <core/lib/AtomicVar.hpp>

#include <math/Triangle.hpp>
#include <math/Ray.hpp>

#include <scene/Scene.hpp>

#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <rendering/RenderGroup.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;

struct LightmapHitsBuffer;

struct LightmapRay
{
    Ray         ray;
    ID<Mesh>    mesh_id;
    uint        triangle_index;
    uint        texel_index;
};

class LightmapPathTracer
{
public:
    LightmapPathTracer(Handle<TLAS> tlas);
    LightmapPathTracer(const LightmapPathTracer &other)                 = delete;
    LightmapPathTracer &operator=(const LightmapPathTracer &other)      = delete;
    LightmapPathTracer(LightmapPathTracer &&other) noexcept             = delete;
    LightmapPathTracer &operator=(LightmapPathTracer &&other) noexcept  = delete;
    ~LightmapPathTracer();

    const GPUBufferRef &GetRaysBuffer() const
        { return m_rays_buffer; }

    const GPUBufferRef &GetHitsBuffer() const
        { return m_hits_buffer; }

    const RaytracingPipelineRef &GetPipeline() const
        { return m_raytracing_pipeline; }   
    
    Array<LightmapRay> &GetPreviousFrameRays()
        { return m_previous_frame_rays; }

    const Array<LightmapRay> &GetPreviousFrameRays() const
        { return m_previous_frame_rays; }

    void Create();
    
    void ReadHitsBuffer(LightmapHitsBuffer *ptr);
    void Trace(Frame *frame, const Array<LightmapRay> &rays, uint32 ray_offset);

private:
    void CreateUniformBuffer();
    void UpdateUniforms(Frame *frame, uint32 ray_offset);

    Handle<TLAS>            m_tlas;
    
    GPUBufferRef            m_uniform_buffer;
    GPUBufferRef            m_rays_buffer;
    GPUBufferRef            m_hits_buffer;
    GPUBufferRef            m_hits_staging_buffer;
    RaytracingPipelineRef   m_raytracing_pipeline;

    Array<LightmapRay>      m_previous_frame_rays;
};

class LightmapJob
{
public:
    static constexpr uint num_multisamples = 1;

    LightmapJob(Handle<Scene> scene);
    LightmapJob(const LightmapJob &other)                   = delete;
    LightmapJob &operator=(const LightmapJob &other)        = delete;
    LightmapJob(LightmapJob &&other) noexcept               = delete;
    LightmapJob &operator=(LightmapJob &&other) noexcept    = delete;
    ~LightmapJob()                                          = default;
    
    LightmapUVMap &GetUVMap()
        { return m_uv_map; }

    const LightmapUVMap &GetUVMap() const
        { return m_uv_map; }

    const Array<LightmapEntity> &GetEntities() const
        { return m_entities; }

    uint32 GetTexelIndex() const
        { return m_texel_index; }

    const Array<uint> &GetTexelIndices() const
        { return m_texel_indices; }

    void BuildUVMap();
    void GatherRays(Frame *frame, Array<LightmapRay> &out_rays);

    bool IsCompleted() const;
    bool IsReady() const
        { return m_is_ready.Get(MemoryOrder::RELAXED); }

private:
    Handle<Scene>                       m_scene;
    LightmapUVMap                       m_uv_map;
    Array<LightmapEntity>               m_entities;

    Array<uint>                         m_texel_indices; // flattened texel indices, flattened so that meshes are grouped together

    AtomicVar<bool>                     m_is_ready;
    uint                                m_texel_index;
};

class LightmapRenderer : public RenderComponent<LightmapRenderer>
{
public:
    LightmapRenderer(Name name);
    virtual ~LightmapRenderer() override = default;

    void AddJob(UniquePtr<LightmapJob> &&job)
    {
        Mutex::Guard guard(m_queue_mutex);

        m_queue.Push(std::move(job));

        m_num_jobs.Increment(1u, MemoryOrder::RELAXED);
    }

    void Init();
    void InitGame();

    void OnRemoved();
    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override
        { }

    UniquePtr<LightmapPathTracer>   m_path_tracer;

    Queue<UniquePtr<LightmapJob>>   m_queue;
    Mutex                           m_queue_mutex;
    AtomicVar<uint>                 m_num_jobs;
};

} // namespace hyperion::v2

#endif