#include "renegade/bridge/GameplayAttributeDocumentService.h"
#include "renegade/bridge/GameplayAttributeAuthoringSession.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/DependencyService.h"
#include "RuntimeGameplayAttributeWorld.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
using namespace renegade::bridge;
namespace fs=std::filesystem;
std::string Bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void Put(const fs::path& path,const std::string& value){std::ofstream out(path,std::ios::binary);out<<value;}
std::string WithoutProfileKeys(const std::string& raw)
{
    std::istringstream in(raw);std::string line,out;
    while(std::getline(in,line))if(line.rfind("attribute_document",0)!=0)out+=line+'\n';
    return out;
}
int main()
{
    const auto parent=fs::temp_directory_path()/fs::u8path("renegade-attribute-doc-"+GenerateStableId());
    struct Cleanup{fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{parent};
    fs::create_directories(parent);std::string error;
    auto fail=[&](const char* message){std::cerr<<message<<": "<<error<<'\n';return 1;};
    ProjectService projects;projects.Initialize((parent/"Editor.ini").generic_u8string());
    if(!projects.CreateStoryFlowProject(parent.generic_u8string(),"Attribute Profiles QA", false))return fail(projects.LastError().c_str());
    const auto metadata=projects.CurrentProject();const auto root=fs::u8path(metadata.rootPath);
    GameplayAttributeDocument doc;doc.envelope=CreateDocumentEnvelope(metadata.projectId,GameplayAttributeDocumentType,
        "Content/Gameplay/Profiles.renegade-attributes","Gameplay attribute integration");
    GameplayAttributeProfile profile{GenerateStableId()," Shared profile ; # ",
        {{"energy","Energy",50.0,0.0,100.0,10.0},{"active","Active",true,{},{},0},
         {"label","Literal label",std::string(" 001+2 ; #\n"),{},{},0},
         {"precision","Exact numeric value",.12345678901234567,0.0,1.0,0}}};
    const auto actorA=GenerateStableId(),actorB=GenerateStableId();
    GameplayAttributeOverride playerOverride;playerOverride.attributeId="energy";playerOverride.initial=20.0;
    GameplayAttributeOverride actorOverride;actorOverride.attributeId="energy";actorOverride.initial=30.0;actorOverride.regeneratePerSecond=0;
    doc.profiles={profile};doc.attachments={
        {GameplayAttributeScope::Project,"",profile.id,{}},
        {GameplayAttributeScope::Player,"",profile.id,{playerOverride}},
        {GameplayAttributeScope::Character,actorA,profile.id,{actorOverride}},
        {GameplayAttributeScope::Character,actorB,profile.id,{}}};
    const auto path=root/fs::u8path(doc.envelope.pathHint);fs::create_directories(path.parent_path());
    if(!WriteGameplayAttributeDocument(path.generic_u8string(),doc,error))return fail("profile write");
    GameplayAttributeDocument parsed;
    if(!ReadGameplayAttributeDocument(path.generic_u8string(),metadata.projectId,parsed,error)||
        parsed.profiles[0].name!=profile.name||std::get<std::string>(parsed.profiles[0].attributes[2].initial)!=" 001+2 ; #\n"||
        std::get<double>(parsed.profiles[0].attributes[3].initial)!=.12345678901234567||
        parsed.attachments[1].overrides[0].initial!=playerOverride.initial||
        parsed.attachments[2].overrides[0].regeneratePerSecond!=0)
        return fail("profile exact typed/override round trip");
    std::string resolved;
    if(!ResolveGameplayAttributeDocumentPath(root.generic_u8string(),metadata.projectId,doc.envelope.documentId,
        doc.envelope.pathHint,resolved,error))return fail("profile identity resolution");
    const auto duplicate=root/"Content/Gameplay/Duplicate.renegade-attributes";fs::copy_file(path,duplicate);
    if(ResolveGameplayAttributeDocumentPath(root.generic_u8string(),metadata.projectId,doc.envelope.documentId,
        doc.envelope.pathHint,resolved,error))return fail("duplicate profile identity accepted");
    fs::remove(duplicate);
    if(ResolveGameplayAttributeDocumentPath(root.generic_u8string(),GenerateStableId(),doc.envelope.documentId,
        doc.envelope.pathHint,resolved,error))return fail("cross-project profile identity accepted");
    GameplayAttributeAuthoringSession author;
    if(!author.Open(path.generic_u8string(),metadata.projectId,error))return fail("profile author open");
    auto edited=author.Document();edited.profiles[0].attributes[0].maximum=80.0;
    if(!author.Replace(edited,error)||!author.Dirty()||!author.Undo(error)||author.Dirty()||
        !author.Redo(error)||!author.Save(error)||author.Dirty())return fail("profile history/save identity");
    const auto external=Bytes(path)+"\n; preserved outside edit\n";Put(path,external);
    edited=author.Document();edited.profiles[0].attributes[0].initial=48.0;
    if(!author.Replace(edited,error)||author.Save(error)||Bytes(path)!=external)return fail("profile external-edit guard");
    if(!author.Open(path.generic_u8string(),metadata.projectId,error))return fail("profile explicit reload");
    auto future=Bytes(path);const auto schema=future.find("schema_version = 1",future.find("[attributes]"));
    if(schema==std::string::npos)return fail("profile schema field missing");
    future.replace(schema,std::string("schema_version = 1").size(),"schema_version = 2");Put(path,future);
    if(ReadGameplayAttributeDocument(path.generic_u8string(),metadata.projectId,parsed,error))return fail("future profile schema accepted");
    Put(path,external);
    auto invalid=doc;invalid.profiles[0].attributes[0].id="health.current";
    if(ValidateGameplayAttributeDocument(invalid,metadata.projectId,error))return fail("duplicate native health authority accepted");
    invalid=doc;invalid.attachments.push_back(doc.attachments[0]);
    if(ValidateGameplayAttributeDocument(invalid,metadata.projectId,error))return fail("duplicate attachment accepted");

    const auto descriptor=fs::u8path(metadata.descriptorPath);
    const auto foreign=Bytes(descriptor)+"\r\n; another stream\r\n[parallel_stream]\r\nopaque = keep me\r\n";
    Put(descriptor,foreign);
    if(!projects.SetGameplayAttributes(doc.envelope.documentId,doc.envelope.pathHint))return fail(projects.LastError().c_str());
    const auto attached=Bytes(descriptor);
    if(WithoutProfileKeys(attached)!=WithoutProfileKeys(foreign)||attached.find("opaque = keep me")==std::string::npos)
        return fail("module attachment changed unrelated descriptor bytes");
    ProjectService reopened;ProjectMetadata inspected;
    if(!reopened.InspectProject(metadata.descriptorPath,inspected,error)||
        inspected.attributeDocumentId!=doc.envelope.documentId||inspected.attributeDocument!=doc.envelope.pathHint)
        return fail("project module reference did not reopen");
    const auto foreignAgain=attached+"\nexternal_more = keep this too\n";Put(descriptor,foreignAgain);
    if(!projects.SetGameplayAttributes(doc.envelope.documentId,doc.envelope.pathHint)||Bytes(descriptor)!=foreignAgain)
        return fail("unchanged module attachment rewrote external bytes");
    auto changedReference=foreignAgain;const auto at=changedReference.find(doc.envelope.documentId);
    if(at==std::string::npos)return fail("attached identity not found");
    changedReference.replace(at,doc.envelope.documentId.size(),GenerateStableId());Put(descriptor,changedReference);
    if(projects.SetGameplayAttributes(doc.envelope.documentId,doc.envelope.pathHint)||Bytes(descriptor)!=changedReference)
        return fail("external module reference was overwritten");
    Put(descriptor,foreignAgain);
    ProjectDependencyDocument dependencies;
    if(!MakeProjectDependencyReader()(metadata.descriptorPath,dependencies,error)||
        std::none_of(dependencies.alwaysInclude.begin(),dependencies.alwaysInclude.end(),[&](const auto& candidate){
            return candidate.dependencyClass==DependencyClass::Data&&candidate.declaredPath==doc.envelope.pathHint;}))
        return fail("unbound gameplay module missing from package closure");

    renegade::runtime::RuntimeGameplayAttributeWorld world;
    if(!world.Configure(doc,true,error)||world.Player()!=nullptr||!world.Characters().empty()||
        !world.SyncActors(1,100,{{actorA,101},{actorB,102}},error))return fail("gameplay module lifecycle");
    if(!world.Player()->Spend("energy",5,error)||!world.Tick(1,true,error)||
        std::get<double>(*world.Player()->Find("energy"))!=15||std::get<double>(*world.Project()->Find("energy"))!=50||
        !world.Tick(1,false,error)||std::get<double>(*world.Player()->Find("energy"))!=25||
        std::get<double>(*world.Project()->Find("energy"))!=60||
        std::get<double>(*world.Character(actorA)->Find("energy"))!=30||
        std::get<double>(*world.Character(actorB)->Find("energy"))!=60)
        return fail("independent gameplay values/pause/cost/regeneration");
    auto revised=doc;revised.profiles[0].attributes[0].initial=60.0;revised.profiles[0].attributes[0].maximum=90.0;
    revised.profiles[0].attributes[0].regeneratePerSecond=2;
    if(!world.Configure(revised,false,error)||!world.Tick(1,false,error)||
        std::get<double>(*world.Project()->Find("energy"))!=62||
        std::get<double>(*world.Player()->Find("energy"))!=27||
        std::get<double>(*world.Character(actorA)->Find("energy"))!=30||
        std::get<double>(*world.Character(actorB)->Find("energy"))!=62)
        return fail("profile edit reset live state or ignored per-property override");
    auto wrongType=revised;wrongType.profiles[0].attributes[0].initial=true;
    wrongType.profiles[0].attributes[0].minimum.reset();wrongType.profiles[0].attributes[0].maximum.reset();
    wrongType.profiles[0].attributes[0].regeneratePerSecond=0;
    for(auto& attachment:wrongType.attachments)attachment.overrides.clear();
    if(world.Configure(wrongType,false,error)||std::get<double>(*world.Project()->Find("energy"))!=62)
        return fail("live type change was not rejected atomically");
    if(!world.Character(actorA)->Set("energy",5.0,error)||!world.SyncActors(1,100,{{actorA,201}},error)||
        world.Character(actorB)!=nullptr||std::get<double>(*world.Character(actorA)->Find("energy"))!=30||
        !world.SyncActors(2,100,{{actorA,201},{actorB,102}},error)||
        std::get<double>(*world.Player()->Find("energy"))!=20||
        std::get<double>(*world.Project()->Find("energy"))!=62||
        std::get<double>(*world.Character(actorB)->Find("energy"))!=60)
        return fail("actor generations leaked state or reset project values");
    if(!world.Configure(revised,true,error)||world.Player()!=nullptr||
        std::get<double>(*world.Project()->Find("energy"))!=60)
        return fail("new session retained live values or fabricated Player instance");
    std::cout<<"PASS: persisted gameplay profiles, guarded authoring, scoped inheritance, project package closure and real-lifetime module contracts\n";
    return 0;
}
