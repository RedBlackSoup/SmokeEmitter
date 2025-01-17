#include "globals.hlsli"

RWStructuredBuffer<Particle> particlePool : register(u0);
RWBuffer<int> aliveList0 : register(u1);
RWBuffer<int> counters : register(u5);
RWBuffer<int> sortedList : register(u6);

void merge(int left, int middle, int right)
{
    int i = left;
    int j = middle;
    int k = left;
    
    int currentAliveCount = counters[0];
    while (i < middle && j < right)
    {
        float dist[2];
        
        
        dist[0] = dot(particlePool[aliveList0[i]].Position.xyz - viewPos.xyz, lookDirection);
        dist[1] = dot(particlePool[aliveList0[j]].Position.xyz - viewPos.xyz, lookDirection);
        
        if(i >= currentAliveCount)
        {
            dist[0] *= -1.0f;
        }
        if(j >= currentAliveCount)
        {
            dist[1] *= -1.0f;
        }
        
        if (dist[0] > dist[1])
            sortedList[k++] = aliveList0[i++];
        else
            sortedList[k++] = aliveList0[j++];
    }
    
    while (i < middle)
        sortedList[k++] = aliveList0[i++];
    while (j < right)
        sortedList[k++] = aliveList0[j++];

    for (int x = left; x < right; x++)
        aliveList0[x] = sortedList[x];
}

[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    
    uint tid = DTid.x; // global thread id
    
    int n = PARTICLE_POOL_SIZE;
    int width = counters[8];
    
    int left = tid * width;
    int middle = left + width / 2;
    int right = left + width;

    if (left < n && middle < n)
    {
        merge(left, middle, right);
    }
        
    AllMemoryBarrierWithGroupSync();
}