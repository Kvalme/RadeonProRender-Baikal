#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "common_structures.glsl"
#include "tonemap.glsl"
#include "common.glsl"
#include "utils.glsl"

layout (location = 0) in vec2 tex_coord;

layout (binding = 0) uniform sampler2D image;
layout (binding = 1) uniform sampler1D luminance;

layout (location = 0) out vec4 color;

layout(push_constant) uniform PushConsts {
	VkTonemapperPushConstants data; // x - enable tonemapper, y - max_mip for luma readback
} push_constants;

//#define DEBUG_LUMA 1

void main()
{
	float avg_luminance = exp(textureLod(luminance, 0.5f, push_constants.data.data.y)).x;
	vec3 current 		= textureLod(image, tex_coord, 0.f).rgb;

	color.rgb = bool(push_constants.data.data.x) ? TonemapReinhard(current, avg_luminance) : current;

#if DEBUG_LUMA
	if (tex_coord.x > 0.95 && tex_coord.y > 0.95)
		color.rgb = avg_luminance.xxx;
#endif

	color.a = 1.0f;
}
