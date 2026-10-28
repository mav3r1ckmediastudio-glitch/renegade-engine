// G8F source-born connected occupancy. Evaluated only at actual foam sources.
// Cellular seams provide shared three-way junctions; never outline scene banks.
float2 WebHash(float2 id)
{
    return frac(sin(float2(dot(id,float2(127.1,311.7)),dot(id,float2(269.5,183.3))))*43758.5453);
}
float WebNetwork(float2 p,float time)
{
    // Smooth warp bends short shared edges without independently moving pixels.
    p+=.38*float2(sin(p.y*1.37+sin(p.x*.7)*.4+time*.21),cos(p.x*1.23+sin(p.y*.6)*.4-time*.17));
    float2 cell=floor(p),f=frac(p);
    float first=1e10,second=1e10,seed=0;
    [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x){
        float2 id=cell+float2(x,y),h=WebHash(id);
        float2 centre=float2(x,y)+.15+.7*h+.065*sin(time*.37+h*6.2831853);
        float distance=dot(f-centre,f-centre);
        if(distance<first){second=first;first=distance;seed=h.x;}
        else second=min(second,distance);
    }
    float gap=sqrt(second)-sqrt(first);
    float thickness=lerp(.12,.23,seed);
    return 1-smoothstep(thickness*.35,thickness,gap);
}
float NetworkAt(float2 world,float time)
{
    float main=WebNetwork(world*2.05,time);
    float small=WebNetwork(world*4.4+13.7,time*.83);
    // Smaller side branches gather near the primary connected seams.
    return saturate(max(main,small*.65*(.3+.7*main)));
}

float Birth(float2 world,float time,float2 flow)
{
    // Move the source pattern with the actual current during birth, so
    // distributed emission does not average its transverse links into stripes.
    // Two bounded lifetimes fade through zero at reset; no unbounded v*time warp.
    float period=1.5,phase0=frac(time/period),phase1=frac(time/period+.5);
    float weight=.5-.5*cos(phase0*6.2831853);
    return lerp(NetworkAt(world-flow*(phase1*period),time),
        NetworkAt(world-flow*(phase0*period),time),weight);
}
