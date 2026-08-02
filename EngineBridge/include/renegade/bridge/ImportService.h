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

    struct ImportResult
    {
        bool succeeded = false;
        std::string sourcePath;
        std::string assetPath;
        std::string error;
        ImportedSceneSummary imported;
        ImportedSceneSummary reloaded;
    };

    // Move-only result of the GLB/GLTF conversion phase. Conversion can run on
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

        // Read-only look at the converted scene without transferring
        // ownership, so a caller can resolve something that depends on the
        // converted geometry (such as an automatic scale factor) before
        // deciding to take the scene via ReleaseScene(). Returns nullptr if
        // IsReady() is false.
        [[nodiscard]] const wi::scene::Scene* PeekScene() const noexcept
        {
            return scene_.IsValid() ? scene_.get() : nullptr;
        }

        // Hands ownership of the converted scene to the caller, for callers
        // that place the model directly (PlaceImportedModelCommand) rather
        // than running the Gate 1 WISCENE round-trip proof. Only meaningful
        // when IsReady() is true; the returned pointer may be empty
        // otherwise, and is always empty after this has been called once.
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

    // A creator-facing unit-correction choice for a just-converted model,
    // applied as a uniform, non-destructive Scale on the import root
    // transform -- never baked into vertex positions, bone matrices, or
    // animation keyframes the way GameGuru MAX's importer applies its
    // equivalent scaling modes. glTF 2.0 mandates metres and a Y-up,
    // right-handed space, so Original and Meters always resolve to the same
    // multiplier for this importer; both are offered so the choice reads
    // the same as a familiar FBX/OBJ importer's list, even though only
    // Centimeters, Inches, and Automatic actually change anything for a
    // GLB/GLTF source.
    enum class ModelScaleMode
    {
        Original,
        Meters,
        Centimeters,
        Inches,
        Automatic,
    };

    // UI-independent conversion boundary for Model Import V1. The caller owns
    // file selection and presentation. Wicked's converter requires an active
    // graphics device while it creates native mesh/material render data, but
    // it never touches Renegade's active editor scene.
    class ImportService
    {
    public:
        [[nodiscard]] PreparedModelImport PrepareGltfAsset(
            const std::string& sourcePath,
            const std::string& assetPath) const;

        [[nodiscard]] ImportResult CompleteGltfAsset(
            PreparedModelImport prepared) const;

        [[nodiscard]] static ImportedSceneSummary Summarize(
            const wi::scene::Scene& scene) noexcept;

        // Resolves a scale mode against a prepared (converted, freshly
        // isolated) scene into a single uniform multiplier for the import
        // root's Scale. Automatic normalizes the union of every mesh's own
        // local vertex-position bounds to a human-scale (2 m) target extent
        // -- an honest approximation, not GameGuru MAX's world-space,
        // bone-aware bounding box, but exactly right for the common
        // single-node or flat-hierarchy model. Only meaningful against the
        // isolated scene PrepareGltfAsset produces, before it is merged into
        // a larger active scene, since a merged scene's meshes would no
        // longer belong to just the imported model.
        [[nodiscard]] static float ResolveScaleFactor(
            ModelScaleMode mode,
            const wi::scene::Scene& preparedScene) noexcept;
    };

    // Merges a freshly converted model directly into a live Studio scene and
    // supports Undo/Redo. This is deliberately separate from the Gate 1 proof
    // pipeline above (PrepareGltfAsset/CompleteGltfAsset), which never
    // touches the active scene. PlaceImportedModelCommand does not write any
    // asset file of its own; the placed entity persists the same way any
    // other native entity does, through the normal scene Save path. Reusable
    // project asset registration is a separate, later milestone.
    class PlaceImportedModelCommand final : public ICommand
    {
    public:
        // preparedScene must be a valid, ready result of
        // ImportService::PrepareGltfAsset (via PreparedModelImport::
        // ReleaseScene()). Wicked's Scene::Merge() empties the source scene
        // as it moves content into targetScene, so this command takes
        // ownership of it. scaleFactor is applied as a uniform Scale on the
        // import root alongside placementPosition; pass 1.0f for no
        // correction, or ImportService::ResolveScaleFactor()'s result to
        // apply a unit-correction/Automatic mode chosen before this command
        // is constructed (that resolution needs the still-isolated prepared
        // scene, so it must happen before ReleaseScene() hands it over).
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
