#pragma once

#include <string>

#include <WickedEngine.h>

namespace renegade::studio
{
    // Installs the Studio-owned companion hooks that let the existing Scene
    // Duplicate and reusable Asset Browser placement commands carry governed
    // .rscripts state without creating a second Undo/Redo stack.
    void InstallCharacterPrefabStudioIntegration();

    // Creator-facing Character Inspector action. The selected configured
    // Character is persisted as a portable Character Prefab under
    // Content/Prefabs and revealed in the existing Asset Browser.
    [[nodiscard]] bool SaveSelectedCharacterPrefab(
        wi::ecs::Entity character,
        std::string& status);
}
