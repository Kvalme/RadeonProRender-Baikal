//From http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 ExposedColor(vec3 color, float avg_luminance)
{
	float key_value = 1.03f - (2.f / (2.f + log(avg_luminance + 1.f)));
	float linear_exposure = key_value / max(avg_luminance, 0.0000001f);
    float exposure = log2(max(linear_exposure, 0.0001f));

	return exp2(exposure) * color;
}

vec3 Tonemap(vec3 color, float avg_luminance)
{
	vec3 exposed_color 	= ExposedColor(color, avg_luminance);
	exposed_color 		= Uncharted2Tonemap(exposed_color);
	vec3 white_scale	= 1.0f / Uncharted2Tonemap(vec3(11.2f));

	return exposed_color * white_scale;
}
/*
WIP
vec3 TonemapReinhard(vec3 color, float avg_luminance, float prescale, float postscale, float burn)
{
	const float white_level_2 = 11.2f * 11.2f;
	const float invB2 = burn > 0.f ? 1.f / (burn * burn) : 1e5f;

	vec3 exposed_color 	= ExposedColor(color, avg_luminance);
	const float luminance = 0.212671f * exposed_color.x + 0.715160f * exposed_color.y + 0.072169f * exposed_color.z;
	const float tonemapped_luminance = postscale * luminance * (1.0f + prescale * invB2 * luminance / white_level_2) / (1.0f + luminance);

	const float luminance_saturation = 1.0f;
	return tonemapped_luminance * pow(exposed_color / tonemapped_luminance, luminance_saturation.xxx);
}
*/

vec3 TonemapReinhard(vec3 color, float avg_luminance)
{
	const float white_level = 11.2f;
	const float white_level_2 = white_level * white_level;

	vec3 exposed_color 	= ExposedColor(color, avg_luminance);
	const float luminance = 0.212671f * exposed_color.x + 0.715160f * exposed_color.y + 0.072169f * exposed_color.z;
	const float tonemapped_luminance =  luminance * (1.0f + luminance / white_level_2) / (1.0f + luminance);

	const float luminance_saturation = 1.0f;
	return tonemapped_luminance * pow(exposed_color / tonemapped_luminance, luminance_saturation.xxx);
}