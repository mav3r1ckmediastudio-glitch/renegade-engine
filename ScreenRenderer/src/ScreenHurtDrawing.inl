// Members of the shared ScreenSurface: Studio and Runtime use the same draw path.
renegade::bridge::HurtFeedbackFrame hurtFrame;
bool hurtReduced=false;
std::map<std::string,wi::Resource> hurtArtwork;
wi::graphics::Texture hurtGenericEdges;
struct HurtVoice {
    wi::Resource sound;wi::audio::SoundInstance voice;std::string path;
    void Stop(){if(voice.IsValid())wi::audio::Stop(&voice);}
    bool Play(const std::string& root,const std::string& clip,float volume){
        if(clip.empty()||volume<=0)return false;
        std::string absolute,error;if(!renegade::bridge::ResolveScreenResourcePath(root,clip,absolute,error))return false;
        if(path!=absolute||!sound.IsValid()){
            if(!renegade::bridge::ValidateAudioAssetForWicked(absolute,error))return false;
            auto resource=wi::resourcemanager::Load(absolute);
            if(!resource.IsValid()||!resource.GetSound().IsValid())return false;
            Stop();sound=std::move(resource);path=absolute;
        }
        Stop();wi::audio::SoundInstance next;next.type=wi::audio::SUBMIX_TYPE_SOUNDEFFECT;next.SetLooped(false);
        if(!wi::audio::CreateSoundInstance(&sound.GetSound(),&next))return false;
        voice=std::move(next);wi::audio::SetVolume(std::clamp(volume,0.f,1.f),&voice);wi::audio::Play(&voice);return true;
    }
    ~HurtVoice(){Stop();}
} hurtPain,hurtHeartbeat;
bool hurtObserved=false;
std::uint64_t hurtLastSequence=0;
float hurtLastPain=-100,hurtLastBeat=-100;
bool LoadHurtArtwork(const std::string& root,std::string& error)
{
    std::map<std::string,wi::Resource> loaded;if(!authored.hurt){hurtArtwork.clear();return true;}
    for(const auto& f:renegade::bridge::HurtResources())if(!f.audio){
        const auto& path=(*authored.hurt).*(f.member);if(path.empty())continue;
        const auto old=hurtArtwork.find(path);if(old!=hurtArtwork.end()){loaded.emplace(path,old->second);continue;}
        std::string absolute;if(!renegade::bridge::ResolveScreenResourcePath(root,path,absolute,error))return false;
        auto resource=wi::resourcemanager::Load(absolute);if(!resource.IsValid()||!resource.GetTexture().IsValid()){error="Cannot load hurt feedback artwork.";return false;}
        loaded.emplace(path,std::move(resource));
    }
    if(!hurtGenericEdges.IsValid()){
        constexpr unsigned size=256;std::vector<std::uint8_t> pixels(size*size*4);
        for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x){
            const float nx=2*(x+.5f)/size-1,ny=2*(y+.5f)/size-1;
            const float edge=std::clamp((std::sqrt(nx*nx+ny*ny)-.5f)/.85f,0.f,1.f);
            const size_t i=(y*size+x)*4;pixels[i]=pixels[i+1]=pixels[i+2]=255;pixels[i+3]=std::uint8_t(std::round(255*edge*edge));
        }
        wi::graphics::TextureDesc desc;desc.width=desc.height=size;desc.format=wi::graphics::Format::R8G8B8A8_UNORM;desc.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE;
        wi::graphics::SubresourceData initial;initial.data_ptr=pixels.data();initial.row_pitch=size*4;
        if(!wi::graphics::GetDevice()->CreateTexture(&desc,&initial,&hurtGenericEdges)){error="Cannot prepare hurt vignette.";return false;}
    }
    hurtArtwork=std::move(loaded);return true;
}
void SetHurtFrame(const renegade::bridge::HurtFeedbackFrame& frame,const std::string& root,bool allowAudio)
{
    hurtFrame=frame;
    if(!authored.hurt)return;const auto& c=*authored.hurt;
    if(!c.painEnabled||c.painSound.empty())hurtPain.Stop();
    if(!c.heartbeatEnabled||c.heartbeatSound.empty())hurtHeartbeat.Stop();
    if(!hurtObserved||frame.sequence<hurtLastSequence||frame.seconds<hurtLastPain){
        hurtLastSequence=frame.sequence;hurtLastPain=hurtLastBeat=-100;hurtObserved=true;
    }
    const bool audible=allowAudio&&IsVisible()&&c.enabled&&frame.available&&frame.alive&&c.intensity>0;
    if(!audible){hurtPain.Stop();hurtHeartbeat.Stop();hurtLastSequence=frame.sequence;return;}
    if(frame.sequence!=hurtLastSequence&&c.painEnabled&&frame.seconds-hurtLastPain>=c.painCooldown){
        if(hurtPain.Play(root,c.painSound,c.painVolume*c.intensity))hurtLastPain=frame.seconds;
    }
    hurtLastSequence=frame.sequence;
    if(c.heartbeatEnabled&&frame.healthFraction<c.lowThreshold){
        if(frame.seconds-hurtLastBeat>=c.heartbeatPeriod&&hurtHeartbeat.Play(root,c.heartbeatSound,c.heartbeatVolume*c.intensity))hurtLastBeat=frame.seconds;
    }else {hurtHeartbeat.Stop();hurtLastBeat=-100;}
}
void DrawHurtFeedback(const wi::Canvas&,wi::graphics::CommandList cmd,const renegade::bridge::ScreenWidgetStyle& style)const
{
    using namespace renegade::bridge;
    const auto& c=*authored.hurt;const auto sample=SampleHurtFeedback(c,hurtFrame,hurtReduced);
    const float x=translation.x,y=translation.y,w=scale.x,h=scale.y;
    const auto colour=[&](std::uint32_t rgba,float opacity){return wi::Color(uint8_t(rgba>>24),uint8_t(rgba>>16),uint8_t(rgba>>8),uint8_t((rgba&255)*std::clamp(opacity*style.opacity,0.f,1.f)));};
    const auto image=[&](const std::string& path,float opacity,std::uint32_t tint,float px,float py,float width,float height,float angle=0,bool centred=false){
        const auto found=hurtArtwork.find(path);if(opacity<=0||found==hurtArtwork.end())return false;
        wi::image::Params p(px,py,width,height,colour(tint,opacity));if(centred)p.pivot={.5f,.5f};p.rotation=angle;wi::image::Draw(&found->second.GetTexture(),p,cmd);return true;
    };
    const auto edges=[&](float opacity,std::uint32_t tint){
        if(opacity<=0||!hurtGenericEdges.IsValid())return;
        wi::image::Params p(x,y,w,h,colour(tint,opacity));wi::image::Draw(&hurtGenericEdges,p,cmd);
    };
    if(sample.flash>0&&!image(c.hitImage,sample.flash,c.colour,x,y,w,h))DrawSurface(nullptr,x,y,w,h,0,colour(c.colour,sample.flash),cmd);
    if(!image(c.lowImage,sample.low,c.lowColour,x,y,w,h))edges(sample.low,c.lowColour);
    if(!image(c.edgeImage,sample.edge,c.colour,x,y,w,h))edges(sample.edge,c.colour);
    const float size=c.directionSize*std::min(scaleX,scaleY),radius=c.directionRadius*std::min(w,h);
    for(const auto& [angle,opacity]:sample.directions){
        const float px=x+w*.5f+std::sin(angle)*radius,py=y+h*.5f-std::cos(angle)*radius;
        if(image(c.directionImage,opacity,c.colour,px,py,size,size,angle,true))continue;
        for(float side:{-1.f,1.f}){
            const float dx=side*size*.15f,dy=size*.05f;
            wi::image::Params p(px+std::cos(angle)*dx-std::sin(angle)*dy,py+std::sin(angle)*dx+std::cos(angle)*dy,size*.16f,size*.65f,colour(c.colour,opacity));
            p.pivot={.5f,.5f};p.rotation=angle-side*.65f;wi::image::Draw(nullptr,p,cmd);
        }
    }
}
