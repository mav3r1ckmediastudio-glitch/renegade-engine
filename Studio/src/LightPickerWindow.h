#pragma once
#include <WickedEngine.h>
#include "renegade/bridge/LightPresetService.h"
#include <functional>
#include <memory>
namespace renegade::bridge {class StudioSession;}
namespace renegade::studio {
class LightPickerWindow final : public wi::gui::Window {
public:
 LightPickerWindow();
 ~LightPickerWindow();
 void Create();
 void Open(bridge::StudioSession&,wi::scene::LightComponent::LightType,
           wi::ecs::Entity target=wi::ecs::INVALID_ENTITY);
 bridge::LightPreset Draft() const;
 bool KeepPlacing() const;
 bool TakePlaceRequest();
 bool TakeApplyRequest();
 wi::ecs::Entity Target() const;
 bool MatchesSession(const bridge::StudioSession&) const;
 void ShowError(const std::string&);
 void UpdatePreview(float dt);
 void PreRenderPreview();
 void RenderPreview() const;
private:
 struct Impl;std::unique_ptr<Impl> impl_;
};
}
