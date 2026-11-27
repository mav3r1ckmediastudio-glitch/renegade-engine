#pragma once
namespace renegade::studio
{
    // Only the focused Studio multiline control consumes native character input.
    // Ordinary Renegade property fields retain their existing input path.
    struct StudioMultilineInputRoute
    {
        void* owner=nullptr;
        bool (*character)(void*,wchar_t)=nullptr;
    };
    inline StudioMultilineInputRoute& MultilineInputRoute()
    {static StudioMultilineInputRoute route;return route;}
    inline bool RouteStudioMultilineCharacter(wchar_t value)
    {
        auto& route=MultilineInputRoute();
        return route.owner && route.character && route.character(route.owner,value);
    }
}
