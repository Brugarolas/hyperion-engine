/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Octree.hpp>
#include <scene/Entity.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/camera/Camera.hpp>

#include <Engine.hpp>

namespace hyperion {

const BoundingBox Octree::default_bounds = BoundingBox({ -250.0f }, { 250.0f });

// 0x80 For index bit because we reserve the highest bit for invalid octants
// 0xff for depth because +1 (used for child octant id) will cause it to overflow to 0
OctantID OctantID::Invalid()
{
    return OctantID(OctantID::invalid_bits, 0xff);
}

void OctreeState::MarkOctantDirty(OctantID octant_id)
{
    const OctantID prev_state = rebuild_state;

    if (octant_id.IsInvalid()) {
        return;
    }

    if (rebuild_state.IsInvalid()) {
        rebuild_state = octant_id;

        return;
    }

    while (octant_id != rebuild_state && !octant_id.IsChildOf(rebuild_state) && !rebuild_state.IsInvalid()) {
        rebuild_state = rebuild_state.GetParent();
    }

    // should always end up at root if it doesnt match any
    AssertThrow(rebuild_state != OctantID::Invalid());
}

Octree::Octree(RC<EntityManager> entity_manager)
    : Octree(std::move(entity_manager), default_bounds)
{
}

Octree::Octree(RC<EntityManager> entity_manager, const BoundingBox &aabb)
    : Octree(std::move(entity_manager), aabb, nullptr, 0)
{
    m_state = new OctreeState;
}

Octree::Octree(RC<EntityManager> entity_manager, const BoundingBox &aabb, Octree *parent, uint8 index)
    : m_entity_manager(std::move(entity_manager)),
      m_aabb(aabb),
      m_parent(nullptr),
      m_is_divided(false),
      m_state(nullptr),
      m_visibility_state(new VisibilityState { }),
      m_octant_id(index, OctantID::Invalid()),
      m_invalidation_marker(0)
{
    if (parent != nullptr) {
        SetParent(parent); // call explicitly to set root ptr
    }

    AssertThrow(m_octant_id.GetIndex() == index);

    InitOctants();
}

Octree::~Octree()
{
    Clear();

    if (IsRoot()) {
        delete m_state;
    }
}

void Octree::SetEntityManager(RC<EntityManager> entity_manager)
{
    m_entity_manager = std::move(entity_manager);

    if (IsDivided()) {
        for (Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->SetEntityManager(m_entity_manager);
        }
    }
}

void Octree::SetParent(Octree *parent)
{
    m_parent = parent;

    if (m_parent != nullptr) {
        m_state = m_parent->m_state;
    } else {
        m_state = nullptr;
    }

    m_octant_id = OctantID(m_octant_id.GetIndex(), parent != nullptr ? parent->m_octant_id : OctantID::Invalid());

    if (IsDivided()) {
        for (Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->SetParent(this);
        }
    }
}

bool Octree::EmptyDeep(int depth, uint8 octant_mask) const
{
    if (!Empty()) {
        return false;
    }

    if (!m_is_divided) {
        return true;
    }

    if (depth != 0) {
        return m_octants.Every([depth, octant_mask](const Octant &octant)
        {
            if (octant_mask & (1u << octant.octree->m_octant_id.GetIndex())) {
                return octant.octree->EmptyDeep(depth - 1);
            }

            return true;
        });
    }

    return true;
}

void Octree::InitOctants()
{
    const Vector3 divided_aabb_dimensions = m_aabb.GetExtent() / 2.0f;

    for (uint x = 0; x < 2; x++) {
        for (uint y = 0; y < 2; y++) {
            for (uint z = 0; z < 2; z++) {
                const uint index = 4 * x + 2 * y + z;

                m_octants[index] = {
                    .aabb = BoundingBox(
                        m_aabb.GetMin() + divided_aabb_dimensions * Vec3f(float(x), float(y), float(z)),
                        m_aabb.GetMin() + divided_aabb_dimensions * (Vec3f(float(x), float(y), float(z)) + Vec3f(1.0f))
                    )
                };
            }
        }
    }
}

Octree *Octree::GetChildOctant(OctantID octant_id)
{
    if (octant_id == OctantID::Invalid()) {
#if HYP_OCTREE_DEBUG
        DebugLog(
            LogType::Warn,
            "Invalid octant id %u:%u: Octant is invalid\n",
            octant_id.GetDepth(),
            octant_id.GetIndex()
        );
#endif

        return nullptr;
    }

    if (octant_id == m_octant_id) {
        return this;
    }

    if (octant_id.depth <= m_octant_id.depth) {
#if HYP_OCTREE_DEBUG
        DebugLog(
            LogType::Warn,
            "Octant id %u:%u is not a child of %u:%u: Octant is not deep enough\n",
            octant_id.GetDepth(),
            octant_id.GetIndex(),
            m_octant_id.GetDepth(),
            m_octant_id.GetIndex()
        );
#endif

        return nullptr;
    }

    Octree *current = this;

    for (uint depth = m_octant_id.depth + 1; depth <= octant_id.depth; depth++) {
        const uint8 index = octant_id.GetIndex(depth);

        if (!current || !current->IsDivided()) {
#if HYP_OCTREE_DEBUG
            DebugLog(
                LogType::Warn,
                "Octant id %u:%u is not a child of %u:%u: Octant %u:%u is not divided",
                octant_id.GetDepth(),
                octant_id.GetIndex(),
                m_octant_id.GetDepth(),
                m_octant_id.GetIndex(),
                current ? current->m_octant_id.GetDepth() : ~0u,
                current ? current->m_octant_id.GetIndex() : ~0u
            );
#endif

            return nullptr;
        }

        current = current->m_octants[index].octree.Get();
    }

    return current;
}

void Octree::Divide()
{
    AssertThrow(!IsDivided());

    for (uint i = 0; i < 8; ++i) {
        Octant &octant = m_octants[i];
        AssertThrow(octant.octree == nullptr);

        octant.octree.Reset(new Octree(m_entity_manager, octant.aabb, this, uint8(i)));
    }

    m_is_divided = true;
}

void Octree::Undivide()
{
    AssertThrow(IsDivided());
    AssertThrowMsg(m_nodes.Empty(), "Undivide() should be called on octrees with no remaining nodes");

    for (Octant &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->IsDivided()) {
            octant.octree->Undivide();
        }

        octant.octree.Reset();
    }

