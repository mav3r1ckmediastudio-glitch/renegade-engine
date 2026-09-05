#pragma once

#include "renegade/bridge/ResourceAssetService.h"

#include <string>
#include <vector>

namespace renegade::bridge
{
    // Builds the transient Wicked resource-manager logical name from the actual
    // governed payload rather than the durable LP08 v1 FNV freshness token.
    // Windows uses SHA-256 through BCrypt. Non-Windows builds use a fresh stable
    // UUID token so they remain collision-safe without adding a platform crypto
    // dependency; Renegade's accepted v1 target remains Windows x64.
    [[nodiscard]] bool BuildResourcePayloadCacheName(
        const std::string& prefix,
        const StableId& assetId,
        ResourceSourceFormat sourceFormat,
        const std::vector<std::uint8_t>& payload,
        std::string& logicalName,
        std::string& error);
}
