// Test-only GPU field extraction. Never installed in Studio/Runtime packages.
#include "globals.hlsli"
#include "volumetricCloudsHF.hlsli"
Texture3D<float4> shapeNoise : register(t0);
Texture3D<float4> detailNoise : register(t1);
Texture2D<float4> curlNoise : register(t2);
Texture2D<float4> weatherMap : register(t3);
RWTexture3D<float> densityVolume : register(u0);
RWTexture2D<float4> parameterAudit : register(u1);
[numthreads(4,4,4)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint w,h,d;densityVolume.GetDimensions(w,h,d);
    if(any(id>=uint3(w,h,d))) return;
    float3 uv=(float3(id)+.5)/float3(w,h,d);
    float3 p=float3((uv.x-.5)*12000,
        GetWeather().volumetric_clouds.cloudStartHeight+
        uv.y*GetWeather().volumetric_clouds.cloudThickness,
        (uv.z-.5)*12000);
    float heightFraction=GetHeightFractionForPoint(GetWeather().atmosphere,p);
    LayerParameters layer=SampleLayerParameters(GetWeather().volumetric_clouds.layerFirst);
    if(all(id==0))
    {
        parameterAudit[uint2(0,0)]=float4(layer.windOffset,GetFrame().time);
        parameterAudit[uint2(1,0)]=float4(GetWeather().volumetric_clouds.cloudStartHeight,
            GetWeather().volumetric_clouds.cloudThickness,layer.layer.coverageAmount,layer.layer.windUpAmount);
    }
    float3 weather=SampleWeather(weatherMap,p,heightFraction,layer);
    densityVolume[id]=SampleCloudDensity(shapeNoise,detailNoise,curlNoise,p,
        heightFraction,layer,weather,0,true);
}