    m_is_divided = false;
}

void Octree::Invalidate()
{
    m_invalidation_marker++;

    if (IsDivided()) {
        for (Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->Invalidate();
        }
    }
}

void Octree::CollapseParents(bool allow_rebuild)
{
    m_state->MarkOctantDirty(m_octant_id);

    if (IsDivided() || !Empty()) {
        return;
    }

    Octree *iteration = m_parent,
        *highest_empty = nullptr;

    while (iteration != nullptr && iteration->Empty()) {
        for (const Octant &octant : iteration->m_octants) {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree.Get() == highest_empty) {
                /* Do not perform a check on our node, as we've already
                 * checked it by going up the chain.
                 * As `iter` becomes the parent of this node we're currently working with,
                 * we will not have to perform duplicate EmptyDeep() checks on any octants because of this check.
                 */
                continue;
            }

            if (!octant.octree->EmptyDeep()) {
                goto undivide;
            }
        }

        highest_empty = iteration;
        iteration = iteration->m_parent;
    }

undivide:
    if (highest_empty != nullptr) {
        if (allow_rebuild) {
            highest_empty->Undivide();
        } else {
            m_state->MarkOctantDirty(highest_empty->GetOctantID());
        }
    }
}

void Octree::Clear()
{
    Array<Node> nodes;
    Clear(nodes);

    RebuildNodesHash();
}

