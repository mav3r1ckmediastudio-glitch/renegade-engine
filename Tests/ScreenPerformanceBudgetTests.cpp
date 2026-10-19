#include "renegade/bridge/ScreenPerformanceBudget.h"
#include "renegade/bridge/ProjectService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/DependencyService.h"
#include "../Runtime/src/RuntimeUiTimingWindow.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
int main()
{
    using namespace renegade;namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/fs::u8path("rg-budget-"+bridge::GenerateStableId().substr(0,8));
    fs::create_directories(root);struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}}cleanup{root};
    int failures=0;std::string error;auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<text<<": "<<error<<'\n';}};
    bridge::ProjectService projects;if(!projects.CreateStoryFlowProject(root.generic_u8string(),"Budgets",false))return 1;
    const auto project=projects.CurrentProject();const auto path=fs::u8path(project.rootPath)/bridge::ScreenPerformanceBudgetPath;
    bridge::ScreenPerformanceBudget budget;budget.bindingCpuUs=1;
    check(bridge::ReadScreenPerformanceBudget(project.rootPath,project.projectId,budget,error)&&budget.bindingCpuUs==2000,"Missing settings use advisory defaults");
    budget.bindingCpuUs=500;budget.widgetWarning=64;budget.animationWarning=32;
    check(bridge::WriteScreenPerformanceBudget(project.rootPath,project.projectId,budget,error),"Write project budget");
    bridge::ScreenPerformanceBudget loaded;
    check(bridge::ReadScreenPerformanceBudget(project.rootPath,project.projectId,loaded,error)&&loaded.bindingCpuUs==500&&loaded.widgetWarning==64&&loaded.animationWarning==32,"Budget cold round trip");
    auto bytes=[&](){std::ifstream s(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(s),{});};
    const auto original=bytes();auto invalid=budget;invalid.bindingCpuUs=0;
    check(!bridge::WriteScreenPerformanceBudget(project.rootPath,project.projectId,invalid,error)&&bytes()==original,"Invalid edit preserves saved settings");
    loaded.bindingCpuUs=77;
    check(!bridge::ReadScreenPerformanceBudget(project.rootPath,bridge::GenerateStableId(),loaded,error)&&loaded.bindingCpuUs==77,"Foreign settings rejected atomically");
    auto changed=budget;changed.bindingCpuUs=10000;
    check(!bridge::WriteScreenPerformanceBudget(project.rootPath,project.projectId,changed,error,
        [](auto stage,std::size_t,const std::string&,std::string& e){if(stage==bridge::ProjectDocumentTransactionStage::AfterReplace)
        {e="Injected budget failure";return bridge::ProjectDocumentTransactionHookAction::Fail;}return bridge::ProjectDocumentTransactionHookAction::Continue;})&&bytes()==original,"Failed commit restores original settings");
    bridge::ProjectDependencyDocument dependencies;
    check(bridge::MakeProjectDependencyReader()(project.descriptorPath,dependencies,error),"Read dependency closure");
    bool included=false;for(const auto& candidate:dependencies.alwaysInclude)
        included|=candidate.declaredPath==bridge::ScreenPerformanceBudgetPath&&candidate.dependencyClass==bridge::DependencyClass::Data;
    check(included,"Saved project budgets are an explicit packaged Data dependency");
    {std::ofstream s(path,std::ios::app);s<<"unexpected=1\n";}
    check(!bridge::ReadScreenPerformanceBudget(project.rootPath,project.projectId,loaded,error),"Unknown trailing settings rejected");
    {std::ofstream s(path);s<<original;}
    runtime::RuntimeUiTimingWindow meter;for(unsigned i=1;i<=120;++i)meter.Observe(i*10);
    check(meter.PercentileUs(50)==600&&meter.PercentileUs(95)==1140&&meter.PercentileUs(99)==1190,"Nearest-rank CPU percentiles use real rolling samples");
    check(meter.SetBudgetUs(500)&&meter.OverBudgetInWindow()==70,"Changing budget recalculates the current window");
    check(!meter.SetBudgetUs(0)&&meter.BudgetUs()==500,"Invalid budget cannot corrupt timing window");
    meter.Observe(10);check(meter.OverBudgetInWindow()==70,"Rolling replacement retains accurate over-budget count");
    meter.Reset();check(meter.BudgetUs()==500&&meter.OverBudgetInWindow()==0&&meter.PercentileUs(95)==0,"Scene reset clears samples and retains project policy");
    std::cout<<"Screen performance budget tests: "<<(failures?"FAIL":"PASS")<<'\n';return failures?1:0;
}
