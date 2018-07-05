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

vec3 Tonemap(vec3 v)
{
	float exposure_bias = 2.0f;

	v 					= Uncharted2Tonemap(exposure_bias * v);
	vec3 white_scale	= 1.0f / Uncharted2Tonemap(vec3(11.2f));

	return v * white_scale;
}