void Octree::Clear(Array<Node> &out_nodes)
{
    ClearInternal(out_nodes);

    if (IsDivided()) {
        Undivide();
    }
}

void Octree::ClearInternal(Array<Node> &out_nodes)
{
    out_nodes.Reserve(out_nodes.Size() + m_nodes.Size());

    // Push command to unset octant ID for all entities
    if (m_entity_manager) {
        m_entity_manager->PushCommand([nodes = m_nodes, octant_id = m_octant_id](EntityManager &mgr, GameCounter::TickUnit delta) mutable
        {
            for (auto &node : nodes) {
                if (mgr.HasEntity(node.id)) {
                    VisibilityStateComponent &visibility_state_component = mgr.GetComponent<VisibilityStateComponent>(node.id);
                    visibility_state_component.octant_id = OctantID::Invalid();
                    visibility_state_component.visibility_state = nullptr;
                }
            }
        });
    }

    for (auto &node : m_nodes) {
        if (m_state != nullptr) {
            auto it = m_state->node_to_octree.Find(node.id);

            if (it != m_state->node_to_octree.End()) {
                m_state->node_to_octree.Erase(it);
            }
        }

        out_nodes.PushBack(std::move(node));
    }

    m_nodes.Clear();

    if (!m_is_divided) {
        return;
    }

    for (Octant &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        octant.octree->ClearInternal(out_nodes);
    }
}

Octree::InsertResult Octree::Insert(ID<Entity> id, const BoundingBox &aabb, bool allow_rebuild)
{
    if (!aabb.IsValid()) {
        return {
            { Octree::Result::OCTREE_ERR, "AABB is in invalid state" },
            OctantID::Invalid()
        };
    }

    if (aabb.IsFinite()) {
        if (allow_rebuild) {
            if (!m_aabb.Contains(aabb)) {
                auto rebuild_result = RebuildExtendInternal(aabb);

                if (!rebuild_result.first) {
#ifdef HYP_OCTREE_DEBUG
                    DebugLog(
                        LogType::Warn,
                        "Failed to rebuild octree when inserting entity #%lu\n",
                        id.Value()
                    );
#endif

                    return rebuild_result;
                }
            }
        }

        // stop recursing if we are at max depth
        if (m_octant_id.GetDepth() < OctantID::max_depth - 1) {
            for (Octant &octant : m_octants) {
                if (octant.aabb.Contains(aabb)) {
                    if (!IsDivided()) {
                        if (allow_rebuild) {
                            Divide();
                        } else {
                            // do not use this octant if it has not been divided yet.
                            // instead, we'll insert into the current octant and mark it dirty
                            // so it will get added after the fact.
                            continue;
                        }
                    }

                    AssertThrow(octant.octree != nullptr);

                    return octant.octree->Insert(id, aabb, allow_rebuild);
                }
            }
        }
    }

    m_state->MarkOctantDirty(m_octant_id);

    return InsertInternal(id, aabb);
}

Octree::InsertResult Octree::InsertInternal(ID<Entity> id, const BoundingBox &aabb)
{
    m_nodes.PushBack(Node { id, aabb });

    if (m_state != nullptr) {
        AssertThrowMsg(m_state->node_to_octree.Find(id) == m_state->node_to_octree.End(), "Entity must not already be in octree hierarchy.");

        m_state->node_to_octree[id] = this;
    }

    // entity->OnAddedToOctree(this);

    if (m_entity_manager) {
        m_entity_manager->PushCommand([id, octant_id = m_octant_id](EntityManager &mgr, GameCounter::TickUnit delta)
        {
            if (mgr.HasEntity(id)) {
                if (!mgr.HasComponent<VisibilityStateComponent>(id)) {
                    mgr.AddComponent(id, VisibilityStateComponent {
                        .octant_id = octant_id,
                        .visibility_state = nullptr
                    });

#ifdef HYP_OCTREE_DEBUG
                    DebugLog(
                        LogType::Warn,
                        "Entity #%lu octant_id was not set, so it was set to %u:%u\n",
                        id.Value(),
                        octant_id.GetDepth(),
                        octant_id.GetIndex()
                    );
#endif
                } else {
                    VisibilityStateComponent &visibility_state_component = mgr.GetComponent<VisibilityStateComponent>(id);
                    visibility_state_component.octant_id = octant_id;
                    visibility_state_component.visibility_state = nullptr;

#ifdef HYP_OCTREE_DEBUG
                    DebugLog(
                        LogType::Debug,
                        "Entity #%lu octant_id was set to %u:%u\n",
                        id.Value(),
                        octant_id.GetDepth(),
                        octant_id.GetIndex()
                    );
#endif
                }
            }
        });   
    }

    return {
        { Octree::Result::OCTREE_OK },
        m_octant_id
    };
}

