#pragma once

#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/TerrainPropDocument.h"
#include <set>

namespace renegade::bridge
{
    // Owner-thread lifetime for one layer/profile in one scene generation.
    // Coarse display stays owned by the caller during fine-model preparation.
    class TerrainPropDetailProjection
    {
    public:
        using Cell = TerrainPropProjection::Cell;

        TerrainPropDetailProjection(wi::scene::Scene& scene, const StableId& layer,
            TerrainPropProfile profile, ReusableModelPlacementRequest request)
            : projection_(scene, false), profile_(std::move(profile)), request_(std::move(request))
        {
            projection_.SetLayerIdentity(layer);
        }

        bool HasResidentModel() const { return !cells_.empty(); }
        const ReusableModelPlacementRequest& Request() const { return request_; }

        bool Acquire(const TerrainPropPage& page, std::string& error, const PreparedReusableModelPlacement* supplied = nullptr)
        {
            const Cell cell{page.cellX, page.cellZ};
            if (cells_.count(cell)) return true;
            if (page.placements.empty() || request_.assetId != profile_.modelAssetId)
            {
                error = "Fine terrain page requires placements and a matching model.";
                return false;
            }
            if (cells_.empty())
            {
                PreparedReusableModelPlacement local;
                if (!supplied) local = ReusableAssetService().PrepareModelAssetPlacement(request_);
                const auto& prepared = supplied ? *supplied : local;
                if (prepared.Result().assetId != request_.assetId) { error = "Detail product identity changed."; return false; }
                if (!prepared.IsReady()) { error = prepared.Result().error; return false; }
                if (!projection_.Register(profile_.id, *prepared.PeekScene(), profile_.collision, error, profile_.drawDistance))
                    return false;
                projection_.SetRepresentation(profile_.id, TerrainPropProjection::Representation::Fine);
            }
            if (!projection_.Place(page, error))
            {
                if (cells_.empty()) projection_.UnregisterUnused(profile_.id);
                return false;
            }
            cells_.insert(cell);
            return true;
        }

        void Release(Cell cell)
        {
            if (!cells_.erase(cell)) return;
            projection_.EvictPage(cell);
            if (cells_.empty()) projection_.UnregisterUnused(profile_.id);
        }

    private:
        TerrainPropProjection projection_;
        TerrainPropProfile profile_;
        ReusableModelPlacementRequest request_;
        std::set<Cell> cells_;
    };
}
