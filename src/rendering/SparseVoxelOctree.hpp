#ifndef HYPERION_V2_SVO_H
#define HYPERION_V2_SVO_H

#include "Base.hpp"
#include "Voxelizer.hpp"
#include "Compute.hpp"

namespace hyperion::v2 {

using renderer::IndirectBuffer;
using renderer::StorageBuffer;

class SparseVoxelOctree
    : public EngineComponentBase<STUB_CLASS(SparseVoxelOctree)>
{
    static constexpr SizeType min_nodes = 10000;
    static constexpr SizeType max_nodes = 10000000;

    using OctreeNode = UInt32[2];

public:
    SparseVoxelOctree();
    SparseVoxelOctree(const SparseVoxelOctree &other) = delete;
    SparseVoxelOctree &operator=(const SparseVoxelOctree &other) = delete;
    ~SparseVoxelOctree();

    Voxelizer *GetVoxelizer() const { return m_voxelizer.get(); }

    void Init(Engine *engine);
    void Build(Engine *engine);

private:
    SizeType CalculateNumNodes() const;
    void CreateBuffers(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void CreateComputePipelines(Engine *engine);

    void WriteMipmaps(Engine *engine);

    void BindDescriptorSets(
        Engine *engine,
        CommandBuffer *command_buffer,
        ComputePipeline *pipeline
    ) const;

    std::unique_ptr<Voxelizer> m_voxelizer;
    std::unique_ptr<AtomicCounter>  m_counter;

    std::unique_ptr<IndirectBuffer> m_indirect_buffer;
    std::unique_ptr<StorageBuffer> m_build_info_buffer;
    std::unique_ptr<StorageBuffer> m_octree_buffer;
    
    Handle<ComputePipeline> m_init_nodes;
    Handle<ComputePipeline> m_tag_nodes;
    Handle<ComputePipeline> m_alloc_nodes;
    Handle<ComputePipeline> m_modify_args;
    Handle<ComputePipeline> m_write_mipmaps;
};

} // namespace hyperion::v2

#endif