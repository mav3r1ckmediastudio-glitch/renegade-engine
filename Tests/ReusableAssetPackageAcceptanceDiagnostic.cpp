#include "renegade/bridge/WindowsGameBuildWorkflow.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{
    namespace fs = std::filesystem;

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty())
            return {};
        const int required = WideCharToMultiByte(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            nullptr, 0, nullptr, nullptr);
        if (required <= 0)
            return {};
        std::string result(static_cast<std::size_t>(required), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            result.data(), required, nullptr, nullptr);
        return result;
    }

    std::string LocalAppData()
    {
        const DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
        if (required <= 1)
            return {};
        std::wstring value(static_cast<std::size_t>(required), L'\0');
        const DWORD written = GetEnvironmentVariableW(
            L"LOCALAPPDATA", value.data(), required);
        if (written == 0 || written >= required)
            return {};
        value.resize(written);
        return WideToUtf8(value);
    }

    void PrintRuntimeEvidence(const std::string& projectId)
    {
        const std::string localAppData = LocalAppData();
        if (localAppData.empty() || projectId.empty())
            return;

        const fs::path evidencePath =
            fs::u8path(localAppData) / "RenegadeEngine" /
            fs::u8path(projectId) / "Logs" / "RuntimeBootstrap.log";
        std::ifstream input(evidencePath, std::ios::binary);
        if (!input)
        {
            std::cerr
                << "LP07 GATE 6 BUILD DIAGNOSTIC // runtime_evidence=<missing> path="
                << evidencePath.generic_u8string() << '\n';
            return;
        }

        const std::string text(
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>());
        std::cerr
            << "LP07 GATE 6 BUILD DIAGNOSTIC // runtime_evidence_begin\n"
            << text
            << "LP07 GATE 6 BUILD DIAGNOSTIC // runtime_evidence_end\n";
    }
}

namespace renegade::bridge
{
    bool Gate6BuildWindowsGameWithDiagnostics(
        const WindowsGameBuildWorkflowRequest& request,
        const WindowsGameBuildSmokeRunner& smokeRunner,
        WindowsGameBuildWorkflowResult& result,
        std::string& error)
    {
        const bool succeeded = BuildWindowsGame(
            request, smokeRunner, result, error);
        if (!succeeded)
        {
            std::cerr
                << "LP07 GATE 6 BUILD DIAGNOSTIC // error="
                << (error.empty() ? "<empty>" : error)
                << " staging_path=" << result.stage.stagingPath
                << " runtime_evidence_path=" << result.runtimeEvidencePath
                << '\n';
            PrintRuntimeEvidence(request.project.projectId);
        }
        return succeeded;
    }
}

#define BuildWindowsGame Gate6BuildWindowsGameWithDiagnostics
#define main RenegadeReusableAssetPackageAcceptanceOriginalMain
#include "ReusableAssetPackageAcceptance.cpp"
#undef main
#undef BuildWindowsGame

int main(int argc, char** argv)
{
    return RenegadeReusableAssetPackageAcceptanceOriginalMain(argc, argv);
}
