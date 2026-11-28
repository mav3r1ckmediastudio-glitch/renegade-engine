#pragma once
#include <filesystem>
#include "wiPlatform.h"
namespace wi { class Application; }
namespace renegade::studio::weapons {
void Choose(const std::filesystem::path& project, const std::filesystem::path& model = {}, bool useAsArms = false);
void Choose(const std::filesystem::path& project,const std::filesystem::path& model,bool useAsArms,bool assembled);
bool Run(wi::Application& application);
bool Wheel(HWND window, WPARAM buttons, LPARAM position);
void Close(wi::Application& application);
}
