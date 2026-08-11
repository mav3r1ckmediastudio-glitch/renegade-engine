#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <WickedEngine.h>

#include "renegade/bridge/CommandService.h"

namespace renegade::bridge
{
    struct ImportedSceneSummary
    {
        std::size_t names = 0;
        std::size_t transforms = 0;
        std::size_t hierarchy = 0;
        std::size_t objects = 0;
        std::size_t meshes = 0;
        std::size_t materials = 0;
        std::size_t armatures = 0;
        std::size_t animations = 0;
        std::size_t textureReferences = 0;
        std::uint64_t structuralFingerprint = 0;

        [[nodiscard]] bool operator==(
            const ImportedSceneSummary& other) const noexcept
        {
            return names == other.names &&
                transforms == other.transforms &&
                hierarchy == other.hierarchy &&
                objects == other.objects &&
                meshes == other.meshes &&
                materials == other.materials &&
                armatures == other.armatures &&
                animations == other.animations &&
                textureReferences == other.textureReferences &&
                structuralFingerprint == other.structuralFingerprint;
        }
    };

    enum class ModelSourceFormat
    {
        Unknown,
        Fbx,
        Gltf,
        Glb,
        Obj,
        Ply,
        Vrm,
        Vrma,
    };

    struct ModelImportRequest
    {
        std::string sourcePath;
        std::string assetPath;

        // Unknown means infer from the source extension. A non-Unknown value
        // is an explicit contract and must agree with the extension.
        ModelSourceFormat expectedFormat = ModelSourceFormat::Unknown;
    };

    // Structural evidence that specifically covers the data a rigged or
    // animated FBX can lose even when a scene still contains the same number
    // of top-level components. The fingerprint is built from stable component
    // indices plus bone weights, inverse bind matrices, animation channels,
    // samplers and keyframe payloads so it can be compared after WISCENE
    // serialization/reload.
    struct ImportedModelEvidence
    {
        std::size_t skinnedMeshes = 0;
        std::size_t primaryInfluenceVertices = 0;
        std::size_t secondaryInfluenceVertices = 0;
        std::size_t armatureBones = 0;
        std::size_t animationChannels = 0;
        std::size_t animationSamplers = 0;
        std::size_t animationData = 0;
        std::size_t animationKeyframes = 0;
        std::size_t animationValues = 0;
        std::uint64_t rigAnimationFingerprint = 0;

        [[nodiscard]] bool operator==(
            const ImportedModelEvidence& other) const noexcept
        {
            return skinnedMeshes == other.skinnedMeshes &&
                primaryInfluenceVertices == other.primaryInfluenceVertices &&
                secondaryInfluenceVertices == other.secondaryInfluenceVertices &&
                armatureBones == other.armatureBones &&
                animationChannels == other.animationChannels &&
                animationSamplers == other.animationSamplers &&
                animationData == other.animationData &&
                animationKeyframes == other.animationKeyframes &&
                animationValues == other.animationValues &&
                rigAnimationFingerprint == other.rigAnimationFingerprint;
        }
    };

    struct ImportResult
    {
        bool succeeded = false;
        std::string sourcePath;
        std::string assetPath;
        std::string error;
        ModelSourceFormat sourceFormat = ModelSourceFormat::Unknown;
        std::string importerBackend;
        std::uint64_t sourceBytes = 0;
        std::uint64_t sourceFingerprint = 0;
        ImportedSceneSummary imported;
        ImportedSceneSummary reloaded;
        ImportedModelEvidence importedEvidence;
        ImportedModelEvidence reloadedEvidence;
    };

    // Move-only result of the model conversion phase. Conversion can run on
    // Wicked's job system, but WISCENE serialization must be completed at the
    // engine thread-safe point, matching the upstream Editor's save path.
    class PreparedModelImport
    {
    public:
        PreparedModelImport() = default;
        PreparedModelImport(PreparedModelImport&&) noexcept = default;
        PreparedModelImport& operator=(PreparedModelImport&&) noexcept = default;
        PreparedModelImport(const PreparedModelImport&) = delete;
        PreparedModelImport& operator=(const PreparedModelImport&) = delete;

