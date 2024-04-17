/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionEngine.hpp>
#include <dotnet/DotNetSystem.hpp>

namespace hyperion {

HYP_API void InitializeApplication(RC<Application> application)
{
    Threads::AssertOnThread(ThreadName::THREAD_MAIN);

    AssertThrowMsg(
        g_engine == nullptr,
        "Hyperion already initialized!"
    );

    g_engine = new Engine;
    g_asset_manager = new AssetManager;
    g_shader_manager = new ShaderManagerSystem;
    g_material_system = new MaterialCache;
    g_safe_deleter = new SafeDeleter;

    g_engine->Initialize(std::move(application));

    dotnet::DotNetSystem::GetInstance().Initialize();
}

HYP_API void ShutdownApplication()
{
    Threads::AssertOnThread(ThreadName::THREAD_MAIN);

    AssertThrowMsg(
        g_engine != nullptr,
        "Hyperion not initialized!"
    );

    g_engine->FinalizeStop();

    dotnet::DotNetSystem::GetInstance().Shutdown();

    delete g_asset_manager;
    g_asset_manager = nullptr;

    delete g_shader_manager;
    g_shader_manager = nullptr;

    delete g_material_system;
    g_material_system = nullptr;

    delete g_engine;
    g_engine = nullptr;
}

} // namespace hyperion