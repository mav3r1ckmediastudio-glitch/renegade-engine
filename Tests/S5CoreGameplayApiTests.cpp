#include "RuntimeScriptEntityApi.h"

#include "renegade/bridge/IdentityService.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>

namespace
{
    using namespace renegade;

    int Fail(const std::string& message)
    {
        std::cerr << "S5 core gameplay API test failed: " << message << '\n';
        return 1;
    }

    bool Near(const float left, const float right) noexcept
    {
        return std::fabs(left - right) < 0.0001f;
    }

    bool PositionMatches(
        const wi::scene::TransformComponent& transform,
        const float x,
        const float y,
        const float z) noexcept
    {
        const XMFLOAT3 world = transform.GetPosition();
        return Near(world.x, x) && Near(world.y, y) && Near(world.z, z);
    }
}

int main()
{
    wi::scene::Scene scene;
    const auto entity = scene.Entity_CreateTransform("S5 Test Barrel");

    std::string error;
    const bridge::StableId entityId = bridge::GenerateStableId();
    if (!bridge::AssignPersistentEntityId(scene, entity, entityId, error))
        return Fail("assign persistent entity ID: " + error);

    std::unordered_map<bridge::StableId, wi::ecs::Entity> entitiesById;
    entitiesById.emplace(entityId, entity);

    constexpr std::uint64_t Generation = 9;
    runtime::RuntimeScriptEntityApi api(scene, entitiesById, Generation);
    runtime::RuntimeScriptEntityReference reference{entityId, Generation};

    wi::ecs::Entity resolved = wi::ecs::INVALID_ENTITY;
    if (!api.Resolve(reference, resolved, error) || resolved != entity)
        return Fail("live persistent EntityRef did not resolve: " + error);

    std::string name;
    if (!api.GetName(reference, name, error) || name != "S5 Test Barrel")
        return Fail("creator-safe entity name did not resolve");

    XMFLOAT3 position;
    if (!api.GetLocalPosition(reference, position, error) ||
        !Near(position.x, 0.0f) || !Near(position.y, 0.0f) || !Near(position.z, 0.0f))
    {
        return Fail("initial local position was not readable");
    }

    if (!api.SetLocalPosition(reference, XMFLOAT3(1.0f, 2.0f, 3.0f), error))
        return Fail("set local position: " + error);
    if (!api.GetLocalPosition(reference, position, error) ||
        !Near(position.x, 1.0f) || !Near(position.y, 2.0f) || !Near(position.z, 3.0f))
    {
        return Fail("set local position did not persist in the live Scene");
    }
    const auto* liveTransform = scene.transforms.GetComponent(entity);
    if (liveTransform == nullptr || !PositionMatches(*liveTransform, 1.0f, 2.0f, 3.0f))
        return Fail("set local position did not update the Wicked world transform");

    if (!api.TranslateLocal(reference, XMFLOAT3(0.5f, -1.0f, 2.0f), error))
        return Fail("translate local position: " + error);
    if (!api.GetLocalPosition(reference, position, error) ||
        !Near(position.x, 1.5f) || !Near(position.y, 1.0f) || !Near(position.z, 5.0f))
    {
        return Fail("local translation produced the wrong transform");
    }
    liveTransform = scene.transforms.GetComponent(entity);
    if (liveTransform == nullptr || !PositionMatches(*liveTransform, 1.5f, 1.0f, 5.0f))
        return Fail("local translation did not update the Wicked world transform");

    const XMFLOAT3 beforeInvalid = position;
    error.clear();
    if (api.SetLocalPosition(
            reference,
            XMFLOAT3(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f),
            error) || error.find("finite") == std::string::npos)
    {
        return Fail("non-finite transform input was not rejected");
    }
    if (!api.GetLocalPosition(reference, position, error) ||
        !Near(position.x, beforeInvalid.x) ||
        !Near(position.y, beforeInvalid.y) ||
        !Near(position.z, beforeInvalid.z))
    {
        return Fail("rejected transform input mutated the Scene");
    }

    runtime::RuntimeScriptEntityReference stale{entityId, Generation - 1};
    error.clear();
    if (api.Resolve(stale, resolved, error) ||
        error.find("stale Level generation") == std::string::npos)
    {
        return Fail("stale-generation EntityRef was accepted");
    }

    scene.Entity_Remove(entity);
    error.clear();
    if (api.Resolve(reference, resolved, error) || error.empty())
        return Fail("removed same-generation entity remained live through the resolver");

    return 0;
}
