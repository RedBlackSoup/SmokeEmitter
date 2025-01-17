#include "globals.hlsli"
#define DEPTHFADED

static const float4 TANGENT = float4(1.0f, 0.0f, 0.0f, 0.0f);
static const float4 BITANGENT = float4(0.0f, -1.0f, 0.0f, 0.0f);
static const float4 NORMAL = float4(0.0f, 0.0f, 1.0f, 0.0f);

struct PSInput
{
    float4 clipPosition : SV_POSITION;
    float4 color : COLOR;
    float4 fragPositon : FRAG_POSITION;
    float2 uv : TEXCOORD;
    float2 uvNext : NEXT;
    float blendAlpha : BLEND_ALPHA;
    float3x3 TBN : TBN_MATRIX;
};

Texture2D<float4> g_texture_0 : register(t0);
Texture2D<float4> g_texture_1 : register(t1);
Texture2D<float4> g_texture_2 : register(t2);
Texture2D<float> g_depth : register(t3);
Texture2D<float4> g_texture_3 : register(t4);

SamplerState g_sampler : register(s0);

void getSubUVOffset(int frame, int rowCount, int colCount, out float2 offest)
{
    int row = frame / rowCount;
    int col = frame % colCount;
    offest.x = float(col) / colCount;
    offest.y = float(row) / rowCount;
}


PSInput VSMain(float3 position : POSITION, float3 uv : TEXCOORD, float4 color : COLOR, float4x4 model : MODEL, float life : LIFE)
{
    PSInput result;
    
    float4 transformedp = mul(model, float4(position, 1.0f));
    result.clipPosition = mul(transformedp, ViewProj);
    
    result.fragPositon = transformedp;
    result.color = color;
    
    life = 16.0f - life * 16.0f;
    life = clamp(life, 0.0f, 16.0f);
    
    // Get sub uv row and column.
    int integerPart = floor(life);
    integerPart = clamp(integerPart, 0, 15);
    float fractionalPart = life - integerPart;
    
    float2 currentFrameOffset, nextFrameOffset;
    
    int currentFrame = integerPart;
    getSubUVOffset(currentFrame, 4, 4, currentFrameOffset);
    int nextFrame = clamp(integerPart + 1, 0, 15);
    getSubUVOffset(nextFrame, 4, 4, nextFrameOffset);
    
    result.uv.x = currentFrameOffset.x + uv.x * 0.25f;
    result.uv.y = currentFrameOffset.y + uv.y * 0.25f;
   
    //result.uv.x = uv.x;
    //result.uv.y = uv.y;
    
    result.uvNext.x = nextFrameOffset.x + uv.x * 0.25f;
    result.uvNext.y = nextFrameOffset.y + uv.y * 0.25f;
    
    if(integerPart == 15)
    {
        fractionalPart = 0.0f;
    }
    
    result.blendAlpha = fractionalPart;
    
    float3 T = normalize(mul(model, TANGENT)).xyz;
    float3 B = normalize(mul(model, BITANGENT)).xyz;
    float3 N = normalize(mul(model, NORMAL)).xyz;
    result.TBN = transpose(float3x3(T, B, N));
    
    return result;
}

float opacityBasedDepthFade(in float Alpha, in float DistanceA, in float DistanceB, in float clip_position_w, in float linear_depth)
{
    float fadeDistance = max(lerp(DistanceA, DistanceB, Alpha), 0.00001f);
    float depth_offset = linear_depth - clip_position_w;
    float fadeValue = saturate(depth_offset / fadeDistance);
    return fadeValue;
}

