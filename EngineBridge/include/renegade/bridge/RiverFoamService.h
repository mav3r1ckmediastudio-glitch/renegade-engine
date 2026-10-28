#pragma once
#include "renegade/bridge/CommandService.h"
namespace renegade::bridge
{
inline constexpr const char* RiverFoamMetadataKey="renegade.river.foam.v1";
inline constexpr std::size_t MaximumRiverFoamRegions=16;
struct RiverFoamRegion
{
    // World metres; heading0 flows +Z, heading90 flows +X.
    XMFLOAT3 centre{0,0,0};
    float headingDegrees=0, obstacleRadius=.8f, wakeLength=4;
    float wakeWidth=.35f, speed=.65f, strength=.85f, contactWidth=.16f;
    bool enabled=true;
};
bool ValidateRiverFoamRegions(const std::vector<RiverFoamRegion>& regions,std::string& error);
bool ReadRiverFoamRegions(const wi::scene::Scene&,wi::ecs::Entity,
    std::vector<RiverFoamRegion>&,std::string& error);
class SetRiverFoamRegionsCommand final : public ICommand
{
public:
    SetRiverFoamRegionsCommand(wi::scene::Scene& scene,wi::ecs::Entity water,
        std::vector<RiverFoamRegion> regions);
    bool Execute() override;
    void Undo() override;
private:
    wi::scene::Scene* scene_; wi::ecs::Entity water_;
    std::vector<RiverFoamRegion> regions_;
    std::string before_,after_;bool captured_=false,hadKey_=false,hadMetadata_=false;
};
// Four float4 words per region, shared with the proof-owned PS.
// No renderer descriptor or custom shader ID is stored in authored metadata.
std::vector<XMFLOAT4> PackRiverFoamRegions(const std::vector<RiverFoamRegion>&);
}
