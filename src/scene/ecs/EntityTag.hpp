/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_TAG_HPP
#define HYPERION_ECS_ENTITY_TAG_HPP

#include <Types.hpp>

namespace hyperion {

enum class EntityTag : uint32
{
    NONE,
    STATIC,
    DYNAMIC,
    LIGHT,

    MAX
};

/*! \brief An EntityTag is a special component that is used to tag an entity with a specific flag.
 * 
 *  \tparam tag The flag value
 */
template <EntityTag tag>
struct EntityTagComponent
{
    static constexpr EntityTag value = tag;
};

} // namespace hyperion

#endif