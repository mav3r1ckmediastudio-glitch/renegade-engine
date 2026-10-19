#pragma once
#include "renegade/bridge/GameplayAttributeProfile.h"
namespace renegade::bridge
{
    inline constexpr const char* GameplayAttributeDocumentType="gameplay-attributes";
    enum class GameplayAttributeScope { Project, Player, Character };
    struct GameplayAttributeAttachment
    {
        GameplayAttributeScope scope=GameplayAttributeScope::Project;
        StableId targetId;
        StableId profileId;
        std::vector<GameplayAttributeOverride> overrides;
    };
    struct GameplayAttributeDocument
    {
        DocumentEnvelope envelope;
        unsigned schemaVersion=1;
        std::vector<GameplayAttributeProfile> profiles;
        std::vector<GameplayAttributeAttachment> attachments;
    };
    [[nodiscard]] bool ValidateGameplayAttributeDocument(const GameplayAttributeDocument&,const StableId& projectId,std::string& error);
    [[nodiscard]] bool WriteGameplayAttributeDocument(const std::string& path,const GameplayAttributeDocument&,std::string& error);
    [[nodiscard]] bool ReadGameplayAttributeDocument(const std::string& path,const StableId& projectId,GameplayAttributeDocument&,std::string& error);
    [[nodiscard]] bool ResolveGameplayAttributeDocumentPath(const std::string& root,const StableId& projectId,
        const StableId& documentId,const std::string& hint,std::string& resolved,std::string& error);
}
