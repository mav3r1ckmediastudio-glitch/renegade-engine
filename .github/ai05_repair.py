from pathlib import Path


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


# 1. Public combat events must use RuntimeScriptRuntime's existing bounded
# GameplayEventService FIFO rather than a parallel RuntimeApplication queue.
path = "Runtime/src/RuntimeScriptRuntime.h"
text = read(path)
text = replace_once(
    text,
    '#include "renegade/bridge/GameplayInputService.h"\n',
    '#include "renegade/bridge/GameplayInputService.h"\n#include "renegade/bridge/GameplayEventService.h"\n',
    "RuntimeScriptRuntime event include")
text = replace_once(
    text,
    '        void SetGameplayState(\n            const bridge::RuntimePlayerState* player,\n            const bridge::GameplayInputFrame* input) noexcept;\n\n        void Update(float dt) noexcept;\n',
    '        void SetGameplayState(\n            const bridge::RuntimePlayerState* player,\n            const bridge::GameplayInputFrame* input) noexcept;\n\n        // Engine/runtime producers publish through the same bounded FIFO that\n        // governed creator scripts consume in Update(). This is the single\n        // public/cross-system GameplayEventService boundary.\n        [[nodiscard]] bool EnqueueGameplayEvent(\n            bridge::GameplayEvent event,\n            std::string& error);\n\n        void Update(float dt) noexcept;\n',
    "RuntimeScriptRuntime event API")
write(path, text)

path = "Runtime/src/RuntimeScriptRuntime.cpp"
text = read(path)
text = replace_once(
    text,
    '    void RuntimeScriptRuntime::SetGameplayState(\n        const bridge::RuntimePlayerState* player,\n        const bridge::GameplayInputFrame* input) noexcept\n    {\n        impl_->playerState = player;\n        impl_->gameplayInput = input;\n    }\n\n    void RuntimeScriptRuntime::Update(const float dt) noexcept\n',
    '    void RuntimeScriptRuntime::SetGameplayState(\n        const bridge::RuntimePlayerState* player,\n        const bridge::GameplayInputFrame* input) noexcept\n    {\n        impl_->playerState = player;\n        impl_->gameplayInput = input;\n    }\n\n    bool RuntimeScriptRuntime::EnqueueGameplayEvent(\n        bridge::GameplayEvent event,\n        std::string& error)\n    {\n        if (impl_ == nullptr)\n        {\n            error = "Governed Runtime script event queue is unavailable.";\n            return false;\n        }\n        return impl_->gameplayEvents.Enqueue(std::move(event), error);\n    }\n\n    void RuntimeScriptRuntime::Update(const float dt) noexcept\n',
    "RuntimeScriptRuntime event implementation")
write(path, text)

path = "Runtime/src/RuntimeApplication.h"
text = read(path)
text = replace_once(
    text,
    '        RuntimeCombatState combatState_;\n        bridge::GameplayEventService combatEventService_;\n',
    '        RuntimeCombatState combatState_;\n',
    "remove parallel combat event service")
write(path, text)

path = "Runtime/src/RuntimeLiveDiagnostics.cpp"
text = read(path)
if text.count('            combatEventService_.Clear();\n') != 2:
    raise RuntimeError("expected exactly two combatEventService clear sites")
text = text.replace('            combatEventService_.Clear();\n', '')
start = text.index('            const CombatEventEmitter combatEmitter = [this](\n')
end_marker = '            };\n            UpdateRuntimeCombatDecision(\n'
end = text.index(end_marker, start)
new_emitter = '''            const CombatEventEmitter combatEmitter = [this](
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
'''
text = text[:start] + new_emitter + text[end:]
text = text.replace(
    '{"combat_event_queue_depth", static_cast<std::uint64_t>(combatEventService_.Size())},\n            {"combat_event_dropped", static_cast<std::uint64_t>(combatEventService_.DroppedCount())},',
    '{"gameplay_event_queue_depth", static_cast<std::uint64_t>(creatorScripts_.PendingEventCount())},\n            {"gameplay_event_dropped", static_cast<std::uint64_t>(creatorScripts_.DroppedEventCount())},')
