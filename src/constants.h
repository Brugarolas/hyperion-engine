#ifndef HYPERION_CONSTANTS_H
#define HYPERION_CONSTANTS_H

#include <types.h>

namespace hyperion {

constexpr UInt max_frames_in_flight = 2;

constexpr UInt num_gbuffer_textures = 7;

template <class ...T>
constexpr bool resolution_failure = false;

} // namespace hyperion

#endif
