#include "renegade/bridge/BuildStageService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "json.hpp"

namespace renegade::bridge
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr const char* RequiredPackageDocuments[] = {
            "ReadMe.txt",
            "Licences/Renegade-Licence-or-Notice.txt",
            "Licences/WickedEngine-LICENSE.txt",
            "Licences/WickedEngine-third_party_software.txt",
            "Licences/DirectXShaderCompiler-LICENSE.txt",
            "Licences/DirectXShaderCompiler-ThirdPartyNotices.txt",
        };

        class Sha256 final
        {
        public:
            Sha256()
            {
                state_ = {
                    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
                };
            }

            void Update(const std::uint8_t* data, std::size_t size)
            {
                totalBytes_ += static_cast<std::uint64_t>(size);
                while (size != 0)
                {
                    const std::size_t available = block_.size() - blockSize_;
                    const std::size_t count = (std::min)(available, size);
                    std::copy_n(data, count, block_.data() + blockSize_);
                    blockSize_ += count;
                    data += count;
                    size -= count;
                    if (blockSize_ == block_.size())
                    {
                        Transform(block_.data());
                        blockSize_ = 0;
                    }
                }
            }

            std::string FinalHex()
            {
                const std::uint64_t totalBits = totalBytes_ * 8u;
                block_[blockSize_++] = 0x80u;
                if (blockSize_ > 56)
                {
                    std::fill(block_.begin() + static_cast<std::ptrdiff_t>(blockSize_),
                        block_.end(), 0u);
                    Transform(block_.data());
                    blockSize_ = 0;
                }
                std::fill(block_.begin() + static_cast<std::ptrdiff_t>(blockSize_),
                    block_.begin() + 56, 0u);
                for (std::size_t index = 0; index < 8; ++index)
                {
                    block_[63 - index] = static_cast<std::uint8_t>(
                        totalBits >> (index * 8));
                }
                Transform(block_.data());

                std::ostringstream stream;
                stream << std::hex << std::setfill('0');
                for (const std::uint32_t value : state_)
                    stream << std::setw(8) << value;
                return stream.str();
            }

        private:
            static std::uint32_t RotateRight(
                const std::uint32_t value, const std::uint32_t shift)
            {
                return (value >> shift) | (value << (32u - shift));
            }

            void Transform(const std::uint8_t* data)
            {
                static constexpr std::array<std::uint32_t, 64> K = {
                    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
                    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
                    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
                    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
                    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
                    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
                    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
                    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
                    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
                    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
                    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
                    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
                    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
                    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
                    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
                    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
                };

                std::array<std::uint32_t, 64> words{};
                for (std::size_t index = 0; index < 16; ++index)
                {
                    const std::size_t offset = index * 4;
                    words[index] =
                        (static_cast<std::uint32_t>(data[offset]) << 24) |
                        (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
                        (static_cast<std::uint32_t>(data[offset + 2]) << 8) |
                        static_cast<std::uint32_t>(data[offset + 3]);
                }
                for (std::size_t index = 16; index < words.size(); ++index)
                {
                    const std::uint32_t x = words[index - 15];
                    const std::uint32_t y = words[index - 2];
                    const std::uint32_t s0 = RotateRight(x, 7) ^
                        RotateRight(x, 18) ^ (x >> 3);
                    const std::uint32_t s1 = RotateRight(y, 17) ^
                        RotateRight(y, 19) ^ (y >> 10);
                    words[index] = words[index - 16] + s0 +
                        words[index - 7] + s1;
                }

                std::uint32_t a = state_[0];
                std::uint32_t b = state_[1];
                std::uint32_t c = state_[2];
                std::uint32_t d = state_[3];
                std::uint32_t e = state_[4];
                std::uint32_t f = state_[5];
                std::uint32_t g = state_[6];
                std::uint32_t h = state_[7];

                for (std::size_t index = 0; index < words.size(); ++index)
                {
                    const std::uint32_t s1 = RotateRight(e, 6) ^
                        RotateRight(e, 11) ^ RotateRight(e, 25);
                    const std::uint32_t choose = (e & f) ^ ((~e) & g);
                    const std::uint32_t temp1 = h + s1 + choose +
                        K[index] + words[index];
                    const std::uint32_t s0 = RotateRight(a, 2) ^
                        RotateRight(a, 13) ^ RotateRight(a, 22);
                    const std::uint32_t majority =
                        (a & b) ^ (a & c) ^ (b & c);
                    const std::uint32_t temp2 = s0 + majority;
                    h = g;
                    g = f;
                    f = e;
                    e = d + temp1;
                    d = c;
                    c = b;
                    b = a;
                    a = temp1 + temp2;
                }

                state_[0] += a;
                state_[1] += b;
                state_[2] += c;
                state_[3] += d;
                state_[4] += e;
                state_[5] += f;
                state_[6] += g;
                state_[7] += h;
            }

            std::array<std::uint32_t, 8> state_{};
            std::array<std::uint8_t, 64> block_{};
            std::size_t blockSize_ = 0;
            std::uint64_t totalBytes_ = 0;
        };

        struct FileDigest
        {
            std::uint64_t byteCount = 0;
            std::string sha256;
            std::string fnv1a64;
        };

        std::string AsciiLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](const unsigned char character)
                {
                    return character >= 'A' && character <= 'Z'
                        ? static_cast<char>(character + ('a' - 'A'))
                        : static_cast<char>(character);
                });
            return value;
        }

