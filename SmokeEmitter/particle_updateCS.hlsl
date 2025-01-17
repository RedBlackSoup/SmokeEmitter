#include "globals.hlsli"

#define DEPTHCOLLISIONS

SamplerState g_sampler : register(s0);
Texture2D<float> g_depth : register(t3);

RWStructuredBuffer<Particle> particlePool : register(u0);
RWBuffer<int> aliveList0 : register(u1);
RWBuffer<int> aliveList1 : register(u2);
RWBuffer<int> deadList : register(u3);
RWStructuredBuffer<ParticleInstance> instanceBuffer : register(u4);
RWBuffer<int> counters : register(u5);


[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    int currentAliveCount = counters[0];

    if (DTid.x < currentAliveCount)
    {
        int poolIndex = aliveList0[DTid.x];
        
        Particle p = particlePool[poolIndex];
    
        if (p.LifeRemaining <= 0.0f)
        {
            p.Active = 0;
        }
    
        if (p.Active == 0)
        {
            int deadCount;
            InterlockedAdd(counters[2], 1, deadCount);
            deadList[deadCount] = poolIndex;
            return;
        }
    
        
        float3 force =  p.Force.xyz;
        //float3 force = airResistance + p.Force.xyz;
        
        if(length(p.Velocity.xyz) > 1.0f)
        {
        //            // 空气阻力
            float airDragCoefficient = 2.0f;
            float3 airResistance = normalize(-p.Velocity.xyz) * pow(length(p.Velocity.xyz), 2);
            airResistance *= airDragCoefficient;

            force += airResistance;
        }

        p.Velocity += elapsedSeconds * float4(force, 1.0f);
        
#ifdef DEPTHCOLLISIONS   
        // 计算当前2D空间中的位置
        float4 pos2D = mul(float4(p.Position.xyz, 1.0f), ViewProj);
        pos2D.xyz /= pos2D.w;

        if (pos2D.x > -1 && pos2D.x < 1 && pos2D.y > -1 && pos2D.y < 1 && pos2D.z > -1 && pos2D.z < 1)
        {
            float2 uv = pos2D.xy * float2(0.5f, -0.5f) + 0.5f;
            float deltaU = 1.0f / screenWidth;
            float deltaV = 1.0f / screenHeight;
            
            float depth0 = g_depth.SampleLevel(g_sampler, uv, 0);
            
            float surfaceLinearDepth = compute_lineardepth(depth0);
            float surfaceThickness = 1.5f;

			// check if particle is colliding with the depth buffer, but not completely behind it:
            if ((pos2D.w + 2.0f > surfaceLinearDepth) && (pos2D.w - 2.0f  < surfaceLinearDepth + surfaceThickness))
            {
				// Calculate surface normal and bounce off the particle:
                float2 uv1 = float2(uv.x + deltaU, uv.y);
                float2 uv2 = float2(uv.x, uv.y + deltaV);
                
                float depth1 = g_depth.SampleLevel(g_sampler, uv1, 0);
                float depth2 = g_depth.SampleLevel(g_sampler, uv2, 0);
                
                float3 p0 = reconstruct_position(uv, depth0, InvViewProj);
                float3 p1 = reconstruct_position(uv1, depth1, InvViewProj);
                float3 p2 = reconstruct_position(uv2, depth2, InvViewProj);

                float3 surfaceNormal = normalize(cross(p2 - p0, p1 - p0));
                if (dot(p.Velocity.xyz, surfaceNormal) < 0)
                {
                    float3 vel = p.Velocity.xyz;
                    float3 vel_n = dot(vel, -surfaceNormal) * (-surfaceNormal);
                    float3 vel_h = vel - vel_n;
                    vel_n *= -0.2f;
                    //p.Velocity.xyz = reflect(p.Velocity.xyz, surfaceNormal) * 0.2f;
                    p.Velocity.xyz = vel_h + vel_n;
                }
            }
        }
#endif // DEPTHCOLLISIONS   
        
        p.Rotation += elapsedSeconds * 0.1f;
        p.Position += elapsedSeconds * p.Velocity;
        
        p.LifeRemaining -= elapsedSeconds;
        
        particlePool[poolIndex] = p;
    
        int nextAliveCount;
        InterlockedAdd(counters[1], 1, nextAliveCount);
        aliveList1[nextAliveCount] = poolIndex;
    }
}