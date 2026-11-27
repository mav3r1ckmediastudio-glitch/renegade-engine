#include "RuntimeScriptRuntime.h"
#include "renegade/bridge/ScreenClickSound.h"
#include "renegade/bridge/DialogueAssetService.h"
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
    // Extensible PCM from the supplied SFX pack is recontainerised, not decoded
    // by the unsafe loader and not resampled. Corrupt/unsupported inputs stay rejected.
    std::vector<unsigned char> extended={'R','I','F','F',62,0,0,0,'W','A','V','E','f','m','t',' ',40,0,0,0,
        0xfe,0xff,1,0,0x44,0xac,0,0,0x88,0x58,1,0,2,0,16,0,22,0,16,0,4,0,0,0,
        1,0,0,0,0,0,16,0,128,0,0,170,0,56,155,113,'d','a','t','a',2,0,0,0,0x34,0x12};
    const auto external=root/"extensible.wav";
    const auto put=[&]{std::ofstream file(external,std::ios::binary);file.write(reinterpret_cast<const char*>(extended.data()),extended.size());};
    put();std::string adopted;
    if(ValidateAudioAssetForWicked(external.generic_u8string(),error)||
        !ImportUiClickSound(root.generic_u8string(),external.generic_u8string(),adopted,error)||
        !ValidateAudioAssetForWicked((root/adopted).generic_u8string(),error))
    {std::cerr<<"Extensible UI PCM import: "<<error;return 1;}
    std::ifstream converted(root/adopted,std::ios::binary);std::vector<unsigned char> pcm{std::istreambuf_iterator<char>(converted),{}};converted.close();
    if(pcm.size()!=46||pcm[20]!=1||pcm[44]!=0x34||pcm[45]!=0x12||fs::file_size(external)!=extended.size()){std::cerr<<"PCM payload mismatch size="<<pcm.size()<<" source="<<fs::file_size(external)<<" expected="<<extended.size();return 1;}
    const auto reject=[&]{std::string path;return !ImportUiClickSound(root.generic_u8string(),external.generic_u8string(),path,error)&&path.empty();};
    extended[44]=3;put();if(!reject()){std::cerr<<"Float subtype accepted";return 1;} // IEEE-float subtype is not PCM.
    extended[44]=1;extended[64]=0xff;put();if(!reject()){std::cerr<<"Oversized data accepted";return 1;} // data chunk exceeds file.
    extended[64]=2;extended[38]=8;put();if(!reject()){std::cerr<<"Invalid bit layout accepted";return 1;} // valid bits disagree with container.
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
    std::cout<<"PASS: UI sound Lua overrides and lossless extensible PCM import/rejections\n";
    return 0;
}
