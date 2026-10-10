#pragma once
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ProjectService.h"

namespace renegade::bridge
{
    inline constexpr const char* ImpactDefaultsDirectionalKey="renegade.impact.defaults.directional_blood";
    bool IsImpactDefaultsMaterialKey(const std::string& key);
    // Imports immutable project-owned resources, preserving existing materials
    // and audio banks. Scene adoption is a separate undoable command.
    std::unique_ptr<ICommand> PrepareImpactDefaults(
        wi::scene::Scene& scene,const ProjectMetadata& target,
        const std::string& libraryDescriptor,std::string& error);
}
