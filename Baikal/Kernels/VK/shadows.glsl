/*
float SampleShadow(sampler2D s, vec3 p, vec3 shadow_uv, float bias, float scale)
{
	float shadow = 1.0f;

	for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
	{
		int index = int(16.0 * Rnd(floor(p.xyz * 255.0f), sampleIdx)) % 16;

		float occluded = texture(s, vec2(shadow_uv.st + PoissonDisk[index] * scale)).r < shadow_uv.z - bias ? 1.0f : 0.0f;
		shadow -= 0.25 * occluded;
	}

	return shadow;
}
*/

float SampleShadow(sampler2DShadow s, vec3 p, vec3 shadow_uv, float bias, float scale)
{
	float shadow = 0.0f;
	shadow_uv.z = shadow_uv.z - bias;
	
	int count = 0;
	int range = 1;

	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadow += texture(s, vec3(shadow_uv + vec3(vec2(x, y) * scale * 0.5f, 0.f))).r;
			count++;
		}
	}

	return shadow / count;
}