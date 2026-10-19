#pragma once

#include "renegade/bridge/GameplayAttributeProfile.h"
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>

namespace renegade::bridge
{
    enum class ScreenBindingProperty { Text, Fill, Visible, Enabled };
    enum class ScreenBindingOperation { Direct, Ratio, AtLeast, AtMost, Equals };
    enum class ScreenBindingMissingPolicy { AuthoredFallback, Hide };
    enum class ScreenBindingScope { PossessedPlayer, Character, ProjectAttribute };

    struct ScreenBindingSource
    {
        ScreenBindingScope scope = ScreenBindingScope::PossessedPlayer;
        StableId targetId;
        std::string attributeId;
        bool operator==(const ScreenBindingSource& other) const noexcept
        {
            return scope == other.scope && targetId == other.targetId &&
                attributeId == other.attributeId;
        }
    };

    struct ScreenBinding
    {
        ScreenBindingProperty property = ScreenBindingProperty::Text;
        ScreenBindingSource source;
        ScreenBindingOperation operation = ScreenBindingOperation::Direct;
        ScreenBindingSource denominator;
        GameplayAttributeValue comparison = 0.0;
        ScreenBindingMissingPolicy missingPolicy = ScreenBindingMissingPolicy::AuthoredFallback;
        bool required = true;
        int decimalPlaces = 0;
        std::string prefix;
        std::string suffix;
    };

    struct ScreenBindingSample
    {
        ScreenBindingSource source;
        GameplayAttributeValue value = 0.0;
        // A stable entity identity is insufficient after despawn/recreation.
        std::uint64_t generation = 0;
    };

    struct ScreenBindingSnapshot
    {
        std::uint64_t session = 0;
        std::uint64_t revision = 0;
        std::vector<ScreenBindingSample> values;
    };

    enum class ScreenBindingDiagnostic
    {
        None, InvalidBinding, MissingSource, DuplicateSource, StaleSession,
        StaleGeneration, WrongType, NonFinite, ZeroDenominator
    };

    struct ScreenBindingResult
    {
        GameplayAttributeValue value = 0.0;
        ScreenBindingDiagnostic diagnostic = ScreenBindingDiagnostic::None;
        std::string message;
        bool resolved = false;
        bool hide = false;
    };

    inline bool ValidateScreenBindingSource(const ScreenBindingSource& source, std::string& error)
    {
        if (source.scope != ScreenBindingScope::PossessedPlayer &&
            source.scope != ScreenBindingScope::Character &&
            source.scope != ScreenBindingScope::ProjectAttribute)
        { error = "Binding source scope is invalid."; return false; }
        if ((source.scope == ScreenBindingScope::Character && !IsValidStableId(source.targetId)) ||
            (source.scope != ScreenBindingScope::Character && !source.targetId.empty()))
        { error = "Binding target must match its explicit source scope."; return false; }
        GameplayAttributeDefinition check;
        check.id = source.attributeId;
        check.name = source.attributeId;
        return ValidateGameplayAttributeDefinition(check, error);
    }

    inline bool ValidateScreenBinding(const ScreenBinding& binding, std::string& error)
    {
        if (!ValidateScreenBindingSource(binding.source, error)) return false;
        if (binding.property != ScreenBindingProperty::Text &&
            binding.property != ScreenBindingProperty::Fill &&
            binding.property != ScreenBindingProperty::Visible &&
            binding.property != ScreenBindingProperty::Enabled)
        { error = "Binding target property is invalid."; return false; }
        if (binding.operation != ScreenBindingOperation::Direct &&
            binding.operation != ScreenBindingOperation::Ratio &&
            binding.operation != ScreenBindingOperation::AtLeast &&
            binding.operation != ScreenBindingOperation::AtMost &&
            binding.operation != ScreenBindingOperation::Equals)
        { error = "Binding operation is invalid."; return false; }
        if (binding.missingPolicy != ScreenBindingMissingPolicy::AuthoredFallback &&
            binding.missingPolicy != ScreenBindingMissingPolicy::Hide)
        { error = "Binding missing-source policy is invalid."; return false; }
        if (binding.decimalPlaces < 0 || binding.decimalPlaces > 6 ||
            binding.prefix.size() > 256 || binding.suffix.size() > 256)
        { error = "Binding formatter exceeds its bounds."; return false; }
        const bool condition = binding.operation == ScreenBindingOperation::AtLeast ||
            binding.operation == ScreenBindingOperation::AtMost ||
            binding.operation == ScreenBindingOperation::Equals;
        const bool boolTarget = binding.property == ScreenBindingProperty::Visible ||
            binding.property == ScreenBindingProperty::Enabled;
        if (condition && !boolTarget)
        { error = "A condition requires a visible/enabled target."; return false; }
        if (binding.operation == ScreenBindingOperation::Ratio)
        {
            if (boolTarget || !ValidateScreenBindingSource(binding.denominator, error))
            { if (error.empty()) error = "A ratio requires a numeric or text target."; return false; }
        }
        if ((binding.operation == ScreenBindingOperation::AtLeast ||
             binding.operation == ScreenBindingOperation::AtMost) &&
            !std::holds_alternative<double>(binding.comparison))
        { error = "Numeric comparison requires a numeric threshold."; return false; }
        if (const auto* n = std::get_if<double>(&binding.comparison); n && !std::isfinite(*n))
        { error = "Binding threshold must be finite."; return false; }
        if (const auto* text = std::get_if<std::string>(&binding.comparison); text && text->size() > 4096)
        { error = "Binding threshold text is too long."; return false; }
        error.clear();
        return true;
    }

