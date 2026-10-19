#pragma once
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct ScreenPortableAdoptionResult
    {
        StableId documentId;
        std::string pathHint;
        std::vector<std::string> unresolvedBindings;
        std::string receiptPathHint;
    };
    // A self-contained hashed, versioned folder. Existing destinations are never overwritten.
    [[nodiscard]] bool PublishPortableScreenPackage(const std::string& projectRoot,const StableId& projectId,
        const StableId& entryId,const std::string& entryHint,const std::string& destination,std::string& error);
    // Fresh project/widget/resource identities; external gameplay slots remain explicitly unassigned.
    // All project files are committed through one journaled transaction.
    [[nodiscard]] bool AdoptPortableScreenPackage(const std::string& projectRoot,const StableId& projectId,
        const std::string& packageRoot,ScreenPortableAdoptionResult& result,std::string& error,
        ProjectDocumentTransactionHook operationHook={});

    struct ScreenLibraryEntry
    {
        StableId documentId;
        std::string name;
        std::string pathHint;
        ScreenUsage usage=ScreenUsage::Menu;
        bool enabledInGame=true;
        bool available=false;
        std::string diagnostic;
    };
    struct ScreenLibraryCatalogue
    {
        bool succeeded=false;
        std::vector<ScreenLibraryEntry> screens;
        std::vector<std::string> diagnostics;
        std::string error;
    };
    [[nodiscard]] bool ResolveProjectScreenLibraryDocument(
        const std::string& projectRoot,const StableId& projectId,const StableId& documentId,
        const std::string& pathHint,ScreenDocument& document,std::string& resolvedPath,
        std::string& resolvedHint,std::string& error);

    struct ScreenLibraryCreateResult
    {
        bool succeeded=false;
        StableId documentId;
        std::string pathHint;
        std::string error;
    };
    // Create a project-owned standalone UI document, never a StoryFlow node.
    // Paths and stable IDs are generated internally, not supplied by a caller.
    [[nodiscard]] ScreenLibraryCreateResult CreateProjectScreenLibraryDocument(
        const std::string& projectRoot,const StableId& projectId,ScreenUsage usage);

    // Read-only lifecycle projection, shared by the Designer and HUD assignment.
    // Call on project/library refresh, never during per-frame GUI update.
    [[nodiscard]] ScreenLibraryCatalogue EnumerateProjectScreenLibrary(
        const std::string& projectRoot,const StableId& projectId);
}