#if defined(_WIN32)
        bool TryDecodeUtf8(const std::string& value, std::wstring& decoded)
        {
            if (value.size() > static_cast<std::size_t>(
                    (std::numeric_limits<int>::max)()))
                return false;
            const int size = static_cast<int>(value.size());
            const int required = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), size, nullptr, 0);
            if (required <= 0)
                return false;
            decoded.resize(static_cast<std::size_t>(required));
            return MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), size,
                decoded.data(), required) == required;
        }
#endif

        bool WindowsPathCaseEquivalent(
            const std::string& left, const std::string& right)
        {
            if (left == right)
                return true;
#if defined(_WIN32)
            std::wstring decodedLeft;
            std::wstring decodedRight;
            if (!TryDecodeUtf8(left, decodedLeft) ||
                !TryDecodeUtf8(right, decodedRight))
                return false;
            return CompareStringOrdinal(
                decodedLeft.data(), static_cast<int>(decodedLeft.size()),
                decodedRight.data(), static_cast<int>(decodedRight.size()),
                TRUE) == CSTR_EQUAL;
#else
            return AsciiLower(left) == AsciiLower(right);
#endif
        }

        bool IsSafeStagingId(const std::string& value)
        {
            if (value.empty() || value.size() > 64 ||
                value == "." || value == "..")
                return false;
            return std::all_of(value.begin(), value.end(),
                [](const unsigned char character)
                {
                    return std::isalnum(character) != 0 ||
                        character == '-' || character == '_';
                });
        }

        bool IsPinnedRevision(const std::string& value)
        {
            return value.size() >= 7 && value.size() <= 40 &&
                std::all_of(value.begin(), value.end(),
                    [](const unsigned char character)
                    {
                        return (character >= '0' && character <= '9') ||
                            (character >= 'a' && character <= 'f');
                    });
        }

        bool IsVersionedProvenance(const std::string& value)
        {
            return value.rfind("repo:", 0) == 0 ||
                value.rfind("pinned:", 0) == 0;
        }

        bool IsSafeRelativePath(const std::string& value)
        {
            if (value.empty() || value.find('\\') != std::string::npos)
                return false;
            const fs::path path = fs::u8path(value);
            if (path.is_absolute() || path.has_root_name() ||
                path.generic_u8string() != value ||
                path.lexically_normal().generic_u8string() != value)
                return false;
            for (const fs::path& part : path)
            {
                const std::string segment = part.generic_u8string();
                if (segment.empty() || segment == "." || segment == "..")
                    return false;
            }
            return true;
        }

        bool ContainsPath(
            const fs::path& root, const fs::path& candidate)
        {
            auto rootPart = root.begin();
            auto candidatePart = candidate.begin();
            for (; rootPart != root.end(); ++rootPart, ++candidatePart)
            {
                if (candidatePart == candidate.end() ||
                    !WindowsPathCaseEquivalent(
                        rootPart->generic_u8string(),
                        candidatePart->generic_u8string()))
                    return false;
            }
            return true;
        }

        bool ResolveRegularFile(
            const fs::path& root,
            const std::string& relativePath,
            fs::path& resolved,
            std::string& error)
        {
            if (!IsSafeRelativePath(relativePath))
            {
                error = "Gate 2 source path is not a safe canonical relative path: " +
                    relativePath;
                return false;
            }
            std::error_code ec;
            const fs::path canonicalRoot = fs::weakly_canonical(root, ec);
            if (ec || !fs::is_directory(canonicalRoot, ec) || ec)
            {
                error = "Gate 2 project root is not an accessible directory.";
                return false;
            }
            const fs::path declared = canonicalRoot / fs::u8path(relativePath);
            if (fs::is_symlink(fs::symlink_status(declared, ec)) || ec)
            {
                error = "Gate 2 refuses a symlink project source: " + relativePath;
                return false;
            }
            resolved = fs::weakly_canonical(declared, ec);
            if (ec || !ContainsPath(canonicalRoot, resolved) ||
                !fs::is_regular_file(resolved, ec) || ec)
            {
                error = "Gate 2 project source is missing or escapes the project root: " +
                    relativePath;
                return false;
            }
            return true;
        }

        bool ResolveExternalRegularFile(
            const std::string& sourcePath,
            fs::path& resolved,
            std::string& error)
        {
            std::error_code ec;
            const fs::path declared = fs::u8path(sourcePath);
            if (sourcePath.empty() || !declared.is_absolute() ||
                fs::is_symlink(fs::symlink_status(declared, ec)) || ec)
            {
                error = "Gate 2 refuses a missing or symlinked governed source: " +
                    sourcePath;
                return false;
            }
            resolved = fs::weakly_canonical(declared, ec);
            if (ec || !fs::is_regular_file(resolved, ec) || ec)
            {
                error = "Gate 2 governed source is not a regular file: " + sourcePath;
                return false;
            }
            return true;
        }

        bool DigestFile(
            const fs::path& path,
            FileDigest& digest,
            std::string& error)
        {
            digest = {};
            std::ifstream input(path, std::ios::binary);
            if (!input)
            {
                error = "Gate 2 could not open file for hashing: " +
                    path.generic_u8string();
                return false;
            }
            Sha256 sha;
            std::uint64_t fnv = 1469598103934665603ull;
            std::array<char, 16384> buffer{};
            while (input)
            {
                input.read(buffer.data(),
                    static_cast<std::streamsize>(buffer.size()));
                const std::streamsize count = input.gcount();
                if (count <= 0)
                    continue;
                const auto byteCount = static_cast<std::size_t>(count);
                sha.Update(reinterpret_cast<const std::uint8_t*>(buffer.data()),
                    byteCount);
                digest.byteCount += static_cast<std::uint64_t>(byteCount);
                for (std::size_t index = 0; index < byteCount; ++index)
                {
                    fnv ^= static_cast<unsigned char>(buffer[index]);
                    fnv *= 1099511628211ull;
                }
            }
            if (input.bad())
            {
                error = "Gate 2 failed while hashing file: " +
                    path.generic_u8string();
                return false;
            }
            digest.sha256 = sha.FinalHex();
            std::ostringstream stream;
            stream << "fnv1a64:" << std::hex << std::setfill('0')
                   << std::setw(16) << fnv;
            digest.fnv1a64 = stream.str();
            return true;
        }

        bool EnsureUniqueDestination(
            std::vector<std::string>& destinations,
            const std::string& destination,
            std::string& error)
        {
            if (!IsSafeRelativePath(destination))
            {
                error = "Gate 2 destination is not a safe canonical relative path: " +
                    destination;
                return false;
            }
            const auto found = std::find_if(destinations.begin(), destinations.end(),
                [&destination](const std::string& existing)
                {
                    return WindowsPathCaseEquivalent(existing, destination);
                });
            if (found != destinations.end())
            {
                error = "Gate 2 destination collision: " + destination +
                    " and " + *found;
                return false;
            }
            destinations.push_back(destination);
            return true;
        }

        bool WriteTextFile(
            const fs::path& stagingRoot,
            const std::string& destination,
            const std::string& text,
            const std::string& fileClass,
            std::vector<std::string> provenance,
            WindowsGameStagedFile& staged,
            std::string& error)
        {
            const fs::path output = stagingRoot / fs::u8path(destination);
            std::error_code ec;
            fs::create_directories(output.parent_path(), ec);
            if (ec || fs::exists(output, ec))
            {
                error = "Gate 2 generated destination already exists or cannot be created: " +
                    destination;
                return false;
            }
            std::ofstream stream(output, std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                error = "Gate 2 could not create generated file: " + destination;
                return false;
            }
            stream.write(text.data(), static_cast<std::streamsize>(text.size()));
            stream.close();
            if (!stream)
            {
                error = "Gate 2 failed writing generated file: " + destination;
                return false;
            }
            FileDigest digest;
            if (!DigestFile(output, digest, error))
                return false;
            std::sort(provenance.begin(), provenance.end());
            provenance.erase(std::unique(provenance.begin(), provenance.end()),
                provenance.end());
            staged.destinationPath = destination;
            staged.byteCount = digest.byteCount;
            staged.sha256 = digest.sha256;
            staged.fileClass = fileClass;
            staged.provenance = std::move(provenance);
            return true;
        }

        bool CopyVerifiedFile(
            const fs::path& source,
            const fs::path& stagingRoot,
            const std::string& destination,
            const std::string& expectedSha256,
            const std::uint64_t expectedBytes,
            const std::string& expectedFnv,
            const std::string& fileClass,
            std::vector<std::string> provenance,
            WindowsGameStagedFile& staged,
            std::string& error)
        {
            FileDigest sourceDigest;
            if (!DigestFile(source, sourceDigest, error))
                return false;
            if ((!expectedSha256.empty() &&
                    sourceDigest.sha256 != expectedSha256) ||
                (expectedBytes != 0 && sourceDigest.byteCount != expectedBytes) ||
                (!expectedFnv.empty() && sourceDigest.fnv1a64 != expectedFnv))
            {
                error = "Gate 2 source bytes no longer match the approved plan for: " +
                    destination;
                return false;
            }

            const fs::path output = stagingRoot / fs::u8path(destination);
            std::error_code ec;
            fs::create_directories(output.parent_path(), ec);
            if (ec || fs::exists(output, ec))
            {
                error = "Gate 2 copy destination already exists or cannot be created: " +
                    destination;
                return false;
            }
            if (!fs::copy_file(source, output, fs::copy_options::none, ec) || ec)
            {
                error = "Gate 2 failed copying approved file: " + destination;
                return false;
            }
            FileDigest copiedDigest;
            if (!DigestFile(output, copiedDigest, error))
                return false;
            if (copiedDigest.sha256 != sourceDigest.sha256 ||
                copiedDigest.byteCount != sourceDigest.byteCount)
            {
                error = "Gate 2 copy verification failed for: " + destination;
                return false;
            }

            std::sort(provenance.begin(), provenance.end());
            provenance.erase(std::unique(provenance.begin(), provenance.end()),
                provenance.end());
            staged.destinationPath = destination;
            staged.byteCount = copiedDigest.byteCount;
            staged.sha256 = copiedDigest.sha256;
            staged.fileClass = fileClass;
            staged.provenance = std::move(provenance);
            return true;
        }

        const WindowsRuntimeSupportSource* FindSupportSource(
            const WindowsGameBuildStagingRequest& request,
            const std::string& destination)
        {
            const auto found = std::find_if(
                request.runtimeSupportSources.begin(),
                request.runtimeSupportSources.end(),
                [&destination](const WindowsRuntimeSupportSource& candidate)
                {
                    return WindowsPathCaseEquivalent(
                        candidate.destinationPath, destination);
                });
            return found == request.runtimeSupportSources.end() ?
                nullptr : &*found;
        }

        const WindowsPackageDocumentInput* FindPackageDocument(
            const WindowsGameBuildStagingRequest& request,
            const std::string& destination)
        {
            const auto found = std::find_if(
                request.packageDocuments.begin(),
                request.packageDocuments.end(),
                [&destination](const WindowsPackageDocumentInput& candidate)
                {
                    return WindowsPathCaseEquivalent(
                        candidate.destinationPath, destination);
                });
            return found == request.packageDocuments.end() ? nullptr : &*found;
        }

        nlohmann::json StagedFileJson(const WindowsGameStagedFile& file)
        {
            nlohmann::json item;
            item["path"] = file.destinationPath;
            item["bytes"] = file.byteCount;
            item["sha256"] = file.sha256;
            item["class"] = file.fileClass;
            item["provenance"] = file.provenance;
            return item;
        }

        void SortStagedFiles(std::vector<WindowsGameStagedFile>& files)
        {
            std::sort(files.begin(), files.end(),
                [](const WindowsGameStagedFile& left,
                   const WindowsGameStagedFile& right)
                {
                    return left.destinationPath < right.destinationPath;
                });
        }

        const WindowsGameStagedFile* FindStagedFile(
            const std::vector<WindowsGameStagedFile>& files,
            const std::string& destination)
        {
            const auto found = std::find_if(files.begin(), files.end(),
                [&destination](const WindowsGameStagedFile& file)
                {
                    return file.destinationPath == destination;
                });
            return found == files.end() ? nullptr : &*found;
        }

        bool ValidateRequestMappings(
            const WindowsGameBuildPlan& plan,
            const WindowsGameBuildStagingRequest& request,
            std::string& error)
        {
            std::vector<std::string> supportDestinations;
            for (const WindowsRuntimeSupportSource& source :
                request.runtimeSupportSources)
            {
                if (!EnsureUniqueDestination(
                        supportDestinations, source.destinationPath, error))
                    return false;
                const auto planned = std::find_if(plan.files.begin(), plan.files.end(),
                    [&source](const WindowsGameBuildFile& file)
                    {
                        return file.kind == WindowsGameBuildFileKind::RuntimeSupport &&
                            WindowsPathCaseEquivalent(
                                file.destinationPath, source.destinationPath);
                    });
                if (planned == plan.files.end())
                {
                    error = "Gate 2 Runtime source is not present in the Gate 1 plan: " +
                        source.destinationPath;
                    return false;
                }
            }

            std::size_t plannedSupportCount = 0;
            for (const WindowsGameBuildFile& file : plan.files)
            {
                if (file.kind != WindowsGameBuildFileKind::RuntimeSupport)
                    continue;
                ++plannedSupportCount;
                if (FindSupportSource(request, file.destinationPath) == nullptr)
                {
                    error = "Gate 2 is missing the authoritative source for Runtime support: " +
                        file.destinationPath;
                    return false;
                }
            }
            if (plannedSupportCount != request.runtimeSupportSources.size())
            {
                error = "Gate 2 Runtime support source set does not exactly match the plan.";
                return false;
            }

            std::vector<std::string> documentDestinations;
            for (const WindowsPackageDocumentInput& document : request.packageDocuments)
            {
                if (!EnsureUniqueDestination(
                        documentDestinations, document.destinationPath, error))
                    return false;
                if (document.component.empty() ||
                    !IsVersionedProvenance(document.provenance))
                {
                    error = "Gate 2 package document lacks governed component/provenance: " +
                        document.destinationPath;
                    return false;
                }
                const bool required = std::any_of(
                    std::begin(RequiredPackageDocuments),
                    std::end(RequiredPackageDocuments),
                    [&document](const char* expected)
                    {
                        return WindowsPathCaseEquivalent(
                            document.destinationPath, expected);
                    });
                if (!required)
                {
                    error = "Gate 2 package document destination is not approved: " +
                        document.destinationPath;
                    return false;
                }
            }
            for (const char* required : RequiredPackageDocuments)
            {
                if (FindPackageDocument(request, required) == nullptr)
                {
                    error = "Gate 2 is missing required version-controlled package input: " +
                        std::string(required);
                    return false;
                }
            }
            if (request.packageDocuments.size() !=
                (sizeof(RequiredPackageDocuments) / sizeof(RequiredPackageDocuments[0])))
            {
                error = "Gate 2 package document set must exactly match the current approved set.";
                return false;
            }
            return true;
        }
    }

    bool StageWindowsGameBuild(
        const WindowsGameBuildPlan& plan,
        const WindowsGameBuildStagingRequest& request,
        WindowsGameBuildStageResult& result,
        std::string& error)
    {
        result = {};
        error.clear();

        std::string planJson;
        if (!SerializeWindowsGameBuildPlan(plan, planJson, error))
        {
            error = "Gate 2 rejected its Gate 1 build plan: " + error;
            return false;
        }
        if (!IsSafeStagingId(request.stagingId) ||
            !IsPinnedRevision(request.renegadeRevision) ||
            !IsPinnedRevision(request.wickedRevision))
        {
            error = "Gate 2 requires a safe unique staging ID and pinned revision IDs.";
            return false;
        }
        if (!ValidateRequestMappings(plan, request, error))
            return false;

        std::error_code ec;
        const fs::path projectRoot = fs::u8path(request.projectRootPath);
        const fs::path outputParent = fs::u8path(request.outputParentPath);
        if (!fs::is_directory(projectRoot, ec) || ec)
        {
            error = "Gate 2 project root does not exist.";
            return false;
        }
        ec.clear();
        fs::create_directories(outputParent, ec);
        if (ec)
        {
            error = "Gate 2 could not create the output parent directory.";
            return false;
        }
        const fs::path canonicalOutputParent = fs::weakly_canonical(outputParent, ec);
        if (ec)
        {
            error = "Gate 2 could not canonicalize the output parent directory.";
            return false;
        }
        const fs::path stagingParent = canonicalOutputParent / ".renegade-staging";
        fs::create_directories(stagingParent, ec);
        if (ec)
        {
            error = "Gate 2 could not create the governed staging parent.";
            return false;
        }
        const fs::path stagingRoot = stagingParent /
            fs::u8path(plan.buildFolderName + "." + request.stagingId);
        const fs::path finalOutput = canonicalOutputParent /
            fs::u8path(plan.buildFolderName);
        if (fs::exists(stagingRoot, ec) || ec)
        {
            error = "Gate 2 staging directory already exists; staging IDs are single-use.";
            return false;
        }
        if (!fs::create_directory(stagingRoot, ec) || ec)
        {
            error = "Gate 2 could not create its unique staging directory.";
            return false;
        }

        result.stagingPath = stagingRoot.generic_u8string();
        result.finalOutputPath = finalOutput.generic_u8string();
        std::vector<std::string> destinations;

        std::vector<WindowsGameBuildFile> plannedFiles = plan.files;
        std::sort(plannedFiles.begin(), plannedFiles.end(),
            [](const WindowsGameBuildFile& left,
               const WindowsGameBuildFile& right)
            {
                return left.destinationPath < right.destinationPath;
            });

        for (const WindowsGameBuildFile& file : plannedFiles)
        {
            if (!EnsureUniqueDestination(destinations, file.destinationPath, error))
                return false;
            fs::path source;
            WindowsGameStagedFile staged;
            if (file.kind == WindowsGameBuildFileKind::ProjectContent)
            {
                if (!ResolveRegularFile(
                        projectRoot, file.projectRelativeSourcePath, source, error))
                    return false;
                if (!CopyVerifiedFile(
                        source,
                        stagingRoot,
                        file.destinationPath,
                        {},
                        0,
                        file.sourceContentHash,
                        "project_content",
                        file.provenance,
                        staged,
                        error))
                    return false;
            }
            else
            {
                const WindowsRuntimeSupportSource* runtimeSource =
                    FindSupportSource(request, file.destinationPath);
                if (runtimeSource == nullptr ||
                    !ResolveExternalRegularFile(
                        runtimeSource->sourcePath, source, error))
                    return false;
                if (!CopyVerifiedFile(
                        source,
                        stagingRoot,
                        file.destinationPath,
                        file.sha256,
                        file.byteCount,
                        {},
                        "runtime_support",
                        file.provenance,
                        staged,
                        error))
                    return false;
            }
            result.files.push_back(std::move(staged));
        }

        for (const char* required : RequiredPackageDocuments)
        {
            const WindowsPackageDocumentInput* document =
                FindPackageDocument(request, required);
            if (document == nullptr ||
                !EnsureUniqueDestination(destinations, required, error))
                return false;
            fs::path source;
            if (!ResolveExternalRegularFile(document->sourcePath, source, error))
                return false;
            WindowsGameStagedFile staged;
            if (!CopyVerifiedFile(
                    source,
                    stagingRoot,
                    required,
                    {},
                    0,
                    {},
                    required == std::string("ReadMe.txt") ?
                        "readme" : "licence_notice",
                    {document->provenance, "component:" + document->component},
                    staged,
                    error))
                return false;
            result.files.push_back(std::move(staged));
        }

        std::vector<WindowsGameStagedFile> projectFiles;
        std::vector<WindowsGameStagedFile> runtimeFiles;
        for (const WindowsGameStagedFile& file : result.files)
        {
            if (file.fileClass == "project_content")
                projectFiles.push_back(file);
            else if (file.fileClass == "runtime_support")
                runtimeFiles.push_back(file);
        }
        SortStagedFiles(projectFiles);
        SortStagedFiles(runtimeFiles);

        std::string projectDocument;
        for (const WindowsGameBuildFile& file : plannedFiles)
        {
            if (file.kind == WindowsGameBuildFileKind::ProjectContent &&
                file.dependencyClass == DependencyClass::ProjectDocument)
            {
                if (!projectDocument.empty())
                {
                    error = "Gate 2 requires exactly one packaged project document.";
                    return false;
                }
                projectDocument = file.destinationPath;
            }
        }
        if (projectDocument.empty())
        {
            error = "Gate 2 build plan contains no packaged project document.";
            return false;
        }

        nlohmann::json projectManifest;
        projectManifest["format"] = "renegade-project-package-manifest";
        projectManifest["schema_version"] = 1;
        projectManifest["project_id"] = plan.projectId;
        projectManifest["game_name"] = plan.gameName;
        projectManifest["executable"] = plan.executableFileName;
        projectManifest["public_version"] = plan.publicVersion;
        projectManifest["save_data_id"] = plan.saveDataId;
        projectManifest["platform"] = plan.platform;
        projectManifest["configuration"] = plan.configuration;
        projectManifest["project_document"] = projectDocument;
        projectManifest["content_manifest"] = "GameData/content-manifest.json";
        projectManifest["runtime_support_manifest"] =
            "Engine/runtime-support-manifest.json";
        projectManifest["stage_only"] = true;
        result.projectManifestJson = projectManifest.dump();

        std::vector<std::string> normalizedWarnings = plan.warnings;
        std::sort(normalizedWarnings.begin(), normalizedWarnings.end());
        normalizedWarnings.erase(
            std::unique(normalizedWarnings.begin(), normalizedWarnings.end()),
            normalizedWarnings.end());

        nlohmann::json contentManifest;
        contentManifest["format"] = "renegade-content-manifest";
        contentManifest["schema_version"] = 1;
        contentManifest["project_id"] = plan.projectId;
        contentManifest["excluded_editor_only"] = plan.excludedEditorOnly;
        contentManifest["excluded_optional_missing"] =
            plan.excludedOptionalMissing;
        contentManifest["excluded_unreachable"] = plan.excludedUnreachable;
        contentManifest["warnings"] = normalizedWarnings;
        contentManifest["files"] = nlohmann::json::array();
        for (const WindowsGameBuildFile& planned : plannedFiles)
        {
            if (planned.kind != WindowsGameBuildFileKind::ProjectContent)
                continue;
            const WindowsGameStagedFile* staged =
                FindStagedFile(projectFiles, planned.destinationPath);
            if (staged == nullptr)
            {
                error = "Gate 2 lost a project content record while generating its manifest.";
                return false;
            }
            nlohmann::json item = StagedFileJson(*staged);
            item["asset_id"] = planned.assetId;
            item["dependency_class"] = DependencyClassName(planned.dependencyClass);
            item["source_hash"] = planned.sourceContentHash;
            contentManifest["files"].push_back(std::move(item));
        }
        result.contentManifestJson = contentManifest.dump();

        nlohmann::json runtimeManifest;
        runtimeManifest["format"] = "renegade-runtime-support-manifest";
        runtimeManifest["schema_version"] = 1;
        runtimeManifest["platform"] = plan.platform;
        runtimeManifest["configuration"] = plan.configuration;
        runtimeManifest["files"] = nlohmann::json::array();
        for (const WindowsGameBuildFile& planned : plannedFiles)
        {
            if (planned.kind != WindowsGameBuildFileKind::RuntimeSupport)
                continue;
            const WindowsGameStagedFile* staged =
                FindStagedFile(runtimeFiles, planned.destinationPath);
            if (staged == nullptr)
            {
                error = "Gate 2 lost a Runtime support record while generating its manifest.";
                return false;
            }
            nlohmann::json item = StagedFileJson(*staged);
            item["support_name"] = planned.runtimeSupportName;
            runtimeManifest["files"].push_back(std::move(item));
        }
        result.runtimeSupportManifestJson = runtimeManifest.dump();

        WindowsGameStagedFile generated;
        if (!EnsureUniqueDestination(
                destinations, "GameData/project.manifest.json", error) ||
            !WriteTextFile(
                stagingRoot,
                "GameData/project.manifest.json",
                result.projectManifestJson,
                "project_manifest",
                {"lp06:gate2:generated:project-manifest"},
                generated,
                error))
            return false;
        result.projectManifestSha256 = generated.sha256;
        result.files.push_back(generated);

        if (!EnsureUniqueDestination(
                destinations, "GameData/content-manifest.json", error) ||
            !WriteTextFile(
                stagingRoot,
                "GameData/content-manifest.json",
                result.contentManifestJson,
                "content_manifest",
                {"lp06:gate2:generated:content-manifest"},
                generated,
                error))
            return false;
        result.contentManifestSha256 = generated.sha256;
        result.files.push_back(generated);

        if (!EnsureUniqueDestination(
                destinations, "Engine/runtime-support-manifest.json", error) ||
            !WriteTextFile(
                stagingRoot,
                "Engine/runtime-support-manifest.json",
                result.runtimeSupportManifestJson,
                "runtime_support_manifest",
                {"lp06:gate2:generated:runtime-support-manifest"},
                generated,
                error))
            return false;
        result.runtimeSupportManifestSha256 = generated.sha256;
        result.files.push_back(generated);

        std::vector<std::string> components;
        for (const WindowsPackageDocumentInput& document : request.packageDocuments)
            components.push_back(document.component + " | " + document.provenance);
        std::sort(components.begin(), components.end());
        std::ostringstream inventory;
        inventory << "Renegade LP06 Gate 2 build component inventory\n"
                  << "Stage-only evidence; not a redistribution approval.\n"
                  << "Renegade revision: " << request.renegadeRevision << "\n"
                  << "Wicked revision: " << request.wickedRevision << "\n";
        for (const std::string& component : components)
            inventory << component << '\n';
        if (!EnsureUniqueDestination(
                destinations, "Licences/Build-Component-Inventory.txt", error) ||
            !WriteTextFile(
                stagingRoot,
                "Licences/Build-Component-Inventory.txt",
                inventory.str(),
                "licence_inventory",
                {"lp06:gate2:generated:component-inventory"},
                generated,
                error))
            return false;
        result.files.push_back(generated);

        nlohmann::json buildReport;
        buildReport["format"] = "renegade-build-report";
        buildReport["schema_version"] = 1;
        buildReport["status"] = "staged_not_launched";
        buildReport["stage_only"] = true;
        buildReport["distribution_ready"] = false;
        buildReport["smoke_test"] = "not_run_gate2";
        buildReport["promotion"] = "not_attempted_gate2";
        buildReport["renegade_revision"] = request.renegadeRevision;
        buildReport["wicked_revision"] = request.wickedRevision;
        buildReport["project_id"] = plan.projectId;
        buildReport["warnings"] = normalizedWarnings;
        const std::string buildReportJson = buildReport.dump();
        if (!EnsureUniqueDestination(destinations, "build-report.json", error) ||
            !WriteTextFile(
                stagingRoot,
                "build-report.json",
                buildReportJson,
                "build_report",
                {"lp06:gate2:generated:build-report"},
                generated,
                error))
            return false;
        result.files.push_back(generated);

        SortStagedFiles(result.files);
        nlohmann::json packageManifest;
        packageManifest["format"] = "renegade-package-manifest";
        packageManifest["schema_version"] = 1;
        packageManifest["stage_only"] = true;
        packageManifest["distribution_ready"] = false;
        packageManifest["project_id"] = plan.projectId;
        packageManifest["game_name"] = plan.gameName;
        packageManifest["executable"] = plan.executableFileName;
        packageManifest["public_version"] = plan.publicVersion;
        packageManifest["save_data_id"] = plan.saveDataId;
        packageManifest["platform"] = plan.platform;
        packageManifest["configuration"] = plan.configuration;
        packageManifest["renegade_revision"] = request.renegadeRevision;
        packageManifest["wicked_revision"] = request.wickedRevision;
        packageManifest["project_manifest_sha256"] =
            result.projectManifestSha256;
        packageManifest["content_manifest_sha256"] =
            result.contentManifestSha256;
        packageManifest["runtime_support_manifest_sha256"] =
            result.runtimeSupportManifestSha256;
        packageManifest["self_path"] = "package-manifest.json";
        packageManifest["self_sha256_excluded"] = true;
        packageManifest["files"] = nlohmann::json::array();
        for (const WindowsGameStagedFile& file : result.files)
            packageManifest["files"].push_back(StagedFileJson(file));
        result.packageManifestJson = packageManifest.dump();

        if (!EnsureUniqueDestination(destinations, "package-manifest.json", error) ||
            !WriteTextFile(
                stagingRoot,
                "package-manifest.json",
                result.packageManifestJson,
                "package_manifest",
                {"lp06:gate2:generated:package-manifest"},
                generated,
                error))
            return false;
        result.packageManifestSha256 = generated.sha256;
        result.files.push_back(generated);
        SortStagedFiles(result.files);

        if (!ValidateWindowsGameBuildStage(result, error))
            return false;
        error.clear();
        return true;
    }

    bool ValidateWindowsGameBuildStage(
        const WindowsGameBuildStageResult& result,
        std::string& error)
    {
        error.clear();
        if (result.stagingPath.empty() || result.files.empty())
        {
            error = "Gate 2 stage result is incomplete.";
            return false;
        }
        const fs::path stagingRoot = fs::u8path(result.stagingPath);
        std::error_code ec;
        if (!fs::is_directory(stagingRoot, ec) || ec)
        {
            error = "Gate 2 staging directory is missing.";
            return false;
        }

        std::vector<std::string> actualDestinations;
        std::vector<std::string> expectedDestinations;
        for (const WindowsGameStagedFile& expected : result.files)
        {
            if (!EnsureUniqueDestination(
                    expectedDestinations, expected.destinationPath, error))
                return false;
        }

        for (fs::recursive_directory_iterator iterator(stagingRoot, ec), end;
             iterator != end && !ec; iterator.increment(ec))
        {
            const fs::directory_entry& entry = *iterator;
            if (entry.is_symlink(ec) || ec)
            {
                error = "Gate 2 staged tree contains a symlink.";
                return false;
            }
            if (entry.is_directory(ec) && !ec)
                continue;
            if (ec || !entry.is_regular_file(ec) || ec)
            {
                error = "Gate 2 staged tree contains a non-regular file.";
                return false;
            }
            const std::string relative =
                fs::relative(entry.path(), stagingRoot, ec).generic_u8string();
            if (ec || !EnsureUniqueDestination(
                    actualDestinations, relative, error))
                return false;
        }
        if (ec)
        {
            error = "Gate 2 failed while enumerating the staged tree.";
            return false;
        }
        if (actualDestinations.size() != expectedDestinations.size())
        {
            error = "Gate 2 staged tree contains missing or extra files.";
            return false;
        }

        for (const WindowsGameStagedFile& expected : result.files)
        {
            const auto actual = std::find_if(
                actualDestinations.begin(), actualDestinations.end(),
                [&expected](const std::string& candidate)
                {
                    return WindowsPathCaseEquivalent(
                        candidate, expected.destinationPath);
                });
            if (actual == actualDestinations.end())
            {
                error = "Gate 2 staged file is missing: " +
                    expected.destinationPath;
                return false;
            }
            FileDigest digest;
            if (!DigestFile(stagingRoot / fs::u8path(*actual), digest, error))
                return false;
            if (digest.byteCount != expected.byteCount ||
                digest.sha256 != expected.sha256)
            {
                error = "Gate 2 staged file failed size/SHA-256 verification: " +
                    expected.destinationPath;
                return false;
            }
        }
        error.clear();
        return true;
    }
}
