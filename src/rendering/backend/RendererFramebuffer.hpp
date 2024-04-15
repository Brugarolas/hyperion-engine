/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BACKEND_RENDERER_FBO_H
#define HYPERION_V2_BACKEND_RENDERER_FBO_H

#include <rendering/backend/Platform.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class FramebufferObject
{
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFramebuffer.hpp>
#else
#error Unsupported rendering backend
#endif


namespace hyperion {
namespace renderer {

using FramebufferObject = platform::FramebufferObject<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif