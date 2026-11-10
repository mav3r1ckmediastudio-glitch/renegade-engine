#include "ParticlePickerWindow.h"
#include "ParticleTextureBrowser.h"
#include <unordered_map>
#include "RenegadeStudioChrome.h"
#include "renegade/bridge/StudioSession.h"
#include "renegade/bridge/ParticleEffectRuntimeService.h"
#include "renegade/bridge/CreatorTextureWorkflowService.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
namespace renegade::studio {
namespace {
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


class ParticleControl : public wi::gui::Slider {
public:
 float minimum=0,maximum=1,increment=.01f;bool integer=false;
 void Configure(float lo,float hi,float stepSize,bool whole=false,int precision=2){
  minimum=lo;maximum=hi;increment=stepSize;integer=whole;start=lo;end=hi;step=std::max(1.0f,std::round((hi-lo)/stepSize));valueInputField.SetFloatPrecision(whole?0:precision);
 }
 float Quantize(float v)const {if(!std::isfinite(v))return GetValue();v=std::clamp(v,minimum,maximum);return integer?std::round(v):std::round(v/increment)*increment;}
 // Let the native Slider place and update its numeric field together.
};
class LifetimeGraph:public PickerButton {
public:
 bridge::ParticleLifetimeCurve curve;size_t selected=0;
 void Render(const wi::Canvas& canvas,wi::graphics::CommandList cmd)const override {
  if(!IsVisible())return;ApplyScissor(canvas,scissorRect,cmd);
  const float x=translation.x,y=translation.y,w=scale.x;
  Rect(x,y,w,scale.y,wi::Color(17,22,23,255),cmd);
  const auto text=[&](const char* value,float top){wi::font::Params fp(x+6,y+top,10);fp.color=wi::Color(205,217,215,255);wi::font::Draw(value,fp,cmd);};
  text("Colour",10);text("Size",47);text("0-400%",60);text("Opacity",91);text("0-100%",104);
  auto display=curve;display.enabled=true;
  const float left=x+76,width=w-86;
  Rect(left,y+41,width,32,wi::Color(34,40,41,255),cmd);Rect(left,y+84,width,32,wi::Color(34,40,41,255),cmd);
  for(int n=0;n<128;++n){
   auto k=bridge::SampleParticleLifetime(display,float(n)/127);
   const float column=left+width*n/128,step=width/128+1;
   Rect(column,y+8,step,20,wi::Color(uint8_t(k.colour.x*255),uint8_t(k.colour.y*255),uint8_t(k.colour.z*255),255),cmd);
   Rect(column,y+71-k.size/4*28,step,2,Teal,cmd);
   Rect(column,y+114-k.colour.w*28,step,2,wi::Color(235,238,239,255),cmd);
  }
  for(size_t n=0;n<curve.keys.size();++n){
   const auto& k=curve.keys[n];float px=left+k.time*width;
   if(n==selected)Rect(px-1,y+5,2,113,wi::Color(137,225,208,95),cmd);
   Rect(px-3,y+70-k.size/4*28,6,6,n==selected?Teal:wi::Color(213,221,219,255),cmd);
   Rect(px-3,y+113-k.colour.w*28,6,6,n==selected?Teal:wi::Color(213,221,219,255),cmd);
  }
 }
};
class ParticlePreview:public wi::RenderPath3D {
public:
 wi::allocator::shared_ptr<wi::scene::Scene> owned;
 wi::scene::CameraComponent view;
 std::string root;bridge::StableId project;
 wi::ecs::Entity effect=wi::ecs::INVALID_ENTITY;
 mutable bool pending=false;float clock=0;bool refresh=true;bool fitted=false;
 XMFLOAT3 centre={0,1,0};XMFLOAT4 orientation={0,0,0,1};float distance=4;
 int drag=0;XMFLOAT2 lastPointer={};
 std::vector<wi::ecs::Entity> layerEntities;
 std::vector<bridge::ParticleMovementField> fields;bool showFields=false;size_t chosenField=0;float fieldClock=0;
 ParticlePreview(std::string r,bridge::StableId p):root(std::move(r)),project(std::move(p)){
  owned=wi::allocator::make_shared<wi::scene::Scene>();scene=owned.get();camera=&view;
  auto floor=scene->Entity_CreateCube("Preview floor");auto* t=scene->transforms.GetComponent(floor);t->Scale(XMFLOAT3(4,.05f,4));t->Translate(XMFLOAT3(0,-.1f,0));t->UpdateTransform();
  auto* object=scene->objects.GetComponent(floor);auto* mesh=scene->meshes.GetComponent(object->meshID);auto* material=scene->materials.GetComponent(mesh->subsets[0].materialID);material->baseColor={.22f,.23f,.24f,1};material->roughness=1;material->SetDirty();
  auto w=wi::ecs::CreateEntity();auto& weather=scene->weathers.Create(w);
  weather.horizon=weather.zenith={.045f,.05f,.055f};weather.ambient={.3f,.3f,.3f};scene->weather=weather;
  auto sun=scene->Entity_CreateLight("Preview fill",{0,3,-2});scene->lights.GetComponent(sun)->SetType(wi::scene::LightComponent::DIRECTIONAL);scene->lights.GetComponent(sun)->intensity=1;
  init(480,350,96);setOcclusionCullingEnabled(false);setBloomEnabled(true);setEyeAdaptionEnabled(false);
  setMotionBlurEnabled(false);setDepthOfFieldEnabled(false);setFXAAEnabled(true);setAO(AO_DISABLED);setSSREnabled(false);setSSGIEnabled(false);setReflectionsEnabled(false);
  view.CreatePerspective(480,350,.005f,200,XM_PI/3);
  view.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(XMVectorSet(3,2,-4,1),XMVectorSet(0,1,0,1),XMVectorSet(0,1,0,0))));view.UpdateCamera();ResizeBuffers();
 }

