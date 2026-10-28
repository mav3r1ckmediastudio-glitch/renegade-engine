"""G13 standalone placed-waterfall shader: native unmodified Wicked shaders staged separately."""
from pathlib import Path
import shutil,sys
root=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]).resolve()
shutil.copytree(root/'WickedEngine/WickedEngine/shaders',out,dirs_exist_ok=True)
p=out/'objectHF.hlsli'
text=p.read_text()
needle='#ifdef UNLIT\n\tcolor = surface.baseColor;\n#endif // UNLIT'
assert text.count(needle)==1,"Native objectHF layout has changed; explicit review required"
effect=r"""
#ifdef PLACEABLE_WATERFALL
    // Original Renegade G13B multi-scale advected optical density.
    // Soft transmittance throughout: no binary ropes, alpha test or additive glow.
    float u=saturate(uvsets.x),v=saturate(uvsets.y);
    float t=asfloat(material.userdata.w);
    uint layer=material.userdata.z&127;
    float turbulence=float(material.userdata.z>>8)*.001;
    float width=max(.3,asfloat(material.userdata.x));
    float drop=max(.5,asfloat(material.userdata.y));
    float phase=float(layer)*3.731;
    float slowTime=t*.24;
    float shift=noise_simplex_2D(float2(u*3.7,uvsets.w*2-t*.5)+phase)*.014*turbulence;
    float phase0=frac(slowTime+layer*.173),phase1=frac(phase0+.5);
    float w0=1-abs(phase0*2-1);
    float repeat=drop/max(.5,width*1.5);
    float2 a=float2(u+shift,uvsets.y*repeat-phase0);
    float2 b=float2(u-shift,uvsets.y*repeat-phase1);
    if((material.userdata.z&128)!=0){
        // Crest breakup R2. Do NOT repeat the same texture seven times.
        // Individual source stream bands have deliberately unequal widths and
        // a coherent, downward-advecting warp, not identical mirrored ropes.
        float2 warped=float2(u+shift*.4,uvsets.y*2);
        float irregular=noise_simplex_2D(float2(u*4.9,uvsets.w*1.9-t*.55));
        warped.x+=.038*irregular*smoothstep(.04,.28,v);
        a=float2(warped.x*2.05,warped.y-phase0*1.45);
        b=float2(warped.x*1.88+.19,warped.y-phase1*1.16);
    }
    float4 water0=material.textures[BASECOLORMAP].Sample(sampler_objectshader,a.xyxy);
    float4 water1=material.textures[BASECOLORMAP].Sample(sampler_objectshader,b.xyxy);
    float4 water=water0*w0+water1*(1-w0);
    float edge=smoothstep(0,.075,u)*(1-smoothstep(.925,1,u));
    float crest=smoothstep(0,.025,v),tail=1-smoothstep(.88,1,v);
    // Detached streams must reach their physical landing, not fade over 12% of total drop.
    if((material.userdata.z&128)!=0)tail=1-smoothstep(1-.06/drop,1,v);
    float facing=smoothstep(.08,.60,abs(dot(surface.N,surface.V)));
    if((material.userdata.z&128)!=0)facing=.24+.76*abs(dot(surface.N,surface.V));
    float optical=.88;
    // Coverage comes from the art alpha. Never composite its grey/black RGB matte.
    color.rgb=float3(.82,.86,.88);
    float photographicDensity=pow(saturate(dot(water.rgb,float3(.2126,.7152,.0722))),1.6);
    color.a=saturate(water.a*photographicDensity*edge*crest*tail*optical*facing);
    if((material.userdata.z&128)!=0){
        float breakup=saturate(.6+.5*noise_simplex_2D(float2(u*93,uvsets.w*13-t*3)));
        color.a*=lerp(.48,.78,smoothstep(.74,.98,v))*lerp(1.0,breakup,smoothstep(.16,.4,v));
        // Seven visually distinct source streams: deliberately asymmetric
        // widths, gaps, and local deflection. They rejoin/fragment over rock.
        // The underlying collision mesh/trace stays continuous and unaltered.
        static const float spans[7]={.223,.064,.184,.079,.251,.081,.118};
        static const float fills[7]={.87,.58,.92,.68,.89,.56,.84};
        static const float offsets[7]={-.12,.24,-.19,.16,.07,-.23,.14};
        float cursor=0,channelCoverage=0;
        [unroll] for(int n=0;n<7;n++){
            float center=cursor+.5*spans[n];
            float acrossWarp=.16*spans[n]*sin(uvsets.w*(1.7+.23*n)-t*(.7+.08*n)+offsets[n]*6);
            float localWidth=.5*spans[n]*fills[n];
            float distance=abs(u-center-acrossWarp);
            float strand=1-smoothstep(localWidth*.74,localWidth*1.18,distance);
            channelCoverage=max(channelCoverage,strand);
            cursor+=spans[n];
        }
        float upper=1-smoothstep(.58,.84,v);
        // Keep the crest water connected: pale translucent gaps, not clip holes.
        color.a*=lerp(1.0,lerp(.45,1.0,channelCoverage),upper);
        float speedVariation=noise_simplex_2D(float2(u*6.3,uvsets.w*.91-t*.44));
        color.a*=lerp(1.0,.83+.17*saturate(.5+.5*speedVariation),upper);

    }
    // Native scene-depth fade prevents hard contact seams on independent geometry.
    if(GetCamera().texture_depth_index>=0){
        float2 screen=input.pos.xy*GetCamera().internal_resolution_rcp;
        float depth=compute_lineardepth(texture_depth.SampleLevel(sampler_point_clamp,screen,0));
        float separation=max(0,depth-input.pos.w);
        float contactFade=(material.userdata.z&128)!=0?.05:max(.08,min(.4,width*.08));
        color.a*=saturate(separation/contactFade);
    }
#endif
"""
text=text.replace(needle,needle+'\n'+effect,1)
vertex_anchor='\tOut.pos = surface.position;'
assert text.count(vertex_anchor)==1
vertex_effect=r"""
#ifdef PLACEABLE_WATERFALL_VERTEX
    if((material.userdata.z&128)!=0){
        float t=asfloat(material.userdata.w),u=surface.uvsets.x;
        float travel=surface.uvsets.w;
        float detached=saturate(surface.uvsets.z-1);
        float anchor=smoothstep(0,.06,surface.uvsets.y)*(1-smoothstep(.91,1,surface.uvsets.y));
        float amplitude=.022*detached*anchor;
        float pulse=sin(travel*16-t*5+u*29)+.4*sin(travel*29-t*7-u*53);
        surface.position.xyz+=surface.normal*amplitude*pulse;
        surface.position.xz+=amplitude*float2(sin(travel*8-t*2+u*31),cos(travel*11-t*2.7+u*43));
    }
#endif
"""
text=text.replace(vertex_anchor,vertex_effect+'\n'+vertex_anchor,1)

p.write_text(text)
(out/'PlaceableWaterfallPS.hlsl').write_text(
    '#define TRANSPARENT\n#define UNLIT\n#define PLACEABLE_WATERFALL\n'
    '#define OBJECTSHADER_COMPILE_PS\n#define OBJECTSHADER_LAYOUT_COMMON\n'
    '#define TILEDFORWARD\n#include "objectHF.hlsli"\n')
(out/'PlaceableWaterfallVS.hlsl').write_text(
    '#define PLACEABLE_WATERFALL_VERTEX\n#define OBJECTSHADER_COMPILE_VS\n#define OBJECTSHADER_LAYOUT_COMMON\n'
    '#include "objectHF.hlsli"\n')
print('G13_STANDALONE_SHADER_STAGE_READY',out)
