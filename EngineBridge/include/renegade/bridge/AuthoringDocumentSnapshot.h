#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace renegade::bridge
{
    inline bool ReadAuthoringDocumentSnapshot(const std::string& path,
        std::vector<std::uint8_t>& bytes, std::string& error)
    {
        std::ifstream stream(std::filesystem::u8path(path), std::ios::binary);
        if (!stream) { error = "Cannot read authored document: " + path; return false; }
        std::vector<std::uint8_t> candidate{std::istreambuf_iterator<char>(stream), {}};
        if (stream.bad()) { error = "Could not finish reading authored document: " + path; return false; }
        bytes = std::move(candidate);
        error.clear();
        return true;
    }
    inline bool MatchesAuthoringDocumentSnapshot(const std::string& path,
        const std::vector<std::uint8_t>& expected, std::string& error)
    {
        std::vector<std::uint8_t> current;
        if (!ReadAuthoringDocumentSnapshot(path,current,error)) return false;
        if (current != expected)
        { error = "Authored document changed outside this session: " + path; return false; }
        error.clear();
        return true;
    }
}
