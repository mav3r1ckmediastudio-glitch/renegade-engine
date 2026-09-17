#!/usr/bin/env python3
"""Temporary GitHub Actions-only patch helper. Removed by its own patch workflow."""
from pathlib import Path

MODEL = Path('EngineBridge/src/ImportServiceModel.cpp')
TEST = Path('Tests/ReusableAssetGraphicsProof.cpp')
model = MODEL.read_text(encoding='utf-8')
test = TEST.read_text(encoding='utf-8')

def replace_once(source: str, old: str, new: str, label: str) -> str:
    if source.count(old) != 1:
        raise RuntimeError(f'{label}: expected one matching anchor, found {source.count(old)}')
    return source.replace(old, new, 1)

start = model.index('    // Wicked\'s pinned MeshComponent::CreateRenderData() normalizes 4/8 skin')
end = model.index('    bool FingerprintFile(', start)
model = model[:start] + '''    // A WISCENE reload invokes Wicked's CreateRenderData() once per mesh. It
    // normalizes skin weights IN PLACE with a float sum, reciprocal and multiply.
    // A second pass can oscillate between float states: a bit-exact fixed point
    // is neither guaranteed nor required. Predict only that single documented
    // reload pass, without modifying the source, live preview or GPU buffers.
    struct ReloadSkinPrediction
    {
        std::size_t meshIndex = 0;
        std::vector<XMFLOAT4> originalPrimary;
        std::vector<XMFLOAT4> originalSecondary;
        std::vector<XMFLOAT4> expectedPrimary;
        std::vector<XMFLOAT4> expectedSecondary;
    };

    bool PredictWickedReloadEvidence(
        wi::scene::Scene& scene,
        const renegade::bridge::ImportedModelEvidence& originalEvidence,
        renegade::bridge::ImportedModelEvidence& expectedEvidence,
        std::string& error)
    {
        std::vector<ReloadSkinPrediction> predictions;
        for (std::size_t meshIndex = 0; meshIndex < scene.meshes.GetCount(); ++meshIndex)
        {
            const auto& mesh = scene.meshes[meshIndex];
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

            ReloadSkinPrediction prediction;
            prediction.meshIndex = meshIndex;
            prediction.originalPrimary.assign(
                mesh.vertex_boneweights.begin(), mesh.vertex_boneweights.end());
            prediction.originalSecondary.assign(
                mesh.vertex_boneweights2.begin(), mesh.vertex_boneweights2.end());
            prediction.expectedPrimary = prediction.originalPrimary;
            prediction.expectedSecondary = prediction.originalSecondary;
            const bool hasSecondary = !prediction.originalSecondary.empty();
            for (std::size_t vertex = 0; vertex < count; ++vertex)
            {
                const XMFLOAT4& primary = prediction.originalPrimary[vertex];
                const XMFLOAT4 secondary = hasSecondary
                    ? prediction.originalSecondary[vertex] : XMFLOAT4{};
                float weights[8] = {
                    primary.x, primary.y, primary.z, primary.w,
                    secondary.x, secondary.y, secondary.z, secondary.w
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
                    sum += weight; // identical order and float precision to pinned Wicked
                }
                if (!std::isfinite(sum))
                {
                    error = "Non-finite skin-weight sum in mesh " +
                        std::to_string(meshIndex) + ", vertex " +
                        std::to_string(vertex) + ".";
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
                            std::to_string(meshIndex) + ", vertex " +
                            std::to_string(vertex) + ".";
                        return false;
                    }
                }
                prediction.expectedPrimary[vertex] =
                    XMFLOAT4(weights[0], weights[1], weights[2], weights[3]);
                if (hasSecondary)
                    prediction.expectedSecondary[vertex] =
                        XMFLOAT4(weights[4], weights[5], weights[6], weights[7]);
            }
            predictions.push_back(std::move(prediction));
        }

        // Summarize the expected *loaded* payload with only CPU weights
        // substituted; immediately restore all original bits. No render-data
        // rebuild or repeated normalization of the live prepared scene.
        for (const auto& prediction : predictions)
        {
            auto& mesh = scene.meshes[prediction.meshIndex];
            std::copy(prediction.expectedPrimary.begin(),
                prediction.expectedPrimary.end(), mesh.vertex_boneweights.begin());
            std::copy(prediction.expectedSecondary.begin(),
                prediction.expectedSecondary.end(), mesh.vertex_boneweights2.begin());
        }
        expectedEvidence = renegade::bridge::ImportService::SummarizeModelEvidence(scene);
        for (const auto& prediction : predictions)
        {
            auto& mesh = scene.meshes[prediction.meshIndex];
            std::copy(prediction.originalPrimary.begin(),
                prediction.originalPrimary.end(), mesh.vertex_boneweights.begin());
            std::copy(prediction.originalSecondary.begin(),
                prediction.originalSecondary.end(), mesh.vertex_boneweights2.begin());
        }
        if (!(originalEvidence ==
              renegade::bridge::ImportService::SummarizeModelEvidence(scene)))
        {
            error = "Wicked reload prediction did not restore original in-memory rig/animation evidence.";
            return false;
        }
        error.clear();
        return true;
    }

''' + model[end:]
model = replace_once(model, '#include <vector>\n', '#include <vector>\n#include <utility>\n', 'utility include')
old = '''        // Canonicalize only the isolated model's CPU/GPU skin weights. The
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

'''
new = '''        // Do not repeatedly normalize the preview or require an impossible
        // float fixed point. The unmodified prepared scene is written first.
        const ImportedModelEvidence originalEvidence =
            SummarizeModelEvidence(*prepared.scene_);
        prepared.result_.importedEvidence = originalEvidence;

'''
model = replace_once(model, old, new, 'remove old canonicalization')
model = replace_once(model,
    '        result.importedEvidence = prepared.result_.importedEvidence;\n',
    '        result.importedEvidence = originalEvidence;\n',
    'original evidence snapshot')
