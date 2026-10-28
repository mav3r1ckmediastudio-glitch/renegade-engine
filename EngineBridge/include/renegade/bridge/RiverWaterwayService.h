#pragma once
#include "RiverFlowNetworkService.h"
#include "IdentityService.h"
#include "CommandService.h"
namespace renegade::bridge {
inline constexpr const char* RiverConnectionMetadataKey="renegade.river.connection.v1";
struct RiverConnection {
    StableId tributary,receiver,receiverNode;
};
struct RiverWaterway {
    std::vector<RiverConnection> connections;
    std::vector<RiverFlowNetworkReachInput> reaches;
    std::vector<RiverFlowNetworkJunctionInput> junctions;
    std::shared_ptr<const RiverFlowNetwork> network;
};
// Stable scene read. References are UUIDs, never runtime ECS indices. Each
// tributary downstream end has at most one receiver control-node attachment.
// Interior attachments create logical native spans; the authored Catmull curve,
// controls, material, terrain and original mesh are never split or regenerated.
// Empty authored connection set succeeds with an empty result.network.
bool ReadRiverWaterway(const wi::scene::Scene&,RiverWaterway&,std::string&);
// Cheap authoring query: validates saved identities and membership only, without
// tessellating splines, reading native meshes or building a flow domain.
// Current creator renderer supports a single interior two-inlet confluence.
// Output is unchanged on failure. Unconnected selection returns itself.
struct RiverWaterwayControls {
    wi::ecs::Entity owner=wi::ecs::INVALID_ENTITY;
    wi::ecs::Entity receivingNode=wi::ecs::INVALID_ENTITY;
    bool connected=false;
};
bool ReadRiverWaterwayControls(const wi::scene::Scene&,wi::ecs::Entity,
    RiverWaterwayControls&,std::string&);

bool RiverConnectionAnchor(const wi::scene::Scene&,wi::ecs::Entity receiver,
    wi::ecs::Entity control,XMFLOAT3& nativeAnchor,std::string&);
// Wrap a native river edit to propagate shared junction controls in one Undo.
std::unique_ptr<ICommand> MakeRiverWaterwayEditCommand(wi::scene::Scene&,wi::ecs::Entity,
    std::unique_ptr<ICommand> ordinary);
// Explicit snap, <= maxSnap metres (default1, supported0..5), flat water only.
// These commands require stable identities already assigned by SceneService.
// No terrain edits: creator integration wraps geometry-changing commands in the
// existing WatercourseSceneCommand transaction before exposing linked beds.
// Creator-safe entry points: snap and all affected linked riverbeds share one Undo.
std::unique_ptr<ICommand> MakeSetRiverConnectionCommand(wi::scene::Scene&,wi::ecs::Entity tributary,
    wi::ecs::Entity receiver,wi::ecs::Entity receiverControl,float maxSnap=1);
std::unique_ptr<ICommand> MakeDisconnectRiverCommand(wi::scene::Scene&,wi::ecs::Entity tributary);
class SetRiverConnectionCommand final:public ICommand {
public:
    SetRiverConnectionCommand(wi::scene::Scene&,wi::ecs::Entity tributary,
        wi::ecs::Entity receiver,wi::ecs::Entity receiverControl,float maxSnap=1);
    static std::unique_ptr<SetRiverConnectionCommand> Disconnect(wi::scene::Scene&,wi::ecs::Entity tributary);
    ~SetRiverConnectionCommand();
    bool Execute()override;void Undo()override;
    const std::string& Error()const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
// Explicit authoring from a chosen receiving control. Points are world-space,
// ordered upstream -> receiving node, at the native anchor's flat water level.
// Creates native WATER + stable control identities + connection + optional bed
// as one Undo. No proximity auto-connection or mountain fitting.
class CreateRiverTributaryCommand final:public ICommand {
public:
    CreateRiverTributaryCommand(wi::scene::Scene&,std::vector<XMFLOAT3> points,
        wi::ecs::Entity receiver,wi::ecs::Entity control,float fullWidth,
        wi::ecs::Entity terrain=wi::ecs::INVALID_ENTITY);
    ~CreateRiverTributaryCommand();
    bool Execute()override;void Undo()override;
    wi::ecs::Entity CreatedEntity()const;
    const std::string& Error()const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
