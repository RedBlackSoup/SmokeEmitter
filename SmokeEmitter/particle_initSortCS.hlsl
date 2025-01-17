#include "globals.hlsli"

RWBuffer<int> counters : register(u5);

[numthreads(1, 1, 1)]
void CSMain( uint3 DTid : SV_DispatchThreadID )
{
    int width = counters[8];
    counters[8] = width * 2;
}