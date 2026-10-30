#include "RuntimeScriptRuntime.h"
#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptDocumentService.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    int Fail(const std::string& msg)
    {
        std::cerr << "G3 weather Lua runtime failed: " << msg << '\n';
        return 1;
    }
    bool Near(const float a, const float b)
    {
        return std::abs(a - b) < 0.0005f;
    }
    bool WriteScript(const std::filesystem::path& file, const char* text)
    {
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
        if (ec) return false;
        std::ofstream stream(file, std::ios::binary);
        stream << text;
        return static_cast<bool>(stream);
    }
    renegade::bridge::ScriptDocument CreateFixture(
        const std::string& sourcePath, std::string& error)
    {
        using namespace renegade::bridge;
        auto document = CreateScriptDocument(
            GenerateStableId(), GenerateStableId(),
            "Content/Scenes/WeatherG3.wiscene", "weather-g3-tests");
        ScriptSourceBinding source;
        source.sourceId = GenerateStableId();
        source.sourcePath = sourcePath;
        source.presentation = ScriptPresentation::GlobalScript;
        source.apiVersion = renegade::runtime::RuntimeScriptRuntime::ApiVersion;
        source.provenance.kind = ScriptProvenanceKind::Project;
        source.provenance.contentHash = "g3-script-fixture";
        if (!AddScriptAttachment(document,
            CreateScriptAttachment(ScriptScope::Level, {}, source), error))
            std::cerr << "Fixture attachment failed: " << error << '\n';
        return document;
    }
}

int main()
{
    using namespace renegade::bridge;
    using namespace renegade::runtime;
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() /
        ("renegade-weather-g3-" + GenerateStableId());
    const auto script = root / "Content/Scripts/weather.lua";
    std::string error;

    if (!WriteScript(script,
        "return {\n"
        " on_start=function(self)\n"
        "  local w=renegade.weather.get_state()\n"
        "  assert(type(w)=='table' and w.transitioning==false)\n"
        "  local ok,err=renegade.weather.set_preset('hail',3)\n"
        "  assert(ok==false and type(err)=='string')\n"
        "  ok,err=renegade.weather.set_preset('storm',-5)\n"
        "  assert(ok==false and type(err)=='string')\n"
        "  assert(renegade.weather.set_preset('storm',4))\n"
        " end,\n"
        " on_update=function(self,dt)\n"
        "  local w=renegade.weather.get_state()\n"
        "  assert(type(w.progress)=='number')\n"
        "  assert(type(w.cloud_coverage)=='number')\n"
        " end\n"
        "}\n"))
        return Fail("could not write governed script fixture");

    wi::scene::Scene scene;
    auto weatherEntity = scene.Entity_CreateTransform("G3 Weather");
    auto& weather = scene.weathers.Create(weatherEntity);
    weather.SetVolumetricClouds(true);
    weather.volumetricCloudParameters.layerFirst.coverageAmount = 0.30f;
    scene.weather = weather;
    auto document = CreateFixture("Content/Scripts/weather.lua", error);
    if (!error.empty()) return Fail("fixture setup: " + error);
    RuntimeScriptRuntime runtime;
    if (!runtime.StartScene(document, scene, root.generic_u8string(), error))
        return Fail("governed weather StartScene: " + error);
    if (runtime.DisabledInstanceCount() != 0 || !runtime.Diagnostics().empty())
        return Fail("weather Lua on_start failed or was disabled");
    if (!Near(scene.weathers[0].volumetricCloudParameters.layerFirst.coverageAmount, .30f))
        return Fail("nonzero duration changed weather instantly");
    for (int i=0; i<8; ++i) runtime.Update(.25f);
    const auto half = scene.weathers[0].volumetricCloudParameters.layerFirst.coverageAmount;
    if (!(half > .30f && half < .95f) || runtime.DisabledInstanceCount() != 0)
        return Fail("gameplay transition failed to interpolate");
    runtime.Pause();
    for (int i=0; i<6; ++i) runtime.Update(.25f);
    if (!Near(half,scene.weathers[0].volumetricCloudParameters.layerFirst.coverageAmount))
        return Fail("paused weather transition advanced");
    runtime.Resume();
    for (int i=0; i<8; ++i) runtime.Update(.25f);
    if (!Near(scene.weathers[0].volumetricCloudParameters.layerFirst.coverageAmount,.95f) ||
        !runtime.Diagnostics().empty())
        return Fail("Storm transition failed to finish or script got an error");
    runtime.StopScene();

    if (!WriteScript(script,
        "return {on_start=function(self)\n"
        "  local ok,err=renegade.weather.set_preset('clear')\n"
        "  assert(ok and err==nil)\n"
        "  local w=renegade.weather.get_state()\n"
        "  assert(w.transitioning==false and w.progress==1)\n"
        "end}\n"))
        return Fail("could not rewrite instant fixture");
    auto instant = CreateFixture("Content/Scripts/weather.lua", error);
    if (!runtime.StartScene(instant, scene, root.generic_u8string(), error))
        return Fail("instant StartScene: " + error);
    if (runtime.DisabledInstanceCount() || !runtime.Diagnostics().empty() ||
        CaptureWeather(scene.weathers[0]).skyMode != WeatherState::SkyMode::Realistic)
        return Fail("instant Clear API failed");
    runtime.StopScene();

    if (!WriteScript(script,
        "local tick=0\n"
        "return {on_start=function(self)\n"
        "  assert(renegade.weather.set_preset('storm',4))\n"
        " end, on_update=function(self,dt)\n"
        "  tick=tick+1\n"
        "  if tick==2 then assert(renegade.weather.cancel()) end\n"
        " end}\n"))
        return Fail("could not rewrite cancel fixture");
    auto cancel = CreateFixture("Content/Scripts/weather.lua", error);
    if (!runtime.StartScene(cancel, scene, root.generic_u8string(), error))
        return Fail("cancel StartScene: " + error);
    runtime.Update(.25f);
    runtime.Update(.25f);
    const float cancelled = scene.weathers[0].volumetricCloudParameters.layerFirst.coverageAmount;
    for (int i=0; i<9; ++i) runtime.Update(.25f);
    if (!Near(cancelled,
        scene.weathers[0].volumetricCloudParameters.layerFirst.coverageAmount) ||
        !runtime.Diagnostics().empty())
        return Fail("runtime cancel did not retain interpolated weather");
    runtime.StopScene();
    runtime.Shutdown();

    std::error_code ec;
    fs::remove_all(root, ec);
    std::cout << "PASS: governed Lua set_preset, get_state, cancel, "
                 "transition progress, pause/resume, invalid input, instant apply\n";
    return 0;
}
