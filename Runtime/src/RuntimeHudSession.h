#pragma once
#include "RuntimeScreen.h"
#include "renegade/bridge/ScreenThemeResolver.h"
#include <memory>

namespace renegade::runtime
{
    // Heap-stable controller/presenter addresses keep native callbacks valid
    // through prepare/commit. A failed prepare retains the previous live HUD.
    class RuntimeHudSession
    {
        struct Instance
        {
            RuntimeScreenController controller;
            RuntimeScreenPresenter presenter;
        };
    public:
        ~RuntimeHudSession() {if(path_)Reset(*path_);}
        bool Load(const bridge::ScreenDocument& document,const std::string& projectRoot,
            wi::RenderPath2D& renderPath,RuntimeScreenPresenter::RequestSink sink,std::string& error,
            bool reducedMotion=false,bridge::ScreenAccessibilitySettings settings={})
        {
            if(document.usage!=bridge::ScreenUsage::Hud || !document.enabledInGame)
            {error="HUD composition requires an enabled HUD Screen.";return false;}
            auto candidate=std::make_unique<Instance>();
            bridge::ScreenDocument effective;
            if(!bridge::ResolveScreenTheme(projectRoot,document,effective,error)||
                !candidate->controller.Initialize(effective,error))return false;
            candidate->presenter.SetReducedMotion(reducedMotion);
            candidate->presenter.SetAccessibilitySettings(settings);
            if(!candidate->presenter.Load(document,projectRoot,renderPath,candidate->controller,std::move(sink),error))
            {candidate->presenter.Reset(renderPath);return false;}
            Reset(renderPath);instance_=std::move(candidate);path_=&renderPath;return true;
        }
        void Reset(wi::RenderPath2D& renderPath) noexcept
        {
            if(instance_)instance_->presenter.Reset(renderPath);
            instance_.reset();path_=nullptr;
        }
        bool IsLoaded() const noexcept{return instance_ && instance_->presenter.IsLoaded();}
        RuntimeScreenController* Controller() noexcept{return instance_?&instance_->controller:nullptr;}
        const RuntimeScreenController* Controller() const noexcept{return instance_?&instance_->controller:nullptr;}
        RuntimeScreenPresenter* Presenter() noexcept{return instance_?&instance_->presenter:nullptr;}
    private:
        std::unique_ptr<Instance> instance_;
        wi::RenderPath2D* path_=nullptr;
    };
}
