#include "renegade/bridge/ScriptLibraryService.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;

    bool Expect(const bool condition, const std::string& message)
    {
        if (condition)
            return true;
        std::cerr << "S6 script library test failed: " << message << '\n';
        return false;
    }

    bool WriteText(
        const fs::path& path,
        const std::string& text)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
            return false;
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << text;
        return static_cast<bool>(stream);
    }

    std::string ReadText(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::string(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
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

    std::string EntrySource(const std::string& version)
    {
        return R"LUA(if renegade and renegade.metadata then
    renegade.metadata({
        schema_version = 1,
        name = "Library Movement",
        description = "S6 package entry.",
        category = "Gameplay",
        role = "SCRIPT",
        properties = {},
    })
end

local helper = require("Library.test.package.Shared.Helper")
return { on_start = function(self) return helper end }
)LUA" + std::string("\n-- package ") + version + "\n";
    }

    std::string HelperSource(const std::string& version)
    {
        return "return { version = \"" + version + "\" }\n";
    }

    std::string Manifest(
        const std::string& packageId,
        const std::string& packageVersion,
        const std::string& displayName,
        const std::string& entryPath,
        const std::string& entryHash,
        const std::string& helperPath,
        const std::string& helperHash,
        const bool cycle = false)
    {
        const std::string entryDependencies = cycle
            ? "[\"" + helperPath + "\"]"
            : "[\"" + helperPath + "\"]";
        const std::string helperDependencies = cycle
            ? "[\"" + entryPath + "\"]"
            : "[]";
        return
            "{\n"
            "  \"schema\": \"renegade-script-package\",\n"
            "  \"schema_version\": 1,\n"
            "  \"package_id\": \"" + packageId + "\",\n"
            "  \"package_version\": \"" + packageVersion + "\",\n"
            "  \"display_name\": \"" + displayName + "\",\n"
            "  \"files\": [\n"
            "    { \"path\": \"" + entryPath + "\", \"content_hash\": \"" +
                entryHash + "\", \"dependencies\": " + entryDependencies + " },\n"
            "    { \"path\": \"" + helperPath + "\", \"content_hash\": \"" +
                helperHash + "\", \"dependencies\": " + helperDependencies + " }\n"
            "  ],\n"
            "  \"entries\": [\"" + entryPath + "\"]\n"
            "}\n";
    }

    bool WritePackage(
        const fs::path& libraryRoot,
        const std::string& folder,
        const std::string& packageId,
        const std::string& version,
        const bool cycle = false)
    {
        const fs::path packageRoot = libraryRoot / folder;
        const std::string entry = EntrySource(version);
        const std::string helper = HelperSource(version);
        if (!WriteText(packageRoot / "Entry.lua", entry) ||
            !WriteText(packageRoot / "Shared" / "Helper.lua", helper))
        {
            return false;
        }
        return WriteText(
            packageRoot / ScriptLibraryPackageFilename,
            Manifest(
                packageId,
                version,
                folder,
                "Entry.lua",
                ContentHash(entry),
                "Shared/Helper.lua",
                ContentHash(helper),
                cycle));
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
    bool ok = true;
    const fs::path root = fs::temp_directory_path() /
        "renegade_s6_script_library_tests";
    const fs::path projectRoot = root / "Project";
    const fs::path libraryRoot = root / "InstalledLibrary";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(projectRoot / "Content" / "Scripts", ec);
    fs::create_directories(libraryRoot, ec);
    ok = Expect(!ec, "create S6 temporary roots") && ok;

    ok = Expect(
        WritePackage(libraryRoot, "Movement Pack", "test.package", "1.0.0"),
        "write immutable v1 package") && ok;

    ScriptLibraryService service;
    std::vector<ScriptLibraryEntry> entries;
    std::vector<ScriptMetadataDiagnostic> diagnostics;
    std::string error;
    ok = Expect(
        service.EnumerateEntries(
            projectRoot.generic_u8string(),
            ScriptPresentation::Script,
            entries,
            diagnostics,
            error,
            libraryRoot.generic_u8string()),
        "enumerate v1 package: " + error) && ok;
    ok = Expect(entries.size() == 1, "v1 exposes exactly one SCRIPT entry") && ok;
    ok = Expect(diagnostics.empty(), "v1 has no library diagnostics") && ok;
    if (entries.size() == 1)
    {
        ok = Expect(
            entries[0].projectSourcePath ==
                "Content/Scripts/Library/test.package/Entry.lua",
            "entry projects into project-owned Content/Scripts") && ok;
        ok = Expect(
            entries[0].binding.provenance.kind ==
                ScriptProvenanceKind::InstalledLibrary,
            "entry preserves installed-library provenance") && ok;
        ok = Expect(
            entries[0].binding.dependencies.size() == 1 &&
                entries[0].binding.dependencies[0].pathHint ==
                    "Content/Scripts/Library/test.package/Shared/Helper.lua",
            "entry carries deterministic transitive module closure") && ok;
    }

    ScriptLibraryAdoptionResult adoptedV1;
    ok = Expect(
        service.AdoptEntry(
            projectRoot.generic_u8string(),
            (libraryRoot / "Movement Pack" / ScriptLibraryPackageFilename)
                .generic_u8string(),
            "Entry.lua",
            adoptedV1,
            error),
        "transactionally adopt v1: " + error) && ok;
    const StableId stableSourceId = adoptedV1.binding.sourceId;
    ok = Expect(
        fs::is_regular_file(
            projectRoot / "Content" / "Scripts" / "Library" /
            "test.package" / "Entry.lua"),
        "adoption writes entry into project") && ok;
    ok = Expect(
        fs::is_regular_file(
            projectRoot / "Content" / "Scripts" / "Library" /
            "test.package" / "Shared" / "Helper.lua"),
        "adoption writes transitive helper into project") && ok;
    ok = Expect(
        fs::is_regular_file(projectRoot / fs::u8path(ScriptLibraryLockRelativePath)),
        "adoption writes durable lock/provenance baseline") && ok;

    entries.clear();
    diagnostics.clear();
    ok = Expect(
        service.EnumerateEntries(
            projectRoot.generic_u8string(),
            ScriptPresentation::Script,
            entries,
            diagnostics,
            error,
            libraryRoot.generic_u8string()),
        "re-enumerate adopted v1: " + error) && ok;
    ok = Expect(entries.size() == 1 && entries[0].adopted,
        "adopted package reopens as adopted") && ok;

    // Clean update: project copy still matches the recorded baseline, so S6 may
    // replace the complete package closure and preserve stable source identity.
    ok = Expect(
        WritePackage(libraryRoot, "Movement Pack", "test.package", "2.0.0"),
        "write v2 package") && ok;
    ScriptLibraryAdoptionResult adoptedV2;
    error.clear();
    ok = Expect(
        service.AdoptEntry(
            projectRoot.generic_u8string(),
            (libraryRoot / "Movement Pack" / ScriptLibraryPackageFilename)
                .generic_u8string(),
            "Entry.lua",
            adoptedV2,
            error),
        "clean v2 update succeeds: " + error) && ok;
    ok = Expect(adoptedV2.updated, "v2 is reported as an update") && ok;
    ok = Expect(adoptedV2.binding.sourceId == stableSourceId,
        "package update preserves deterministic source identity") && ok;
    ok = Expect(
        ReadText(
            projectRoot / "Content" / "Scripts" / "Library" /
            "test.package" / "Entry.lua") == EntrySource("2.0.0"),
        "clean update replaces project copy") && ok;

    // Creator modification is authoritative. A newer installed package must not
    // overwrite it, and enumeration must expose the conflict diagnostically.
    const fs::path projectEntry =
        projectRoot / "Content" / "Scripts" / "Library" /
        "test.package" / "Entry.lua";
    const std::string creatorModified =
        ReadText(projectEntry) + "-- creator local edit\n";
    ok = Expect(WriteText(projectEntry, creatorModified),
        "write creator modification") && ok;
    ok = Expect(
        WritePackage(libraryRoot, "Movement Pack", "test.package", "3.0.0"),
        "write v3 package") && ok;

    ScriptLibraryAdoptionResult blocked;
    error.clear();
    ok = Expect(
        !service.AdoptEntry(
            projectRoot.generic_u8string(),
            (libraryRoot / "Movement Pack" / ScriptLibraryPackageFilename)
                .generic_u8string(),
            "Entry.lua",
            blocked,
            error),
        "creator-modified project copy blocks v3 overwrite") && ok;
    ok = Expect(
        error.find("local edits") != std::string::npos ||
            error.find("creator-modified") != std::string::npos,
        "blocked update reports creator conflict") && ok;
    ok = Expect(ReadText(projectEntry) == creatorModified,
        "blocked update preserves creator bytes exactly") && ok;

    entries.clear();
    diagnostics.clear();
    error.clear();
    ok = Expect(
        service.EnumerateEntries(
            projectRoot.generic_u8string(),
            ScriptPresentation::Script,
            entries,
            diagnostics,
            error,
            libraryRoot.generic_u8string()),
        "enumerate v3 conflict state: " + error) && ok;
    const auto conflicted = std::find_if(
        entries.begin(), entries.end(),
        [](const ScriptLibraryEntry& entry)
        {
            return entry.packageId == "test.package";
        });
    ok = Expect(
        conflicted != entries.end() && conflicted->localConflict &&
            conflicted->updateAvailable,
        "Creator Library surfaces update + local conflict state") && ok;

    // Existing project bytes without an S6 lock are not adoption authority.
    ok = Expect(
        WritePackage(libraryRoot, "Collision Pack", "collision.package", "1.0.0"),
        "write collision package") && ok;
    const fs::path collisionDestination =
        projectRoot / "Content" / "Scripts" / "Library" /
        "collision.package" / "Entry.lua";
    ok = Expect(WriteText(collisionDestination, "return {}\n"),
        "seed unrelated project file at adoption destination") && ok;
    ScriptLibraryAdoptionResult collision;
    error.clear();
    ok = Expect(
        !service.AdoptEntry(
            projectRoot.generic_u8string(),
            (libraryRoot / "Collision Pack" / ScriptLibraryPackageFilename)
                .generic_u8string(),
            "Entry.lua",
            collision,
            error),
        "unowned project-path collision blocks adoption") && ok;
    ok = Expect(error.find("collision") != std::string::npos,
        "collision failure is explicit") && ok;

    // Cycles are rejected from the deterministic package closure without
    // executing/scanning Lua source text.
    ok = Expect(
        WritePackage(libraryRoot, "Cycle Pack", "cycle.package", "1.0.0", true),
        "write cyclic package") && ok;
    entries.clear();
    diagnostics.clear();
    error.clear();
    ok = Expect(
        service.EnumerateEntries(
            projectRoot.generic_u8string(),
            ScriptPresentation::Script,
            entries,
            diagnostics,
            error,
            libraryRoot.generic_u8string()),
        "library enumeration survives invalid cyclic package") && ok;
    ok = Expect(
        HasDiagnostic(diagnostics, "S6_LIBRARY_CLOSURE_INVALID"),
        "cyclic package produces structured closure diagnostic") && ok;

    fs::remove_all(root, ec);
    return ok ? 0 : 1;
}
