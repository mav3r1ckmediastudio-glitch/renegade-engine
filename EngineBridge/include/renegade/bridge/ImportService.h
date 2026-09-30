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
        std::uint64_t meshFingerprint = 0;
        std::uint64_t skinIndexFingerprint = 0;
        std::uint64_t skinWeightFingerprint = 0;
        std::uint64_t armatureFingerprint = 0;
        std::uint64_t boneHierarchyFingerprint = 0;
        std::uint64_t inverseBindFingerprint = 0;
        std::uint64_t animationFingerprint = 0;
        std::uint64_t animationChannelFingerprint = 0;
        std::uint64_t animationSamplerFingerprint = 0;
        std::uint64_t animationDataFingerprint = 0;
        std::uint64_t animationTimesFingerprint = 0;
        std::uint64_t animationValuesFingerprint = 0;
        std::uint64_t rigAnimationFingerprint = 0;

        [[nodiscard]] bool HasRigOrAnimationPayload() const noexcept
        {
            return skinnedMeshes != 0 ||
                primaryInfluenceVertices != 0 ||
                secondaryInfluenceVertices != 0 ||
                armatureBones != 0 ||
                animationChannels != 0 ||
                animationSamplers != 0 ||
                animationData != 0 ||
                animationKeyframes != 0 ||
                animationValues != 0;
        }

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
                meshFingerprint == other.meshFingerprint &&
                skinIndexFingerprint == other.skinIndexFingerprint &&
                skinWeightFingerprint == other.skinWeightFingerprint &&
                armatureFingerprint == other.armatureFingerprint &&
                boneHierarchyFingerprint == other.boneHierarchyFingerprint &&
                inverseBindFingerprint == other.inverseBindFingerprint &&
                animationFingerprint == other.animationFingerprint &&
                animationChannelFingerprint == other.animationChannelFingerprint &&
                animationSamplerFingerprint == other.animationSamplerFingerprint &&
                animationDataFingerprint == other.animationDataFingerprint &&
                animationTimesFingerprint == other.animationTimesFingerprint &&
                animationValuesFingerprint == other.animationValuesFingerprint &&
                rigAnimationFingerprint == other.rigAnimationFingerprint;
        }
    };

    enum class ModelScaleMode
    {
        Original,
        Meters,
        Centimeters,
        Inches,
        Automatic,
    };

    struct ModelBounds
    {
        bool valid = false;
        XMFLOAT3 minimum = {};
        XMFLOAT3 maximum = {};

        [[nodiscard]] XMFLOAT3 Extents() const noexcept
        {
            return valid
                ? XMFLOAT3(maximum.x - minimum.x,
                    maximum.y - minimum.y,
                    maximum.z - minimum.z)
                : XMFLOAT3{};
        }
    };

    class ImportService
    {
    public:
        [[nodiscard]] static ModelSourceFormat ClassifyModelSourceFormat(
            const std::string& sourcePath) noexcept;
        [[nodiscard]] static bool IsModelSourceFormatSupported(
            ModelSourceFormat format) noexcept;
        [[nodiscard]] static const char* ModelSourceFormatName(
            ModelSourceFormat format) noexcept;
        [[nodiscard]] static ImportedModelEvidence SummarizeModelEvidence(
            const wi::scene::Scene& scene) noexcept;

        [[nodiscard]] static ImportedSceneSummary Summarize(
            const wi::scene::Scene& scene) noexcept;
        [[nodiscard]] static float ResolveScaleFactor(
            ModelScaleMode mode,
            const wi::scene::Scene& preparedScene) noexcept;
        [[nodiscard]] static ModelBounds MeasureModelBounds(
            const wi::scene::Scene& preparedScene) noexcept;
        [[nodiscard]] static bool RebuildHierarchyAwareModelBounds(
            wi::scene::Scene& preparedScene) noexcept;
        [[nodiscard]] static float ResolveScaleFactorForTargetHeight(
            float targetHeightMeters,
            const wi::scene::Scene& preparedScene) noexcept;
        [[nodiscard]] static float ResolveGroundedPlacementY(
            float surfaceY,
            const ModelBounds& bounds,
            float scaleFactor) noexcept;
    };


}