        [[nodiscard]] bool IsReady() const noexcept
        {
            return scene_.IsValid() && result_.error.empty();
        }

        [[nodiscard]] const ImportResult& Result() const noexcept
        {
            return result_;
        }

        [[nodiscard]] const wi::scene::Scene* PeekScene() const noexcept
        {
            return scene_.IsValid() ? scene_.get() : nullptr;
        }

        [[nodiscard]] wi::allocator::shared_ptr<wi::scene::Scene>
        ReleaseScene() noexcept
        {
            return std::move(scene_);
        }

    private:
        friend class ImportService;

        wi::allocator::shared_ptr<wi::scene::Scene> scene_;
        ImportResult result_;
    };

    enum class ModelScaleMode
    {
        Original,
        Meters,
        Centimeters,
        Inches,
        Automatic,
    };

    // UI-independent conversion boundary. Gate 1 supports FBX through the
    // pinned Wicked/ufbx converter and GLB/GLTF through Wicked's native glTF
    // converter. Other formats can be classified without being silently
    // accepted. Conversion always occurs in an isolated Wicked scene.
    class ImportService
    {
    public:
        [[nodiscard]] PreparedModelImport PrepareModelAsset(
            const ModelImportRequest& request) const;

        [[nodiscard]] ImportResult CompleteModelAsset(
            PreparedModelImport prepared) const;

        [[nodiscard]] ImportResult SavePreparedModelAsset(
            PreparedModelImport& prepared) const;

        [[nodiscard]] static ModelSourceFormat ClassifyModelSourceFormat(
            const std::string& sourcePath) noexcept;

        [[nodiscard]] static bool IsModelSourceFormatSupported(
            ModelSourceFormat format) noexcept;

        [[nodiscard]] static const char* ModelSourceFormatName(
            ModelSourceFormat format) noexcept;

        [[nodiscard]] static ImportedModelEvidence SummarizeModelEvidence(
            const wi::scene::Scene& scene) noexcept;

        // Compatibility surface retained while Studio's existing GLB/GLTF
        // workflow is migrated onto the neutral contract in later LP07 gates.
        [[nodiscard]] PreparedModelImport PrepareGltfAsset(
            const std::string& sourcePath,
            const std::string& assetPath) const;

        [[nodiscard]] ImportResult CompleteGltfAsset(
            PreparedModelImport prepared) const;

        [[nodiscard]] ImportResult SavePreparedGltfAsset(
            PreparedModelImport& prepared) const;

        [[nodiscard]] static ImportedSceneSummary Summarize(
            const wi::scene::Scene& scene) noexcept;

        [[nodiscard]] static float ResolveScaleFactor(
            ModelScaleMode mode,
            const wi::scene::Scene& preparedScene) noexcept;
    };

    class PlaceImportedModelCommand final : public ICommand
    {
    public:
        PlaceImportedModelCommand(
            wi::scene::Scene& targetScene,
            wi::allocator::shared_ptr<wi::scene::Scene> preparedScene,
            const XMFLOAT3& placementPosition,
            float scaleFactor = 1.0f);

        bool Execute() override;
        void Undo() override;

        [[nodiscard]] wi::ecs::Entity PlacedEntity() const noexcept;

    private:
        wi::scene::Scene* scene_ = nullptr;
        wi::allocator::shared_ptr<wi::scene::Scene> preparedScene_;
        XMFLOAT3 placementPosition_ = {};
        float scaleFactor_ = 1.0f;
        wi::ecs::Entity entity_ = wi::ecs::INVALID_ENTITY;
        wi::Archive snapshot_;
        bool hasSnapshot_ = false;
    };
}