Octree::Result Octree::Remove(ID<Entity> id, bool allow_rebuild)
{
    if (m_state != nullptr) {
        const auto it = m_state->node_to_octree.Find(id);

        if (it == m_state->node_to_octree.End()) {
            return { Result::OCTREE_ERR, "Not found in node map" };
        }

        if (auto *octree = it->second) {
            return octree->RemoveInternal(id, allow_rebuild);
        }

        return { Result::OCTREE_ERR, "Could not be removed from any sub octants" };
    }

    return RemoveInternal(id, allow_rebuild);
}

Octree::Result Octree::RemoveInternal(ID<Entity> id, bool allow_rebuild)
{
    const auto it = FindNode(id);

    if (it == m_nodes.End()) {
        if (m_is_divided) {
            for (Octant &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->RemoveInternal(id, allow_rebuild)) {
                    return { };
                }
            }
        }

        return { Result::OCTREE_ERR, "Could not be removed from any sub octants and not found in this octant" };
    }

    if (m_state != nullptr) {
        m_state->node_to_octree.Erase(id);
    }

    if (m_entity_manager) {
        m_entity_manager->PushCommand([id](EntityManager &mgr, GameCounter::TickUnit delta)
        {
            if (mgr.HasEntity(id)) {
                VisibilityStateComponent &visibility_state_component = mgr.GetComponent<VisibilityStateComponent>(id);
                visibility_state_component.octant_id = OctantID::Invalid();
                visibility_state_component.visibility_state = nullptr;
            }
        });
    }

    m_nodes.Erase(it);

    m_state->MarkOctantDirty(m_octant_id);

    if (!m_is_divided && m_nodes.Empty()) {
        Octree *last_empty_parent = nullptr;

        if (Octree *parent = m_parent) {
            const Octree *child = this;

            while (parent->EmptyDeep(DEPTH_SEARCH_INF, 0xff & ~(1 << child->m_octant_id.GetIndex()))) { // do not search this branch of the tree again
                last_empty_parent = parent;

                if (parent->m_parent == nullptr) {
                    break;
                }

                child = parent;
                parent = child->m_parent;
            }
        }

        if (last_empty_parent != nullptr) {
            AssertThrow(last_empty_parent->EmptyDeep(DEPTH_SEARCH_INF));

            /* At highest empty parent octant, call Undivide() to collapse nodes */
            if (allow_rebuild) {
                last_empty_parent->Undivide();
            } else {
                m_state->MarkOctantDirty(last_empty_parent->GetOctantID());
            }
        }
    }

    return { };
}

