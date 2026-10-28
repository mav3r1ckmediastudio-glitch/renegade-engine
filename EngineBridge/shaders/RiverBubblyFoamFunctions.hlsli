// G8C optional visual proof. World-metric cells; no density/source generation.
#ifndef RENEGADE_BUBBLY_FOAM_FUNCTIONS
#define RENEGADE_BUBBLY_FOAM_FUNCTIONS
float4 RiverBubbleCell(float2 p, float time)
{
    float2 cell=floor(p),f=frac(p),nearest=0;
    float best=1e10,seed=0;
    [unroll] for(int y=-1;y<=1;++y)
    [unroll] for(int x=-1;x<=1;++x)
    {
        float2 id=cell+float2(x,y);
        float2 h=frac(sin(float2(dot(id,float2(127.1,311.7)),dot(id,float2(269.5,183.3))))*43758.5453);
        float2 delta=f-(float2(x,y)+.18+.64*h);
        float distanceSquared=dot(delta,delta);
        if(distanceSquared<best){best=distanceSquared;nearest=delta;seed=h.x;}
    }
    // Small smooth deformation of existing cells, never random frame reseeding.
    float radius=lerp(.35,.49,seed)*(1+.065*sin(time*3.0+seed*6.2831853));
    float2 r=nearest/radius;
    float cap=saturate(1-dot(r,r));
    float2 slope=r*(.85*smoothstep(0,.3,cap));
    return float4(slope,cap,sqrt(best)/radius);
}
#endif
