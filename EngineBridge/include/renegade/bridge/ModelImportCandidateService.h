#pragma once

#include "renegade/bridge/ImportService.h"

#include <cstdint>
#include <string>

namespace renegade::bridge
{
    // A conversion candidate is kept separate from the active editor Scene.
    // It is not a project asset until the later transaction commits and reopens it.
    class ModelImportCandidate
    {
    public:
        ModelImportCandidate() = default;
        ModelImportCandidate(ModelImportCandidate&&) noexcept = default;
        ModelImportCandidate& operator=(ModelImportCandidate&&) noexcept = default;
        ModelImportCandidate(const ModelImportCandidate&) = delete;
        ModelImportCandidate& operator=(const ModelImportCandidate&) = delete;

        [[nodiscard]] bool IsReady() const noexcept
        {
            return scene_.IsValid() && error_.empty();
        }
        [[nodiscard]] const std::string& Error() const noexcept { return error_; }
        [[nodiscard]] const std::string& SourcePath() const noexcept { return sourcePath_; }
        [[nodiscard]] std::uint64_t SourceBytes() const noexcept { return sourceBytes_; }
        [[nodiscard]] std::uint64_t SourceFingerprint() const noexcept { return sourceFingerprint_; }
        [[nodiscard]] const ImportedSceneSummary& Summary() const noexcept { return summary_; }
        [[nodiscard]] const ImportedModelEvidence& Evidence() const noexcept { return evidence_; }
        [[nodiscard]] const wi::scene::Scene* PeekScene() const noexcept
        {
            return scene_.IsValid() ? scene_.get() : nullptr;
        }
        [[nodiscard]] wi::scene::Scene* PeekMutableScene() noexcept
        {
            return scene_.IsValid() ? scene_.get() : nullptr;
        }
        [[nodiscard]] wi::allocator::shared_ptr<wi::scene::Scene> ReleaseScene() noexcept
        {
            return std::move(scene_);
        }

    private:
        friend class ModelImportCandidateService;
        wi::allocator::shared_ptr<wi::scene::Scene> scene_;
        std::string sourcePath_;
        std::string error_;
        std::uint64_t sourceBytes_ = 0;
        std::uint64_t sourceFingerprint_ = 0;
        ImportedSceneSummary summary_;
        ImportedModelEvidence evidence_;
    };

    class ModelImportCandidateService
    {
    public:
        // The first vertical slice is GLB only. Commit must still inspect URI
        // references and reject any external dependency it cannot retain.
        [[nodiscard]] ModelImportCandidate PrepareGlb(
            const std::string& sourcePath) const;
    };
}
