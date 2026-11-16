#include "renegade/bridge/DialogueDisplayService.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace renegade::bridge;
#define CHECK(x) do { if(!(x)){std::cerr<<"Dialogue display line "<<__LINE__<<": "<<#x<<" "<<error<<'\n';return 1;} }while(false)
int main(){
 namespace fs=std::filesystem;std::string error;auto project=GenerateStableId();
 const auto root=fs::temp_directory_path()/("renegade-dialogue-display-"+GenerateStableId());
 fs::create_directories(root/"Content/UI");
 auto source=MakeDialogueDisplayTemplate(project,"Content/UI/Dialogue.renegade-screen");
 CHECK(ValidateDialogueDisplay(source,project,error));
 auto find=[](auto& d,const std::string& role)->ScreenWidget&{return *std::find_if(d.widgets.begin(),d.widgets.end(),[&](const auto& w){return w.dialogueRole==role;});};
 auto& body=find(source,"text");body.name="Renamed by the creator";body.style.text.fontSize=31;
 body.style.normal.foreground={225,185,120,255};body.rect={80,70,450,160};
 body.style.text.fontResource="Content/UI/custom.ttf";
 {std::ofstream(root/"Content/UI/custom.ttf")<<"dependency fixture font bytes";}
 auto& backdrop=source.widgets.front();backdrop.rect={40,240,1200,280};
 backdrop.style.normal.background={45,18,8,255};backdrop.style.borderWidth=3;
 backdrop.resourcePath="Content/UI/panel.png";
 {std::ofstream(root/"Content/UI/panel.png")<<"dependency fixture image bytes";}
 const auto file=(root/"Content/UI/Dialogue.renegade-screen").generic_u8string();
 CHECK(WriteScreenDocument(file,source,error));
 ScreenDocument reopened;CHECK(ReadScreenDocument(file,project,reopened,error));
 CHECK(find(reopened,"text").name==body.name&&find(reopened,"text").style.text.fontSize==31);
 CHECK(SetDialogueDisplayAssignment(root.generic_u8string(),project,{source.envelope.documentId,"Content/UI/Dialogue.renegade-screen"},error));
 ScreenDocument loaded;CHECK(LoadDialogueDisplay(root.generic_u8string(),project,loaded,error));
 CHECK(find(loaded,"text").style.text.fontResource==body.style.text.fontResource);
 DialogueView view;view.active=true;view.speaker="Guard";view.text="What business have you here?";
 for(int n=0;n<6;++n)view.replies.push_back({"r"+std::to_string(n),"Reply "+std::to_string(n)});
 ScreenDocument rendered;std::vector<StableId> controls;std::vector<std::string> actions;
 CHECK(BuildDialogueDisplay(loaded,view,0,rendered,controls,actions,error));
 CHECK(find(rendered,"speaker").text==view.speaker&&find(rendered,"text").text==view.text);
 CHECK(find(rendered,"text").style.text.fontSize==31&&find(rendered,"text").rect.x==80);
 CHECK(rendered.widgets.front().style.normal.background.red==45);
 CHECK(find(rendered,"reply4").visible&&!find(rendered,"continue").visible&&find(rendered,"next").visible);
 CHECK(!find(rendered,"previous").visible);
 CHECK(actions.size()==6&&actions[0]=="r0");
 CHECK(BuildDialogueDisplay(loaded,view,1,rendered,controls,actions,error));
 CHECK(find(rendered,"reply1").text=="Reply 4"&&!find(rendered,"reply3").visible);
 CHECK(find(rendered,"previous").visible&&!find(rendered,"next").visible);
 CHECK(actions.front()=="r4");
 view.replies.clear();CHECK(BuildDialogueDisplay(loaded,view,0,rendered,controls,actions,error));
 CHECK(find(rendered,"continue").visible&&!find(rendered,"reply1").visible);
 CHECK(actions.size()==2&&actions.front()=="@continue"&&actions.back()=="@close");
 ScreenAuthoringSession author;CHECK(author.Open(file,project,error));
 CHECK(author.UpdateDialogueRole(body.id,"speaker",error));
 CHECK(author.Undo(error));CHECK(author.Save(error));
 CHECK(ReadScreenDocument(file,project,reopened,error)&&find(reopened,"text").id==body.id);
 auto bad=loaded;find(bad,"speaker").dialogueRole="text";
 CHECK(!ValidateDialogueDisplay(bad,project,error));
 bad=loaded;find(bad,"reply4").dialogueRole.clear();CHECK(!ValidateDialogueDisplay(bad,project,error));
 bad=loaded;bad.widgets.front().visible=false;CHECK(!ValidateDialogueDisplay(bad,project,error));
 bad=loaded;find(bad,"text").kind=ScreenWidgetKind::Image;CHECK(!ValidateDialogueDisplay(bad,project,error));
 CHECK(!SetDialogueDisplayAssignment(root.generic_u8string(),GenerateStableId(),{source.envelope.documentId,"Content/UI/Dialogue.renegade-screen"},error));
 const auto snapshot=root/"Snapshot";
 CHECK(SnapshotDialogueDisplay(root.generic_u8string(),snapshot.generic_u8string(),project,error));
 CHECK(LoadDialogueDisplay(snapshot.generic_u8string(),project,reopened,error));
 CHECK(fs::exists(snapshot/"Content/UI/custom.ttf")&&fs::exists(snapshot/"Content/UI/panel.png"));
 CHECK(find(reopened,"text").style.text.fontSize==31);
 fs::remove(root/"Content/UI/panel.png");
 CHECK(!SnapshotDialogueDisplay(root.generic_u8string(),(root/"Broken").generic_u8string(),project,error));
 CHECK(SetDialogueDisplayAssignment(root.generic_u8string(),project,{},error));
 CHECK(LoadDialogueDisplay(root.generic_u8string(),project,reopened,error)&&reopened.envelope.documentId.empty());
 fs::remove_all(root);std::cout<<"Dialogue display: roles, creator styles, pagination, authoring Undo, persistence and snapshot dependencies passed.\n";return 0;
}
