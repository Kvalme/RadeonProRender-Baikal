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

    vec4 projPos = vec4(uv, depth, 1.0f);
    vec4 viewPos = projPos * inv_proj;

    return viewPos.xyz / viewPos.w;
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