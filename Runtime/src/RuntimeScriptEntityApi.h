#pragma once

#include "renegade/bridge/IdentityService.h"

#include <WickedEngine.h>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace renegade::runtime
{
    // Runtime-only view of one governed EntityRef. The stable ID is never
    // surfaced to creator Lua as a string; RuntimeScriptRuntime owns the opaque
    // userdata and projects only this view into the C++ gameplay API layer.
    struct RuntimeScriptEntityReference final
    {
        bridge::StableId stableId;
        std::uint64_t generation = 0;
    };

    // S5A core entity/transform safety seam.
    //
    // This service deliberately owns no Lua state and no entity identity. It
    // resolves the opaque reference against S3's current generation map, then
    // revalidates the live Scene's persistent Renegade identity on every call.
    // That preserves S3's stale-ref guarantees while keeping gameplay mutation
    // independent of Wicked's global Lua VM and raw ECS IDs.
    class RuntimeScriptEntityApi final
    {
    public:
        RuntimeScriptEntityApi(
            wi::scene::Scene& scene,
            const std::unordered_map<bridge::StableId, wi::ecs::Entity>& entitiesById,
            std::uint64_t generation) noexcept;

        [[nodiscard]] bool Resolve(
            const RuntimeScriptEntityReference& reference,
            wi::ecs::Entity& entity,
            std::string& error) const;

        [[nodiscard]] bool GetName(
            const RuntimeScriptEntityReference& reference,
            std::string& name,
            std::string& error) const;

        [[nodiscard]] bool GetLocalPosition(
            const RuntimeScriptEntityReference& reference,
            XMFLOAT3& position,
            std::string& error) const;

        [[nodiscard]] bool SetLocalPosition(
            const RuntimeScriptEntityReference& reference,
            const XMFLOAT3& position,
            std::string& error) const;

        [[nodiscard]] bool TranslateLocal(
            const RuntimeScriptEntityReference& reference,
            const XMFLOAT3& delta,
            std::string& error) const;

    private:
        [[nodiscard]] bool ResolveTransform(
            const RuntimeScriptEntityReference& reference,
            wi::scene::TransformComponent*& transform,
            std::string& error) const;
        [[nodiscard]] static bool IsFinite(const XMFLOAT3& value) noexcept;

        wi::scene::Scene* scene_ = nullptr;
        const std::unordered_map<bridge::StableId, wi::ecs::Entity>* entitiesById_ = nullptr;
        std::uint64_t generation_ = 0;
    };
}
