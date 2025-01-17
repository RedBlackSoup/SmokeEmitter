#include "globals.hlsli"
struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    //float2 uv : TEXCOORD0;
    //float3 tangent : TANGENT;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 fragPositon : FRAG_POSITION;
    float3 normal : NORMAL;
};

//Texture2D g_txDiffuse : register(t0);
//SamplerState g_sampler : register(s0);

//cbuffer cb0 : register(b0)
//{
//    float4x4 g_mWorldViewProj;
//};

PSInput VSMain(VSInput input)
{
    PSInput result;
    
    result.position = mul(float4(input.position, 1.0f), ViewProj);
    result.fragPositon = float4(input.position, 1.0f);
    result.normal = input.normal;
    
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 color = float4(0.15f, 0.3f, 0.3f, 1.0f);
    float3 normal = input.normal;
    // phong shading
    float ambient = 0.1;
    float lightning = ambient;
    for (int i = 0; i < LIGHT_NUM; i++)
    {
        
        float3 lightViewOffset = pointLights[i].position.xyz - input.fragPositon.xyz;
        float distance = length(lightViewOffset);
        float attenuation = 1.0f / (distance * distance);
        
        float ambientstrength = 10.0f;
        
        float3 lightDir = normalize(lightViewOffset);
        float diffuse = max(dot(normal, lightDir), 0.0);
        diffuse *= ambientstrength * attenuation;
    
        float specularStrength = 0.01f;
        float3 viewDir = normalize(viewPos.xyz - input.fragPositon.xyz);
        float3 reflectDir = reflect(-lightDir, normal);
        float specular = specularStrength * pow(max(dot(viewDir, reflectDir), 0.0), 2) * attenuation;
        
        lightning += (diffuse + specular) * pointLights[i].intensity;
    }
    
    for (int i = 0; i < DIRLIGHT_NUM; i++)
    {
        float ambientstrength = 1.0f;
        
        float3 lightDir = normalize(-dirLights[i].direction);
        float diffuse = max(dot(normal, lightDir), 0.0);
        diffuse *= ambientstrength;
        
        // No specular reflection
        //float specularStrength = 0.001f;
        //float3 viewDir = normalize(viewPos.xyz - input.fragPositon.xyz);
        //float3 reflectDir = reflect(-lightDir, normal);
        //float specular = specularStrength * pow(max(dot(viewDir, reflectDir), 0.0), 32);
        
        lightning += diffuse * dirLights[i].intensity;
    }
    
    return lightning * color;
    
    //return g_txDiffuse.Sample(g_sampler, input.uv);
}