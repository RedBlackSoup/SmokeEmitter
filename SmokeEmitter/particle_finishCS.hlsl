RWBuffer<int> aliveList0 : register(u1);
RWBuffer<int> aliveList1 : register(u2);
RWBuffer<int> deadList : register(u3);
RWBuffer<int> counters : register(u5);

[numthreads(1, 1, 1)]
void CSMain( uint3 DTid : SV_DispatchThreadID )
{
    if (DTid.x == 0)
    {
        counters[0] = counters[1];
        counters[1] = 0;
    }
    
    
    counters[8] = 1; // merge width
}