//float life;
// life 
float4 PSMain(PSInput input) : SV_TARGET
{   
    float2 motionCurrent = g_texture_1.Sample(g_sampler, input.uv);
    float2 motionNext = g_texture_1.Sample(g_sampler, input.uvNext);
    
    const float strength = 0.005f;
    motionCurrent = (motionCurrent - 0.5f) * (-2.0f);
    motionCurrent.x *= strength;
    motionCurrent.y *= -strength;
    
    motionNext = (motionNext - 0.5f) * (-2.0f);
    motionNext.x *= strength;
    motionNext.y *= -strength;
   
    float alpha = input.blendAlpha;
    float2 uvCurrent = input.uv + alpha * motionCurrent;
    float2 uvNext = input.uvNext - (1 - alpha) * motionNext;
    
    // Blend color bewteen current and next frames.
    float4 colorCurrent = g_texture_0.Sample(g_sampler, uvCurrent);
    float4 colorNext = g_texture_0.Sample(g_sampler, uvNext);
    
    float4 colorMixed = lerp(colorCurrent, colorNext, alpha);
    float4 color = input.color;
    color.rgb *= saturate(colorMixed.r);
    color.a *= saturate(colorMixed.g);
    
#ifdef DEPTHFADED
    float4 ndc = input.clipPosition;
    ndc.x /= screenWidth;
    ndc.y /= screenHeight;
    
    float2 screenUv = ndc.xy;
    float depth0 = g_depth.SampleLevel(g_sampler, screenUv, 0);
    float surfaceLinearDepth = compute_lineardepth(depth0);
    float DistanceA = 4.0f;
    float DistanceB = 0.8f;
    
    float fadedValue = opacityBasedDepthFade(color.a, DistanceA, DistanceB, ndc.w, surfaceLinearDepth);
    
    float3 shadowColor = float3(0.1f, 0.1f, 0.1f);
    float3 baseColor = color.rgb;
    
    //color.rgb = lerp(shadowColor, baseColor, saturate(fadedValue));
    color.a *= fadedValue;
#endif // DEPTHFADED
    
    // Blend normal between current and next frames.
    float4 normalCurrent = g_texture_2.Sample(g_sampler, uvCurrent);
    normalCurrent = (normalCurrent * 2.0f) - 1.0f;
    normalCurrent = normalize(normalCurrent);
    
    float4 normalNext = g_texture_2.Sample(g_sampler, uvNext);
    normalNext = (normalNext * 2.0f) - 1.0f;
    normalNext = normalize(normalNext);
    
    float4 normalMixed = lerp(normalCurrent, normalNext, alpha);
    normalMixed = normalize(normalMixed);
    
    float3 normal = mul(input.TBN, normalMixed.xyz);
    //normal = lerp(float3(0.0f, 0.0f, 1.0f), normal, 0.5);
    
    // phong shading
    float ambient = 0.1;
    float lightning = ambient;
    for (int i = 0; i < LIGHT_NUM; i++)
    {
        float3 lightViewOffset = pointLights[i].position.xyz - input.fragPositon.xyz;
        float distance = length(lightViewOffset);
        float attenuation = 1.0f / (distance * distance);
        
        float ambientstrength = 14.0f;
        
        float3 lightDir = normalize(lightViewOffset);
        float diffuse = ambientstrength * max(dot(normal, lightDir), 0.0) * attenuation;
    
        float specularStrength = 5.0f;
        float3 viewDir = normalize(viewPos.xyz - input.fragPositon.xyz);
        float3 reflectDir = reflect(-lightDir, normal);
        float specular = specularStrength * pow(max(dot(viewDir, reflectDir), 0.0), 16) * attenuation;
        
        lightning += (diffuse + specular) * pointLights[i].intensity;
    }
    
    for (int i = 0; i < DIRLIGHT_NUM; i++)
    {
        float ambientstrength = 2.0f;
        
        float3 lightDir = normalize(-dirLights[i].direction);
        float diffuse = ambientstrength * max(dot(normal, lightDir), 0.0);
        
        float specularStrength = 0.5f;
        float3 viewDir = normalize(viewPos.xyz - input.fragPositon.xyz);
        float3 reflectDir = reflect(-lightDir, normal);
        float specular = specularStrength * pow(max(dot(viewDir, reflectDir), 0.0), 16);
        
        lightning += (diffuse + specular) * dirLights[i].intensity;
    }
    
    float4 result;
    result.rgb = lightning * color.rgb;
    result.a = color.a;
    
    //result = g_texture_3.Sample(g_sampler, input.uv);
    
    return result;
}
