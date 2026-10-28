#pragma once
#include "renegade/bridge/CommandService.h"
#include <string>
#include <vector>

namespace renegade::bridge
{
    // The same material must hydrate in a fresh Runtime process where
    // authored Studio GPU resources do not exist. Rebind only the named
    // bundled rocky material; never rewrite unrelated creator assets.
    // Returns false with an actionable diagnostic when its three maps are
    // unavailable; on headless scenes (no GPU) it is intentionally inert.
    [[nodiscard]] bool RebindBundledRockyRiverbedMaterials(
        wi::scene::Scene& scene, std::string& error);

    // Opt-in native terrain MATERIAL paint. Different from river WATER shader
    // and the authored height-only linked-bed mask. No procedural regeneration.
    // Rivers share one added rocky layer; each command preserves earlier paint.
    // Always execute via scene-safe CommandService with GPU work drained.
    class PaintRockyRiverbedCommand final : public ICommand
    {
    public:
        PaintRockyRiverbedCommand(wi::scene::Scene& scene,
            wi::ecs::Entity terrain, wi::ecs::Entity river,
            std::string assetDirectory);
        bool Execute() override;
        void Undo() override;
        const std::string& Error() const noexcept { return error_; }

    private:
        struct ChunkMask
        {
            wi::ecs::Entity entity = wi::ecs::INVALID_ENTITY;
            std::vector<std::uint8_t> pixels, before;
        };
        bool Prepare();
        bool Apply();
        wi::scene::Scene* scene_;
        wi::ecs::Entity terrain_, river_, materialEntity_=wi::ecs::INVALID_ENTITY;
        std::string assets_, error_;
        std::vector<ChunkMask> masks_;
        bool prepared_ = false, applied_ = false, reuseMaterial_ = false;
    };
}
