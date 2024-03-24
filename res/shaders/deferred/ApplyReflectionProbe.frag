#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord;

layout(location=0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/shared.inc"
#include "../include/defines.inc"
#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/brdf.inc"
#include "../include/tonemap.inc"
#include "../include/noise.inc"

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer, size = 512) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, ScenesBuffer, size = 256) readonly buffer ScenesBuffer
{
    Scene scene;
};

HYP_DESCRIPTOR_SRV(Global, GBufferTextures, count = 8) uniform texture2D gbuffer_textures[8];
HYP_DESCRIPTOR_SRV(Global, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(Global, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

#define HYP_DEFERRED_NO_RT_RADIANCE
#define HYP_DEFERRED_NO_SSR
#define HYP_DEFERRED_NO_ENV_GRID

#include "../include/env_probe.inc"
HYP_DESCRIPTOR_SRV(Scene, EnvProbeTextures, count = 16) uniform textureCube env_probe_textures[16];
HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer, size = 131072) readonly buffer EnvProbesBuffer { EnvProbe env_probes[HYP_MAX_ENV_PROBES]; };
HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, CurrentEnvProbe, size = 512) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

#include "./DeferredLighting.glsl"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

#define SAMPLE_COUNT 8

void main()
{
    vec3 irradiance = vec3(0.0);

    const float depth = Texture2D(sampler_nearest, gbuffer_depth_texture, v_texcoord).r;
    const vec3 N = normalize(DecodeNormal(Texture2D(sampler_nearest, gbuffer_normals_texture, v_texcoord)));
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), v_texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);
    const vec3 R = normalize(reflect(-V, N));

    const vec4 material = Texture2D(sampler_nearest, gbuffer_material_texture, v_texcoord); 
    const float roughness = material.r;

    const float lod = float(9.0) * roughness * (2.0 - roughness);

    vec4 ibl = vec4(0.0);

    const uint probe_texture_index = max(0, min(current_env_probe.texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1));
    
    uvec2 pixel_coord = uvec2(v_texcoord * vec2(camera.dimensions.xy) - 0.5);

    vec3 tangent;
    vec3 bitangent;
    ComputeOrthonormalBasis(N, tangent, bitangent);

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        vec2 rnd = Hammersley(uint(i), uint(SAMPLE_COUNT));

        vec3 H = ImportanceSampleGGX(rnd, N, roughness);
        H = tangent * H.x + bitangent * H.y + N * H.z;

        vec3 dir = normalize(2.0 * dot(V, H) * H - V);

        vec4 sample_ibl = vec4(0.0);
        ApplyReflectionProbe(current_env_probe, P, dir, lod, sample_ibl);
        ibl += sample_ibl * (1.0 / float(SAMPLE_COUNT));
    }

    color_output = ibl;
}