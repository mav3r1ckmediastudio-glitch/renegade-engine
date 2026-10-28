#include "RiverWaterwayFixture.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/RiverFlowSpanService.h"
#include <iostream>
#include <cstring>
#include <limits>
#include "../WickedEngine/Editor/json.hpp"
using namespace waterway_proof;
static int checks=0;
static void Test(bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);}
static std::string Metadata(const Fixture& f){auto* m=f.scene.metadatas.GetComponent(f.tributary);return m&&m->string_values.has(RiverConnectionMetadataKey)?m->string_values.get(RiverConnectionMetadataKey):"";}
int main(){try{
 {Fixture f;std::string error;RiverWaterwayControls a,b;
  Test(ReadRiverWaterwayControls(f.scene,f.tributary,a,error)&&!a.connected&&a.owner==f.tributary,"Independent river owns its controls");
  SetRiverConnectionCommand connect(f.scene,f.tributary,f.receiver,f.node);Test(connect.Execute(),connect.Error());
  Test(ReadRiverWaterwayControls(f.scene,f.tributary,a,error)&&ReadRiverWaterwayControls(f.scene,f.receiver,b,error),"Both members resolve shared controls");
  Test(a.connected&&b.connected&&a.owner==f.receiver&&b.owner==f.receiver&&a.receivingNode==f.node&&b.receivingNode==f.node,"Shared settings target receiver and exact saved node");
  // This query must not need a mesh or regenerate native geometry.
  f.scene.meshes.Remove(f.receiver);
  Test(ReadRiverWaterwayControls(f.scene,f.tributary,b,error)&&b.owner==f.receiver,"Control lookup independent of mesh geometry");
  auto* metadata=f.scene.metadatas.GetComponent(f.tributary);auto text=metadata->string_values.get(RiverConnectionMetadataKey);
  metadata->string_values.set(RiverConnectionMetadataKey,"{}");
  Test(!ReadRiverWaterwayControls(f.scene,f.tributary,b,error)&&b.owner==f.receiver&&b.connected,"Invalid controls preserve previous output");
  metadata->string_values.set(RiverConnectionMetadataKey,text);
  RiverFlowProfile p;p.enabled=true;p.intensity=1.7f;
  SetRiverFlowProfileCommand settings(f.scene,a.owner,p);Test(settings.Execute(),"Shared receiver profile edit");
  RiverFlowProfile source,receiver;Test(ReadRiverFlowProfile(f.scene,f.tributary,source,error)&&ReadRiverFlowProfile(f.scene,f.receiver,receiver,error)&&receiver.intensity==1.7f&&source.intensity!=1.7f,"Shared edit changes receiver only");
  settings.Undo();Test(ReadRiverFlowProfile(f.scene,f.receiver,receiver,error)&&receiver.intensity==1,"Shared profile Undo");
 }
 {Fixture f;SetRiverConnectionCommand link(f.scene,f.tributary,f.receiver,f.node);Test(link.Execute(),link.Error());

  auto* legacyMetadata=f.scene.metadatas.GetComponent(f.receiver);
  auto legacy=nlohmann::json::parse(legacyMetadata->string_values.get(RiverFlowProfileMetadataKey));
  legacy["version"]=1;legacy.erase("rapids");legacy.erase("rapidIntensity");
  legacyMetadata->string_values.set(RiverFlowProfileMetadataKey,legacy.dump());
  std::string error;RiverFlowProfile original;Test(ReadRiverFlowProfile(f.scene,f.receiver,original,error),error);
  Test(!original.rapids&&original.rapidIntensity==1,"Version1 preserves accepted rendering by default");

  const auto* metadata=f.scene.metadatas.GetComponent(f.receiver);const bool had=metadata->string_values.has(RiverFlowProfileMetadataKey);
  const auto before=had?metadata->string_values.get(RiverFlowProfileMetadataKey):"";
  for(int k=0;k<3;++k){auto changed=original;if(k==0)changed.speed=0;if(k==1)changed.reversed=!changed.reversed;if(k==2)changed.turbulence=.5f;
   SetRiverFlowProfileCommand unsafe(f.scene,f.receiver,changed);Test(!unsafe.Execute(),"Connected structural/unsupported profile edit refused");
   metadata=f.scene.metadatas.GetComponent(f.receiver);
   Test(metadata->string_values.has(RiverFlowProfileMetadataKey)==had&&(!had||metadata->string_values.get(RiverFlowProfileMetadataKey)==before),"Refused settings preserve exact metadata");
  }
  for(auto entity:{f.receiver,f.tributary}){
   RiverFlowProfile initial;Test(ReadRiverFlowProfile(f.scene,entity,initial,error),error);
   RiverWaterway beforeGraph;Test(ReadRiverWaterway(f.scene,beforeGraph,error),error);
   auto changed=initial;changed.speed=2;SetRiverFlowProfileCommand speed(f.scene,entity,changed);
   Test(speed.Execute(),"Connected inlet speed edit");
   RiverWaterway afterGraph;Test(ReadRiverWaterway(f.scene,afterGraph,error)&&afterGraph.network->DeterministicHash()!=beforeGraph.network->DeterministicHash(),"Speed updates actual native flow field");
   speed.Undo();Test(ReadRiverWaterway(f.scene,afterGraph,error)&&afterGraph.network->DeterministicHash()==beforeGraph.network->DeterministicHash(),"Speed Undo restores graph exactly");
   Test(speed.Execute(),"Speed Redo");speed.Undo();
  }
  auto contact=original;contact.sourceWidth=.06f;SetRiverFlowProfileCommand emission(f.scene,f.receiver,contact);
  Test(emission.Execute(),"Connected contact width edit");emission.Undo();
  Test(f.scene.metadatas.GetComponent(f.receiver)->string_values.get(RiverFlowProfileMetadataKey)==before,"Contact Undo restores exact profile");
  auto rapid=original;rapid.rapids=true;rapid.rapidIntensity=1.3f;
  SetRiverFlowProfileCommand enableRapids(f.scene,f.receiver,rapid);Test(enableRapids.Execute(),"Opt-in measured rapids command");
  RiverFlowProfile read;Test(ReadRiverFlowProfile(f.scene,f.receiver,read,error)&&read.rapids&&read.rapidIntensity==1.3f,"Version2 rapid controls roundtrip");
  enableRapids.Undo();Test(f.scene.metadatas.GetComponent(f.receiver)->string_values.get(RiverFlowProfileMetadataKey)==before,"Rapid Undo preserves exact legacy metadata");
  Test(enableRapids.Execute(),"Rapid Redo");enableRapids.Undo();
  rapid.rapidIntensity=std::numeric_limits<float>::quiet_NaN();SetRiverFlowProfileCommand invalidRapid(f.scene,f.receiver,rapid);Test(!invalidRapid.Execute(),"NaN rapid amount refused atomically");
  const float width=f.scene.splines.GetComponent(f.receiver)->width;
  auto unsafeWidth=MakeSetRiverWidthCommand(f.scene,f.receiver,6);
  Test(!unsafeWidth->Execute()&&f.scene.splines.GetComponent(f.receiver)->width==width,"Connected width edit refused atomically");
  RiverWaterway graph;Test(ReadRiverWaterway(f.scene,graph,error)&&graph.network,"Refused settings leave connected graph valid");
 }
 for(bool reversed:{false,true})for(bool transformed:{false,true}){
  Fixture f(reversed,transformed);std::string error;XMFLOAT3 a;Test(RiverConnectionAnchor(f.scene,f.receiver,f.node,a,error),error);
  RiverFlowProfile originalProfile;Test(ReadRiverFlowProfile(f.scene,f.receiver,originalProfile,error),error);
  originalProfile.enabled=false;SetRiverFlowProfileCommand disable(f.scene,f.receiver,originalProfile);Test(disable.Execute(),"Default receiving flow disabled");
  SetRiverWaterMaterialCommand stock(f.scene,f.receiver,RiverWaterMaterialState{});
  Test(stock.Execute(),"Stock receiving WATER optics for no-op copy regression");
  const auto before=f.scene.splines.GetCount();
  // Reuse actual fixture controls in downstream flow order. A fixed world
  // direction becomes an opposing inlet when the receiver parent rotates.
  std::vector<XMFLOAT3> points;
  for(auto node:f.scene.splines.GetComponent(f.tributary)->spline_node_entities){
   XMFLOAT3 p;XMStoreFloat3(&p,f.scene.ComputeEntityMatrixRecursive(node).r[3]);points.push_back(p);}
  if(reversed)std::reverse(points.begin(),points.end());points.back()=a;
  CreateRiverTributaryCommand draw(f.scene,points,f.receiver,f.node,3);
  Test(draw.Execute(),draw.Error());const auto source=draw.CreatedEntity();const auto id=PersistentEntityId(f.scene,source);
  RiverWaterway g;Test(ReadRiverWaterway(f.scene,g,error)&&g.network&&f.scene.splines.GetCount()==before+1,"Draw command creates one native connected tributary");
  Test(ValidatePersistentEntityIdentities(f.scene).IsValid(),"Drawn controls receive stable identities");auto hash=g.network->DeterministicHash();
  draw.Undo();Test(draw.Error().empty()&&f.scene.splines.GetCount()==before&&!f.scene.transforms.Contains(source),"Single Undo removes created river and controls");
  Test(ReadRiverWaterway(f.scene,g,error)&&!g.network,"Single Undo removes connection");
  RiverFlowProfile restored;Test(ReadRiverFlowProfile(f.scene,f.receiver,restored,error)&&!restored.enabled,"Single Undo restores receiving flow opt-in");
  Test(draw.Execute(),draw.Error());Test(PersistentEntityId(f.scene,source)==id&&ReadRiverWaterway(f.scene,g,error)&&g.network->DeterministicHash()==hash,"Draw Redo restores native identities and topology");
  draw.Undo();
  auto opposing=points;for(size_t k=0;k+1<opposing.size();++k){opposing[k].x=2*a.x-opposing[k].x;opposing[k].z=2*a.z-opposing[k].z;}
  CreateRiverTributaryCommand backwards(f.scene,opposing,f.receiver,f.node,3);
  Test(!backwards.Execute()&&f.scene.splines.GetCount()==before,"Opposing inlet refused without orphan spline");
  Test(ReadRiverFlowProfile(f.scene,f.receiver,restored,error)&&!restored.enabled,"Failed new tributary restores receiver metadata");
  points.back().x+=2;CreateRiverTributaryCommand stale(f.scene,points,f.receiver,f.node,3);
  Test(!stale.Execute()&&f.scene.splines.GetCount()==before,"Invalid drawing rolls back without orphan river");
 }
 for(bool reversed:{false,true})for(bool transformed:{false,true}){
  Fixture f(reversed,transformed);std::string error;auto before=Endpoint(f.scene,f.tributary);XMFLOAT3 target;
  Test(RiverConnectionAnchor(f.scene,f.receiver,f.node,target,error),error);
  auto receiverNodes=f.scene.splines.GetComponent(f.receiver)->spline_node_entities;
  std::vector<TransformState> original;for(auto e:receiverNodes)original.push_back(CaptureTransform(*f.scene.transforms.GetComponent(e)));
  SetRiverConnectionCommand connect(f.scene,f.tributary,f.receiver,f.node);
  Test(connect.Execute(),connect.Error());Test(Distance(Endpoint(f.scene,f.tributary),target)<1e-5,"Actual native anchor snap");
  RiverWaterway graph;Test(ReadRiverWaterway(f.scene,graph,error),error);Test(graph.network&&graph.reaches.size()==3&&graph.junctions.size()==1,"Interior attachment creates three logical reaches");
  const auto hash=graph.network->DeterministicHash();const auto text=Metadata(f);
  Test(text.find(PersistentEntityId(f.scene,f.receiver))!=std::string::npos&&text.find(PersistentEntityId(f.scene,f.node))!=std::string::npos,"Persist UUID receiver and control identity");
  Test(f.scene.splines.GetComponent(f.receiver)->spline_node_entities==receiverNodes,"Never physically split receiving spline");
  for(size_t k=0;k<original.size();++k){auto now=CaptureTransform(*f.scene.transforms.GetComponent(receiverNodes[k]));Test(std::memcmp(&now,&original[k],sizeof(now))==0,"Receiver authored transform untouched");}
  connect.Undo();Test(connect.Error().empty(),connect.Error());Test(Metadata(f).empty()&&Distance(Endpoint(f.scene,f.tributary),before)==0,"Undo restores endpoint and removes only connection");
  Test(connect.Execute(),connect.Error());RiverWaterway again;Test(ReadRiverWaterway(f.scene,again,error)&&again.network->DeterministicHash()==hash,"Redo preserves graph identity/hash");
  auto disconnect=SetRiverConnectionCommand::Disconnect(f.scene,f.tributary);Test(disconnect->Execute(),disconnect->Error());Test(ReadRiverWaterway(f.scene,again,error)&&!again.network,"Disconnect preserves geometry and removes logical graph");
  Test(Distance(Endpoint(f.scene,f.tributary),target)<1e-5,"Disconnect does not move spline");disconnect->Undo();Test(disconnect->Error().empty(),disconnect->Error());Test(Metadata(f)==text,"Disconnect Undo restores exact metadata");
  // Unrelated owner metadata is not captured or clobbered by Undo.
  f.scene.metadatas.GetComponent(f.receiver)->string_values.set("owner-note","keep me");connect.Undo();
  Test(f.scene.metadatas.GetComponent(f.receiver)->string_values.get("owner-note")=="keep me","Preserve unrelated owner metadata");
  Test(connect.Execute(),connect.Error());
  auto* t=f.scene.transforms.GetComponent(f.node);t->translation_local.z+=.4f;t->SetDirty();auto changed=t->translation_local;
  connect.Undo();Test(!connect.Error().empty()&&Metadata(f)==text&&t->translation_local.z==changed.z,"Stale Undo refuses without clobbering owner edit");
  auto preserved=graph.network;Test(!ReadRiverWaterway(f.scene,graph,error)&&graph.network==preserved,"Moved receiver invalidates join without replacing valid prior output");
 }
 for(bool reversed:{false,true})for(bool transformed:{false,true}){
  Fixture f(reversed,transformed);SetRiverConnectionCommand link(f.scene,f.tributary,f.receiver,f.node);Test(link.Execute(),link.Error());
  std::string error;RiverWaterway graph;Test(ReadRiverWaterway(f.scene,graph,error),error);auto originalHash=graph.network->DeterministicHash();auto originalEndpoint=Endpoint(f.scene,f.tributary);
  auto before=CaptureTransform(*f.scene.transforms.GetComponent(f.node));auto after=before;after.translation.x+=.7f;after.translation.z+=.3f;
  SetWatercourseTransformCommand move(f.scene,f.node,before,after);Test(move.Execute(),"Connected receiving node edit");
  XMFLOAT3 anchor;Test(RiverConnectionAnchor(f.scene,f.receiver,f.node,anchor,error),error);Test(Distance(anchor,Endpoint(f.scene,f.tributary))<.00003f,"Incoming endpoint follows receiving junction");
  Test(ReadRiverWaterway(f.scene,graph,error),error);auto movedHash=graph.network->DeterministicHash();Test(movedHash!=originalHash,"Edit changes native geometry hash");
  move.Undo();Test(ReadRiverWaterway(f.scene,graph,error)&&graph.network->DeterministicHash()==originalHash&&Distance(originalEndpoint,Endpoint(f.scene,f.tributary))==0,"Connected Undo restores all native poses");
  Test(move.Execute(),"Connected Redo");Test(ReadRiverWaterway(f.scene,graph,error)&&graph.network->DeterministicHash()==movedHash,"Connected Redo restores all poses deterministically");
  move.Undo();
  auto* spline=f.scene.splines.GetComponent(f.tributary);auto endpoint=reversed?spline->spline_node_entities.front():spline->spline_node_entities.back();
  before=CaptureTransform(*f.scene.transforms.GetComponent(endpoint));after=before;after.translation.x+=.35f;after.translation.z+=.2f;
  SetWatercourseTransformCommand drag(f.scene,endpoint,before,after);Test(drag.Execute(),"Connected tributary endpoint promotes shared receiver edit");
  Test(ReadRiverWaterway(f.scene,graph,error),error);Test(RiverConnectionAnchor(f.scene,f.receiver,f.node,anchor,error)&&Distance(anchor,Endpoint(f.scene,f.tributary))<.00003f,"Either connected endpoint remains attached");
  drag.Undo();Test(ReadRiverWaterway(f.scene,graph,error)&&graph.network->DeterministicHash()==originalHash,"Shared endpoint Undo");
  before=CaptureTransform(*f.scene.transforms.GetComponent(f.node));after=before;after.translation.y+=1;
  SetWatercourseTransformCommand unsafe(f.scene,f.node,before,after);Test(!unsafe.Execute(),"Nonflat connected edit refused");Test(ReadRiverWaterway(f.scene,graph,error)&&graph.network->DeterministicHash()==originalHash,"Refused connected edit rolls back all poses");
 }
 {Fixture f;auto connect=MakeSetRiverConnectionCommand(f.scene,f.tributary,f.receiver,f.node);Test(connect->Execute(),"Creator-safe connect transaction");
  RiverWaterway graph;std::string error;Test(ReadRiverWaterway(f.scene,graph,error)&&graph.network,"Creator connect yields graph");auto hash=graph.network->DeterministicHash();
  connect->Undo();Test(ReadRiverWaterway(f.scene,graph,error)&&!graph.network,"Creator connect Undo");Test(connect->Execute(),"Creator connect Redo");
  auto disconnect=MakeDisconnectRiverCommand(f.scene,f.tributary);Test(disconnect->Execute(),"Creator-safe disconnect transaction");Test(ReadRiverWaterway(f.scene,graph,error)&&!graph.network,"Creator disconnect graph empty");
  disconnect->Undo();Test(ReadRiverWaterway(f.scene,graph,error)&&graph.network->DeterministicHash()==hash,"Creator disconnect Undo restores graph");}
 {Fixture f;SetRiverConnectionCommand connect(f.scene,f.tributary,f.receiver,f.node);Test(connect.Execute(),connect.Error());
  auto before=CaptureTransform(*f.scene.transforms.GetComponent(f.node));auto after=before;after.translation.x+=.7f;after.translation.z+=.3f;
  SetWatercourseTransformCommand move(f.scene,f.node,before,after);Test(move.Execute(),"Precision guard fixture edit");auto edited=CaptureTransform(*f.scene.transforms.GetComponent(f.node));
  auto* spline=f.scene.splines.GetComponent(f.receiver);const auto width=spline->width;spline->width=std::nextafter(width,std::numeric_limits<float>::infinity());
  const auto ownerWidth=spline->width;move.Undo();const auto kept=CaptureTransform(*f.scene.transforms.GetComponent(f.node));Test(spline->width==ownerWidth&&std::memcmp(&edited,&kept,sizeof(edited))==0,"Sub-micro width owner edit refuses stale Undo");
  spline->width=width;move.Undo();Test(CaptureTransform(*f.scene.transforms.GetComponent(f.node)).translation.x==before.translation.x,"Undo works after exact width restoration");}
 {Fixture f;SetRiverConnectionCommand connect(f.scene,f.tributary,f.receiver,f.node);Test(connect.Execute(),connect.Error());
  auto before=CaptureTransform(*f.scene.transforms.GetComponent(f.node));auto after=before;after.translation.x+=.7f;after.translation.z+=.3f;
  SetWatercourseTransformCommand move(f.scene,f.node,before,after);Test(move.Execute(),"Topology guard fixture edit");
  auto* metadata=f.scene.metadatas.GetComponent(f.receiver);metadata->string_values.set(RiverConnectionMetadataKey,"{}");move.Undo();
  Test(CaptureTransform(*f.scene.transforms.GetComponent(f.node)).translation.x==after.translation.x&&metadata->string_values.get(RiverConnectionMetadataKey)=="{}","New connection carrier refuses stale Undo without clobbering it");
  metadata->string_values.erase(RiverConnectionMetadataKey);move.Undo();Test(CaptureTransform(*f.scene.transforms.GetComponent(f.node)).translation.x==before.translation.x,"Undo after exact topology restoration");
  metadata->string_values.set(RiverConnectionMetadataKey,"{}");Test(!move.Execute()&&metadata->string_values.get(RiverConnectionMetadataKey)=="{}","New connection carrier refuses stale Redo");
  metadata->string_values.erase(RiverConnectionMetadataKey);Test(move.Execute(),"Redo after exact topology restoration");}
 for(int scenario=0;scenario<7;++scenario){Fixture f;auto before=Endpoint(f.scene,f.tributary);auto* s=f.scene.splines.GetComponent(f.receiver);auto node=f.node;float distance=1;
  if(scenario==0){node=s->spline_node_entities.back();auto* t=f.scene.transforms.GetComponent(f.tributary);t->translation_local.x=20;t->SetDirty();t->UpdateTransform();before=Endpoint(f.scene,f.tributary);}
  if(scenario==1)distance=.001f;
  if(scenario==2)distance=std::numeric_limits<float>::quiet_NaN();
  if(scenario==3){for(auto e:f.scene.splines.GetComponent(f.tributary)->spline_node_entities){auto* t=f.scene.transforms.GetComponent(e);t->translation_local.y+=.1f;t->SetDirty();}before=Endpoint(f.scene,f.tributary);}
  if(scenario==4){RiverFlowProfile p;p.enabled=true;p.speed=0;SetRiverFlowProfileCommand edit(f.scene,f.tributary,p);Test(edit.Execute(),"Zero fixture profile");}
  auto receiver=scenario==5?f.tributary:f.receiver;if(scenario==6)node=f.scene.splines.GetComponent(f.tributary)->spline_node_entities.front();
  SetRiverConnectionCommand c(f.scene,f.tributary,receiver,node,distance);Test(!c.Execute(),"Unsafe connection refused");
  Test(Metadata(f).empty()&&Distance(Endpoint(f.scene,f.tributary),before)==0,"Rejected transaction is atomic");
  std::cout<<"REFUSED="<<c.Error()<<"\n";
 }
 {Fixture f;SetRiverConnectionCommand c(f.scene,f.tributary,f.receiver,f.node);Test(c.Execute(),c.Error());
  auto* m=f.scene.metadatas.GetComponent(f.tributary);m->string_values.set(RiverConnectionMetadataKey,"{broken-json");RiverWaterway g;std::string e;
  Test(!ReadRiverWaterway(f.scene,g,e),"Malformed metadata refused");auto remove=SetRiverConnectionCommand::Disconnect(f.scene,f.tributary);Test(remove->Execute(),remove->Error());
  Test(ReadRiverWaterway(f.scene,g,e)&&!g.network,"Explicit disconnect recovers invalid metadata");remove->Undo();Test(m->string_values.get(RiverConnectionMetadataKey)=="{broken-json","Undo malformed metadata recovery exact");}
 {Fixture f;std::shared_ptr<const RiverFlowField> full,a,b;std::string error;Test(BuildRiverFlowField(f.scene,f.receiver,{},full,error),error);
  const float cut=.423f;Test(BuildRiverFlowFieldSpan(*full,0,cut,a,error)&&BuildRiverFlowFieldSpan(*full,cut,1,b,error),error);
  size_t covered=0;for(int x=0;x<400;++x)for(int z=0;z<80;++z){XMFLOAT2 p{-21+(x+.371f)*42/400,-1+(z+.193f)*6/80};const bool oracle=full->Evaluate(p).valid;const bool split=a->Evaluate(p).valid||b->Evaluate(p).valid;
   Test(oracle==split,"Logical split preserves native footprint");covered+=oracle;}
  auto keep=a;Test(!BuildRiverFlowFieldSpan(*full,.8f,.3f,a,error)&&a==keep,"Invalid span preserves prior output");
  std::cout<<"NATIVE_SPAN_COVERAGE_SAMPLES=32000 covered="<<covered<<"\n";
 }
 std::cout<<"RIVER_WATERWAY_CPU=PASS checks="<<checks<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<" checks="<<checks<<"\n";return 1;}}
