#include "renegade/bridge/SelectionService.h"
#include <algorithm>
namespace renegade::bridge {
void SelectionService::Select(wi::ecs::Entity e) { Set(e==wi::ecs::INVALID_ENTITY?std::vector<wi::ecs::Entity>{}:std::vector<wi::ecs::Entity>{e}); }
void SelectionService::Set(std::vector<wi::ecs::Entity> values) {
    std::vector<wi::ecs::Entity> unique;
    for(auto e:values) if(e!=wi::ecs::INVALID_ENTITY && std::find(unique.begin(),unique.end(),e)==unique.end()) unique.push_back(e);
    if(unique!=entities_) { entities_=std::move(unique);++revision_; }
}
void SelectionService::Toggle(wi::ecs::Entity e) {
    if(e==wi::ecs::INVALID_ENTITY)return;
    auto values=entities_;auto it=std::find(values.begin(),values.end(),e);
    if(it==values.end())values.push_back(e);else values.erase(it);
    Set(std::move(values));
}
void SelectionService::Clear() noexcept { if(!entities_.empty()){entities_.clear();++revision_;} }
bool SelectionService::Contains(wi::ecs::Entity e) const noexcept { return std::find(entities_.begin(),entities_.end(),e)!=entities_.end(); }
void SelectionService::Prune(const wi::scene::Scene& scene) {
    auto values=entities_;values.erase(std::remove_if(values.begin(),values.end(),[&](auto e){return !scene.transforms.Contains(e)&&!scene.names.Contains(e);}),values.end());Set(std::move(values));
}
}
