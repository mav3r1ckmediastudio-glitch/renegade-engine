#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <WickedEngine.h>

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

    // UI-independent conversion boundary for Model Import V1. The caller owns
    // file selection and presentation. Wicked's converter requires an active
    // graphics device while it creates native mesh/material render data, but
    // it never touches Renegade's active editor scene.
    class ImportService
    {
    public:
        [[nodiscard]] ImportResult ImportGltfAsset(
            const std::string& sourcePath,
            const std::string& assetPath) const;

        [[nodiscard]] static ImportedSceneSummary Summarize(
            const wi::scene::Scene& scene) noexcept;
    };
}
