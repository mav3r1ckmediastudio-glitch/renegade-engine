#include "renegade/bridge/ImportService.h"

#include <ModelImporter.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

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

    template<typename Component>
    std::size_t StableIndex(
        const wi::ecs::ComponentManager<Component>& manager,
        const wi::ecs::Entity entity) noexcept
    {
        return entity == wi::ecs::INVALID_ENTITY
            ? wi::ecs::INVALID_INDEX
            : manager.GetIndex(entity);
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

        for (std::size_t index = 0; index < scene.meshes.GetCount(); ++index)
        {
            const auto& mesh = scene.meshes[index];
            const auto armatureIndex = StableIndex(scene.armatures, mesh.armatureID);
            HashValue(fingerprint, armatureIndex);

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

            HashValue(fingerprint, mesh.vertex_boneindices.size());
            for (const auto& value : mesh.vertex_boneindices)
            {
                HashUInt4(fingerprint, value);
            }
            HashValue(fingerprint, mesh.vertex_boneweights.size());
            for (const auto& value : mesh.vertex_boneweights)
            {
                HashFloat4(fingerprint, value);
            }
            HashValue(fingerprint, mesh.vertex_boneindices2.size());
            for (const auto& value : mesh.vertex_boneindices2)
            {
                HashUInt4(fingerprint, value);
            }
            HashValue(fingerprint, mesh.vertex_boneweights2.size());
            for (const auto& value : mesh.vertex_boneweights2)
            {
                HashFloat4(fingerprint, value);
            }
        }

        for (std::size_t index = 0; index < scene.armatures.GetCount(); ++index)
        {
            const auto& armature = scene.armatures[index];
            evidence.armatureBones += armature.boneCollection.size();
            HashValue(fingerprint, armature.boneCollection.size());
            for (const auto bone : armature.boneCollection)
            {
                const auto transformIndex = StableIndex(scene.transforms, bone);
                HashValue(fingerprint, transformIndex);
            }
            HashValue(fingerprint, armature.inverseBindMatrices.size());
            for (const auto& matrix : armature.inverseBindMatrices)
            {
                HashMatrix(fingerprint, matrix);
            }
        }

        for (std::size_t index = 0; index < scene.animations.GetCount(); ++index)
        {
            const auto& animation = scene.animations[index];
            evidence.animationChannels += animation.channels.size();
            evidence.animationSamplers += animation.samplers.size();

            HashValue(fingerprint, animation.channels.size());
            for (const auto& channel : animation.channels)
            {
                const auto path = static_cast<std::uint32_t>(channel.path);
                const auto targetIndex = StableIndex(scene.transforms, channel.target);
                HashValue(fingerprint, path);
                HashValue(fingerprint, targetIndex);
                HashValue(fingerprint, channel.samplerIndex);
                HashValue(fingerprint, channel.retargetIndex);
            }

            HashValue(fingerprint, animation.samplers.size());
            for (const auto& sampler : animation.samplers)
            {
                const auto mode = static_cast<std::uint32_t>(sampler.mode);
                const auto dataIndex = StableIndex(scene.animation_datas, sampler.data);
                HashValue(fingerprint, mode);
                HashValue(fingerprint, dataIndex);
            }
        }

        evidence.animationData = scene.animation_datas.GetCount();
        HashValue(fingerprint, evidence.animationData);
        for (std::size_t index = 0; index < scene.animation_datas.GetCount(); ++index)
        {
            const auto& data = scene.animation_datas[index];
            evidence.animationKeyframes += data.keyframe_times.size();
            evidence.animationValues += data.keyframe_data.size();

            HashValue(fingerprint, data.keyframe_times.size());
            for (const auto value : data.keyframe_times)
            {
                HashValue(fingerprint, value);
            }
            HashValue(fingerprint, data.keyframe_data.size());
            for (const auto value : data.keyframe_data)
            {
                HashValue(fingerprint, value);
            }
        }

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

        // Reuse the proven WISCENE serializer/structural round-trip path. Its
        // name is retained for compatibility, but it serializes a Wicked Scene
        // and is therefore format-neutral after conversion.
        ImportResult result = SavePreparedGltfAsset(prepared);
        if (!result.succeeded)
        {
            return result;
        }

        result.importedEvidence = prepared.result_.importedEvidence;
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
                "Imported WISCENE rig/animation evidence changed after round-trip reload. Before: " +
                DescribeRigAnimationEvidence(result.importedEvidence) +
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
