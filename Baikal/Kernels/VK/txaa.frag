#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "common_structures.glsl"
#include "tonemap.glsl"
#include "common.glsl"
#include "utils.glsl"

layout (location = 0) in vec2 tex_coord;

layout (binding = 1) uniform sampler2D deferred_buffer;
layout (binding = 2) uniform sampler2D history_buffer;
layout (binding = 3) uniform sampler2DMS motion_buffer;
layout (binding = 4) uniform sampler2DMS depth_buffer;

layout (location = 0) out vec4 color;

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

vec2 GetVelocity(vec2 uv)
{
	vec2 velocity = vec2(0.0f);

	float closest_depth = 0.0f;

	ivec2 texture_size = textureSize(motion_buffer);
	ivec2 iuv = ivec2(texture_size * uv);

	for(int vy = -1; vy <= 1; ++vy)
    {
		for(int vx = -1; vx <= 1; ++vx)
        {
			ivec2 offset = ivec2(vx, vy);

			vec2 neighbor_velocity = texelFetch(motion_buffer, iuv + offset, 0).xy;
			float neighbor_depth = texelFetch(depth_buffer, iuv + offset, 0).x;

            if(neighbor_depth > closest_depth)
            {
            	velocity = neighbor_velocity;
                closest_depth = neighbor_depth;
            }
        }
    }

	return velocity;
}

vec3 Reproject(vec2 uv)
{
	const bool use_standart_reprojection = false;
	const bool use_dilate_velocity = true;

	ivec2 texture_size = textureSize(motion_buffer);
	ivec2 iuv = ivec2(texture_size * uv);

	vec2 motion 	= use_dilate_velocity ? GetVelocity(uv) : texelFetch(motion_buffer, iuv, 0).xy;

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

vec3 HistoryAABBClamp(vec3 current_pixel, vec3 history)
{
	vec3 neighbor[8] = 
	{
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(1, 0)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(0, 1)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(-1, 0)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(0, -1)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(-1, -1)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(1, 1)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(1, -1)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(-1, 1)).rgb
	};

	vec3 box_min = current_pixel.rgb;
	vec3 box_max = current_pixel.rgb;

	for (int i = 0; i < 8; i++)
	{
		box_min = min(box_min, neighbor[i]);
		box_max = max(box_max, neighbor[i]);
	}

	return clamp(history, box_min, box_max);
}

// From "Temporal Reprojection Anti-Aliasing"
// https://github.com/playdeadgames/temporal
vec3 ClipAABB(vec3 aabbMin, vec3 aabbMax, vec3 prevSample)
{
        // note: only clips towards aabb center
        vec3 p_clip = 0.5 * (aabbMax + aabbMin);
        vec3 e_clip = 0.5 * (aabbMax - aabbMin);

        vec3 v_clip = prevSample - p_clip;
        vec3 v_unit = v_clip.xyz / e_clip;
        vec3 a_unit = abs(v_unit);
        float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

        if (ma_unit > 1.0)
            return p_clip + v_clip / ma_unit;
        else
            return prevSample;// point inside aabb		
}

vec3 HistoryAABBClip(vec3 current_pixel, vec3 history)
{
	vec3 neighbor[8] = 
	{
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(1, 0)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(0, 1)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(-1, 0)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(0, -1)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(-1, -1)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(1, 1)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(1, -1)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(-1, 1)).rgb
	};

	vec3 box_min = current_pixel.rgb;
	vec3 box_max = current_pixel.rgb;

	for (int i = 0; i < 8; i++)
	{
		box_min = min(box_min, neighbor[i]);
		box_max = max(box_max, neighbor[i]);
	}

	return ClipAABB(box_min, box_max, history);
}

vec3 HistoryVarianceClamp(vec3 current_pixel, vec3 history)
{
	const float VARIANCE_CLIPPING_GAMMA = 1.0;

	vec3 neighbor[4] = 
	{
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(1, 0)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(0, 1)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(-1, 0)).rgb,
		textureLodOffset(deferred_buffer, tex_coord, 0.0, ivec2(0, -1)).rgb
	};

	vec3 m1 = current_pixel + neighbor[0] + neighbor[1] + neighbor[2] + neighbor[3];
	vec3 m2 = current_pixel * current_pixel + neighbor[0] * neighbor[0] 
											+ neighbor[1] * neighbor[1]
	 										+ neighbor[2] * neighbor[2]
											+ neighbor[3] * neighbor[3];

	vec3 mu = m1 / 5.0;
	vec3 sigma = sqrt(m2 / 5.0 - mu * mu);

	vec3 box_min = mu - VARIANCE_CLIPPING_GAMMA * sigma;
	vec3 box_max = mu + VARIANCE_CLIPPING_GAMMA * sigma;

	return clamp(history, box_min, box_max);
}

void main()
{
	vec3 history 	= Reproject(tex_coord);
	vec3 current 	= textureLod(deferred_buffer, tex_coord, 0.f).rgb;
	history 		= HistoryAABBClip(current, history);
	
	float temporal_aa_weight = 0.9f;

	color.rgb = mix(current, history, temporal_aa_weight);
	color.a = 1.0f;
}