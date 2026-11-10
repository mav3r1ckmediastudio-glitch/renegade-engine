#pragma once
#include "renegade/bridge/ScriptDocumentService.h"
#include "renegade/bridge/ReusableAssetInstanceService.h"
#include <utility>
namespace renegade::bridge {
inline constexpr const char* SceneAssemblyScriptTemplateKey = "renegade.scene_assembly.scripts_v1";
inline constexpr const char* SceneAssemblyLocalIdKey = "renegade.scene_assembly.local_id";
inline constexpr const char* SceneAssemblyAuthoredRootKey = "renegade.scene_assembly.authored_root";
inline constexpr const char* SceneAssemblyPreserveContentsKey = "renegade.scene_assembly.preserve_contents";
using SceneEntityPairs = std::vector<std::pair<wi::ecs::Entity,wi::ecs::Entity>>;
bool CaptureSceneAssemblyScripts(const std::string& projectRoot, const StableId& project,
    const wi::scene::Scene& source, const ScriptDocument*,
    const SceneEntityPairs&, wi::scene::Scene& payload, wi::ecs::Entity payloadRoot,
    std::string& error);
bool HasSceneAssemblyScriptTemplate(const wi::scene::Scene&,wi::ecs::Entity);
bool ApplySceneAssemblyScripts(wi::scene::Scene&,wi::ecs::Entity,const std::string& projectRoot,
    ScriptDocument*,ReusablePlacementCompanionCallbacks&,std::string& error);
bool DuplicateHierarchyScriptAttachments(ScriptDocument&, const wi::scene::Scene&,
    const SceneEntityPairs&, std::string& error);
std::unique_ptr<ICommand> MakeDeleteSceneHierarchyCommand(
    wi::scene::Scene&,ScriptDocument*,wi::ecs::Entity);
}
