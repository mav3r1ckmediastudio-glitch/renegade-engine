#include "renegade/bridge/ImportService.h"

#include <ModelImporter.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr std::uint64_t FingerprintSeed = 1469598103934665603ull;
    constexpr std::uint64_t FingerprintPrime = 1099511628211ull;

    void HashBytes(std::uint64_t& hash, const void* bytes, std::size_t count)
    {
        const auto* data = static_cast<const unsigned char*>(bytes);
        for (std::size_t index = 0; index < count; ++index)
        {
            hash ^= data[index];
            hash *= FingerprintPrime;
        }
    }

    template<typename Value>
    void HashValue(std::uint64_t& hash, const Value& value)
    {
        HashBytes(hash, &value, sizeof(value));
    }

    void HashFloat4(std::uint64_t& hash, const XMFLOAT4& value)
    {
        HashValue(hash, value.x);
        HashValue(hash, value.y);
        HashValue(hash, value.z);
        HashValue(hash, value.w);
    }

    void HashUInt4(std::uint64_t& hash, const XMUINT4& value)
    {
        HashValue(hash, value.x);
        HashValue(hash, value.y);
        HashValue(hash, value.z);
        HashValue(hash, value.w);
    }

    void HashMatrix(std::uint64_t& hash, const XMFLOAT4X4& value)
    {
        HashValue(hash, value._11); HashValue(hash, value._12);
        HashValue(hash, value._13); HashValue(hash, value._14);
        HashValue(hash, value._21); HashValue(hash, value._22);
        HashValue(hash, value._23); HashValue(hash, value._24);
        HashValue(hash, value._31); HashValue(hash, value._32);
        HashValue(hash, value._33); HashValue(hash, value._34);
        HashValue(hash, value._41); HashValue(hash, value._42);
        HashValue(hash, value._43); HashValue(hash, value._44);
    }

    void HashString(std::uint64_t& hash, const std::string& value)
    {
        HashValue(hash, value.size());
        if (!value.empty())
            HashBytes(hash, value.data(), value.size());
    }

    void HashEntitySemanticIdentity(
        std::uint64_t& hash,
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity,
        const std::size_t depth = 0)
    {
        const bool valid = entity != wi::ecs::INVALID_ENTITY;
        HashValue(hash, valid);
        if (!valid)
            return;

        const auto* name = scene.names.GetComponent(entity);
        const bool hasName = name != nullptr;
        HashValue(hash, hasName);
        if (hasName)
            HashString(hash, name->name);

        const auto* transform = scene.transforms.GetComponent(entity);
        const bool hasTransform = transform != nullptr;
        HashValue(hash, hasTransform);
        if (hasTransform)
        {
            HashValue(hash, transform->scale_local.x);
            HashValue(hash, transform->scale_local.y);
            HashValue(hash, transform->scale_local.z);
            HashValue(hash, transform->rotation_local.x);
            HashValue(hash, transform->rotation_local.y);
            HashValue(hash, transform->rotation_local.z);
            HashValue(hash, transform->rotation_local.w);
            HashValue(hash, transform->translation_local.x);
            HashValue(hash, transform->translation_local.y);
            HashValue(hash, transform->translation_local.z);
        }

        const auto* hierarchy = scene.hierarchy.GetComponent(entity);
        const bool hasParent = hierarchy != nullptr &&
            hierarchy->parentID != wi::ecs::INVALID_ENTITY && depth < 256;
        HashValue(hash, hasParent);
        if (hasParent)
        {
            HashEntitySemanticIdentity(
                hash, scene, hierarchy->parentID, depth + 1);
        }
    }

    void HashAnimationDataSemanticIdentity(
        std::uint64_t& hash,
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        const auto* data = scene.animation_datas.GetComponent(entity);
        const bool valid = data != nullptr;
        HashValue(hash, valid);
        if (!valid)
            return;
        HashValue(hash, data->keyframe_times.size());
        for (const float value : data->keyframe_times)
            HashValue(hash, value);
        HashValue(hash, data->keyframe_data.size());
        for (const float value : data->keyframe_data)
            HashValue(hash, value);
    }

    void HashArmatureSemanticIdentity(
        std::uint64_t& hash,
        const wi::scene::Scene& scene,
        const wi::ecs::Entity entity)
    {
        const auto* armature = scene.armatures.GetComponent(entity);
        const bool valid = armature != nullptr;
        HashValue(hash, valid);
        if (!valid)
            return;
        HashValue(hash, armature->boneCollection.size());
        for (const auto bone : armature->boneCollection)
            HashEntitySemanticIdentity(hash, scene, bone);
        HashValue(hash, armature->inverseBindMatrices.size());
        for (const auto& matrix : armature->inverseBindMatrices)
            HashMatrix(hash, matrix);
    }

    void HashSortedBlocks(
        std::uint64_t& hash,
        std::vector<std::uint64_t>& blocks)
    {
        std::sort(blocks.begin(), blocks.end());
        HashValue(hash, blocks.size());
        for (const auto block : blocks)
            HashValue(hash, block);
    }

    // Wicked's pinned MeshComponent::CreateRenderData() normalizes 4/8 skin
    // weights IN PLACE when constructing its GPU buffers, including after a
    // WISCENE reload. A second normalization can change individual float bits.
    // Bring the isolated creator scene to that exact finite fixed point before
    // taking the authoritative evidence snapshot. We NEVER apply tolerance to
    // the round-trip proof, and we fail closed if a fixed point cannot be found.
    bool CanonicalizeWisceneSkinWeights(wi::scene::Scene& scene, std::string& error)
    {
        constexpr unsigned MaximumNormalizationPasses = 32;
        for (std::size_t meshIndex = 0; meshIndex < scene.meshes.GetCount(); ++meshIndex)
        {
            auto& mesh = scene.meshes[meshIndex];
            const auto count = mesh.vertex_boneindices.size();
            if (mesh.vertex_boneweights.size() != count ||
                mesh.vertex_boneindices2.size() != mesh.vertex_boneweights2.size() ||
                (!mesh.vertex_boneindices2.empty() &&
                    mesh.vertex_boneindices2.size() != count))
            {
                error = "Skin-weight stream lengths are inconsistent in mesh " +
                    std::to_string(meshIndex) + ".";
                return false;
            }
            if (count == 0)
                continue;

            const bool hasSecondary = !mesh.vertex_boneindices2.empty();
            bool meshChanged = false;
            for (std::size_t vertex = 0; vertex < count; ++vertex)
            {
                bool stable = false;
                for (unsigned pass = 0; pass < MaximumNormalizationPasses; ++pass)
                {
                    const XMFLOAT4 before = mesh.vertex_boneweights[vertex];
                    const XMFLOAT4 before2 = hasSecondary
                        ? mesh.vertex_boneweights2[vertex] : XMFLOAT4{};
                    // Match the pinned Wicked order: four weights followed by
                    // optional secondary four, with unused weights set to 0.
                    float weights[8] = {
                        before.x, before.y, before.z, before.w,
                        before2.x, before2.y, before2.z, before2.w
                    };
                    float sum = 0.0f;
                    for (const float weight : weights)
                    {
                        if (!std::isfinite(weight) || weight < 0.0f)
                        {
                            error = "Non-finite or negative skin weight in mesh " +
                                std::to_string(meshIndex) + ", vertex " +
                                std::to_string(vertex) + ".";
                            return false;
                        }
                        sum += weight;
                    }
                    if (!std::isfinite(sum))
                    {
                        error = "Non-finite skin-weight sum in mesh " +
                            std::to_string(meshIndex) + ".";
                        return false;
                    }
                    if (sum > 0.0f)
                    {
                        const float norm = 1.0f / sum;
                        for (float& weight : weights)
                            weight *= norm;
                    }
                    for (const float weight : weights)
                    {
                        if (!std::isfinite(weight))
                        {
                            error = "Skin-weight normalization overflow in mesh " +
                                std::to_string(meshIndex) + ".";
                            return false;
                        }
                    }
                    const XMFLOAT4 next(weights[0], weights[1], weights[2], weights[3]);
                    const XMFLOAT4 next2(weights[4], weights[5], weights[6], weights[7]);
                    const bool unchanged =
                        std::memcmp(&before, &next, sizeof(XMFLOAT4)) == 0 &&
                        (!hasSecondary ||
                            std::memcmp(&before2, &next2, sizeof(XMFLOAT4)) == 0);
                    if (unchanged)
                    {
                        stable = true;
                        break;
                    }
                    mesh.vertex_boneweights[vertex] = next;
                    if (hasSecondary)
                        mesh.vertex_boneweights2[vertex] = next2;
                    meshChanged = true;
                }
                if (!stable)
                {
                    error = "Skin weights did not reach an exact Wicked normalization "
                        "fixed point in mesh " + std::to_string(meshIndex) +
                        ", vertex " + std::to_string(vertex) + ".";
                    return false;
                }
            }

            if (meshChanged)
            {
                // GPU skin buffers must use the same weights as the WISCENE.
                // This pinned Wicked call re-normalizes, so verify it cannot
                // silently change the fixed-point CPU weights again.
                const auto expected = mesh.vertex_boneweights;
                const auto expected2 = mesh.vertex_boneweights2;
                mesh.CreateRenderData();
                const bool firstMatches =
                    mesh.vertex_boneweights.size() == expected.size() &&
                    (expected.empty() || std::memcmp(
                        mesh.vertex_boneweights.data(), expected.data(),
                        expected.size() * sizeof(XMFLOAT4)) == 0);
                const bool secondMatches =
                    mesh.vertex_boneweights2.size() == expected2.size() &&
                    (expected2.empty() || std::memcmp(
                        mesh.vertex_boneweights2.data(), expected2.data(),
                        expected2.size() * sizeof(XMFLOAT4)) == 0);
                if (!firstMatches || !secondMatches)
                {
                    error = "Wicked render-data creation changed canonical skin "
                        "weights in mesh " + std::to_string(meshIndex) + ".";
                    return false;
                }
            }
        }
        error.clear();
        return true;
    }

    bool FingerprintFile(
        const std::string& path,
        std::uint64_t& bytes,
        std::uint64_t& fingerprint,
        std::string& error)
    {
        std::ifstream input(fs::u8path(path), std::ios::binary);
        if (!input)
        {
            error = "Could not open model source for integrity proof: " + path;
            return false;
        }

        fingerprint = FingerprintSeed;
        bytes = 0;
        char buffer[64 * 1024];
        while (input)
        {
            input.read(buffer, sizeof(buffer));
            const auto count = input.gcount();
            if (count > 0)
            {
                HashBytes(
                    fingerprint,
                    buffer,
                    static_cast<std::size_t>(count));
                bytes += static_cast<std::uint64_t>(count);
            }
        }
        if (!input.eof())
        {
            error = "Could not read model source for integrity proof: " + path;
            return false;
        }
        return true;
    }

    bool ReloadEvidence(
        const std::string& path,
        renegade::bridge::ImportedModelEvidence& evidence,
        std::string& error)
    {
        auto scene = wi::allocator::make_shared_single<wi::scene::Scene>();
        wi::Archive archive(path, true, false);
        if (!archive.IsOpen())
        {
            error = "Could not reopen the imported WISCENE asset for rig/animation proof: " + path;
            return false;
        }

        scene->Serialize(archive);
        if (archive.GetPos() != archive.GetSize())
        {
            error = "Imported WISCENE rig/animation proof found trailing or incomplete data: " + path;
            return false;
        }

        evidence = renegade::bridge::ImportService::SummarizeModelEvidence(*scene);
        return true;
    }

    std::string LowerExtension(const std::string& sourcePath)
    {
        std::string extension = fs::u8path(sourcePath).extension().u8string();
        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
        return extension;
    }

    std::string DescribeRigAnimationEvidenceDifference(
        const renegade::bridge::ImportedModelEvidence& before,
        const renegade::bridge::ImportedModelEvidence& after)
    {
        std::ostringstream out;
        bool changed = false;
        const auto report = [&](const char* field, std::uint64_t left, std::uint64_t right)
        {
            if (left == right)
                return;
            if (changed)
                out << "; ";
            changed = true;
            out << field << " (0x" << std::hex << left << " -> 0x" << right
                << std::dec << ')';
        };
        report("mesh", before.meshFingerprint, after.meshFingerprint);
        report("skinIndex", before.skinIndexFingerprint, after.skinIndexFingerprint);
        report("skinWeight", before.skinWeightFingerprint, after.skinWeightFingerprint);
        report("armature", before.armatureFingerprint, after.armatureFingerprint);
        report("boneHierarchy", before.boneHierarchyFingerprint, after.boneHierarchyFingerprint);
        report("inverseBind", before.inverseBindFingerprint, after.inverseBindFingerprint);
        report("animation", before.animationFingerprint, after.animationFingerprint);
        report("animationChannel", before.animationChannelFingerprint, after.animationChannelFingerprint);
        report("animationSampler", before.animationSamplerFingerprint, after.animationSamplerFingerprint);
        report("animationData", before.animationDataFingerprint, after.animationDataFingerprint);
        report("animationTimes", before.animationTimesFingerprint, after.animationTimesFingerprint);
        report("animationValues", before.animationValuesFingerprint, after.animationValuesFingerprint);
        if (!changed)
            return "No diagnostic subgroup changed; investigate aggregate ordering or count drift.";
        return out.str();
    }

    std::string DescribeRigAnimationEvidence(
        const renegade::bridge::ImportedModelEvidence& evidence)
    {
        std::ostringstream out;
        out << "skinnedMeshes=" << evidence.skinnedMeshes
            << ", primaryInfluenceVertices=" << evidence.primaryInfluenceVertices
            << ", secondaryInfluenceVertices=" << evidence.secondaryInfluenceVertices
            << ", bones=" << evidence.armatureBones
            << ", channels=" << evidence.animationChannels
            << ", samplers=" << evidence.animationSamplers
            << ", animationData=" << evidence.animationData
            << ", keyframes=" << evidence.animationKeyframes
            << ", values=" << evidence.animationValues
            << ", fingerprint=0x" << std::hex
            << evidence.rigAnimationFingerprint << std::dec;
        return out.str();
    }
}

