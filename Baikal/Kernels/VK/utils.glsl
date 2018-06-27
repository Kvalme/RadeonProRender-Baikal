
// Spheremap Transform
uvec2 EncodeNormal(vec3 n)
{
    vec2 enc = normalize(n.xy) * (sqrt(abs(-n.z*0.5+0.5)));

    return uvec2((enc * 0.5f + 0.5f) * 65535.0f);
}

// Spheremap Transform
vec3 DecodeNormal(uvec2 enc)
{
    vec2 enc_n = vec2(enc.xy) / 65535.0f;

    vec4 nn = enc_n.xyxx * vec4(2,2,0,0) + vec4(-1,-1, 1,-1);
    float l = abs(dot(nn.xyz,-nn.xyw));
    nn.z = l;
    nn.xy *= sqrt(l);
    return nn.xyz * 2 + vec3(0,0,-1);
}

// depth z/w - 24 bits, mesh id - 8 bits
uvec2 EncodeDepthAndMeshID(float depth, float meshID)
{
    uint idepth = uint(depth * 16777215.0f);
    return uvec2(idepth & 0xFFFF, ((idepth & 0xFF0000) >> 8) | uint(meshID));
}

// Decode depth z/w - 24 bits, mesh id - 8 bits
vec2 DecodeDepthAndMeshID(uvec2 data)
{
    float meshID 	    = data.y & 0xFF;
    uint depthHighBits 	= ((data.y >> 8) & 0xFF) << 16;
    float depth 		= float(depthHighBits | data.x) / 16777215.0f;

    return vec2(depth, meshID);
}

vec3 ReconstructVSPositionFromDepth(mat4 inv_proj, vec2 uv, float depth)
{
    uv = uv * vec2(2.0f, 2.0f) - vec2(1.0f, 1.0f);

    vec4 proj_pos = vec4(uv, depth, 1.0f);
    vec4 view_pos = proj_pos * inv_proj;

    return view_pos.xyz / view_pos.w;
}

// From http://www.thetenthplanet.de/archives/1180
mat3 CotangentFrame( vec3 N, vec3 p, vec2 uv )
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx( p );
    vec3 dp2 = dFdy( p );
    vec2 duv1 = dFdx( uv );
    vec2 duv2 = dFdy( uv );
 
    // solve the linear system
    vec3 dp2perp = cross( dp2, N );
    vec3 dp1perp = cross( N, dp1 );
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct a scale-invariant frame 
    float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
    return mat3( T * invmax, B * invmax, N );
}

vec3 ConvertBumpToNormal(sampler2D bump, vec2 uv)
{  
    
    ivec2 size = textureSize(bump, 0);
    vec2 inv_size = vec2(1.0f, 1.0f) / size;

    float heights[4] = {
        texture(bump, uv + vec2(0.0f, 0.f) * inv_size).x,
        texture(bump, uv + vec2(1.0f, 0.f) * inv_size).x,
        texture(bump, uv + vec2(0.0f, 1.f) * inv_size).x,
        texture(bump, uv + vec2(1.0f, 1.f) * inv_size).x
    };

    //vec4 heights = textureGather(bump, uv);

    float du = (heights[1] - heights[0]);
    float dv = (heights[3] - heights[0]);
    
    return normalize(vec3(du, dv, 1.0f/3.0f));
}

vec2 PoissonDisk[16] = vec2[]( 
   vec2( -0.94201624, -0.39906216 ), 
   vec2( 0.94558609, -0.76890725 ), 
   vec2( -0.094184101, -0.92938870 ), 
   vec2( 0.34495938, 0.29387760 ), 
   vec2( -0.91588581, 0.45771432 ), 
   vec2( -0.81544232, -0.87912464 ), 
   vec2( -0.38277543, 0.27676845 ), 
   vec2( 0.97484398, 0.75648379 ), 
   vec2( 0.44323325, -0.97511554 ), 
   vec2( 0.53742981, -0.47373420 ), 
   vec2( -0.26496911, -0.41893023 ), 
   vec2( 0.79197514, 0.19090188 ), 
   vec2( -0.24188840, 0.99706507 ), 
   vec2( -0.81409955, 0.91437590 ), 
   vec2( 0.19984126, 0.78641367 ), 
   vec2( 0.14383161, -0.14100790 ) 
);

float Rnd(vec3 seed, int i){
	vec4 seed4 = vec4(seed,i);
	float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
	return fract(sin(dot_product) * 43758.5453);
}