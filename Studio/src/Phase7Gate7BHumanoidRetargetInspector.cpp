#include "Phase7Gate7BHumanoidRetargetInspector.h"
#include "InspectorSectionFramework.h"
#include "Phase7Gate7AAnimationInspector.h"
#include "RenegadeStudioChrome.h"
#include "S4BScriptAttachmentInspector.h"
#include "S4DGlobalScriptInspector.h"
#include "StudioApplication.h"
#include "renegade/bridge/AnimationService.h"
#include "renegade/bridge/HumanoidRetargetService.h"
#include "renegade/bridge/StudioSession.h"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
namespace renegade::studio
{
namespace
{
class HumanoidRetargetInspector;
class HumanoidRetargetSectionProvider final : public IInspectorSectionProvider
{
public:
explicit HumanoidRetargetSectionProvider(HumanoidRetargetInspector& owner) noexcept
: owner_(&owner)
{
descriptor_.id = Phase7HumanoidRetargetSectionId;
descriptor_.title = "HUMANOID / RETARGET";
descriptor_.order = 34;
descriptor_.defaultExpanded = false;
descriptor_.headerHeight = 28.0f;
descriptor_.spacingAfter = 6.0f;
}
[[nodiscard]] const InspectorSectionDescriptor& Descriptor() const noexcept override
{
return descriptor_;
}
[[nodiscard]] bool IsVisible(const InspectorSectionContext& context) const override;
[[nodiscard]] float MeasureContentHeight(const InspectorSectionContext&, float) const override;
void Refresh(const InspectorSectionContext&) override;
void ApplyLayout(const InspectorSectionContext&, const InspectorSectionLayout& layout) override;
private:
HumanoidRetargetInspector* owner_ = nullptr;
InspectorSectionDescriptor descriptor_;
};
class HumanoidRetargetInspector final
{
public:
HumanoidRetargetInspector(
StudioRenderPath& owner,
wi::gui::Window& panel,
InspectorSectionRegistry& registry,
std::function<void()> requestRefresh,
std::function<void(std::string)> setStatus)
: owner_(&owner), panel_(&panel), registry_(&registry),
requestRefresh_(std::move(requestRefresh)), setStatus_(std::move(setStatus))
{
}
void Register()
{
header_.Create("Phase 7 Humanoid Retarget Section Header");
header_.SetTooltip(
"Native Wicked humanoid mapping, pose reset and baked animation retargeting.");
header_.OnClick([this](const wi::gui::EventArgs&)
{
const bool opening = !registry_->IsExpanded(Phase7HumanoidRetargetSectionId);
for (const char* sectionId : {
"transform", "rendering", "materials",
S4BActionSectionId, S4BScriptSectionId, S4DGlobalScriptSectionId,
Phase7HumanoidRetargetSectionId, Phase7AnimationSectionId})
{
(void)registry_->SetExpanded(sectionId, false);
}
if (opening)
(void)registry_->SetExpanded(Phase7HumanoidRetargetSectionId, true);
RequestRefresh();
});
panel_->AddWidget(&header_);
status_.Create("Phase 7 Humanoid Retarget Status");
status_.SetColor(wi::Color::Transparent());
status_.SetFitTextEnabled(true);
panel_->AddWidget(&status_);
rigInfo_.Create("Phase 7 Humanoid Rig Info");
rigInfo_.SetColor(wi::Color::Transparent());
rigInfo_.SetFitTextEnabled(true);
panel_->AddWidget(&rigInfo_);
CreateButton(autoMap_, "Humanoid Auto Map", "AUTO-MAP", [this]() { AutoMap(); });
CreateButton(resetPose_, "Humanoid Reset Pose", "RESET POSE", [this]() { ResetPose(); });
boneSlot_.Create("Humanoid Bone Slot");
boneSlot_.SetTooltip("Choose the native humanoid slot to inspect or correct manually.");
for (std::size_t i = 0; i < bridge::HumanoidBoneCount; ++i)
{
const auto bone = static_cast<bridge::HumanoidBone>(i);
boneSlot_.AddItem(
bridge::HumanoidBoneName(bone), static_cast<std::uint64_t>(i));
}
boneSlot_.OnSelect([this](const wi::gui::EventArgs& args)
{
selectedBone_ = std::min<std::size_t>(
static_cast<std::size_t>(args.userdata),
bridge::HumanoidBoneCount - 1u);
RefreshBoneTargetList();
RequestRefresh();
});
panel_->AddWidget(&boneSlot_);
boneTarget_.Create("Humanoid Bone Target");
boneTarget_.SetTooltip(
"Manual correction: map the selected humanoid slot to a bone from this native armature.");
boneTarget_.OnSelect([this](const wi::gui::EventArgs& args)
{
SetBoneTarget(static_cast<wi::ecs::Entity>(args.userdata));
});
panel_->AddWidget(&boneTarget_);
CreateButton(
importAnimations_, "Humanoid Import Retarget Animations", "IMPORT + RETARGET",
[this]() { BrowseAnimationSource(); });
importHint_.Create("Phase 7 Humanoid Import Hint");
importHint_.SetColor(wi::Color::Transparent());
importHint_.SetText("WISCENE / FBX / GLTF / GLB / VRM / VRMA  // native baked retarget");
importHint_.SetFitTextEnabled(true);
panel_->AddWidget(&importHint_);
rigClips_.Create("Humanoid Rig Animation Clips");
rigClips_.SetTooltip(
"Native clips attached to this humanoid. Use the ANIMATION section for playback and scrub controls.");
panel_->AddWidget(&rigClips_);
clipInfo_.Create("Phase 7 Humanoid Clip Info");
clipInfo_.SetColor(wi::Color::Transparent());
clipInfo_.SetFitTextEnabled(true);
panel_->AddWidget(&clipInfo_);
std::string error;
if (!registry_->Register(
std::make_shared<HumanoidRetargetSectionProvider>(*this), error))
{
SetStatus("PHASE 7B // " + error);
}
}
[[nodiscard]] bool IsVisible(const InspectorSectionContext& context)
{
if (!context.hasSelection)
return false;
ResolveRig();
return rigEntity_ != wi::ecs::INVALID_ENTITY;
}
[[nodiscard]] float Measure() const noexcept
{
return 284.0f;
}
void Refresh()
{
ResolveRig();
RefreshMappingInfo();
RefreshBoneTargetList();
RefreshRigClips();
}
void PrepareForLayout()
{
for (auto* widget : Widgets())
widget->SetVisible(false);
}
void Layout(const InspectorSectionLayout& layout)
{
header_.SetVisible(true);
header_.SetPos(XMFLOAT2(12.0f, layout.top));
header_.SetSize(XMFLOAT2(layout.width, layout.headerHeight));
header_.SetText(
std::string(layout.expanded ? "▼  " : "▶  ") + "HUMANOID / RETARGET");
if (!layout.expanded)
return;
const float x = 12.0f;
const float width = layout.width;
float y = layout.contentTop;
status_.SetVisible(true);
status_.SetPos(XMFLOAT2(x, y));
status_.SetSize(XMFLOAT2(width, 22.0f));
y += 24.0f;
rigInfo_.SetVisible(true);
rigInfo_.SetPos(XMFLOAT2(x, y));
rigInfo_.SetSize(XMFLOAT2(width, 22.0f));
y += 26.0f;
const float half = (width - 8.0f) * 0.5f;
autoMap_.SetVisible(true);
autoMap_.SetPos(XMFLOAT2(x, y));
autoMap_.SetSize(XMFLOAT2(half, 28.0f));
resetPose_.SetVisible(true);
resetPose_.SetPos(XMFLOAT2(x + half + 8.0f, y));
resetPose_.SetSize(XMFLOAT2(half, 28.0f));
y += 34.0f;
boneSlot_.SetVisible(true);
boneSlot_.SetPos(XMFLOAT2(x, y));
boneSlot_.SetSize(XMFLOAT2(width, 28.0f));
y += 34.0f;
boneTarget_.SetVisible(true);
boneTarget_.SetPos(XMFLOAT2(x, y));
boneTarget_.SetSize(XMFLOAT2(width, 28.0f));
y += 34.0f;
importAnimations_.SetVisible(true);
importAnimations_.SetPos(XMFLOAT2(x, y));
importAnimations_.SetSize(XMFLOAT2(width, 28.0f));
y += 32.0f;
importHint_.SetVisible(true);
importHint_.SetPos(XMFLOAT2(x, y));
importHint_.SetSize(XMFLOAT2(width, 20.0f));
y += 24.0f;
rigClips_.SetVisible(true);
rigClips_.SetPos(XMFLOAT2(x, y));
rigClips_.SetSize(XMFLOAT2(width, 28.0f));
y += 32.0f;
clipInfo_.SetVisible(true);
clipInfo_.SetPos(XMFLOAT2(x, y));
clipInfo_.SetSize(XMFLOAT2(width, 22.0f));
}
private:
template<typename Fn>
void CreateButton(
SceneInspectorButton& button,
const char* name,
const char* text,
Fn&& fn)
{
button.Create(name);
button.SetText(text);
button.OnClick(
[action = std::forward<Fn>(fn)](const wi::gui::EventArgs&) mutable
{
action();
});
panel_->AddWidget(&button);
}
[[nodiscard]] std::vector<wi::gui::Widget*> Widgets()
{
return {
&header_, &status_, &rigInfo_, &autoMap_, &resetPose_,
&boneSlot_, &boneTarget_, &importAnimations_, &importHint_,
&rigClips_, &clipInfo_};
}
void ResolveRig()
{
auto* session = bridge::StudioSession::Current();
if (session == nullptr || !session->Selection().HasSelection())
{
rigEntity_ = wi::ecs::INVALID_ENTITY;
return;
}
rigEntity_ = bridge::FindHumanoidRigEntity(
session->Scenes().GetScene(), session->Selection().SelectedEntity());
}
void RefreshMappingInfo()
{
auto* session = bridge::StudioSession::Current();
if (session == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY)
{
status_.SetText("No native armature/humanoid selected.");
rigInfo_.SetText("");
return;
}
const auto& scene = session->Scenes().GetScene();
const auto mapping = bridge::CaptureHumanoidMapping(scene, rigEntity_);
const auto mapped = static_cast<std::size_t>(std::count_if(
mapping.bones.begin(), mapping.bones.end(),
[](const wi::ecs::Entity entity)
{
return entity != wi::ecs::INVALID_ENTITY;
}));
const bool hasArmature = scene.armatures.Contains(rigEntity_);
const bool valid = bridge::IsHumanoidMappingValid(mapping);
status_.SetText(valid
? "Native Wicked humanoid // VALID"
: mapping.componentExists
? "Native Wicked humanoid // mapping incomplete"
: "Native Wicked armature // humanoid not mapped");
rigInfo_.SetText(
std::string(hasArmature ? "Armature" : "Humanoid") + " // " +
std::to_string(mapped) + "/" +
std::to_string(bridge::HumanoidBoneCount) + " slots mapped");
autoMap_.SetEnabled(hasArmature);
resetPose_.SetEnabled(mapping.componentExists);
importAnimations_.SetEnabled(valid);
}
void RefreshBoneTargetList()
{
boneSlot_.SetSelectedByUserdataWithoutCallback(
static_cast<std::uint64_t>(selectedBone_));
boneTarget_.ClearItems();
boneTarget_.AddItem("NONE", static_cast<std::uint64_t>(wi::ecs::INVALID_ENTITY));
auto* session = bridge::StudioSession::Current();
if (session == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY)
return;
const auto& scene = session->Scenes().GetScene();
for (const auto entity : bridge::CollectArmatureBones(scene, rigEntity_))
{
const auto* name = scene.names.GetComponent(entity);
boneTarget_.AddItem(
name != nullptr && !name->name.empty()
? name->name
: std::string("Bone ") + std::to_string(entity),
static_cast<std::uint64_t>(entity));
}
const auto mapping = bridge::CaptureHumanoidMapping(scene, rigEntity_);
const auto mappedEntity = selectedBone_ < bridge::HumanoidBoneCount
? mapping.bones[selectedBone_]
: wi::ecs::INVALID_ENTITY;
boneTarget_.SetSelectedByUserdataWithoutCallback(
static_cast<std::uint64_t>(mappedEntity));
}
void RefreshRigClips()
{
rigClips_.ClearItems();
auto* session = bridge::StudioSession::Current();
if (session == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY)
{
clipInfo_.SetText("");
return;
}
const auto clips = bridge::CollectAnimationClips(
session->Scenes().GetScene(), rigEntity_, true);
for (const auto& clip : clips)
{
rigClips_.AddItem(
clip.name, static_cast<std::uint64_t>(clip.entity));
}
clipInfo_.SetText(
std::to_string(clips.size()) +
" native rig clip(s) // playback lives in ANIMATION");
}
void AutoMap()
{
auto* session = bridge::StudioSession::Current();
if (session == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY)
return;
auto& scene = session->Scenes().GetScene();
const auto result = bridge::BuildAutoHumanoidMapping(scene, rigEntity_);
if (result.mappedBones == 0)
{
SetStatus("PHASE 7B // auto-map failed: " + result.error);
RequestRefresh();
return;
}
const bool changed = session->Commands().Execute(
std::make_unique<bridge::SetHumanoidMappingCommand>(
scene, rigEntity_, result.mapping));
SetStatus(
changed
? "PHASE 7B // native humanoid auto-map committed: " +
std::to_string(result.mappedBones) + " slots"
: "PHASE 7B // auto-map already matches current mapping");
RefreshMappingInfo();
RefreshBoneTargetList();
RequestRefresh();
}
void SetBoneTarget(const wi::ecs::Entity entity)
{
auto* session = bridge::StudioSession::Current();
if (session == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY ||
selectedBone_ >= bridge::HumanoidBoneCount)
{
return;
}
auto& scene = session->Scenes().GetScene();
const auto bone = static_cast<bridge::HumanoidBone>(selectedBone_);
if (session->Commands().Execute(
std::make_unique<bridge::SetHumanoidBoneCommand>(
scene, rigEntity_, bone, entity)))
{
SetStatus(
std::string("PHASE 7B // mapped ") + bridge::HumanoidBoneName(bone));
}
RefreshMappingInfo();
RefreshBoneTargetList();
RequestRefresh();
}
void ResetPose()
{
auto* session = bridge::StudioSession::Current();
if (session == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY)
return;
session->Scenes().GetScene().ResetPose(rigEntity_);
SetStatus("PHASE 7B // native bind pose restored");
RequestRefresh();
}
void BrowseAnimationSource()
{
auto* session = bridge::StudioSession::Current();
if (session == nullptr || rigEntity_ == wi::ecs::INVALID_ENTITY)
return;
const auto destination = rigEntity_;
wi::helper::FileDialogParams params;
params.type = wi::helper::FileDialogParams::OPEN;
params.description = "Humanoid animation source (WISCENE, FBX, GLTF, GLB, VRM, VRMA)";
params.extensions = {"wiscene", "fbx", "gltf", "glb", "vrm", "vrma"};
wi::helper::FileDialog(params, [this, destination](const std::string& fileName)
{
wi::eventhandler::Subscribe_Once(
wi::eventhandler::EVENT_THREAD_SAFE_POINT,
[this, destination, fileName](std::uint64_t)
{
RetargetAnimations(destination, fileName);
});
});
}
void RetargetAnimations(
const wi::ecs::Entity destination,
const std::string& fileName)
{
auto* session = bridge::StudioSession::Current();
if (session == nullptr)
return;
auto command = std::make_unique<bridge::RetargetHumanoidAnimationsCommand>(
session->Scenes().GetScene(), destination, fileName);
auto* commandView = command.get();
if (!command->Execute())
{
SetStatus("PHASE 7B // retarget failed: " + commandView->Result().error);
RequestRefresh();
return;
}
const auto created = commandView->Result().createdAnimations.size();
const auto format = commandView->Result().sourceFormat;
if (!session->Commands().RecordExecuted(std::move(command)))
{
SetStatus("PHASE 7B // retarget succeeded but command history rejected the transaction");
RequestRefresh();
return;
}
SetStatus(
std::string("PHASE 7B // ") +
bridge::HumanoidAnimationSourceFormatName(format) + " retargeted " +
std::to_string(created) + " baked clip(s)");
ResolveRig();
RefreshMappingInfo();
RefreshRigClips();
RequestRefresh();
}
void RequestRefresh()
{
if (requestRefresh_)
requestRefresh_();
}
void SetStatus(std::string status)
{
if (setStatus_)
setStatus_(std::move(status));
}
StudioRenderPath* owner_ = nullptr;
wi::gui::Window* panel_ = nullptr;
InspectorSectionRegistry* registry_ = nullptr;
std::function<void()> requestRefresh_;
std::function<void(std::string)> setStatus_;
wi::ecs::Entity rigEntity_ = wi::ecs::INVALID_ENTITY;
std::size_t selectedBone_ = 0;
SceneInspectorButton header_;
wi::gui::Label status_;
wi::gui::Label rigInfo_;
SceneInspectorButton autoMap_;
SceneInspectorButton resetPose_;
SceneInspectorComboBox boneSlot_;
SceneInspectorComboBox boneTarget_;
SceneInspectorButton importAnimations_;
wi::gui::Label importHint_;
SceneInspectorComboBox rigClips_;
wi::gui::Label clipInfo_;
};
bool HumanoidRetargetSectionProvider::IsVisible(
const InspectorSectionContext& context) const
{
return owner_ != nullptr && owner_->IsVisible(context);
}
float HumanoidRetargetSectionProvider::MeasureContentHeight(
const InspectorSectionContext&,
float) const
{
return owner_ != nullptr ? owner_->Measure() : 0.0f;
}
void HumanoidRetargetSectionProvider::Refresh(const InspectorSectionContext&)
{
if (owner_ != nullptr)
owner_->Refresh();
}
void HumanoidRetargetSectionProvider::ApplyLayout(
const InspectorSectionContext&,
const InspectorSectionLayout& layout)
{
if (owner_ != nullptr)
owner_->Layout(layout);
}
std::unique_ptr<HumanoidRetargetInspector> activeInspector;
StudioRenderPath* activeOwner = nullptr;
}
void RegisterPhase7Gate7BHumanoidRetargetInspector(
StudioRenderPath& owner,
wi::gui::Window& inspectorPanel,
InspectorSectionRegistry& registry,
std::function<void()> requestRefresh,
std::function<void(std::string)> setStatus)
{
activeInspector.reset();
activeOwner = &owner;
activeInspector = std::make_unique<HumanoidRetargetInspector>(
owner,
inspectorPanel,
registry,
std::move(requestRefresh),
std::move(setStatus));
activeInspector->Register();
}
void PreparePhase7Gate7BHumanoidRetargetInspector(StudioRenderPath& owner)
{
if (activeOwner == &owner && activeInspector)
activeInspector->PrepareForLayout();
}
}
