#include "renegade/bridge/DependencyService.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "RenegadeDependencyTests: " << message << '\n';
        return 1;
    }
}
int main()
{
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "renegade-lp05-path-tests";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root / "Content/Textures");
    std::ofstream(root / "Content/Textures/Stone.png") << "fixture";

    const auto existing = renegade::bridge::ResolveDependencyPath(
        root.string(), "Content/Textures/../Textures/Stone.png");
    if (!existing.accepted || !existing.exists ||
        existing.canonicalRelativePath != "Content/Textures/Stone.png")
        return Fail("existing dependency did not canonicalize deterministically");

    const auto missing = renegade::bridge::ResolveDependencyPath(
        root.string(), "Content/Audio/missing.ogg");
    if (!missing.accepted || missing.exists ||
        missing.canonicalRelativePath != "Content/Audio/missing.ogg")
        return Fail("missing dependency was not accepted for graph diagnosis");

    const auto escaped = renegade::bridge::ResolveDependencyPath(
        root.string(), "../outside-project.txt");
    if (escaped.accepted)
        return Fail("parent traversal escaped the project boundary");

    const auto absolute = renegade::bridge::ResolveDependencyPath(
        root.string(), fs::absolute(root / "Content/Textures/Stone.png").string());
    if (absolute.accepted)
        return Fail("absolute dependency path was accepted");

    renegade::bridge::DependencyPathRegistry registry;
    const auto first = registry.Register(
        "scene:startup", "Content/Textures/Stone.png");
    if (!first.inserted || !first.diagnostics.empty())
        return Fail("first canonical dependency path was not registered");

    const auto duplicate = registry.Register(
        "scene:secondary", "Content/Textures/Stone.png");
    if (duplicate.inserted || duplicate.diagnostics.size() != 1 ||
        duplicate.diagnostics.front().code !=
            renegade::bridge::DependencyDiagnosticCode::Duplicate)
        return Fail("exact duplicate dependency was not diagnosed");

    const auto caseCollision = registry.Register(
        "scene:secondary", "Content/Textures/stone.png");
    if (caseCollision.inserted || caseCollision.diagnostics.size() != 1 ||
        caseCollision.diagnostics.front().code !=
            renegade::bridge::DependencyDiagnosticCode::CaseCollision)
        return Fail("case-only dependency collision was not diagnosed");

    const auto outside = fs::temp_directory_path() / "renegade-lp05-outside";
    fs::create_directories(outside);
    std::ofstream(outside / "escaped.txt") << "outside";
    fs::create_directory_symlink(outside, root / "Content/Linked", ignored);
    if (!ignored)
    {
        const auto symlinkEscape = renegade::bridge::ResolveDependencyPath(
            root.string(), "Content/Linked/escaped.txt");
        if (symlinkEscape.accepted)
            return Fail("symlink escaped the project boundary");
    }

    fs::remove_all(root, ignored);
    fs::remove_all(outside, ignored);
    std::cout << "RenegadeDependencyTests passed\n";
    return 0;
}
