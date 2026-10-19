#include "renegade/bridge/ScreenPerformanceBudget.h"
#include "renegade/bridge/IdentityService.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <locale>
#include <algorithm>

namespace renegade::bridge
{
namespace
{
    bool Parse(const std::string& text,const std::string& projectId,ScreenPerformanceBudget& value,std::string& error)
    {
        if(text.size()>1024){error="UI performance settings exceed their size limit.";return false;}
        std::istringstream stream(text);stream.imbue(std::locale::classic());
        auto line=[&](std::string& s){if(!std::getline(stream,s)||s.size()>128)return false;
            if(!s.empty()&&s.back()=='\r')s.pop_back();return true;};
        std::string header,identity;ScreenPerformanceBudget next;
        auto number=[&](const char* key,auto& out)
        {
            std::string s;if(!line(s)||s.rfind(key,0)!=0)return false;
            auto body=s.substr(std::char_traits<char>::length(key));
            if(body.empty()||!std::all_of(body.begin(),body.end(),[](unsigned char ch){return ch>='0'&&ch<='9';}))return false;
            std::istringstream parser(body);parser.imbue(std::locale::classic());parser>>std::noskipws>>out;
            return !parser.fail()&&parser.peek()==std::char_traits<char>::eof();
        };
        if(!line(header)||header!="renegade_ui_budget=1"||!line(identity)||identity!="project_id="+projectId||
            !number("binding_cpu_us=",next.bindingCpuUs)||!number("widget_warning=",next.widgetWarning)||
            !number("animation_warning=",next.animationWarning)||stream.peek()!=std::char_traits<char>::eof()||
            !ValidateScreenPerformanceBudget(next,error))
        {if(error.empty())error="Invalid, foreign or unsupported UI performance settings.";return false;}
        value=next;error.clear();return true;
    }
    bool Resolve(const std::string& root,const std::string& projectId,std::filesystem::path& path,std::string& error)
    {
        namespace fs=std::filesystem;
        if(!IsValidStableId(projectId)||root.empty())
        {error="UI budgets require an active project identity.";return false;}
        std::error_code ec;const auto canonical=fs::canonical(fs::u8path(root),ec);
        if(ec||!fs::is_directory(canonical)){error="UI budget project folder is unavailable.";return false;}
        path=fs::weakly_canonical(canonical/fs::u8path(ScreenPerformanceBudgetPath),ec);
        const auto relative=path.lexically_relative(canonical);
        if(ec||relative.empty()||relative.is_absolute()||
            std::any_of(relative.begin(),relative.end(),[](const auto& p){return p=="..";}))
        {error="UI budget path escapes the project folder.";return false;}
        return true;
    }
}
bool ValidateScreenPerformanceBudget(const ScreenPerformanceBudget& v,std::string& error)
{
    if(v.bindingCpuUs<1||v.bindingCpuUs>1'000'000||v.widgetWarning<1||v.widgetWarning>256||
        v.animationWarning<1||v.animationWarning>4096)
    {error="UI budgets require CPU1..1000000us, widgets1..256 and animation tracks1..4096.";return false;}
    error.clear();return true;
}
bool ReadScreenPerformanceBudget(const std::string& root,const std::string& projectId,
    ScreenPerformanceBudget& value,std::string& error)
{
    std::filesystem::path path;if(!Resolve(root,projectId,path,error))return false;
    std::error_code ec;if(!std::filesystem::exists(path,ec))
    {if(ec){error="Could not inspect UI performance settings.";return false;}value={};error.clear();return true;}
    const auto size=std::filesystem::file_size(path,ec);
    if(ec||size>1024){error="Invalid UI performance settings size.";return false;}
    std::ifstream input(path,std::ios::binary);if(!input){error="Could not read UI performance settings.";return false;}
    const std::string text{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    return Parse(text,projectId,value,error);
}
bool WriteScreenPerformanceBudget(const std::string& root,const std::string& projectId,
    const ScreenPerformanceBudget& value,std::string& error,ProjectDocumentTransactionHook hook)
{
    if(!ValidateScreenPerformanceBudget(value,error))return false;
    std::filesystem::path path;if(!Resolve(root,projectId,path,error))return false;
    const auto text="renegade_ui_budget=1\nproject_id="+projectId+"\nbinding_cpu_us="+std::to_string(value.bindingCpuUs)+
        "\nwidget_warning="+std::to_string(value.widgetWarning)+"\nanimation_warning="+std::to_string(value.animationWarning)+"\n";
    ProjectDocumentWrite write;write.destinationPath=path.generic_u8string();write.content.assign(text.begin(),text.end());
    write.validator=[projectId](const std::string& candidate,std::string& e)
    {std::ifstream in(std::filesystem::u8path(candidate),std::ios::binary);
        if(!in){e="Could not verify staged UI budgets.";return false;}
        const std::string bytes{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
        ScreenPerformanceBudget v;return Parse(bytes,projectId,v,e);};
    ProjectDocumentTransactionOptions options;options.transactionId=GenerateStableId();options.allowedRoot=root;
    options.journalDirectory=(std::filesystem::u8path(root)/"Intermediate/Transactions").generic_u8string();options.operationHook=std::move(hook);
    const auto result=ProjectDocumentTransaction{}.Execute({std::move(write)},std::move(options));
    if(!result.success){error=result.message;return false;}error.clear();return true;
}
}
