#pragma once
#include <WickedEngine.h>
#include <cmath>
#include <string>
namespace renegade::bridge {
struct FirearmSettings {
    int capacity = 2;
    float minimumShotInterval = 0.0f;
    bool allowPartialReload = true;
    bool operator==(const FirearmSettings& rhs) const noexcept {
        return capacity == rhs.capacity && minimumShotInterval == rhs.minimumShotInterval &&
            allowPartialReload == rhs.allowPartialReload;
    }
};
inline bool ValidateFirearmSettings(const FirearmSettings& s, std::string& error) {
    if (s.capacity < 1 || s.capacity > 1000) {
        error = "Weapon capacity must be between 1 and 1000."; return false;
    }
    if (!std::isfinite(s.minimumShotInterval) || s.minimumShotInterval < 0 ||
        s.minimumShotInterval > 60) {
        error = "Minimum shot interval must be between 0 and 60 seconds."; return false;
    }
    error.clear(); return true;
}
inline void ApplyFirearmSettings(wi::scene::MetadataComponent& metadata, const FirearmSettings& s) {
    metadata.int_values.set("renegade.firearm.schema", 1);
    metadata.int_values.set("renegade.firearm.capacity", s.capacity);
    metadata.float_values.set("renegade.firearm.minimum_shot_interval", s.minimumShotInterval);
    metadata.bool_values.set("renegade.firearm.allow_partial_reload", s.allowPartialReload);
}
inline bool CaptureFirearmSettings(const wi::scene::MetadataComponent& metadata,
    FirearmSettings& result, std::string& error) {
    result = {};
    if (!metadata.int_values.has("renegade.firearm.schema")) {
        error.clear(); return true; // Existing assemblies retain accepted shotgun behavior.
    }
    if (metadata.int_values.get("renegade.firearm.schema") != 1 ||
        !metadata.int_values.has("renegade.firearm.capacity") ||
        !metadata.float_values.has("renegade.firearm.minimum_shot_interval") ||
        !metadata.bool_values.has("renegade.firearm.allow_partial_reload")) {
        error = "Weapon settings metadata is incomplete or unsupported."; return false;
    }
    result.capacity = metadata.int_values.get("renegade.firearm.capacity");
    result.minimumShotInterval = metadata.float_values.get("renegade.firearm.minimum_shot_interval");
    result.allowPartialReload = metadata.bool_values.get("renegade.firearm.allow_partial_reload");
    return ValidateFirearmSettings(result, error);
}
}