namespace renegade::bridge
{
    ModelSourceFormat ImportService::ClassifyModelSourceFormat(
        const std::string& sourcePath) noexcept
    {
        try
        {
            const std::string extension = LowerExtension(sourcePath);
            if (extension == ".fbx") return ModelSourceFormat::Fbx;
            if (extension == ".gltf") return ModelSourceFormat::Gltf;
            if (extension == ".glb") return ModelSourceFormat::Glb;
            if (extension == ".obj") return ModelSourceFormat::Obj;
            if (extension == ".ply") return ModelSourceFormat::Ply;
            if (extension == ".vrm") return ModelSourceFormat::Vrm;
            if (extension == ".vrma") return ModelSourceFormat::Vrma;
        }
        catch (...)
        {
        }
        return ModelSourceFormat::Unknown;
    }

    bool ImportService::IsModelSourceFormatSupported(
        const ModelSourceFormat format) noexcept
    {
        return format == ModelSourceFormat::Fbx ||
            format == ModelSourceFormat::Gltf ||
            format == ModelSourceFormat::Glb;
    }

    const char* ImportService::ModelSourceFormatName(
        const ModelSourceFormat format) noexcept
    {
        switch (format)
        {
        case ModelSourceFormat::Fbx: return "FBX";
        case ModelSourceFormat::Gltf: return "glTF";
        case ModelSourceFormat::Glb: return "GLB";
        case ModelSourceFormat::Obj: return "OBJ";
        case ModelSourceFormat::Ply: return "PLY";
        case ModelSourceFormat::Vrm: return "VRM";
        case ModelSourceFormat::Vrma: return "VRMA";
        default: return "unknown";
        }
    }

