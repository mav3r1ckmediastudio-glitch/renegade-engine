#include "ParticleTextureBrowser.h"
#include "RenegadeStudioChrome.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace renegade::studio {
namespace {
constexpr wi::Color Teal(137,225,208,255),Panel(33,35,37,255),Text(235,238,239,255);
void Rect(float x,float y,float w,float h,wi::Color c,wi::graphics::CommandList cmd){wi::image::Draw(nullptr,wi::image::Params(x,y,w,h,c),cmd);}
void Caption(const std::string& text,float x,float y,int size,wi::graphics::CommandList cmd,wi::Color color=Text){wi::font::Draw(text,wi::font::Params(x,y,size,wi::font::WIFALIGN_LEFT,wi::font::WIFALIGN_TOP,color),cmd);}
void Backdrop(float x,float y,float w,float h,bool checker,wi::graphics::CommandList cmd){
 Rect(x,y,w,h,wi::Color(19,21,23,255),cmd);if(!checker)return;
 const int n=6;for(int j=0;j<n;++j)for(int i=0;i<n;++i)if((i+j)%2)Rect(x+i*w/n,y+j*h/n,w/n+.5f,h/n+.5f,wi::Color(37,40,42,255),cmd);
}
class BrowserButton:public wi::gui::Button {
public:bool chosen=false,primary=false;
 void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd)const override{
  if(!IsVisible())return;ApplyScissor(canvas,scissorRect,cmd);auto edge=chosen||GetState()==wi::gui::FOCUS?Teal:wi::Color(85,91,94,255);
  Rect(translation.x,translation.y,scale.x,scale.y,edge,cmd);Rect(translation.x+1,translation.y+1,scale.x-2,scale.y-2,primary||chosen?Teal:wi::Color(45,49,51,255),cmd);
  auto color=!IsEnabled()?wi::Color(115,122,124,255):primary||chosen?wi::Color(15,28,27,255):Text;
  wi::font::Draw(GetText(),wi::font::Params(translation.x+scale.x*.5f,translation.y+scale.y*.5f,12,wi::font::WIFALIGN_CENTER,wi::font::WIFALIGN_CENTER,color),cmd);
 }
};
class TextureTile:public wi::gui::Button {
public:
 const wi::gui::Window* clipWindow=nullptr;
 bridge::ParticleTextureEntry entry;wi::Resource texture;bool chosen=false,checker=true,large=false;float clock=0;
 void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd)const override{
  if(!IsVisible())return;
  auto clip=scissorRect;
  if(clipWindow){
   const auto origin=clipWindow->GetPos();
   const float top=origin.y+clipWindow->GetControlSize()+1+110;
   clip.left=std::max(clip.left,int32_t(std::ceil(origin.x+200)));
   clip.right=std::min(clip.right,int32_t(std::floor(origin.x+770)));
   clip.top=std::max(clip.top,int32_t(std::ceil(top)));
   clip.bottom=std::min(clip.bottom,int32_t(std::floor(top+486)));
  }
  ApplyScissor(canvas,clip,cmd);
  const float x=translation.x,y=translation.y,w=scale.x,h=scale.y;
  Rect(x,y,w,h,chosen?Teal:wi::Color(76,83,86,255),cmd);float imageHeight=large?h-2:h-27;Backdrop(x+1,y+1,w-2,imageHeight,checker,cmd);
  if(texture.IsValid()&&texture.GetTexture().IsValid()){
   const auto& t=texture.GetTexture();auto d=t.GetDesc();float cw=float(d.width)/std::max(1u,entry.columns),ch=float(d.height)/std::max(1u,entry.rows);
   auto count=std::max(1u,entry.frames-entry.start);unsigned frame=entry.start;if(entry.animated)frame+=unsigned(clock*(entry.fps>0?entry.fps:float(count)/2))%count;
   float fit=std::min((w-12)/cw,(imageHeight-10)/ch);wi::image::Params p(x+(w-cw*fit)*.5f,y+(imageHeight-ch*fit)*.5f,cw*fit,ch*fit,wi::Color::White());
   p.blendFlag=entry.distortion?wi::enums::BLENDMODE_ALPHA:entry.blend;p.enableDrawRect({float(frame%entry.columns)*cw,float(frame/entry.columns)*ch,cw,ch});wi::image::Draw(&t,p,cmd);
  }
  if(!large){Rect(x+1,y+h-26,w-2,25,wi::Color(31,35,37,255),cmd);auto name=entry.name;const size_t letters=size_t(std::max(5.f,std::floor((w-10)/6.f)));if(name.size()>letters)name=name.substr(0,letters-2)+"..";Caption(name,x+5,y+h-23,11,cmd);Caption(entry.distortion?"MAP":entry.animated?"SHEET":"STATIC",x+5,y+5,9,cmd,Teal);}
 }
};
}
struct ParticleTextureBrowser::Impl {
 ParticleTextureBrowser& window;std::string root,project,category="All textures";int source=0,kind=0,page=0,capacity=20;size_t selected=0;float clock=0;bool setting=false,importing=false;
 std::vector<bridge::ParticleTextureEntry> included,owned,*entries=&included;std::vector<size_t> filtered;std::vector<wi::Resource> previews;std::vector<bool> attempted;
 std::function<bool()> current;std::function<void(const bridge::ParticleTextureEntry&,bool)> apply;std::function<void()> closed;
 std::shared_ptr<int> lifetime=std::make_shared<int>(0);unsigned generation=0;
 RenegadeTextInputField search;std::array<BrowserButton,2> sources;std::array<BrowserButton,3> kinds;std::array<BrowserButton,11> categories;std::array<TextureTile,35> tiles;TextureTile preview;
 BrowserButton previous,next,import,use,cancel,allFrames;wi::gui::Slider thumbnailSize;wi::gui::CheckBox checker,recommended;
 wi::gui::ComboBox textureKind;std::array<wi::gui::TextInputField,5> layout;std::array<wi::gui::Label,5> layoutLabels;
 wi::gui::Label selectedTitle,name,metadata,target,status,range,hint,checkerLabel,recommendedLabel;std::vector<wi::gui::Label*> labels;
 bridge::MaterialTextureSlot slot=bridge::MaterialTextureSlot::BaseColor;
 Impl(ParticleTextureBrowser& w):window(w){}
 template<class T>void Add(T& w,float x,float y,float width,float height){w.SetPos({x,y});w.SetSize({width,height});window.AddWidget(&w);w.SetShadowRadius(0);}
 void Button(BrowserButton& b,const char* text,float x,float y,float width){b.Create(text);b.SetText(text);Add(b,x,y,width,28);}
 void Label(wi::gui::Label& l,const char* text,float x,float y,float width,int size=13){l.Create(text);l.SetText(text);l.font.params.size=size;Add(l,x,y,width,23);labels.push_back(&l);}
 void Theme(){wi::gui::Theme t;t.image.background=false;t.image.color=Panel;t.font.color=Text;t.shadow=0;t.tooltipImage.color=Panel;t.tooltipFont.color=Text;window.SetTheme(t);window.SetColor(Panel,wi::gui::WIDGET_ID_WINDOW_BASE);window.SetColor(Teal,wi::gui::FOCUS);window.SetColor(wi::Color(56,94,88,255),wi::gui::ACTIVE);window.SetColor(Panel,wi::gui::WIDGET_ID_COMBO_DROPDOWN);for(auto* l:labels)for(auto& s:l->sprites)s.params.color=wi::Color::Transparent();}
 void Build(){
  Button(sources[0],"Included",18,20,80);Button(sources[1],"Project",104,20,80);for(int i=0;i<2;++i)sources[i].OnClick([this,i](auto&){Switch(i);});
  search.Create("Search particle textures");search.SetText("");search.SetPlaceholder("Search particle textures...");Add(search,200,20,570,28);search.OnInput([this](auto&){page=0;Filter();});search.OnInputAccepted([this](auto&){page=0;Filter();});
  Button(import,"Import...",1010,20,140);import.OnClick([this](auto&){Import();});
  const char* typeNames[]={"All","Static images","Animated sheets"};float x=200;for(int i=0;i<3;++i){Button(kinds[i],typeNames[i],x,68,i?150:80);x+=i?158:88;kinds[i].OnClick([this,i](auto&){kind=i;page=0;Filter();});}
  const char* names[]={"All textures","Fire & sparks","Smoke & mist","Dust","Nature","Water","Glows","Distortion","Impacts","Magic","Project"};
  for(int i=0;i<11;++i){Button(categories[i],names[i],18,112+i*34,166);categories[i].OnClick([this,i,names=std::string(names[i])](auto&){category=names;page=0;Filter();});}
  for(size_t i=0;i<tiles.size();++i){tiles[i].clipWindow=&window;tiles[i].Create("Texture "+std::to_string(i));tiles[i].SetText("");Add(tiles[i],200,110,104,118);tiles[i].OnClick([this,i](auto&){size_t n=size_t(page*capacity)+i;if(n<filtered.size())Select(filtered[n]);});}
  Label(selectedTitle,"Selected texture",800,68,300,16);preview.Create("Selected texture preview");preview.SetText("");preview.large=true;Add(preview,800,110,350,250);preview.SetEnabled(false);
  checker.Create("");Label(checkerLabel,"Checkerboard",830,370,300,12);checker.SetCheck(true);Add(checker,800,370,20,20);checker.SetTooltip("Show transparency over a checkerboard; untick for dark.");checker.OnClick([this](auto&){RefreshTiles();});
  Label(name,"Choose a texture",800,404,350,17);Label(metadata,"",800,435,350,12);
  textureKind.Create("Texture kind");textureKind.SetText("");textureKind.AddItem("Static image");textureKind.AddItem("Animated sheet");Add(textureKind,800,466,350,26);
  textureKind.OnSelect([this](auto&){if(setting||entries->empty()||selected>=entries->size())return;auto& e=(*entries)[selected];e.animated=textureKind.GetSelected()==1;e.layoutKnown=true;if(!e.animated){e.columns=e.rows=e.frames=1;e.start=0;e.fps=0;e.frameBlend=false;}else if(e.fps==0)e.fps=12;Details();RefreshTiles();});
  const char* controls[]={"Columns","Rows","Frames","FPS","Start frame"};for(int i=0;i<5;++i){float xx=800+(i%2)*178,yy=510+(i/2)*38;Label(layoutLabels[i],controls[i],xx,yy,100,12);layout[i].Create(controls[i]);layout[i].SetText("");layout[i].SetFloatPrecision(i==3?1:0);Add(layout[i],xx+95,yy,72,24);layout[i].OnInputAccepted([this,i](const auto& a){EditLayout(i,a.fValue);});}
  Button(allFrames,"All frames",978,586,172);allFrames.OnClick([this](auto&){if(selected<entries->size()){auto& e=(*entries)[selected];e.frames=e.columns*e.rows;e.start=0;Details();RefreshTiles();}});
  recommended.Create("");Label(recommendedLabel,"Use recommended blend",830,630,300,12);recommended.SetCheck(true);Add(recommended,800,630,20,20);recommended.SetTooltip("Use the texture's Alpha or Additive blend. Movement, size, lifetime and fields are kept.");
  Label(target,"",800,666,350,12);Label(hint,"",800,694,350,11);
  Button(previous,"Previous",200,612,100);Button(next,"Next",670,612,100);previous.OnClick([this](auto&){if(page>0){--page;RefreshTiles();}});next.OnClick([this](auto&){if((page+1)*capacity<int(filtered.size())){++page;RefreshTiles();}});Label(range,"",310,615,350,12);
  thumbnailSize.Create(72,136,96,64,"Thumbnail size");Add(thumbnailSize,330,665,260,22);thumbnailSize.OnSlide([this](auto&){page=0;RefreshTiles();});
  Label(status,"Choose an image for the selected particle layer.",18,730,765,12);Button(cancel,"Cancel",830,730,130);Button(use,"Use texture",980,730,170);use.primary=true;cancel.OnClick([this](auto&){window.Close();});use.OnClick([this](auto&){Use();});
 }
 void Ensure(size_t n){if(n>=entries->size()||attempted[n])return;attempted[n]=true;std::string error;previews[n]=bridge::LoadParticleTexturePreview(root,project,(*entries)[n],error);if(!error.empty())status.SetText(error);if(previews[n].IsValid()&&previews[n].GetTexture().IsValid()){auto d=previews[n].GetTexture().GetDesc();(*entries)[n].width=d.width;(*entries)[n].height=d.height;}}
 void Switch(int tab){source=tab;entries=source?&owned:&included;previews.assign(entries->size(),{});attempted.assign(entries->size(),false);selected=0;page=0;category="All textures";Filter();}
 void Filter(){filtered=bridge::FilterParticleTextures(*entries,category,search.GetText(),kind);if(!filtered.empty()&&std::find(filtered.begin(),filtered.end(),selected)==filtered.end())selected=filtered.front();RefreshTiles();Details();}
 void RefreshTiles(){
  float size=std::round(thumbnailSize.GetValue());int cols=std::clamp(int(570/(size+8)),4,7),rows=std::clamp(int(486/(size+22)),3,5);capacity=std::min(35,cols*rows);int pages=std::max(1,(int(filtered.size())+capacity-1)/capacity);page=std::clamp(page,0,pages-1);
  for(size_t i=0;i<tiles.size();++i){size_t n=size_t(page*capacity)+i;bool visible=i<size_t(capacity)&&n<filtered.size();auto& t=tiles[i];t.SetVisible(visible);if(!visible)continue;auto index=filtered[n];Ensure(index);t.entry=(*entries)[index];t.texture=previews[index];t.chosen=index==selected;t.checker=checker.GetCheck();t.clock=clock;t.SetPos({200+float(i%cols)*(570/cols),110+float(i/cols)*(486/rows)});t.SetSize({float(570/cols-8),float(486/rows-6)});}
  previous.SetEnabled(page>0);next.SetEnabled((page+1)*capacity<int(filtered.size()));range.SetText(filtered.empty()?"No matching textures":std::to_string(page*capacity+1)+" - "+std::to_string(std::min((page+1)*capacity,int(filtered.size())))+" of "+std::to_string(filtered.size()));
  for(int i=0;i<2;++i)sources[i].chosen=i==source;for(int i=0;i<3;++i)kinds[i].chosen=i==kind;for(auto& c:categories)c.chosen=c.GetText()==category;
 }
 void Select(size_t n){selected=n;Ensure(n);clock=0;Details();RefreshTiles();}
 void Details(){
  setting=true;bool valid=!filtered.empty()&&selected<entries->size();use.SetEnabled(valid&&!importing);textureKind.SetEnabled(valid);preview.SetVisible(valid);
  if(valid){Ensure(selected);auto& e=(*entries)[selected];preview.entry=e;preview.texture=previews[selected];preview.checker=checker.GetCheck();name.SetText(e.name);metadata.SetText(std::to_string(e.width)+" x "+std::to_string(e.height)+(e.distortion?"  |  Distortion map":e.animated?"  |  Animated sheet":"  |  Static image"));textureKind.SetSelected(e.animated?1:0);
   const float values[]={float(e.columns),float(e.rows),float(e.frames),e.fps,float(e.start)};for(int i=0;i<5;++i){layout[i].SetValue(values[i]);layout[i].SetVisible(e.animated);layoutLabels[i].SetVisible(e.animated);}allFrames.SetVisible(e.animated);
   hint.SetText(e.distortion?"Applies to Normal / distortion with Heat distortion.":!e.animated&&e.columns*e.rows>1?"Uses the selected atlas cell; no animation.":"Movement and emitter settings are kept.");
  }else{name.SetText("No matching textures");metadata.SetText("");hint.SetText("");for(int i=0;i<5;++i){layout[i].SetVisible(false);layoutLabels[i].SetVisible(false);}allFrames.SetVisible(false);}
  setting=false;
 }
 void EditLayout(int index,float value){if(setting||selected>=entries->size()||!std::isfinite(value))return;auto& e=(*entries)[selected];e.layoutKnown=true;
  if(index==0)e.columns=unsigned(std::clamp(std::round(value),1.f,32.f));if(index==1)e.rows=unsigned(std::clamp(std::round(value),1.f,32.f));if(index==2)e.frames=unsigned(std::clamp(std::round(value),1.f,float(e.columns*e.rows)));if(index==3)e.fps=std::clamp(value,0.f,240.f);if(index==4)e.start=unsigned(std::clamp(std::round(value),0.f,float(e.frames-1)));e.frames=std::min(e.frames,e.columns*e.rows);e.start=std::min(e.start,e.frames-1);Details();RefreshTiles();}
 void Use(){
  if(importing||filtered.empty()||selected>=entries->size()||!current||!current())return;auto entry=(*entries)[selected];entry.layoutKnown=true;bool blend=recommended.GetCheck();auto token=generation;std::weak_ptr<int> alive=lifetime;importing=true;use.SetEnabled(false);status.SetText("Preparing project texture...");
  wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,alive,token,entry,blend](uint64_t)mutable{
   if(alive.expired()||token!=generation||!window.IsVisible()||!current||!current())return;std::string error;if(!bridge::AdoptParticleTexture(root,project,entry,error)){importing=false;use.SetEnabled(true);status.SetText(error);return;}if(apply)apply(entry,blend);window.Close();
  });
 }
 void Import(){
  wi::helper::FileDialogParams p;p.type=wi::helper::FileDialogParams::OPEN;p.description="Particle texture or spritesheet";p.extensions={"png","tga","dds","jpg","jpeg"};auto token=generation;std::weak_ptr<int> alive=lifetime;
  wi::helper::FileDialog(p,[this,alive,token](const std::string& path){if(path.empty()||alive.expired())return;wi::eventhandler::Subscribe_Once(wi::eventhandler::EVENT_THREAD_SAFE_POINT,[this,alive,token,path](uint64_t){
   if(alive.expired()||token!=generation||!window.IsVisible()||!current||!current())return;bridge::ParticleTextureEntry e;std::string error;if(!bridge::ImportParticleTexture(root,project,path,e,error)){status.SetText(error);return;}if(!bridge::ReadProjectParticleTextures(root,project,owned,error)){status.SetText(error);return;}search.SetText("");kind=0;Switch(1);for(size_t n=0;n<owned.size();++n)if(owned[n].assetId==e.assetId){Select(n);break;}status.SetText("Imported. Choose Static image or Animated sheet, then Use texture.");
  });});
 }
};
ParticleTextureBrowser::ParticleTextureBrowser():impl_(std::make_unique<Impl>(*this)){}
ParticleTextureBrowser::~ParticleTextureBrowser(){impl_->lifetime.reset();RemoveWidgets();}
void ParticleTextureBrowser::Create(){wi::gui::Window::Create("Particle Texture Library",WindowControls::CLOSE|WindowControls::MOVE);SetControlSize(28);SetSize({1180,810});impl_->Build();impl_->Theme();SetVisible(false);}
void ParticleTextureBrowser::Open(const std::string& root,const bridge::StableId& project,const std::string& layer,bridge::MaterialTextureSlot slot,std::function<bool()> current,std::function<void(const bridge::ParticleTextureEntry&,bool)> apply,std::function<void()> closed){
 auto& i=*impl_;++i.generation;i.root=root;i.project=project;i.slot=slot;i.current=std::move(current);i.apply=std::move(apply);i.closed=std::move(closed);i.importing=false;i.clock=0;i.kind=0;i.search.SetText("");i.recommended.SetCheck(true);std::string error,warning;
 if(!bridge::ReadIncludedParticleTextures(bridge::InstalledParticleTextureDirectory(),i.included,error))warning=error;if(!bridge::ReadProjectParticleTextures(root,project,i.owned,error))warning=error;
 SetVisible(true);i.Theme();i.target.SetText("Apply to layer: "+layer);i.status.SetText(warning.empty()?"Choose an image for the selected particle layer.":warning);i.Switch(i.included.empty()?1:0);
}
void ParticleTextureBrowser::Close(){auto& i=*impl_;++i.generation;SetVisible(false);auto callback=std::move(i.closed);i.current={};i.apply={};if(callback)callback();}
void ParticleTextureBrowser::Update(const wi::Canvas& c,float dt){
 auto& i=*impl_;
 // Change the page before native child transforms/scissors update. Revealing
 // a previously hidden tile afterwards draws its stale transform for one frame.
 if(IsVisible()){
  const auto p=wi::input::GetPointer();const auto pos=GetPos();
  const float top=pos.y+GetControlSize()+1+110;
  if(p.z&&p.x>=pos.x+200&&p.x<pos.x+770&&p.y>=top&&p.y<top+486){
   i.page+=p.z>0?-1:1;i.RefreshTiles();
  }
 }
 bool was=IsVisible();wi::gui::Window::Update(c,dt);if(was&&!IsVisible()){Close();return;}if(!IsVisible())return;if(!i.current||!i.current()){Close();return;}
 i.clock+=std::clamp(dt,0.f,.1f);for(auto& t:i.tiles)t.clock=i.clock;i.preview.clock=i.clock;i.preview.checker=i.checker.GetCheck();
}
}
