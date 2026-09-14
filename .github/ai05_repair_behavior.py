from pathlib import Path
import runpy

runpy.run_path(".github/ai05_repair_final.py", run_name="__main__")


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


path = "Runtime/src/RuntimeCombatDecision.h"
text = read(path)
text = replace_once(
    text,
    '''        const bool civilianLike =
            character.authoring.role == bridge::CharacterRole::Civilian ||
            character.authoring.role == bridge::CharacterRole::Passive;

        if (hostileKnowledge && health <= character.tuning.retreatHealthThreshold)
''',
    '''        const bool civilianLike =
            character.authoring.role == bridge::CharacterRole::Civilian ||
            character.authoring.role == bridge::CharacterRole::Passive;
        const bool usableWeapon = HasUsableWeapon(combat);

        // Unarmed/non-viable Characters must not inherit AI-04's hostile Chase
        // as their best response. Civilians/passive creatures flee by default;
        // other Characters retreat or surrender according to authored traits.
        if (hostileKnowledge && !usableWeapon)
        {
            if (character.authoring.canFlee &&
                (civilianLike || courage < 0.40f))
            {
                scores.push_back({
                    CharacterIntent::Flee,
                    148.0f + (1.0f - courage) * 24.0f});
            }
            else if (character.authoring.canFlee)
            {
                scores.push_back({
                    CharacterIntent::Retreat,
                    124.0f + (1.0f - courage) * 18.0f});
            }
            if (character.authoring.canSurrender && courage < 0.55f)
            {
                scores.push_back({
                    CharacterIntent::Surrender,
                    128.0f + (1.0f - courage) * 20.0f});
            }
        }

        if (hostileKnowledge && health <= character.tuning.retreatHealthThreshold)
''',
    "unarmed hostile response")
text = text.replace(
    '        if (!hostileKnowledge || !HasUsableWeapon(combat))\n',
    '        if (!hostileKnowledge || !usableWeapon)\n')
text = replace_once(
    text,
    '''            combat.weapon.style != bridge::WeaponAiStyle::Melee)
''',
    '''            combat.weapon.style == bridge::WeaponAiStyle::Ranged)
''',
    "mixed-style close-range behavior")
write(path, text)

path = "Tests/CharacterAiCombatTests.cpp"
text = read(path)
needle = '''    character.authoring.role = CharacterRole::Civilian;
    character.authoring.personality = PersonalityPreset::Timid;
    character.authoring.canFlee = true;
    character.authoring.canSurrender = true;
    character.tuning = ResolveCharacterTuning(character.authoring);
    combat->health = 5.0f;
'''
replacement = '''    character.authoring.role = CharacterRole::Civilian;
    character.authoring.personality = PersonalityPreset::Timid;
    character.authoring.canFlee = true;
    character.authoring.canSurrender = true;
    character.tuning = ResolveCharacterTuning(character.authoring);
    combat->health = 100.0f;
    combat->maxHealth = 100.0f;
    combat->magazineAmmo = 0;
    combat->reserveAmmo = 0;
    combat->weapon = DefaultWeaponAiDescriptor(CombatStyle::Ranged);
    combat->effectiveRange = ResolveEffectiveWeaponAiRange(
        character.tuning, combat->weapon);
    const auto healthyCivilianScores = ScoreCharacterCombatIntents(
        character, perception.characters.front(), decision, *combat);
    if (!ContainsIntent(healthyCivilianScores, CharacterIntent::Flee))
        return Fail("healthy unarmed civilian should flee from hostile knowledge");

    combat->health = 5.0f;
'''
text = replace_once(text, needle, replacement, "healthy civilian flee regression")
write(path, text)

path = "Tests/CharacterAiCombatSourceContract.cmake"
text = read(path)
text = replace_once(
    text,
    'require_text("${tests}" "runtime-player combat event must be publicly deliverable broadcast" "public combat event routing regression")\n',
    'require_text("${tests}" "runtime-player combat event must be publicly deliverable broadcast" "public combat event routing regression")\nrequire_text("${tests}" "healthy unarmed civilian should flee from hostile knowledge" "unarmed civilian flee regression")\n',
    "source contract civilian flee regression")
write(path, text)

print("AI-05 unarmed/mixed threat-response audit repairs applied")
