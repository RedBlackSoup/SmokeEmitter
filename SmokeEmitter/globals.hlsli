static const int LIGHT_NUM = 2;
static const int DIRLIGHT_NUM = 2;

static const int PARTICLE_POOL_SIZE = 1024;

struct Particle
{
    float4 Position;
    float4 Velocity;
    float4 Force;
    float4 ColorBegin;//64
    
    float4 ColorEnd;
    
    float Rotation;
    float SizeBegin;
    float SizeEnd;
    float LifeTime;//32
    
    float LifeRemaining;
    int Active;// 104
};

struct ParticleProps
{
    float4 Position;
    float4 PositionDistribution;
    float4 Velocity;
    float4 VelocityDistribution;
    float4 Force;
    float4 ColorBegin;
    float4 ColorEnd;

    float SizeBegin;
    float SizeEnd;
    float SizeDistribution;
    float LifeTime;
};

struct Light
{
    float3 position;
    float intensity;
    float4 color;
};

struct DirectionLight
{
    float3 direction;
    float intensity;
    float4 color;
};

struct ParticleInstance
{
    float4 color;
    float4x4 model;
    float life;
};

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 InvViewProj;
    float4x4 View;
    float4x4 ViewProj;
    float4 viewPos;
    float4 upDirection;
    Light pointLights[LIGHT_NUM];
    ParticleProps particleProps;
    float elapsedSeconds;   // 432 bytes
    float3 lookDirection;
    float4 randomBuffer[5]; // 512 bytes
    
    uint screenWidth;
    uint screenHeight;
    float totalSeconds;
    float pads1;
    
    DirectionLight dirLights[DIRLIGHT_NUM];
};


// Reconstructs world-space position from depth buffer
//	uv		: screen space coordinate in [0, 1] range
//	z		: depth value at current pixel
//	InvVP	: Inverse of the View-Projection matrix that was used to generate the depth value
inline float3 reconstruct_position(in float2 uv, in float z, in float4x4 inverse_view_projection)
{
    float x = uv.x * 2 - 1;
    float y = (1 - uv.y) * 2 - 1;
    float4 position_s = float4(x, y, z, 1);
    float4 position_v = mul(position_s, inverse_view_projection);
    return position_v.xyz / position_v.w;
}

// Computes linear depth from post-projection depth
inline float compute_lineardepth(in float z, in float near, in float far)
{
    float z_n = 2.0f * z - 1;
    float lin = 2 * far * near / (near + far + z_n * (near - far));
    return lin;
}

inline float compute_lineardepth(in float z)
{
    return compute_lineardepth(z, 1.0f, 1000.0f);
}