if 'combatEventService_' in text:
    raise RuntimeError("parallel combat event service reference remains")
write(path, text)


# 2. AI-02 Character tactical ranges must compose with AI-05 weapon hard
# capabilities. A profile may choose a narrower operating band but never extend
# beyond the physical weapon descriptor.
path = "EngineBridge/include/renegade/bridge/WeaponCombatService.h"
text = read(path)
text = replace_once(
    text,
    '#include "renegade/bridge/CharacterService.h"\n',
    '#include "renegade/bridge/CharacterProfileService.h"\n',
    "WeaponCombatService profile include")
text = replace_once(
    text,
    '    struct WeaponAiDescriptor\n    {\n        WeaponAiStyle style = WeaponAiStyle::None;\n        float minRange = 0.0f;\n        float preferredRange = 0.0f;\n        float maxRange = 0.0f;\n        float damage = 0.0f;\n        float fireIntervalSeconds = 0.5f;\n        float reloadSeconds = 0.0f;\n        int magazineSize = 0;\n        int reserveAmmo = 0;\n        float baseAccuracy = 1.0f;\n    };\n',
    '    struct WeaponAiDescriptor\n    {\n        WeaponAiStyle style = WeaponAiStyle::None;\n        float minRange = 0.0f;\n        float preferredRange = 0.0f;\n        float maxRange = 0.0f;\n        float damage = 0.0f;\n        float fireIntervalSeconds = 0.5f;\n        float reloadSeconds = 0.0f;\n        int magazineSize = 0;\n        int reserveAmmo = 0;\n        float baseAccuracy = 1.0f;\n    };\n\n    struct WeaponAiRangeBand\n    {\n        float minRange = 0.0f;\n        float preferredRange = 0.0f;\n        float maxRange = 0.0f;\n    };\n\n    [[nodiscard]] inline WeaponAiRangeBand ResolveEffectiveWeaponAiRange(\n        const CharacterTuning& tuning,\n        const WeaponAiDescriptor& weapon) noexcept\n    {\n        WeaponAiRangeBand range;\n        if (weapon.style == WeaponAiStyle::None || weapon.maxRange <= 0.0f ||\n            tuning.maxCombatRange <= 0.0f)\n        {\n            return range;\n        }\n\n        range.maxRange = std::max(0.0f, std::min(\n            weapon.maxRange, tuning.maxCombatRange));\n        range.minRange = std::min(\n            range.maxRange,\n            std::max(weapon.minRange, tuning.minCombatRange));\n        range.preferredRange = std::clamp(\n            tuning.preferredCombatRange,\n            range.minRange,\n            range.maxRange);\n        return range;\n    }\n',
    "effective range composition")
write(path, text)

path = "Runtime/src/RuntimeCombatService.h"
text = read(path)
text = replace_once(
    text,
    '        bridge::WeaponAiDescriptor weapon;\n        float maxHealth = 100.0f;\n',
    '        bridge::WeaponAiDescriptor weapon;\n        bridge::WeaponAiRangeBand effectiveRange;\n        float maxHealth = 100.0f;\n',
    "combat record effective range")
text = replace_once(
    text,
    '        if (combat.dead || combat.weapon.style == bridge::WeaponAiStyle::None ||\n            combat.weapon.damage <= 0.0f || combat.weapon.maxRange <= 0.0f)\n',
    '        if (combat.dead || combat.weapon.style == bridge::WeaponAiStyle::None ||\n            combat.weapon.damage <= 0.0f || combat.effectiveRange.maxRange <= 0.0f)\n',
    "usable weapon effective range")
text = replace_once(
    text,
    '            combat.maxHealth = nativeCharacter->health > 0\n',
    '            combat.effectiveRange = bridge::ResolveEffectiveWeaponAiRange(\n                character.tuning, combat.weapon);\n\n            combat.maxHealth = nativeCharacter->health > 0\n',
    "initialize effective range")