    ImportedModelEvidence ImportService::SummarizeModelEvidence(
        const wi::scene::Scene& scene) noexcept
    {
        ImportedModelEvidence evidence;
        std::uint64_t fingerprint = FingerprintSeed;
        std::vector<std::uint64_t> meshBlocks;
        std::vector<std::uint64_t> armatureBlocks;
        std::vector<std::uint64_t> animationBlocks;
        std::vector<std::uint64_t> animationDataBlocks;
        std::vector<std::uint64_t> skinIndexBlocks, skinWeightBlocks;
        std::vector<std::uint64_t> boneHierarchyBlocks, inverseBindBlocks;
        std::vector<std::uint64_t> animationChannelBlocks, animationSamplerBlocks;
        std::vector<std::uint64_t> animationTimesBlocks, animationValuesBlocks;

        for (std::size_t index = 0; index < scene.meshes.GetCount(); ++index)
        {
            const auto& mesh = scene.meshes[index];
            std::uint64_t block = FingerprintSeed;
            std::uint64_t indexBlock = FingerprintSeed;
            std::uint64_t weightBlock = FingerprintSeed;
            HashArmatureSemanticIdentity(block, scene, mesh.armatureID);

            if (mesh.armatureID != wi::ecs::INVALID_ENTITY)
            {
                ++evidence.skinnedMeshes;
            }

            evidence.primaryInfluenceVertices += std::min(
                mesh.vertex_boneindices.size(),
                mesh.vertex_boneweights.size());
            evidence.secondaryInfluenceVertices += std::min(
                mesh.vertex_boneindices2.size(),
                mesh.vertex_boneweights2.size());

            HashValue(block, mesh.vertex_boneindices.size());
            HashValue(indexBlock, mesh.vertex_boneindices.size());
            for (const auto& value : mesh.vertex_boneindices)
            {
                HashUInt4(block, value);
                HashUInt4(indexBlock, value);
            }
            HashValue(block, mesh.vertex_boneweights.size());
            HashValue(weightBlock, mesh.vertex_boneweights.size());
            for (const auto& value : mesh.vertex_boneweights)
            {
                HashFloat4(block, value);
                HashFloat4(weightBlock, value);
            }
            HashValue(block, mesh.vertex_boneindices2.size());
            HashValue(indexBlock, mesh.vertex_boneindices2.size());
            for (const auto& value : mesh.vertex_boneindices2)
            {
                HashUInt4(block, value);
                HashUInt4(indexBlock, value);
            }
            HashValue(block, mesh.vertex_boneweights2.size());
            HashValue(weightBlock, mesh.vertex_boneweights2.size());
            for (const auto& value : mesh.vertex_boneweights2)
            {
                HashFloat4(block, value);
                HashFloat4(weightBlock, value);
            }
            meshBlocks.push_back(block);
            skinIndexBlocks.push_back(indexBlock);
            skinWeightBlocks.push_back(weightBlock);
        }
        evidence.meshFingerprint = FingerprintSeed;
        evidence.skinIndexFingerprint = FingerprintSeed;
        evidence.skinWeightFingerprint = FingerprintSeed;
        HashSortedBlocks(evidence.meshFingerprint, meshBlocks);
        HashSortedBlocks(evidence.skinIndexFingerprint, skinIndexBlocks);
        HashSortedBlocks(evidence.skinWeightFingerprint, skinWeightBlocks);
        HashSortedBlocks(fingerprint, meshBlocks);

        for (std::size_t index = 0; index < scene.armatures.GetCount(); ++index)
        {
            const auto& armature = scene.armatures[index];
            std::uint64_t block = FingerprintSeed;
            std::uint64_t boneBlock = FingerprintSeed;
            std::uint64_t bindBlock = FingerprintSeed;
            evidence.armatureBones += armature.boneCollection.size();
            HashValue(block, armature.boneCollection.size());
            HashValue(boneBlock, armature.boneCollection.size());
            for (const auto bone : armature.boneCollection)
            {
                HashEntitySemanticIdentity(block, scene, bone);
                HashEntitySemanticIdentity(boneBlock, scene, bone);
            }
            HashValue(block, armature.inverseBindMatrices.size());
            HashValue(bindBlock, armature.inverseBindMatrices.size());
            for (const auto& matrix : armature.inverseBindMatrices)
            {
                HashMatrix(block, matrix);
                HashMatrix(bindBlock, matrix);
            }
            armatureBlocks.push_back(block);
            boneHierarchyBlocks.push_back(boneBlock);
            inverseBindBlocks.push_back(bindBlock);
        }
        evidence.armatureFingerprint = FingerprintSeed;
        evidence.boneHierarchyFingerprint = FingerprintSeed;
        evidence.inverseBindFingerprint = FingerprintSeed;
        HashSortedBlocks(evidence.armatureFingerprint, armatureBlocks);
        HashSortedBlocks(evidence.boneHierarchyFingerprint, boneHierarchyBlocks);
        HashSortedBlocks(evidence.inverseBindFingerprint, inverseBindBlocks);
        HashSortedBlocks(fingerprint, armatureBlocks);

        for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
        {
            const auto& animation = scene.animations[index];
            std::uint64_t block = FingerprintSeed;
            std::uint64_t channelBlock = FingerprintSeed;
            std::uint64_t samplerBlock = FingerprintSeed;
            evidence.animationChannels += animation.channels.size();
            evidence.animationSamplers += animation.samplers.size();

            HashValue(block, animation.channels.size());
            HashValue(channelBlock, animation.channels.size());
            for (const auto& channel : animation.channels)
            {
                const auto path = static_cast<std::uint32_t>(channel.path);
                HashValue(block, path);
                HashValue(channelBlock, path);
                HashEntitySemanticIdentity(block, scene, channel.target);
                HashEntitySemanticIdentity(channelBlock, scene, channel.target);
                HashValue(block, channel.samplerIndex);
                HashValue(channelBlock, channel.samplerIndex);
                HashValue(block, channel.retargetIndex);
                HashValue(channelBlock, channel.retargetIndex);
            }

            HashValue(block, animation.samplers.size());
            HashValue(samplerBlock, animation.samplers.size());
            for (const auto& sampler : animation.samplers)
            {
                const auto mode = static_cast<std::uint32_t>(sampler.mode);
                HashValue(block, mode);
                HashValue(samplerBlock, mode);
                HashAnimationDataSemanticIdentity(block, scene, sampler.data);
                HashAnimationDataSemanticIdentity(samplerBlock, scene, sampler.data);
            }
            animationBlocks.push_back(block);
            animationChannelBlocks.push_back(channelBlock);
            animationSamplerBlocks.push_back(samplerBlock);
        }
        evidence.animationFingerprint = FingerprintSeed;
        evidence.animationChannelFingerprint = FingerprintSeed;
        evidence.animationSamplerFingerprint = FingerprintSeed;
        HashSortedBlocks(evidence.animationFingerprint, animationBlocks);
        HashSortedBlocks(evidence.animationChannelFingerprint, animationChannelBlocks);
        HashSortedBlocks(evidence.animationSamplerFingerprint, animationSamplerBlocks);
        HashSortedBlocks(fingerprint, animationBlocks);

        evidence.animationData = scene.animation_datas.GetCount();
        HashValue(fingerprint, evidence.animationData);
        for (std::size_t index = 0; index < scene.animation_datas.GetCount(); ++index)
        {
            const auto& data = scene.animation_datas[index];
            std::uint64_t block = FingerprintSeed;
            std::uint64_t timesBlock = FingerprintSeed;
            std::uint64_t valuesBlock = FingerprintSeed;
            evidence.animationKeyframes += data.keyframe_times.size();
            evidence.animationValues += data.keyframe_data.size();

            HashValue(block, data.keyframe_times.size());
            HashValue(timesBlock, data.keyframe_times.size());
            for (const auto value : data.keyframe_times)
            {
                HashValue(block, value);
                HashValue(timesBlock, value);
            }
            HashValue(block, data.keyframe_data.size());
            HashValue(valuesBlock, data.keyframe_data.size());
            for (const auto value : data.keyframe_data)
            {
                HashValue(block, value);
                HashValue(valuesBlock, value);
            }
            animationDataBlocks.push_back(block);
            animationTimesBlocks.push_back(timesBlock);
            animationValuesBlocks.push_back(valuesBlock);
        }
        evidence.animationDataFingerprint = FingerprintSeed;
        evidence.animationTimesFingerprint = FingerprintSeed;
        evidence.animationValuesFingerprint = FingerprintSeed;
        HashSortedBlocks(evidence.animationDataFingerprint, animationDataBlocks);
        HashSortedBlocks(evidence.animationTimesFingerprint, animationTimesBlocks);
        HashSortedBlocks(evidence.animationValuesFingerprint, animationValuesBlocks);
        HashSortedBlocks(fingerprint, animationDataBlocks);

        evidence.rigAnimationFingerprint = fingerprint;
        return evidence;
    }

