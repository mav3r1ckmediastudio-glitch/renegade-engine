#include "LightPickerWindow.h"
#include "RenegadeStudioChrome.h"
#include "renegade/bridge/StudioSession.h"
#include "../../Runtime/src/RuntimeScriptRuntime.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <utility>
#include <cctype>
#include <cstdio>
namespace renegade::studio {
namespace {
using Behaviour=bridge::LightBehaviour;
constexpr wi::Color Teal(137,225,208,255);
constexpr wi::Color Grey(46,49,51,255);
void Rect(float x,float y,float w,float h,wi::Color c,wi::graphics::CommandList cmd) {
 wi::image::Params p(x,y,w,h,c);wi::image::Draw(nullptr,p,cmd);
}
class PickerButton : public wi::gui::Button {
public:
 int textSize=12;bool swatch=false,primary=false;wi::Color tone=Grey;
 void SetRenderTextSize(int size){textSize=size;}
 void SetColor(wi::Color colour,int id=-1) override {wi::gui::Button::SetColor(colour,id);tone=colour;}
 void Render(const wi::Canvas&,wi::graphics::CommandList cmd) const override {
  if(!IsVisible())return;
  auto fill=swatch||primary?tone:wi::Color(48,51,53,255);
  const auto edge=GetState()==wi::gui::FOCUS?Teal:wi::Color(91,96,98,255);
  Rect(translation.x,translation.y,scale.x,scale.y,edge,cmd);
  Rect(translation.x+1,translation.y+1,scale.x-2,scale.y-2,fill,cmd);
  wi::font::Params fp(translation.x+scale.x*.5f,translation.y+scale.y*.5f,textSize,
   wi::font::WIFALIGN_CENTER,wi::font::WIFALIGN_CENTER,
   !IsEnabled()?wi::Color(125,130,132,255):primary?wi::Color(18,30,29,255):wi::Color(239,242,242,255));
  fp.bolden=.12f;wi::font::Draw(GetText(),fp,cmd);
 }
};
class PickerCheckBox : public wi::gui::CheckBox {
public:
 void Render(const wi::Canvas&,wi::graphics::CommandList cmd) const override {
  if(!IsVisible())return;
  const auto edge=GetState()==wi::gui::FOCUS?Teal:wi::Color(105,112,113,255);
  Rect(translation.x,translation.y,18,18,edge,cmd);
  Rect(translation.x+1,translation.y+1,16,16,wi::Color(27,30,31,255),cmd);
  if(GetCheck())Rect(translation.x+4,translation.y+4,10,10,Teal,cmd);
  wi::font::Params fp(translation.x+27,translation.y+9,13,wi::font::WIFALIGN_LEFT,wi::font::WIFALIGN_CENTER,
   IsEnabled()?wi::Color(235,238,239,255):wi::Color(120,128,129,255));
  wi::font::Draw(GetName(),fp,cmd);
 }
};
class LightColourWheel : public wi::gui::ColorPicker {
public:
 void HideFields() {
  for(wi::gui::Widget* w:std::array<wi::gui::Widget*,8>{&text_R,&text_G,&text_B,&text_H,&text_S,&text_V,&text_hex,&alphaSlider})w->SetVisible(false);
 }
 void ResizeLayout() override {HideFields();wi::gui::ColorPicker::ResizeLayout();}
};
class IconButton : public PickerButton {
public:
 int icon=0;bool chosen=false;
 void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd) const override {
  PickerButton::Render(canvas,cmd);if(!IsVisible())return;
  const XMFLOAT2 p={translation.x,translation.y},sz={scale.x,scale.y};
  const float cx=p.x+sz.x*.5f,cy=p.y+sz.y*.5f,k=std::min(sz.x,sz.y)*.28f;
  auto line=[&](float ax,float ay,float bx,float by) {
   const float dx=(bx-ax)*k,dy=(by-ay)*k;
   wi::image::Params a; a.pos={cx+(ax+bx)*k*.5f,cy+(ay+by)*k*.5f,0};
   a.siz={std::sqrt(dx*dx+dy*dy),2};a.pivot={.5f,.5f};a.rotation=std::atan2(dy,dx);
   a.color=chosen?Teal:wi::Color(225,228,229,255);wi::image::Draw(nullptr,a,cmd);
  };
  auto circle=[&](float radius,int count=24,float begin=0,float end=6.2831853f) {
   for(int i=0;i<count;++i){float a=begin+(end-begin)*i/count,b=begin+(end-begin)*(i+1)/count;
    line(radius*std::cos(a),radius*std::sin(a),radius*std::cos(b),radius*std::sin(b));}
  };
  switch(icon) {
  case 0: // flame
   line(0,-1,-.28f,-.3f);line(-.28f,-.3f,-.65f,-.55f);line(-.65f,-.55f,-.8f,.25f);
   line(-.8f,.25f,-.45f,.85f);line(-.45f,.85f,.3f,1);line(.3f,1,.8f,.45f);
   line(.8f,.45f,.7f,-.3f);line(.7f,-.3f,0,-1);line(0,.8f,-.25f,.3f);line(-.25f,.3f,.15f,-.2f);break;
  case 1: line(.15f,-1,-.6f,.15f);line(-.6f,.15f,.1f,.15f);line(.1f,.15f,-.1f,1);line(-.1f,1,.65f,-.3f);line(.65f,-.3f,0,-.3f);line(0,-.3f,.15f,-1);break;
  case 2: case 6:
   circle(.48f);for(int i=0;i<8;++i){float a=i*.785398f;line(.7f*std::cos(a),.7f*std::sin(a),std::cos(a),std::sin(a));}break;
  case 3: circle(.8f,20,-2.9f,2.6f);line(-.7f,-.35f,-1,-.1f);line(-.7f,-.35f,-.4f,-.1f);break;
  case 4: for(int i=0;i<24;++i){float x=-1+2.0f*i/24,x2=x+2.0f/24;line(x,.6f*std::sin(x*4),x2,.6f*std::sin(x2*4));}break;
  case 5: for(int i=0;i<8;++i){float a=i*.785398f;line(.45f*std::cos(a),.45f*std::sin(a),std::cos(a),std::sin(a));}break;
  case 7: line(0,-.8f,-.8f,.8f);line(-.8f,.8f,.8f,.8f);line(.8f,.8f,0,-.8f);circle(.22f);break;
  case 8: for(int i=0;i<3;++i){float x=-.8f+i*.8f;line(x,-.8f,x+.35f,.65f);line(x+.35f,.65f,x,.4f);line(x+.35f,.65f,x+.5f,.25f);}break;
  case 9: line(-1,-.6f,1,-.6f);line(1,-.6f,1,.6f);line(1,.6f,-1,.6f);line(-1,.6f,-1,-.6f);break;
  }
  if(chosen){Rect(p.x,p.y,sz.x,2,Teal,cmd);Rect(p.x,p.y+sz.y-2,sz.x,2,Teal,cmd);Rect(p.x,p.y,2,sz.y,Teal,cmd);Rect(p.x+sz.x-2,p.y,2,sz.y,Teal,cmd);}
 }
};
class LightPreview : public wi::RenderPath3D {
public:
 wi::allocator::shared_ptr<wi::scene::Scene> owned;
 wi::scene::CameraComponent view;
 runtime::RuntimeScriptRuntime scripts;
 wi::ecs::Entity lamp=wi::ecs::INVALID_ENTITY;
 std::string root,error;
 mutable unsigned settle=8;
 mutable bool renderPending=false;
 bool animated=false;
 float elapsed=0;
 LightPreview(const std::string& projectRoot):root(projectRoot) {
  owned=wi::allocator::make_shared<wi::scene::Scene>();scene=owned.get();camera=&view;
  const auto cube=[&](const char* name,XMFLOAT3 pos,XMFLOAT3 scale) {
   auto e=scene->Entity_CreateCube(name);auto* t=scene->transforms.GetComponent(e);
   t->Scale(scale);t->Translate(pos);t->UpdateTransform();
   auto* object=scene->objects.GetComponent(e);auto* mesh=scene->meshes.GetComponent(object->meshID);
   auto* material=scene->materials.GetComponent(mesh->subsets[0].materialID);
   material->baseColor={.32f,.33f,.34f,1};material->roughness=.92f;material->metalness=0;material->SetDirty();
  };
  cube("Preview floor",{0,-.15f,0},{3,.15f,3});
  cube("Preview wall",{0,1.5f,1.5f},{3,1.5f,.15f});
  cube("Shadow pillar",{0,.65f,.15f},{.18f,.65f,.18f});
  auto w=wi::ecs::CreateEntity();auto& weather=scene->weathers.Create(w);
  weather.horizon=weather.zenith={.025f,.025f,.025f};weather.ambient={.035f,.035f,.035f};scene->weather=weather;
  lamp=scene->Entity_CreateLight("Light preview",{-.65f,1.5f,-.7f});
  bridge::EnsurePersistentEntityIdentities(*scene,error);
  init(256,220,96);setOcclusionCullingEnabled(false);setBloomEnabled(false);setEyeAdaptionEnabled(false);
  setMotionBlurEnabled(false);setDepthOfFieldEnabled(false);setFXAAEnabled(true);
  setAO(AO_DISABLED);setSSREnabled(false);setSSGIEnabled(false);
  setRaytracedReflectionsEnabled(false);setRaytracedDiffuseEnabled(false);
  setReflectionsEnabled(false);setVolumeLightsEnabled(false);setLightShaftsEnabled(false);
  view.CreatePerspective(256,220,.05f,30,XM_PI/3);
  view.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(3,2.5f,-4,1),XMVectorSet(0,1,.4f,1),XMVectorSet(0,1,0,0))));
  view.UpdateCamera();ResizeBuffers();
  bridge::EnsureLightBehaviourSource(root,error);
 }
 void Setup(const bridge::LightPreset& p) {
  scripts.StopScene();
  auto* t=scene->transforms.GetComponent(lamp);t->translation_local={-.65f,1.5f,-.7f};
  XMStoreFloat4(&t->rotation_local,XMQuaternionRotationRollPitchYaw(-.5f,0,0));t->SetDirty();t->UpdateTransform();
  bridge::ApplyLight(*scene->lights.GetComponent(lamp),p.light);
  scene->lights.GetComponent(lamp)->forced_shadow_resolution=256;
  animated=p.behaviour!=Behaviour::Constant;settle=8;elapsed=0;
  if(p.behaviour!=Behaviour::Constant) {
   auto doc=bridge::CreateScriptDocument(bridge::GenerateStableId(),bridge::GenerateStableId(),"Content/Scenes/Preview.wiscene","light-preview");
   doc.attachments.push_back(bridge::MakeLightBehaviourAttachment(bridge::PersistentEntityId(*scene,lamp),p,doc));
   scripts.StartScene(doc,*scene,root,error);
  }
 }
 void Tick(float dt,bool animate) {
  elapsed+=dt;const bool playing=animate&&animated;
  renderPending=(settle>0)||(playing&&elapsed>=1.0f/30);
  if(!renderPending)return;
  if(playing)scripts.Update(elapsed);
  PreUpdate();wi::RenderPath3D::Update(elapsed);elapsed=0;
 }
 void Render() const override {
  if(!renderPending)return;
  wi::RenderPath3D::Render();renderPending=false;
  if(settle&&wi::renderer::IsPipelineCreationActive()==0)--settle;
 }
};
}
struct LightPickerWindow::Impl {
 LightPickerWindow& window;bridge::StudioSession* session=nullptr;
 bridge::StableId project;std::uint64_t sceneRevision=0;
 wi::ecs::Entity target=wi::ecs::INVALID_ENTITY;
 bridge::LightPreset draft;std::vector<bridge::LightPreset> presets;
 std::vector<XMFLOAT3> colours;bool placeRequest=false,applyRequest=false,dirtyPreview=true;
 std::unique_ptr<LightPreview> preview;
 wi::gui::TreeList list;RenegadeTextInputField search,name;
 LightColourWheel colour;
 wi::gui::TextInputField red,green,blue,hex;
 XMFLOAT3 colourValue={1,1,1};
 std::array<IconButton,4> types;std::array<IconButton,6> behaviours;
 std::array<wi::gui::Label,4> typeLabels;std::array<wi::gui::Label,6> behaviourLabels;
 std::array<PickerButton,24> swatches;
 wi::gui::Label savedTitle,typeTitle,colourTitle,behaviourTitle,settingsTitle,previewTitle,status,help,nameLabel;
 wi::gui::Label previewImage;
 PickerButton saveColour,saveNew,update,place,cancel,advanced;
 wi::gui::Slider brightness,range,strength,speed,movement,outerCone,innerCone,radius,length,height,boost;
 PickerCheckBox shadows,keep,randomise,animate,volumetrics;
 bool expanded=false;
 std::vector<wi::gui::Label*> captions;
 Impl(LightPickerWindow& w):window(w){}
 template<class W> void Add(W& c,float x,float y,float width,float height) {
  c.SetPos({x,y});c.SetSize({width,height});window.AddWidget(&c);c.SetShadowRadius(0);
 }
 void Label(wi::gui::Label& c,const char* text,float x,float y,float width,int size=15) {
  c.Create(text);c.SetText(text);c.font.params.size=size;c.font.params.bolden=.2f;Add(c,x,y,width,24);captions.push_back(&c);
 }
 void Button(PickerButton& b,const char* text,float x,float y,float width) {
  b.Create(text);b.SetText(text);b.SetRenderTextSize(12);Add(b,x,y,width,30);
 }
 void Slider(wi::gui::Slider& s,const char* label,float lo,float hi,float value,float x,float y,float width) {
  s.Create(lo,hi,value,1000,label);s.SetText(label);s.SetSize({width,22});Add(s,x,y,width,22);
  s.OnSlide([this](const wi::gui::EventArgs&){dirtyPreview=true;});
 }
 void Check(wi::gui::CheckBox& c,const char* text,bool value,float x,float y) {
  c.Create(text);c.SetText(text);c.SetCheck(value);Add(c,x,y,20,20);
  c.OnClick([this](const wi::gui::EventArgs&){dirtyPreview=true;});
 }
 bridge::LightPreset Read() const {
  auto p=draft;p.name=name.GetText();p.light.color=colourValue;
  p.light.intensity=brightness.GetValue();p.light.range=range.GetValue();p.light.castShadow=shadows.GetCheck();
  p.strength=strength.GetValue();p.speed=speed.GetValue();p.movement=movement.GetValue();p.randomise=randomise.GetCheck();
  p.light.outerConeDegrees=outerCone.GetValue();p.light.innerConeDegrees=innerCone.GetValue();
  p.light.radius=radius.GetValue();p.light.length=length.GetValue();p.light.height=height.GetValue();
  p.light.volumetrics=volumetrics.GetCheck();p.light.volumetricBoost=boost.GetValue();
  return bridge::SanitizeLightPreset(p);
 }
 void Set(const bridge::LightPreset& p) {
  draft=p;name.SetText(p.name);colour.SetPickColor(wi::Color::fromFloat3(p.light.color));colourValue=p.light.color;SyncColour();
  brightness.SetValue(p.light.intensity);range.SetValue(p.light.range);shadows.SetCheck(p.light.castShadow);
  strength.SetValue(p.strength);speed.SetValue(p.speed);movement.SetValue(p.movement);randomise.SetCheck(p.randomise);
  outerCone.SetValue(p.light.outerConeDegrees);innerCone.SetValue(p.light.innerConeDegrees);radius.SetValue(p.light.radius);
  length.SetValue(p.light.length);height.SetValue(p.light.height);volumetrics.SetCheck(p.light.volumetrics);boost.SetValue(p.light.volumetricBoost);
  for(int i=0;i<4;++i){const int native[]={wi::scene::LightComponent::POINT,wi::scene::LightComponent::SPOT,wi::scene::LightComponent::DIRECTIONAL,wi::scene::LightComponent::RECTANGLE};
   types[i].chosen=int(p.light.type)==native[i];}
  const Behaviour order[]={Behaviour::Flame,Behaviour::ElectricalFault,Behaviour::Constant,Behaviour::Rotating,Behaviour::Pulsing,Behaviour::Strobe};
  for(int i=0;i<6;++i)behaviours[i].chosen=p.behaviour==order[i];
  settingsTitle.SetText(std::string(bridge::LightBehaviourName(p.behaviour))+" Settings");
  const bool moving=p.behaviour!=Behaviour::Constant;
  strength.SetEnabled(moving&&p.behaviour!=Behaviour::Rotating);speed.SetEnabled(moving);
  movement.SetEnabled(p.behaviour==Behaviour::Flame);randomise.SetEnabled(moving&&p.behaviour!=Behaviour::Rotating);
  range.SetEnabled(p.light.type!=wi::scene::LightComponent::DIRECTIONAL);
  update.SetEnabled(!p.id.empty());dirtyPreview=true;
 }
 void SyncColour() {
  const auto c=wi::Color::fromFloat3(colourValue);
  red.SetValue(int(c.getR()));green.SetValue(int(c.getG()));blue.SetValue(int(c.getB()));
  char text[8];std::snprintf(text,sizeof(text),"%02X%02X%02X",unsigned(c.getR()),unsigned(c.getG()),unsigned(c.getB()));
  hex.SetText(text);
 }
 void Rows() {
  list.ClearItems();std::string filter=search.GetText();
  std::transform(filter.begin(),filter.end(),filter.begin(),[](unsigned char c){return char(std::tolower(c));});
  for(size_t i=0;i<presets.size();++i) {
   auto text=presets[i].name;std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return char(std::tolower(c));});
   if(text.find(filter)==std::string::npos)continue;
   wi::gui::TreeList::Item row;row.name=presets[i].name+(presets[i].id.empty()?" (built-in)":"");row.userdata=i;row.selected=draft.id.empty()?(presets[i].id.empty()&&draft.name==presets[i].name):presets[i].id==draft.id;list.AddItem(row);
  }
 }
 void Colours() {
  for(size_t i=0;i<swatches.size();++i) {
   swatches[i].SetVisible(i<colours.size());
   if(i<colours.size())swatches[i].SetColor(wi::Color::fromFloat3(colours[i]));
  }
 }
 void Reload() {
  std::string error;const auto& p=session->Projects().CurrentProject();
  presets=bridge::ListLightPresets(p.rootPath,p.projectId,error);
  const auto defaults=bridge::DefaultLightPresets();
  presets.insert(presets.end(),defaults.begin(),defaults.end());Rows();
  colours=bridge::LoadLightColours(p.rootPath);
  if(colours.empty())colours={{1,.71f,.36f},{1,.86f,.61f},{1,1,1},{.57f,.72f,1},{.8f,.4f,1},{1,.2f,.12f}};
  Colours();if(!error.empty())status.SetText(error);
 }
 void Save(bool asNew) {
  auto p=Read();std::string error;const auto& project=session->Projects().CurrentProject();
  if(!bridge::SaveLightPreset(project.rootPath,project.projectId,p,asNew,error)){status.SetText(error);return;}
  Set(p);Reload();status.SetText("Saved "+p.name+" in Content/Lights. Placed lights remain independent.");
 }
 void Build() {
  Label(savedTitle,"Light Presets",18,16,250,17);
  search.Create("Search light presets");search.SetText("");search.SetPlaceholder("Search presets...");Add(search,18,48,250,28);
  search.OnInputAccepted([this](const wi::gui::EventArgs&){Rows();});
  list.Create("Saved project lights");list.SetText("");Add(list,18,86,250,270);
  list.OnSelect([this](const wi::gui::EventArgs& a){if(a.userdata<presets.size()){Set(presets[a.userdata]);Rows();}});
  Label(previewTitle,"Live Preview",18,372,250);
  previewImage.Create("Light preview image");previewImage.SetText("");previewImage.SetColor(wi::Color::White());Add(previewImage,18,405,256,220);
  Check(animate,"Preview animation",true,18,641);
  Label(typeTitle,"Light Type",300,16,680,17);
  const char* names[]={"Point","Spot","Directional","Area"};
  for(int i=0;i<4;++i) {
   types[i].icon=6+i;types[i].Create(names[i]);types[i].SetText("");Add(types[i],300+i*172.0f,48,64,56);
   Label(typeLabels[i],names[i],370+i*172.0f,65,100,13);
   types[i].OnClick([this,i](const wi::gui::EventArgs&){
    auto p=Read();const wi::scene::LightComponent::LightType native[]={wi::scene::LightComponent::POINT,wi::scene::LightComponent::SPOT,wi::scene::LightComponent::DIRECTIONAL,wi::scene::LightComponent::RECTANGLE};
    p.light.type=native[i];if(p.light.type==wi::scene::LightComponent::RECTANGLE&&p.light.length==0){p.light.length=1;p.light.height=1;}
    Set(p);
   });
  }
  Label(colourTitle,"Colour",300,116,680,17);
  colour.Create("",wi::gui::Window::WindowControls::DISABLE_TITLE_BAR);
  colour.SetControlSize(0);colour.HideFields();Add(colour,300,144,360,185);
  colour.OnColorChanged([this](const wi::gui::EventArgs&){colourValue=colour.GetPickColor().toFloat3();SyncColour();dirtyPreview=true;});
  auto channel=[&](wi::gui::TextInputField& input,const char* description,float x) {
   input.Create(description);input.SetDescription(description);Add(input,x,245,70,26);
   input.OnInputAccepted([this](const wi::gui::EventArgs&) {
    const auto value=[](wi::gui::TextInputField& f){try{return std::clamp(std::stoi(f.GetText()),0,255);}catch(...){return 0;}};
    const auto c=wi::Color(value(red),value(green),value(blue),255);
    colour.SetPickColor(c);colourValue=c.toFloat3();SyncColour();dirtyPreview=true;
   });
  };
  channel(red,"R: ",710);channel(green,"G: ",815);channel(blue,"B: ",920);
  hex.Create("Hex colour");hex.SetDescription("Hex: ");Add(hex,710,183,280,28);
  hex.SetTooltip("Six-digit RGB colour, e.g. FFB65C.");
  hex.OnInputAccepted([this](const wi::gui::EventArgs& a) {
   std::string text=a.sValue;if(!text.empty()&&text[0]=='#')text.erase(text.begin());
   if(text.size()!=6||!std::all_of(text.begin(),text.end(),[](unsigned char c){return std::isxdigit(c)!=0;})) {
    status.SetText("Enter six hex digits, e.g. FFB65C.");return;
   }
   const auto rgb=std::stoul(text,nullptr,16);
   const auto c=wi::Color((rgb>>16)&255,(rgb>>8)&255,rgb&255,255);
   colour.SetPickColor(c);colourValue=c.toFloat3();SyncColour();dirtyPreview=true;
  });
  Button(saveColour,"Save Colour",858,333,132);
  saveColour.OnClick([this](const wi::gui::EventArgs&){
   const auto c=colourValue;
   if(colours.size()==24)colours.erase(colours.begin());colours.push_back(c);
   std::string e;if(!bridge::SaveLightColours(session->Projects().CurrentProject().rootPath,colours,e))status.SetText(e);
   Colours();
  });
  for(size_t i=0;i<swatches.size();++i) {
   swatches[i].swatch=true;swatches[i].Create("Saved colour "+std::to_string(i+1));swatches[i].SetText("");
   Add(swatches[i],300+float(i)*23,333,18,24);
   swatches[i].OnClick([this,i](const wi::gui::EventArgs&){if(i<colours.size()){colour.SetPickColor(wi::Color::fromFloat3(colours[i]));colourValue=colours[i];SyncColour();dirtyPreview=true;}});
  }
  Slider(brightness,"Brightness",0,500,24,390,382,130);
  Slider(range,"Range (m)",.1f,100,8,664,382,130);
  Check(shadows,"Cast shadows",false,860,382);
  Label(behaviourTitle,"Behaviour",300,418,690,17);
  const char* bn[]={"Flame","Electrical Fault","Constant","Rotating","Pulsing","Strobe"};
  const Behaviour order[]={Behaviour::Flame,Behaviour::ElectricalFault,Behaviour::Constant,Behaviour::Rotating,Behaviour::Pulsing,Behaviour::Strobe};
  for(int i=0;i<6;++i) {
   behaviours[i].icon=i;behaviours[i].Create(bn[i]);behaviours[i].SetText("");
   Add(behaviours[i],300+i*116.0f,450,56,56);
   Label(behaviourLabels[i],bn[i],300+i*116.0f,512,114,11);
   behaviours[i].SetTooltip(std::string("Apply ")+bn[i]+". Replaces the previous stock light behaviour.");
   behaviours[i].OnClick([this,i,mode=order[i]](const wi::gui::EventArgs&){
    auto p=Read();p.behaviour=mode;
    if(mode==Behaviour::Strobe||mode==Behaviour::ElectricalFault)p.strength=1;
    Set(p);
   });
  }
  Label(settingsTitle,"Constant Settings",300,544,690);
  Slider(strength,"Strength",0,1,.35f,390,580,120);Slider(speed,"Speed",.05f,20,1.2f,650,580,105);
  Slider(movement,"Movement (m)",0,.5f,.03f,390,615,120);Check(randomise,"Randomise timing",true,652,615);
  Button(advanced,"Shape / volumetrics...",822,610,168);
  advanced.OnClick([this](const wi::gui::EventArgs&){expanded=!expanded;Advanced();});
  Slider(outerCone,"Outer cone",1,179,45,390,653,110);
  Slider(innerCone,"Inner cone",0,179,0,650,653,110);
  Slider(radius,"Radius (m)",0,10,.025f,880,653,90);
  Slider(length,"Width (m)",0,20,0,390,687,110);Slider(height,"Height (m)",0,20,0,650,687,110);
  Slider(boost,"Volume boost",0,100,0,880,687,90);Check(volumetrics,"Volumetrics",false,300,722);
  Label(nameLabel,"Preset name",300,658,100,13);
  name.Create("Light preset name");Add(name,400,653,270,30);
  Button(update,"Update Preset",684,653,143);Button(saveNew,"Save As New",842,653,148);
  update.OnClick([this](const wi::gui::EventArgs&){Save(false);});
  saveNew.OnClick([this](const wi::gui::EventArgs&){Save(true);});
  Label(help,"Saved in this project's Content/Lights folder",300,692,690,12);
  Check(keep,"Keep placing",true,18,740);
  Label(previewTitleHint_,"RMB to finish",174,739,200,12);
  Button(cancel,"Cancel",720,733,115);Button(place,"Place Light",850,733,140);
  place.primary=true;place.SetColor(Teal);
  cancel.OnClick([this](const wi::gui::EventArgs&){window.SetVisible(false);});
  place.OnClick([this](const wi::gui::EventArgs&){if(target==wi::ecs::INVALID_ENTITY)placeRequest=true;else applyRequest=true;});
  Label(status,"Choose a light or saved preset, then Place Light.",18,784,970,12);
  Advanced();
 }
 wi::gui::Label previewTitleHint_;
 void Advanced() {
  for(wi::gui::Widget* w:std::array<wi::gui::Widget*,7>{&outerCone,&innerCone,&radius,&length,&height,&boost,&volumetrics})w->SetVisible(expanded);
  const float dy=expanded?110:0;
  nameLabel.SetPos({300,658+dy});name.SetPos({400,653+dy});update.SetPos({684,653+dy});saveNew.SetPos({842,653+dy});help.SetPos({300,692+dy});
  keep.SetPos({18,740+dy});previewTitleHint_.SetPos({174,739+dy});cancel.SetPos({720,733+dy});place.SetPos({850,733+dy});status.SetPos({18,784+dy});
  window.SetSize({1020,840+dy});
 }
};
LightPickerWindow::LightPickerWindow():impl_(std::make_unique<Impl>(*this)){}
LightPickerWindow::~LightPickerWindow(){RemoveWidgets();}
void LightPickerWindow::Create() {
 wi::gui::Window::Create("Light Picker",WindowControls::CLOSE|WindowControls::MOVE);
 SetSize({1020,840});SetControlSize(28);SetColor(wi::Color(33,35,37,255),wi::gui::WIDGET_ID_WINDOW_BASE);
 SetColor(wi::Color(33,35,37,255),wi::gui::WIDGET_ID_COMBO_DROPDOWN);
 impl_->Build();SetVisible(false);
}
void LightPickerWindow::Open(bridge::StudioSession& session,wi::scene::LightComponent::LightType type,wi::ecs::Entity target) {
 SetVisible(true);
 wi::gui::Theme theme;theme.image.background=false;theme.image.color=wi::Color(40,42,44,255);
 theme.font.color=wi::Color(235,238,239,255);theme.shadow=0;
 theme.tooltipImage.color=Grey;theme.tooltipFont.color=wi::Color(235,238,239,255);
 SetTheme(theme);SetColor(wi::Color(43,46,48,255),wi::gui::IDLE);
 SetColor(wi::Color(65,78,77,255),wi::gui::FOCUS);SetColor(wi::Color(60,108,101,255),wi::gui::ACTIVE);
 SetColor(wi::Color(33,35,37,255),wi::gui::WIDGET_ID_WINDOW_BASE);
 SetColor(wi::Color(33,35,37,255),wi::gui::WIDGET_ID_COMBO_DROPDOWN);
 auto& i=*impl_;
 for(auto* heading:std::array<wi::gui::Label*,6>{&i.savedTitle,&i.typeTitle,&i.colourTitle,&i.behaviourTitle,&i.settingsTitle,&i.previewTitle}){heading->font.params.size=16;heading->font.params.bolden=.2f;}
 i.previewImage.SetColor(wi::Color::White());for(auto& sprite:i.previewImage.sprites)sprite.params.disableBackground();
 i.place.SetColor(Teal);
 for(auto* caption:i.captions)for(auto& sprite:caption->sprites)sprite.params.color=wi::Color::Transparent();
 i.session=&session;i.project=session.Projects().CurrentProject().projectId;
 i.sceneRevision=session.Scenes().Revision();i.target=target;i.placeRequest=i.applyRequest=false;i.expanded=false;
 std::string error;session.Scripts().EnsureCurrent(error);
 bridge::LightPreset p;
 if(target!=wi::ecs::INVALID_ENTITY)p=bridge::CaptureLightPreset(session.Scenes().GetScene(),target,session.Scripts().Document());
 else {p.light=bridge::MakeNewLightState(type);p.name=type==wi::scene::LightComponent::SPOT?"Spot Light":type==wi::scene::LightComponent::DIRECTIONAL?"Directional Light":type==wi::scene::LightComponent::RECTANGLE?"Area Light":"Point Light";}
 i.search.SetText("");i.Set(p);i.Reload();i.Advanced();i.colour.HideFields();
 i.place.SetText(target==wi::ecs::INVALID_ENTITY?"Place Light":"Apply to Light");
 i.keep.SetEnabled(target==wi::ecs::INVALID_ENTITY);i.status.SetText("Choose a light setup. Saved presets include behaviour and settings.");
}
bridge::LightPreset LightPickerWindow::Draft() const{return impl_->Read();}
bool LightPickerWindow::KeepPlacing() const{return impl_->keep.GetCheck();}
bool LightPickerWindow::TakePlaceRequest(){return std::exchange(impl_->placeRequest,false);}
bool LightPickerWindow::TakeApplyRequest(){return std::exchange(impl_->applyRequest,false);}
wi::ecs::Entity LightPickerWindow::Target() const{return impl_->target;}
bool LightPickerWindow::MatchesSession(const bridge::StudioSession& s) const {
 return s.Projects().HasProject()&&s.Projects().CurrentProject().projectId==impl_->project&&s.Scenes().Revision()==impl_->sceneRevision;
}
void LightPickerWindow::ShowError(const std::string& error){impl_->status.SetText(error);}
void LightPickerWindow::UpdatePreview(float dt) {
 auto& i=*impl_;if(!IsVisible())return;
 if(!i.preview||i.preview->root!=i.session->Projects().CurrentProject().rootPath) {
  i.preview=std::make_unique<LightPreview>(i.session->Projects().CurrentProject().rootPath);i.dirtyPreview=true;
 }
 if(i.dirtyPreview){i.preview->Setup(i.Read());i.dirtyPreview=false;}
 i.preview->Tick(dt,i.animate.GetCheck());wi::Resource image;image.SetTexture(i.preview->GetRenderResult3D());i.previewImage.SetImage(image);
}
void LightPickerWindow::PreRenderPreview() {
 if(IsVisible()&&impl_->preview&&impl_->preview->renderPending)impl_->preview->PreRender();
}
void LightPickerWindow::RenderPreview() const {if(IsVisible()&&impl_->preview)impl_->preview->Render();}
}
