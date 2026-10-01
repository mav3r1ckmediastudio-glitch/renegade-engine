#pragma once

#include "renegade/bridge/ImportService.h"

#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    struct ModelImportDependency
    {
        std::string sourcePath; // Empty for embedded data.
        std::string referenceName;
        std::string previewResourceName;
        std::string retainedRelativePath;
        std::vector<std::uint8_t> bytes;
    };

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
        [[nodiscard]] ModelSourceFormat SourceFormat() const noexcept { return sourceFormat_; }
        [[nodiscard]] const std::vector<ModelImportDependency>& Dependencies() const noexcept { return dependencies_; }
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
        ModelSourceFormat sourceFormat_ = ModelSourceFormat::Unknown;
        std::vector<ModelImportDependency> dependencies_;
        ImportedSceneSummary summary_;
        ImportedModelEvidence evidence_;
    };

    class ModelImportCandidateService
    {
    public:
        // Static GLB or FBX. FBX file textures are snapshotted before conversion;
        // dependencies must be embedded or within the model source folder.
        [[nodiscard]] ModelImportCandidate PrepareStaticModel(
            const std::string& sourcePath) const;
        // Compatibility entry point preserves the original GLB-only contract.
        [[nodiscard]] ModelImportCandidate PrepareGlb(
            const std::string& sourcePath) const;
    };
}
