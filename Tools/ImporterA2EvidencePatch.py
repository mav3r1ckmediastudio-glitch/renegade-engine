#!/usr/bin/env python3
"""One-shot, exact-anchor A2 diagnostic patch. Run only on the isolated A2 branch."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def patch(path, before, after):
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    count = text.count(before)
    if count != 1:
        raise RuntimeError(f"{path}: expected one patch anchor, found {count}: {before[:95]!r}")
    target.write_text(text.replace(before, after, 1), encoding="utf-8")

HEADER = "EngineBridge/include/renegade/bridge/ImportService.h"
MODEL = "EngineBridge/src/ImportServiceModel.cpp"
TESTS = "Tests/ImportTests.cpp"

# Preserve the original aggregate evidence fingerprint. These independently
# hashed, sorted semantic subgroups are DIAGNOSTICS, not an acceptance bypass.
fields = ["mesh", "skinIndex", "skinWeight", "armature", "boneHierarchy",
          "inverseBind", "animation", "animationChannel", "animationSampler",
          "animationData", "animationTimes", "animationValues"]
patch(HEADER,
      "        std::uint64_t rigAnimationFingerprint = 0;",
      "".join(f"        std::uint64_t {field}Fingerprint = 0;\n" for field in fields)
      + "        std::uint64_t rigAnimationFingerprint = 0;")
patch(HEADER,
      "                rigAnimationFingerprint == other.rigAnimationFingerprint;",
      "".join(f"                {field}Fingerprint == other.{field}Fingerprint &&\n" for field in fields)
      + "                rigAnimationFingerprint == other.rigAnimationFingerprint;")

patch(MODEL,
      "        std::vector<std::uint64_t> animationDataBlocks;\n",
      "        std::vector<std::uint64_t> animationDataBlocks;\n"
      "        std::vector<std::uint64_t> skinIndexBlocks, skinWeightBlocks;\n"
      "        std::vector<std::uint64_t> boneHierarchyBlocks, inverseBindBlocks;\n"
      "        std::vector<std::uint64_t> animationChannelBlocks, animationSamplerBlocks;\n"
      "        std::vector<std::uint64_t> animationTimesBlocks, animationValuesBlocks;\n")

patch(MODEL,
      "            std::uint64_t block = FingerprintSeed;\n            HashArmatureSemanticIdentity(block, scene, mesh.armatureID);",
      "            std::uint64_t block = FingerprintSeed;\n"
      "            std::uint64_t indexBlock = FingerprintSeed;\n"
      "            std::uint64_t weightBlock = FingerprintSeed;\n"
      "            HashArmatureSemanticIdentity(block, scene, mesh.armatureID);")

patch(MODEL,
      """            HashValue(block, mesh.vertex_boneindices.size());
            for (const auto& value : mesh.vertex_boneindices)
            {
                HashUInt4(block, value);
            }
            HashValue(block, mesh.vertex_boneweights.size());
            for (const auto& value : mesh.vertex_boneweights)
            {
                HashFloat4(block, value);
            }
            HashValue(block, mesh.vertex_boneindices2.size());
            for (const auto& value : mesh.vertex_boneindices2)
            {
                HashUInt4(block, value);
            }
            HashValue(block, mesh.vertex_boneweights2.size());
            for (const auto& value : mesh.vertex_boneweights2)
            {
                HashFloat4(block, value);
            }
            meshBlocks.push_back(block);
        }
        HashSortedBlocks(fingerprint, meshBlocks);
""",
      """            HashValue(block, mesh.vertex_boneindices.size());
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
""")

patch(MODEL,
      """            const auto& armature = scene.armatures[index];
            std::uint64_t block = FingerprintSeed;
            evidence.armatureBones += armature.boneCollection.size();
            HashValue(block, armature.boneCollection.size());
            for (const auto bone : armature.boneCollection)
            {
                HashEntitySemanticIdentity(block, scene, bone);
            }
            HashValue(block, armature.inverseBindMatrices.size());
            for (const auto& matrix : armature.inverseBindMatrices)
            {
                HashMatrix(block, matrix);
            }
            armatureBlocks.push_back(block);
        }
        HashSortedBlocks(fingerprint, armatureBlocks);
""",
      """            const auto& armature = scene.armatures[index];
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
""")

patch(MODEL,
      """            const auto& animation = scene.animations[index];
            std::uint64_t block = FingerprintSeed;
            evidence.animationChannels += animation.channels.size();
            evidence.animationSamplers += animation.samplers.size();

            HashValue(block, animation.channels.size());
            for (const auto& channel : animation.channels)
            {
                const auto path = static_cast<std::uint32_t>(channel.path);
                HashValue(block, path);
                HashEntitySemanticIdentity(
                    block, scene, channel.target);
                HashValue(block, channel.samplerIndex);
                HashValue(block, channel.retargetIndex);
            }

            HashValue(block, animation.samplers.size());
            for (const auto& sampler : animation.samplers)
            {
                const auto mode = static_cast<std::uint32_t>(sampler.mode);
                HashValue(block, mode);
                HashAnimationDataSemanticIdentity(
                    block, scene, sampler.data);
            }
            animationBlocks.push_back(block);
        }
        HashSortedBlocks(fingerprint, animationBlocks);
