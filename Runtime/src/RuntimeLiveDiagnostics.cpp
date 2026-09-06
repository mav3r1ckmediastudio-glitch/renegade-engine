#include "RuntimeApplication.h"

namespace renegade::runtime
{
    void RuntimeApplication::UpdateLiveDiagnostics()
    {
        diagnosticService_.Heartbeat();
        const auto now = diagnosticService_.ElapsedMs();
        if (now - lastDiagnosticSampleMs_ < 250) return;
        lastDiagnosticSampleMs_ = now;
        diagnosticService_.Observe("runtime", {
            {"project", startupResult_.projectDescriptorPath},
            {"scene", scenes_.CurrentPath()},
            {"startup_finished", startupFinished_}, {"startup_succeeded", startupResult_.succeeded},
            {"startup_code", std::string(RuntimeBootstrapCodeName(startupResult_.code))},
            {"startup_message", startupResult_.message},
            {"scene_loaded", !scenes_.CurrentPath().empty()},
            {"scene_revision", scenes_.Revision()}, {"paused", paused_},
            {"quit_requested", quitRequested_}, {"player_spawned", player_.IsSpawned()},
            {"scripts_running", creatorScripts_.IsRunning()},
            {"scripts_active", static_cast<std::uint64_t>(creatorScripts_.ActiveInstanceCount())},
            {"scripts_disabled", static_cast<std::uint64_t>(creatorScripts_.DisabledInstanceCount())},
            {"event_queue_depth", static_cast<std::uint64_t>(creatorScripts_.PendingEventCount())},
            {"event_dropped", static_cast<std::uint64_t>(creatorScripts_.DroppedEventCount())},
            {"events_dispatched", creatorScripts_.DispatchedEventCount()},
            {"event_delivery_attempts", creatorScripts_.EventDeliveryAttemptCount()},
            {"last_event_sequence", creatorScripts_.LastEventSequence()},
            {"last_event_name", creatorScripts_.LastEventName()},
            {"last_event_target", creatorScripts_.LastEventTarget()},
            {"audio_scene_synced", audioSceneRevision_ != 0 && audioSceneRevision_ == scenes_.Revision()},
            {"audio_source_count", static_cast<std::uint64_t>(scenes_.GetScene().sounds.GetCount())},
            {"screen_loaded", screenPresenter_.IsLoaded()},
            {"last_action", startupResult_.lastActionId},
            {"last_action_code", startupResult_.lastActionCode},
            {"last_action_message", startupResult_.lastActionMessage}}, "Runtime/src/RuntimeApplication.cpp");
    }
}
