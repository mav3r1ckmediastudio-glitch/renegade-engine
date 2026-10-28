#pragma once
#include "renegade/bridge/ProjectService.h"
#include <filesystem>
#include <fstream>
#include <string>

// Explicit opt-in, exact compiled source BUILD root and known test identity.
// A similarly named user project elsewhere must never qualify for fixture writes.
inline bool IsDisposableG5Project(const std::filesystem::path& root,
                                  const char* expectedName)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto build = fs::weakly_canonical(
        fs::u8path(__FILE__).parent_path().parent_path() / "BUILD", ec);
    if (ec || root.filename() != expectedName || root.parent_path() != build)
        return false;
    std::ifstream marker(root / "G5-INTEGRATION-DISPOSABLE.txt");
    std::string line;
    if (!std::getline(marker, line) || line != "RENEGADE_G5_DISPOSABLE_V1")
        return false;
    renegade::bridge::ProjectService projects;
    renegade::bridge::ProjectMetadata project;
    std::string error;
    return projects.InspectProject((root / "ScreenProject.renegade").generic_u8string(),
        project, error) && project.projectId == "61111111-1111-4111-8111-111111111111";
}