Octree::InsertResult Octree::Move(ID<Entity> id, const BoundingBox &aabb, bool allow_rebuild, const Array<Node>::Iterator *it)
{
    const BoundingBox &new_aabb = aabb;

    const bool is_root = IsRoot();
    const bool contains = m_aabb.Contains(new_aabb);

    if (!contains) {
        // NO LONGER CONTAINS AABB

        if (is_root) {
            // have to rebuild, invalidating child octants.
            // which we have a Contains() check for child nodes walking upwards

#if HYP_OCTREE_DEBUG
            DebugLog(
                LogType::Debug,
                "In root, but does not contain node aabb, so rebuilding octree. %lu\n",
                id.Value()
            );
#endif

            if (allow_rebuild) {
                return RebuildExtendInternal(new_aabb);
            } else {
                m_state->MarkOctantDirty(m_octant_id);

                // Moved outside of the root octree, but we keep it here for now.
                // Next call of PerformUpdates(), we will extend the octree.
                return {
                    { Octree::Result::OCTREE_OK },
                    m_octant_id
                };
            }
        }

        // not root
#if HYP_OCTREE_DEBUG
        DebugLog(
            LogType::Debug,
            "Moving entity #%lu into the closest fitting (or root) parent\n",
            id.Value()
        );
#endif


        Optional<InsertResult> parent_insert_result;

        /* Contains is false at this point */
        Octree *parent = m_parent;
        Octree *last_parent = m_parent;

        while (parent != nullptr) {
            last_parent = parent;

            if (parent->m_aabb.Contains(new_aabb)) {
                if (it != nullptr) {
                    if (m_state != nullptr) {
                        m_state->node_to_octree.Erase(id);
                    }

                    m_nodes.Erase(*it);
                }

                parent_insert_result = parent->Move(id, aabb, allow_rebuild);

                break;
            }

            parent = parent->m_parent;
        }

        if (parent_insert_result.HasValue()) { // succesfully inserted, safe to call CollapseParents()
            // Node has now been added to it's appropriate octant which is a parent of this - 
            // collapse this up to the top
            CollapseParents(allow_rebuild);

            return parent_insert_result.Get();
        }

        // not inserted because no Move() was called on parents (because they don't contain AABB),
        // have to _manually_ call Move() which will go to the above branch for the parent octant,
        // this invalidating `this`

#if HYP_OCTREE_DEBUG
        DebugLog(
            LogType::Debug,
            "In child, no parents contain AABB so calling Move() on last valid octant (root). This will invalidate `this`.. %lu\n",
            id.Value()
        );
#endif

        AssertThrow(last_parent != nullptr);

        return last_parent->Move(id, aabb, allow_rebuild);
    }

    // CONTAINS AABB HERE

    if (allow_rebuild) {
        // Check if we can go deeper.
        for (Octant &octant : m_octants) {
            if (octant.aabb.Contains(new_aabb)) {
                if (it != nullptr) {
                    if (m_state != nullptr) {
                        m_state->node_to_octree.Erase(id);
                    }

                    m_nodes.Erase(*it);
                }
                
                if (!IsDivided()) {
                    if (allow_rebuild && m_octant_id.GetDepth() < OctantID::max_depth - 1) {
                        Divide();
                    } else {
                        continue;
                    }
                }
                
                AssertThrow(octant.octree != nullptr);

                const auto octant_move_result = octant.octree->Move(id, aabb, allow_rebuild);
                AssertThrow(octant_move_result.first);

                return octant_move_result;
            }
        }
    } else {
        m_state->MarkOctantDirty(m_octant_id);
    }

    if (it != nullptr) { /* Not moved out of this octant (for now) */
        auto &entity_it = *it;

        entity_it->aabb = new_aabb;
    } else { /* Moved into new octant */

        m_nodes.PushBack(Node { id, new_aabb });

        if (m_state != nullptr) {
            AssertThrowMsg(m_state->node_to_octree.Find(id) == m_state->node_to_octree.End(), "Entity must not already be in octree hierarchy.");

            m_state->node_to_octree[id] = this;
        }

        if (m_entity_manager) {
            m_entity_manager->PushCommand([id, octant_id = m_octant_id](EntityManager &mgr, GameCounter::TickUnit delta)
            {
                if (mgr.HasEntity(id)) {
                    VisibilityStateComponent &visibility_state_component = mgr.GetComponent<VisibilityStateComponent>(id);
                    visibility_state_component.octant_id = octant_id;
                    visibility_state_component.visibility_state = nullptr;
                }
            });

#ifdef HYP_OCTREE_DEBUG
            DebugLog(
                LogType::Debug,
                "Entity #%lu octant_id was moved to %u:%u\n",
                id.Value(),
                m_octant_id.GetDepth(),
                m_octant_id.GetIndex()
            );
#endif
        }
    }
    
    return {
        { Octree::Result::OCTREE_OK },
        m_octant_id
    };
}