old = '''        if (!ReloadEvidence(
                result.assetPath,
                result.reloadedEvidence,
                result.error))
'''
new = '''        // Predict exactly one pinned Wicked CreateRenderData() normalization
        // pass on reload. All other bytes still require strict evidence parity;
        // the prediction may differ from raw input only in skin weights.
        ImportedModelEvidence expectedReloadEvidence;
        std::string predictionError;
        if (!PredictWickedReloadEvidence(*prepared.scene_, originalEvidence,
                expectedReloadEvidence, predictionError))
        {
            result.succeeded = false;
            result.error = "WISCENE reload skin-weight prediction failed: " + predictionError;
            prepared.result_ = result;
            return result;
        }
        if (originalEvidence.skinIndexFingerprint != expectedReloadEvidence.skinIndexFingerprint ||
            originalEvidence.armatureFingerprint != expectedReloadEvidence.armatureFingerprint ||
            originalEvidence.animationFingerprint != expectedReloadEvidence.animationFingerprint ||
            originalEvidence.animationDataFingerprint != expectedReloadEvidence.animationDataFingerprint ||
            originalEvidence.primaryInfluenceVertices != expectedReloadEvidence.primaryInfluenceVertices ||
            originalEvidence.secondaryInfluenceVertices != expectedReloadEvidence.secondaryInfluenceVertices)
        {
            result.succeeded = false;
            result.error = "WISCENE reload prediction unexpectedly changed non-weight rig/animation evidence.";
            prepared.result_ = result;
            return result;
        }
        if (!ReloadEvidence(
                result.assetPath,
                result.reloadedEvidence,
                result.error))
'''
model = replace_once(model, old, new, 'insert single-pass expected reload proof')
old = '''        const bool importedHasRigAnimation =
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
'''
new = '''        const bool expectedHasRigAnimation =
            expectedReloadEvidence.HasRigOrAnimationPayload();
        const bool reloadedHasRigAnimation =
            result.reloadedEvidence.HasRigOrAnimationPayload();
        if (expectedHasRigAnimation != reloadedHasRigAnimation ||
            (expectedHasRigAnimation &&
                !(expectedReloadEvidence == result.reloadedEvidence)))
        {
            result.succeeded = false;
            result.error =
                "Imported WISCENE rig/animation evidence differs from exact single-pass Wicked reload prediction. Changed groups: " +
                DescribeRigAnimationEvidenceDifference(expectedReloadEvidence, result.reloadedEvidence) +
                ". Expected: " + DescribeRigAnimationEvidence(expectedReloadEvidence) +
                ". Actual: " +
                DescribeRigAnimationEvidence(result.reloadedEvidence) + ".";
            prepared.result_ = result;
            return result;
        }
        // The authoritative imported evidence describes the persisted payload,
        // not transient pre-reload weights. Reopening that same WISCENE twice
        // must reproduce these exact hashes (no fixed-point requirement).
        result.importedEvidence = expectedReloadEvidence;
        prepared.result_.importedEvidence = expectedReloadEvidence;

        result.sourceFormat = prepared.result_.sourceFormat;
'''
model = replace_once(model, old, new, 'replace raw equality with predicted equality')

