#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "common_structures.glsl"
#include "tonemap.glsl"

layout (location = 0) in vec2 tex_coord;

layout (binding = 1) uniform sampler2D deferred_buffer;
layout (binding = 2) uniform sampler2D history_buffer;
layout (binding = 3) uniform sampler2D motion_buffer;

layout (location = 0) out vec4 color;

float Luminance(vec3 clr)
{
    return dot(clr, vec3(0.299f, 0.587f, 0.114f));
}

 float FilterCubic(float x, float B, float C)
{
    float y = 0.0f;
    float x2 = x * x;
    float x3 = x * x * x;
    if(x < 1)
        y = (12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 + (6 - 2 * B);
    else if (x <= 2)
        y = (-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 + (-12 * B - 48 * C) * x + (8 * B + 24 * C);

    return y / 6.0f;
}

vec3 Reproject(vec2 uv)
{
	bool use_standart_reprojection = false;
	
	vec2 motion 	= textureLod(motion_buffer, tex_coord, 0.f).xy;

	vec3 reprojected_pixel = vec3(0.f);
	vec2 reprojected_uv = tex_coord - motion;
	ivec2 history_buffer_size = textureSize(history_buffer, 0);

	vec2 reprojected_pos = reprojected_uv * history_buffer_size;

	if (use_standart_reprojection)
	{
		reprojected_pixel = textureLod(history_buffer, reprojected_uv, 0.f).rgb;
	}
	else
	{
		float weight = 0.f;
		vec3 sum = vec3(0.f);

		for (int i = -1; i < 2; i++)
		{
			for (int j = -1; j < 2; j++)
			{
				vec2 current_sample_pos = floor(reprojected_pos + vec2(i, j)) + 0.5f;

				vec2 current_uv = current_sample_pos / history_buffer_size;
				vec3 current_sample = textureLod(history_buffer, current_uv, 0).xyz;

				vec2 sample_dist = abs(current_sample_pos - reprojected_pos);
				
				// Catmull-Rom filtering
				float filter_weight = FilterCubic(sample_dist.x, 0, 0.5f) * FilterCubic(sample_dist.y, 0, 0.5f);

				float sample_lum = Luminance(current_sample);

				filter_weight *= 1.0f / (1.0f + sample_lum);

				sum += current_sample * filter_weight;
				weight += filter_weight;
			}
		}

		reprojected_pixel = max(sum / weight, 0.f);
	}

	return reprojected_pixel;
}

void main()
{
	vec3 history = Reproject(tex_coord);

	// RGB clamp
	vec3 current 	= textureLod(deferred_buffer, tex_coord, 0.f).rgb;

	vec3 neighbor[4] = 
	{
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(1, 0)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(0, 1)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(-1, 0)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(0, -1)).rgb
	};

	vec3 box_min = min(current.rgb, min(neighbor[0], min(neighbor[1], min(neighbor[2], neighbor[3]))));
	vec3 box_max = max(current.rgb, max(neighbor[0], max(neighbor[1], max(neighbor[2], neighbor[3]))));

	history = clamp(history, box_min, box_max);

	float temporal_aa_weight = 0.9f;

	float weight_a = 1.0f - temporal_aa_weight;
	float weight_b = 1.0f - weight_a;

	color.rgb = mix(current, history, temporal_aa_weight);
	color.a = 1.0f;
}