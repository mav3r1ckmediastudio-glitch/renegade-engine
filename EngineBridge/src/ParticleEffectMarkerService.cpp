#include "renegade/bridge/ParticleEffectMarkerService.h"
#include "renegade/bridge/ParticleEffectService.h"
#include <unordered_set>
namespace renegade::bridge {
std::vector<wi::ecs::Entity> ParticleEffectMarkerEntities(const wi::scene::Scene& scene) {
 std::vector<wi::ecs::Entity> result;
 std::unordered_set<wi::ecs::Entity> seen;
 result.reserve(scene.emitters.GetCount());
 for (size_t i=0;i<scene.emitters.GetCount();++i) {
  auto entity=scene.emitters.GetEntity(i);
  const auto root=ParticleEffectRoot(scene,entity);
  if(root!=wi::ecs::INVALID_ENTITY)entity=root;
  if(scene.transforms.Contains(entity)&&seen.insert(entity).second)result.push_back(entity);
 }
 return result;
}
}
