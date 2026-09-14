#pragma once

#include "renegade/bridge/CharacterService.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include <WickedEngine.h>

namespace renegade::bridge
{
    inline constexpr const char* WeaponAiMetadataKey = "renegade.weapon.ai";
    inline constexpr const char* WeaponAiSchemaMetadataKey = "renegade.weapon.ai.version";
    inline constexpr const char* WeaponAiStyleMetadataKey = "renegade.weapon.ai.style";
    inline constexpr const char* WeaponAiMinRangeMetadataKey = "renegade.weapon.ai.min_range";
    inline constexpr const char* WeaponAiPreferredRangeMetadataKey = "renegade.weapon.ai.preferred_range";
    inline constexpr const char* WeaponAiMaxRangeMetadataKey = "renegade.weapon.ai.max_range";
    inline constexpr const char* WeaponAiDamageMetadataKey = "renegade.weapon.ai.damage";
    inline constexpr const char* WeaponAiFireIntervalMetadataKey = "renegade.weapon.ai.fire_interval";
    inline constexpr const char* WeaponAiReloadSecondsMetadataKey = "renegade.weapon.ai.reload_seconds";
    inline constexpr const char* WeaponAiMagazineSizeMetadataKey = "renegade.weapon.ai.magazine_size";
    inline constexpr const char* WeaponAiReserveAmmoMetadataKey = "renegade.weapon.ai.reserve_ammo";
    inline constexpr const char* WeaponAiBaseAccuracyMetadataKey = "renegade.weapon.ai.base_accuracy";
    inline constexpr int WeaponAiSchemaVersion = 1;

    enum class WeaponAiStyle : std::int32_t
    {
        None = 0,
        Melee,
        Ranged,
        Mixed,
        Custom,
    };

    struct WeaponAiDescriptor
    {
        WeaponAiStyle style = WeaponAiStyle::None;
        float minRange = 0.0f;
        float preferredRange = 0.0f;
        float maxRange = 0.0f;
        float damage = 0.0f;
        float fireIntervalSeconds = 0.5f;
        float reloadSeconds = 0.0f;
        int magazineSize = 0;
        int reserveAmmo = 0;
        float baseAccuracy = 1.0f;
    };

    [[nodiscard]] inline WeaponAiStyle WeaponStyleForCombatStyle(
        const CombatStyle style) noexcept
    {
        switch (style)
        {
        case CombatStyle::Melee: return WeaponAiStyle::Melee;
        case CombatStyle::Ranged: return WeaponAiStyle::Ranged;
        case CombatStyle::Mixed: return WeaponAiStyle::Mixed;
        case CombatStyle::Custom: return WeaponAiStyle::Custom;
        case CombatStyle::None:
        default: return WeaponAiStyle::None;
        }
    }

    [[nodiscard]] inline WeaponAiDescriptor DefaultWeaponAiDescriptor(
        const CombatStyle combatStyle) noexcept
    {
        WeaponAiDescriptor descriptor;
        descriptor.style = WeaponStyleForCombatStyle(combatStyle);
        switch (descriptor.style)
        {
        case WeaponAiStyle::Melee:
            descriptor.minRange = 0.0f;
            descriptor.preferredRange = 1.4f;
            descriptor.maxRange = 2.4f;
            descriptor.damage = 22.0f;
            descriptor.fireIntervalSeconds = 0.75f;
            descriptor.baseAccuracy = 1.0f;
            break;
        case WeaponAiStyle::Ranged:
            descriptor.minRange = 3.0f;
            descriptor.preferredRange = 15.0f;
            descriptor.maxRange = 35.0f;
            descriptor.damage = 18.0f;
            descriptor.fireIntervalSeconds = 0.40f;
            descriptor.reloadSeconds = 2.10f;
            descriptor.magazineSize = 30;
            descriptor.reserveAmmo = 90;
            descriptor.baseAccuracy = 0.78f;
            break;
        case WeaponAiStyle::Mixed:
            descriptor.minRange = 1.0f;
            descriptor.preferredRange = 12.0f;
            descriptor.maxRange = 30.0f;
            descriptor.damage = 16.0f;
            descriptor.fireIntervalSeconds = 0.45f;
            descriptor.reloadSeconds = 2.00f;
            descriptor.magazineSize = 24;
            descriptor.reserveAmmo = 72;
            descriptor.baseAccuracy = 0.74f;
            break;
        case WeaponAiStyle::Custom:
            descriptor.minRange = 2.0f;
            descriptor.preferredRange = 10.0f;
            descriptor.maxRange = 25.0f;
            descriptor.damage = 15.0f;
            descriptor.fireIntervalSeconds = 0.50f;
            descriptor.reloadSeconds = 2.00f;
            descriptor.magazineSize = 20;
            descriptor.reserveAmmo = 60;
            descriptor.baseAccuracy = 0.70f;
            break;
        case WeaponAiStyle::None:
        default:
            break;
        }
        return descriptor;
    }

