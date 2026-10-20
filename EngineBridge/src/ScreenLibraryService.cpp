#include "renegade/bridge/ScreenLibraryService.h"
#include "renegade/bridge/DependencyService.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>

namespace renegade::bridge
{
    bool ResolveProjectScreenLibraryDocument(const std::string& projectRoot,const StableId& projectId,
        const StableId& documentId,const std::string& pathHint,ScreenDocument& document,
        std::string& resolvedPath,std::string& resolvedHint,std::string& error)
    {
        std::string path;ScreenDocument candidate;
        if(!ResolveRuntimeScreenDocumentPath(projectRoot,projectId,documentId,pathHint,path,error) ||
            !ReadScreenDocument(path,projectId,candidate,error))return false;
        std::error_code ec;
        const auto relative=std::filesystem::relative(std::filesystem::u8path(path),std::filesystem::u8path(projectRoot),ec);
        if(ec){error="Resolved Screen cannot be represented within the project.";return false;}
        resolvedPath=std::move(path);resolvedHint=relative.generic_u8string();document=std::move(candidate);
        error.clear();return true;
    }

    ScreenLibraryCreateResult CreateProjectScreenLibraryDocument(
        const std::string& projectRoot,const StableId& projectId,ScreenUsage usage)
    {
        namespace fs=std::filesystem;
        ScreenLibraryCreateResult result;
        if(projectRoot.empty() || !IsValidStableId(projectId) ||
            (usage!=ScreenUsage::Menu && usage!=ScreenUsage::Hud &&
             usage!=ScreenUsage::Overlay && usage!=ScreenUsage::Modal))
        {
            result.error="Creating a UI document requires a valid project and Screen usage.";
            return result;
        }
        std::error_code ec;
        const fs::path root=fs::canonical(fs::u8path(projectRoot),ec);
        if(ec || !fs::is_directory(root))
        {
            result.error="The project root is not accessible.";
            return result;
        }
        const fs::path content=root/"Content";
        if(fs::is_symlink(content,ec) || !fs::is_directory(content,ec))
        {
            result.error="Project Content is unavailable or linked outside the project.";
            return result;
        }
        const fs::path ui=content/"UI";
        if(fs::is_symlink(ui,ec))
        {
            result.error="The project UI folder must not be a symbolic link.";
            return result;
        }
        fs::create_directories(ui,ec);
        if(ec || !fs::is_directory(ui,ec))
        {
            result.error="Cannot create the project UI folder.";
            return result;
        }
        const fs::path actual=fs::canonical(ui,ec);
        if(ec || actual!=ui)
        {
            result.error="The project UI folder does not resolve within the expected location.";
            return result;
        }
        const char* name=usage==ScreenUsage::Hud ? "NewHUD" :
            usage==ScreenUsage::Overlay ? "NewOverlay" :
            usage==ScreenUsage::Modal ? "NewModal" : "NewMenu";
        ScreenDocument document;
        document.usage=usage;
        document.enabledInGame=true;
        const auto provisional=CreateDocumentEnvelope(projectId,RuntimeScreenDocumentType,
            "Content/UI/"+std::string(name)+"-"+GenerateStableId()+".renegade-screen",
            "Renegade UI Library");
        document.envelope=provisional;
        // Menu and Modal documents require an actionable, focusable control.
        // Its symbolic action is authored, never assigned a fabricated Flow route.
        if(usage==ScreenUsage::Menu || usage==ScreenUsage::Modal)
        {
            const bool modal=usage==ScreenUsage::Modal;
            const std::string action=modal ? "close" : "back";
            document.actions.push_back({action});
            ScreenWidget button;
            button.id=GenerateStableId();
            button.kind=ScreenWidgetKind::Button;
            button.name=modal ? "Close" : "Back";
            button.rect={440.0f,330.0f,400.0f,64.0f};
            button.visible=true;
            button.enabled=true;
            button.text=modal ? "CLOSE" : "BACK";
            button.actionId=action;
            button.style=MakeScreenWidgetStyleTemplate(button.kind,button.rect.height);
            document.focusOrder.push_back(button.id);
            document.widgets.push_back(std::move(button));
        }
        const fs::path target=root/fs::u8path(document.envelope.pathHint);
        if(fs::exists(target,ec) || ec)
        {
            result.error="The generated UI destination already exists or is inaccessible.";
            return result;
        }
        std::string error;
        if(!WriteScreenDocument(target.generic_u8string(),document,error))
        {
            result.error="Could not transactionally create UI document: "+error;
            return result;
        }
        ScreenDocument readBack;
        if(!ReadScreenDocument(target.generic_u8string(),projectId,readBack,error) ||
           readBack.envelope.documentId!=document.envelope.documentId ||
           readBack.envelope.pathHint!=document.envelope.pathHint ||
           readBack.usage!=usage)
        {
            result.error="Created UI document did not pass identity and usage verification: "+error;
            return result;
        }
        result.documentId=readBack.envelope.documentId;
        result.pathHint=readBack.envelope.pathHint;
        result.succeeded=true;
        return result;
    }

