static const int kPoint = 0x1;
static const int kDirectional = 0x2;
static const int kSpot = 0x3;
static const int kIbl = 0x4;
static const int kArea = 0x5;

struct Light
{
    float4 position;
    float4 direction;
    float4 radiance;
};

struct Camera
{
    float4  camera_position;

    mat4    view_projection;

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
};

static const int TextureFormatRGBA8 = 1;
static const int TextureFormatRGBA16 = 2;
static const int TextureFormatRGBA32 = 3;

/// Texture description
struct Texture
{
    // Width, height and depth
    int w;
    int h;
    int d;
    // Offset in texture data array
    int data_offset;
    // Format
    int fmt;
    int extra;
};
