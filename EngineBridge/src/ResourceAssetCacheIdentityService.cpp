#include "renegade/bridge/ResourceAssetCacheIdentityService.h"

#include "renegade/bridge/IdentityService.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>
#endif

namespace renegade::bridge
{
    namespace
    {
        std::string ExtensionForFormat(const ResourceSourceFormat format)
        {
            for (const auto& capability : GetSupportedResourceFormats())
            {
                if (capability.format == format)
                    return capability.extension;
            }
            return {};
        }

#ifdef _WIN32
        bool Sha256(
            const std::vector<std::uint8_t>& payload,
            std::string& digest,
            std::string& error)
        {
            digest.clear();
            BCRYPT_ALG_HANDLE algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            DWORD objectBytes = 0;
            DWORD hashBytes = 0;
            DWORD returned = 0;
            std::vector<unsigned char> object;
            std::vector<unsigned char> result;

            if (BCryptOpenAlgorithmProvider(
                    &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
                BCryptGetProperty(
                    algorithm, BCRYPT_OBJECT_LENGTH,
                    reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes),
                    &returned, 0) < 0 ||
                BCryptGetProperty(
                    algorithm, BCRYPT_HASH_LENGTH,
                    reinterpret_cast<PUCHAR>(&hashBytes), sizeof(hashBytes),
                    &returned, 0) < 0)
            {
                if (algorithm != nullptr)
                    BCryptCloseAlgorithmProvider(algorithm, 0);
                error = "Could not initialize governed resource SHA-256 cache identity.";
                return false;
            }

            object.resize(objectBytes);
            result.resize(hashBytes);
            if (BCryptCreateHash(
                    algorithm, &hash, object.data(), objectBytes,
                    nullptr, 0, 0) < 0)
            {
                BCryptCloseAlgorithmProvider(algorithm, 0);
                error = "Could not create governed resource SHA-256 cache state.";
                return false;
            }

            bool succeeded = true;
            constexpr std::size_t MaximumChunk = 1024u * 1024u;
            for (std::size_t offset = 0;
                succeeded && offset < payload.size();)
            {
                const std::size_t count = std::min(
                    MaximumChunk, payload.size() - offset);
                if (BCryptHashData(
                        hash,
                        const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(
                            payload.data() + offset)),
                        static_cast<ULONG>(count), 0) < 0)
                {
                    succeeded = false;
                    break;
                }
                offset += count;
            }
            if (succeeded &&
                BCryptFinishHash(hash, result.data(), hashBytes, 0) >= 0)
            {
                std::ostringstream stream;
                stream << std::hex << std::setfill('0');
                for (const unsigned char value : result)
                    stream << std::setw(2) << static_cast<unsigned int>(value);
                digest = stream.str();
            }
            else
            {
                succeeded = false;
                error = "Could not finish governed resource SHA-256 cache identity.";
            }

            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            if (succeeded)
                error.clear();
            return succeeded;
        }
#endif
    }

    bool BuildResourcePayloadCacheName(
        const std::string& prefix,
        const StableId& assetId,
        const ResourceSourceFormat sourceFormat,
        const std::vector<std::uint8_t>& payload,
        std::string& logicalName,
        std::string& error)
    {
        logicalName.clear();
        if (prefix.empty() || !IsValidStableId(assetId) || payload.empty())
        {
            error =
                "Governed resource cache identity requires a prefix, stable asset ID and non-empty payload.";
            return false;
        }
        const std::string extension = ExtensionForFormat(sourceFormat);
        if (extension.empty())
        {
            error = "Governed resource cache identity has an unsupported source format.";
            return false;
        }

#ifdef _WIN32
        std::string payloadDigest;
        if (!Sha256(payload, payloadDigest, error))
            return false;
        logicalName = prefix + assetId + "_sha256_" + payloadDigest + extension;
#else
        // This path is compile portability only today. A fresh UUID guarantees
        // that two distinct payload preparations cannot alias a Wicked cache key.
        logicalName = prefix + assetId + "_load_" + GenerateStableId() + extension;
#endif
        error.clear();
        return true;
    }
}