Octree::InsertResult Octree::Update(ID<Entity> id, const BoundingBox &aabb, bool force_invalidation, bool allow_rebuild)
{
    if (m_state != nullptr) {
        const auto it = m_state->node_to_octree.Find(id);

        if (it == m_state->node_to_octree.End()) {
            return {
                { Result::OCTREE_ERR, "Object not found in node map!" },
                OctantID::Invalid()
            };
        }

        if (Octree *octree = it->second) {
            return octree->UpdateInternal(id, aabb, force_invalidation, allow_rebuild);
        }

        return {
            { Result::OCTREE_ERR, "Object has no octree in node map!" },
            OctantID::Invalid()
        };
    }

    return UpdateInternal(id, aabb, force_invalidation, allow_rebuild);
}

Octree::InsertResult Octree::UpdateInternal(ID<Entity> id, const BoundingBox &aabb, bool force_invalidation, bool allow_rebuild)
{
    const auto it = FindNode(id);

    if (it == m_nodes.End()) {
        if (m_is_divided) {
            for (Octant &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                auto update_internal_result = octant.octree->UpdateInternal(id, aabb, force_invalidation, allow_rebuild);

                if (update_internal_result.first) {
                    return update_internal_result;
                }
            }
        }

        return {
            { Result::OCTREE_ERR, "Could not update in any sub octants" },
            OctantID::Invalid()
        };
    }

    if (force_invalidation) {
        DebugLog(
            LogType::Debug,
            "Forcing invalidation of octant entity #%lu\n",
            id.Value()
        );

        // force invalidation of this entry so the octant's hash will be updated
        Invalidate();
    }

    const BoundingBox &new_aabb = aabb;
    const BoundingBox &old_aabb = it->aabb;

    if (new_aabb == old_aabb) {
        if (force_invalidation) {
            // force invalidation of this entry so the octant's hash will be updated
            m_state->MarkOctantDirty(m_octant_id);
        }

        /* AABB has not changed - no need to update */
        return {
            { Result::OCTREE_OK },
            m_octant_id
        };
    }

    /* AABB has changed to we remove it from this octree and either:
     * If we don't contain it anymore - insert it from the highest level octree that still contains the aabb and then walking down from there
     * If we do still contain it - we will remove it from this octree and re-insert it to find the deepest child octant
     */

    return Move(id, new_aabb, allow_rebuild, &it);
}

Octree::InsertResult Octree::Rebuild()
{
#ifdef HYP_OCTREE_DEBUG
    DebugLog(LogType::Debug, "Rebuild octant (Index: %u, Depth: %u)\n", m_octant_id.GetIndex(), m_octant_id.GetDepth());
#endif

    Array<Node> new_nodes;
    Clear(new_nodes);

    BoundingBox prev_aabb = m_aabb;

    if (IsRoot()) {
        m_aabb = BoundingBox::Empty();
    }

    for (auto &it : new_nodes) {
        if (it.aabb.IsFinite()) {
            if (IsRoot()) {
                m_aabb.Extend(it.aabb);
            } else {
                AssertThrow(m_aabb.Contains(it.aabb));
            }
        }

        auto insert_result = Insert(it.id, it.aabb, true /* allow rebuild */);

        if (!insert_result.first) {
            return insert_result;
        }
    }

    return {
        { Octree::Result::OCTREE_OK },
        m_octant_id
    };
}

