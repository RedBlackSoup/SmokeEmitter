#include "globals.hlsli"

RWStructuredBuffer<Particle> particlePool : register(u0);
RWBuffer<int> aliveList0 : register(u1);
RWBuffer<int> aliveList1 : register(u2);
RWBuffer<int> deadList : register(u3);
RWStructuredBuffer<ParticleInstance> instanceBuffer : register(u4);
RWBuffer<int> counters : register(u5);
RWBuffer<int> sortedAliveList : register(u6);


float4x4 scaleXYZ(float x, float y, float z)
{
    return float4x4(
        x, 0.0f, 0.0f, 0.0f,
        0.0f, y, 0.0f, 0.0f,
        0.0f, 0.0f, z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

float4x4 rotationZ(float angle)
{
    float cosTheta = cos(angle);
    float sinTheta = sin(angle);

    return float4x4(
        cosTheta, -sinTheta, 0.0f, 0.0f,
        sinTheta, cosTheta, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

float4x4 billboardTrans(float3 pos)
{
    float3 eye = viewPos.xyz;
    float3 upDir = upDirection.xyz;
    float3 g = normalize(eye - pos);
    float3 t = upDir;
    float3 r = normalize(cross(t, g));
    t = normalize(cross(g, r));
    return float4x4(
        r.x, t.x, g.x, pos.x,
        r.y, t.y, g.y, pos.y,
        r.z, t.z, g.z, pos.z,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

float4x4 zeroMatrix()
{
    return float4x4(
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    );
}

[numthreads(1, 1, 1)]
void CSMain( uint3 DTid : SV_DispatchThreadID )
{
    int currentAliveCount = counters[0];

    if (DTid.x < currentAliveCount)
    {
        Particle p = particlePool[sortedAliveList[DTid.x]];
    
        float life = p.LifeRemaining / p.LifeTime;
    
        float scale = lerp(p.SizeEnd, p.SizeBegin, life);
        
        
        
        float alpha = 1.0f;
        if (life < 0.1f)
        {
            float alphaLife = saturate(life / 0.1f);
            alpha = lerp(0.0f, 1.0f, alphaLife);
        }
        else if(life > 0.9f)
        {
            float alphaLife = saturate((life - 0.9f) / 0.1f);
            alpha = lerp(1.0f, 0.0f, alphaLife);
        }
        
        float4 color = lerp(p.ColorEnd, p.ColorBegin, life);
        color.a *= alpha;
        
        //float4x4 transMatrix = mul(billboardTrans(p.Position.xyz), mul(rotationZ(p.Rotation), scaleXYZ(scale, scale, 1.0f)));
        float4x4 transMatrix = mul(billboardTrans(p.Position.xyz), mul(rotationZ(p.Rotation), scaleXYZ(scale, scale, scale)));

        ParticleInstance pIns;
        
        pIns.color = color;
        pIns.model = transMatrix;
        pIns.life = life;
        instanceBuffer[DTid.x] = pIns;
    }
    else
    {
        ParticleInstance pIns;
        
        pIns.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        pIns.model = zeroMatrix();
        pIns.life = 0;
        instanceBuffer[DTid.x] = pIns;
    }
}