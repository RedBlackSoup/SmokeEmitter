#include "globals.hlsli"

RWStructuredBuffer<Particle> particlePool : register(u0);
RWBuffer<int> aliveList0 : register(u1);
RWBuffer<int> aliveList1 : register(u2);
RWBuffer<int> deadList : register(u3);
RWStructuredBuffer<ParticleInstance> instanceBuffer : register(u4);
RWBuffer<int> counters : register(u5);

static uint randomCounter = 0;
static uint subsrciptCounter = 0;
static const float PI = 3.14159265359f;


float getRandom()
{
    float result = randomBuffer[randomCounter][subsrciptCounter];
    if (subsrciptCounter == 4)
    {
        randomCounter++;
    }
    
    subsrciptCounter = (++subsrciptCounter) % 4;
    return result;
}

[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x == 0)
    {
        Particle p;
        
        p.Position = particleProps.Position;
        {
            float theta = 2 * PI * getRandom();
            float r = particleProps.PositionDistribution.y * sqrt(getRandom());
            float h = particleProps.PositionDistribution.z * getRandom();
            
            float3 a = float3(1.0f, 0.0f, 0.0f);
            float3 b = float3(0.0f, 0.0f, 1.0f);
            float3 c = float3(0.0f, 1.0f, 0.0f);
            float3 dist = r * a * sin(theta) + r * b * cos(theta) + h * c;
            p.Position += float4(dist, 0.0f);
        }
        p.Velocity = particleProps.Velocity;
        {
            float3 dist;
            dist.x = particleProps.VelocityDistribution.x * (getRandom() - 0.5f);
            dist.y = particleProps.VelocityDistribution.y * (getRandom() - 0.5f);
            dist.z = particleProps.VelocityDistribution.z * (getRandom() - 0.5f);
            dist = dist * 0.1f;
            p.Velocity += float4(dist, 0.0f);
        }
        
        p.Force = particleProps.Force;
    
        p.SizeBegin = particleProps.SizeBegin;
        //p.SizeBegin = particleProps.SizeBegin + particleProps.SizeDistribution * (getRandom() - 0.5f) * 0.25f;
        p.SizeEnd = particleProps.SizeEnd;
    
        p.Rotation = getRandom() * 2.0f * PI;
    
        p.ColorBegin = particleProps.ColorBegin;
        p.ColorEnd = particleProps.ColorEnd;
    
        p.LifeTime = particleProps.LifeTime;
        p.LifeRemaining = particleProps.LifeTime;
    
        p.Active = 1;
        
        int deadCount;
        InterlockedAdd(counters[2], -1, deadCount);
        if (deadCount <= 0)
        {
            InterlockedAdd(counters[2], 1);
            return;
        }
        int poolIndex = deadList[deadCount - 1];
        particlePool[poolIndex] = p;
    
        int nextAliveCount;
        InterlockedAdd(counters[1], 1, nextAliveCount);
        aliveList1[nextAliveCount] = poolIndex;
    }
    
    InterlockedAdd(counters[3], 1);
}