 void Camera(){
  auto q=XMLoadFloat4(&orientation);
  auto target=XMLoadFloat3(&centre);
  auto eye=target+XMVector3Rotate(XMVectorSet(0,0,-distance,0),q);
  auto up=XMVector3Rotate(XMVectorSet(0,1,0,0),q);
  view.TransformCamera(XMMatrixInverse(nullptr,XMMatrixLookAtLH(eye,target,up)));view.UpdateCamera();refresh=true;
 }
 void Fit(const bridge::ParticleEffect& effect){
  float extent=.12f,top=.12f;
  for(const auto& l:effect.layers)if(l.enabled){
   float travel=l.playback==bridge::ParticlePlayback::StationaryLoop?0:std::abs(l.emitter.velocity.y)*l.emitter.life;
   float maxSize=l.emitter.size;if(l.lifetime.enabled)for(const auto& key:l.lifetime.keys)maxSize=std::max(maxSize,l.emitter.size*key.size);
   float shapeExtent=int(l.shape.type)>1?2*(l.shape.radius+l.shape.thickness*.5f)+l.shape.height:0;float h=std::max({.06f,2*maxSize+travel,shapeExtent});
   extent=std::max(extent,h);top=std::max(top,l.transform.translation.y+h*.5f);
  }
  for(const auto& f:effect.fields)if(f.enabled){extent=std::max(extent,2*std::max({f.transform.scale.x,f.transform.scale.y,f.transform.scale.z}));top=std::max(top,f.transform.translation.y+f.transform.scale.y);}
  extent=std::clamp(extent,.12f,20.0f);centre={0,top*.5f,0};distance=extent*2.1f;
  XMStoreFloat4(&orientation,XMQuaternionRotationRollPitchYaw(.18f,-.35f,0));Camera();
 }
 void Navigate(const XMFLOAT4& pointer,XMFLOAT2 pos,XMFLOAT2 size){
  bool inside=pointer.x>=pos.x&&pointer.y>=pos.y&&pointer.x<pos.x+size.x&&pointer.y<pos.y+size.y;
  if(drag&&!wi::input::Down(wi::input::MOUSE_BUTTON_LEFT)&&!wi::input::Down(wi::input::MOUSE_BUTTON_RIGHT))drag=0;
  if(!drag&&inside){
   if(wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))drag=wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT)?3:1;
   else if(wi::input::Press(wi::input::MOUSE_BUTTON_RIGHT))drag=2;
   if(drag)lastPointer={pointer.x,pointer.y};
  }
  if(inside&&pointer.z!=0){distance=std::clamp(distance*std::exp(-std::clamp(pointer.z,-10.0f,10.0f)*.15f),.04f,200.0f);Camera();}
  if(!drag)return;
  float dx=pointer.x-lastPointer.x,dy=pointer.y-lastPointer.y;lastPointer={pointer.x,pointer.y};
  if(dx==0&&dy==0)return;
  auto q=XMLoadFloat4(&orientation);
  if(drag==2){
   auto right=XMVector3Rotate(XMVectorSet(1,0,0,0),q),up=XMVector3Rotate(XMVectorSet(0,1,0,0),q);
   XMStoreFloat3(&centre,XMLoadFloat3(&centre)+(-dx*right+dy*up)*(distance*.0015f));
  }else if(drag==3){XMStoreFloat4(&orientation,XMQuaternionNormalize(XMQuaternionMultiply(XMQuaternionRotationAxis(XMVectorSet(0,0,1,0),dx*.008f),q)));}
  else {
   // Quaternion rotations allow repeated full turns and pass the poles without a pitch clamp.
   auto delta=XMQuaternionRotationRollPitchYaw(dy*.008f,dx*.008f,0);
   XMStoreFloat4(&orientation,XMQuaternionNormalize(XMQuaternionMultiply(delta,q)));
  }
  Camera();
 }
 bool Setup(const bridge::ParticleEffect& draft,std::string& error){
  fields=draft.fields;layerEntities.clear();
  if(effect!=wi::ecs::INVALID_ENTITY)scene->Entity_Remove(effect);
  bridge::ParticleEffectCommand command(*scene,root,project,draft);if(!command.Execute()){error=command.Error();effect=wi::ecs::INVALID_ENTITY;return false;}effect=command.Entity();for(size_t n=0;n<scene->emitters.GetCount();++n)layerEntities.push_back(scene->emitters.GetEntity(n));if(!fitted){Fit(draft);fitted=true;}refresh=true;return true;
 }
 bool ResizeLayer(size_t index,const bridge::ParticleEffectLayer& layer,const std::vector<bridge::ParticleMovementField>& movement){
  if(effect==wi::ecs::INVALID_ENTITY||index>=layerEntities.size())return false;
  auto entity=layerEntities[index];auto* emitter=scene->emitters.GetComponent(entity);if(!emitter)return false;
  bridge::ApplyParticleEffectLayerSettings(*scene,effect,entity,layer,movement,true);
  if(layer.playback==bridge::ParticlePlayback::Continuous)emitter->Restart();
  refresh=true;return true;
 }
 void DrawField() {
  if(!showFields||chosenField>=fields.size())return;const auto& field=fields[chosenField];const auto& t=field.transform;
  auto matrix=XMMatrixScaling(t.scale.x,t.scale.y,t.scale.z)*XMMatrixRotationQuaternion(XMLoadFloat4(&t.rotation))*XMMatrixTranslation(t.translation.x,t.translation.y,t.translation.z);
  XMFLOAT4X4 box;XMStoreFloat4x4(&box,matrix);wi::renderer::DrawBox(box,{.35f,.8f,.72f,.65f});
  for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z){
   XMFLOAT3 pos;XMStoreFloat3(&pos,XMVector3TransformCoord(XMVectorSet(x*.6f,y*.6f,z*.6f,1),matrix));
   auto velocity=bridge::SampleParticleMovementField(field,pos,fieldClock);auto v=XMLoadFloat3(&velocity);float len=XMVectorGetX(XMVector3Length(v));if(len<.001f)continue;
   auto dir=v/len;float length=std::min({t.scale.x,t.scale.y,t.scale.z})*.25f;auto start=XMLoadFloat3(&pos),end=start+dir*length;
   XMFLOAT3 tip;XMStoreFloat3(&tip,end);wi::renderer::DrawLine({pos,tip,{.55f,1,.85f,1},{.55f,1,.85f,1}});
   auto side=XMVector3Cross(dir,XMVectorSet(0,1,0,0));if(XMVectorGetX(XMVector3LengthSq(side))<.001f)side=XMVector3Cross(dir,XMVectorSet(1,0,0,0));side=XMVector3Normalize(side);
   for(float sign:{-1.0f,1.0f}){XMFLOAT3 tail;XMStoreFloat3(&tail,end-dir*length*.3f+side*length*.15f*sign);wi::renderer::DrawLine({tip,tail,{.55f,1,.85f,1},{.55f,1,.85f,1}});}
  }
 }
 void Tick(float dt,bool animate){if(animate)fieldClock+=dt;clock+=dt;if(clock<1.0f/30||(!animate&&!refresh))return;pending=true;float step=animate?std::min(clock,.1f):0;bridge::UpdateParticleEffectSprites(*scene,step);PreUpdate();wi::RenderPath3D::Update(step);clock=0;refresh=false;}
 void Render()const override{if(pending){const_cast<ParticlePreview*>(this)->DrawField();wi::RenderPath3D::Render();pending=false;}}
};
}
struct ParticlePickerWindow::Impl {
 ParticlePickerWindow& window;bridge::StudioSession* session=nullptr;bridge::StableId project;uint64_t revision=0;
 wi::ecs::Entity target=wi::ecs::INVALID_ENTITY;
 bridge::ParticleEffect draft,baseline;std::vector<bridge::ParticleEffect> presets;size_t selected=0;
 bool dirty=true,request=false,setting=false,advanced=false;
 std::vector<size_t> pendingLayerResizes;
 std::unordered_map<bridge::StableId,float> layerSizeReferences;
 std::unique_ptr<ParticlePreview> preview;
 wi::gui::TreeList list,layers;
 std::vector<wi::gui::Label*> captions;
 RenegadeTextInputField search,name,layerName;
 wi::gui::Label image,status,title,textureTitle,settingsTitle,layerTitle,saveHint;
 PickerButton add,copy,remove,saveNew,update,apply,cancel,choose,advancedButton,solo,allFrames;
 PickerCheckBox keep,animate,enabled,blendFrames;
 ParticleControl size,amount,height,spread,red,green,blue,opacity,brightness,particleSize,cols,rows,frames,fps;
 wi::gui::ComboBox blend,textureSlot,shader,playback;
 std::array<ParticleControl,12> detail;
 wi::gui::ComboBox detailPage;
 std::array<float,12> simulation{};std::array<PickerCheckBox,8> flags;
 bool timingMode=false;PickerButton timingButton,shapeButton;PickerCheckBox timingEnabled,shapeSurface;wi::gui::ComboBox timingLoop,shapeType;std::array<ParticleControl,3> timingValues;wi::gui::Label timingHelp;
 bool lifetimeMode=false;size_t selectedKey=0;
 PickerButton lifetimeButton,addKey,removeKey,resetCurve;PickerCheckBox curveEnabled;
 wi::gui::ComboBox keyList;std::array<ParticleControl,6> curveValues;wi::gui::Label curveHelp;LifetimeGraph curveGraph;
 bool soloActive=false,fieldsMode=false;size_t selectedField=0;int fieldPageIndex=0;
 PickerButton fieldsButton,variationButton,addField,removeField;
 PickerCheckBox fieldEnabled,clockwise,showField;
 RenegadeTextInputField fieldName;
 wi::gui::ComboBox fieldList,fieldType,fieldPage;
 std::array<ParticleControl,6> fieldValues;
 ParticleControl fieldWeight;
 wi::gui::Label fieldHelp;

