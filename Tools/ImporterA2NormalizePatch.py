#!/usr/bin/env python3
"""One-shot GitHub Actions patch for A2, with exact source anchors."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]

def patch(relative, old, new):
    path = root / relative
    data = path.read_text(encoding='utf-8')
    n = data.count(old)
    if n != 1:
        raise SystemExit(f'{relative}: expected exactly one anchor; found {n}: {old[:85]!r}')
    path.write_text(data.replace(old, new, 1), encoding='utf-8', newline='')

source = 'EngineBridge/src/ImportServiceModel.cpp'
patch(source, '#include <algorithm>\n#include <cctype>\n', '#include <algorithm>\n#include <cctype>\n#include <cmath>\n#include <cstring>\n')
patch(source, '    bool FingerprintFile(\n', '''    // Wicked's pinned MeshComponent::CreateRenderData() normalizes 4/8 skin
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
''')
patch(source, '''        // Reuse the proven WISCENE serializer/structural round-trip path. Its
        // name is retained for compatibility, but it serializes a Wicked Scene
        // and is therefore format-neutral after conversion.
        ImportResult result = SavePreparedGltfAsset(prepared);
''', '''        // Canonicalize only the isolated model's CPU/GPU skin weights. The
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
''')

test = 'Tests/ReusableAssetGraphicsProof.cpp'
patch(test, '#include <algorithm>\n#include <filesystem>\n', '#include <algorithm>\n#include <cstring>\n#include <filesystem>\n')
patch(test, '''    bool RunExistingProductPreservationProof(const fs::path& projectRoot)
''', '''    // Public animated FBX fixture: prove the original raw WISCENE path
    // really changes 0.2 + 0.2 + 0.2 + 0.2 to 0.25 x4 ON RELOAD, then
    // prove the model path canonicalizes and survives two independent reloads.
    bool RunSkinWeightNormalizationProof(const fs::path& projectRoot)
    {
        using namespace renegade::bridge;
        ImportService imports;
        const fs::path source = projectRoot / "SourceAssets/Models/animated.fbx";
        const auto makePrepared = [&](const char* leaf)
        {
            ModelImportRequest request;
            request.sourcePath = source.generic_u8string();
            request.assetPath = (projectRoot / "Intermediate" / leaf).generic_u8string();
            request.expectedFormat = ModelSourceFormat::Fbx;
            return imports.PrepareModelAsset(request);
        };
        const auto setWeights = [&](PreparedModelImport& prepared, std::size_t& meshIndex)
        {
            auto* scene = prepared.PeekMutableScene();
            if (scene == nullptr)
                return false;
            for (meshIndex = 0; meshIndex < scene->meshes.GetCount(); ++meshIndex)
            {
                auto& mesh = scene->meshes[meshIndex];
                if (mesh.vertex_boneindices.empty() || mesh.vertex_boneweights.empty())
                    continue;
                mesh.vertex_boneweights[0] = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.2f);
                if (!mesh.vertex_boneweights2.empty())
                    mesh.vertex_boneweights2[0] = XMFLOAT4(0, 0, 0, 0);
                return true;
            }
            return false;
        };
        const auto readScene = [&](const fs::path& path,
            wi::scene::Scene& scene, std::string& error)
        {
            wi::Archive archive(path.generic_u8string(), true, false);
            if (!archive.IsOpen())
            {
                error = "could not open normalization proof WISCENE";
                return false;
            }
            scene.Serialize(archive);
            if (archive.GetPos() != archive.GetSize())
            {
                error = "normalization proof WISCENE had trailing/incomplete data";
                return false;
            }
            return true;
        };
        std::string error;
        auto raw = makePrepared("a2-normalization-raw.wiscene");
        std::size_t meshIndex = 0;
        if (!Require(raw.IsReady(), "weight proof could not prepare public FBX: " +
                raw.Result().error) ||
            !Require(setWeights(raw, meshIndex), "public FBX had no skinned mesh") ||
            !Require(imports.RefreshPreparedModelEvidence(raw, error),
                "weight proof could not refresh raw evidence: " + error))
            return false;
        const auto original = raw.Result().importedEvidence;
        const auto rawSave = imports.SavePreparedGltfAsset(raw);
        if (!Require(rawSave.succeeded,
                "original structural WISCENE proof failed: " + rawSave.error) ||
            !Require(original == ImportService::SummarizeModelEvidence(*raw.PeekScene()),
                "raw prepared skin weights changed during WISCENE write"))
            return false;
        auto rawReload = wi::allocator::make_shared_single<wi::scene::Scene>();
        if (!Require(readScene(fs::u8path(rawSave.assetPath), *rawReload, error),
                "raw WISCENE reload failed: " + error) ||
            !Require(meshIndex < rawReload->meshes.GetCount(),
                "raw WISCENE lost its skinned mesh"))
            return false;
        const auto& reloadedMesh = rawReload->meshes[meshIndex];
        if (!Require(!reloadedMesh.vertex_boneweights.empty() &&
                reloadedMesh.vertex_boneweights[0].x == 0.25f &&
                reloadedMesh.vertex_boneweights[0].y == 0.25f &&
                reloadedMesh.vertex_boneweights[0].z == 0.25f &&
                reloadedMesh.vertex_boneweights[0].w == 0.25f,
                "pinned Wicked reload did not normalize the controlled 0.8 sum to unity"))
            return false;
        const auto normalized = ImportService::SummarizeModelEvidence(*rawReload);
        if (!Require(original.skinWeightFingerprint != normalized.skinWeightFingerprint &&
                original.meshFingerprint != normalized.meshFingerprint &&
                original.skinIndexFingerprint == normalized.skinIndexFingerprint &&
                original.armatureFingerprint == normalized.armatureFingerprint &&
                original.animationFingerprint == normalized.animationFingerprint &&
                original.animationDataFingerprint == normalized.animationDataFingerprint,
                "weight proof changed unrelated rig/animation evidence"))
            return false;
        std::uint32_t originalBits = 0;
        std::uint32_t normalizedBits = 0;
        const float rawValue = 0.2f;
        const float fixedValue = reloadedMesh.vertex_boneweights[0].x;
        std::memcpy(&originalBits, &rawValue, sizeof(originalBits));
        std::memcpy(&normalizedBits, &fixedValue, sizeof(normalizedBits));
        std::cout << "A2 PINNED WICKED SKIN NORMALIZATION REPRO PASS"
            << " // first_vertex_weight_x_bits=0x" << std::hex << originalBits
            << " -> 0x" << normalizedBits << std::dec << '\\n';
        rawReload.reset();

        auto repaired = makePrepared("a2-normalization-fixed.wiscene");
        std::size_t repairedMeshIndex = 0;
        if (!Require(repaired.IsReady(), "repair proof could not prepare public FBX: " +
                repaired.Result().error) ||
            !Require(setWeights(repaired, repairedMeshIndex),
                "repair proof had no skinned mesh") ||
            !Require(imports.RefreshPreparedModelEvidence(repaired, error),
                "repair proof could not refresh raw evidence: " + error))
            return false;
        const auto saved = imports.SavePreparedModelAsset(repaired);
        if (!Require(saved.succeeded,
                "canonical WISCENE save failed: " + saved.error) ||
            !Require(saved.importedEvidence == saved.reloadedEvidence,
                "canonical rig/animation evidence did not match exactly"))
            return false;
        for (int pass = 0; pass < 2; ++pass)
        {
            auto reopened = wi::allocator::make_shared_single<wi::scene::Scene>();
            if (!Require(readScene(fs::u8path(saved.assetPath), *reopened, error),
                    "canonical WISCENE reopen failed: " + error) ||
                !Require(ImportService::SummarizeModelEvidence(*reopened) ==
                    saved.importedEvidence,
                    "canonical rig/animation weights drifted on repeated reload") ||
                !Require(repairedMeshIndex < reopened->meshes.GetCount() &&
                    !reopened->meshes[repairedMeshIndex].vertex_boneweights.empty() &&
                    reopened->meshes[repairedMeshIndex].vertex_boneweights[0].x == 0.25f,
                    "canonical first-vertex skin weight was not preserved"))
                return false;
        }
        std::cout << "A2 CANONICAL SKIN ROUND-TRIP PASS\\n";
        return true;
    }

    bool RunExistingProductPreservationProof(const fs::path& projectRoot)
''')
patch(test, '''            const bool rollbackPassed = staticPassed && animatedPassed && gltfPassed &&
                RunFailurePreservationProof(projectRoot);
            const bool lastGoodPassed = rollbackPassed &&
                RunExistingProductPreservationProof(projectRoot);
            if (!staticPassed || !animatedPassed || !gltfPassed ||
                !rollbackPassed || !lastGoodPassed)
''', '''            const bool weightProofPassed = animatedPassed &&
                RunSkinWeightNormalizationProof(projectRoot);
            const bool rollbackPassed = staticPassed && animatedPassed && gltfPassed &&
                weightProofPassed && RunFailurePreservationProof(projectRoot);
            const bool lastGoodPassed = rollbackPassed &&
                RunExistingProductPreservationProof(projectRoot);
            if (!staticPassed || !animatedPassed || !gltfPassed ||
                !weightProofPassed || !rollbackPassed || !lastGoodPassed)
''')
print('A2 patch anchors applied to exactly EngineBridge/src/ImportServiceModel.cpp and Tests/ReusableAssetGraphicsProof.cpp')
