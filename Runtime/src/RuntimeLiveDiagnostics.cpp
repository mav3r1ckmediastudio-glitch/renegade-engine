#include "RuntimeApplication.h"

#include <utility>

namespace renegade::runtime
{
    void RuntimeApplication::UpdateLiveDiagnostics()
    {
        // This method is already the one lightweight Runtime hook called once
        // per application frame before Wicked advances the active RenderPath.
        // Character discovery, native navigation and bounded AI cognition stay
        // here rather than introducing another Runtime or Scene owner.
        const std::uint64_t sceneRevision = scenes_.Revision();
        const bool hasLevel = !scenes_.CurrentPath().empty() &&
            !screenPresenter_.IsLoaded();

        // A Screen destination is not an active gameplay Level. Explicitly
        // deactivate any Character controllers retained by the current Scene,
        // clear all transient AI state, and clear synchronization authority so
        // returning to a Level performs fresh deterministic setup.
        if (!hasLevel)
        {
            ResetRuntimeCombat(combatState_);
            ResetRuntimeCharacterDecision(characterDecisionState_);
            ResetRuntimeCharacterPerception(characterPerceptionState_);
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
            ResetRuntimeCombat(combatState_);
            ResetRuntimeCharacterDecision(characterDecisionState_);
            ResetRuntimeCharacterPerception(characterPerceptionState_);
            ResetRuntimeCharacterSystem(characterAiState_);
            characterState_ = {};
            characterSceneRevision_ = 0;
            characterSceneAttemptRevision_ = sceneRevision;
            characterSceneAttempted_ = true;
            characterSceneSyncFailed_ = false;

            bridge::CharacterRuntimeState discoveredCharacters;
            RuntimeCharacterSystemState resolvedCharacters;
            RuntimeCharacterPerceptionState resolvedPerception;
            RuntimeCharacterDecisionState resolvedDecision;
            RuntimeCombatState resolvedCombat;
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
            else if (!InitializeRuntimeCharacterPerception(
                         resolvedCharacters, resolvedPerception, characterError))
            {
                bridge::ResetRuntimeCharacters(
                    scenes_.GetScene(), discoveredCharacters);
                ResetRuntimeCharacterSystem(resolvedCharacters);
                ResetRuntimeCharacterPerception(resolvedPerception);
                characterSceneSyncFailed_ = true;
                diagnosticService_.Record(
                    bridge::DiagnosticSeverity::Error,
                    "runtime.ai",
                    "character.perception.failed",
                    characterError);
                wi::backlog::post(
                    "Renegade Runtime: Character perception setup failed: " +
                        characterError,
                    wi::backlog::LogLevel::Error);
            }
            else if (!InitializeRuntimeCharacterDecision(
                         scenes_.GetScene(), resolvedCharacters,
                         resolvedPerception, resolvedDecision, characterError))
            {
                bridge::ResetRuntimeCharacters(
                    scenes_.GetScene(), discoveredCharacters);
                ResetRuntimeCharacterSystem(resolvedCharacters);
                ResetRuntimeCharacterPerception(resolvedPerception);
                ResetRuntimeCharacterDecision(resolvedDecision);
                characterSceneSyncFailed_ = true;
                diagnosticService_.Record(
                    bridge::DiagnosticSeverity::Error,
                    "runtime.ai",
                    "character.decision.failed",
                    characterError);
                wi::backlog::post(
                    "Renegade Runtime: Character decision setup failed: " +
                        characterError,
                    wi::backlog::LogLevel::Error);
            }
            else if (!InitializeRuntimeCombat(
                         scenes_.GetScene(), resolvedCharacters,
                         resolvedCombat, characterError))
            {
                // AI-05 health/weapon/ammo state is Runtime-transient but joins
                // the same fail-closed Character transaction. A bad weapon
                // descriptor must never leave a partially autonomous actor.
                bridge::ResetRuntimeCharacters(
                    scenes_.GetScene(), discoveredCharacters);
                ResetRuntimeCharacterSystem(resolvedCharacters);
                ResetRuntimeCharacterPerception(resolvedPerception);
                ResetRuntimeCharacterDecision(resolvedDecision);
                ResetRuntimeCombat(resolvedCombat);
                characterSceneSyncFailed_ = true;
                diagnosticService_.Record(
                    bridge::DiagnosticSeverity::Error,
                    "runtime.ai",
                    "character.combat.failed",
                    characterError);
                wi::backlog::post(
                    "Renegade Runtime: Character combat setup failed: " +
                        characterError,
                    wi::backlog::LogLevel::Error);
            }
            else
            {
                characterState_ = std::move(discoveredCharacters);
                characterAiState_ = std::move(resolvedCharacters);
                characterPerceptionState_ = std::move(resolvedPerception);
                characterDecisionState_ = std::move(resolvedDecision);
                combatState_ = std::move(resolvedCombat);
                characterSceneRevision_ = sceneRevision;
                if (!characterState_.characters.empty())
                {
                    diagnosticService_.Record(
                        bridge::DiagnosticSeverity::Info,
                        "runtime.ai",
                        "character.scene.started",
                        "Renegade Character runtime initialized profile, perception, decision and combat state for " +
                            std::to_string(characterState_.characters.size()) +
                            " authored character(s)");
                }
            }
        }

        if (hasLevel && !paused_ && !characterSceneSyncFailed_ &&
            characterSceneRevision_ == sceneRevision &&
            playerSceneRevision_ == sceneRevision &&
            characterPerceptionState_.characters.size() == characterAiState_.characters.size() &&
            characterDecisionState_.characters.size() == characterAiState_.characters.size() &&
            combatState_.characters.size() == characterAiState_.characters.size())
        {
            // UpdateLiveDiagnostics runs before SyncPlayerForScene(). Requiring
            // matching scene revisions prevents a stale transient player ECS
            // handle from a previous Level being sampled during the first frame
            // after a scene transition.
            const float simulationDt = scenes_.GetScene().dt;
            UpdateRuntimeCharacterPerception(
                scenes_.GetScene(),
                characterAiState_,
                characterPerceptionState_,
                player_,
                playerSettings_,
                simulationDt);
            const CombatEventEmitter combatEmitter = [this](
                bridge::GameplayEvent event,
                std::string& error)
            {
                const std::string eventName = event.name;
                const std::string eventPayload = event.payload;
                const bool accepted = creatorScripts_.EnqueueGameplayEvent(
                    std::move(event), error);
                if (accepted)
                {
                    diagnosticService_.Record(
                        bridge::DiagnosticSeverity::Info,
                        "runtime.ai.combat",
                        eventName,
                        eventPayload);
                }
                return accepted;
            };
            UpdateRuntimeCombatDecision(
                scenes_.GetScene(),
                characterAiState_,
                characterPerceptionState_,
                characterDecisionState_,
                combatState_,
                combatEmitter,
                simulationDt);
            UpdateRuntimeCharacterDecision(
                scenes_.GetScene(),
                characterAiState_,
                characterPerceptionState_,
                characterDecisionState_,
                simulationDt,
                false);
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
            characterAiState_.characters.size() == characterState_.characters.size() &&
            characterPerceptionState_.characters.size() == characterAiState_.characters.size() &&
            characterDecisionState_.characters.size() == characterAiState_.characters.size() &&
            combatState_.characters.size() == characterAiState_.characters.size();
        std::string firstCharacterId;
        std::string firstFaction;
        std::string firstVisionDistance;
        std::string firstAggression;
        std::string firstAwareness;
        std::string firstSuspicion;
        std::string firstKnowledgeSource;
        std::string firstKnowledgeSubject;
        std::string firstKnowledgeConfidence;
        std::string firstMemoryAge;
        std::string firstLastKnownPosition;
        std::string firstIntent;
        std::string firstPreviousIntent;
        std::string firstIntentReason;
        std::string firstDecisionGoal;
        std::string firstPatrolRouteId;
        std::string firstTopScores;
        std::string firstHealth;
        std::string firstHealthFraction;
        std::string firstWeaponStyle;
        std::string firstWeaponRange;
        std::string firstAmmo;
        std::string firstReserveAmmo;
        std::string firstReloadRemaining;
        std::string firstTargetDistance;
        bool firstDirectSight = false;
        bool firstHasDecisionGoal = false;
        std::uint64_t totalMemories = 0;
        std::uint64_t totalCognitionTicks = 0;
        std::uint64_t totalIntentTransitions = 0;
        if (!characterAiState_.characters.empty())
        {
            const auto& first = characterAiState_.characters.front();
            firstCharacterId = first.stableEntityId;
            firstFaction = first.authoring.factionId;
            firstVisionDistance = std::to_string(first.tuning.visionDistance);
            firstAggression = std::to_string(first.tuning.aggression);
        }
        for (const auto& cognition : characterPerceptionState_.characters)
        {
            totalMemories += static_cast<std::uint64_t>(cognition.memories.size());
            totalCognitionTicks += cognition.cognitionTicks;
        }
        for (const auto& decision : characterDecisionState_.characters)
            totalIntentTransitions += decision.transitionCount;
        if (!characterPerceptionState_.characters.empty())
        {
            const auto& first = characterPerceptionState_.characters.front();
            firstAwareness = ToString(first.awareness);
            firstSuspicion = std::to_string(first.suspicion);
            const CharacterMemoryRecord* memory =
                FindCharacterMemory(first, RuntimePlayerKnowledgeId);
            if (memory == nullptr && !first.memories.empty())
                memory = &first.memories.front();
            if (memory != nullptr)
            {
                firstKnowledgeSource = ToString(memory->source);
                firstKnowledgeSubject = memory->subjectId;
                firstKnowledgeConfidence = std::to_string(memory->confidence);
                firstMemoryAge = std::to_string(memory->ageSeconds);
                firstDirectSight = memory->directSight;
                firstLastKnownPosition =
                    std::to_string(memory->lastKnownPosition.x) + "," +
                    std::to_string(memory->lastKnownPosition.y) + "," +
                    std::to_string(memory->lastKnownPosition.z);
            }
        }
        if (!characterDecisionState_.characters.empty())
        {
            const auto& first = characterDecisionState_.characters.front();
            firstIntent = ToString(first.intent);
            firstPreviousIntent = ToString(first.previousIntent);
            firstIntentReason = first.lastTransitionReason;
            firstHasDecisionGoal = first.hasGoal;
            firstPatrolRouteId = first.patrol.routeId;
            if (first.hasGoal)
            {
                firstDecisionGoal =
                    std::to_string(first.goal.x) + "," +
                    std::to_string(first.goal.y) + "," +
                    std::to_string(first.goal.z);
            }
            for (std::size_t index = 0; index < first.topScores.size(); ++index)
            {
                if (index > 0)
                    firstTopScores += ";";
                firstTopScores += std::string(ToString(first.topScores[index].intent)) +
                    "=" + std::to_string(first.topScores[index].score);
            }
        }
        if (!combatState_.characters.empty())
        {
            const auto& first = combatState_.characters.front();
            firstHealth = std::to_string(first.health);
            firstHealthFraction = std::to_string(HealthFraction(first));
            firstWeaponStyle = std::to_string(static_cast<std::int32_t>(first.weapon.style));
            firstWeaponRange =
                std::to_string(first.effectiveRange.minRange) + "," +
                std::to_string(first.effectiveRange.preferredRange) + "," +
                std::to_string(first.effectiveRange.maxRange);
            firstAmmo = std::to_string(first.magazineAmmo);
            firstReserveAmmo = std::to_string(first.reserveAmmo);
            firstReloadRemaining = std::to_string(first.reloadRemainingSeconds);
            firstTargetDistance = std::isfinite(first.targetDistance)
                ? std::to_string(first.targetDistance)
                : std::string("none");
        }
        diagnosticService_.Observe("ai", {
            {"character_count", static_cast<std::uint64_t>(characterState_.characters.size())},
            {"resolved_character_count", static_cast<std::uint64_t>(characterAiState_.characters.size())},
            {"perception_character_count", static_cast<std::uint64_t>(characterPerceptionState_.characters.size())},
            {"decision_character_count", static_cast<std::uint64_t>(characterDecisionState_.characters.size())},
            {"combat_character_count", static_cast<std::uint64_t>(combatState_.characters.size())},
            {"active_character_count", activeCharacters},
            {"profile_registry_version", static_cast<std::uint64_t>(bridge::CharacterProfileRegistryVersion)},
            {"first_character_id", firstCharacterId},
            {"first_faction", firstFaction},
            {"first_vision_distance", firstVisionDistance},
            {"first_aggression", firstAggression},
            {"first_awareness", firstAwareness},
            {"first_suspicion", firstSuspicion},
            {"first_knowledge_source", firstKnowledgeSource},
            {"first_knowledge_subject", firstKnowledgeSubject},
            {"first_knowledge_confidence", firstKnowledgeConfidence},
            {"first_memory_age", firstMemoryAge},
            {"first_last_known_position", firstLastKnownPosition},
            {"first_direct_sight", firstDirectSight},
            {"first_intent", firstIntent},
            {"first_previous_intent", firstPreviousIntent},
            {"first_intent_reason", firstIntentReason},
            {"first_has_decision_goal", firstHasDecisionGoal},
            {"first_decision_goal", firstDecisionGoal},
            {"first_patrol_route_id", firstPatrolRouteId},
            {"first_top_utility_scores", firstTopScores},
            {"first_health", firstHealth},
            {"first_health_fraction", firstHealthFraction},
            {"first_weapon_style", firstWeaponStyle},
            {"first_weapon_range", firstWeaponRange},
            {"first_ammo", firstAmmo},
            {"first_reserve_ammo", firstReserveAmmo},
            {"first_reload_remaining", firstReloadRemaining},
            {"first_target_distance", firstTargetDistance},
            {"player_health", std::to_string(combatState_.playerHealth)},
            {"player_dead", combatState_.playerDead},
            {"combat_shots_fired", combatState_.shotsFired},
            {"combat_shots_hit", combatState_.shotsHit},
            {"combat_reloads", combatState_.reloads},
            {"combat_damage_events", combatState_.damageEvents},
            {"gameplay_event_queue_depth", static_cast<std::uint64_t>(creatorScripts_.PendingEventCount())},
            {"gameplay_event_dropped", static_cast<std::uint64_t>(creatorScripts_.DroppedEventCount())},
            {"combat_events_rejected", combatState_.combatEventsRejected},
            {"memory_count", totalMemories},
            {"cognition_ticks", totalCognitionTicks},
            {"decision_ticks", characterDecisionState_.decisionTicks},
            {"intent_transitions", totalIntentTransitions},
            {"ai_path_requests", characterDecisionState_.pathRequests},
            {"ai_stuck_recoveries", characterDecisionState_.stuckRecoveries},
            {"ai_rejected_goals", characterDecisionState_.rejectedGoals},
            {"ai_navigation_grid_id", characterDecisionState_.navigationGridId},
            {"los_queries", characterPerceptionState_.lineOfSightQueries},
            {"visual_detections", characterPerceptionState_.visualDetections},
            {"heard_stimuli", characterPerceptionState_.heardStimuli},
            {"damage_stimuli", characterPerceptionState_.damageStimuli},
            {"sound_queue_depth", static_cast<std::uint64_t>(characterPerceptionState_.sounds.size())},
            {"sound_dropped", characterPerceptionState_.droppedSounds},
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
            {"character_perception_count", static_cast<std::uint64_t>(characterPerceptionState_.characters.size())},
            {"character_decision_count", static_cast<std::uint64_t>(characterDecisionState_.characters.size())},
            {"character_combat_count", static_cast<std::uint64_t>(combatState_.characters.size())},
            {"character_memory_count", totalMemories},
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
