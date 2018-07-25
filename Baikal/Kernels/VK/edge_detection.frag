#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "common_structures.glsl"
#include "common.glsl"
#include "utils.glsl"

layout (location = 0) in vec2 tex_coord;

layout (binding = 0) uniform sampler2DMS normal_buffer;
layout (binding = 1) uniform sampler2DMS depth_buffer;
layout (binding = 2) uniform sampler2DMS g_buffer_3;

layout (location = 0) out vec4 color;

layout(push_constant) uniform PushConsts {
	int num_samples;
} push_constants;

void main()
{
    ivec2 size = textureSize(normal_buffer);
	ivec2 iuv = ivec2(tex_coord * size);

    int num_samples = push_constants.num_samples;
    int edge = 0;

    for (int i = 0; i < num_samples; i++)
    {
        /*float depth  = texelFetch(depth_buffer, iuv, i).x;

        if (abs(depth) > 0)
        {
            const float depth_threshold  = 0.01f;

            float depth_left = float(texelFetch(depth_buffer, iuv + ivec2(-1, 0), i).x);
            float depth_top  = float(texelFetch(depth_buffer, iuv + ivec2(0, 1), i).x);
            
            vec2 delta = abs(depth.xx - vec2(depth_left, depth_top));
            vec2 edges = step(depth_threshold.xx, delta);

            edge |= dot(edges, edges) == 0.f ? 0 : 1;

            if (edge > 0) break;
        }
        */

        vec3 n  = texelFetch(normal_buffer, iuv, i).xyz * 2.f - 1.f;

        if (dot(n, n) > 0)
        {
            const float threshold  = cos(PI / 40.f);

            vec3 n_left = texelFetch(normal_buffer, iuv + ivec2(-1, 0), i).xyz * 2.f - 1.f;
            vec3 n_top  = texelFetch(normal_buffer, iuv + ivec2(0, -1), i).xyz * 2.f - 1.f;

            vec2 delta  = vec2(dot(n, n_left), dot(n, n_top));
            edge |= delta.x < threshold ? 1 : 0;
            edge |= delta.y < threshold ? 1 : 0;

            if (edge > 0) break;
        }

        int id      = int(texelFetch(g_buffer_3, iuv, i).z);
        int id_top  = int(texelFetch(g_buffer_3, iuv + ivec2(0, -1), i).z);
        edge        = id != id_top ? 1 : 0;

        if (edge > 0) break;

        int id_left = int(texelFetch(g_buffer_3, iuv + ivec2(-1, 0), i).z);
        edge        = id != id_left ? 1 : 0;

        if (edge > 0) break;
    }

	color = float(edge).xxxx;
}