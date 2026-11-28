// ScreenSurface members: shared Studio/Runtime minimap and full artwork map.
wi::graphics::Texture minimapTexture;
std::vector<float> minimapSettings;std::string minimapImageKey;
std::function<const wi::graphics::Texture*(const renegade::bridge::StableId&)> minimapProvider;
float mapPanX=0,mapPanZ=0,mapZoom=1,mapPointerX=0,mapPointerY=0;
bool mapDragging=false;
renegade::bridge::ScreenNavigation EffectiveMinimap()const
{
    auto c=*authored.navigation;c.minimap->range/=mapZoom;return c;
}
renegade::bridge::MinimapFrame MinimapFrameFor(const renegade::bridge::ScreenNavigation& c)const
{
    using namespace renegade::bridge;
    NavigationObserver camera{true,{},0},player=camera;
    std::vector<NavigationTarget> empty;std::map<std::string,NavigationPoint> entities;
    if(navigationSnapshot){camera=navigationSnapshot->camera;player=navigationSnapshot->player;}
    auto config=c;auto observer=c.headingSource==NavigationHeadingSource::Camera?camera:player;
    if(c.minimap->fullMap){observer.position.x=(c.minimap->west+c.minimap->east)*.5f;observer.position.z=(c.minimap->south+c.minimap->north)*.5f;}
    observer.position.x+=mapPanX;observer.position.z+=mapPanZ;
    config.headingSource=NavigationHeadingSource::Camera;camera=observer;
    return BuildMinimapFrame(config,camera,player,navigationSnapshot?navigationSnapshot->targets:empty,
        navigationSnapshot?navigationSnapshot->entities:entities,authored.rect.width,authored.rect.height);
}
void MinimapInput(bool enabled)
{
    if(!enabled||!IsVisible()||!authored.navigation||!authored.navigation->minimap||!authored.navigation->minimap->fullMap){mapDragging=false;return;}
    const auto pointer=wi::input::GetPointer();
    const bool inside=pointer.x>=translation.x&&pointer.x<translation.x+scale.x&&pointer.y>=translation.y&&pointer.y<translation.y+scale.y;
    if(inside&&pointer.z!=0)mapZoom=std::clamp(mapZoom*std::pow(1.2f,std::clamp(pointer.z,-4.f,4.f)),.25f,32.f);
    const bool down=wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);
    if(down&&(inside||mapDragging)){
        if(mapDragging){const auto c=EffectiveMinimap();const float unit=2*c.minimap->range/std::max(1.f,scale.y);
            mapPanX-=(pointer.x-mapPointerX)*unit;mapPanZ+=(pointer.y-mapPointerY)*unit;
            mapPanX=std::clamp(mapPanX,-1e7f,1e7f);mapPanZ=std::clamp(mapPanZ,-1e7f,1e7f);}
        mapDragging=true;mapPointerX=pointer.x;mapPointerY=pointer.y;
    }else mapDragging=false;
}
void UpdateMinimap(wi::graphics::Shader& shader)
{
    if(!IsVisible()||!authored.navigation||!authored.navigation->minimap)return;
    using namespace renegade::bridge;using namespace wi::graphics;
    const auto c=EffectiveMinimap();const auto& m=*c.minimap;const auto frame=MinimapFrameFor(c);
    const Texture* source=nullptr;
    if(m.live&&minimapProvider)source=minimapProvider(authored.id);
    if(!m.live){const auto art=navigationArtwork.find(m.image);if(art!=navigationArtwork.end())source=&art->second.GetTexture();}
    if(!source||!source->IsValid()||!frame.available){minimapTexture={};return;}
    auto* device=GetDevice();if(!shader.IsValid()&&!device->CreateShader(ShaderStage::CS,kScreenMinimap,sizeof(kScreenMinimap),&shader))return;
    const float aspect=m.round?1:authored.rect.width/std::max(1.f,authored.rect.height);
    const unsigned width=std::max(1u,unsigned(m.resolution*std::min(1.f,aspect))),height=std::max(1u,unsigned(m.resolution*std::min(1.f,1/aspect)));
    if(!minimapTexture.IsValid()||minimapTexture.GetDesc().width!=width||minimapTexture.GetDesc().height!=height){
        TextureDesc desc;desc.width=width;desc.height=height;desc.format=Format::R8G8B8A8_UNORM;
        desc.bind_flags=BindFlag::SHADER_RESOURCE|BindFlag::UNORDERED_ACCESS;
        if(!device->CreateTexture(&desc,nullptr,&minimapTexture))return;minimapSettings.clear();
    }
    const float angle=frame.heading*3.141592653589793f/180;
    const float settings[]={frame.observer.position.x,frame.observer.position.z,std::cos(angle),std::sin(angle),
        m.west,m.east,m.south,m.north,m.range,aspect,m.live?1.f:0.f,0};
    const std::vector<float> key(std::begin(settings),std::end(settings));
    if(!m.live&&minimapSettings==key&&minimapImageKey==m.image)return;
    minimapSettings=key;minimapImageKey=m.image;
    auto cmd=device->BeginCommandList();auto barrier=GPUBarrier::Image(&minimapTexture,ResourceState::SHADER_RESOURCE,ResourceState::UNORDERED_ACCESS);
    device->Barrier(&barrier,1,cmd);device->BindComputeShader(&shader,cmd);device->BindResource(source,0,cmd);device->BindUAV(&minimapTexture,0,cmd);
    device->PushConstants(settings,sizeof(settings),cmd);device->Dispatch((width+7)/8,(height+7)/8,1,cmd);
    barrier=GPUBarrier::Image(&minimapTexture,ResourceState::UNORDERED_ACCESS,ResourceState::SHADER_RESOURCE);device->Barrier(&barrier,1,cmd);
}
void DrawMinimap(const wi::Canvas& canvas,wi::graphics::CommandList cmd,wi::graphics::Rect clipped,
    const renegade::bridge::ScreenWidgetStyle& style)const
{
    using namespace renegade::bridge;const auto c=EffectiveMinimap();const auto& m=*c.minimap;
    float w=scale.x,h=scale.y,x=translation.x,y=translation.y;
    if(m.round){const float size=std::min(w,h);x+=(w-size)*.5f;y+=(h-size)*.5f;w=h=size;}
    if(w<1||h<1)return;
    const float unit=std::min(scaleX,scaleY),border=std::max(0.f,style.borderWidth*unit);
    DrawSurface(nullptr,x,y,w,h,m.round?w*.5f:std::min(8*unit,style.cornerRadius*unit),ToWickedColor(style.borderColor,style.opacity),cmd);
    x+=border;y+=border;w-=2*border;h-=2*border;if(w<=0||h<=0)return;
    const float radius=m.round?std::min(w,h)*.5f:std::max(0.f,std::min(8*unit,style.cornerRadius*unit)-border);
    DrawSurface(nullptr,x,y,w,h,radius,ToWickedColor(style.normal.background,style.opacity),cmd);
    if(minimapTexture.IsValid()){
        wi::image::Params p(x,y,w,h,wi::Color(255,255,255,uint8_t(255*style.opacity)));
        ApplyRounding(p,radius);wi::image::Draw(&minimapTexture,p,cmd);
    }
    clipped.left=std::max(clipped.left,int(x));clipped.top=std::max(clipped.top,int(y));
    clipped.right=std::min(clipped.right,int(x+w));clipped.bottom=std::min(clipped.bottom,int(y+h));ApplyScissor(canvas,clipped,cmd);
    const auto frame=MinimapFrameFor(c);
    const auto label=[&](const std::string& text,float px,float py,float size,wi::Color colour){
        wi::font::Params f(px,py,std::max(1,int(size*unit*style.text.fontSize/18.f)));f.style=fontStyle;f.color=colour;
        f.h_align=wi::font::WIFALIGN_CENTER;f.v_align=wi::font::WIFALIGN_CENTER;
        f.shadowColor=ToWickedColor(style.text.shadowColor,style.opacity);f.shadow_offset_x=style.text.shadowOffsetX*unit;f.shadow_offset_y=style.text.shadowOffsetY*unit;f.shadow_softness=style.text.shadowSoftness;f.shadow_bolden=style.text.shadowBolden;wi::font::Draw(text,f,cmd);
    };
    const auto white=wi::Color(255,255,255,uint8_t(255*style.opacity));
    const auto textColour=ToWickedColor(style.normal.foreground,style.opacity);
    if(!frame.available){label("No active player",x+w*.5f,y+h*.5f,14,textColour);return;}
    if(!minimapTexture.IsValid()){
        for(int i=1;i<8;++i){const float f=i/8.f;
            const float span=m.round?std::sqrt(std::max(0.f,1-(2*f-1)*(2*f-1))):1;
            DrawSurface(nullptr,x+w*(.5f-span*.5f),y+h*f,w*span,unit,0,wi::Color(60,70,80,100),cmd);
            DrawSurface(nullptr,x+w*f,y+h*(.5f-span*.5f),unit,h*span,0,wi::Color(60,70,80,100),cmd);}
        if(!navigationSnapshot||navigationPreview)label(m.live?"LIVE VIEW IN TEST GAME":"CHOOSE MAP ARTWORK",x+w*.5f,y+h*.3f,11,textColour);
    }
    const auto image=[&](const std::string& path,float px,float py,float size,float angle=0){
        const auto i=navigationArtwork.find(path);if(i==navigationArtwork.end()||!i->second.IsValid())return false;
        const auto& texture=i->second.GetTexture();const auto d=texture.GetDesc();
        const auto rect=FitScreenImage({px-size*.5f,py-size*.5f,size,size},float(d.width),float(d.height),ScreenImageFit::Contain);
        wi::image::Params p(px,py,rect.width,rect.height,white);p.pivot={.5f,.5f};p.rotation=angle;wi::image::Draw(&texture,p,cmd);return true;
    };
    for(const auto& marker:frame.markers){
        const auto& t=marker.target;const float px=x+marker.x*w,py=y+marker.y*h,size=c.markerSize*unit;
        auto i=c.icons.find(t.category);if(i==c.icons.end())i=c.icons.find("default");
        const std::string path=!t.icon.empty()?t.icon:i==c.icons.end()?"":i->second;
        if(!image(path,px,py,size)){
            const auto colour=t.category=="enemy"?wi::Color(245,65,50,255):
                (t.category=="vendor"?wi::Color(50,210,195,255):wi::Color(255,202,72,255));
            if(t.category=="enemy"){
                wi::image::Params p(px,py,size*.55f,size*.55f,colour);p.pivot={.5f,.5f};p.rotation=XM_PIDIV4;wi::image::Draw(nullptr,p,cmd);
            }else{
                DrawSurface(nullptr,px-size*.4f,py-size*.4f,size*.8f,size*.8f,size*.4f,colour,cmd);
                label(t.category=="vendor"?"$":(t.category=="quest"||t.category=="questgiver")?"!":"+",px,py,13,wi::Color(12,18,24,255));
            }
        }
        if((c.showNames||c.showDistance)&&(!m.round||std::hypot(marker.x-.5f,marker.y-.5f)<.30f)){
            std::string text=c.showNames?t.label:"";if(text.size()>24)text=text.substr(0,21)+"...";
            if(c.showDistance)text+=(text.empty()?"":" ")+std::to_string(int(marker.distance))+" m";
            label(text,std::clamp(px,x+std::min(w*.35f,60*unit),x+w-std::min(w*.35f,60*unit)),std::min(y+h-10*unit,py+size*.7f),11,textColour);
        }
    }
    if(m.showPlayer&&(!navigationSnapshot||navigationSnapshot->player.available)){
        const auto player=navigationSnapshot?navigationSnapshot->player.position:NavigationPoint{};
        const float a=frame.heading*XM_PI/180,dx=player.x-frame.observer.position.x,dz=player.z-frame.observer.position.z;
        const float size=m.playerSize*unit,px=x+w*.5f+(std::cos(a)*dx-std::sin(a)*dz)/(2*m.range*(m.round?1:w/h))*w,
            py=y+h*.5f-(std::sin(a)*dx+std::cos(a)*dz)/(2*m.range)*h,angle=frame.playerHeading*XM_PI/180;
        if(!image(m.playerIcon,px,py,size,angle)){
            const auto cyan=wi::Color(80,220,255,255);
            for(float side:{-1.f,1.f}){
                const float bx=side*size*.17f,by=size*.06f;
                wi::image::Params p(px+std::cos(angle)*bx-std::sin(angle)*by,py+std::sin(angle)*bx+std::cos(angle)*by,size*.18f,size*.75f,cyan);
                p.pivot={.5f,.5f};p.rotation=angle-side*.5f;wi::image::Draw(nullptr,p,cmd);
            }
        }
    }
    const float north=-frame.heading*XM_PI/180;
    label("N",x+w*.5f+std::sin(north)*w*.42f,y+h*.5f-std::cos(north)*h*.42f,12,textColour);
    if(m.mapKeyEnabled&&!m.fullMap)label("M  MAP",x+w*.5f,y+h*.88f,10,textColour);
    if(m.fullMap)label("Drag to move   |   Wheel to zoom   |   M / Esc to close",x+w*.5f,y+h-16*unit,13,textColour);
}
