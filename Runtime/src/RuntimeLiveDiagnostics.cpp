#include "RuntimeApplication.h"

namespace renegade::runtime
{
    void RuntimeApplication::UpdateLiveDiagnostics()
    {
        // This method is already the one lightweight Runtime hook called once
        // per application frame before Wicked advances the active RenderPath.
        // Keep native navigation here rather than introducing another Runtime
        // loop or Scene owner: SetPathGoal/Turn/Move are consumed by Wicked's
        // normal Scene update immediately afterwards.
        const std::uint64_t sceneRevision = scenes_.Revision();
        const bool hasLevel = !scenes_.CurrentPath().empty() &&
            !screenPresenter_.IsLoaded();
        if (navigationSceneRevision_ != sceneRevision ||
            (navigationSceneRevision_ == 0 && hasLevel))
        {
            navigationState_ = {};
            navigationSceneRevision_ = sceneRevision;
            if (hasLevel)
            {
                std::string navigationError;
                if (!bridge::InitializeRuntimeNavigation(
                        scenes_.GetScene(), navigationState_, navigationError))
                {
                    diagnosticService_.Record(
                        bridge::DiagnosticSeverity::Error,
                        "runtime.navigation",
                        "navigation.scene.failed",
                        navigationError);
                    wi::backlog::post(
                        "Renegade Runtime: native navigation setup failed: " +
                            navigationError,
                        wi::backlog::LogLevel::Error);
                }
                else if (!navigationState_.agents.empty())
                {
                    diagnosticService_.Record(
                        bridge::DiagnosticSeverity::Info,
                        "runtime.navigation",
                        "navigation.scene.started",
                        "Native Wicked navigation started " +
                            std::to_string(navigationState_.agents.size()) +
                            " authored agent(s)");
                    wi::backlog::post(
                        "Renegade Runtime: native Wicked navigation started " +
                            std::to_string(navigationState_.agents.size()) +
                            " authored agent(s).",
                        wi::backlog::LogLevel::Default);
                }
            }
        }

        if (hasLevel && !paused_ && !navigationState_.agents.empty())
        {
            // Move() follows Wicked's own character-controller contract: the
            // authored amount is supplied once per frame and Wicked integrates
            // it in CharacterComponent's fixed update. This fixed value is used
            // only for the low-frequency repath countdown.
            bridge::UpdateRuntimeNavigation(
                scenes_.GetScene(), navigationState_, 1.0f / 60.0f);
        }

        diagnosticService_.Heartbeat();
        const auto now = diagnosticService_.ElapsedMs();
        if (now - lastDiagnosticSampleMs_ < 250) return;
        lastDiagnosticSampleMs_ = now;

        std::uint64_t navigationArrived = 0;
        for (const auto& agent : navigationState_.agents)
        {
            if (agent.arrived)
                ++navigationArrived;
        }

        diagnosticService_.Observe("runtime", {
            {"project", startupResult_.projectDescriptorPath},
            {"scene", scenes_.CurrentPath()},
            {"startup_finished", startupFinished_}, {"startup_succeeded", startupResult_.succeeded},
            {"startup_code", std::string(RuntimeBootstrapCodeName(startupResult_.code))},
            {"startup_message", startupResult_.message},
            {"scene_loaded", !scenes_.CurrentPath().empty()},
            {"scene_revision", scenes_.Revision()}, {"paused", paused_},
            {"quit_requested", quitRequested_}, {"player_spawned", player_.IsSpawned()},
            {"navigation_synced", navigationSceneRevision_ == scenes_.Revision()},
            {"navigation_agents", static_cast<std::uint64_t>(navigationState_.agents.size())},
            {"navigation_arrived", navigationArrived},
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