// ScreenSurface member implementation: the same drawing path in Studio and Runtime.
std::shared_ptr<const renegade::bridge::WorldNavigationSnapshot> navigationSnapshot;
std::map<std::string,wi::Resource> navigationArtwork;
bool navigationPreview=false;
bool LoadNavigationArtwork(const std::string& root,std::string& error,const std::vector<renegade::bridge::NavigationTarget>* liveTargets=nullptr)
{
    if(!authored.navigation){navigationArtwork.clear();return true;}
    auto config=*authored.navigation;std::map<std::string,wi::Resource> loaded;
    std::vector<const std::string*> paths;for(const auto* path:renegade::bridge::NavigationResources(config))paths.push_back(path);
    if(liveTargets)for(const auto& t:*liveTargets)paths.push_back(&t.icon);
    for(const auto* path:paths)if(!path->empty())
    {
        std::string absolute;if(!renegade::bridge::ResolveScreenResourcePath(root,*path,absolute,error))return false;
        auto resource=wi::resourcemanager::Load(absolute);
        if(!resource.IsValid()||!resource.GetTexture().IsValid()){error="Cannot load navigation artwork: "+*path;return false;}
        loaded.emplace(*path,std::move(resource));
    }
    navigationArtwork=std::move(loaded);minimapSettings.clear();return true;
}
void DrawNavigation(const wi::Canvas& canvas,wi::graphics::CommandList cmd,
    wi::graphics::Rect clipped,const renegade::bridge::ScreenWidgetStyle& style) const
{
    if(!authored.navigation)return;
    using namespace renegade::bridge;const auto& config=*authored.navigation;
    const float unit=std::min(scaleX,scaleY),x=translation.x+scale.x*.05f,y=translation.y,w=scale.x*.9f,h=scale.y;
    if(w<=0||h<=0)return;
    clipped.left=std::max(clipped.left,int(std::ceil(x)));clipped.right=std::min(clipped.right,int(std::floor(x+w)));
    clipped.top=std::max(clipped.top,int(std::ceil(y)));clipped.bottom=std::min(clipped.bottom,int(std::floor(y+h)));
    if(clipped.right<=clipped.left||clipped.bottom<=clipped.top)return;ApplyScissor(canvas,clipped,cmd);
    NavigationObserver c{true,{},config.manualHeading},p=c;std::vector<NavigationTarget> empty;std::map<std::string,NavigationPoint> entities;
    if(navigationSnapshot){c=navigationSnapshot->camera;p=navigationSnapshot->player;}
    const auto frame=BuildNavigationFrame(config,c,p,navigationSnapshot?navigationSnapshot->targets:empty,
        navigationSnapshot?navigationSnapshot->entities:entities,w/std::max(.001f,unit));
    const auto colour=ToWickedColor(style.normal.foreground,style.opacity);
    const auto label=[&](const std::string& text,float px,float py,int size,wi::font::Alignment alignment=wi::font::WIFALIGN_CENTER){
        wi::font::Params f(px,py,std::max(1,int(size*unit)));f.style=fontStyle;f.color=colour;f.bolden=style.text.bolden;
        f.h_align=alignment;f.v_align=wi::font::WIFALIGN_TOP;f.shadowColor=ToWickedColor(style.text.shadowColor,style.opacity);wi::font::Draw(text,f,cmd);
    };
    if(!frame.available){label("Navigation: no active player/camera",x+w*.5f,y+h*.35f,14);return;}
    const float pixels=w/config.fieldOfView;
    const auto artwork=[&](const std::string& path)->const wi::Resource*{const auto i=navigationArtwork.find(path);return i==navigationArtwork.end()?nullptr:&i->second;};
    if(config.showTicks)
    {
        if(const auto* strip=artwork(config.bearingStrip))
        {
            const float width=pixels*360,start=x+w*.5f-frame.heading*pixels;
            for(int wrap=-1;wrap<=1;++wrap)DrawSurface(strip,start+wrap*width,y+h*.1f,width,h*.30f,0,ToWickedColor(style.normal.foreground,style.opacity),cmd);
        }
        else
        {
            const char* labels[]={"N","NE","E","SE","S","SW","W","NW"};
            const int begin=int(std::floor((frame.heading-config.fieldOfView*.5f)/15))*15;
            for(int bearing=begin;bearing<=frame.heading+config.fieldOfView*.5f+15;bearing+=15)
            {
                const float px=x+w*.5f+(bearing-frame.heading)*pixels;
                DrawSurface(nullptr,px,y+h*.08f,std::max(1.f,unit),h*(bearing%45==0?.12f:.07f),0,colour,cmd);
                if(bearing%45==0)label(labels[int(WrapNavigationHeading(float(bearing)))/45],px,y+h*.24f,int(style.text.fontSize));
            }
        }
    }
    const auto* pointer=artwork(config.pointerImage);
    if(pointer)DrawSurface(pointer,x+w*.5f-8*unit,y+2*unit,16*unit,12*unit,0,wi::Color(255,255,255,uint8_t(255*style.opacity)),cmd);
    else DrawSurface(nullptr,x+w*.5f-unit,y+2*unit,2*unit,13*unit,0,colour,cmd);
    for(const auto& marker:frame.markers)
    {
        const float px=x+marker.fraction*w,size=config.markerSize*unit,my=y+h*.49f;
        auto icon=config.icons.find(marker.category);if(icon==config.icons.end())icon=config.icons.find("default");
        const auto* image=!marker.icon.empty()?artwork(marker.icon):icon==config.icons.end()?nullptr:artwork(icon->second);
        if(image){const auto& desc=image->GetTexture().GetDesc();const auto rect=FitScreenImage({px-size*.5f,my,size,size},float(desc.width),float(desc.height),ScreenImageFit::Contain);
            DrawSurface(image,rect.x,rect.y,rect.width,rect.height,0,wi::Color(255,255,255,uint8_t(255*style.opacity)),cmd);}
        else DrawSurface(nullptr,px-size*.18f,my+size*.20f,size*.36f,size*.36f,size*.18f,colour,cmd);
        std::string text=config.showNames?marker.label:"";
        if(text.size()>24)text=text.substr(0,21)+"...";
        if(config.showDistance){if(!text.empty())text+=" ";text+=std::to_string(int(std::round(marker.distance)))+" m";}
        if(!text.empty())label(text,px,my+size+2*unit,std::max(10,int(style.text.fontSize*.65f)));
    }
    if(!navigationSnapshot||navigationPreview)label("EDITOR SAMPLE",x+w-2*unit,y+h-12*unit,10,wi::font::WIFALIGN_RIGHT);
}