Octree::InsertResult Octree::Rebuild(const BoundingBox &new_aabb)
{
    Array<Node> new_nodes;
    Clear(new_nodes);

    m_aabb = new_aabb;

    for (auto &it : new_nodes) {
        auto insert_result = Insert(it.id, it.aabb, true /* allow rebuild */);

        if (!insert_result.first) {
            return insert_result;
        }
    }

    return {
        { Octree::Result::OCTREE_OK },
        m_octant_id
    };
}

Octree::InsertResult Octree::RebuildExtendInternal(const BoundingBox &extend_include_aabb)
{
    if (!extend_include_aabb.IsValid()) {
        return {
            { Octree::Result::OCTREE_ERR, "AABB is in invalid state" },
            OctantID::Invalid()
        };
    }

    if (!extend_include_aabb.IsFinite()) {
        return {
            { Octree::Result::OCTREE_ERR, "AABB is not finite" },
            OctantID::Invalid()
        };
    }

    // have to grow the aabb by rebuilding the octree
    BoundingBox new_aabb(m_aabb);
    // extend the new aabb to include the entity
    new_aabb.Extend(extend_include_aabb);
    // grow our new aabb by a predetermined growth factor,
    // to keep it from constantly resizing
    new_aabb *= growth_factor;

    return Rebuild(new_aabb);
}

void Octree::PerformUpdates()
{
    AssertThrow(m_state != nullptr);

    if (m_state->rebuild_state == OctantID::Invalid()) {
        return;
    }

    Octree *octant = GetChildOctant(m_state->rebuild_state);
    AssertThrow(octant != nullptr);

    const auto rebuild_result = octant->Rebuild();

    RebuildNodesHash();

    if (!rebuild_result.first) {
        DebugLog(
            LogType::Warn,
            "Failed to rebuild octree when performing updates: %s\n",
            rebuild_result.first.message
        );
    } else {
        // set rebuild state to invalid if rebuild was successful
        m_state->rebuild_state = OctantID::Invalid();
    }
}

void Octree::CollectEntities(Array<ID<Entity>> &out) const
{
    out.Reserve(out.Size() + m_nodes.Size());

    for (auto &node : m_nodes) {
        out.PushBack(node.id);
    }

    if (m_is_divided) {
        for (const Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->CollectEntities(out);
        }
    }
}

void Octree::CollectEntitiesInRange(const Vector3 &position, float radius, Array<ID<Entity>> &out) const
{
    const BoundingBox inclusion_aabb(position - radius, position + radius);

    if (!inclusion_aabb.Intersects(m_aabb)) {
        return;
    }

    out.Reserve(out.Size() + m_nodes.Size());

    for (const Octree::Node &node : m_nodes) {
        if (inclusion_aabb.Intersects(node.aabb)) {
            out.PushBack(node.id);
        }
    }

    if (m_is_divided) {
        for (const Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->CollectEntitiesInRange(position, radius, out);
        }
    }
}

bool Octree::GetNearestOctants(const Vector3 &position, FixedArray<Octree *, 8> &out) const
{
    if (!m_aabb.ContainsPoint(position)) {
        return false;
    }

    if (!m_is_divided) {
        return false;
    }

    for (const Octant &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->GetNearestOctants(position, out)) {
            return true;
        }
    }

    for (SizeType i = 0; i < m_octants.Size(); i++) {
        out[i] = m_octants[i].octree.Get();
    }

    return true;
}

bool Octree::GetNearestOctant(const Vector3 &position, Octree const *&out) const
{
    if (!m_aabb.ContainsPoint(position)) {
        return false;
    }

    if (m_is_divided) {
        for (const Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree->GetNearestOctant(position, out)) {
                return true;
            }
        }
    }

    out = this;

    return true;
}

bool Octree::GetFittingOctant(const BoundingBox &aabb, Octree const *&out) const
{
    if (!m_aabb.Contains(aabb)) {
        return false;
    }

    if (m_is_divided) {
        for (const Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            if (octant.octree->GetFittingOctant(aabb, out)) {
                return true;
            }
        }
    }

    out = this;

    return true;
}

