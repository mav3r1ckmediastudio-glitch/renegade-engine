#include "renegade/bridge/ScriptLibraryService.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef RENEGADE_SOURCE_DIR
#error RENEGADE_SOURCE_DIR must be supplied by CMake.
#endif

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    int failures = 0;

    void Check(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    }

    bool WriteText(const fs::path& path, const std::string& text)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
            return false;
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << text;
        return static_cast<bool>(stream);
    }

    std::uint64_t Fnv1a64(const std::string& value)
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char c : value)
        {
            hash ^= static_cast<std::uint64_t>(c);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    std::string ContentHash(const std::string& value)
    {
        std::ostringstream stream;
        stream << "fnv1a64:" << std::hex << std::setfill('0')
            << std::setw(16) << Fnv1a64(value);
        return stream.str();
    }

    bool WriteExternalPackage(const fs::path& libraryRoot)
    {
        const fs::path packageRoot = libraryRoot / "External Example";
        const std::string source = R"LUA(if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "External Example Action",
        description = "S7 external Creator Library coexistence proof.",
        category = "Test",
        role = "ACTION",
        properties = {},
    })
end

return {}
)LUA";
        if (!WriteText(packageRoot / "External.lua", source))
            return false;

        const std::string manifest =
            "{\n"
            "  \"schema\": \"renegade-script-package\",\n"
            "  \"schema_version\": 1,\n"
            "  \"package_id\": \"example.external.action\",\n"
            "  \"package_version\": \"1.0.0\",\n"
            "  \"display_name\": \"External Example\",\n"
            "  \"files\": [\n"
            "    { \"path\": \"External.lua\", \"content_hash\": \"" +
                ContentHash(source) + "\", \"dependencies\": [] }\n"
            "  ],\n"
            "  \"entries\": [\"External.lua\"]\n"
            "}\n";
        return WriteText(packageRoot / ScriptLibraryPackageFilename, manifest);
    }

    bool HasDiagnostic(
        const std::vector<ScriptMetadataDiagnostic>& diagnostics,
        const std::string& code)
    {
        return std::any_of(
            diagnostics.begin(), diagnostics.end(),
            [&](const ScriptMetadataDiagnostic& diagnostic)
            {
                return diagnostic.code == code;
            });
    }
}

int main()
{
    using namespace renegade::bridge;

    const fs::path originalCwd = fs::current_path();
    const char* originalEnvironment = std::getenv("RENEGADE_SCRIPT_LIBRARY");
    const bool hadOriginalEnvironment = originalEnvironment != nullptr;
    const std::string savedEnvironment = hadOriginalEnvironment
        ? std::string(originalEnvironment)
        : std::string{};

    const auto stamp = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("renegade-s7-library-roots-" + std::to_string(stamp));
    const fs::path projectRoot = root / "Project";
    const fs::path builtinRoot = root / "Content" / "ScriptLibrary";
    const fs::path builtinPackage = builtinRoot / "RenegadeStockActions";
    const fs::path externalRoot = root / "ExternalLibrary";
    const fs::path sourcePackage = fs::u8path(RENEGADE_SOURCE_DIR) /
        "Library" / "Scripts" / "RenegadeStockActions";

    std::error_code ec;
    fs::create_directories(projectRoot / "Content" / "Scripts", ec);
    Check(!ec, "create temporary project root");
    fs::create_directories(builtinRoot, ec);
    Check(!ec, "create built-in library root");
    fs::create_directories(externalRoot, ec);
    Check(!ec, "create external library root");

    ec.clear();
    fs::copy(
        sourcePackage,
        builtinPackage,
        fs::copy_options::recursive | fs::copy_options::overwrite_existing,
        ec);
    Check(!ec, "copy official stock package into simulated built-in root");

    ec.clear();
    fs::copy(
        sourcePackage,
        externalRoot / "Duplicate Stock Package",
        fs::copy_options::recursive | fs::copy_options::overwrite_existing,
        ec);
    Check(!ec, "copy shadow attempt into external Creator Library");
    Check(WriteExternalPackage(externalRoot),
        "write independent external Creator Library package");

    fs::current_path(root, ec);
    Check(!ec, "switch to simulated packaged Studio working directory");
#ifdef _WIN32
    Check(_putenv_s(
            "RENEGADE_SCRIPT_LIBRARY",
            externalRoot.generic_u8string().c_str()) == 0,
        "set external Creator Library environment root");
#else
    Check(setenv(
            "RENEGADE_SCRIPT_LIBRARY",
            externalRoot.generic_u8string().c_str(),
            1) == 0,
        "set external Creator Library environment root");
#endif

    ScriptLibraryService service;
    std::vector<ScriptLibraryEntry> entries;
    std::vector<ScriptMetadataDiagnostic> diagnostics;
    std::string error;
    Check(service.EnumerateEntries(
            projectRoot.generic_u8string(),
            ScriptPresentation::Action,
            entries,
            diagnostics,
            error),
        "enumerate built-in + external Action libraries: " + error);

    const auto stockCount = static_cast<std::size_t>(std::count_if(
        entries.begin(), entries.end(),
        [](const ScriptLibraryEntry& entry)
        {
            return entry.packageId == "renegade.stock.actions.wave_a";
        }));
    const auto externalCount = static_cast<std::size_t>(std::count_if(
        entries.begin(), entries.end(),
        [](const ScriptLibraryEntry& entry)
        {
            return entry.packageId == "example.external.action";
        }));

    Check(stockCount == 6,
        "built-in Renegade stock package exposes exactly six Actions");
    Check(externalCount == 1,
        "external Creator Library remains visible beside built-in stock Actions");
    Check(entries.size() == 7,
        "combined library contains six stock Actions plus one external Action");
    Check(HasDiagnostic(diagnostics, "S7_STOCK_LIBRARY_SHADOW_BLOCKED"),
        "external duplicate of the official stock package is diagnostically blocked");

    for (const auto& entry : entries)
    {
        if (entry.packageId != "renegade.stock.actions.wave_a")
            continue;
        const fs::path manifest = fs::u8path(entry.manifestPath);
        Check(
            manifest.generic_u8string().find(builtinRoot.generic_u8string()) == 0,
            "official stock Action resolved from built-in package authority");
    }

#ifdef _WIN32
    if (hadOriginalEnvironment)
        _putenv_s("RENEGADE_SCRIPT_LIBRARY", savedEnvironment.c_str());
    else
        _putenv_s("RENEGADE_SCRIPT_LIBRARY", "");
#else
    if (hadOriginalEnvironment)
        setenv("RENEGADE_SCRIPT_LIBRARY", savedEnvironment.c_str(), 1);
    else
        unsetenv("RENEGADE_SCRIPT_LIBRARY");
#endif
    fs::current_path(originalCwd, ec);
    fs::remove_all(root, ec);

    if (failures != 0)
    {
        std::cerr << failures << " S7 stock-library root test(s) failed.\n";
        return 1;
    }

    std::cout << "S7 built-in + external script library tests passed.\n";
    return 0;
}
