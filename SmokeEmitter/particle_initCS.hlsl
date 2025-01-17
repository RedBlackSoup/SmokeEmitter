#include "globals.hlsli"

RWBuffer<int> deadList : register(u3);
RWBuffer<int> counters : register(u5);


[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    counters[0] = 0;                // currentAliveCount = 0;
    counters[1] = 0;                // nextAliveCount = 0
    counters[2] = PARTICLE_POOL_SIZE; // deadCount = poolsize
    counters[3] = 0;
    
    counters[8] = 1; // merge width
        
        for (int i = 0; i < PARTICLE_POOL_SIZE; i++)
    {
        deadList[i] = i;
    }
}