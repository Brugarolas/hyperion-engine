#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 output_color;
layout(location=1) out vec4 output_normals;
layout(location=2) out vec4 output_positions;

// layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 12) uniform texture2D ssr_uvs;
// layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 13) uniform texture2D ssr_sample;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 21) uniform texture2D ssr_blur_vert;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 25) uniform textureCube rendered_cubemaps[];

#include "include/gbuffer.inc"
#include "include/material.inc"
#include "include/post_fx.inc"
#include "include/tonemap.inc"

#include "include/scene.inc"
#include "include/brdf.inc"

vec2 texcoord = v_texcoord0;//vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);


#define HYP_VCT_ENABLED 1
#define HYP_SSR_ENABLED 0

#if HYP_VCT_ENABLED
#include "include/vct/cone_trace.inc"
#endif

#include "include/shadows.inc"

/* Begin main shader program */

#define IBL_INTENSITY 10000.0
#define DIRECTIONAL_LIGHT_INTENSITY 100000.0
#define IRRADIANCE_MULTIPLIER 1.0
#define ROUGHNESS_LOD_MULTIPLIER 12.0
#define SSAO_DEBUG 0

#include "include/rt/probe/shared.inc"

#if 0

vec4 SampleIrradiance(vec3 P, vec3 N, vec3 V)
{
    const uvec3 base_grid_coord    = BaseGridCoord(P);
    const vec3 base_probe_position = GridPositionToWorldPosition(base_grid_coord);
    
    vec3 total_irradiance = vec3(0.0);
    float total_weight    = 0.0;
    
    vec3 alpha = clamp((P - base_probe_position) / PROBE_GRID_STEP, vec3(0.0), vec3(1.0));
    
    for (uint i = 0; i < 8; i++) {
        uvec3 offset = uvec3(i, i >> 1, i >> 2) & uvec3(1);
        uvec3 probe_grid_coord = clamp(base_grid_coord + offset, uvec3(0.0), probe_system.probe_counts.xyz - uvec3(1));
        
        uint probe_index    = GridPositionToProbeIndex(probe_grid_coord);
        vec3 probe_position = GridPositionToWorldPosition(probe_grid_coord);
        vec3 probe_to_point = P - probe_position + (N + 3.0 * V) * PROBE_NORMAL_BIAS;
        vec3 dir            = normalize(-probe_to_point);
        
        vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));
        float weight   = 1.0;
        
        /* Backface test */
        
        vec3 true_direction_to_probe = normalize(probe_position - P);
        weight *= SQR(max(0.0001, (dot(true_direction_to_probe, N) + 1.0) * 0.5)) + 0.2;
        
        /* Visibility test */
        /* TODO */
        
        weight = max(0.000001, weight);
        
        vec3 irradiance_direction = N;
        
    }
}

#endif


