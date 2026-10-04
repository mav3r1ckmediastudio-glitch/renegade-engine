#pragma once
#include <WickedEngine.h>
#include <string>
#include <vector>
namespace renegade::bridge
{
    // Requires one identical named skeleton, parent topology and inverse binds.
    // Call on an isolated working scene. Copies only native clips/keyframe data.
    [[nodiscard]] bool AppendMatchingRigAnimationScene(wi::scene::Scene& destination,
        const wi::scene::Scene& source, std::vector<wi::ecs::Entity>& created,
        std::string& error);
    [[nodiscard]] bool ImportMatchingRigAnimations(wi::scene::Scene& destination,
        const std::string& sourcePath, std::vector<wi::ecs::Entity>& created,
        std::string& error);
}
