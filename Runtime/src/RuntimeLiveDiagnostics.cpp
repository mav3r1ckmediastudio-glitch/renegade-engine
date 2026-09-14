#include "RuntimeApplication.h"

#include <utility>

namespace renegade::runtime
{
    void RuntimeApplication::UpdateLiveDiagnostics()
    {
        // This method is already the one lightweight Runtime hook called once
        // per application frame before Wicked advances the active RenderPath.
        // Character discovery and native navigation stay here rather than
        // introducing another Runtime loop or Scene owner.
        const std::uint64_t sceneRevision = scenes_.Revision();
        const bool hasLevel = !scenes_.CurrentPath().empty() &&
            !screenPresenter_.IsLoaded();

        // A Screen destination is not an active gameplay Level. Explicitly
        // deactivate any Character controllers retained by the current Scene,
        // clear AI-02's transient resolved state, and clear synchronization
        // authority so returning to a Level performs fresh deterministic setup.
        if (!hasLevel)
        {
            ResetRuntimeCharacterSystem(characterAiState_);
            if (!characterState_.characters.empty())
                bridge::ResetRuntimeCharacters(scenes_.GetScene(), characterState_);
            characterSceneRevision_ = 0;
            characterSceneAttemptRevision_ = 0;
            characterSceneAttempted_ = false;
            characterSceneSyncFailed_ = false;
        }
        else if (!characterSceneAttempted_ ||
                 characterSceneAttemptRevision_ != sceneRevision)
        {
            ResetRuntimeCharacterSystem(characterAiState_);
            characterState_ = {};
            characterSceneRevision_ = 0;
            characterSceneAttemptRevision_ = sceneRevision;
            characterSceneAttempted_ = true;
            characterSceneSyncFailed_ = false;

            bridge::CharacterRuntimeState discoveredCharacters;
            RuntimeCharacterSystemState resolvedCharacters;
            std::string characterError;
            if (!bridge::InitializeRuntimeCharacters(
                    scenes_.GetScene(), discoveredCharacters, characterError))
            {
                characterSceneSyncFailed_ = true;
                diagnosticService_.Record(
                    bridge::DiagnosticSeverity::Error,
                    "runtime.ai",
                    "character.scene.failed",
                    characterError);
                wi::backlog::post(
                    "Renegade Runtime: Character discovery failed: " +
                        characterError,
                    wi::backlog::LogLevel::Error);
            }
            else if (!InitializeRuntimeCharacterSystem(
                         scenes_.GetScene(), discoveredCharacters,
                         resolvedCharacters, characterError))
            {
                // AI-01 has already activated the native CharacterComponents.
                // Fail the combined Character transaction atomically if AI-02
                // profile/faction/reference resolution cannot complete.
                bridge::ResetRuntimeCharacters(
                    scenes_.GetScene(), discoveredCharacters);
                ResetRuntimeCharacterSystem(resolvedCharacters);
                characterSceneSyncFailed_ = true;
                diagnosticService_.Record(
                    bridge::DiagnosticSeverity::Error,
                    "runtime.ai",
                    "character.profile.failed",
                    characterError);
                wi::backlog::post(
                    "Renegade Runtime: Character profile setup failed: " +
                        characterError,
                    wi::backlog::LogLevel::Error);
            }
            else
            {
                characterState_ = std::move(discoveredCharacters);
                characterAiState_ = std::move(resolvedCharacters);
                characterSceneRevision_ = sceneRevision;
                if (!characterState_.characters.empty())
                {
                    diagnosticService_.Record(
                        bridge::DiagnosticSeverity::Info,
                        "runtime.ai",
                        "character.scene.started",
                        "Renegade Character runtime discovered and resolved " +
                            std::to_string(characterState_.characters.size()) +
                            " authored character(s)");
                }
            }
        }

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

        std::uint64_t activeCharacters = 0;
        for (const auto& record : characterState_.characters)
        {
            const auto* character = scenes_.GetScene().characters.GetComponent(record.entity);
            if (character != nullptr && character->IsActive())
                ++activeCharacters;
        }

        const bool characterSceneSynced = hasLevel &&
            characterSceneAttempted_ && !characterSceneSyncFailed_ &&
            characterSceneRevision_ == scenes_.Revision() &&
            characterAiState_.characters.size() == characterState_.characters.size();
        std::string firstCharacterId;
        std::string firstFaction;
        std::string firstVisionDistance;
        std::string firstAggression;
        if (!characterAiState_.characters.empty())
        {
            const auto& first = characterAiState_.characters.front();
            firstCharacterId = first.stableEntityId;
            firstFaction = first.authoring.factionId;
            firstVisionDistance = std::to_string(first.tuning.visionDistance);
            firstAggression = std::to_string(first.tuning.aggression);
        }
        diagnosticService_.Observe("ai", {
            {"character_count", static_cast<std::uint64_t>(characterState_.characters.size())},
            {"resolved_character_count", static_cast<std::uint64_t>(characterAiState_.characters.size())},
            {"active_character_count", activeCharacters},
            {"profile_registry_version", static_cast<std::uint64_t>(bridge::CharacterProfileRegistryVersion)},
            {"first_character_id", firstCharacterId},
            {"first_faction", firstFaction},
            {"first_vision_distance", firstVisionDistance},
            {"first_aggression", firstAggression},
            {"scene_synced", characterSceneSynced},
            {"scene_sync_attempted", characterSceneAttempted_},
            {"scene_sync_failed", characterSceneSyncFailed_},
            {"scene_attempt_revision", characterSceneAttemptRevision_}},
            "Runtime/src/RuntimeLiveDiagnostics.cpp");

        diagnosticService_.Observe("runtime", {
            {"project", startupResult_.projectDescriptorPath},
            {"scene", scenes_.CurrentPath()},
            {"startup_finished", startupFinished_}, {"startup_succeeded", startupResult_.succeeded},
            {"startup_code", std::string(RuntimeBootstrapCodeName(startupResult_.code))},
            {"startup_message", startupResult_.message},
            {"scene_loaded", !scenes_.CurrentPath().empty()},
            {"scene_revision", scenes_.Revision()}, {"paused", paused_},
            {"quit_requested", quitRequested_}, {"player_spawned", player_.IsSpawned()},
            {"character_count", static_cast<std::uint64_t>(characterState_.characters.size())},
            {"character_profile_count", static_cast<std::uint64_t>(characterAiState_.characters.size())},
            {"character_scene_synced", characterSceneSynced},
            {"character_scene_sync_failed", characterSceneSyncFailed_},
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
