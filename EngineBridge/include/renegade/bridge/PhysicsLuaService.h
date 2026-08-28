#pragma once

#include <WickedEngine.h>

struct lua_State;

namespace renegade::bridge
{
    // JP01 scripting seam. Wicked owns the single Lua VM and scene/script
    // execution lifecycle; Renegade only installs a stable entity-oriented
    // physics namespace into an already-created VM.
    //
    // Production callers use the one-argument overload after Wicked has
    // initialized Lua. The explicit-state overload exists for isolated
    // contract tests and embedders and never takes ownership of the state.
    [[nodiscard]] bool BindPhysicsLua(wi::scene::Scene& scene) noexcept;
    [[nodiscard]] bool BindPhysicsLua(
        wi::scene::Scene& scene,
        lua_State* state) noexcept;
    void UnbindPhysicsLua(const wi::scene::Scene* scene = nullptr) noexcept;

    [[nodiscard]] bool IsPhysicsLuaBound() noexcept;
    [[nodiscard]] bool IsPhysicsLuaBoundTo(
        const wi::scene::Scene& scene) noexcept;
}
