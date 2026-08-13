#include "renegade/bridge/ResourceAssetCacheIdentityService.h"

#include <iostream>
#include <string>
#include <vector>

int main()
{
    using namespace renegade::bridge;

    const StableId assetId =
        "a2222222-2222-4222-8222-222222222222";
    const std::vector<std::uint8_t> first = {
        137,80,78,71,13,10,26,10,1,2,3,4};
    const std::vector<std::uint8_t> second = {
        137,80,78,71,13,10,26,10,4,3,2,1};

    std::string firstName;
    std::string repeatedName;
    std::string secondName;
    std::string error;
    if (!BuildResourcePayloadCacheName(
            "renegade_test_", assetId, ResourceSourceFormat::Png,
            first, firstName, error) ||
        !BuildResourcePayloadCacheName(
            "renegade_test_", assetId, ResourceSourceFormat::Png,
            first, repeatedName, error) ||
        !BuildResourcePayloadCacheName(
            "renegade_test_", assetId, ResourceSourceFormat::Png,
            second, secondName, error))
    {
        std::cerr << "LP08 GATE 5 CACHE IDENTITY FAIL // " << error << '\n';
        return 1;
    }

#ifdef _WIN32
    const std::string prefix = "renegade_test_" + assetId + "_sha256_";
    if (firstName.rfind(prefix, 0) != 0 ||
        firstName.size() != prefix.size() + 64 + 4 ||
        firstName.substr(firstName.size() - 4) != ".png" ||
        firstName != repeatedName ||
        firstName == secondName)
    {
        std::cerr
            << "LP08 GATE 5 CACHE IDENTITY FAIL // Windows cache name was not deterministic SHA-256 over payload bytes\n";
        return 1;
    }
#else
    if (firstName == repeatedName || firstName == secondName)
    {
        std::cerr
            << "LP08 GATE 5 CACHE IDENTITY FAIL // portability fallback reused a transient cache identity\n";
        return 1;
    }
#endif

    std::cout
        << "LP08 GATE 5 CACHE IDENTITY PASS // payload_sha256_not_fnv live_cache_collision_resistant\n";
    return 0;
}
