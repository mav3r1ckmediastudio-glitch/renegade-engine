#pragma once
#include "renegade/bridge/DiagnosticService.h"

namespace renegade::studio
{
    // A stack observation covers every early return without changing routing.
    class DiagnosticInputFrame
    {
    public:
        DiagnosticInputFrame(bridge::DiagnosticService& service, bool left, bool right)
            : service_(service), left_(left), right_(right) {}
        void StopAt(const char* owner) { owner_ = owner; }
        void CameraReached(bool active) { cameraReached_ = true; cameraActive_ = active; }
        ~DiagnosticInputFrame()
        {
            if (!left_ && !right_) return;
            service_.SetState("last_pointer", {{"left_down", left_}, {"right_down", right_},
                {"routing_stop", std::string(owner_)}, {"camera_handler_reached", cameraReached_},
                {"camera_active", cameraActive_}, {"elapsed_ms", service_.ElapsedMs()},
                {"source", std::string("Studio/src/StudioApplication.cpp:StudioRenderPath::Update")}});
        }
    private:
        bridge::DiagnosticService& service_;
        bool left_, right_;
        bool cameraReached_ = false, cameraActive_ = false;
        const char* owner_ = "update.completed";
    };
}
