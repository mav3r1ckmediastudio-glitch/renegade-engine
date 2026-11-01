#pragma once
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>

// Local Gate 1 evidence only: callers must gate this to their isolated QA project.
// Native archives preserve the scene/metadata; this sidecar preserves the actual
// non-serialized editor camera and sky-relevant render settings.
namespace renegade::cloudqa
{
struct ReplayState
{
    uint32_t width=0,height=0,cloudWidth=0,cloudHeight=0;
    float cameraWidth=0,cameraHeight=0,nearPlane=0,farPlane=0,fov=0;
    XMFLOAT4X4 inverseView{};
    float exposure=1,brightness=0,contrast=1,saturation=1,bloomThreshold=1;
    bool bloom=false,fxaa=false,eyeAdaption=false,motionBlur=false,depthOfField=false;
    bool lightShafts=false,taa=false,referenceQuality=false;
    int tonemap=0;
    float focalLength=1,aperture=0,apertureX=1,apertureY=1;
    float dofStrength=10,motionBlurStrength=100,eyeKey=.115f,eyeRate=1;
    float lightStrength=.5f,lightFade=3;
    bool colorGrading=true;
    bool Write(const std::filesystem::path& path) const
    {
        std::ofstream f(path);f<<std::setprecision(std::numeric_limits<float>::max_digits10);
        f<<2<<' '<<width<<' '<<height<<' '<<cloudWidth<<' '<<cloudHeight<<'\n'
         <<cameraWidth<<' '<<cameraHeight<<' '<<nearPlane<<' '<<farPlane<<' '<<fov<<'\n';
        for(int row=0;row<4;++row)for(int col=0;col<4;++col)f<<inverseView.m[row][col]<<' ';
        f<<'\n'<<exposure<<' '<<brightness<<' '<<contrast<<' '<<saturation<<' '<<bloomThreshold<<'\n'
         <<bloom<<' '<<fxaa<<' '<<eyeAdaption<<' '<<motionBlur<<' '<<depthOfField<<' '
         <<lightShafts<<' '<<taa<<' '<<referenceQuality<<' '<<tonemap<<'\n'
         <<focalLength<<' '<<aperture<<' '<<apertureX<<' '<<apertureY<<' '
         <<dofStrength<<' '<<motionBlurStrength<<' '<<eyeKey<<' '<<eyeRate<<' '
         <<lightStrength<<' '<<lightFade<<' '<<colorGrading<<'\n';
        return bool(f);
    }
    bool Read(const std::filesystem::path& path)
    {
        std::ifstream f(path);int version=0;
        f>>version>>width>>height>>cloudWidth>>cloudHeight
         >>cameraWidth>>cameraHeight>>nearPlane>>farPlane>>fov;
        for(int row=0;row<4;++row)for(int col=0;col<4;++col)f>>inverseView.m[row][col];
        f>>exposure>>brightness>>contrast>>saturation>>bloomThreshold
         >>bloom>>fxaa>>eyeAdaption>>motionBlur>>depthOfField>>lightShafts
         >>taa>>referenceQuality>>tonemap;
        if(version==2)f>>focalLength>>aperture>>apertureX>>apertureY
            >>dofStrength>>motionBlurStrength>>eyeKey>>eyeRate>>lightStrength>>lightFade>>colorGrading;
        return bool(f)&&(version==1||version==2)&&width>0&&height>0&&width<=8192&&height<=8192
            &&cameraWidth>0&&cameraHeight>0&&nearPlane>0&&farPlane>nearPlane;
    }
    void Apply(wi::RenderPath3D& render,wi::scene::CameraComponent& camera) const
    {
        camera.CreatePerspective(cameraWidth,cameraHeight,nearPlane,farPlane,fov);
        camera.focal_length=focalLength;camera.aperture_size=aperture;
        camera.aperture_shape=XMFLOAT2(apertureX,apertureY);
        camera.TransformCamera(XMLoadFloat4x4(&inverseView));camera.UpdateCamera();
        render.setExposure(exposure);render.setBrightness(brightness);
        render.setContrast(contrast);render.setSaturation(saturation);
        render.setBloomThreshold(bloomThreshold);render.setBloomEnabled(bloom);
        render.setFXAAEnabled(fxaa);render.setEyeAdaptionEnabled(eyeAdaption);
        render.setMotionBlurEnabled(motionBlur);render.setDepthOfFieldEnabled(depthOfField);
        render.setLightShaftsEnabled(lightShafts);render.setTonemap(wi::renderer::Tonemap(tonemap));
        render.setDepthOfFieldStrength(dofStrength);render.setMotionBlurStrength(motionBlurStrength);
        render.setEyeAdaptionKey(eyeKey);render.setEyeAdaptionRate(eyeRate);
        render.setLightShaftsStrength(lightStrength);render.setLightShaftsFadeSpeed(lightFade);
        render.setColorGradingEnabled(colorGrading);
        wi::renderer::SetTemporalAAEnabled(taa);
    }
};
inline bool SaveStudioReplay(wi::RenderPath3D& render,
    wi::scene::Scene& scene,const std::filesystem::path& folder)
{
    if(!render.camera || scene.weather.volumetricCloudParameters.animationMultiplier!=0) return false;
    const auto& result=render.GetRenderResult3D();
    const auto& clouds=render.volumetriccloudResources;
    const auto& opacity=clouds.texture_reproject[clouds.GetTemporalOutputIndex()];
    if(!result.IsValid()||!opacity.IsValid()) return false;
    std::filesystem::create_directories(folder);
    ReplayState state;state.width=result.desc.width;state.height=result.desc.height;
    state.cloudWidth=opacity.desc.width;state.cloudHeight=opacity.desc.height;
    const auto& camera=*render.camera;
    state.cameraWidth=camera.width;state.cameraHeight=camera.height;
    state.nearPlane=camera.zNearP;state.farPlane=camera.zFarP;state.fov=camera.fov;
    state.inverseView=camera.InvView;
    state.exposure=render.getExposure();state.brightness=render.getBrightness();
    state.contrast=render.getContrast();state.saturation=render.getSaturation();
    state.bloomThreshold=render.getBloomThreshold();state.bloom=render.getBloomEnabled();
    state.fxaa=render.getFXAAEnabled();state.eyeAdaption=render.getEyeAdaptionEnabled();
    state.motionBlur=render.getMotionBlurEnabled();state.depthOfField=render.getDepthOfFieldEnabled();
    state.lightShafts=render.getLightShaftsEnabled();
    state.taa=wi::renderer::GetTemporalAAEnabled();
    state.referenceQuality=wi::arguments::HasArgument("cloud-reference-quality");
    state.tonemap=int(render.getTonemap());
    state.focalLength=camera.focal_length;state.aperture=camera.aperture_size;
    state.apertureX=camera.aperture_shape.x;state.apertureY=camera.aperture_shape.y;
    state.dofStrength=render.getDepthOfFieldStrength();state.motionBlurStrength=render.getMotionBlurStrength();
    state.eyeKey=render.getEyeAdaptionKey();state.eyeRate=render.getEyeAdaptionRate();
    state.lightStrength=render.getLightShaftsStrength();state.lightFade=render.getLightShaftsFadeSpeed();
    state.colorGrading=render.getColorGradingEnabled();
    auto* device=wi::graphics::GetDevice();device->WaitForGPU();
    wi::Archive archive((folder/"scene.wiscene").generic_string(),false,false);
    if(!archive.IsOpen())return false;
    archive.SetCompressionEnabled(true);scene.Serialize(archive);
    if(!archive.SaveFile((folder/"scene.wiscene").generic_string()))return false;
    archive=wi::Archive();
    const bool saved=state.Write(folder/"state.txt")&&
        wi::helper::saveTextureToFile(result,(folder/"studio.png").generic_string())&&
        wi::helper::saveTextureToFile(opacity,(folder/"studio-opacity.png").generic_string());
    std::ofstream info(folder/"capture.txt");
    const auto& c=scene.weather.volumetricCloudParameters;
    info<<std::setprecision(9)<<"saved="<<saved<<"\nadapter="<<device->GetAdapterName()
        <<"\nshader_format="<<int(device->GetShaderFormat())
        <<"\nformation="<<c.layerFirst.padding0<<"\ncoverage="<<c.layerFirst.coverageAmount
        <<"\nbase="<<c.cloudStartHeight<<"\ndepth="<<c.cloudThickness
        <<"\nsecondary_coverage="<<c.layerSecond.coverageAmount
        <<"\nanimation="<<c.animationMultiplier<<"\neye="<<camera.Eye.x<<','<<camera.Eye.y<<','<<camera.Eye.z
        <<"\nfov_radians="<<camera.fov<<"\n";
    return saved&&bool(info);
}
}
