#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=3) in flat uint v_object_index;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_material;
layout(location=3) out vec4 gbuffer_tangents;
layout(location=4) out vec4 gbuffer_mask;

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler texture_sampler;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "include/gbuffer.inc"
#include "include/object.inc"
#include "include/material.inc"
#include "include/packing.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer, size = 33554432) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};

#ifdef HYP_DESCRIPTOR_INDEX_SAMPLER
HYP_DESCRIPTOR_SRV(Material, Textures, count = 16) uniform texture2D textures[HYP_MAX_BOUND_TEXTURES];
#if defined(HYP_MATERIAL_CUBEMAP_TEXTURES) && HYP_MATERIAL_CUBEMAP_TEXTURES
HYP_DESCRIPTOR_SRV(Material, Textures, count = 16) uniform textureCube cubemap_textures[HYP_MAX_BOUND_TEXTURES];
#endif
#else
// layout(set = HYP_DESCRIPTOR_SET_TEXTURES, binding = HYP_DESCRIPTOR_INDEX_TEXTURES_ARRAY) uniform sampler2D textures[HYP_MAX_BOUND_TEXTURES];
// #if defined(HYP_MATERIAL_CUBEMAP_TEXTURES) && HYP_MATERIAL_CUBEMAP_TEXTURES
//     layout(set = HYP_DESCRIPTOR_SET_TEXTURES, binding = HYP_DESCRIPTOR_INDEX_TEXTURES_ARRAY) uniform samplerCube cubemap_textures[HYP_MAX_BOUND_TEXTURES];
// #endif
#endif

// #ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
HYP_DESCRIPTOR_SSBO(Object, MaterialsBuffer, size = 8388608) readonly buffer MaterialsBuffer
{
    Material materials[HYP_MAX_MATERIALS];
};

#ifndef CURRENT_MATERIAL
    #define CURRENT_MATERIAL (materials[object.material_index])
#endif
// #else

// HYP_DESCRIPTOR_SSBO_DYNAMIC(Object, MaterialsBuffer, size = 128) readonly buffer MaterialsBuffer
// {
//     Material material;
// };

// #ifndef CURRENT_MATERIAL
//     #define CURRENT_MATERIAL material
// #endif
// #endif

void main()
{
    vec3 normal = normalize(v_normal);

//#if defined(HYP_MATERIAL_CUBEMAP_TEXTURES) && HYP_MATERIAL_CUBEMAP_TEXTURES
    gbuffer_albedo = vec4(SAMPLE_TEXTURE_CUBE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map, v_position).rgb, 0.0 /* just for now to tell deferred to not perform lighting */);
//#else
 //   gbuffer_albedo = vec4(0.0);
//#endif

    gbuffer_normals = EncodeNormal(normal);
    gbuffer_material = vec4(0.0, 0.0, 0.0, 1.0);
    gbuffer_tangents = vec4(0.0);  // not needed
    gbuffer_mask = UINT_TO_VEC4(GET_OBJECT_BUCKET(object));
}
