#include "InventoryG5DisposableProject.h"
// G5H health-potion demonstration HUD authoring; writes only disposable BUILD/G5PackagePlay.
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/ScreenService.h"
#include "renegade/bridge/ScreenWidgetPresets.h"
#include <filesystem>
#include <iostream>
#include <string>
using namespace renegade::bridge;
namespace fs=std::filesystem;
#define REQUIRE(expr) do{if(!(expr)){std::cerr<<"G5 HEALTH HUD: "<<#expr<<" at "<<__LINE__<<" / "<<error<<" "<<projects.LastError()<<"\n";return 1;}}while(false)
int main(int argc,char** argv) {
    if(argc!=2 && !(argc==3 && std::string(argv[2])=="--relayout"))return 2;
    const bool relayout=argc==3;
    std::error_code ec;
    const auto root=fs::weakly_canonical(fs::u8path(argv[1]),ec);
    if(ec || root.filename()!="G5PackagePlay" ||
       !IsDisposableG5Project(root,"G5PackagePlay"))
       return 3;
    std::string error;
    ProjectService projects;
    projects.Initialize((root/"G5HealthHudEditor.ini").generic_u8string());
    REQUIRE(projects.OpenProject((root/"ScreenProject.renegade").generic_u8string()));
    const auto project=projects.CurrentProject();
    REQUIRE(project.projectId=="61111111-1111-4111-8111-111111111111");
    const std::string rel="Content/UI/G5HealthTestHud.renegade-screen";
    const auto target=root/fs::u8path(rel);
    if(relayout) {
        REQUIRE(fs::is_regular_file(target));
        ScreenDocument existing;
        REQUIRE(ReadScreenDocument(target.generic_u8string(),project.projectId,existing,error));
        REQUIRE(existing.usage==ScreenUsage::Hud && existing.widgets.size()==4);
        REQUIRE(existing.widgets[0].name=="Health Test HUD Background");
        REQUIRE(existing.widgets[1].name=="Player Health (Live)");
        REQUIRE(existing.widgets[2].name=="Player Health Bar (Live)");
        REQUIRE(existing.widgets[3].name=="Health Test Instructions");
        existing.widgets[0].rect={921,10,331,88};
        existing.widgets[1].rect={938,20,285,35};
        existing.widgets[2].rect={939,60,288,21};
        existing.widgets[3].rect={358,30,554,35};
        existing.widgets[3].text="START 50 HP  |  USE POTION +50  |  H DAMAGE -25";
        REQUIRE(WriteScreenDocument(target.generic_u8string(),existing,error));
        std::cout<<"PASS G5 health HUD moved to top-right, FPS overlay no longer obscures it\n";
        return 0;
    }
    REQUIRE(!fs::exists(target));
    ScreenDocument hud;
    hud.usage=ScreenUsage::Hud;
    hud.enabledInGame=true;
    hud.envelope=CreateDocumentEnvelope(project.projectId,RuntimeScreenDocumentType,
        rel,"G5 editable player health test HUD");
    hud.designWidth=1280;hud.designHeight=720;
    auto panel=MakeScreenWidgetPreset(ScreenWidgetPreset::Panel,1280,720);
    panel.id=GenerateStableId();panel.name="Health Test HUD Background";
    panel.rect={18,10,336,88};panel.style.normal.background={11,18,27,240};
    panel.style.cornerRadius=9;
    hud.widgets.push_back(panel);
    auto current=MakeScreenWidgetPreset(ScreenWidgetPreset::PlayerHealthText,1280,720);
    current.id=GenerateStableId();current.name="Player Health (Live)";
    current.rect={32,20,300,35};current.style.text.fontSize=19;
    current.style.normal.foreground={237,246,251,255};
    hud.widgets.push_back(current);
    auto progress=MakeScreenWidgetPreset(ScreenWidgetPreset::PlayerHealthBar,1280,720);
    progress.id=GenerateStableId();progress.name="Player Health Bar (Live)";
    progress.rect={33,60,303,21};
    progress.style.normal.background={52,68,76,255};
    progress.style.normal.foreground={59,214,124,255};
    progress.style.cornerRadius=4;
    hud.widgets.push_back(progress);
    ScreenWidget help;
    help.id=GenerateStableId();help.kind=ScreenWidgetKind::Text;
    help.name="Health Test Instructions";
    help.rect={379,30,610,35};
    help.text="H  TAKE 25 DAMAGE (TEST)    |    I  INVENTORY / POTION";
    help.style=MakeScreenWidgetStyleTemplate(help.kind,help.rect.height);
    help.style.text.fontSize=16;
    help.style.normal.foreground={224,239,249,255};
    hud.widgets.push_back(help);
    REQUIRE(WriteScreenDocument(target.generic_u8string(),hud,error));
    REQUIRE(projects.SetDefaultHud(hud.envelope.documentId,rel));
    ScreenDocument read;
    REQUIRE(ReadScreenDocument(target.generic_u8string(),project.projectId,read,error));
    REQUIRE(read.usage==ScreenUsage::Hud && read.widgets.size()==4);
    REQUIRE(read.widgets[1].bindings[0].source.attributeId=="health.current");
    REQUIRE(read.widgets[2].bindings[0].denominator.attributeId=="health.maximum");
    ProjectMetadata disk;
    REQUIRE(projects.InspectProject((root/"ScreenProject.renegade").generic_u8string(),disk,error));
    REQUIRE(disk.defaultHudId==hud.envelope.documentId && disk.defaultHud==rel);
    std::cout<<"PASS G5 health HUD authored, editable, bound to real player health, default project HUD; "<<rel<<"\n";
    return 0;
}
