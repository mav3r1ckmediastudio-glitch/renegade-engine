#pragma once
#include "renegade/bridge/LightService.h"
#include "renegade/bridge/ScriptDocumentService.h"
namespace renegade::bridge {
enum class LightBehaviour { Constant, Flame, ElectricalFault, Rotating, Pulsing, Strobe };
inline constexpr const char* LightBehaviourSourcePath="Content/Scripts/Renegade/LightBehaviour_v1.lua";
const char* LightBehaviourName(LightBehaviour value) noexcept;
struct LightPreset {
 StableId id;
 std::string name="New Light";
 LightState light;
 LightBehaviour behaviour=LightBehaviour::Constant;
 float strength=0.35f, speed=1.2f, movement=0.03f;
 bool randomise=true;
};
LightPreset SanitizeLightPreset(LightPreset value);
std::vector<LightPreset> DefaultLightPresets();
bool SaveLightPreset(const std::string& root,const StableId& project,LightPreset& preset,
                     bool asNew,std::string& error);
std::vector<LightPreset> ListLightPresets(const std::string& root,const StableId& project,std::string& error);
bool SaveLightColours(const std::string& root,const std::vector<XMFLOAT3>& colours,std::string& error);
std::vector<XMFLOAT3> LoadLightColours(const std::string& root);
bool EnsureLightBehaviourSource(const std::string& root,std::string& error);
ScriptAttachment MakeLightBehaviourAttachment(const StableId& owner,const LightPreset& preset,
                                               const ScriptDocument& document);
LightPreset CaptureLightPreset(const wi::scene::Scene& scene,wi::ecs::Entity entity,
                              const ScriptDocument* document);
std::unique_ptr<ICommand> MakeDeleteLightSetupCommand(wi::scene::Scene&,ScriptDocument*,wi::ecs::Entity);
class LightSetupCommand final : public ICommand {
public:
 LightSetupCommand(wi::scene::Scene& scene,ScriptDocument* scripts,LightPreset setup,
                   wi::ecs::Entity target=wi::ecs::INVALID_ENTITY,
                   XMFLOAT3 position={},XMFLOAT4 rotation={0,0,0,1});
 bool Execute() override;
 void Undo() override;
 wi::ecs::Entity Entity() const noexcept {return entity_;}
private:
 wi::scene::Scene* scene_;
 ScriptDocument* scripts_;
 LightPreset setup_,before_;
 ScriptDocument scriptsBefore_,scriptsAfter_;
 std::unique_ptr<CreateLightCommand> create_;
 wi::ecs::Entity entity_;
 StableId ownerId_;
 bool captured_=false, hadIdentity_=false, hadName_=false;
};
}
