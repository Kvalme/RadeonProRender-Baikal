#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "common_structures.glsl"
#include "tonemap.glsl"
#include "common.glsl"
#include "utils.glsl"

layout (binding = 0) uniform sampler2D current_luminance;
layout (binding = 1) uniform sampler1D prev_luminance;

layout (location = 0) in vec2 tex_coord;
layout (location = 0) out vec4 color;

layout(push_constant) uniform PushConsts {
	VkTonemapperPushConstants data; // x - dt, y - tau
} push_constants;

void main()
{
	float current 	= exp(textureLod(current_luminance, vec2(0.5f), 0.0f).x);
	float prev 		= exp(textureLod(prev_luminance, 0.5f, 0.0f).x);

	float dt		= push_constants.data.data.x;
	float tau	 	= push_constants.data.data.y;

	float adapt		= prev + (current - prev) * (1 - exp(-dt * tau));
	adapt 			= isnan(adapt) ? 0.05f : adapt;

	color			= log(adapt).xxxx;
}
