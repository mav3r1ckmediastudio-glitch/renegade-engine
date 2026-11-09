#pragma once
#include <WickedEngine.h>
#include "renegade/bridge/ParticleEffectService.h"
#include <memory>
namespace renegade::bridge {class StudioSession;}
namespace renegade::studio {
class ParticlePickerWindow final:public wi::gui::Window {
public:
 ParticlePickerWindow();~ParticlePickerWindow();
 void SetVisible(bool) override;
 wi::gui::Widget& TextureBrowserWidget();
 void Create();void Open(bridge::StudioSession&,wi::ecs::Entity target=wi::ecs::INVALID_ENTITY);
 bridge::ParticleEffect Draft() const;wi::ecs::Entity Target() const;
 bool KeepPlacing() const;bool TakeApplyRequest();bool MatchesSession(const bridge::StudioSession&) const;
 void ShowError(const std::string&);void UpdatePreview(float);void PreRenderPreview();void RenderPreview() const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
