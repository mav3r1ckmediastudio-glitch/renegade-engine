#include "../EngineBridge/src/RiverFlowSourceSelection.h"
#include <iostream>
using namespace renegade::bridge::river_flow_detail;
static unsigned checks=0;
static void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
int main(){
 try{
  wi::scene::Scene scene;auto entity=scene.Entity_CreateObject("Source fixture");
  auto& object=*scene.objects.GetComponent(entity);object.meshID=entity;
  auto& mesh=scene.meshes.Create(entity);mesh.aabb=wi::primitive::AABB({-.2f,-1,-.2f},{.2f,1,.2f});
  SourceBounds bounds;bounds.minimum={-2,0,-2};bounds.maximum={2,0,2};
  auto selected=[&]{return IsPotentialSource(scene,entity,object,mesh,&bounds);};
  Check(selected(),"Static waterline obstacle omitted");
  object.SetForeground(true);Check(!selected(),"First-person foreground entered world water");
  object.SetForeground(false);Check(selected(),"Static object failed after foreground removal");
  object.SetRenderable(false);Check(!selected(),"Hidden source entered cache");object.SetRenderable(true);
  auto* transform=scene.transforms.GetComponent(entity);
  transform->translation_local={0,3,0};transform->UpdateTransform();
  Check(!selected(),"Object above water invalidated river");
  object.filterMask|=wi::enums::FILTER_TERRAIN;
  Check(selected(),"Terrain above water lost solid-bank coverage");
  object.filterMask&=~wi::enums::FILTER_TERRAIN;
  transform->translation_local={0,-3,0};transform->UpdateTransform();
  Check(!selected(),"Fully submerged object invalidated surface contact");
  Check(IsPotentialSource(scene,entity,object,mesh,&bounds,true),"Rapids omitted fully submerged bed");
  transform->translation_local={0,-6,0};transform->UpdateTransform();
  Check(!IsPotentialSource(scene,entity,object,mesh,&bounds,true),"Rapids accepted geometry below supported depth");

  transform->translation_local={0,0,0};transform->UpdateTransform();
  Check(selected(),"Object moved into water was not discovered");
  auto parent=scene.Entity_CreateTransform("Dynamic parent");
  transform=scene.transforms.GetComponent(entity);
  auto& hierarchy=scene.hierarchy.Create(entity);hierarchy.parentID=parent;
  scene.rigidbodies.Create(parent).mass=1;
  Check(!selected(),"Dynamic parent child entered static contacts");
  scene.rigidbodies.GetComponent(parent)->mass=0;
  Check(selected(),"Static parent child refused");
  scene.characters.Create(parent);
  Check(!selected(),"Moving character child entered static contacts");
  scene.characters.Remove(parent);scene.rigidbodies.Remove(parent);
  auto& material=scene.materials.Create(entity);material.shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
  Check(!selected(),"Water surface entered its obstacle cache");
  material.shaderType=wi::scene::MaterialComponent::SHADERTYPE_PBR;
  mesh.armatureID=parent;Check(!selected(),"Skinned source entered static contacts");
  mesh.armatureID=wi::ecs::INVALID_ENTITY;
  transform->translation_local={20,0,0};transform->UpdateTransform();
  Check(!selected(),"Distant source invalidated river");
  transform->translation_local={0,0,0};transform->UpdateTransform();
  Check(selected(),"Static terrain/rock source not restored");
  // G13E14 integration regression: runtime waterfall water sheets receive
  // internal generated metadata and MUST NOT enter the real river obstacle
  // cache, even if their object component otherwise looks opaque.
  scene.metadatas.Create(entity).string_values.set(
      "renegade.waterfall.generated.v1","fixture");
  Check(!selected(),"Generated waterfall visual entered river foam obstacle cache");
  std::cout<<"RiverFlowSourceSelectionTests PASS "<<checks<<" checks\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}
