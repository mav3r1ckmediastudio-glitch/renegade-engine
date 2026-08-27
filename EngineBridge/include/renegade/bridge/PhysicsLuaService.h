#pragma once

#include <WickedEngine.h>

namespace renegade::bridge
{
    // JP01 scripting seam. Wicked owns the single Lua VM and scene/script
    // execution lifecycle; Renegade only installs a stable entity-oriented
    // physics namespace into that VM.
    void BindPhysicsLua(wi::scene::Scene& scene);
    void UnbindPhysicsLua(const wi::scene::Scene* scene = nullptr) noexcept;

    [[nodiscard]] bool IsPhysicsLuaBound() noexcept;
    [[nodiscard]] bool IsPhysicsLuaBoundTo(
        const wi::scene::Scene& scene) noexcept;
}