text = replace_once(
    text,
    '        EmitSoundStimulus(\n            perception,\n',
    '        (void)EmitSoundStimulus(\n            perception,\n',
    "nodiscard sound stimulus")
write(path, text)

path = "Runtime/src/RuntimeCombatDecision.h"
text = read(path)
text = text.replace('combat.weapon.preferredRange * 1.20f', 'combat.effectiveRange.preferredRange * 1.20f')
text = text.replace('combat.weapon.maxRange', 'combat.effectiveRange.maxRange')
text = text.replace('combat.weapon.minRange', 'combat.effectiveRange.minRange')
text = text.replace('combat.weapon.preferredRange', 'combat.effectiveRange.preferredRange')
write(path, text)

# Runtime diagnostics and creator UI must report the same effective tactical
# range that the decision layer uses.
path = "Runtime/src/RuntimeLiveDiagnostics.cpp"
text = read(path)
text = text.replace(
    '                std::to_string(first.weapon.minRange) + "," +\n                std::to_string(first.weapon.preferredRange) + "," +\n                std::to_string(first.weapon.maxRange);',
    '                std::to_string(first.effectiveRange.minRange) + "," +\n                std::to_string(first.effectiveRange.preferredRange) + "," +\n                std::to_string(first.effectiveRange.maxRange);')
write(path, text)

path = "Studio/src/AICombatInspector.cpp"
text = read(path)
old = '''                status_.SetText(settings.weaponEntityId.empty()
                    ? "Style defaults active // assign a governed weapon entity optionally"
                    : "Weapon assigned by stable identity");
                descriptor_.SetText(
                    "RANGE " + std::to_string(weaponDescriptor.minRange) + " / " +
                    std::to_string(weaponDescriptor.preferredRange) + " / " +
                    std::to_string(weaponDescriptor.maxRange) + "m   DAMAGE " +
                    std::to_string(weaponDescriptor.damage) + "   AMMO " +
                    std::to_string(weaponDescriptor.magazineSize) + "+" +
                    std::to_string(weaponDescriptor.reserveAmmo));
'''
new = '''                bridge::CharacterAdvancedOverrides overrides;
                if (!bridge::CaptureCharacterAdvancedOverrides(
                        scene, selectedCharacter_, overrides, error))
                {
                    status_.SetText("INVALID ADVANCED AI // " + error);
                    descriptor_.SetText("Combat range cannot resolve until Advanced AI is repaired.");
                    return;
                }
                const auto tuning = bridge::ResolveCharacterTuning(settings, overrides);
                const auto effectiveRange = bridge::ResolveEffectiveWeaponAiRange(
                    tuning, weaponDescriptor);

                status_.SetText(settings.weaponEntityId.empty()
                    ? "Style defaults active // assign a governed weapon entity optionally"
                    : "Weapon assigned by stable identity");
                descriptor_.SetText(
                    "EFFECTIVE RANGE " + std::to_string(effectiveRange.minRange) + " / " +
                    std::to_string(effectiveRange.preferredRange) + " / " +
                    std::to_string(effectiveRange.maxRange) + "m   WEAPON CAP " +
                    std::to_string(weaponDescriptor.maxRange) + "m   DAMAGE " +
                    std::to_string(weaponDescriptor.damage) + "   AMMO " +
                    std::to_string(weaponDescriptor.magazineSize) + "+" +
                    std::to_string(weaponDescriptor.reserveAmmo));
'''
text = replace_once(text, old, new, "Combat Inspector effective range")
write(path, text)


# 3. Repair focused Windows fixture calls against the pinned Wicked API and add
# a regression proving AI-02 tactical range survives weapon assignment.
path = "Tests/CharacterAiCombatTests.cpp"
text = read(path)
text = replace_once(
    text,
    'scene.Entity_CreateTransform(\n        "AI05 Character", XMFLOAT3(0.0f, 0.0f, 0.0f));',
    'scene.Entity_CreateTransform("AI05 Character");',
    "character transform fixture")
