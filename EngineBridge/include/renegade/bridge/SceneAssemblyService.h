#pragma once
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/SelectionService.h"
#include "renegade/bridge/ReusableAssetService.h"
namespace renegade::bridge {
inline constexpr const char* SceneAssemblyKey = "renegade.scene_assembly";
bool AssemblyTransformChanged(const TransformState&,const TransformState&) noexcept;
bool IsSceneAssembly(const wi::scene::Scene&,wi::ecs::Entity);
wi::ecs::Entity ResolveAssemblySelection(const wi::scene::Scene&,wi::ecs::Entity,wi::ecs::Entity editing=wi::ecs::INVALID_ENTITY);
std::vector<wi::ecs::Entity> SelectionRoots(const wi::scene::Scene&,const std::vector<wi::ecs::Entity>&);
XMMATRIX AssemblyWorldMatrix(const wi::scene::Scene&,wi::ecs::Entity);
// Keep authoring selection and the scene operation in one Undo/Redo entry.
class SelectionHistoryCommand final : public ICommand {
public:
    using Selection=std::vector<wi::ecs::Entity>;
    SelectionHistoryCommand(SelectionService&,std::unique_ptr<ICommand>,std::function<Selection()> after={});
    bool Execute() override;
    void Undo() override;
private:
    SelectionService& selection_;
    std::unique_ptr<ICommand> command_;
    std::function<Selection()> resolveAfter_;
    Selection before_,after_;
    bool capturedAfter_=false;
};
class SelectionBatchCommand final : public ICommand {
public:
    void Add(std::unique_ptr<ICommand> c) { commands_.push_back(std::move(c)); }
    bool Execute() override;
    void Undo() override;
private:
    std::vector<std::unique_ptr<ICommand>> commands_;
};
class SceneAssemblyCommand final : public ICommand {
public:
    SceneAssemblyCommand(wi::scene::Scene&,std::vector<wi::ecs::Entity>,std::string name="Group");
    SceneAssemblyCommand(wi::scene::Scene&,wi::ecs::Entity unpackRoot);
    bool Execute() override;
    void Undo() override;
    wi::ecs::Entity Root() const {return root_;}
    const std::vector<wi::ecs::Entity>& Pieces() const {return pieces_;}
private:
    struct State {wi::ecs::Entity entity,parent;TransformState transform;};
    void Restore();
    wi::scene::Scene& scene_;
    std::vector<wi::ecs::Entity> pieces_;
    std::vector<State> before_;
    std::string name_;
    wi::ecs::Entity root_=wi::ecs::INVALID_ENTITY;
    wi::Archive rootSnapshot_;
    std::vector<std::pair<wi::ecs::Entity,wi::Archive>> payloadSnapshots_;
    TransformState rootTransform_;
    bool unpack_=false,initialized_=false,hasRootSnapshot_=false;
};
bool SaveSceneAssemblyPrefab(const std::string& projectRoot,const StableId& projectId,
    wi::scene::Scene&,const std::vector<wi::ecs::Entity>&,const std::string& name,StableId& asset,std::string& error);
}
