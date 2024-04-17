/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_HELPERS_HPP
#define HYPERION_BACKEND_RENDERER_HELPERS_HPP

#include <core/Defines.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace helpers {

uint MipmapSize(uint src_size, int lod);

} // namespace helpers
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererHelpers.hpp>
#else
#error Unsupported rendering backend
#endif

#endif