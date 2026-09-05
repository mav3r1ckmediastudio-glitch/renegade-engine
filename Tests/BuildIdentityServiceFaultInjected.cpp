#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace
{
    std::size_t g_failResourceWriteOrdinal = 0;
    std::size_t g_resourceWriteCount = 0;
}

extern "C" BOOL WINAPI RenegadeGate3TestUpdateResourceW(
    HANDLE updater,
    LPCWSTR type,
    LPCWSTR name,
    WORD language,
    LPVOID data,
    DWORD bytes)
{
    ++g_resourceWriteCount;
    if (g_failResourceWriteOrdinal != 0 &&
        g_resourceWriteCount == g_failResourceWriteOrdinal)
    {
        SetLastError(ERROR_WRITE_FAULT);
        return FALSE;
    }

    return ::UpdateResourceW(
        updater,
        type,
        name,
        language,
        data,
        bytes);
}

// Compile the real Gate 3 implementation into this test-only translation unit,
// redirecting only UpdateResourceW. BeginUpdateResourceW and EndUpdateResourceW
// remain the real Win32 APIs, so the production transaction/discard path is the
// code under test rather than a test reimplementation.
#define UpdateResourceW RenegadeGate3TestUpdateResourceW
#include "../EngineBridge/src/BuildIdentityService.cpp"
#undef UpdateResourceW

namespace renegade::bridge::testing
{
    bool DigestWindowsExecutableForRollbackTest(
        const std::string& executablePath,
        std::uint64_t& byteCount,
        std::string& sha256,
        std::string& error)
    {
        FileDigest digest;
        if (!DigestFile(fs::u8path(executablePath), digest, error))
            return false;

        byteCount = digest.byteCount;
        sha256 = std::move(digest.sha256);
        return true;
    }

    bool StampWindowsExecutableWithForcedResourceFailureForTest(
        const std::string& executablePath,
        const WindowsGameBuildPlan& plan,
        const WindowsGameExecutableIdentityRequest& request,
        const std::size_t failResourceWriteOrdinal,
        std::size_t& attemptedResourceWrites,
        std::string& error)
    {
        attemptedResourceWrites = 0;
        error.clear();

        std::array<std::uint16_t, 4> version{};
        if (!ParsePublicVersion(plan.publicVersion, version))
        {
            error = "Gate 3 rollback test received an invalid public version.";
            return false;
        }

        g_failResourceWriteOrdinal = failResourceWriteOrdinal;
        g_resourceWriteCount = 0;
        const bool succeeded = StampExecutableResources(
            fs::u8path(executablePath),
            plan,
            request,
            version,
            error);
        attemptedResourceWrites = g_resourceWriteCount;
        g_failResourceWriteOrdinal = 0;
        g_resourceWriteCount = 0;
        return succeeded;
    }
}
