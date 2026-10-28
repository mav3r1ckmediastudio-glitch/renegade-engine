#pragma once
#include <WickedEngine.h>
#include <memory>
#include <string>
namespace renegade::bridge {
class PlaceableWaterfallRenderer {
public:
 PlaceableWaterfallRenderer();~PlaceableWaterfallRenderer();
 void Prepare(wi::scene::Scene&,uint64_t revision,float dt,bool suspended);
 void Clear(wi::scene::Scene&);
 bool Owns(wi::ecs::Entity)const;wi::ecs::Entity Owner(wi::ecs::Entity)const;
 size_t ActiveCount()const;const std::string& Status()const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
void RemoveGeneratedWaterfallVisuals(wi::scene::Scene&);
}
