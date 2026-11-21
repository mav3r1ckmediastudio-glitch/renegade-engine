#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace renegade::bridge
{
    bool ComputePayloadSha256(const std::vector<std::uint8_t>& payload,
        std::string& digest, std::string& error);
}
