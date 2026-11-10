#pragma once
#include <WickedEngine.h>
#include "renegade/bridge/ParticleTextureLibraryService.h"
#include <functional>
#include <memory>
namespace renegade::studio {
class ParticleTextureBrowser final:public wi::gui::Window {
public:
 ParticleTextureBrowser();~ParticleTextureBrowser();
 void Create();
 void Open(const std::string& root,const bridge::StableId& project,const std::string& layer,bridge::MaterialTextureSlot slot,
  std::function<bool()> current,std::function<void(const bridge::ParticleTextureEntry&,bool)> apply,std::function<void()> closed);
 void Close();
 void Update(const wi::Canvas&,float dt) override;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
