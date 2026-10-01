#pragma once

#include "renegade/bridge/ModelImportCandidateService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "renegade/bridge/ReusableAssetService.h"

#include <string>

namespace renegade::bridge
{
    struct ModelImportCommitRequest
    {
        std::string projectRoot;
        StableId projectId;
        std::string assetName;
        // Optional for headless callers; Studio supplies its rendered PNG.
        std::vector<std::uint8_t> thumbnailPng;
    };

    struct ModelImportCommitResult
    {
        bool succeeded = false;
        bool committed = false;
        StableId assetId;
        StableId sourceAssetId;
        std::string assetProjectRelativePath;
        std::string sourceProjectRelativePath;
        ProjectDocumentTransactionResult transaction;
        std::string error;
    };

    class ModelImportCommitService
    {
    public:
        // Run on Wicked's thread-safe point. All project-visible writes cross
        // one journaled transaction; success requires catalogue and placement
        // reopen by the committed stable ID.
        [[nodiscard]] ModelImportCommitResult CommitStaticModel(
            const ModelImportCommitRequest& request,
            ModelImportCandidate& candidate) const;
        [[nodiscard]] ModelImportCommitResult CommitGlb(
            const ModelImportCommitRequest& request,
            ModelImportCandidate& candidate) const;
    };
}
