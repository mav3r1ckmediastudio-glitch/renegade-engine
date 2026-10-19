#pragma once
#include "renegade/bridge/ProjectService.h"
#include <string>
#include <vector>
#include "renegade/bridge/ScreenService.h"
namespace renegade::bridge
{
    const std::vector<std::string>& ProjectUiScaffoldTemplateNames();
    [[nodiscard]] bool MakeProjectUiScaffoldTemplate(const std::string& projectId,const std::string& templateName,
        ScreenDocument& document,std::string& error);
    // Only called inside a private new-project staging directory. Existing
    // projects are never implicitly seeded or upgraded on open.
    [[nodiscard]] bool SeedProjectUiScaffold(ProjectService& project, std::string& error);
}