    // Evaluates an immutable, coherent gameplay-boundary snapshot. It neither
    // reads ECS nor creates gameplay values. The caller owns session/generation.
    inline ScreenBindingResult EvaluateScreenBinding(const ScreenBinding& binding,
        const ScreenBindingSnapshot& snapshot, std::uint64_t expectedSession,
        std::uint64_t expectedGeneration, GameplayAttributeValue fallback,
        std::uint64_t denominatorGeneration = 0)
    {
        ScreenBindingResult result;
        result.value = std::move(fallback);
        const auto fail = [&](ScreenBindingDiagnostic diagnostic, const char* message)
        {
            result.diagnostic = diagnostic;
            result.message = message;
            result.hide = binding.missingPolicy == ScreenBindingMissingPolicy::Hide;
            return result;
        };
        std::string validation;
        if (!ValidateScreenBinding(binding, validation))
        { result.message = validation; result.diagnostic = ScreenBindingDiagnostic::InvalidBinding; return result; }
        if (snapshot.session == 0 || snapshot.session != expectedSession)
            return fail(ScreenBindingDiagnostic::StaleSession, "Binding snapshot belongs to a different Runtime session.");
        if (snapshot.values.size() > 4096)
            return fail(ScreenBindingDiagnostic::InvalidBinding, "Binding snapshot exceeds its source budget.");
        const auto lookup = [&](const ScreenBindingSource& source, std::uint64_t generation,
            ScreenBindingDiagnostic& diagnostic)
            -> const ScreenBindingSample*
        {
            const ScreenBindingSample* found = nullptr;
            for (const auto& sample : snapshot.values)
            {
                if (!(sample.source == source)) continue;
                if (found) { diagnostic = ScreenBindingDiagnostic::DuplicateSource; return nullptr; }
                found = &sample;
            }
            if (!found) { diagnostic = ScreenBindingDiagnostic::MissingSource; return nullptr; }
            if (source.scope != ScreenBindingScope::ProjectAttribute &&
                (generation == 0 || found->generation != generation))
            { diagnostic = ScreenBindingDiagnostic::StaleGeneration; return nullptr; }
            if (const auto* number = std::get_if<double>(&found->value); number && !std::isfinite(*number))
            { diagnostic = ScreenBindingDiagnostic::NonFinite; return nullptr; }
            if (const auto* text = std::get_if<std::string>(&found->value); text && text->size() > 4096)
            { diagnostic = ScreenBindingDiagnostic::WrongType; return nullptr; }
            return found;
        };
        ScreenBindingDiagnostic diagnostic = ScreenBindingDiagnostic::None;
        const auto* sample = lookup(binding.source, expectedGeneration, diagnostic);
        if (!sample) return fail(diagnostic, "Binding source is missing, stale or invalid.");
        GameplayAttributeValue value = sample->value;
        if (binding.operation == ScreenBindingOperation::Ratio)
        {
            if (denominatorGeneration == 0 &&
                binding.denominator.scope == binding.source.scope &&
                binding.denominator.targetId == binding.source.targetId)
                denominatorGeneration = expectedGeneration;
            const auto* denominator = lookup(binding.denominator, denominatorGeneration, diagnostic);
            if (!denominator) return fail(diagnostic, "Binding denominator is missing, stale or invalid.");
            const auto* a = std::get_if<double>(&value);
            const auto* b = std::get_if<double>(&denominator->value);
            if (!a || !b) return fail(ScreenBindingDiagnostic::WrongType, "Binding ratio requires two numbers.");
            if (*b == 0) return fail(ScreenBindingDiagnostic::ZeroDenominator, "Binding ratio denominator is zero.");
            const double ratio = *a / *b;
            if (!std::isfinite(ratio)) return fail(ScreenBindingDiagnostic::NonFinite, "Binding ratio overflow.");
            value = ratio;
        }
        else if (binding.operation != ScreenBindingOperation::Direct)
        {
            if (value.index() != binding.comparison.index())
                return fail(ScreenBindingDiagnostic::WrongType, "Binding condition type changed.");
            if (binding.operation == ScreenBindingOperation::Equals) value = value == binding.comparison;
            else
            {
                const auto* number = std::get_if<double>(&value);
                const auto* threshold = std::get_if<double>(&binding.comparison);
                if (!number || !threshold)
                    return fail(ScreenBindingDiagnostic::WrongType, "Binding comparison requires numbers.");
                value = binding.operation == ScreenBindingOperation::AtLeast ?
                    *number >= *threshold : *number <= *threshold;
            }
        }
        if (binding.property == ScreenBindingProperty::Fill)
        {
            auto* number = std::get_if<double>(&value);
            if (!number) return fail(ScreenBindingDiagnostic::WrongType, "Fill requires a numeric binding.");
            *number = std::clamp(*number, 0.0, 1.0);
        }
        else if (binding.property == ScreenBindingProperty::Visible ||
                 binding.property == ScreenBindingProperty::Enabled)
        {
            if (!std::holds_alternative<bool>(value))
                return fail(ScreenBindingDiagnostic::WrongType, "Visibility/enabled requires a boolean binding.");
        }
        else
        {
            std::ostringstream text;
            text.imbue(std::locale::classic());
            text << binding.prefix;
            if (const auto* n = std::get_if<double>(&value))
                text << std::fixed << std::setprecision(binding.decimalPlaces) << *n;
            else if (const auto* boolean = std::get_if<bool>(&value)) text << (*boolean ? "true" : "false");
            else text << std::get<std::string>(value);
            text << binding.suffix;
            value = text.str();
        }
        result.value = std::move(value);
        result.resolved = true;
        return result;
    }
}
