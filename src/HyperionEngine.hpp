#ifndef HYPERION_ENGINE_HPP
#define HYPERION_ENGINE_HPP

#include <Engine.hpp>
#include <system/Application.hpp>
#include <util/Defines.hpp>

namespace hyperion {

using namespace v2;

HYP_API void InitializeApplication(RC<Application> application);
HYP_API void ShutdownApplication();

} // namespace hyperion

#endif