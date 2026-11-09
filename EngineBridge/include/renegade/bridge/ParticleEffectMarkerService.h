#pragma once
#include <WickedEngine.h>
#include <vector>
namespace renegade::bridge {
 // One editor marker per composite effect; legacy standalone emitters remain selectable.
 // Iterates only the emitter component manager, never a whole-scene inventory.
 std::vector<wi::ecs::Entity> ParticleEffectMarkerEntities(const wi::scene::Scene& scene);
}
