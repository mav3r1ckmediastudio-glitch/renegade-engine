#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    enum class FactionRelationship : std::int32_t
    {
        Ally = 0,
        Friendly,
        Neutral,
        Suspicious,
        Hostile,
    };

    struct FactionDefinition
    {
        const char* id = "Neutral";
        const char* displayName = "Neutral";
    };

    [[nodiscard]] inline const std::array<FactionDefinition, 6>&
        BuiltInFactions() noexcept
    {
        static constexpr std::array<FactionDefinition, 6> factions = {{
            {"Player", "Player"},
            {"Friendly", "Friendly"},
            {"Enemy", "Enemy"},
            {"Civilian", "Civilian"},
            {"Wildlife", "Wildlife"},
            {"Neutral", "Neutral"},
        }};
        return factions;
    }

    [[nodiscard]] inline bool ValidateFactionId(
        const std::string& factionId,
        std::string& error) noexcept
    {
        if (factionId.empty() || factionId.size() > 64)
        {
            error = "Faction ID must be between 1 and 64 characters.";
            return false;
        }
        for (const unsigned char character : factionId)
        {
            if (std::iscntrl(character) != 0)
            {
                error = "Faction ID cannot contain control characters.";
                return false;
            }
        }
        error.clear();
        return true;
    }

    [[nodiscard]] inline bool IsBuiltInFaction(const std::string& factionId) noexcept
    {
        for (const auto& faction : BuiltInFactions())
        {
            if (factionId == faction.id)
                return true;
        }
        return false;
    }

    // Runtime keeps a deterministic registry of every faction that is known in
    // the active level. Built-ins are always registered. Creator-defined IDs are
    // admitted only through the same validation boundary, then stored sorted so
    // registration order and transient ECS allocation cannot affect results.
    // The registry records affiliation only; it owns no perception state.
    class FactionRegistry final
    {
    public:
        FactionRegistry()
        {
            ResetToBuiltIns();
        }

        void ResetToBuiltIns()
        {
            factionIds_.clear();
            factionIds_.reserve(BuiltInFactions().size());
            for (const auto& faction : BuiltInFactions())
                factionIds_.emplace_back(faction.id);
            std::sort(factionIds_.begin(), factionIds_.end());
        }

        [[nodiscard]] bool Register(
            const std::string& factionId,
            std::string& error)
        {
            if (!ValidateFactionId(factionId, error))
                return false;
            const auto iterator = std::lower_bound(
                factionIds_.begin(), factionIds_.end(), factionId);
            if (iterator == factionIds_.end() || *iterator != factionId)
                factionIds_.insert(iterator, factionId);
            error.clear();
            return true;
        }

        [[nodiscard]] bool Contains(const std::string& factionId) const noexcept
        {
            return std::binary_search(
                factionIds_.begin(), factionIds_.end(), factionId);
        }

        [[nodiscard]] const std::vector<std::string>& KnownFactionIds() const noexcept
        {
            return factionIds_;
        }

        [[nodiscard]] std::size_t Size() const noexcept
        {
            return factionIds_.size();
        }

    private:
        std::vector<std::string> factionIds_;
    };

    [[nodiscard]] inline FactionRelationship DefaultFactionRelationship(
        const std::string& observerFaction,
        const std::string& subjectFaction) noexcept
    {
        if (!observerFaction.empty() && observerFaction == subjectFaction)
            return FactionRelationship::Ally;

        if (observerFaction == "Player")
        {
            if (subjectFaction == "Friendly") return FactionRelationship::Friendly;
            if (subjectFaction == "Enemy") return FactionRelationship::Hostile;
            if (subjectFaction == "Wildlife") return FactionRelationship::Suspicious;
            return FactionRelationship::Neutral;
        }
        if (observerFaction == "Friendly")
        {
            if (subjectFaction == "Player" || subjectFaction == "Civilian")
                return FactionRelationship::Friendly;
            if (subjectFaction == "Enemy") return FactionRelationship::Hostile;
            return FactionRelationship::Neutral;
        }
        if (observerFaction == "Enemy")
        {
            if (subjectFaction == "Player" || subjectFaction == "Friendly")
                return FactionRelationship::Hostile;
            return FactionRelationship::Neutral;
        }
        if (observerFaction == "Civilian")
        {
            if (subjectFaction == "Friendly") return FactionRelationship::Friendly;
            if (subjectFaction == "Enemy") return FactionRelationship::Suspicious;
            return FactionRelationship::Neutral;
        }
        if (observerFaction == "Wildlife")
        {
            if (subjectFaction == "Player") return FactionRelationship::Suspicious;
            return FactionRelationship::Neutral;
        }
        return FactionRelationship::Neutral;
    }

    [[nodiscard]] inline const char* ToString(
        const FactionRelationship relationship) noexcept
    {
        switch (relationship)
        {
        case FactionRelationship::Ally: return "Ally";
        case FactionRelationship::Friendly: return "Friendly";
        case FactionRelationship::Suspicious: return "Suspicious";
        case FactionRelationship::Hostile: return "Hostile";
        case FactionRelationship::Neutral:
        default:
            return "Neutral";
        }
    }
}