    PreparedModelImport ImportService::PrepareModelAsset(
        const ModelImportRequest& request) const
    {
        PreparedModelImport prepared;
        ImportResult& result = prepared.result_;
        result.sourcePath = request.sourcePath;
        result.assetPath = request.assetPath;

        if (request.sourcePath.empty() || request.assetPath.empty())
        {
            result.error = "Model import requires both a source path and a WISCENE asset path.";
            return prepared;
        }

        result.sourceFormat = ClassifyModelSourceFormat(request.sourcePath);
        if (result.sourceFormat == ModelSourceFormat::Unknown)
        {
            result.error = "Unsupported model source extension: " + request.sourcePath;
            return prepared;
        }

        if (request.expectedFormat != ModelSourceFormat::Unknown &&
            request.expectedFormat != result.sourceFormat)
        {
            result.error = std::string("Model source format mismatch: expected ") +
                ModelSourceFormatName(request.expectedFormat) + " but path resolves to " +
                ModelSourceFormatName(result.sourceFormat) + ".";
            return prepared;
        }

        if (!IsModelSourceFormatSupported(result.sourceFormat))
        {
            result.error = std::string(ModelSourceFormatName(result.sourceFormat)) +
                " is classified but not enabled by LP07 Gate 1.";
            return prepared;
        }

        if (LowerExtension(request.assetPath) != ".wiscene")
        {
            result.error = "Model import destination must use the .wiscene extension.";
            return prepared;
        }

        std::error_code ec;
        const fs::path source = fs::u8path(request.sourcePath);
        if (!fs::is_regular_file(source, ec) || ec)
        {
            result.error = "Model source file does not exist: " + request.sourcePath;
            return prepared;
        }

        if (wi::graphics::GetDevice() == nullptr)
        {
            result.error = "Model conversion requires an initialized Wicked graphics device.";
            return prepared;
        }

        if (!FingerprintFile(
                request.sourcePath,
                result.sourceBytes,
                result.sourceFingerprint,
                result.error))
        {
            return prepared;
        }

        const fs::path destination = fs::u8path(request.assetPath);
        if (!destination.parent_path().empty())
        {
            fs::create_directories(destination.parent_path(), ec);
            if (ec)
            {
                result.error = "Could not create model import destination folder: " +
                    destination.parent_path().u8string();
                return prepared;
            }
        }

        prepared.scene_ = wi::allocator::make_shared_single<wi::scene::Scene>();
        try
        {
            switch (result.sourceFormat)
            {
            case ModelSourceFormat::Fbx:
                result.importerBackend = "wicked.ufbx";
                ImportModel_FBX(request.sourcePath, *prepared.scene_);
                break;
            case ModelSourceFormat::Gltf:
            case ModelSourceFormat::Glb:
                result.importerBackend = "wicked.gltf";
                ImportModel_GLTF(request.sourcePath, *prepared.scene_);
                break;
            default:
                result.error = "Internal model import dispatch error.";
                prepared.scene_.reset();
                return prepared;
            }
        }
        catch (const std::exception& ex)
        {
            result.error = std::string("Wicked model conversion failed: ") + ex.what();
            prepared.scene_.reset();
            return prepared;
        }
        catch (...)
        {
            result.error = "Wicked model conversion failed with an unknown error.";
            prepared.scene_.reset();
            return prepared;
        }

        std::uint64_t afterBytes = 0;
        std::uint64_t afterFingerprint = 0;
        std::string integrityError;
        if (!FingerprintFile(
                request.sourcePath,
                afterBytes,
                afterFingerprint,
                integrityError))
        {
            result.error = integrityError;
            prepared.scene_.reset();
            return prepared;
        }
        if (afterBytes != result.sourceBytes ||
            afterFingerprint != result.sourceFingerprint)
        {
            result.error = "Model importer modified the source file; import aborted.";
            prepared.scene_.reset();
            return prepared;
        }

        result.imported = Summarize(*prepared.scene_);
        result.importedEvidence = SummarizeModelEvidence(*prepared.scene_);
        if (result.imported.meshes == 0 || result.imported.objects == 0)
        {
            result.error = "Model conversion produced no reusable mesh/object content.";
            prepared.scene_.reset();
            return prepared;
        }

        return prepared;
    }