old = '''    // Public animated FBX fixture: prove the original raw WISCENE path
    // really changes 0.2 + 0.2 + 0.2 + 0.2 to 0.25 x4 ON RELOAD, then
    // prove the model path canonicalizes and survives two independent reloads.
'''
new = '''    // Public FBX proof covers ordinary weight normalization AND a valid
    // floating-point two-cycle that the former 32-pass fixed-point repair rejected.
'''
test = replace_once(test, old, new, 'proof comment')
old = '''                if (mesh.vertex_boneindices.empty() || mesh.vertex_boneweights.empty())
                    continue;
                mesh.vertex_boneweights[0] = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.2f);
                if (!mesh.vertex_boneweights2.empty())
                    mesh.vertex_boneweights2[0] = XMFLOAT4(0, 0, 0, 0);
                return true;
'''
new = '''                if (mesh.vertex_boneindices.empty() || mesh.vertex_boneweights.size() <= 5)
                    continue;
                mesh.vertex_boneweights[0] = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.2f);
                // Exact IEEE-754 input bits; normalizing once/twice alternates
                // between two different weight vectors on pinned Wicked/MSVC.
                const std::uint32_t cycleBits[4] = {
                    0x3eec3653u, 0x3f12861du, 0x3eea5aa6u, 0x3f4e81e1u
                };
                float cycle[4] = {};
                for (int index = 0; index < 4; ++index)
                    std::memcpy(&cycle[index], &cycleBits[index], sizeof(float));
                mesh.vertex_boneweights[5] =
                    XMFLOAT4(cycle[0], cycle[1], cycle[2], cycle[3]);
                if (!mesh.vertex_boneweights2.empty())
                {
                    mesh.vertex_boneweights2[0] = XMFLOAT4(0, 0, 0, 0);
                    mesh.vertex_boneweights2[5] = XMFLOAT4(0, 0, 0, 0);
                }
                return true;
'''
test = replace_once(test, old, new, 'public fixture weight injection')
old = '''        std::string error;
        auto raw = makePrepared("a2-normalization-raw.wiscene");
'''
new = '''        // Confirm the regression is genuinely nonconvergent: the original
        // 32-pass repair would reject it even though all weights are finite.
        const std::uint32_t cycleBits[4] = {
            0x3eec3653u, 0x3f12861du, 0x3eea5aa6u, 0x3f4e81e1u
        };
        float cycle[4] = {};
        for (int index = 0; index < 4; ++index)
            std::memcpy(&cycle[index], &cycleBits[index], sizeof(float));
        const auto normalizeOnce = [](const XMFLOAT4& original)
        {
            float weights[8] = {
                original.x, original.y, original.z, original.w, 0, 0, 0, 0
            };
            float sum = 0.0f;
            for (const float weight : weights)
                sum += weight;
            if (sum > 0.0f)
            {
                const float factor = 1.0f / sum;
                for (float& weight : weights)
                    weight *= factor;
            }
            return XMFLOAT4(weights[0], weights[1], weights[2], weights[3]);
        };
        const XMFLOAT4 cycleStart(cycle[0], cycle[1], cycle[2], cycle[3]);
        const auto cycleFirst = normalizeOnce(cycleStart);
        const auto cycleSecond = normalizeOnce(cycleFirst);
        const auto cycleThird = normalizeOnce(cycleSecond);
        if (!Require(std::memcmp(&cycleFirst, &cycleSecond, sizeof(XMFLOAT4)) != 0 &&
                std::memcmp(&cycleFirst, &cycleThird, sizeof(XMFLOAT4)) == 0,
                "public cycle fixture must reproduce nonconvergent Wicked weights"))
            return false;
        std::string error;
        auto raw = makePrepared("a2-normalization-raw.wiscene");
'''
test = replace_once(test, old, new, 'nonconvergent fixture assertion')
old = '''        const auto saved = imports.SavePreparedModelAsset(repaired);
        if (!Require(saved.succeeded,
                "canonical WISCENE save failed: " + saved.error) ||
            !Require(saved.importedEvidence == saved.reloadedEvidence,
                "canonical rig/animation evidence did not match exactly"))
            return false;
'''
new = '''        const auto originalPreparedEvidence = repaired.Result().importedEvidence;
        const auto saved = imports.SavePreparedModelAsset(repaired);
        if (!Require(saved.succeeded,
                "single-pass WISCENE save failed: " + saved.error) ||
            !Require(originalPreparedEvidence ==
                ImportService::SummarizeModelEvidence(*repaired.PeekScene()),
                "reload prediction mutated original prepared weights") ||
            !Require(originalPreparedEvidence.skinWeightFingerprint !=
                saved.importedEvidence.skinWeightFingerprint &&
                originalPreparedEvidence.skinIndexFingerprint ==
                saved.importedEvidence.skinIndexFingerprint &&
                originalPreparedEvidence.armatureFingerprint ==
                saved.importedEvidence.armatureFingerprint &&
                originalPreparedEvidence.animationFingerprint ==
                saved.importedEvidence.animationFingerprint,
                "single-pass prediction changed unexpected rig or animation groups") ||
            !Require(saved.importedEvidence == saved.reloadedEvidence,
                "single-pass rig/animation evidence did not match exactly"))
            return false;
'''
test = replace_once(test, old, new, 'assert original unmodified and exact expected reload')
test = replace_once(test, '"canonical WISCENE reopen failed: "', '"single-pass WISCENE reopen failed: "', 'reopen failure description')
test = replace_once(test, '"canonical rig/animation weights drifted on repeated reload"', '"single-pass rig/animation evidence differed across independent reloads"', 'reopen evidence description')
test = replace_once(test, '"canonical first-vertex skin weight was not preserved"', '"normalized first-vertex skin weight was not preserved"', 'reopen first-vertex description')
test = replace_once(test, 'A2 CANONICAL SKIN ROUND-TRIP PASS', 'A2 SINGLE-PASS SKIN ROUND-TRIP AND NONCONVERGENT CYCLE PASS', 'proof success label')

assert 'CanonicalizeWisceneSkinWeights' not in model
assert 'PredictWickedReloadEvidence' in model
assert '0x3eec3653u' in test
assert model.count('result.importedEvidence = expectedReloadEvidence;') == 1
MODEL.write_text(model, encoding='utf-8')
TEST.write_text(test, encoding='utf-8')
print('A2 one-pass patch applied to exactly two source/test files')
