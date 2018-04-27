struct Light
{
    float4 position;
    float4 direction;
    float4 radiance;
};

struct Camera
{
    float4  camera_position;

    mat4    world_view;
    mat4    projection;

    mat4    inv_view;
    mat4    inv_projection;
};

// Push constants
struct MaterialConstants
{
    int      data[4];   // x - mat_id, xyz - reserved
    float4   diffuse;
    float4   metallic;
    float4   roughness;
    float4   normal;
};