 ParticleTextureBrowser textureBrowser;
 std::unordered_map<bridge::StableId,bridge::ParticleTextureEntry> textureProfiles;
 Impl(ParticlePickerWindow&w):window(w){}
 template<class T> void Add(T& w,float x,float y,float width,float height){w.SetPos({x,y});w.SetSize({width,height});window.AddWidget(&w);w.SetShadowRadius(0);}
 void Label(wi::gui::Label&w,const char*text,float x,float y,float width,int size=15){w.Create(text);w.SetText(text);w.font.params.size=size;w.font.params.bolden=.15f;Add(w,x,y,width,24);captions.push_back(&w);}
 void Button(PickerButton&w,const char*text,float x,float y,float width){w.Create(text);w.SetText(text);Add(w,x,y,width,30);}
 void Slider(ParticleControl&w,const char*text,float lo,float hi,float value,float x,float y,float width){
  w.Create(lo,hi,value,100,text);w.SetText(text);w.Configure(lo,hi,.01f);Add(w,x,y,width,22);
  auto changed=[this,&w](float value){if(setting)return;w.SetValue(w.Quantize(value));Flush();dirty=true;};
  w.OnSlide([changed](const wi::gui::EventArgs&a){changed(a.fValue);});
  w.valueInputField.SetTooltip("Enter an exact value within this control range.");
  w.valueInputField.OnInputAccepted([changed](const wi::gui::EventArgs&a){changed(a.fValue);});
 }
 void Check(PickerCheckBox&w,const char*text,bool value,float x,float y){w.Create(text);w.SetCheck(value);Add(w,x,y,20,20);w.OnClick([this](const wi::gui::EventArgs&){if(!setting){Flush();dirty=true;}});}
 float LayerSizeReference(){
  auto& layer=draft.layers[selected];auto [it,inserted]=layerSizeReferences.emplace(layer.id,std::max(.000001f,layer.emitter.size));return it->second;
 }
 void SyncLayerSize(){
  if(selected>=draft.layers.size())return;
  float reference=LayerSizeReference();float percent=draft.layers[selected].emitter.size/reference*100;
  constexpr float minimum=1;
  particleSize.Configure(minimum,std::max(300.0f,std::ceil(percent)),1,true,0);particleSize.SetValue(percent);
 }
 void ChangeLayerPercent(float percent){
  if(setting||selected>=draft.layers.size())return;
  ChangeLayerSize(LayerSizeReference()*particleSize.Quantize(percent)/100);
 }
 void ChangeLayerSize(float value){
  if(setting||selected>=draft.layers.size())return;
  if(!std::isfinite(value))return;draft.layers[selected].emitter.size=std::clamp(value,.000001f,10000.0f);SyncLayerSize();
  DetailSet();
  if(std::find(pendingLayerResizes.begin(),pendingLayerResizes.end(),selected)==pendingLayerResizes.end())pendingLayerResizes.push_back(selected);
 }
 void Flush(){
  if(setting||selected>=draft.layers.size())return;
  if(timingMode){draft.timing.enabled=timingEnabled.GetCheck();draft.timing.loop=timingLoop.GetSelected()==0;draft.timing.duration=timingValues[0].GetValue();draft.timing.delay=timingValues[1].GetValue();draft.timing.burst=uint32_t(timingValues[2].GetValue());return;}
  if(fieldsMode){FieldRead();return;}
  if(lifetimeMode){auto& l=draft.layers[selected];l.name=layerName.GetText();l.enabled=enabled.GetCheck();CurveRead();return;}
  auto& l=draft.layers[selected];l.playback=(bridge::ParticlePlayback)playback.GetSelected();l.name=layerName.GetText();l.enabled=enabled.GetCheck();
  l.emitter.color={red.GetValue(),green.GetValue(),blue.GetValue(),opacity.GetValue()};l.material.baseColor=l.emitter.color;
  l.emitter.emissiveStrength=brightness.GetValue();l.emitter.framesX=(uint32_t)cols.GetValue();l.emitter.framesY=(uint32_t)rows.GetValue();
  l.emitter.frameCount=std::clamp((uint32_t)frames.GetValue(),1u,l.emitter.framesX*l.emitter.framesY);frames.Configure(1,float(l.emitter.framesX*l.emitter.framesY),1,true);frames.SetValue(float(l.emitter.frameCount));l.emitter.frameRate=fps.GetValue();l.emitter.frameBlending=blendFrames.GetCheck();
  l.material.blendMode=(wi::enums::BLENDMODE)blend.GetSelected();
  if(advanced){l.emitter.shaderType=(wi::EmittedParticleSystem::PARTICLESHADERTYPE)shader.GetSelected();DetailRead();}
  else {DetailSet();}
  SyncLayerSize();
 }
 bridge::ParticleEffect Read()const{
  auto p=draft;p.name=name.GetText();
  for(auto& l:p.layers)l=bridge::ScaleParticleLayer(l,size.GetValue()/100,amount.GetValue()/100,height.GetValue()/100,spread.GetValue()/100);
  float overall=size.GetValue()/100;
  for(auto& field:p.fields){field.transform.translation.x*=overall;field.transform.translation.y*=overall;field.transform.translation.z*=overall;field.transform.scale.x*=overall;field.transform.scale.y*=overall;field.transform.scale.z*=overall;field.strength*=overall;field.lift*=overall;}
  return p;
 }
 void LayerRows(){
  bool continuous=std::any_of(draft.layers.begin(),draft.layers.end(),[](const auto& l){return l.playback==bridge::ParticlePlayback::Continuous;});
  amount.SetEnabled(continuous);height.SetEnabled(continuous);spread.SetEnabled(continuous);
  layers.ClearItems();for(size_t i=0;i<draft.layers.size();++i){wi::gui::TreeList::Item row;row.name=(draft.layers[i].enabled?"":"[off] ")+draft.layers[i].name;row.userdata=i;row.selected=i==selected;layers.AddItem(row);}
 }
 void Rows(){
  list.ClearItems();auto filter=search.GetText();std::transform(filter.begin(),filter.end(),filter.begin(),::tolower);
  for(size_t i=0;i<presets.size();++i){auto n=presets[i].name;std::transform(n.begin(),n.end(),n.begin(),::tolower);if(n.find(filter)==std::string::npos)continue;
   wi::gui::TreeList::Item row;row.name=presets[i].name;row.userdata=i;row.selected=!draft.id.empty()&&presets[i].id==draft.id;list.AddItem(row);}
 }
 void Set(bridge::ParticleEffect p){
  setting=true;if(preview)preview->fitted=false;draft=std::move(p);baseline=draft;layerSizeReferences.clear();for(const auto& layer:draft.layers)layerSizeReferences[layer.id]=std::max(.000001f,layer.emitter.size);selected=0;name.SetText(draft.name);
  size.SetValue(100);amount.SetValue(100);height.SetValue(100);spread.SetValue(100);setting=false;
  update.SetEnabled(!draft.id.empty());LayerRows();SelectLayer();Visibility();dirty=true;
 }
 void SelectLayer(){
  if(selected>=draft.layers.size())return;setting=true;auto& l=draft.layers[selected];
  playback.SetSelected(int(l.playback));layerName.SetText(l.name);enabled.SetCheck(l.enabled);red.SetValue(l.emitter.color.x);green.SetValue(l.emitter.color.y);blue.SetValue(l.emitter.color.z);opacity.SetValue(l.emitter.color.w);
  SyncLayerSize();brightness.SetValue(l.emitter.emissiveStrength);cols.SetValue(float(l.emitter.framesX));rows.SetValue(float(l.emitter.framesY));frames.Configure(1,float(std::max(1u,l.emitter.framesX*l.emitter.framesY)),1,true);frames.SetValue(float(l.emitter.frameCount));fps.SetValue(l.emitter.frameRate);
  blendFrames.SetCheck(l.emitter.frameBlending);blend.SetSelected(int(l.material.blendMode));
  shader.SetSelected(int(l.emitter.shaderType));settingsTitle.SetText("Layer: "+l.name);blendFrames.SetEnabled(l.playback==bridge::ParticlePlayback::Continuous);blendFrames.SetTooltip(l.playback==bridge::ParticlePlayback::StationaryLoop?"Stationary sprites use discrete frame animation.":"Blend adjacent particle frames.");textureTitle.SetText(l.texture.empty()?(l.extraTextures[0].empty()?"No sprite texture assigned":"Normal / distortion texture assigned"):"Project sprite texture assigned");
  auto key=l.emitter.shaderType==wi::EmittedParticleSystem::SOFT_DISTORTION&&!l.extraTextures[0].empty()?l.extraTextures[0]:l.texture;
  auto profile=textureProfiles.find(key);bool fixed=profile!=textureProfiles.end()&&profile->second.layoutKnown&&!profile->second.animated;
  for(auto* control:{&cols,&rows,&frames,&fps})control->SetEnabled(!fixed);allFrames.SetEnabled(!fixed);blendFrames.SetEnabled(!fixed&&l.playback==bridge::ParticlePlayback::Continuous);
  if(profile!=textureProfiles.end())textureTitle.SetText(profile->second.name+(fixed?" / static":" / animated sheet"));
  DetailSet();FieldSet();CurveSet();TimingSet();setting=false;Visibility();if(soloActive)dirty=true;
 }
 std::array<float*,12> Details(){
  if(selected>=draft.layers.size())return {};auto& l=draft.layers[selected];auto& e=l.emitter;
  switch(detailPage.GetSelected()){
  case 5:return {nullptr,&l.shape.radius,&l.shape.height,&l.shape.angle,&l.shape.thickness,&e.normalFactor,&simulation[0],&simulation[1],&simulation[2],&l.transform.scale.x,&l.transform.scale.y,&l.transform.scale.z};
  case 4:return {&simulation[0],&simulation[1],&simulation[2],&l.transform.scale.x,&l.transform.scale.y,&l.transform.scale.z,&l.transform.translation.x,&l.transform.translation.y,&l.transform.translation.z,&e.emitCount,&e.size,&e.life};
  case 1:return {&e.velocity.x,&e.velocity.y,&e.velocity.z,&e.gravity.x,&e.gravity.y,&e.gravity.z,&e.drag,&e.rotation,&e.motionBlurAmount,&e.mass,&e.randomFactor,&e.randomLife};
  case 3:return {&simulation[0],&simulation[1],&simulation[2],&simulation[3],&simulation[4],&simulation[5],&simulation[6],&simulation[7],&simulation[8],&simulation[9],&simulation[10],&simulation[11]};
  case 2:return {&l.transform.translation.x,&l.transform.translation.y,&l.transform.translation.z,&l.transform.scale.x,&l.transform.scale.y,&l.transform.scale.z,&e.opacityPeakStart,&e.opacityPeakEnd,&e.scaleX,&e.scaleY,&e.normalFactor,&e.randomColor};
  default:return {&e.emitCount,&e.size,&e.life,&e.randomLife,&e.randomFactor,&e.normalFactor,&e.scaleX,&e.scaleY,&e.opacityPeakStart,&e.opacityPeakEnd,&e.rotation,&e.motionBlurAmount};
  }
 }
 void DetailSet(){
  bool oldSetting=setting;setting=true;
  const char* labels[6][12]={{"Emit / sec","Particle size","Lifetime","Life variation","Randomness","Normal factor","Scale X","Scale Y","Fade peak start","Fade peak end","Rotation","Motion blur"},
   {"Velocity X","Velocity Y","Velocity Z","Gravity X","Gravity Y","Gravity Z","Drag","Rotation","Motion blur","Mass","Randomness","Life variation"},
   {"Offset X","Offset Y","Offset Z","Scale X","Scale Y","Scale Z","Fade peak start","Fade peak end","Particle scale X","Particle scale Y","Normal factor","Colour variation"},
   {"Max particles","Burst on create","Start frame","Fixed timestep","SPH smoothing","SPH pressure","SPH density","SPH viscosity","Restitution","Emissive R","Emissive G","Emissive B"},
   {"Size variation %","Speed variation %","Life variation %","Spawn half width","Spawn half height","Spawn half depth","Spawn centre X","Spawn centre Y","Spawn centre Z","Emit / sec","Base size","Lifetime"},
   {"","Radius","Height","Cone angle","Ring thickness","Outward speed","Pitch","Yaw","Roll","Half width / X","Half height / Y","Half depth / Z"}};
  if(selected<draft.layers.size()){
   const auto& e=draft.layers[selected].emitter;
   simulation={float(e.maxParticles),float(e.burstOnCreate),float(e.frameStart),e.fixedTimestep,e.sphH,e.sphK,e.sphP0,e.sphE,e.restitution,e.emissiveColor.x,e.emissiveColor.y,e.emissiveColor.z};
   if(detailPage.GetSelected()==4){simulation[0]=draft.layers[selected].sizeVariation*100;simulation[1]=draft.layers[selected].speedVariation*100;simulation[2]=e.randomLife*100;}
   const bool values[]={e.paused,e.sorted,e.depthCollision,e.sph,e.volume,e.collidersDisabled,e.takeColorFromMesh,e.frameBlending};
   for(int n=0;n<8;++n)flags[n].SetCheck(values[n]);
  }
  if(selected<draft.layers.size()&&detailPage.GetSelected()==5){const auto& l=draft.layers[selected];const auto& q=l.transform.rotation;
   shapeType.SetSelected(int(l.shape.type));shapeSurface.SetCheck(l.shape.surface);
   simulation[0]=XMConvertToDegrees(std::asin(std::clamp(2*(q.w*q.x-q.y*q.z),-1.0f,1.0f)));
   simulation[1]=XMConvertToDegrees(std::atan2(2*(q.w*q.y+q.x*q.z),1-2*(q.x*q.x+q.y*q.y)));
   simulation[2]=XMConvertToDegrees(std::atan2(2*(q.w*q.z+q.x*q.y),1-2*(q.x*q.x+q.z*q.z)));
  }
  auto fields=Details();int page=std::max(0,detailPage.GetSelected());
  struct Range{float lo,hi,step;bool whole=false;int precision=2;};
  const Range ranges[6][12]={
   {{0,200,1,true},{.005f,2,.005f,false,3},{.05f,6,.05f,false,3},{0,1,.01f},{0,1,.01f},{0,1,.01f},{.1f,3,.05f},{.1f,3,.05f},{0,1,.01f},{0,1,.01f},{-3.14f,3.14f,.05f},{0,1,.01f}},
   {{-2,2,.05f},{-2,3,.05f},{-2,2,.05f},{-10,10,.1f},{-10,10,.1f},{-10,10,.1f},{0,1,.01f},{-3.14f,3.14f,.05f},{0,1,.01f},{.01f,10,.1f},{0,1,.01f},{0,1,.01f}},
   {{-2,2,.025f,false,3},{-2,2,.025f,false,3},{-2,2,.025f,false,3},{.1f,3,.05f},{.1f,3,.05f},{.1f,3,.05f},{0,1,.01f},{0,1,.01f},{.1f,3,.05f},{.1f,3,.05f},{0,1,.01f},{0,1,.01f}},
   {{1,5000,50,true},{0,100,1,true},{0,255,1,true},{-1,.033333f,.001f,false,4},{.05f,3,.05f},{0,500,5},{0,10,.1f},{0,1,.005f,false,3},{0,1,.01f},{0,1,.01f},{0,1,.01f},{0,1,.01f}},
   {{0,100,1,true},{0,100,1,true},{0,100,1,true},{.05f,10,.05f},{.05f,10,.05f},{.05f,10,.05f},{-5,5,.05f},{-5,5,.05f},{-5,5,.05f},{0,200,1,true},{.005f,2,.005f,false,3},{.05f,10,.05f}},
   {{0,1,.01f},{0,10,.05f},{0,10,.05f},{0,90,1,true},{0,5,.025f,false,3},{0,10,.05f},{-180,180,1,true},{-180,180,1,true},{-180,180,1,true},{.05f,10,.05f},{.05f,10,.05f},{.05f,10,.05f}}
  };
  for(int j=0;j<12;++j){auto r=ranges[page][j];detail[j].Configure(r.lo,r.hi,r.step,r.whole,r.precision);detail[j].SetText(labels[page][j]);if(fields[j])detail[j].SetValue(*fields[j]);}
  if(page==5)for(int j=9;j<12;++j){const char* scales[]={"Scale X","Scale Y","Scale Z"};if(draft.layers[selected].shape.type!=bridge::ParticleShapeType::Box)detail[j].SetText(scales[j-9]);}
  const bool stationary=selected<draft.layers.size()&&draft.layers[selected].playback==bridge::ParticlePlayback::StationaryLoop;
  for(int j=0;j<12;++j){bool relevant=!stationary||(page==0&&j==1)||(page==2&&j<6)||(page==3&&j==2);
   bool curve=selected<draft.layers.size()&&draft.layers[selected].lifetime.enabled&&!stationary;
   bool replaced=curve&&((page==0&&(j==6||j==8||j==9))||(page==2&&(j==6||j==7||j==8)));
   detail[j].SetEnabled(relevant&&!replaced);
   detail[j].SetTooltip(replaced?"Controlled by the Lifetime curve. Disable it to use this setting.":!relevant?"Not used by a stationary looping sprite.":"");
  }
  for(int j=0;j<8;++j)flags[j].SetEnabled(!stationary||j==0);
  if(page==5&&selected<draft.layers.size()){int type=int(draft.layers[selected].shape.type);shapeType.SetEnabled(!stationary&&draft.layers[selected].emitter.meshId==wi::ecs::INVALID_ENTITY);shapeSurface.SetEnabled(!stationary&&type==2);
   for(int j=0;j<12;++j){bool used=j>=6||(j==1&&type>=2)||(j==2&&type==6)||(j==3&&type==3)||(j==4&&type==5)||(j==5&&type>=2);detail[j].SetEnabled(used&&!stationary);}
  }

  setting=oldSetting;
 }
 void FieldRows(){
  fieldList.ClearItems();for(size_t n=0;n<draft.fields.size();++n)fieldList.AddItem(draft.fields[n].name,n);
  if(!draft.fields.empty())fieldList.SetSelected(int(selectedField));
 }
 void FieldRead(){
  if(setting||selectedField>=draft.fields.size()||selected>=draft.layers.size())return;
  auto& f=draft.fields[selectedField];f.name=fieldName.GetText();f.enabled=fieldEnabled.GetCheck();f.clockwise=clockwise.GetCheck();f.type=bridge::ParticleFieldType(fieldType.GetSelected());
  draft.layers[selected].fieldInfluence[selectedField]=fieldWeight.GetValue()/100;
  if(fieldPageIndex==0){f.strength=fieldValues[0].GetValue();f.lift=fieldValues[1].GetValue();f.falloff=fieldValues[2].GetValue()/100;f.noiseScale=fieldValues[3].GetValue();f.animationSpeed=fieldValues[4].GetValue();}
  else if(fieldPageIndex==1){f.transform.translation={fieldValues[0].GetValue(),fieldValues[1].GetValue(),fieldValues[2].GetValue()};f.transform.scale={fieldValues[3].GetValue(),fieldValues[4].GetValue(),fieldValues[5].GetValue()};}
  else {XMStoreFloat4(&f.transform.rotation,XMQuaternionRotationRollPitchYaw(XMConvertToRadians(fieldValues[0].GetValue()),XMConvertToRadians(fieldValues[1].GetValue()),XMConvertToRadians(fieldValues[2].GetValue())));}
 }
 void FieldSet(){
  bool previous=setting;setting=true;if(selectedField>=draft.fields.size())selectedField=0;FieldRows();
  bool valid=selectedField<draft.fields.size();fieldName.SetEnabled(valid);fieldEnabled.SetEnabled(valid);fieldType.SetEnabled(valid);fieldPage.SetEnabled(valid);removeField.SetEnabled(valid);clockwise.SetEnabled(valid);fieldWeight.SetEnabled(valid&&selected<draft.layers.size()&&draft.layers[selected].playback==bridge::ParticlePlayback::Continuous);
  if(valid){auto& f=draft.fields[selectedField];fieldName.SetText(f.name);fieldEnabled.SetCheck(f.enabled);clockwise.SetCheck(f.clockwise);fieldType.SetSelected(int(f.type));fieldWeight.SetValue(draft.layers[selected].fieldInfluence[selectedField]*100);
   const char* labels[3][6]={{"Strength","Upward lift","Edge fade %","Noise scale","Noise animation",""},{"Position X","Position Y","Position Z","Half width","Half height","Half depth"},{"Pitch","Yaw","Roll","","",""}};
   float values[6]={};
   if(fieldPageIndex==0){values[0]=f.strength;values[1]=f.lift;values[2]=f.falloff*100;values[3]=f.noiseScale;values[4]=f.animationSpeed;}
   else if(fieldPageIndex==1){values[0]=f.transform.translation.x;values[1]=f.transform.translation.y;values[2]=f.transform.translation.z;values[3]=f.transform.scale.x;values[4]=f.transform.scale.y;values[5]=f.transform.scale.z;}
   else {const auto& q=f.transform.rotation;values[0]=XMConvertToDegrees(std::asin(std::clamp(2*(q.w*q.x-q.y*q.z),-1.0f,1.0f)));values[1]=XMConvertToDegrees(std::atan2(2*(q.w*q.y+q.x*q.z),1-2*(q.x*q.x+q.y*q.y)));values[2]=XMConvertToDegrees(std::atan2(2*(q.w*q.z+q.x*q.y),1-2*(q.x*q.x+q.z*q.z)));}
   for(int j=0;j<6;++j){auto& control=fieldValues[j];control.SetText(labels[fieldPageIndex][j]);float lo=0,hi=5,step=.05f;
    if(fieldPageIndex==0){if(j==1){lo=-3;hi=3;}if(j==2){hi=100;step=1;}if(j==3)lo=.1f;if(j==4)hi=3;}
    else if(fieldPageIndex==1){lo=j<3?-5:.05f;hi=j<3?5:10;step=.05f;}
    else {lo=-180;hi=180;step=1;}
    control.Configure(lo,hi,step,step==1);control.SetValue(values[j]);control.SetEnabled(fieldPageIndex!=0||j<3||f.type==bridge::ParticleFieldType::Turbulence);
   }
   clockwise.SetEnabled(f.type==bridge::ParticleFieldType::Swirl);
  }else fieldName.SetText("Add a movement field");
  addField.SetEnabled(draft.fields.size()<4);setting=previous;
 }
 void CurveRead(){
  if(setting||selected>=draft.layers.size())return;
  auto& c=draft.layers[selected].lifetime;c.enabled=curveEnabled.GetCheck();
  if(selectedKey>=c.keys.size())return;auto& k=c.keys[selectedKey];
  if(selectedKey>0&&selectedKey+1<c.keys.size()){
   float lo=c.keys[selectedKey-1].time+.001f,hi=c.keys[selectedKey+1].time-.001f;
   if(lo<=hi)k.time=std::clamp(curveValues[0].GetValue()/100,lo,hi);
  }
  curveValues[0].SetValue(k.time*100);
  k.size=curveValues[1].GetValue()/100;
  k.colour={curveValues[3].GetValue()/100,curveValues[4].GetValue()/100,curveValues[5].GetValue()/100,curveValues[2].GetValue()/100};
  curveGraph.curve=c;curveGraph.selected=selectedKey;
 }
 void CurveSet(){
  if(selected>=draft.layers.size())return;bool old=setting;setting=true;
  auto& l=draft.layers[selected];auto& c=l.lifetime;selectedKey=std::min(selectedKey,c.keys.size()-1);
  bool continuous=l.playback==bridge::ParticlePlayback::Continuous,editable=continuous&&c.enabled;
  curveEnabled.SetCheck(c.enabled);curveEnabled.SetEnabled(continuous);
  keyList.ClearItems();
  for(size_t n=0;n<c.keys.size();++n)keyList.AddItem("Point "+std::to_string(n+1)+"  /  "+std::to_string(int(std::round(c.keys[n].time*100)))+"%",n);
  keyList.SetSelected(int(selectedKey));keyList.SetEnabled(editable);
  addKey.SetEnabled(editable&&c.keys.size()<8);removeKey.SetEnabled(editable&&selectedKey>0&&selectedKey+1<c.keys.size());resetCurve.SetEnabled(editable);
  const auto& k=c.keys[selectedKey];float values[]={k.time*100,k.size*100,k.colour.w*100,k.colour.x*100,k.colour.y*100,k.colour.z*100};
  for(int n=0;n<6;++n){curveValues[n].SetValue(values[n]);curveValues[n].SetEnabled(editable&&(n<2||l.emitter.shaderType!=wi::EmittedParticleSystem::SIMPLE));}
  curveValues[0].SetEnabled(editable&&selectedKey>0&&selectedKey+1<c.keys.size());
  curveGraph.curve=c;curveGraph.selected=selectedKey;curveGraph.SetEnabled(editable);
  curveHelp.SetText(!continuous?"Stationary loops have no particle lifetime. These settings are retained for continuous mode.":c.enabled&&l.emitter.shaderType==wi::EmittedParticleSystem::SIMPLE?"Simple debug shading shows size only. Choose Soft shading for colour and opacity.":c.enabled?"Click the graph to select a point. Size uses the initial particle size. Opacity replaces fade peaks; RGB tints the layer colour.":"Enable to change colour, size and opacity as particles age. Existing size and fade settings apply while this is off.");
  setting=old;
 }
 void DetailRead(){
  auto fields=Details();for(int j=0;j<12;++j)if(fields[j])*fields[j]=detail[j].GetValue();
  if(selected>=draft.layers.size())return;auto& e=draft.layers[selected].emitter;
  if(detailPage.GetSelected()==4){draft.layers[selected].sizeVariation=simulation[0]/100;draft.layers[selected].speedVariation=simulation[1]/100;e.randomLife=simulation[2]/100;}
  if(detailPage.GetSelected()==3){
   e.maxParticles=(uint32_t)std::clamp(simulation[0],1.0f,1000000.0f);e.burstOnCreate=(int)std::clamp(simulation[1],0.0f,1000000.0f);
   e.frameStart=(uint32_t)std::max(0.0f,simulation[2]);e.fixedTimestep=simulation[3];e.sphH=simulation[4];e.sphK=simulation[5];e.sphP0=simulation[6];e.sphE=simulation[7];e.restitution=simulation[8];
   e.emissiveColor.x=simulation[9];e.emissiveColor.y=simulation[10];e.emissiveColor.z=simulation[11];
  }
  bool previousVolume=e.volume;
  e.paused=flags[0].GetCheck();e.sorted=flags[1].GetCheck();e.depthCollision=flags[2].GetCheck();e.sph=flags[3].GetCheck();e.volume=flags[4].GetCheck();e.collidersDisabled=flags[5].GetCheck();e.takeColorFromMesh=flags[6].GetCheck();e.frameBlending=flags[7].GetCheck();
  auto& l=draft.layers[selected];
  if(detailPage.GetSelected()==5){l.shape.surface=shapeSurface.GetCheck();e.volume=l.shape.type==bridge::ParticleShapeType::Box;XMStoreFloat4(&l.transform.rotation,XMQuaternionRotationRollPitchYaw(XMConvertToRadians(simulation[0]),XMConvertToRadians(simulation[1]),XMConvertToRadians(simulation[2])));}
  else if(previousVolume!=e.volume)l.shape.type=e.volume?bridge::ParticleShapeType::Box:bridge::ParticleShapeType::Point;
 }
 void TimingSet(){bool oldSetting=setting;setting=true;timingEnabled.SetCheck(draft.timing.enabled);timingLoop.SetSelected(draft.timing.loop?0:1);timingValues[0].SetValue(draft.timing.duration);timingValues[1].SetValue(draft.timing.delay);timingValues[2].SetValue(float(draft.timing.burst));setting=oldSetting;}
 void Visibility(){
  if(timingMode)advanced=fieldsMode=lifetimeMode=false;
  if(lifetimeMode)advanced=fieldsMode=false;
  for(auto* w:std::array<wi::gui::Widget*,10>{&red,&green,&blue,&opacity,&brightness,&particleSize,&cols,&rows,&frames,&fps})w->SetVisible(!advanced&&!fieldsMode&&!lifetimeMode&&!timingMode);
  allFrames.SetVisible(!advanced&&!fieldsMode&&!lifetimeMode&&!timingMode);blendFrames.SetVisible(!advanced&&!fieldsMode&&!lifetimeMode&&!timingMode);blend.SetVisible(!advanced&&!fieldsMode&&!lifetimeMode&&!timingMode);textureSlot.SetVisible(!advanced&&!fieldsMode&&!lifetimeMode&&!timingMode);choose.SetVisible(!advanced&&!fieldsMode&&!lifetimeMode&&!timingMode);textureTitle.SetVisible(!advanced&&!fieldsMode&&!lifetimeMode&&!timingMode);
  for(auto& s:detail)s.SetVisible(advanced&&!fieldsMode&&!lifetimeMode);detailPage.SetVisible(advanced&&!fieldsMode&&!lifetimeMode);playback.SetVisible(!advanced&&!fieldsMode&&!lifetimeMode&&!timingMode);for(auto& flag:flags)flag.SetVisible(advanced&&!fieldsMode&&!lifetimeMode);shader.SetVisible(advanced&&!fieldsMode&&!lifetimeMode);
  advancedButton.SetText((advanced||fieldsMode||lifetimeMode||timingMode)?"Basic":"Advanced");
  layerName.SetVisible(!fieldsMode&&!timingMode);enabled.SetVisible(!fieldsMode&&!timingMode);
  for(auto* widget:std::array<wi::gui::Widget*,12>{&fieldName,&fieldEnabled,&clockwise,&showField,&fieldList,&fieldType,&fieldPage,&addField,&removeField,&fieldWeight,&fieldHelp,&fieldsButton})widget->SetVisible(fieldsMode);
  fieldsButton.SetVisible(true);lifetimeButton.SetVisible(true);
  for(auto* w:std::array<wi::gui::Widget*,8>{&curveEnabled,&keyList,&addKey,&removeKey,&resetCurve,&curveGraph,&curveHelp,&lifetimeButton})w->SetVisible(lifetimeMode);
  lifetimeButton.SetVisible(true);for(auto& control:curveValues)control.SetVisible(lifetimeMode);
  bool shape=advanced&&!fieldsMode&&!lifetimeMode&&detailPage.GetSelected()==5;
  shapeType.SetVisible(shape);shapeSurface.SetVisible(shape);if(shape){detail[0].SetVisible(false);for(auto& flag:flags)flag.SetVisible(false);}
  for(auto* w:std::array<wi::gui::Widget*,4>{&timingEnabled,&timingLoop,&timingHelp,&timingButton})w->SetVisible(timingMode);timingButton.SetVisible(true);
  for(auto& value:timingValues)value.SetVisible(timingMode);
  settingsTitle.SetText(timingMode?"Whole Effect Timing":selected<draft.layers.size()?"Layer: "+draft.layers[selected].name:"Layer Settings");
  bool valid=selectedField<draft.fields.size();
  for(int j=0;j<6;++j)fieldValues[j].SetVisible(fieldsMode&&valid&&(fieldPageIndex==1||(fieldPageIndex==0&&j<5)||(fieldPageIndex==2&&j<3)));
  fieldHelp.SetText("Shared field; influence applies to selected layer.");

 }
 void Save(bool asNew){
  Flush();auto p=Read();std::string error;const auto& pr=session->Projects().CurrentProject();
  if(!bridge::SaveParticleEffect(pr.rootPath,pr.projectId,p,asNew,error)){status.SetText(error);return;}
  Set(p);presets=bridge::ListParticleEffects(pr.rootPath,pr.projectId,error);Rows();status.SetText("Saved in Content/Particles. Other placed effects stay unchanged.");
 }
 void Choose(){
  Flush();if(!session||selected>=draft.layers.size())return;
  auto layer=draft.layers[selected].id;auto expectedProject=project;auto expectedRevision=revision;
  auto slot=textureSlot.GetSelected()==0?bridge::MaterialTextureSlot::BaseColor:textureSlot.GetSelected()==1?bridge::MaterialTextureSlot::Normal:textureSlot.GetSelected()==2?bridge::MaterialTextureSlot::Surface:textureSlot.GetSelected()==3?bridge::MaterialTextureSlot::Emissive:bridge::MaterialTextureSlot::Occlusion;
  window.SetEnabled(false);auto pos=window.GetPos();textureBrowser.SetPos({std::max(0.f,pos.x-15),std::max(0.f,pos.y+10)});
  textureBrowser.Open(session->Projects().CurrentProject().rootPath,project,draft.layers[selected].name,slot,
   [this,expectedProject,expectedRevision]{return window.IsVisible()&&session&&session->Projects().HasProject()&&session->Projects().CurrentProject().projectId==expectedProject&&session->Scenes().Revision()==expectedRevision;},
   [this,layer,slot](const bridge::ParticleTextureEntry& entry,bool recommended){
    auto it=std::find_if(draft.layers.begin(),draft.layers.end(),[&](auto& l){return l.id==layer;});if(it==draft.layers.end())return;
    bridge::ApplyParticleTextureToLayer(*it,entry,slot,recommended);textureProfiles[entry.assetId]=entry;
    if(entry.distortion)textureSlot.SetSelected(1);SelectLayer();dirty=true;status.SetText("Texture applied to "+it->name+". Movement settings kept.");
   },
   [this]{window.SetEnabled(true);LayerRows();SelectLayer();Visibility();update.SetEnabled(!draft.id.empty());keep.SetEnabled(target==wi::ecs::INVALID_ENTITY);}
  );
 }
 void Build(){
  Label(title,"Effect Presets",18,14,240,17);search.Create("Search effects");search.SetText("");search.SetPlaceholder("Search effects...");Add(search,18,46,174,28);
  Button(blank,"New",202,46,46);blank.SetTooltip("Start a blank effect.");
  blank.OnClick([this](auto&){bridge::ParticleEffect p;bridge::ParticleEffectLayer l;l.id=bridge::GenerateStableId();l.name="Emitter";l.emitter=bridge::MakeNewParticleEmitterState();l.material.blendMode=wi::enums::BLENDMODE_ALPHA;p.layers.push_back(l);Set(p);});search.OnInputAccepted([this](auto&){Rows();});
  list.Create("Particle presets");list.SetText("");Add(list,18,84,230,260);list.OnSelect([this](const wi::gui::EventArgs&a){if(a.userdata<presets.size())Set(presets[a.userdata]);});
  Label(layerTitle,"Effect Layers",18,360,230,17);layers.Create("Effect layers");layers.SetText("");Add(layers,18,392,230,220);
  layers.OnSelect([this](const wi::gui::EventArgs&a){Flush();if(a.userdata<draft.layers.size()){selected=a.userdata;SelectLayer();}});
  Button(add,"Add Layer",18,624,108);Button(copy,"Duplicate",136,624,112);Button(remove,"Remove Layer",18,664,135);Button(solo,"Solo",164,664,84);
  add.OnClick([this](auto&){Flush();if(draft.layers.size()>=32){status.SetText("Maximum 32 layers.");return;}bridge::ParticleEffectLayer l;l.id=bridge::GenerateStableId();l.name="Emitter "+std::to_string(draft.layers.size()+1);l.emitter=bridge::MakeNewParticleEmitterState();l.material.blendMode=wi::enums::BLENDMODE_ALPHA;draft.layers.push_back(l);selected=draft.layers.size()-1;LayerRows();SelectLayer();dirty=true;});
  copy.OnClick([this](auto&){Flush();if(selected>=draft.layers.size()||draft.layers.size()>=32)return;auto l=draft.layers[selected];l.id=bridge::GenerateStableId();l.name+=" 2";draft.layers.push_back(l);selected=draft.layers.size()-1;LayerRows();SelectLayer();dirty=true;});
  remove.OnClick([this](auto&){if(draft.layers.size()<2)return;draft.layers.erase(draft.layers.begin()+selected);selected=std::min(selected,draft.layers.size()-1);LayerRows();SelectLayer();dirty=true;});
  solo.OnClick([this](auto&){soloActive=!soloActive;solo.SetText(soloActive?"Show All":"Solo");dirty=true;});
  Label(previewHeading,"Live Preview",274,14,480,17);image.Create("Effect preview");image.SetText("");image.SetColor(wi::Color::White());Add(image,274,46,480,350);
  Check(animate,"Animate preview",true,274,412);Button(restart,"Restart",620,407,134);restart.OnClick([this](auto&){dirty=true;});
  Button(fit,"Fit / Reset",474,407,132);fit.OnClick([this](auto&){if(preview)preview->Fit(Read());});
  Label(cameraHint,"Drag orbit | RMB pan | wheel zoom | Shift-drag roll",274,438,480,11);
  Label(globalHeading,"Whole Effect",274,468,280,17);
  Button(timingButton,"Timing...",604,465,150);timingButton.OnClick([this](auto&){Flush();timingMode=true;TimingSet();Visibility();});
  Check(timingEnabled,"Use effect timing",false,782,121);
  timingLoop.Create("Repeat mode");timingLoop.SetText("");timingLoop.AddItem("Loop");timingLoop.AddItem("Play once");Add(timingLoop,782,164,340,28);timingLoop.OnSelect([this](auto&){if(!setting){Flush();dirty=true;}});
  Slider(timingValues[0],"Duration (sec)",.05f,60,5,938,218,165);timingValues[0].Configure(.05f,60,.05f);
  Slider(timingValues[1],"Delay (sec)",0,30,0,938,266,165);timingValues[1].Configure(0,30,.05f);
  Slider(timingValues[2],"Burst / layer",0,500,0,938,314,165);timingValues[2].Configure(0,500,1,true);
  Label(timingHelp,"Duration controls emission. Existing particles finish naturally. Loop repeats delay + duration. Burst adds particles at each start. Restart replays the whole effect.",782,376,340,12);timingHelp.SetSize({340,150});
  Slider(size,"Overall size %",5,500,100,414,496,220);Slider(amount,"Amount %",5,500,100,414,532,220);
  Slider(height,"Height %",5,300,100,414,568,220);Slider(spread,"Spread %",5,300,100,414,604,220);
  Label(help,"100% preserves the authored setup. Height keeps lifetime unchanged.",274,650,480,12);
  Label(settingsTitle,"Layer Settings",782,14,360,17);layerName.Create("Layer name");Add(layerName,782,46,340,28);layerName.OnInputAccepted([this](auto&){Flush();LayerRows();});
  Check(enabled,"Layer enabled",true,782,86);
  playback.Create("Playback mode");playback.SetText("");playback.AddItem("Continuous emitter");playback.AddItem("Stationary / looping sprite");Add(playback,782,116,340,28);
  playback.OnSelect([this](auto&){if(!setting){Flush();LayerRows();SelectLayer();dirty=true;}});
  Slider(red,"Red",0,1,1,866,157,190);Slider(green,"Green",0,1,1,866,188,190);Slider(blue,"Blue",0,1,1,866,219,190);
  Slider(opacity,"Opacity",0,1,1,866,250,190);Slider(brightness,"Emission",0,50,0,866,281,190);
  Slider(particleSize,"Layer size %",1,300,100,910,312,196);particleSize.Configure(1,300,1,true);particleSize.SetTooltip("Particle size of the selected layer: 100% is its loaded size, 50% halves it, 200% doubles it.");
  particleSize.OnSlide([this](const auto& a){ChangeLayerPercent(a.fValue);});particleSize.valueInputField.OnInputAccepted([this](const auto& a){ChangeLayerPercent(a.fValue);});
  Label(textureTitle,"Texture",782,343,360,12);textureSlot.Create("Texture slot");textureSlot.SetText("");for(auto text:{"Colour / sprite","Normal / distortion","Surface","Emissive","Occlusion"})textureSlot.AddItem(text);Add(textureSlot,782,368,340,26);
  Button(choose,"Browse textures / sheets...",782,401,340);choose.OnClick([this](auto&){Choose();});
  Slider(cols,"Columns",1,64,1,866,442,190);Slider(rows,"Rows",1,64,1,866,475,190);
  Slider(frames,"Frames",1,4096,1,866,508,190);Slider(fps,"FPS",0,120,24,866,541,190);Button(allFrames,"All",1074,505,48);allFrames.OnClick([this](auto&){frames.SetValue(cols.GetValue()*rows.GetValue());Flush();dirty=true;});
  Check(blendFrames,"Blend frames",true,782,571);
  blend.Create("Blend mode");blend.SetText("");for(auto name:{"Opaque","Alpha","Premultiplied","Additive","Multiply","Inverse"})blend.AddItem(name);Add(blend,782,609,340,28);blend.OnSelect([this](auto&){if(!setting){Flush();dirty=true;}});
  Button(advancedButton,"Advanced",782,657,64);advancedButton.OnClick([this](auto&){Flush();if(timingMode){timingMode=false;advanced=false;}else if(fieldsMode||lifetimeMode){fieldsMode=false;lifetimeMode=false;advanced=false;}else advanced=!advanced;DetailSet();Visibility();});
  shader.Create("Particle shading");shader.SetText("");for(auto text:{"Soft","Heat distortion","Simple (debug)","Soft lit"})shader.AddItem(text);Add(shader,968,84,154,28);shader.OnSelect([this](auto&){if(!setting){Flush();dirty=true;}});
  detailPage.Create("Advanced section");detailPage.SetText("");for(auto n:{"Emission / appearance","Motion","Position / fading","Simulation / flags","Variation / spawn","Emission shape"})detailPage.AddItem(n);Add(detailPage,782,127,340,28);detailPage.OnSelect([this](auto&){DetailSet();Visibility();});
  for(int j=0;j<12;++j){
   Slider(detail[j],"Value",-10000,10000,0,910,162+j*30,145);
   auto change=[this,j](float value){if(setting)return;detail[j].SetValue(detail[j].Quantize(value));int page=detailPage.GetSelected();if((page==0&&j==1)||(page==4&&j==10))ChangeLayerSize(detail[j].GetValue());else{Flush();dirty=true;}};
   detail[j].OnSlide([change](const auto& a){change(a.fValue);});detail[j].valueInputField.OnInputAccepted([change](const auto& a){change(a.fValue);});
  }
  const char* flagNames[]={"Paused","Sorted","Depth collision","SPH","Volume","Ignore colliders","Mesh colour","Blend frames"};
  for(int n=0;n<8;++n)Check(flags[n],flagNames[n],false,782+(n%2)*175.0f,532+(n/2)*29.0f);
  Button(fieldsButton,"Fields",920,657,64);fieldsButton.OnClick([this](auto&){Flush();timingMode=false;fieldsMode=true;lifetimeMode=false;advanced=false;FieldSet();Visibility();});
  fieldName.Create("Field name");Add(fieldName,782,46,340,28);fieldName.OnInputAccepted([this](auto&){FieldRead();bool old=setting;setting=true;FieldRows();setting=old;dirty=true;});
  Check(fieldEnabled,"Field enabled",true,782,86);fieldEnabled.OnClick([this](auto&){FieldRead();dirty=true;});
  fieldType.Create("Field type");fieldType.SetText("");for(auto n:{"Swirl","Turbulence","Updraft","Directional"})fieldType.AddItem(n);Add(fieldType,968,84,154,28);fieldType.OnSelect([this](auto&){if(!setting){FieldRead();FieldSet();dirty=true;}});
  fieldList.Create("Effect movement fields");fieldList.SetText("");Add(fieldList,782,120,220,28);fieldList.OnSelect([this](const auto& a){if(setting)return;FieldRead();selectedField=size_t(a.userdata);FieldSet();Visibility();dirty=true;});
  Button(addField,"Add",1014,120,50);addField.OnClick([this](auto&){Flush();if(draft.fields.size()>=4)return;bridge::ParticleMovementField f;f.id=bridge::GenerateStableId();f.name="Field "+std::to_string(draft.fields.size()+1);f.transform.scale={2,2,2};draft.fields.push_back(f);selectedField=draft.fields.size()-1;FieldSet();Visibility();dirty=true;});
  Button(removeField,"Remove",1072,120,50);removeField.OnClick([this](auto&){if(selectedField>=draft.fields.size())return;draft.fields.erase(draft.fields.begin()+selectedField);for(auto& layer:draft.layers){for(size_t i=selectedField;i<3;++i)layer.fieldInfluence[i]=layer.fieldInfluence[i+1];layer.fieldInfluence[3]=1;}selectedField=0;FieldSet();Visibility();dirty=true;});
  fieldPage.Create("Field controls");fieldPage.SetText("");for(auto n:{"Flow","Position / Size","Rotation"})fieldPage.AddItem(n);Add(fieldPage,782,160,340,28);fieldPage.OnSelect([this](auto&){if(setting)return;FieldRead();fieldPageIndex=fieldPage.GetSelected();FieldSet();Visibility();});
  for(int j=0;j<6;++j)Slider(fieldValues[j],"Field value",0,5,1,910,204+j*42,196);
  Slider(fieldWeight,"Layer influence %",0,100,100,936,486,170);fieldWeight.Configure(0,100,1,true);
  Check(clockwise,"Clockwise",false,782,534);clockwise.OnClick([this](auto&){FieldRead();dirty=true;});
  Check(showField,"Show field",true,958,534);showField.OnClick([this](auto&){if(preview)preview->refresh=true;});
  Label(fieldHelp,"Shared field; influence applies to selected layer.",782,580,340,11);
  Button(variationButton,"Variation",851,657,64);variationButton.OnClick([this](auto&){Flush();timingMode=false;fieldsMode=false;lifetimeMode=false;advanced=true;detailPage.SetSelected(4);DetailSet();Visibility();});
  Button(lifetimeButton,"Lifetime",989,657,64);lifetimeButton.OnClick([this](auto&){Flush();timingMode=false;lifetimeMode=true;fieldsMode=false;advanced=false;CurveSet();Visibility();});
  Button(shapeButton,"Shapes",1058,657,64);shapeButton.OnClick([this](auto&){Flush();timingMode=fieldsMode=lifetimeMode=false;advanced=true;detailPage.SetSelected(5);DetailSet();Visibility();});
  shapeType.Create("Emission shape");shapeType.SetText("");for(auto text:{"Point","Box","Sphere","Cone","Disc","Ring","Cylinder"})shapeType.AddItem(text);Add(shapeType,782,160,340,28);
  shapeType.OnSelect([this](auto&){if(setting||selected>=draft.layers.size())return;auto& l=draft.layers[selected];l.shape.type=bridge::ParticleShapeType(shapeType.GetSelected());l.emitter.volume=l.shape.type==bridge::ParticleShapeType::Box;DetailSet();Visibility();dirty=true;});
  Check(shapeSurface,"Sphere surface only",false,782,570);shapeSurface.SetTooltip("Shapes use local +Z as their forward axis. Pitch, yaw and roll aim the emission.");
  lifetimeButton.SetTooltip("Change colour, size and opacity over each particle's lifetime.");
  Check(curveEnabled,"Use lifetime curves",false,782,121);curveEnabled.OnClick([this](auto&){if(!setting){CurveRead();CurveSet();dirty=true;}});
  curveGraph.Create("Lifetime profile");curveGraph.SetText("");Add(curveGraph,782,152,340,128);
  curveGraph.SetTooltip("Click a point's position to select it. Edit age, size, opacity and colour below.");
  curveGraph.OnClick([this](auto&){if(setting||selected>=draft.layers.size())return;CurveRead();const auto& keys=draft.layers[selected].lifetime.keys;float time=std::clamp((wi::input::GetPointer().x-curveGraph.GetPos().x-76)/(curveGraph.GetSize().x-86),0.0f,1.0f);selectedKey=0;for(size_t n=1;n<keys.size();++n)if(std::abs(keys[n].time-time)<std::abs(keys[selectedKey].time-time))selectedKey=n;CurveSet();});
  keyList.Create("Lifetime points");keyList.SetText("");Add(keyList,782,291,192,28);
  keyList.OnSelect([this](const auto& a){if(setting)return;CurveRead();selectedKey=size_t(a.userdata);CurveSet();});
  Button(addKey,"Add",982,291,60);Button(removeKey,"Remove",1050,291,72);
  addKey.OnClick([this](auto&){Flush();auto& c=draft.layers[selected].lifetime;if(c.keys.size()>=8)return;size_t widest=0;for(size_t n=1;n+1<c.keys.size();++n)if(c.keys[n+1].time-c.keys[n].time>c.keys[widest+1].time-c.keys[widest].time)widest=n;auto sample=c;sample.enabled=true;auto key=bridge::SampleParticleLifetime(sample,(c.keys[widest].time+c.keys[widest+1].time)*.5f);c.keys.insert(c.keys.begin()+widest+1,key);selectedKey=widest+1;CurveSet();dirty=true;});
  removeKey.OnClick([this](auto&){Flush();auto& c=draft.layers[selected].lifetime;if(selectedKey==0||selectedKey+1>=c.keys.size())return;c.keys.erase(c.keys.begin()+selectedKey);selectedKey=std::min(selectedKey,c.keys.size()-1);CurveSet();dirty=true;});
  const char* curveLabels[]={"Age %","Size %","Opacity %","Red %","Green %","Blue %"};
  for(int n=0;n<6;++n){Slider(curveValues[n],curveLabels[n],0,n==1?400:100,n==0?0:100,918,338+n*36,176);curveValues[n].Configure(0,n==1?400:100,n==0?.1f:1,n!=0,n==0?1:0);}
  Label(curveHelp,"",782,560,340,11);curveHelp.SetSize({340,68});
  Button(resetCurve,"Reset curve",782,615,340);resetCurve.OnClick([this](auto&){Flush();auto& c=draft.layers[selected].lifetime;bool enabled=c.enabled;c={};c.enabled=enabled;selectedKey=0;CurveSet();dirty=true;});
  flags[4].SetName("Spawn box");flags[4].SetTooltip("Spawn throughout the layer box. Half width, height and depth are in Variation / spawn.");
  name.Create("Effect preset name");Add(name,274,702,480,28);
  Button(update,"Update Preset",782,702,160);Button(saveNew,"Save As New",956,702,166);update.OnClick([this](auto&){Save(false);});saveNew.OnClick([this](auto&){Save(true);});
  Check(keep,"Keep placing",true,18,754);Label(finishHint,"RMB to finish",176,752,170,12);
  Button(cancel,"Cancel",842,750,120);Button(apply,"Place Effect",976,750,146);apply.primary=true;apply.SetColor(Teal);
  cancel.OnClick([this](auto&){window.SetVisible(false);});apply.OnClick([this](auto&){Flush();request=true;});
  size.Configure(10,300,5,true);amount.Configure(0,200,5,true);height.Configure(25,200,5,true);spread.Configure(0,200,5,true);
  red.Configure(0,1,.01f);green.Configure(0,1,.01f);blue.Configure(0,1,.01f);opacity.Configure(0,1,.01f);brightness.Configure(0,5,.05f);
  cols.Configure(1,32,1,true);rows.Configure(1,32,1,true);frames.Configure(1,256,1,true);fps.Configure(0,60,1,true);fps.SetTooltip("Continuous emitters: 0 advances from Frame start over the particle lifetime. Stationary sprites: 0 pauses animation.");
  Label(status,"Choose a saved effect or configure emitter layers.",18,798,1104,12);Visibility();
 }
 wi::gui::Label previewHeading,globalHeading,help,finishHint,cameraHint;PickerButton restart,fit,blank;
};
ParticlePickerWindow::ParticlePickerWindow():impl_(std::make_unique<Impl>(*this)){}
ParticlePickerWindow::~ParticlePickerWindow(){RemoveWidgets();}
void ParticlePickerWindow::Create(){
 wi::gui::Window::Create("Particle Picker",WindowControls::CLOSE|WindowControls::MOVE);SetControlSize(28);SetSize({1150,850});
 SetColor(wi::Color(33,35,37,255),wi::gui::WIDGET_ID_WINDOW_BASE);
 SetColor(wi::Color(33,35,37,255),wi::gui::WIDGET_ID_COMBO_DROPDOWN);impl_->textureBrowser.Create();impl_->Build();SetVisible(false);
}
void ParticlePickerWindow::SetVisible(bool visible){if(!visible&&impl_&&impl_->textureBrowser.IsVisible())impl_->textureBrowser.Close();wi::gui::Window::SetVisible(visible);}
wi::gui::Widget& ParticlePickerWindow::TextureBrowserWidget(){return impl_->textureBrowser;}
void ParticlePickerWindow::Open(bridge::StudioSession&s,wi::ecs::Entity target){
 SetVisible(true);
 wi::gui::Theme theme;theme.image.background=false;theme.image.color=wi::Color(40,42,44,255);theme.font.color=wi::Color(235,238,239,255);theme.shadow=0;
 theme.tooltipImage.color=Grey;theme.tooltipFont.color=wi::Color(235,238,239,255);SetTheme(theme);
 SetColor(wi::Color(43,46,48,255),wi::gui::IDLE);SetColor(wi::Color(65,78,77,255),wi::gui::FOCUS);SetColor(wi::Color(60,108,101,255),wi::gui::ACTIVE);SetColor(wi::Color(33,35,37,255),wi::gui::WIDGET_ID_WINDOW_BASE);
 SetColor(wi::Color(33,35,37,255),wi::gui::WIDGET_ID_COMBO_DROPDOWN);
 auto&i=*impl_;i.apply.SetColor(Teal);i.apply.SetEnabled(true);i.image.SetColor(wi::Color::White());for(auto& sp:i.image.sprites)sp.params.disableBackground();
 for(auto* label:i.captions)for(auto& sp:label->sprites)sp.params.color=wi::Color::Transparent();
 for(auto* heading:{&i.title,&i.previewHeading,&i.layerTitle,&i.globalHeading,&i.settingsTitle}){heading->font.params.size=16;heading->font.params.bolden=.2f;}
i.session=&s;i.project=s.Projects().CurrentProject().projectId;i.revision=s.Scenes().Revision();i.target=target;i.request=false;i.advanced=false;i.fieldsMode=false;i.lifetimeMode=false;i.timingMode=false;i.selectedKey=0;i.selectedField=0;i.fieldPageIndex=0;i.fieldPage.SetSelected(0);i.soloActive=false;
 auto p=bridge::CaptureParticleEffect(s.Scenes().GetScene(),target);
 if(p.layers.empty()){bridge::ParticleEffectLayer l;l.id=bridge::GenerateStableId();l.name="Emitter";l.emitter=bridge::MakeNewParticleEmitterState();l.material.blendMode=wi::enums::BLENDMODE_ALPHA;p.layers.push_back(l);}
 i.textureProfiles.clear();std::vector<bridge::ParticleTextureEntry> profiles;std::string profileError;
 if(bridge::ReadProjectParticleTextures(s.Projects().CurrentProject().rootPath,i.project,profiles,profileError))for(auto& e:profiles)i.textureProfiles.emplace(e.assetId,e);
 i.search.SetText("");i.Set(p);std::string error;i.presets=bridge::ListParticleEffects(s.Projects().CurrentProject().rootPath,i.project,error);if(target==wi::ecs::INVALID_ENTITY&&!i.presets.empty())i.Set(i.presets.front());i.Rows();i.Visibility();
 i.apply.SetText(target==wi::ecs::INVALID_ENTITY?"Place Effect":"Apply to Effect");i.keep.SetEnabled(target==wi::ecs::INVALID_ENTITY);
 i.status.SetText("Saved setups include every layer, texture and spritesheet setting.");
}
bridge::ParticleEffect ParticlePickerWindow::Draft()const{return impl_->Read();}
wi::ecs::Entity ParticlePickerWindow::Target()const{return impl_->target;}
bool ParticlePickerWindow::KeepPlacing()const{return impl_->keep.GetCheck();}
bool ParticlePickerWindow::TakeApplyRequest(){return std::exchange(impl_->request,false);}
bool ParticlePickerWindow::MatchesSession(const bridge::StudioSession&s)const{return s.Projects().HasProject()&&s.Projects().CurrentProject().projectId==impl_->project&&s.Scenes().Revision()==impl_->revision;}
void ParticlePickerWindow::ShowError(const std::string&e){impl_->status.SetText(e);}
void ParticlePickerWindow::UpdatePreview(float dt){
 auto&i=*impl_;if(!IsVisible()||i.textureBrowser.IsVisible())return;
 if(!i.preview||i.preview->root!=i.session->Projects().CurrentProject().rootPath){i.preview=std::make_unique<ParticlePreview>(i.session->Projects().CurrentProject().rootPath,i.project);i.dirty=true;}
 if(i.dirty){auto p=i.Read();if(i.soloActive)for(size_t n=0;n<p.layers.size();++n)p.layers[n].enabled=n==i.selected;std::string error;if(!i.preview->Setup(p,error))i.status.SetText(error);i.dirty=false;i.pendingLayerResizes.clear();}
 else if(!i.pendingLayerResizes.empty()){
  auto p=i.Read();if(i.soloActive)for(size_t n=0;n<p.layers.size();++n)p.layers[n].enabled=n==i.selected;
  for(auto index:i.pendingLayerResizes)if(index>=p.layers.size()||!i.preview->ResizeLayer(index,p.layers[index],p.fields))i.dirty=true;
  i.pendingLayerResizes.clear();
 }
 i.preview->showFields=i.fieldsMode&&i.showField.GetCheck();i.preview->chosenField=i.selectedField;
 i.preview->Navigate(wi::input::GetPointer(),i.image.GetPos(),i.image.GetSize());i.preview->Tick(dt,i.animate.GetCheck());wi::Resource image;image.SetTexture(i.preview->GetRenderResult3D());i.image.SetImage(image);
}
void ParticlePickerWindow::PreRenderPreview(){if(IsVisible()&&impl_->preview&&impl_->preview->pending)impl_->preview->PreRender();}
void ParticlePickerWindow::RenderPreview()const{if(IsVisible()&&impl_->preview)impl_->preview->Render();}
}