    [[nodiscard]] inline bool ValidateWeaponAiDescriptor(
        const WeaponAiDescriptor& descriptor,
        std::string& error) noexcept
    {
        const auto finite = [](const float value) noexcept { return std::isfinite(value); };
        if (!finite(descriptor.minRange) || !finite(descriptor.preferredRange) ||
            !finite(descriptor.maxRange) || !finite(descriptor.damage) ||
            !finite(descriptor.fireIntervalSeconds) || !finite(descriptor.reloadSeconds) ||
            !finite(descriptor.baseAccuracy))
        {
            error = "Weapon AI descriptor contains a non-finite value.";
            return false;
        }
        if (descriptor.minRange < 0.0f || descriptor.maxRange < descriptor.minRange ||
            descriptor.preferredRange < descriptor.minRange ||
            descriptor.preferredRange > descriptor.maxRange || descriptor.maxRange > 500.0f)
        {
            error = "Weapon AI descriptor contains invalid range bands.";
            return false;
        }
        if (descriptor.damage < 0.0f || descriptor.damage > 100000.0f ||
            descriptor.fireIntervalSeconds < 0.02f || descriptor.fireIntervalSeconds > 60.0f ||
            descriptor.reloadSeconds < 0.0f || descriptor.reloadSeconds > 120.0f ||
            descriptor.magazineSize < 0 || descriptor.magazineSize > 100000 ||
            descriptor.reserveAmmo < 0 || descriptor.reserveAmmo > 1000000 ||
            descriptor.baseAccuracy < 0.0f || descriptor.baseAccuracy > 1.0f)
        {
            error = "Weapon AI descriptor contains invalid combat values.";
            return false;
        }
        if (descriptor.style == WeaponAiStyle::None &&
            (descriptor.damage > 0.0f || descriptor.maxRange > 0.0f || descriptor.magazineSize > 0))
        {
            error = "A None weapon descriptor cannot deal damage or hold ammunition.";
            return false;
        }
        error.clear();
        return true;
    }

    [[nodiscard]] inline bool CaptureWeaponAiDescriptor(
        const wi::scene::Scene& scene,
        const wi::ecs::Entity weaponEntity,
        const CombatStyle fallbackCombatStyle,
        WeaponAiDescriptor& descriptor,
        std::string& error)
    {
        descriptor = DefaultWeaponAiDescriptor(fallbackCombatStyle);
        if (weaponEntity == wi::ecs::INVALID_ENTITY)
            return ValidateWeaponAiDescriptor(descriptor, error);

        const auto* metadata = scene.metadatas.GetComponent(weaponEntity);
        if (metadata == nullptr || !metadata->string_values.has(WeaponAiMetadataKey))
            return ValidateWeaponAiDescriptor(descriptor, error);
        if (metadata->string_values.get(WeaponAiMetadataKey) != "1")
        {
            error = "Weapon AI metadata contains an unsupported marker value.";
            return false;
        }
        if (!metadata->int_values.has(WeaponAiSchemaMetadataKey) ||
            metadata->int_values.get(WeaponAiSchemaMetadataKey) != WeaponAiSchemaVersion)
        {
            error = "Weapon AI metadata uses an unsupported schema version.";
            return false;
        }

        if (metadata->int_values.has(WeaponAiStyleMetadataKey))
            descriptor.style = static_cast<WeaponAiStyle>(metadata->int_values.get(WeaponAiStyleMetadataKey));
        if (metadata->float_values.has(WeaponAiMinRangeMetadataKey))
            descriptor.minRange = metadata->float_values.get(WeaponAiMinRangeMetadataKey);
        if (metadata->float_values.has(WeaponAiPreferredRangeMetadataKey))
            descriptor.preferredRange = metadata->float_values.get(WeaponAiPreferredRangeMetadataKey);
        if (metadata->float_values.has(WeaponAiMaxRangeMetadataKey))
            descriptor.maxRange = metadata->float_values.get(WeaponAiMaxRangeMetadataKey);
        if (metadata->float_values.has(WeaponAiDamageMetadataKey))
            descriptor.damage = metadata->float_values.get(WeaponAiDamageMetadataKey);
        if (metadata->float_values.has(WeaponAiFireIntervalMetadataKey))
            descriptor.fireIntervalSeconds = metadata->float_values.get(WeaponAiFireIntervalMetadataKey);
        if (metadata->float_values.has(WeaponAiReloadSecondsMetadataKey))
            descriptor.reloadSeconds = metadata->float_values.get(WeaponAiReloadSecondsMetadataKey);
        if (metadata->int_values.has(WeaponAiMagazineSizeMetadataKey))
            descriptor.magazineSize = metadata->int_values.get(WeaponAiMagazineSizeMetadataKey);
        if (metadata->int_values.has(WeaponAiReserveAmmoMetadataKey))
            descriptor.reserveAmmo = metadata->int_values.get(WeaponAiReserveAmmoMetadataKey);
        if (metadata->float_values.has(WeaponAiBaseAccuracyMetadataKey))
            descriptor.baseAccuracy = metadata->float_values.get(WeaponAiBaseAccuracyMetadataKey);

        const auto styleValue = static_cast<std::int32_t>(descriptor.style);
        if (styleValue < static_cast<std::int32_t>(WeaponAiStyle::None) ||
            styleValue > static_cast<std::int32_t>(WeaponAiStyle::Custom))
        {
            error = "Weapon AI metadata contains an unsupported style.";
            return false;
        }
        return ValidateWeaponAiDescriptor(descriptor, error);
    }
}