void main()
{
    vec4 albedo    = SampleGBuffer(gbuffer_albedo_texture, texcoord);
    vec4 normal    = vec4(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, texcoord)), 1.0);
    vec4 tangent   = vec4(DecodeNormal(SampleGBuffer(gbuffer_tangents_texture, texcoord)), 1.0);
    vec4 bitangent = vec4(DecodeNormal(SampleGBuffer(gbuffer_bitangents_texture, texcoord)), 1.0);
    vec4 position  = SampleGBuffer(gbuffer_positions_texture, texcoord);
    vec4 material  = SampleGBuffer(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */
    
    bool perform_lighting = albedo.a > 0.0;
    
    vec3 albedo_linear = albedo.rgb;
	vec3 result = vec3(0.0);

    /* Physical camera */
    float aperture = 16.0;
    float shutter = 1.0/125.0;
    float sensitivity = 100.0;
    float ev100 = log2((aperture * aperture) / shutter * 100.0f / sensitivity);
    float exposure = 1.0f / (1.2f * pow(2.0f, ev100));
    
    vec3 N = normalize(normal.xyz);
    vec3 T = normalize(tangent.xyz);
    vec3 B = normalize(bitangent.xyz);
    vec3 L = normalize(scene.light_direction.xyz);
    vec3 V = normalize(scene.camera_position.xyz - position.xyz);
    vec3 R = normalize(reflect(-V, N));
    vec3 H = normalize(L + V);
    
    float ao = 1.0;
    
    if (perform_lighting) {
        ao = SampleEffectPre(0, v_texcoord0, vec4(1.0)).r * material.a;

        const float roughness = material.r;
        const float metalness = material.g;
        
        float NdotL = max(0.0001, dot(N, L));
        float NdotV = max(0.0001, dot(N, V));
        float NdotH = max(0.0001, dot(N, H));
        float LdotH = max(0.0001, dot(L, H));
        float VdotH = max(0.0001, dot(V, H));
        float HdotV = max(0.0001, dot(H, V));

        float shadow = 1.0;

        if (scene.num_environment_shadow_maps != 0) {

#if HYP_SHADOW_PENUMBRA
            shadow = GetShadowContactHardened(0, position.xyz, NdotL);
#else
            shadow = GetShadow(0, position.xyz, vec2(0.0), NdotL);
#endif
        }

        const vec3 diffuse_color = albedo_linear * (1.0 - metalness);

        const float material_reflectance = 0.5;
        const float reflectance = 0.16 * material_reflectance * material_reflectance; // dielectric reflectance
        const vec3 F0 = albedo_linear.rgb * metalness + (reflectance * (1.0 - metalness));
        
        const vec2 AB = BRDFMap(roughness, NdotV);
        const vec3 dfg = albedo_linear.rgb * AB.x + AB.y;
        
        const vec3 energy_compensation = 1.0 + F0 * (AB.y - 1.0);
        const float perceptual_roughness = sqrt(roughness);
        const float lod = ROUGHNESS_LOD_MULTIPLIER * perceptual_roughness * (2.0 - perceptual_roughness);
        
        vec3 irradiance = vec3(0.0);
        vec4 reflections = vec4(0.0);

        vec3 ibl = vec3(0.0);

        if (scene.environment_texture_usage != 0) {
            ibl = TextureCubeLod(gbuffer_sampler, rendered_cubemaps[scene.environment_texture_index], R, lod).rgb;
        }

#if HYP_VCT_ENABLED
        vec4 vct_specular = ConeTraceSpecular(position.xyz, N, R, roughness);
        vec4 vct_diffuse  = ConeTraceDiffuse(position.xyz, N, T, B, roughness);

        irradiance  = vct_diffuse.rgb;
        reflections = vct_specular;
#endif

#if HYP_SSR_ENABLED
        // vec4 screen_space_reflections = Texture2D(gbuffer_sampler, ssr_sample, texcoord);
        // reflections = mix(reflections, screen_space_reflections, screen_space_reflections.a);
#endif

        vec3 light_color = vec3(1.0);
        
        // ibl
        //vec3 dfg =  AB;// mix(vec3(AB.x), vec3(AB.y), vec3(1.0) - F0);
        vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
        vec3 radiance = dfg * ibl * specular_ao;
        result = (diffuse_color * (irradiance * IRRADIANCE_MULTIPLIER) * (1.0 - dfg) * ao) + radiance;
        result *= exposure * IBL_INTENSITY;
        result = (result * (1.0 - reflections.a)) + (dfg * reflections.rgb);
        //end ibl

        // surface

        for (int i = 0; i < min(scene.num_lights, 8); i++) {
            Light light = lights[i];

            L = light.position.xyz;
            L -= position.xyz * float(min(light.type, 1));
            L = normalize(L);

            H = normalize(L + V);

            NdotL = max(0.0001, dot(N, L));
            NdotV = max(0.0001, dot(N, V));
            NdotH = max(0.0001, dot(N, H));
            LdotH = max(0.0001, dot(L, H));

            vec4 light_color = unpackUnorm4x8(light.color_encoded);
            vec3 light_color_rgb = light_color.rgb * light_color.a;

            vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));
            float D = DistributionGGX(N, H, roughness);
            float G = CookTorranceG(NdotL, NdotV, LdotH, NdotH);
            vec3 F = SchlickFresnel(F0, F90, LdotH);
            vec3 specular = D * G * F;

            float distance = max(length(light.position.xyz - position.xyz), light.radius);
            float attenuation = (light.type == HYP_LIGHT_TYPE_POINT) ?
                (light.intensity/(distance*distance)) : 1.0;
            vec3 light_scatter = SchlickFresnel(vec3(1.0), F90, NdotL);
            vec3 view_scatter  = SchlickFresnel(vec3(1.0), F90, NdotV);
            vec3 diffuse = diffuse_color * (light_scatter * view_scatter * (1.0 / PI));
            result += (specular + diffuse * energy_compensation) * ((exposure * light.intensity) * NdotL * ao * shadow * light_color_rgb) * attenuation;

        }
        //result = reflections.rgb;
    } else {
        result = albedo.rgb;
    }

#if SSAO_DEBUG
    result = vec3(ao);
#endif

    output_color = vec4(result, 1.0);
    output_color.rgb = Tonemap(output_color.rgb);


//    output_color.rgb = Texture2D(gbuffer_sampler, ssr_blur_vert, texcoord).rgb;

}
