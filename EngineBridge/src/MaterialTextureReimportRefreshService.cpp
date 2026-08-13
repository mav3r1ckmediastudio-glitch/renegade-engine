#include "renegade/bridge/MaterialTextureAssetService.h"

#include "renegade/bridge/MaterialService.h"

#include <utility>

namespace renegade::bridge
{
    MaterialTextureRestoreResult RefreshMaterialTextureBindingsForAsset(
        wi::scene::Scene& scene,
        const std::string& projectRoot,
        const StableId& projectId,
        const StableId& textureAssetId,
        MaterialTextureResourceLoader loader)
    {
        MaterialTextureRestoreResult result;
        if (!IsValidStableId(textureAssetId))
        {
            result.error = "Texture reimport refresh requires a valid stable texture asset ID.";
            return result;
        }

        std::vector<MaterialTextureBindingRecord> bindings;
        if (!InspectMaterialTextureBindings(scene, bindings, result.error))
            return result;

        std::vector<MaterialTextureBindingRecord> matching;
        for (const auto& binding : bindings)
        {
            if (binding.textureAssetId == textureAssetId)
                matching.push_back(binding);
        }
        result.discovered = matching.size();
        if (matching.empty())
        {
            result.succeeded = true;
            result.error.clear();
            return result;
        }

        PreparedMaterialTextureAsset prepared;
        if (!PrepareMaterialTextureAsset(
                projectRoot, projectId, textureAssetId, prepared, result.error))
            return result;
        if (!loader)
            loader = LoadPreparedMaterialTextureAsset;

        std::size_t failures = 0;
        std::string firstFailure;
        for (const auto& binding : matching)
        {
            auto* material = scene.materials.GetComponent(binding.materialEntity);
            if (material == nullptr || IsTerrainOwnedMaterial(scene, binding.materialEntity))
            {
                ++failures;
                if (firstFailure.empty())
                    firstFailure = "persisted texture binding lost its editable material target";
                continue;
            }

            std::string bindingError;
            if (!ApplyPreparedMaterialTextureAsset(
                    scene, binding.materialEntity, binding.slot,
                    prepared, loader, bindingError))
            {
                ++failures;
                if (firstFailure.empty())
                {
                    firstFailure = bindingError.empty()
                        ? "Wicked did not create a valid refreshed resource"
                        : bindingError;
                }
                continue;
            }
            ++result.restored;
        }

        if (failures != 0)
        {
            result.error = std::to_string(failures) +
                " material binding(s) could not refresh after texture reimport. First failure: " +
                firstFailure;
            return result;
        }
        result.succeeded = true;
        result.error.clear();
        return result;
    }
}
