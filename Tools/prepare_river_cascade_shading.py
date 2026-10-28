"""Independent native WATER falling-sheet extension; original owned art only."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1])
subprocess.run([sys.executable,str(root/'Tools/prepare_river_graded_shading.py'),str(out)],check=True)
p=out/'objectHF.hlsli';s=p.read_text()
anchor='        if (material.userdata.y == 0x52464c35)'
normal='''        if(material.userdata.y==0x43415343||material.userdata.y==0x43555052){
            // uv1.y is actual native-ribbon integral(ds / velocity), in seconds.
            // A constant phase therefore travels with accelerating 3D flow.
            float clock=frac(GetTime()/128)*128;
            float2 fallUV=float2(uvsets.z*3.2,(uvsets.w-clock)*2);
            float2 dx=ddx_coarse(fallUV),dy=ddy_coarse(fallUV);dy.y=max(abs(dy.y),min(GetDeltaTime(),.05));
            bumpColor0=2*texture_normalmap.SampleGrad(sampler_objectshader,fallUV,dx,dy).rg-1;
            bumpColor1=2*texture_normalmap.SampleGrad(sampler_objectshader,fallUV*1.37+float2(.31,.71),dx*1.37,dy*1.37).rg-1;
        }
'''
assert s.count(anchor)==1;s=s.replace(anchor,normal+anchor)
anchor='\tApplyLighting(surface, lighting, color);'
fall='''#ifdef WATER
    if(material.userdata.y==0x43415343){
        ByteAddressBuffer cascadeParams=bindless_buffers[descriptor_index(material.userdata.x)];
        uint4 art=cascadeParams.Load4(112);
        Texture2D<float4> artwork=bindless_textures[descriptor_index(art.x)];
        float clock=frac(GetTime()/128)*128;
        float2 artUV=float2(uvsets.z*8,(uvsets.w-clock)*1.5);
        float2 dx=ddx_coarse(artUV),dy=ddy_coarse(artUV);dy.y=max(abs(dy.y),min(GetDeltaTime(),.05)*.75);
        float mask=artwork.SampleGrad(sampler_linear_mirror,artUV,dx,dy).r;
        float fine=artwork.SampleGrad(sampler_linear_mirror,artUV*float2(1.71,.5)+float2(7.3,2.6),dx*float2(1.71,.5),dy*float2(1.71,.5)).r;
        mask=max(mask*.85,fine*.6);
        float progress=saturate(uvsets.w/max(.001,asfloat(material.userdata.z)));
        // Calibrated entrainment increases along a real gravity-driven fall.
        // Artwork modulates this fraction; it cannot paint dry/rock pixels.
        float entrainment=.08+.6*(1-exp(-max(0,-surface.P.y)*.8));
        float amount=asfloat(cascadeParams.Load(64));
        float coverage=pow(saturate((mask-.08)/.75),.7)*(1-exp(-entrainment*3))*saturate(amount*3);
        Surface bubbles=surface;bubbles.albedo=float3(.62,.645,.66);bubbles.f0=.02;bubbles.roughness=.8;
        bubbles.N=normalize(lerp(surface.N,normalize(input.nor),.7));bubbles.sss=0;bubbles.transmission=0;bubbles.refraction=0;bubbles.extinction=1;bubbles.emissiveColor=0;bubbles.update();
        Lighting bubbleLight;bubbleLight.create(0,0,GetAmbient(bubbles.N),0);TiledLighting(bubbles,bubbleLight,flat_tile_index,camera);
        half4 bubbleColor=half4(0,0,0,1);ApplyLighting(bubbles,bubbleLight,bubbleColor);
        color.rgb=lerp(color.rgb,bubbleColor.rgb,coverage);
    }
#endif
'''
assert s.count(anchor)==1;s=s.replace(anchor,anchor+'\n'+fall)
p.write_text(s)
print('CASCADE_TRAVEL_TIME_WATER_SHADING_STAGED')

# A refined derivative of the native sheet; lip, toe and banks stay attached.
p=out/'SurfaceObjectHF.hlsli';s=p.read_text();anchor='    if(material.userdata.y==0x52464c35)'
wave='''    if(material.userdata.y==0x43415343){
        ByteAddressBuffer cascadeMotion=bindless_buffers[descriptor_index(material.userdata.x)];if(cascadeMotion.Load(84)!=0){
        float4 uv=input.GetUVSets();float total=max(.001,asfloat(material.userdata.z));
        float u=saturate(uv.z),p=saturate(uv.w/total),time=frac(GetTime()/128)*128;
        float phase=6.2831853*(u*5+(uv.w-time)*1.25);
        float a=sin(3.14159265*u),b=sin(3.14159265*p),f=a*b;
        float height=.012*f*sin(phase);
        float dhdu=.012*(3.14159265*cos(3.14159265*u)*b*sin(phase)+f*31.4159265*cos(phase));
        float dhdt=.012*(a*3.14159265*cos(3.14159265*p)/total*sin(phase)+f*7.8539816*cos(phase));
        float speed=sqrt(1+2*9.80665*(.2+max(0,-surface.position.y)));
        float3 n=normalize(surface.normal),t=normalize(surface.tangent.xyz),across=normalize(cross(t,n));
        surface.position.xyz+=n*height;
        surface.normal=normalize(n-across*(dhdu/4)-t*(dhdt/speed));
        surface.tangent.xyz=normalize(t+n*(dhdt/speed));
        }
    }
'''
assert s.count(anchor)==1;s=s.replace(anchor,wave+anchor);p.write_text(s)
