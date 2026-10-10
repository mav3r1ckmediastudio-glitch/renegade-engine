#pragma once
#include "renegade/bridge/EquipmentActionState.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
namespace renegade::bridge {
inline constexpr const char* EquipmentAssetExtension=".requipment";
struct EquipmentAssetDocument { StableId projectId; EquipmentDefinition equipment; };
struct EquipmentAssetSaveResult {
    bool succeeded=false;
    EquipmentAssetDocument document;
    std::string projectRelativePath,error;
    ProjectDocumentTransactionResult transaction;
};
bool SerializeEquipmentAsset(const EquipmentAssetDocument&,std::string&,std::string&);
bool DeserializeEquipmentAsset(const std::string&,EquipmentAssetDocument&,std::string&);
bool ReadEquipmentAssetFile(const std::string&,EquipmentAssetDocument&,std::string&);
bool LoadEquipmentAsset(const std::string& root,const StableId& project,const StableId& id,
    EquipmentAssetDocument&,std::string&);
EquipmentAssetSaveResult SaveEquipmentAsset(const std::string& root,const StableId& project,
    EquipmentDefinition,ProjectDocumentTransactionHook hook={});
std::vector<EquipmentAssetDocument> ListEquipmentAssets(const std::string& root,
    const StableId& project,std::string&);
// Copies supported semantic actions and native paired durations, without changing the assembly.
bool PrepareEquipmentFromAssembly(const std::string& root,const StableId& project,
    const StableId& presentation,const std::string& name,EquipmentHandUse,
    EquipmentDefinition&,std::string&);
bool ValidateStartingEquipment(const std::string& root,const StableId& project,
    const StableId& primary,const StableId& offHand,std::string&);
}
