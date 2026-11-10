#include "renegade/bridge/ParticleEffectService.h"
#include "renegade/bridge/ParticleEffectMarkerService.h"
#include "renegade/bridge/SelectionService.h"
#include <filesystem>
#include <iostream>
#include <limits>
using namespace renegade::bridge;
int failures=0;
void Check(bool b,const char* label){if(!b){std::cerr<<"FAIL "<<label<<'\n';++failures;}}
int main(){
 namespace fs=std::filesystem;
 const auto root=fs::temp_directory_path()/("particle-effects-"+GenerateStableId());fs::create_directories(root);
 auto project=GenerateStableId();std::string error;
 ParticleEffect p;p.name="Brazier Fire";
 for(auto name:{"Flame","Smoke","Embers"}){
  ParticleEffectLayer l;l.id=GenerateStableId();l.name=name;l.emitter=MakeNewParticleEmitterState();
  l.emitter.emitCount=51;l.emitter.size=.7f;l.emitter.life=1.248f;l.emitter.velocity.y=.75f;
  l.emitter.framesX=l.emitter.framesY=8;l.emitter.frameCount=64;l.emitter.frameRate=24;
  l.material.blendMode=wi::enums::BLENDMODE_ADDITIVE;p.layers.push_back(l);
 }
 Check(SaveParticleEffect(root.string(),project,p,true,error),"save layered preset");
 auto cold=ListParticleEffects(root.string(),project,error);Check(cold.size()==1,"cold read");
 if(cold.size()==1){Check(cold[0].layers.size()==3,"all layers");Check(!HasParticleEmitterStateChange(cold[0].layers[0].emitter,p.layers[0].emitter),"exact emitter state");Check(!HasMaterialStateChange(cold[0].layers[0].material,p.layers[0].material),"exact material state");}
 Check(ListParticleEffects(root.string(),GenerateStableId(),error).empty(),"cross-project rejection");
 p.layers[1].enabled=false;Check(SaveParticleEffect(root.string(),project,p,false,error),"explicit update");
 wi::scene::Scene scene;ParticleEffectCommand create(scene,root.string(),project,p,wi::ecs::INVALID_ENTITY,{2,3,4});
 Check(create.Execute(),"create grouped effect");auto entity=create.Entity();auto rootid=PersistentEntityId(scene,entity);
 Check(scene.emitters.GetCount()==3,"native emitters");Check(ParticleEffectRoot(scene,scene.emitters.GetEntity(0))==entity,"parent effect ownership");
 auto markers=ParticleEffectMarkerEntities(scene);Check(markers.size()==1&&markers[0]==entity,"one root marker for three layers");
 auto legacy=scene.Entity_CreateTransform("Legacy emitter");scene.emitters.Create(legacy);
 Check(ParticleEffectMarkerEntities(scene).size()==2,"legacy standalone retains its marker");scene.Entity_Remove(legacy);
 SelectionService selection;selection.BindScene(scene);
 for(size_t i=0;i<scene.emitters.GetCount();++i){selection.Select(scene.emitters.GetEntity(i));Check(selection.SelectedEntity()==entity,"all layer picks select one effect root");}
 auto unrelated=scene.Entity_CreateTransform("Unrelated object");selection.Select(unrelated);Check(selection.SelectedEntity()==unrelated,"ordinary selection unchanged");
 selection.Clear();Check(!selection.HasSelection(),"clear effect selection");
 selection.Set({scene.emitters.GetEntity(0),unrelated,scene.emitters.GetEntity(1)});
 Check(selection.Entities()==std::vector<wi::ecs::Entity>({entity,unrelated}),"multi-selection normalizes and deduplicates effect layers");
 selection.Toggle(scene.emitters.GetEntity(2));
 Check(selection.Entities()==std::vector<wi::ecs::Entity>({unrelated}),"Shift-toggle on another layer removes the whole effect only");
 selection.Toggle(scene.emitters.GetEntity(0));
 Check(selection.Entities()==std::vector<wi::ecs::Entity>({unrelated,entity})&&selection.Contains(scene.emitters.GetEntity(1)),"Shift-toggle adds the complete effect alongside an ordinary object");
 auto selectionRevision=selection.Revision();selection.Set(selection.Entities());
 Check(selection.Revision()==selectionRevision,"normalized selection does not cause idle invalidation");
 selection.Clear();

 auto captured=CaptureParticleEffect(scene,entity);Check(captured.layers.size()==3,"capture editable layers");
 if(captured.layers.size()==3){Check(!captured.layers[1].enabled&&captured.layers[1].emitter.emitCount==51,"disabled layer retains authored values");
  captured.layers[1].enabled=true;captured.layers[2].emitter.life=2.5f;
  auto layerid=PersistentEntityId(scene,scene.emitters.GetEntity(0));
  ParticleEffectCommand edit(scene,root.string(),project,captured,entity);
  Check(edit.Execute(),"edit effect");Check(PersistentEntityId(scene,scene.emitters.GetEntity(0))==layerid,"edit retains layer identity");
  edit.Undo();Check(CaptureParticleEffect(scene,entity).layers[2].emitter.life==1.248f,"one undo restores layers");Check(edit.Execute(),"edit redo");
 }
 DuplicateEntityCommand copy(scene,entity);Check(copy.Execute(),"duplicate full effect");Check(scene.emitters.GetCount()==6,"duplicate has all emitters");Check(ParticleEffectMarkerEntities(scene).size()==2,"duplicate receives exactly one separate marker");
 for(size_t i=0;i<scene.emitters.GetCount();++i){auto child=scene.emitters.GetEntity(i);selection.Select(child);Check(selection.SelectedEntity()==ParticleEffectRoot(scene,child),"duplicate children resolve their own effect root");}
 copy.Undo();Check(scene.emitters.GetCount()==3,"undo duplicate");Check(copy.Execute(),"redo duplicate");copy.Undo();
 {wi::Archive a((root/"Effect.wiscene").string(),false);scene.Serialize(a);}
 wi::scene::Scene reopened;{wi::Archive a((root/"Effect.wiscene").string(),true);reopened.Serialize(a);}
 Check(reopened.emitters.GetCount()==3,"native cold scene reload");
 Check(ParticleEffectMarkerEntities(reopened).size()==1,"cold reload retains one effect marker");
 SelectionService reopenedSelection;reopenedSelection.BindScene(reopened);
 auto reopenedRoot=ParticleEffectRoot(reopened,reopened.emitters.GetEntity(0));
 for(size_t i=0;i<reopened.emitters.GetCount();++i){reopenedSelection.Select(reopened.emitters.GetEntity(i));Check(reopenedSelection.SelectedEntity()==reopenedRoot,"cold reopened layers remain one selectable object");}
 create.Undo();Check(scene.emitters.GetCount()==0,"one undo removes whole group");Check(create.Execute(),"create redo");
 Check(PersistentEntityId(scene,entity)==rootid,"redo stable identity");
 auto bad=p;bad.layers[1].id=bad.layers[0].id;Check(!ValidateParticleEffect(bad,error),"reject repeated layer identity");
 bad=p;bad.layers[0].emitter.life=std::numeric_limits<float>::quiet_NaN();Check(!ValidateParticleEffect(bad,error),"reject NaN");
 const auto scaled=ScaleParticleLayer(p.layers[0],2,1,2,1);
 Check(scaled.emitter.life==p.layers[0].emitter.life&&scaled.emitter.emitCount==102,"height preserves lifetime and increases overlap");
 Check(scaled.emitter.velocity.y==3&&scaled.emitter.size==1.4f,"scale and height coordinate");

 auto loop=CaptureParticleEffect(scene,entity);loop.layers[0].playback=ParticlePlayback::StationaryLoop;loop.layers[0].emitter.framesX=loop.layers[0].emitter.framesY=6;loop.layers[0].emitter.frameCount=36;loop.layers[0].emitter.frameRate=24;
 ParticleEffectCommand anchor(scene,root.string(),project,loop,entity);Check(anchor.Execute(),"stationary animated layer");
 auto spriteEntity=scene.sprites.GetEntity(0);Check(scene.sprites.GetCount()==1,"one native scene sprite");
 Check(scene.emitters.GetComponent(spriteEntity)->count==0&&scene.emitters.GetComponent(spriteEntity)->IsPaused(),"no stationary particle stream");
 Check(ParticleEffectMarkerEntities(scene).size()==1,"stationary animation retains root marker");
 Check(scene.sprites[0].IsCameraFacing()&&scene.sprites[0].IsDisableUpdate(),"stationary sprite uses controlled billboard animation");
 for(int n=0;n<120;++n)UpdateParticleEffectSprites(scene,1.0f/60);
 Check(scene.sprites[0].anim.drawRectAnim._currentFrame>=10&&scene.sprites[0].anim.drawRectAnim._currentFrame<=12,"stationary sheet loops past its 36 frames");
 auto frame=scene.sprites[0].anim.drawRectAnim._currentFrame;UpdateParticleEffectSprites(scene,0);Check(scene.sprites[0].anim.drawRectAnim._currentFrame==frame,"stationary pause preserves frame");
 auto recaptured=CaptureParticleEffect(scene,entity);Check(recaptured.layers[0].playback==ParticlePlayback::StationaryLoop&&recaptured.layers[0].emitter.emitCount==51,"stationary edit preserves original emitter settings");
 Check(SaveParticleEffect(root.string(),project,recaptured,true,error),"save stationary preset");
 auto saved=ListParticleEffects(root.string(),project,error);bool hasStationary=false;for(auto& effect:saved)for(auto& layer:effect.layers)hasStationary|=layer.playback==ParticlePlayback::StationaryLoop;Check(hasStationary,"stationary mode cold read");
 anchor.Undo();Check(scene.sprites.GetCount()==0,"one undo restores continuous layer");Check(anchor.Execute()&&scene.sprites.GetCount()==1,"stationary redo");

 // Shared fields survive preset migration, native archives and composite commands.
 auto moving=CaptureParticleEffect(scene,entity);for(auto& l:moving.layers){l.playback=ParticlePlayback::Continuous;l.enabled=true;}
 ParticleMovementField field;field.id=GenerateStableId();field.name="Leaf swirl";field.transform.translation={0,1,0};field.transform.scale={2,3,2};field.strength=2;field.lift=.5f;field.falloff=.25f;
 moving.layers[0].sizeVariation=.65f;moving.layers[0].speedVariation=.3f;
 moving.fields={field};moving.layers[0].fieldInfluence[0]=0;moving.layers[1].fieldInfluence[0]=.35f;
 Check(SaveParticleEffect(root.string(),project,moving,true,error),"save field preset");
 auto fieldPresets=ListParticleEffects(root.string(),project,error);bool exactField=false;
 for(auto& preset:fieldPresets)if(preset.id==moving.id)exactField=preset.fields.size()==1&&preset.fields[0].name=="Leaf swirl"&&preset.fields[0].transform.scale.y==3&&preset.layers[0].sizeVariation==.65f&&preset.layers[0].speedVariation==.3f&&preset.layers[0].fieldInfluence[0]==0&&preset.layers[1].fieldInfluence[0]==.35f;
 Check(exactField,"cold shared field and independent layer weights");
 ParticleEffectCommand fieldEdit(scene,root.string(),project,moving,entity);Check(fieldEdit.Execute(),"apply fields through composite command");HydrateParticleMovementFields(scene);
 size_t activeFields=0;for(size_t i=0;i<scene.emitters.GetCount();++i)activeFields+=scene.emitters[i].movementFields.size();
 Check(activeFields==2,"zero influence excludes layer from field runtime");Check(scene.emitters[0].movementSizeVariation==.65f&&scene.emitters[0].movementSpeedVariation==.3f,"independent variation reaches native emitter");
 auto withFields=CaptureParticleEffect(scene,entity);Check(withFields.fields.size()==1,"capture placed field for editing");
 {wi::Archive a((root/"Fields.wiscene").string(),false);scene.Serialize(a);}
 wi::scene::Scene fieldReload;{wi::Archive a((root/"Fields.wiscene").string(),true);fieldReload.Serialize(a);}HydrateParticleMovementFields(fieldReload);
 size_t coldFields=0;for(size_t i=0;i<fieldReload.emitters.GetCount();++i)coldFields+=fieldReload.emitters[i].movementFields.size();Check(coldFields==2,"native cold reload rehydrates layer fields");
 DuplicateEntityCommand fieldCopy(scene,entity);Check(fieldCopy.Execute(),"duplicate field-owned effect");HydrateParticleMovementFields(scene);activeFields=0;for(size_t i=0;i<scene.emitters.GetCount();++i)activeFields+=scene.emitters[i].movementFields.size();Check(activeFields==4,"duplicate retains all field weights");fieldCopy.Undo();
 fieldEdit.Undo();Check(CaptureParticleEffect(scene,entity).fields.empty(),"undo restores effect without fields");Check(fieldEdit.Execute(),"redo field edit");
 auto v=SampleParticleMovementField(field,{1,1,0},0);Check(v.z>1.9f&&v.y==.5f,"swirl tangent and upward lift");field.clockwise=true;Check(SampleParticleMovementField(field,{1,1,0},0).z<0,"clockwise reverses swirl");
 v=SampleParticleMovementField(field,{4,1,0},0);Check(v.x==0&&v.y==0&&v.z==0,"field boundary excludes external particles");
 field.clockwise=false;field.type=ParticleFieldType::Turbulence;auto noiseA=SampleParticleMovementField(field,{.3f,1.2f,.4f},0),noiseB=SampleParticleMovementField(field,{.3f,1.2f,.4f},2);
 Check(std::abs(noiseA.x-noiseB.x)+std::abs(noiseA.y-noiseB.y)+std::abs(noiseA.z-noiseB.z)>.01f,"turbulence changes during particle life");
 field.type=ParticleFieldType::Directional;field.transform.rotation={0,.70710678f,0,.70710678f};v=SampleParticleMovementField(field,{0,1,0},0);Check(v.x>1.9f&&std::abs(v.z)<.01f,"rotated directional field follows its orientation");
 auto badField=moving;badField.fields[0].strength=std::numeric_limits<float>::quiet_NaN();Check(!ValidateParticleEffect(badField,error),"reject nonfinite field");
 badField=moving;badField.layers[0].fieldInfluence[0]=2;Check(!ValidateParticleEffect(badField,error),"reject invalid layer influence");
 fs::remove_all(root);std::cout<<"ParticleEffectTests failures="<<failures<<'\n';return failures?1:0;
}
