#pragma once
#include <WickedEngine.h>
#include <functional>
#include <string>

namespace renegade::runtime
{
// Trusted host callbacks. Lua receives no Runtime pointer, file access or raw
// ECS entity: RuntimeScriptRuntime validates EntityRefs before invoking start.
struct RuntimeDialogueScriptApi
{
    std::function<bool(wi::ecs::Entity, const std::string &, const std::string &, std::string &)> start;
    std::function<bool(std::string &)> stop;
    std::function<bool()> isActive;
    std::function<bool(const std::string &, int &, std::string &)> getValue;
    std::function<bool(const std::string &, int, std::string &)> setValue;
};
}