    bool ImportService::RefreshPreparedModelEvidence(
        PreparedModelImport& prepared,
        std::string& error) const
    {
        if (!prepared.IsReady() || prepared.scene_ == nullptr)
        {
            error = "Prepared model import is not ready for creator recipe edits.";
            return false;
        }
        prepared.result_.imported = Summarize(*prepared.scene_);
        prepared.result_.importedEvidence = SummarizeModelEvidence(*prepared.scene_);
        if (prepared.result_.imported.meshes == 0 ||
            prepared.result_.imported.objects == 0)
        {
            error = "Creator recipe removed all reusable mesh/object content.";
            return false;
        }
        error.clear();
        return true;
    }

    bool ImportService::RetargetPreparedModelAsset(
        PreparedModelImport& prepared,
        const ModelImportRequest& request,
        std::string& error) const
    {
        if (!prepared.IsReady() || prepared.scene_ == nullptr)
        {
            error = "Prepared model import is not ready to retarget.";
            return false;
        }
        if (request.sourcePath.empty() || request.assetPath.empty())
        {
            error = "Prepared model retarget requires source and destination paths.";
            return false;
        }

        const ModelSourceFormat format = ClassifyModelSourceFormat(request.sourcePath);
        if (!IsModelSourceFormatSupported(format) ||
            format != prepared.result_.sourceFormat ||
            (request.expectedFormat != ModelSourceFormat::Unknown &&
                request.expectedFormat != format))
        {
            error = "Prepared model retarget source format does not match the converted preview.";
            return false;
        }
        if (LowerExtension(request.assetPath) != ".wiscene")
        {
            error = "Prepared model retarget destination must use the .wiscene extension.";
            return false;
        }

        std::uint64_t sourceBytes = 0;
        std::uint64_t sourceFingerprint = 0;
        if (!FingerprintFile(
                request.sourcePath,
                sourceBytes,
                sourceFingerprint,
                error))
        {
            return false;
        }
        if (sourceBytes != prepared.result_.sourceBytes ||
            sourceFingerprint != prepared.result_.sourceFingerprint)
        {
            error =
                "Project-retained model source differs from the source used to create the importer preview.";
            return false;
        }

        std::error_code ec;
        const fs::path destination = fs::u8path(request.assetPath);
        if (!destination.parent_path().empty())
        {
            fs::create_directories(destination.parent_path(), ec);
            if (ec)
            {
                error = "Could not create prepared model destination folder: " +
                    destination.parent_path().u8string();
                return false;
            }
        }

        prepared.result_.sourcePath = request.sourcePath;
        prepared.result_.assetPath = request.assetPath;
        prepared.result_.error.clear();
        error.clear();
        return true;
    }