""",
      """            const auto& animation = scene.animations[index];
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
""")

patch(MODEL,
      """            const auto& data = scene.animation_datas[index];
            std::uint64_t block = FingerprintSeed;
            evidence.animationKeyframes += data.keyframe_times.size();
            evidence.animationValues += data.keyframe_data.size();

            HashValue(block, data.keyframe_times.size());
            for (const auto value : data.keyframe_times)
            {
                HashValue(block, value);
            }
            HashValue(block, data.keyframe_data.size());
            for (const auto value : data.keyframe_data)
            {
                HashValue(block, value);
            }
            animationDataBlocks.push_back(block);
        }
        HashSortedBlocks(fingerprint, animationDataBlocks);
""",
      """            const auto& data = scene.animation_datas[index];
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
""")

# A field-specific report without leaking asset paths/names/animation values.
patch(MODEL,
      "    std::string DescribeRigAnimationEvidence(\n",
      """    std::string DescribeRigAnimationEvidenceDifference(
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
"""
      + "".join(f'        report("{field}", before.{field}Fingerprint, after.{field}Fingerprint);\n' for field in fields)
      + """        if (!changed)
            return "No diagnostic subgroup changed; investigate aggregate ordering or count drift.";
        return out.str();
    }

    std::string DescribeRigAnimationEvidence(
""")

patch(MODEL,
      """        result.importedEvidence = prepared.result_.importedEvidence;
        if (!ReloadEvidence(
""",
      """        result.importedEvidence = prepared.result_.importedEvidence;
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
""")
patch(MODEL,
      """                "Imported WISCENE rig/animation evidence changed after round-trip reload. Before: " +
                DescribeRigAnimationEvidence(result.importedEvidence) +
""",
      """                "Imported WISCENE rig/animation evidence changed after round-trip reload. Changed groups: " +
                DescribeRigAnimationEvidenceDifference(result.importedEvidence, result.reloadedEvidence) +
                ". Before: " + DescribeRigAnimationEvidence(result.importedEvidence) +
""")

# The headless synthetic tests already verify strict rejection of exact payload
# mutation; assert that the diagnostic subgroups identify the expected field.
patch(TESTS,
      """        if (evidence.rigAnimationFingerprint ==
            changedWeightEvidence.rigAnimationFingerprint)
""",
      """        if (evidence.rigAnimationFingerprint ==
                changedWeightEvidence.rigAnimationFingerprint ||
            evidence.skinWeightFingerprint == changedWeightEvidence.skinWeightFingerprint ||
            evidence.skinIndexFingerprint != changedWeightEvidence.skinIndexFingerprint ||
            evidence.animationTimesFingerprint != changedWeightEvidence.animationTimesFingerprint)
""")
patch(TESTS,
      """        if (evidence.rigAnimationFingerprint ==
            changedBindEvidence.rigAnimationFingerprint)
""",
      """        if (evidence.rigAnimationFingerprint ==
                changedBindEvidence.rigAnimationFingerprint ||
            evidence.inverseBindFingerprint == changedBindEvidence.inverseBindFingerprint ||
            evidence.boneHierarchyFingerprint != changedBindEvidence.boneHierarchyFingerprint ||
            evidence.animationValuesFingerprint != changedBindEvidence.animationValuesFingerprint)
""")
patch(TESTS,
      """        if (evidence.rigAnimationFingerprint ==
            changedTargetEvidence.rigAnimationFingerprint)
""",
      """        if (evidence.rigAnimationFingerprint ==
                changedTargetEvidence.rigAnimationFingerprint ||
            evidence.animationChannelFingerprint == changedTargetEvidence.animationChannelFingerprint ||
            evidence.skinWeightFingerprint != changedTargetEvidence.skinWeightFingerprint ||
            evidence.animationTimesFingerprint != changedTargetEvidence.animationTimesFingerprint)
""")
patch(TESTS,
      """        if (evidence.rigAnimationFingerprint ==
            changedAnimationEvidence.rigAnimationFingerprint)
""",
      """        if (evidence.rigAnimationFingerprint ==
                changedAnimationEvidence.rigAnimationFingerprint ||
            evidence.animationValuesFingerprint == changedAnimationEvidence.animationValuesFingerprint ||
            evidence.animationTimesFingerprint != changedAnimationEvidence.animationTimesFingerprint ||
            evidence.skinWeightFingerprint != changedAnimationEvidence.skinWeightFingerprint)
""")
print("A2 exact-anchor diagnostic patch applied to ImportService.h, ImportServiceModel.cpp, ImportTests.cpp")
