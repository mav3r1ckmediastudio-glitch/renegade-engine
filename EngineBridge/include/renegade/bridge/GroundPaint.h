#pragma once
#include "renegade/bridge/CommandService.h"
#include <filesystem>
#include <map>

namespace renegade::bridge
{
    struct GroundMaterial
    {
        std::string name;
        std::filesystem::path colour, normal, surface;
        float grassCoverage = 1;
        float tileMetres = 4;
    };
    constexpr const char* GroundCustomFolder = "Content/Terrain/Materials/Custom";
    enum class GroundTool { Paint, Erase, Soften };
    std::vector<GroundMaterial> GroundPalette(const std::filesystem::path& project);
    bool ImportGroundMaterial(const std::filesystem::path& project, const std::filesystem::path& colour,
        GroundMaterial& result, std::string& error);
    wi::ecs::Entity ResolveGroundMaterial(wi::scene::Scene& scene, const GroundMaterial& source,
        const std::filesystem::path& project, std::string& error);
    wi::ecs::Entity FindGroundMaterial(const wi::scene::Scene& scene, const GroundMaterial& source);
    void ReadGroundSettings(const wi::scene::Scene& scene, GroundMaterial& source);
    std::unique_ptr<ICommand> SetGroundSettings(wi::scene::Scene& scene, wi::ecs::Entity terrain,
        wi::ecs::Entity material, float tileMetres, float grassCoverage);
    void RefreshGroundSlope(wi::terrain::Terrain& terrain, wi::terrain::ChunkData& chunk,
        const wi::scene::MeshComponent& mesh);
    void RefreshGroundGrass(wi::scene::Scene& scene, wi::terrain::Terrain& terrain, wi::terrain::ChunkData& chunk);
    std::vector<float> AuthoredPlantLengths(const wi::scene::Scene& scene, wi::ecs::Entity entity,
        const wi::HairParticleSystem& hair);
    void SetAuthoredPlantLengths(wi::scene::Scene& scene, wi::ecs::Entity entity,
        const std::vector<float>& lengths);
    class GroundStroke final : public ICommand
    {
    public:
        GroundStroke(wi::scene::Scene& scene, wi::ecs::Entity terrain, wi::ecs::Entity material,
            GroundTool tool, float radius, float strength, float softness);
        bool Dab(const XMFLOAT3& point);
        bool Execute() override;
        bool TryUndo() override;
        void Undo() override { (void)TryUndo(); }
        bool Changed() const { return !edits_.empty(); }
        const std::string& Error() const { return error_; }
    private:
        struct Edit { std::vector<uint8_t> before, after; };
        using Key = std::tuple<int,int,size_t>;
        std::map<Key,Edit> edits_;
        bool Restore(bool after);
        wi::scene::Scene& scene_;
        wi::ecs::Entity terrain_, material_;
        GroundTool tool_;
        float radius_, strength_, softness_;
        size_t paintSlot_ = SIZE_MAX;
        XMFLOAT3 last_{};
        bool haveLast_ = false;
        size_t addedSlot_ = SIZE_MAX;
        std::string error_;
    };
    struct GroundRules { float rockStart=32, rockFull=55, highStart=0, highFull=30; bool highEnabled=false; };
    GroundRules ReadGroundRules(const wi::scene::Scene& scene, wi::ecs::Entity terrain);
    std::unique_ptr<ICommand> SetGroundBase(wi::scene::Scene& scene, wi::ecs::Entity terrain,
        wi::ecs::Entity material, wi::ecs::Entity rock, GroundRules rules);
}