text = replace_once(
    text,
    'scene.Entity_CreateTransform(\n        "AI05 Rifle", XMFLOAT3(0.0f, 0.0f, 0.0f));',
    'scene.Entity_CreateTransform("AI05 Rifle");',
    "weapon transform fixture")
text = replace_once(
    text,
    '    character.tuning = ResolveCharacterTuning(character.authoring);\n    characterSystem.characters.push_back(character);\n',
    '    character.tuning = ResolveCharacterTuning(character.authoring);\n    character.tuning.minCombatRange = 4.0f;\n    character.tuning.preferredCombatRange = 11.0f;\n    character.tuning.maxCombatRange = 20.0f;\n    characterSystem.characters.push_back(character);\n',
    "combat test tactical range fixture")
text = replace_once(
    text,
    '    if (combat == nullptr || combat->magazineAmmo != 12 || combat->reserveAmmo != 24)\n        return Fail("combat ammo initialization");\n',
    '    if (combat == nullptr || combat->magazineAmmo != 12 || combat->reserveAmmo != 24)\n        return Fail("combat ammo initialization");\n    if (std::abs(combat->effectiveRange.minRange - 4.0f) > 0.001f ||\n        std::abs(combat->effectiveRange.preferredRange - 11.0f) > 0.001f ||\n        std::abs(combat->effectiveRange.maxRange - 20.0f) > 0.001f)\n        return Fail("effective combat range composes profile preference with weapon capability");\n',
    "combat effective range regression")
write(path, text)


# 4. Harden the source contract so these two audit findings cannot regress.
path = "Tests/CharacterAiCombatSourceContract.cmake"
text = read(path)
text = replace_once(
    text,
    'read_required("Runtime/src/RuntimeApplication.h" app)\nread_required("Runtime/src/RuntimeLiveDiagnostics.cpp" runtime)\n',
    'read_required("Runtime/src/RuntimeApplication.h" app)\nread_required("Runtime/src/RuntimeScriptRuntime.h" script_runtime_h)\nread_required("Runtime/src/RuntimeScriptRuntime.cpp" script_runtime_cpp)\nread_required("Runtime/src/RuntimeLiveDiagnostics.cpp" runtime)\n',
    "source contract script runtime reads")
text = replace_once(
    text,
    'require_text("${weapon}" "baseAccuracy" "AI accuracy semantics")\n',
    'require_text("${weapon}" "baseAccuracy" "AI accuracy semantics")\nrequire_text("${weapon}" "ResolveEffectiveWeaponAiRange" "profile + weapon effective range composition")\nrequire_text("${weapon}" "tuning.maxCombatRange" "profile combat-range ceiling")\nrequire_text("${weapon}" "tuning.preferredCombatRange" "profile preferred combat range")\n',
    "source contract effective range")
text = replace_once(
    text,
    'require_text("${app}" "RuntimeCombatState combatState_" "Runtime combat ownership")\nrequire_text("${app}" "GameplayEventService combatEventService_" "existing event-service instance")\n',
    'require_text("${app}" "RuntimeCombatState combatState_" "Runtime combat ownership")\nforbid_text("${app}" "combatEventService_" "parallel combat gameplay-event queue")\nrequire_text("${script_runtime_h}" "EnqueueGameplayEvent" "shared Runtime gameplay-event producer seam")\nrequire_text("${script_runtime_cpp}" "impl_->gameplayEvents.Enqueue" "combat/script events share existing GameplayEventService")\n',
    "source contract single event bus")
text = text.replace(
    'require_text("${runtime}" "GameplayEventService contract" "combat event integration note")',
    'require_text("${runtime}" "creatorScripts_.EnqueueGameplayEvent" "combat events enter existing script/runtime event bus")')
text = replace_once(
    text,
    'require_text("${tests}" "NPC native health synchronization" "native health regression")\n',
    'require_text("${tests}" "NPC native health synchronization" "native health regression")\nrequire_text("${tests}" "effective combat range composes profile preference with weapon capability" "profile/weapon range regression")\n',
    "source contract range regression")
write(path, text)

print("AI-05 audit repairs applied")
