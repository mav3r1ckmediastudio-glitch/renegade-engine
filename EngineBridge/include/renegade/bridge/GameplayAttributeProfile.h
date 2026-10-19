#pragma once
#include "renegade/bridge/IdentityService.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace renegade::bridge
{
    // Optional gameplay module. Native combat/ammunition are adapted separately;
    // putting an attribute on a UI element never creates gameplay authority.
    using GameplayAttributeValue = std::variant<double, bool, std::string>;
    struct GameplayAttributeDefinition
    {
        std::string id;
        std::string name;
        GameplayAttributeValue initial = 0.0;
        std::optional<double> minimum;
        std::optional<double> maximum;
        double regeneratePerSecond = 0.0;
    };
    struct GameplayAttributeProfile
    {
        StableId id;
        std::string name;
        std::vector<GameplayAttributeDefinition> attributes;
    };
    struct GameplayAttributeOverride
    {
        std::string attributeId;
        std::optional<GameplayAttributeValue> initial;
        std::optional<double> minimum;
        std::optional<double> maximum;
        std::optional<double> regeneratePerSecond;
    };

    inline bool ValidateGameplayAttributeDefinition(
        const GameplayAttributeDefinition& definition, std::string& error)
    {
        if (definition.id.empty() || definition.id.size() > 128 ||
            definition.name.empty() || definition.name.size() > 256)
        { error = "Attribute identity/name is missing or too long."; return false; }
        for (unsigned char c : definition.id)
            if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') &&
                c != '.' && c != '_' && c != '-')
            { error = "Attribute identity contains invalid characters."; return false; }
        const auto* number = std::get_if<double>(&definition.initial);
        const auto finite = [](const std::optional<double>& value) {
            return !value || std::isfinite(*value);
        };
        if (!finite(definition.minimum) || !finite(definition.maximum) ||
            !std::isfinite(definition.regeneratePerSecond) ||
            (number && !std::isfinite(*number)) ||
            (definition.minimum && definition.maximum &&
                *definition.minimum > *definition.maximum))
        { error = "Attribute bounds/value/rule are invalid."; return false; }
        if (!number && (definition.minimum || definition.maximum ||
            definition.regeneratePerSecond != 0))
        { error = "Numeric bounds and regeneration require a numeric attribute."; return false; }
        if (number && ((definition.minimum && *number < *definition.minimum) ||
            (definition.maximum && *number > *definition.maximum)))
        { error = "Attribute initial value is outside its bounds."; return false; }
        if (const auto* text = std::get_if<std::string>(&definition.initial);
            text && text->size() > 4096)
        { error = "Attribute text exceeds its limit."; return false; }
        error.clear();
        return true;
    }

    inline bool ResolveGameplayAttributeProfile(
        const GameplayAttributeProfile& profile,
        const std::vector<GameplayAttributeOverride>& overrides,
        std::map<std::string, GameplayAttributeDefinition>& resolved,
        std::string& error)
    {
        if (!IsValidStableId(profile.id) || profile.name.empty() ||
            profile.attributes.size() > 256 || overrides.size() > 256)
        { error = "Attribute profile identity or count is invalid."; return false; }
        std::map<std::string, GameplayAttributeDefinition> candidate;
        for (const auto& definition : profile.attributes)
        {
            if (!ValidateGameplayAttributeDefinition(definition, error)) return false;
            if (!candidate.emplace(definition.id, definition).second)
            { error = "Duplicate attribute identity."; return false; }
        }
        std::map<std::string, bool> visited;
        for (const auto& value : overrides)
        {
            auto it = candidate.find(value.attributeId);
            if (it == candidate.end() || !visited.emplace(value.attributeId, true).second)
            { error = "Override references an absent or duplicate attribute."; return false; }
            auto& target = it->second;
            if (value.initial)
            {
                if (value.initial->index() != target.initial.index())
                { error = "Attribute override changes its type."; return false; }
                target.initial = *value.initial;
            }
            if (value.minimum) target.minimum = value.minimum;
            if (value.maximum) target.maximum = value.maximum;
            if (value.regeneratePerSecond) target.regeneratePerSecond = *value.regeneratePerSecond;
            if (!ValidateGameplayAttributeDefinition(target,error)) return false;
        }
        resolved = std::move(candidate);
        error.clear();
        return true;
    }

    class GameplayAttributeInstance
    {
    public:
        bool ApplyProfile(const GameplayAttributeProfile& profile,
            const std::vector<GameplayAttributeOverride>& overrides,
            bool reset, std::string& error)
        {
            std::map<std::string, GameplayAttributeDefinition> definitions;
            if (!ResolveGameplayAttributeProfile(profile, overrides, definitions, error))
                return false;
            std::map<std::string, GameplayAttributeValue> next;
            for (const auto& [id, definition] : definitions)
            {
                auto value = definition.initial;
                const auto old = values_.find(id);
                if (!reset && old != values_.end())
                {
                    if (old->second.index() != definition.initial.index())
                    { error = "A live attribute cannot change type without reset."; return false; }
                    value = old->second;
                }
                if (auto* n = std::get_if<double>(&value))
                {
                    if (definition.minimum) *n = std::max(*n,*definition.minimum);
                    if (definition.maximum) *n = std::min(*n,*definition.maximum);
                }
                next.emplace(id,std::move(value));
            }
            definitions_ = std::move(definitions);
            values_ = std::move(next);
            ++revision_;
            error.clear();
            return true;
        }

        bool Set(const std::string& id, GameplayAttributeValue value, std::string& error)
        {
            const auto definition = definitions_.find(id);
            if (definition == definitions_.end() || value.index() != definition->second.initial.index())
            { error = "Attribute source or type is invalid."; return false; }
            auto check = definition->second;
            check.initial = value;
            if (!ValidateGameplayAttributeDefinition(check,error)) return false;
            values_.at(id) = std::move(value);
            ++revision_;
            error.clear();
            return true;
        }

        bool Spend(const std::string& id, double cost, std::string& error)
        {
            auto found = values_.find(id);
            if (!std::isfinite(cost) || cost < 0 || found == values_.end() ||
                !std::holds_alternative<double>(found->second))
            { error = "Attribute cost/source is invalid."; return false; }
            return Set(id,std::get<double>(found->second)-cost,error);
        }

        bool Tick(double seconds, bool paused, std::string& error)
        {
            if (!std::isfinite(seconds) || seconds < 0 || seconds > 60)
            { error = "Attribute tick must be finite and between zero and sixty seconds."; return false; }
            if (paused || seconds == 0) { error.clear(); return true; }
            auto next = values_;
            bool changed = false;
            for (const auto& [id, definition] : definitions_)
            {
                auto* n = std::get_if<double>(&next.at(id));
                if (!n || definition.regeneratePerSecond == 0) continue;
                double result = *n + definition.regeneratePerSecond * seconds;
                if (!std::isfinite(result)) { error = "Attribute rule overflow."; return false; }
                if (definition.minimum) result = std::max(result,*definition.minimum);
                if (definition.maximum) result = std::min(result,*definition.maximum);
                changed |= result != *n;
                *n = result;
            }
            if (changed) { values_ = std::move(next); ++revision_; }
            error.clear();
            return true;
        }
        const GameplayAttributeValue* Find(const std::string& id) const noexcept
        {
            const auto it=values_.find(id);
            return it==values_.end() ? nullptr : &it->second;
        }
        std::uint64_t Revision() const noexcept { return revision_; }
    private:
        std::map<std::string,GameplayAttributeDefinition> definitions_;
        std::map<std::string,GameplayAttributeValue> values_;
        std::uint64_t revision_ = 0;
    };
}