    ScreenLibraryCatalogue EnumerateProjectScreenLibrary(const std::string& projectRoot,const StableId& projectId)
    {
        namespace fs=std::filesystem;
        ScreenLibraryCatalogue result;
        if(projectRoot.empty() || !IsValidStableId(projectId))
        {result.error="Screen Library requires a valid project identity.";return result;}
        std::error_code ec;
        const auto root=fs::absolute(fs::u8path(projectRoot),ec).lexically_normal();
        if(ec || !fs::is_directory(root,ec))
        {result.error="Screen Library project root is unavailable.";return result;}
        const auto content=root/"Content";
        if(!fs::is_directory(content,ec))
        {result.error="Project Content folder is unavailable.";return result;}
        std::map<StableId,std::vector<std::size_t>> identities;
        fs::recursive_directory_iterator iterator(content,fs::directory_options::skip_permission_denied,ec),end;
        if(ec){result.error="Could not enumerate the project Screen Library.";return result;}
        while(iterator!=end)
        {
            const auto path=iterator->path();
            if(iterator->is_symlink(ec) && iterator->is_directory(ec))iterator.disable_recursion_pending();
            auto extension=path.extension().generic_u8string();
            std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            if(extension==".renegade-screen" && iterator->is_regular_file(ec))
            {
                if(result.screens.size()>=4096)
                {result.error="Screen Library exceeds its 4096 document limit.";return result;}
                ScreenLibraryEntry entry;entry.name=path.stem().generic_u8string();
                entry.pathHint=path.lexically_relative(root).generic_u8string();
                const auto resolved=ResolveDependencyPath(projectRoot,entry.pathHint);
                ScreenDocument document;
                if(!resolved.accepted || !resolved.exists)
                    entry.diagnostic="Screen path is unavailable or escapes project Content.";
                else if(ReadScreenDocument(resolved.absolutePath,projectId,document,entry.diagnostic))
                {
                    entry.available=true;entry.documentId=document.envelope.documentId;
                    entry.usage=document.usage;entry.enabledInGame=document.enabledInGame;
                    identities[entry.documentId].push_back(result.screens.size());
                }
                else
                {
                    DocumentEnvelope envelope;std::string ignored;
                    if(ReadDocumentEnvelope(resolved.absolutePath,envelope,ignored))
                    {
                        entry.documentId=envelope.documentId;
                        if(envelope.projectId==projectId && envelope.documentType==RuntimeScreenDocumentType)
                            identities[entry.documentId].push_back(result.screens.size());
                    }
                }
                if(!entry.diagnostic.empty())result.diagnostics.push_back(entry.pathHint+": "+entry.diagnostic);
                result.screens.push_back(std::move(entry));
            }
            ec.clear();iterator.increment(ec);
            if(ec){result.diagnostics.push_back("Some project folders could not be enumerated: "+ec.message());ec.clear();}
        }
        for(const auto& identity:identities)
        {
            if(identity.second.size()<2)continue;
            for(const auto index:identity.second)
            {
                auto& entry=result.screens[index];entry.available=false;
                entry.diagnostic="Duplicate Screen document identity: "+identity.first;
                result.diagnostics.push_back(entry.pathHint+": "+entry.diagnostic);
            }
        }
        std::sort(result.screens.begin(),result.screens.end(),[](const auto& a,const auto& b)
        {
            if(a.usage!=b.usage)return a.usage<b.usage;
            if(a.name!=b.name)return a.name<b.name;
            return a.pathHint<b.pathHint;
        });
        result.succeeded=true;return result;
    }
}
