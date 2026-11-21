#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>

namespace renegade::bridge
{
    // The pinned WISCENE serializer writes uints, lengths and jump offsets as
    // 64-bit values. The buffer is the already decoded archive, including its
    // header. Inspect only its resource table and never copy image data.
    inline bool ReadWisceneEmbeddedNames(const std::uint8_t* bytes, std::size_t size,
        std::size_t pos, std::uint64_t version, const std::string& sourceDirectory,
        std::unordered_set<std::string>& names)
    {
        names.clear();
        if (version < 63)
            return true;

        std::size_t end = size;
        auto readWord = [&](std::uint64_t& value)
        {
            if (pos > end || end - pos < sizeof(value))
                return false;
            std::memcpy(&value, bytes + pos, sizeof(value));
            pos += sizeof(value);
            return true;
        };

        std::uint64_t reserved;
        if (!readWord(reserved))
            return false;
        if (version >= 90)
        {
            std::uint64_t start, after;
            if (!readWord(start) || !readWord(after) || start < pos ||
                start > after || after > size)
                return false;
            pos = static_cast<std::size_t>(start);
            end = static_cast<std::size_t>(after);
        }

        std::uint64_t count;
        if (!readWord(count) || count > (end - pos) / (3 * sizeof(std::uint64_t)))
            return false;
        std::unordered_set<std::string> embedded;
        for (std::uint64_t i = 0; i < count; ++i)
        {
            std::uint64_t nameLength, flags, dataLength;
            if (!readWord(nameLength) || nameLength > end - pos)
                return false;
            std::string name(reinterpret_cast<const char*>(bytes + pos),
                static_cast<std::size_t>(nameLength));
            pos += static_cast<std::size_t>(nameLength);
            if (version < 73 && !name.empty())
                name.pop_back(); // Native legacy strings include their terminator.
            if (!readWord(flags) || !readWord(dataLength) || dataLength > end - pos)
                return false;
            if (dataLength != 0 && !name.empty())
                embedded.insert(sourceDirectory + name);
            pos += static_cast<std::size_t>(dataLength);
        }
        if (version >= 90 && pos != end)
            return false;
        names = std::move(embedded);
        return true;
    }
}
