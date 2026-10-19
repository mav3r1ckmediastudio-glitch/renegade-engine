#include "RuntimeApplication.h"
#include "RuntimeCharacterCollision.h"

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
        const bool hasLevel = !scenes_.CurrentPath().empty();

        // A retained Level remains gameplay authority while UI is displayed.
        // Composition controls input and pause independently. Clear gameplay
        // state only when no Level is loaded, never merely because a menu opened.
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

            PrepareRuntimeCharacterCollisionScene(scenes_.GetScene());

            bridge::CharacterRuntimeState discoveredCharacters;
            RuntimeCharacterSystemState resolvedCharacters;
            RuntimeCharacterPerceptionState resolvedPerception;
            RuntimeCharacterDecisionState resolvedDecision;
            RuntimeCombatState resolvedCombat;
            RuntimeCharacterAnimationState resolvedAnimation;
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
            else if (!InitializeRuntimeCharacterAnimations(
                         scenes_.GetScene(), resolvedCharacters,
                         resolvedCombat, resolvedAnimation, characterError))
            {
                bridge::ResetRuntimeCharacters(
                    scenes_.GetScene(), discoveredCharacters);
                ResetRuntimeCharacterSystem(resolvedCharacters);
                ResetRuntimeCharacterPerception(resolvedPerception);
                ResetRuntimeCharacterDecision(resolvedDecision);
                ResetRuntimeCombat(resolvedCombat);
                ResetRuntimeCharacterAnimations(resolvedAnimation);
                characterSceneSyncFailed_ = true;
                diagnosticService_.Record(
                    bridge::DiagnosticSeverity::Error,
                    "runtime.ai",
                    "character.animation.failed",
                    characterError);
                wi::backlog::post(
                    "Renegade Runtime: Character animation setup failed: " +
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
                characterAnimationState_ = std::move(resolvedAnimation);
                characterSceneRevision_ = sceneRevision;
                for (const auto& actor : characterAiState_.characters)
                {
                    const auto* clips = FindCharacterAnimation(
                        characterAnimationState_, actor.stableEntityId);
                    const auto* weapon = FindCharacterCombat(
                        combatState_, actor.stableEntityId);
                    if (clips == nullptr || weapon == nullptr)
                        continue;
                    const auto clipCount = [clips](CharacterAnimationSemantic semantic)
                    {
                        return clips->clips[CharacterAnimationIndex(semantic)].size();
                    };
                    wi::backlog::post("[AI06-ACTOR-READY] id=" + actor.stableEntityId +
                        " faction=" + actor.authoring.factionId +
                        " combatStyle=" + std::to_string(static_cast<int>(actor.authoring.combatStyle)) +
                        " weaponStyle=" + std::to_string(static_cast<int>(weapon->weapon.style)) +
                        " idle=" + std::to_string(clipCount(CharacterAnimationSemantic::Idle)) +
                        " walk=" + std::to_string(clipCount(CharacterAnimationSemantic::Locomotion)) +
                        " run=" + std::to_string(clipCount(CharacterAnimationSemantic::Run)) +
                        " attack=" + std::to_string(clipCount(CharacterAnimationSemantic::Attack)));
                }
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
            combatState_.characters.size() == characterAiState_.characters.size() &&
            characterAnimationState_.characters.size() == characterAiState_.characters.size())
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
                simulationDt,
                !combatState_.playerDead);
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
                simulationDt,
                [this](const bridge::StableId& id)
                {
                    const auto* record = FindCharacterAnimation(characterAnimationState_, id);
                    if (!record) return true;
                    const auto* clip = scenes_.GetScene().animations.GetComponent(record->activeClip);
                    if (!clip || !clip->IsPlayingOnce() || !clip->IsPlaying()) return true;
                    if (record->activeSemantic == CharacterAnimationSemantic::Hit) return false;
                    return record->activeSemantic != CharacterAnimationSemantic::Attack ||
                        CharacterAnimationSecondsRemaining(*clip) <= 0.18f;
                });
            UpdateRuntimeCharacterDecision(
                scenes_.GetScene(),
                characterAiState_,
                characterPerceptionState_,
                characterDecisionState_,
                simulationDt,
                false);
            const auto priorPlayback = characterAnimationState_.playbackRequests;
            UpdateRuntimeCharacterAnimations(
                scenes_.GetScene(),
                characterAiState_,
                characterDecisionState_,
                combatState_,
                characterAnimationState_,
                simulationDt);
            // Log only native clip transitions, not every frame. This makes a
            // real TestGame T-pose distinguishable from missing AI activation.
            if (characterAnimationState_.playbackRequests != priorPlayback)
            {
                for (const auto& animation : characterAnimationState_.characters)
                {
                    const auto* clip = scenes_.GetScene().animations.GetComponent(
                        animation.activeClip);
                    if (clip != nullptr)
                        wi::backlog::post("[AI06-PLAY] id=" + animation.characterId +
                            " action=" + CharacterAnimationSemanticName(animation.activeSemantic) +
                            " clip=" + animation.resolvedClipName +
                            " channels=" + std::to_string(clip->channels.size()) +
                            " playing=" + std::to_string(clip->IsPlaying()));
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
            bridge::UpdateRuntimeNavigation(
                scenes_.GetScene(), navigationState_, 1.0f / 60.0f);
        }

        diagnosticService_.Heartbeat();
        const auto now = diagnosticService_.ElapsedMs();
        const auto pendingRelease=playerEquipment_.scheduledProjectiles.size();
        const bool projectileTransition=projectiles_.launched!=lastDiagnosticProjectileLaunched_ ||
            pendingRelease!=lastDiagnosticProjectilePending_;
        if (now - lastDiagnosticSampleMs_ < 250 && !projectileTransition) return;
        lastDiagnosticSampleMs_ = now;
        lastDiagnosticProjectileLaunched_=projectiles_.launched;
        lastDiagnosticProjectilePending_=pendingRelease;

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
        std::string firstPursuitOrigin;
        std::string firstPursuitSubject;
        std::string firstWanderOrigin;
        std::uint64_t firstWanderExtentChunks = 0;
        std::uint64_t firstWanderVisitCount = 0;
        bool firstDirectSight = false;
        bool firstHasDecisionGoal = false;
        bool firstPursuitExhausted = false;
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
            firstWanderExtentChunks = static_cast<std::uint64_t>(
                std::max(0, first.authoring.wanderExtentChunks));
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
            firstPursuitSubject = first.pursuitSubjectId;
            firstPursuitExhausted = !first.exhaustedPursuitSubjectId.empty();
            firstWanderVisitCount = first.wanderVisitCount;
            if (first.hasWanderOrigin)
            {
                firstWanderOrigin =
                    std::to_string(first.wanderOrigin.x) + "," +
                    std::to_string(first.wanderOrigin.y) + "," +
                    std::to_string(first.wanderOrigin.z);
            }
            if (first.hasPursuitOrigin)
            {
                firstPursuitOrigin =
                    std::to_string(first.pursuitOrigin.x) + "," +
                    std::to_string(first.pursuitOrigin.y) + "," +
                    std::to_string(first.pursuitOrigin.z);
            }
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
        // Expose native clip selection beside AI intent/combat so an actor
        // gliding in TestGame is diagnosable without synthetic test assumptions.
        std::uint64_t animationPlaybackCount = 0;
        std::uint64_t animationMissingCount = 0;
        std::uint64_t animationClipCount = 0;
        std::uint64_t animationCrossfadeCount = 0;
        std::uint64_t animationIncompatibleCount = 0;
        std::string animationBlendState;
        std::string firstAnimationSemantic;
        std::string firstAnimationClip;
        std::string firstAnimationRequest;
        bool firstAnimationPlaying = false;
        for (const auto& animation : characterAnimationState_.characters)
        {
            animationCrossfadeCount += animation.crossfadeTransitions;
            animationIncompatibleCount += animation.incompatibleTransitions;
            if (animationBlendState.size() < 2048)
                animationBlendState += animation.characterId + ":" + animation.resolvedClipName +
                    ":duration=" + std::to_string(animation.blendDuration) +
                    ":contributors=" + std::to_string(animation.blendClips.size()) +
                    ":fades=" + std::to_string(animation.crossfadeTransitions) +
                    ":base_idle=" + std::to_string(animation.baseIdle) +
                    ":variation=" + std::to_string(animation.idleVariation) + ";";
            animationPlaybackCount += animation.playbackRequests;
            animationMissingCount += animation.missingRequests;
            for (const auto& variants : animation.clips)
                animationClipCount += variants.size();
        }
        if (!characterAnimationState_.characters.empty())
        {
            const auto& first = characterAnimationState_.characters.front();
            firstAnimationSemantic = CharacterAnimationSemanticName(first.activeSemantic);
            firstAnimationClip = first.resolvedClipName;
            firstAnimationRequest = first.lastRequest;
            const auto* native = scenes_.GetScene().animations.GetComponent(first.activeClip);
            firstAnimationPlaying = native != nullptr && native->IsPlaying();
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
            {"first_pursuit_origin", firstPursuitOrigin},
            {"first_pursuit_subject", firstPursuitSubject},
            {"first_pursuit_exhausted", firstPursuitExhausted},
            {"first_wander_origin", firstWanderOrigin},
            {"first_wander_extent_chunks", firstWanderExtentChunks},
            {"first_wander_visit_count", firstWanderVisitCount},
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
            {"animation_character_count", static_cast<std::uint64_t>(characterAnimationState_.characters.size())},
            {"animation_clip_count", animationClipCount},
            {"animation_playback_requests", animationPlaybackCount},
            {"animation_missing_requests", animationMissingCount},
            {"animation_crossfade_transitions", animationCrossfadeCount},
            {"animation_incompatible_transitions", animationIncompatibleCount},
            {"animation_blend_state", animationBlendState},
            {"first_animation_semantic", firstAnimationSemantic},
            {"first_animation_clip", firstAnimationClip},
            {"first_animation_request", firstAnimationRequest},
            {"first_animation_playing", firstAnimationPlaying},
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

        XMFLOAT3 diagnosticCharacterPosition{};
        if(!characterAiState_.characters.empty())
            if(const auto* t=scenes_.GetScene().transforms.GetComponent(characterAiState_.characters.front().entity))
                diagnosticCharacterPosition=t->GetPosition();
        const auto* projectileTargetHealth=FindCharacterCombat(combatState_,projectiles_.lastContact.targetSubjectId);
        XMFLOAT3 diagnosticPlayerPosition{};
        if(const auto* t=scenes_.GetScene().transforms.GetComponent(player_.entity))diagnosticPlayerPosition=t->GetPosition();
        std::string burnRegions;
        if(!burning_.entries.empty()) {
            const auto& body=burning_.entries.front();
            for(unsigned i=0;i<RuntimeBurningStatus::FlameCount;++i) {
                const auto* emitter=scenes_.GetScene().emitters.GetComponent(body.emitters[i]);
                const auto* name=scenes_.GetScene().names.GetComponent(body.bones[i]);
                if(emitter)burnRegions+=std::to_string(i)+":"+(name?name->name:"?")+":"+
                    std::to_string(emitter->center.x)+","+std::to_string(emitter->center.y)+","+
                    std::to_string(emitter->center.z)+";";
            }
        }
        diagnosticService_.Observe("projectiles", {
            {"burn_region_positions",burnRegions},
            {"player_x",std::to_string(diagnosticPlayerPosition.x)},
            {"player_y",std::to_string(diagnosticPlayerPosition.y)},
            {"player_z",std::to_string(diagnosticPlayerPosition.z)},
            {"player_spawn_z",std::to_string(player_.spawnPosition.z)},
            {"player_yaw",std::to_string(player_.yaw)},
            {"first_character_x",std::to_string(diagnosticCharacterPosition.x)},
            {"first_character_y",std::to_string(diagnosticCharacterPosition.y)},
            {"first_character_z",std::to_string(diagnosticCharacterPosition.z)},
            {"channel_active",channel_.active},
            {"channel_ticks",static_cast<std::uint64_t>(channel_.ticks)},
            {"channel_starts",static_cast<std::uint64_t>(channel_.starts)},
            {"channel_target",channel_.lastImpact.contact.targetSubjectId},
            {"channel_target_health",FindCharacterCombat(combatState_,channel_.lastImpact.contact.targetSubjectId)?
                std::to_string(FindCharacterCombat(combatState_,channel_.lastImpact.contact.targetSubjectId)->health):std::string{}},
            {"channel_visuals",channelVisual_.root==wi::ecs::INVALID_ENTITY?0ull:4ull},
            {"active_burns",static_cast<std::uint64_t>(burning_.entries.size())},
            {"burn_remaining",burning_.entries.empty()?std::string{}:std::to_string(burning_.entries.front().remaining)},
            {"burn_damage_per_second",burning_.entries.empty()?std::string{}:std::to_string(burning_.entries.front().damagePerSecond)},
            {"projectile_target_health",projectileTargetHealth?std::to_string(projectileTargetHealth->health):std::string{}},
            {"impact_marks",static_cast<std::uint64_t>(projectileVisuals_.impactDecals.size())},
            {"projectile_generation", projectiles_.generation},
            {"projectile_launched", projectiles_.launched},
            {"projectile_impacted", projectiles_.impacted},
            {"projectile_active", static_cast<std::uint64_t>(projectiles_.simulation.Records().size())},
            {"projectile_markers", static_cast<std::uint64_t>(projectiles_.markers.size())},
            {"projectile_visuals", static_cast<std::uint64_t>(projectileVisuals_.instances.size())},
            {"projectile_visual_templates", static_cast<std::uint64_t>(projectileVisuals_.templates.size())},
            {"projectile_visual_error", projectileVisuals_.error},
            {"blood_sheets", static_cast<std::uint64_t>(projectileVisuals_.blood.liquidSheets.sheets.size())},
            {"blood_drops", static_cast<std::uint64_t>(projectileVisuals_.blood.drops.size())},
            {"blood_stains", static_cast<std::uint64_t>(projectileVisuals_.blood.stains.size())},
            {"projectile_stuck", static_cast<std::uint64_t>(projectileVisuals_.retained.size())},
            {"projectile_effect_emitters", static_cast<std::uint64_t>(projectileVisuals_.EffectCount())},
            {"projectile_pending_release", static_cast<std::uint64_t>(playerEquipment_.scheduledProjectiles.size())},
            {"projectile_animation_time_ms", static_cast<std::uint64_t>(std::max(0.0f,playerViewAnimation_.pairedTime)*1000)},
            {"projectile_last_x", std::to_string(projectiles_.lastContact.position.x)},
            {"projectile_last_y", std::to_string(projectiles_.lastContact.position.y)},
            {"projectile_last_z", std::to_string(projectiles_.lastContact.position.z)},
            {"projectile_last_target", projectiles_.lastContact.targetSubjectId},
            {"projectile_last_surface", projectiles_.lastContact.surfaceId},
            {"projectile_error", projectiles_.lastError},
            {"projectile_launch_socket", projectiles_.lastLaunchSocket},
            {"projectile_launch_x", std::to_string(projectiles_.lastLaunchPosition.x)},
            {"projectile_launch_y", std::to_string(projectiles_.lastLaunchPosition.y)},
            {"projectile_launch_z", std::to_string(projectiles_.lastLaunchPosition.z)},
            {"player_loaded_shells", static_cast<std::uint64_t>(std::max(0,playerViewAnimation_.loadedShells))}}, "Runtime/src/RuntimeApplication.cpp");

        const auto* uiPlayerTransform=scenes_.GetScene().transforms.GetComponent(player_.entity);
        const XMFLOAT3 uiPlayerPosition=uiPlayerTransform?uiPlayerTransform->GetPosition():XMFLOAT3(0,0,0);
        // Opt-in Natural Lightning playground: live GPU/scene visibility audit.
        // Does not affect gameplay events or project contents.
        const auto& visualStorm=lightningStorm_.natural;
        const auto& visualScene=scenes_.GetScene();
        diagnosticService_.Observe("lightning", {
            {"storm_active",lightningStorm_.active},
            {"natural_enabled",lightningStorm_.naturalEnabled},
            {"natural_driven",visualStorm.driven},
            {"natural_root_valid",visualStorm.root!=wi::ecs::INVALID_ENTITY},
            {"natural_live_lands",static_cast<std::uint64_t>(visualStorm.liveLands)},
            {"natural_visible_ticks",static_cast<std::uint64_t>(visualStorm.visibleTicks)},
            {"natural_live_clock",std::to_string(visualStorm.liveClock)},
            {"natural_last_x",std::to_string(visualStorm.lastLand.x)},
            {"natural_last_y",std::to_string(visualStorm.lastLand.y)},
            {"natural_last_z",std::to_string(visualStorm.lastLand.z)},
            {"natural_slot0_bolt",visualScene.materials.Contains(visualStorm.slots[0].bolt)},
            {"natural_slot0_ground",visualScene.materials.Contains(visualStorm.slots[0].ground)},
            {"natural_gpu_frame0",visualStorm.lightning[0][0].IsValid()},
            {"natural_bolt_visible_scale",visualScene.transforms.GetComponent(visualStorm.slots[0].bolt)?
                std::to_string(visualScene.transforms.GetComponent(visualStorm.slots[0].bolt)->scale_local.x):std::string("none")}},
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
            {"player_view_rig_spawned", playerViewRig_.IsSpawned()},
            {"player_position_x", std::to_string(uiPlayerPosition.x)},
            {"player_position_y", std::to_string(uiPlayerPosition.y)},
            {"player_position_z", std::to_string(uiPlayerPosition.z)},
            {"player_yaw", std::to_string(player_.yaw)},
            {"player_primary_hand_socket",
                playerViewRig_.primaryHandSocket != wi::ecs::INVALID_ENTITY},
            {"player_off_hand_socket",
                playerViewRig_.offHandSocket != wi::ecs::INVALID_ENTITY},
            {"player_two_hand_support_socket",
                playerViewRig_.twoHandSupportSocket != wi::ecs::INVALID_ENTITY},
            {"player_view_rig_proof_geometry",
                playerViewRig_.primaryArmProof != wi::ecs::INVALID_ENTITY &&
                playerViewRig_.offHandArmProof != wi::ecs::INVALID_ENTITY},
            {"player_equipment_reserved_hands", static_cast<std::uint64_t>(playerEquipment_.actions.ReservedHands())},
            {"player_equipment_primary_phase", static_cast<std::uint64_t>(playerEquipment_.actions.Channels()[0].phase)},
            {"player_equipment_authored", playerEquipment_.authored},
            {"player_equipment_ready", playerEquipment_.ready},
            {"player_primary_equipment", playerEquipment_.primary.equipment.assetId},
            {"player_off_hand_equipment", playerEquipment_.offHand.equipment.assetId},
            {"player_equipment_error", playerEquipment_.error},
            {"player_view_asset_loaded",
                playerViewRig_.viewModelRoot != wi::ecs::INVALID_ENTITY},
            {"player_view_action", PlayerViewActionName(playerViewRig_.action)},
            {"player_view_animation_initialized", playerViewAnimation_.initialized},
            {"player_view_animation_clip", playerViewAnimation_.resolvedClipName},
            {"player_view_paired_assembly", playerViewAnimation_.pairedAssembly},
            {"player_view_independent_hands", playerViewAnimation_.handLayers.enabled},
            {"player_view_hand_avoidance", playerViewAnimation_.handLayers.avoidance.enabled},
            {"player_view_hand_corrections", static_cast<std::uint64_t>(playerViewAnimation_.handLayers.avoidance.corrections)},
            {"player_view_hand_unresolved", static_cast<std::uint64_t>(playerViewAnimation_.handLayers.avoidance.unresolved)},
            {"player_view_shield_phase", static_cast<std::uint64_t>(playerViewAnimation_.handLayers.blockPhase)},
            {"player_melee_chain_window", playerEquipment_.chainInputWindow},
            {"player_melee_queued_direction", static_cast<std::uint64_t>(playerEquipment_.queuedStrike.direction)},
            {"player_melee_queued", playerEquipment_.queuedStrike.pending},
            {"player_melee_chained", static_cast<std::uint64_t>(playerEquipment_.chainedCount)},
            {"player_view_melee_direction", static_cast<std::uint64_t>(playerViewAnimation_.handLayers.direction)},
            {"player_view_melee_charge_phase", static_cast<std::uint64_t>(playerViewAnimation_.handLayers.chargePhase)},
            {"player_view_melee_charge_percent", static_cast<std::uint64_t>(playerViewAnimation_.handLayers.chargeSeconds/playerViewAnimation_.handLayers.fullChargeSeconds*100)},
            {"player_view_melee_release_percent", static_cast<std::uint64_t>(playerViewAnimation_.handLayers.chargeStrength*100)},
            {"player_view_primary_attacking", playerViewAnimation_.handLayers.attacking},
            {"player_view_shield_time_ms", static_cast<std::uint64_t>(playerViewAnimation_.handLayers.leftTime * 1000.0f)},
            {"player_view_primary_time_ms", static_cast<std::uint64_t>(playerViewAnimation_.handLayers.rightTime * 1000.0f)},
            {"player_view_active_tracks", static_cast<std::uint64_t>(
                (playerViewAnimation_.activeClip != wi::ecs::INVALID_ENTITY ? 1 : 0) +
                (playerViewAnimation_.activeWeaponClip != wi::ecs::INVALID_ENTITY ? 1 : 0))},
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
            {"screen_loaded", screenPresenter_->IsLoaded()},
            {"gameplay_attribute_document_loaded",attributeDocument_.has_value()},
            {"gameplay_attribute_project_instance",gameplayAttributes_.Project()!=nullptr},
            {"gameplay_attribute_player_instance",gameplayAttributes_.Player()!=nullptr},
            {"gameplay_attribute_character_instances",static_cast<std::uint64_t>(gameplayAttributes_.Characters().size())},
            {"gameplay_attribute_diagnostic",attributeDiagnostic_},
            {"hud_loaded", hudSession_.IsLoaded()},
            {"hud_visible", uiComposition_.IsHudVisible()},
            {"hud_document_id", hudSession_.Controller()?hudSession_.Controller()->AuthoredDocument().envelope.documentId:std::string{}},
            {"hud_instance", uiComposition_.HudIdentity()},
            {"hud_level_session", hudLevelSession_},
            {"hud_bindings_ready", !hudSession_.Controller() || hudSession_.Controller()->BindingFrame().ready},
            {"hud_binding_issues", static_cast<std::uint64_t>(hudSession_.Controller()?hudSession_.Controller()->BindingFrame().issues.size():0)},
            {"ui_binding_session", uiSessionIdentity_},
            {"ui_binding_revision", uiGameplayRevision_},
            {"ui_gameplay_input_blocked", uiComposition_.Resolve().gameplayInputBlocked},
            {"ui_pause_requested", uiPauseRequested_},
            {"ui_keyboard_focus_instance", uiComposition_.Resolve().keyboardFocusOwner},
            {"last_action", startupResult_.lastActionId},
            {"last_action_code", startupResult_.lastActionCode},
            {"last_action_message", startupResult_.lastActionMessage}}, "Runtime/src/RuntimeApplication.cpp");

        // The large general Runtime telemetry group is bounded to 64 entries. Give
        // UI authoring its own stable group so recently added gameplay/HUD values
        // are actually observable in native QA rather than silently truncated.
        const std::string sampleAttributeId =
            attributeDocument_ && !attributeDocument_->profiles.empty() &&
            !attributeDocument_->profiles.front().attributes.empty()
                ? attributeDocument_->profiles.front().attributes.front().id : std::string{};
        const auto sampleNumber=[&](const bridge::GameplayAttributeInstance* instance)->std::string
        {
            if(instance && !sampleAttributeId.empty())
                if(const auto* value=instance->Find(sampleAttributeId))
                    if(const auto* numeric=std::get_if<double>(value))
                        return std::to_string(*numeric);
            return {};
        };
        std::uint64_t presentedWidgets=0,presentedTracks=0;
        const auto countPresented=[&](const bridge::ScreenDocument& document)
        {
            presentedWidgets+=document.widgets.size();
            for(const auto& widget:document.widgets)presentedTracks+=widget.animations.size();
        };
        if(screenPresenter_->IsLoaded())countPresented(screenController_->AuthoredDocument());
        if(hudSession_.IsLoaded()&&uiComposition_.IsHudVisible())
            countPresented(hudSession_.Controller()->AuthoredDocument());
        diagnosticService_.Observe("ui", {
            {"project_id",startupResult_.project.projectId},
            {"primary_transition_phase",static_cast<std::uint64_t>(screenPresenter_->TransitionPhase())},
            {"primary_exit_action_pending",pendingScreenExit_.has_value()},
            {"hud_transition_phase",hudSession_.Presenter()?static_cast<std::uint64_t>(hudSession_.Presenter()->TransitionPhase()):0},
            {"hud_exit_pending",hudExitRequested_},
            {"hud_transition_error",hudTransitionError_},
            {"reduced_motion",uiAccessibility_.reducedMotion},
            {"text_scale",std::to_string(uiAccessibility_.presentation.textScale)},
            {"ui_scale",std::to_string(uiAccessibility_.presentation.uiScale)},
            {"minimum_target_size",std::to_string(uiAccessibility_.presentation.minimumTargetSize)},
            {"contrast_palette",static_cast<std::uint64_t>(uiAccessibility_.presentation.contrast)},
            {"focus_style",static_cast<std::uint64_t>(uiAccessibility_.presentation.focusStyle)},
            {"focus_width",std::to_string(uiAccessibility_.presentation.focusWidth)},
            {"reduced_motion_primary_applied",screenPresenter_->ReducedMotion()},
            {"reduced_motion_hud_applied",hudSession_.IsLoaded()&&hudSession_.Presenter()->ReducedMotion()},
            {"gameplay_attribute_document_loaded",attributeDocument_.has_value()},
            {"gameplay_attribute_project_instance",gameplayAttributes_.Project()!=nullptr},
            {"gameplay_attribute_player_instance",gameplayAttributes_.Player()!=nullptr},
            {"gameplay_attribute_character_instances",static_cast<std::uint64_t>(gameplayAttributes_.Characters().size())},
            {"gameplay_attribute_diagnostic",attributeDiagnostic_},
            {"sample_attribute_id",sampleAttributeId},
            {"sample_project_value",sampleNumber(gameplayAttributes_.Project())},
            {"sample_player_value",sampleNumber(gameplayAttributes_.Player())},
            {"hud_loaded",hudSession_.IsLoaded()},
            {"hud_visible",uiComposition_.IsHudVisible()},
            {"hud_document_id",hudSession_.Controller()?hudSession_.Controller()->AuthoredDocument().envelope.documentId:std::string{}},
            {"hud_bindings_ready",!hudSession_.Controller()||hudSession_.Controller()->BindingFrame().ready},
            {"hud_binding_issues",static_cast<std::uint64_t>(hudSession_.Controller()?hudSession_.Controller()->BindingFrame().issues.size():0)},
            {"ui_binding_revision",uiGameplayRevision_},
            {"binding_cpu_latest_us",uiBindingCpuTiming_.LatestUs()},
            {"binding_cpu_avg_120_us",uiBindingCpuTiming_.WindowAverageUs()},
            {"binding_cpu_peak_session_us",uiBindingCpuTiming_.PeakUs()},
            {"binding_cpu_window_samples",uiBindingCpuTiming_.WindowCount()},
            {"binding_cpu_over_advisory_2ms_samples",uiBindingCpuTiming_.OverThresholdInWindow(RuntimeUiTimingWindow::AdvisoryBudgetUs)},
            {"binding_cpu_over_budget_samples",uiBindingCpuTiming_.OverBudgetInWindow()},
            {"binding_cpu_budget_us",uiBindingCpuTiming_.BudgetUs()},
            {"binding_cpu_p50_120_us",uiBindingCpuTiming_.PercentileUs(50)},
            {"binding_cpu_p95_120_us",uiBindingCpuTiming_.PercentileUs(95)},
            {"binding_cpu_p99_120_us",uiBindingCpuTiming_.PercentileUs(99)},
            {"performance_budget_diagnostic",uiBudgetDiagnostic_},
            {"presented_document_widgets",presentedWidgets},
            {"presented_document_authored_tracks",presentedTracks},
            {"widget_budget_warning",presentedWidgets>uiBudget_.widgetWarning},
            {"animation_budget_warning",presentedTracks>uiBudget_.animationWarning},
            {"widget_warning_budget",static_cast<std::uint64_t>(uiBudget_.widgetWarning)},
            {"animation_warning_budget",static_cast<std::uint64_t>(uiBudget_.animationWarning)},
            {"ui_gpu_time_measured",false},
            {"ui_resident_memory_measured",false},
            {"ui_input_latency_measured",false},
            {"binding_cpu_budget_provisional_us",RuntimeUiTimingWindow::AdvisoryBudgetUs},
            {"paused",paused_},
            {"player_spawned",player_.IsSpawned()}}, "Runtime/src/RuntimeApplication.cpp");
    }
}
