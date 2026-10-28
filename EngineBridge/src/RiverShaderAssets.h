#pragma once
// Runtime Content/river paths MUST be executable-rooted. Native Win32 file
// dialogs can change the process working directory, and a relative lookup
// silently loses shader and foam artwork in the editor after Open Project.
#include "WickedEngine.h"
#include <filesystem>
#include <string>
namespace renegade::bridge::river_shader_assets {
inline std::string Directory(){
    return (std::filesystem::u8path(wi::helper::GetExecutablePath()).parent_path()/
        "Content"/"river").generic_u8string();
}
}
