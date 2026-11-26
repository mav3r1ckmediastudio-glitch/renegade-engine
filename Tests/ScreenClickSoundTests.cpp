#include "RuntimeScriptRuntime.h"
#include "renegade/bridge/ScreenClickSound.h"
#include <filesystem>
#include <fstream>
#include <iostream>

int main()
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;
    using renegade::runtime::RuntimeScriptRuntime;
    const auto root = fs::temp_directory_path() / ("renegade-ui-sound-" + GenerateStableId());
    fs::create_directories(root / "Content/Scripts");
    fs::create_directories(root / "Content/Audio/UI");
    const std::string clip = "Content/Audio/UI/click.wav";
    const unsigned char wav[] = {
        'R','I','F','F',38,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
        1,0,1,0,0x44,0xac,0,0,0x88,0x58,1,0,2,0,16,0,'d','a','t','a',2,0,0,0,0,0};
    std::ofstream(root/clip,std::ios::binary).write(reinterpret_cast<const char*>(wav),sizeof(wav));
    ScreenClickSounds authoring;
    authoring.projectDefault=clip;
    std::string error;
    if (!authoring.Save(root.generic_u8string(),error)) {std::cerr<<error;return 1;}
    const std::string sourcePath="Content/Scripts/test.lua";
    std::ofstream(root/sourcePath) << R"lua(
return {on_start=function()
  local sound = renegade.ui.button_click_sound
  local clip = 'Content/Audio/UI/click.wav'
  local function check(expected, mode, ...)
    local path, actual = sound('screen','button',...)
    assert(path == expected and actual == mode)
  end
  check(clip,'default')
  check('','silent','silent')
  check('','silent')
  check(clip,'local','local',clip)
  check(clip,'local')
  check(clip,'default','default')
  check(clip,'default')
end}
)lua";
    auto document=CreateScriptDocument(GenerateStableId(),GenerateStableId(),
        "Content/Scenes/Test.wiscene","ui-sound-test");
    ScriptSourceBinding source;
    source.sourceId=GenerateStableId();
    source.sourcePath=sourcePath;
    source.presentation=ScriptPresentation::GlobalScript;
    source.apiVersion=RuntimeScriptRuntime::ApiVersion;
    source.provenance.kind=ScriptProvenanceKind::Project;
    source.provenance.contentHash="button-click-sound-fixture";
    if (!AddScriptAttachment(document,CreateScriptAttachment(ScriptScope::Level,{},source),error))
    {std::cerr<<error;return 1;}
    wi::scene::Scene scene;
    RuntimeScriptRuntime runtime;
    if (!runtime.StartScene(document,scene,root.generic_u8string(),error) ||
        runtime.DisabledInstanceCount() || !runtime.Diagnostics().empty())
    {std::cerr<<"Button sound script failed: "<<error;return 1;}
    runtime.StopScene();
    if (!ScreenClickSounds::Read(root.generic_u8string()).buttons.empty()) return 1;
    fs::remove_all(root);
    return 0;
}
