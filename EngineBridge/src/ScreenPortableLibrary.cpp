#include "renegade/bridge/ScreenLibraryService.h"
#include "renegade/bridge/ScreenThemeResolver.h"
#include "renegade/bridge/DependencyService.h"
#include "renegade/bridge/PackageIntegrityService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "renegade/bridge/ResourceAssetService.h"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <algorithm>

namespace renegade::bridge
{
namespace
{
    namespace fs=std::filesystem;
    using Json=nlohmann::json;
    constexpr std::uint64_t MaxUiPackageBytes=512ull*1024*1024;
    struct UiStageCleanup
    {
        fs::path& path;
        ~UiStageCleanup(){if(!path.empty()){std::error_code ec;fs::remove_all(path,ec);}}
    };
    struct UiPackageFile{std::string path;DependencyClass type;WindowsGamePackageFileDigest digest;};
    struct UiPackage{StableId projectId,entryId;std::string entryPath;std::vector<UiPackageFile> files;};
    bool Bytes(const fs::path& p,std::vector<std::uint8_t>& bytes,std::string& error)
    {
        std::error_code ec;const auto size=fs::file_size(p,ec);
        if(ec||size>MaxUiPackageBytes){error="UI package file unavailable or over budget.";return false;}
        std::ifstream in(p,std::ios::binary);if(!in){error="Cannot read UI package file.";return false;}
        bytes.assign(std::istreambuf_iterator<char>(in),{});return !in.bad();
    }
    bool Put(const fs::path& p,const std::vector<std::uint8_t>& bytes,std::string& error)
    {
        std::error_code ec;fs::create_directories(p.parent_path(),ec);
        if(ec){error="Cannot prepare UI package staging folder.";return false;}
        std::ofstream out(p,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        if(!out){error="Cannot write UI package staging file.";return false;}return true;
    }
    bool PackagePath(const std::string& root,const std::string& relative,std::string& path,std::string& error)
    {
        if(relative.rfind("Content/",0)!=0){error="UI package paths must be contained in Content.";return false;}
        const auto p=ResolveDependencyPath(root,relative);
        if(!p.accepted||!p.exists){error="Unsafe or missing UI package dependency: "+relative;return false;}
        path=p.absolutePath;return true;
    }
    bool UiDependencyErrors(const DependencyGraph& graph,std::string& error)
    {
        for(const auto& diagnostic:graph.diagnostics)
            if(diagnostic.code!=DependencyDiagnosticCode::Duplicate)
            {error=diagnostic.path+": "+diagnostic.message;return true;}
        return false;
    }
    bool LoadUiPackage(const fs::path& root,UiPackage& result,std::string& error)
    {
        try
        {
            const auto manifest=root/"renegade-ui-package.json";
            if(fs::is_symlink(root)||fs::is_symlink(manifest)||fs::file_size(manifest)>1024*1024)
            {error="Unsafe or oversized UI package manifest.";return false;}
            std::ifstream in(manifest);Json json;in>>json;
            if(json.at("schema")!="renegade-ui-package"||json.at("version")!=1)
            {error="Unsupported UI package version.";return false;}
            UiPackage next;next.projectId=json.at("project_id");next.entryId=json.at("entry_id");next.entryPath=json.at("entry_path");
            if(!IsValidStableId(next.projectId)||!IsValidStableId(next.entryId)||!json.at("files").is_array()||
                json.at("files").empty()||json.at("files").size()>2048)
            {error="Invalid UI package identities or file budget.";return false;}
            DependencyPathRegistry paths;std::uint64_t total=0;bool entry=false;
            for(const auto& f:json.at("files"))
            {
                UiPackageFile file;file.path=f.at("path");const std::string type=f.at("class");
                if(!TryParseDependencyClassName(type,file.type)||
                    (file.type!=DependencyClass::RuntimeScreenDocument&&file.type!=DependencyClass::Texture&&
                     file.type!=DependencyClass::Font))
                {error="UI package contains an unsupported dependency class.";return false;}
                const auto registration=paths.Register("ui-package",file.path);
                if(!registration.inserted||!registration.diagnostics.empty())
                {error="Duplicate or case-colliding UI package path.";return false;}
                std::string absolute;
                if(!PackagePath(root.generic_u8string(),file.path,absolute,error)||
                    !DigestWindowsGamePackageFile(absolute,file.digest,error))return false;
                const std::uint64_t expectedSize=f.at("bytes");const std::string expectedHash=f.at("sha256");
                if(expectedSize!=file.digest.byteCount||expectedHash!=file.digest.sha256||
                    file.digest.byteCount>MaxUiPackageBytes-total)
                {error="UI package hash/size validation failed.";return false;}
                total+=file.digest.byteCount;
                if(file.path==next.entryPath&&file.type==DependencyClass::RuntimeScreenDocument)entry=true;
                next.files.push_back(std::move(file));
            }
            if(!entry){error="UI package entry is not in its declared closure.";return false;}
            // No symlink or undeclared payload can ride along with a portable package.
            std::set<std::string> declared{"renegade-ui-package.json"};
            for(const auto& file:next.files)declared.insert(file.path);
            for(const auto& p:fs::recursive_directory_iterator(root))
            {
                if(p.is_symlink()){error="UI packages cannot contain symlinks.";return false;}
                if(p.is_regular_file()&&!declared.count(fs::relative(p.path(),root).generic_u8string()))
                {error="UI package contains an undeclared file.";return false;}
            }
            ScreenDocument doc,effective;
            if(!ReadScreenDocument((root/fs::u8path(next.entryPath)).generic_u8string(),next.projectId,doc,error)||
                doc.envelope.documentId!=next.entryId||!ResolveScreenTheme(root.generic_u8string(),doc,effective,error))
            {if(error.empty())error="UI package entry identity mismatch.";return false;}
            DependencyCollector closure(root.generic_u8string());
            RuntimeScreenDependencyProvider provider(MakeRuntimeScreenDependencyReader(next.projectId));
            if(!closure.RegisterProvider(provider,error)||
                !closure.AddRoot({next.entryPath,DependencyClass::RuntimeScreenDocument,DependencyRequirement::Required,"ui-package-entry"},error)||
                !closure.DiscoverTransitiveDependencies(error)||UiDependencyErrors(closure.Graph(),error))return false;
            if(closure.Graph().nodes.size()!=next.files.size())
            {error="UI package manifest does not match its transitive dependency closure.";return false;}
            for(const auto& node:closure.Graph().nodes)
            {
                const auto file=std::find_if(next.files.begin(),next.files.end(),[&](const auto& f){
                    return f.path==node.projectRelativePath&&f.type==node.dependencyClass;});
                if(file==next.files.end()){error="UI package dependency type/closure mismatch.";return false;}
            }
            result=std::move(next);return true;
        }
        catch(const std::exception& e){error=std::string("Invalid UI package: ")+e.what();return false;}
    }
}
bool PublishPortableScreenPackage(const std::string& projectRoot,const StableId& projectId,
    const StableId& entryId,const std::string& entryHint,const std::string& destination, std::string& error)
{
    fs::path stage;UiStageCleanup cleanup{stage};
    try
    {
        const auto target=fs::absolute(fs::u8path(destination)).lexically_normal();
        if(fs::exists(target)){error="Choose a new package folder; existing packages are never overwritten.";return false;}
        if(!fs::is_directory(target.parent_path())){error="Package parent folder is unavailable.";return false;}
        ScreenDocument doc,effective;std::string path,hint;
        if(!ResolveProjectScreenLibraryDocument(projectRoot,projectId,entryId,entryHint,doc,path,hint,error)||
            !ResolveScreenTheme(projectRoot,doc,effective,error))return false;
        DependencyCollector collector(projectRoot);
        RuntimeScreenDependencyProvider provider(MakeRuntimeScreenDependencyReader(projectId));
        if(!collector.RegisterProvider(provider,error)||
            !collector.AddRoot({hint,DependencyClass::RuntimeScreenDocument,DependencyRequirement::Required,"portable-ui-entry"},error)||
            !collector.DiscoverTransitiveDependencies(error))return false;
        const auto& graph=collector.Graph();
        if(UiDependencyErrors(graph,error))return false;
        if(graph.nodes.size()>2048){error="UI package exceeds its file budget.";return false;}
        stage=target.parent_path()/fs::u8path(".renegade-ui-stage-"+GenerateStableId());
        if(!fs::create_directory(stage)){stage.clear();error="UI package staging conflict.";return false;}
        Json manifest={{"schema","renegade-ui-package"},{"version",1},{"project_id",projectId},
            {"entry_id",entryId},{"entry_path",hint},{"files",Json::array()}};
        std::uint64_t total=0;
        for(const auto& node:graph.nodes)
        {
            if(node.dependencyClass!=DependencyClass::RuntimeScreenDocument&&node.dependencyClass!=DependencyClass::Texture&&
                node.dependencyClass!=DependencyClass::Font)
            {error="This UI package has an unsupported dependency class.";fs::remove_all(stage);return false;}
            std::string source;WindowsGamePackageFileDigest digest;
            if(!PackagePath(projectRoot,node.projectRelativePath,source,error)||
                !DigestWindowsGamePackageFile(source,digest,error)){fs::remove_all(stage);return false;}
            if(digest.byteCount>MaxUiPackageBytes-total)
            {error="UI package exceeds512MiB.";fs::remove_all(stage);return false;}
            total+=digest.byteCount;
            const auto out=stage/fs::u8path(node.projectRelativePath);fs::create_directories(out.parent_path());
            fs::copy_file(fs::u8path(source),out);
            manifest["files"].push_back({{"path",node.projectRelativePath},{"class",DependencyClassName(node.dependencyClass)},
                {"bytes",digest.byteCount},{"sha256",digest.sha256}});
        }
        {std::ofstream out(stage/"renegade-ui-package.json");out<<manifest.dump(2);if(!out)throw std::runtime_error("Cannot write manifest");}
        UiPackage verified;
        if(!LoadUiPackage(stage,verified,error)){fs::remove_all(stage);return false;}
        fs::rename(stage,target);error.clear();return true;
    }
    catch(const std::exception& e)
    {if(!stage.empty()){std::error_code ec;fs::remove_all(stage,ec);}error=std::string("Cannot publish UI package: ")+e.what();return false;}
}
bool AdoptPortableScreenPackage(const std::string& projectRoot,const StableId& projectId,
    const std::string& packageRoot,ScreenPortableAdoptionResult& result,std::string& error,
    ProjectDocumentTransactionHook operationHook)
{
    fs::path stage;UiStageCleanup cleanup{stage};
    try
    {
        if(!IsValidStableId(projectId)){error="A valid destination project is required.";return false;}
        UiPackage package;if(!LoadUiPackage(fs::u8path(packageRoot),package,error))return false;
        const auto token=GenerateStableId();const std::string prefix="Content/UI/Imported/"+token+"/";
        const auto mappedPath=[&](const std::string& p){
            auto path=fs::u8path(prefix+p.substr(8));
            if(path.extension()==".renegade-screen")
                path=path.parent_path()/fs::u8path(path.stem().generic_u8string()+"-"+token.substr(0,8)+".renegade-screen");
            return path.generic_u8string();
        };
        const auto safe=ResolveDependencyPath(projectRoot,prefix+"receipt.json");
        if(!safe.accepted){error="Imported UI destination is unsafe.";return false;}
        stage=fs::temp_directory_path()/fs::u8path("RenegadeUiAdopt-"+token);
        if(!fs::create_directory(stage)){stage.clear();error="UI adoption staging conflict.";return false;}
        const auto inputRoot=stage/"original";
        for(const auto& file:package.files)
        {
            const auto source=fs::u8path(packageRoot)/fs::u8path(file.path),copy=inputRoot/fs::u8path(file.path);
            fs::create_directories(copy.parent_path());fs::copy_file(source,copy);
            WindowsGamePackageFileDigest actual;
            if(!DigestWindowsGamePackageFile(copy.generic_u8string(),actual,error)||
                actual.sha256!=file.digest.sha256||actual.byteCount!=file.digest.byteCount)
            {error="UI package changed during adoption.";fs::remove_all(stage);return false;}
        }
        std::map<std::string,ScreenDocument> docs;std::map<StableId,StableId> docIds;
        std::map<StableId,std::map<StableId,StableId>> widgetIds;
        for(const auto& file:package.files)if(file.type==DependencyClass::RuntimeScreenDocument)
        {
            ScreenDocument doc;if(!ReadScreenDocument((inputRoot/fs::u8path(file.path)).generic_u8string(),
                package.projectId,doc,error))return false;
            if(!docIds.emplace(doc.envelope.documentId,GenerateStableId()).second)
            {error="UI package contains duplicate Screen identities.";return false;}
            auto& ids=widgetIds[doc.envelope.documentId];for(const auto& w:doc.widgets)ids[w.id]=GenerateStableId();
            docs.emplace(file.path,std::move(doc));
        }
        ScreenPortableAdoptionResult next;next.documentId=docIds.at(package.entryId);next.pathHint=mappedPath(package.entryPath);
        next.receiptPathHint="Intermediate/Editor/UiAdoptions/"+token+".json";
        Json receipt={{"schema","renegade-ui-adoption"},{"version",1},{"project_id",projectId},
            {"source_project_id",package.projectId},{"source_entry_id",package.entryId},
            {"entry_id",next.documentId},{"entry_path",next.pathHint},
            {"gameplay_slots",Json::array()},{"files",Json::array()}};

        const auto reference=[&](StableId& id,std::string& hint)->bool{
            const auto found=docIds.find(id);if(found==docIds.end()){error="UI reference escapes package closure.";return false;}
            for(const auto& [path,doc]:docs)if(doc.envelope.documentId==id){hint=mappedPath(path);id=found->second;return true;}
            return false;
        };
        const auto resource=[&](std::string& path)->bool{
            if(path.empty()||path.rfind("builtin:",0)==0)return true;
            const auto found=std::find_if(package.files.begin(),package.files.end(),[&](const auto& f){return f.path==path;});
            if(found==package.files.end()){error="UI resource escapes package closure.";return false;}path=mappedPath(path);return true;
        };
        std::map<StableId,StableId> assetIds;
        const auto assetId=[&](const StableId& old){auto [it,inserted]=assetIds.emplace(old,StableId{});
            if(inserted)it->second=GenerateStableId();return it->second;};
        for(const auto& file:package.files)
        {
            const auto destination=stage/fs::u8path(mappedPath(file.path));
            if(file.type==DependencyClass::RuntimeScreenDocument)
            {
                auto doc=docs.at(file.path);const auto oldDocId=doc.envelope.documentId;
                auto& ids=widgetIds.at(oldDocId);
                doc.envelope.projectId=projectId;doc.envelope.documentId=docIds.at(oldDocId);doc.envelope.pathHint=mappedPath(file.path);
                if(doc.theme&&!reference(doc.theme->documentId,doc.theme->pathHint)){fs::remove_all(stage);return false;}
                for(auto& focus:doc.focusOrder)focus=ids.at(focus);
                for(auto& w:doc.widgets)
                {
                    const auto oldId=w.id;w.id=ids.at(oldId);if(!w.parentId.empty())w.parentId=ids.at(w.parentId);
                    for(auto& action:w.interactions){if(!action.targetId.empty())action.targetId=ids.at(action.targetId); if(action.condition)action.condition->widgetId=ids.at(action.condition->widgetId);}
                    if(w.component)
                    {
                        const auto sourceDoc=w.component->documentId;
                        const auto source=widgetIds.find(sourceDoc);
                        if(source==widgetIds.end()||!source->second.count(w.component->widgetId)||
                            !reference(w.component->documentId,w.component->pathHint))
                        {error="Linked UI component is outside package closure.";fs::remove_all(stage);return false;}
                        w.component->widgetId=source->second.at(w.component->widgetId);
                        if(!w.component->treeRootId.empty())w.component->treeRootId=ids.at(w.component->treeRootId);
                    }
                    for(auto& binding:w.bindings)
                    {
                        // Cross-project adoption never guesses gameplay ownership, even
                        // for a same-named project attribute or possessed-player adapter.
                        const auto unassign=[&](ScreenBindingSource& source){
                            next.unresolvedBindings.push_back(w.name+": "+source.attributeId);
                            auto id=GenerateStableId();std::replace(id.begin(),id.end(),'-','_');
                            const auto slot="library.unassigned."+source.attributeId.substr(0,32)+"."+id.substr(0,8);
                            receipt["gameplay_slots"].push_back({{"document_id",doc.envelope.documentId},
                                {"widget_id",w.id},{"original_scope",int(source.scope)},{"original_target_id",source.targetId},
                                {"original_attribute",source.attributeId},{"unassigned_attribute",slot}});
                            source.attributeId=slot;
                            if(source.scope==ScreenBindingScope::Character)source.targetId=GenerateStableId();
                        };
                        unassign(binding.source);
                        if(binding.operation==ScreenBindingOperation::Ratio)unassign(binding.denominator);
                    }
                    if(!resource(w.resourcePath)||!resource(w.style.text.fontResource))
                    {fs::remove_all(stage);return false;}
                    for(auto* visual:{&w.style.normal,&w.style.hover,&w.style.pressed,&w.style.focused,&w.style.disabled})
                        if(!resource(visual->imageResourcePath)){fs::remove_all(stage);return false;}
                }
                fs::create_directories(destination.parent_path());
                if(!WriteScreenDocument(destination.generic_u8string(),doc,error)){fs::remove_all(stage);return false;}
            }
            else
            {
                std::vector<std::uint8_t> bytes;const auto source=inputRoot/fs::u8path(file.path);
                if(source.extension()==".rasset")
                {
                    ResourceAssetDocument doc;
                    if(!ReadResourceAssetDocument(source.generic_u8string(),doc,error)){fs::remove_all(stage);return false;}
                    doc.manifest.projectId=projectId;doc.manifest.assetId=assetId(doc.manifest.assetId);
                    if(!doc.manifest.sourceAssetId.empty())doc.manifest.sourceAssetId=assetId(doc.manifest.sourceAssetId);
                    if(!SerializeResourceAssetDocument(doc,bytes,error)){fs::remove_all(stage);return false;}
                }
                else if(!Bytes(source,bytes,error)){fs::remove_all(stage);return false;}
                if(!Put(destination,bytes,error)){fs::remove_all(stage);return false;}
            }
        }
        std::vector<ProjectDocumentWrite> writes;
        for(const auto& file:package.files)
        {
            const auto hint=mappedPath(file.path);const auto staged=stage/fs::u8path(hint);
            const auto output=ResolveDependencyPath(projectRoot,hint);
            if(!output.accepted||output.exists){error="UI adoption destination conflict.";fs::remove_all(stage);return false;}
            if(file.type==DependencyClass::RuntimeScreenDocument)
            {
                ScreenDocument doc,effective;
                if(!ReadScreenDocument(staged.generic_u8string(),projectId,doc,error)||
                    !ResolveScreenTheme(stage.generic_u8string(),doc,effective,error)){fs::remove_all(stage);return false;}
            }
            std::vector<std::uint8_t> bytes;if(!Bytes(staged,bytes,error)){fs::remove_all(stage);return false;}
            ProjectDocumentWrite write;write.destinationPath=output.absolutePath;write.content=std::move(bytes);
            if(file.type==DependencyClass::RuntimeScreenDocument)write.validator=[projectId](const std::string& path,std::string& e){
                ScreenDocument doc;return ReadScreenDocument(path,projectId,doc,e);};
            WindowsGamePackageFileDigest adoptedDigest;
            if(!DigestWindowsGamePackageFile(staged.generic_u8string(),adoptedDigest,error))return false;
            receipt["files"].push_back({{"path",hint},{"source_path",file.path},
                {"source_sha256",file.digest.sha256},{"adopted_sha256",adoptedDigest.sha256}});
            writes.push_back(std::move(write));
        }
        const auto receiptPath=ResolveDependencyPath(projectRoot,next.receiptPathHint);
        if(!receiptPath.accepted||receiptPath.exists){error="UI adoption receipt conflict.";return false;}
        const auto receiptBytes=receipt.dump(2);
        ProjectDocumentWrite receiptWrite;receiptWrite.destinationPath=receiptPath.absolutePath;
        receiptWrite.content.assign(receiptBytes.begin(),receiptBytes.end());writes.push_back(std::move(receiptWrite));
        ProjectDocumentTransactionOptions options;options.allowedRoot=projectRoot;options.journalDirectory=projectRoot;options.operationHook=std::move(operationHook);
        auto committed=ProjectDocumentTransaction{}.Execute(std::move(writes),options);
        fs::remove_all(stage);
        if(!committed.success){error=committed.message;return false;}
        result=std::move(next);error.clear();return true;
    }
    catch(const std::exception& e)
    {if(!stage.empty()){std::error_code ec;fs::remove_all(stage,ec);}error=std::string("UI adoption rejected: ")+e.what();return false;}
}
}