void Octree::NextVisibilityState()
{
    m_visibility_state->Next();
}

void Octree::CalculateVisibility(const Handle<Camera> &camera)
{
    if (!camera.IsValid()) {
        return;
    }

    const Frustum &frustum = camera->GetFrustum();

    if (frustum.ContainsAABB(m_aabb)) {
        UpdateVisibilityState(camera, m_visibility_state->GetSnapshot(camera.GetID()).validity_marker);
    } else {
        DebugLog(LogType::Debug,
            "Camera frustum for camera #%lu does not contain octree aabb [%f, %f, %f] - [%f, %f, %f].\n",
            camera->GetID().Value(),
            m_aabb.GetMin().x, m_aabb.GetMin().y, m_aabb.GetMin().z,
            m_aabb.GetMax().x, m_aabb.GetMax().y, m_aabb.GetMax().z);
    }
}

void Octree::UpdateVisibilityState(const Handle<Camera> &camera, uint16 validity_marker)
{
    /* assume we are already visible from CalculateVisibility() check */
    const Frustum &frustum = camera->GetFrustum();

    auto &snapshot = m_visibility_state->GetSnapshot(camera.GetID());
    snapshot.validity_marker = validity_marker;

    if (m_is_divided) {
        for (Octant &octant : m_octants) {
            if (!frustum.ContainsAABB(octant.aabb)) {
                continue;
            }

            octant.octree->UpdateVisibilityState(camera, validity_marker);
        }
    }
}

void Octree::ResetNodesHash()
{
    m_entry_hashes = { };
}

void Octree::RebuildNodesHash(uint level)
{
    ResetNodesHash();

    for (const Node &item : m_nodes) {
        const HashCode entry_hash_code = item.GetHashCode();
        m_entry_hashes[0].Add(entry_hash_code);

        Array<EntityTag> tags;
        
        if (m_entity_manager) {
            tags = m_entity_manager->GetTags(item.id);
        }

        for (uint i = 0; i < uint(tags.Size()); i++) {
            const uint num_combinations = (1u << i);

            for (uint k = 0; k < num_combinations; k++) {
                uint32 mask = (1u << (uint32(tags[i]) - 1));

                for (uint j = 0; j < i; j++) {
                    if ((k & (1u << j)) != (1u << j)) {
                        continue;
                    }
                    
                    mask |= (1u << (uint32(tags[j]) - 1));
                }

                AssertThrow(m_entry_hashes.Size() > mask);

                m_entry_hashes[mask].Add(entry_hash_code);
            }
        }
        
    }

    if (m_is_divided) {
        for (const Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->RebuildNodesHash(level + 1);
        }
    }

    // Update parent to include this
    if (m_parent) {
        for (uint i = 0; i < uint(m_entry_hashes.Size()); i++) {
            m_parent->m_entry_hashes[i].Add(m_entry_hashes[i]);
        }
    }
}

bool Octree::TestRay(const Ray &ray, RayTestResults &out_results) const
{
    bool has_hit = false;

    if (ray.TestAABB(m_aabb)) {
        for (const Octree::Node &node : m_nodes) {
            // if (!node.entity->HasFlags(Entity::InitInfo::ENTITY_FLAGS_RAY_TESTS_ENABLED)) {
            //     continue;
            // }

            // if (!BucketRayTestsEnabled(node.entity->GetRenderableAttributes().GetMaterialAttributes().bucket)) {
            //     continue;
            // }

            if (ray.TestAABB(
                node.aabb,
                node.id.Value(),
                nullptr,
                out_results
            )) {
                has_hit = true;
            }
        }
        
        if (m_is_divided) {
            for (const Octant &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                if (octant.octree->TestRay(ray, out_results)) {
                    has_hit = true;
                }
            }
        }
    }

    return has_hit;
}

} // namespace hyperion