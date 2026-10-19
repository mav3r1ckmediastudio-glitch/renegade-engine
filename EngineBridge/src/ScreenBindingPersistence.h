#pragma once
#include "renegade/bridge/ScreenBinding.h"
#include <wiConfig.h>
#include <limits>

namespace renegade::bridge::detail
{
    inline std::string EncodeBindingText(const std::string& value)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string encoded;
        encoded.reserve(value.size() * 2);
        for (unsigned char byte : value)
        {
            encoded.push_back(digits[byte >> 4]);
            encoded.push_back(digits[byte & 15]);
        }
        return encoded;
    }

    inline bool DecodeBindingText(const std::string& encoded, std::size_t maximum,
        std::string& value)
    {
        if (encoded.size() % 2 != 0 || encoded.size() > maximum * 2) return false;
        const auto nibble = [](char c) {
            return c >= '0' && c <= '9' ? c - '0' :
                (c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1);
        };
        std::string decoded;
        decoded.reserve(encoded.size() / 2);
        for (std::size_t i = 0; i < encoded.size(); i += 2)
        {
            const int a = nibble(encoded[i]), b = nibble(encoded[i + 1]);
            if (a < 0 || b < 0) return false;
            decoded.push_back(static_cast<char>((a << 4) | b));
        }
        value = std::move(decoded);
        return true;
    }

    inline void WriteScreenBinding(wi::config::Section& section, const ScreenBinding& binding)
    {
        section.Set("property", static_cast<int>(binding.property));
        section.Set("scope", static_cast<int>(binding.source.scope));
        section.Set("target", binding.source.targetId);
        section.Set("attribute", binding.source.attributeId);
        section.Set("operation", static_cast<int>(binding.operation));
        section.Set("denominator_scope", static_cast<int>(binding.denominator.scope));
        section.Set("denominator_target", binding.denominator.targetId);
        section.Set("denominator_attribute", binding.denominator.attributeId);
        section.Set("missing_policy", static_cast<int>(binding.missingPolicy));
        section.Set("required", binding.required);
        section.Set("decimal_places", binding.decimalPlaces);
        // Config syntax trims whitespace/comments; encode user formatter text.
        section.Set("text_encoding", "hex");
        section.Set("prefix", EncodeBindingText(binding.prefix));
        section.Set("suffix", EncodeBindingText(binding.suffix));
        section.Set("comparison_type", static_cast<int>(binding.comparison.index()));
        if (const auto* number = std::get_if<double>(&binding.comparison))
        {
            std::ostringstream text;
            text.imbue(std::locale::classic());
            text << std::setprecision(std::numeric_limits<double>::max_digits10) << *number;
            section.Set("comparison", text.str());
        }
        else if (const auto* boolean = std::get_if<bool>(&binding.comparison))
            section.Set("comparison", *boolean ? "true" : "false");
        else section.Set("comparison", EncodeBindingText(std::get<std::string>(binding.comparison)));
    }

    inline bool ReadScreenBinding(const wi::config::Section& section,
        ScreenBinding& binding, std::string& error)
    {
        for (const auto* field : {"property", "scope", "target", "attribute", "operation",
            "denominator_scope", "denominator_target", "denominator_attribute",
            "missing_policy", "required", "decimal_places", "prefix", "suffix",
            "comparison_type", "comparison", "text_encoding"})
        {
            if (!section.Has(field))
            { error = std::string("Screen binding is missing field ") + field + "."; return false; }
        }
        // Reject malformed integers rather than allowing a forgiving config
        // getter to silently convert bad input into a valid default enum.
        const auto readInt = [&](const char* key, int& value)
        {
            std::istringstream text(section.GetText(key));
            text.imbue(std::locale::classic());
            text >> std::noskipws >> value;
            return !text.fail() && text.peek() == std::char_traits<char>::eof();
        };
        int property, scope, operation, denominatorScope, missingPolicy, decimals, comparisonType;
        if (!readInt("property", property) || !readInt("scope", scope) ||
            !readInt("operation", operation) || !readInt("denominator_scope", denominatorScope) ||
            !readInt("missing_policy", missingPolicy) || !readInt("decimal_places", decimals) ||
            !readInt("comparison_type", comparisonType))
        { error = "Screen binding contains a malformed integer."; return false; }
        ScreenBinding candidate;
        candidate.property = static_cast<ScreenBindingProperty>(property);
        candidate.source.scope = static_cast<ScreenBindingScope>(scope);
        candidate.source.targetId = section.GetText("target");
        candidate.source.attributeId = section.GetText("attribute");
        candidate.operation = static_cast<ScreenBindingOperation>(operation);
        candidate.denominator.scope = static_cast<ScreenBindingScope>(denominatorScope);
        candidate.denominator.targetId = section.GetText("denominator_target");
        candidate.denominator.attributeId = section.GetText("denominator_attribute");
        candidate.missingPolicy = static_cast<ScreenBindingMissingPolicy>(missingPolicy);
        const auto required = section.GetText("required");
        if (required != "true" && required != "false" && required != "1" && required != "0")
        { error = "Screen binding required flag is malformed."; return false; }
        candidate.required = required == "true" || required == "1";
        candidate.decimalPlaces = decimals;
        if (section.GetText("text_encoding") != "hex" ||
            !DecodeBindingText(section.GetText("prefix"), 256, candidate.prefix) ||
            !DecodeBindingText(section.GetText("suffix"), 256, candidate.suffix))
        { error = "Screen binding formatter encoding is malformed."; return false; }
        const auto comparison = section.GetText("comparison");
        if (comparisonType == 0)
        {
            double number = 0;
            std::istringstream text(comparison);
            text.imbue(std::locale::classic());
            text >> std::noskipws >> number;
            if (text.fail() || text.peek() != std::char_traits<char>::eof() || !std::isfinite(number))
            { error = "Screen binding numeric comparison is malformed."; return false; }
            candidate.comparison = number;
        }
        else if (comparisonType == 1)
        {
            if (comparison != "true" && comparison != "false")
            { error = "Screen binding boolean comparison is malformed."; return false; }
            candidate.comparison = comparison == "true";
        }
        else if (comparisonType == 2)
        {
            std::string decoded;
            if (!DecodeBindingText(comparison, 4096, decoded))
            { error = "Screen binding text comparison encoding is malformed."; return false; }
            candidate.comparison = std::move(decoded);
        }
        else { error = "Screen binding comparison type is invalid."; return false; }
        if (!ValidateScreenBinding(candidate, error)) return false;
        binding = std::move(candidate);
        error.clear();
        return true;
    }
}