    ImportResult ImportService::SavePreparedModelAsset(
        PreparedModelImport& prepared) const
    {
        if (!prepared.IsReady())
        {
            ImportResult result = prepared.Result();
            if (result.error.empty())
            {
                result.error = "Prepared model import is not ready to save.";
            }
            return result;
        }

        // Canonicalize only the isolated model's CPU/GPU skin weights. The
        // source file stays untouched; all rig/animation equality tests remain
        // exact, including the original aggregate fingerprint.
        std::string normalizationError;
        if (!CanonicalizeWisceneSkinWeights(*prepared.scene_, normalizationError))
        {
            ImportResult failed = prepared.Result();
            failed.error = "WISCENE skin-weight canonicalization failed: " +
                normalizationError;
            prepared.result_ = failed;
            return failed;
        }
        prepared.result_.imported = Summarize(*prepared.scene_);
        prepared.result_.importedEvidence = SummarizeModelEvidence(*prepared.scene_);

        // Reuse the proven WISCENE serializer/structural round-trip path. Its
        // name is retained for compatibility, but it serializes a Wicked Scene
        // and is therefore format-neutral after conversion.
        ImportResult result = SavePreparedGltfAsset(prepared);
        if (!result.succeeded)
        {
            return result;
        }

        result.importedEvidence = prepared.result_.importedEvidence;
        // Distinguish a mutation of the live prepared Scene DURING Wicked's
        // write from a change introduced by serialized data or reload.
        // Neither outcome is accepted; the authoritative transaction remains
        // gated by the same strict round-trip proof.
        const auto afterWriteInMemory = SummarizeModelEvidence(*prepared.scene_);
        if (!(result.importedEvidence == afterWriteInMemory))
        {
            result.succeeded = false;
            result.error =
                "Prepared WISCENE rig/animation evidence changed in memory during save. Changed groups: " +
                DescribeRigAnimationEvidenceDifference(result.importedEvidence, afterWriteInMemory) +
                ". Before: " + DescribeRigAnimationEvidence(result.importedEvidence) +
                ". After: " + DescribeRigAnimationEvidence(afterWriteInMemory) + ".";
            prepared.result_ = result;
            return result;
        }
        if (!ReloadEvidence(
                result.assetPath,
                result.reloadedEvidence,
                result.error))
        {
            result.succeeded = false;
            prepared.result_ = result;
            return result;
        }
        const bool importedHasRigAnimation =
            result.importedEvidence.HasRigOrAnimationPayload();
        const bool reloadedHasRigAnimation =
            result.reloadedEvidence.HasRigOrAnimationPayload();
        if (importedHasRigAnimation != reloadedHasRigAnimation ||
            (importedHasRigAnimation &&
                !(result.importedEvidence == result.reloadedEvidence)))
        {
            result.succeeded = false;
            result.error =
                "Imported WISCENE rig/animation evidence changed after round-trip reload. Changed groups: " +
                DescribeRigAnimationEvidenceDifference(result.importedEvidence, result.reloadedEvidence) +
                ". Before: " + DescribeRigAnimationEvidence(result.importedEvidence) +
                ". After: " +
                DescribeRigAnimationEvidence(result.reloadedEvidence) + ".";
            prepared.result_ = result;
            return result;
        }

        result.sourceFormat = prepared.result_.sourceFormat;
        result.importerBackend = prepared.result_.importerBackend;
        result.sourceBytes = prepared.result_.sourceBytes;
        result.sourceFingerprint = prepared.result_.sourceFingerprint;
        prepared.result_ = result;
        return result;
    }

    ImportResult ImportService::CompleteModelAsset(
        PreparedModelImport prepared) const
    {
        ImportResult result = SavePreparedModelAsset(prepared);
        if (result.succeeded)
        {
            (void)prepared.ReleaseScene();
        }
        return result;
    }
}
