#pragma once
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <cstdint>
#include <string>
namespace renegade::bridge
{
    inline constexpr const char* ScreenPerformanceBudgetPath="Content/Data/UiPerformance.renegade-uibudget";
    struct ScreenPerformanceBudget
    {
        std::uint64_t bindingCpuUs=2000;
        std::uint32_t widgetWarning=256;
        std::uint32_t animationWarning=128;
    };
    [[nodiscard]] bool ValidateScreenPerformanceBudget(const ScreenPerformanceBudget& value,std::string& error);
    // Missing optional settings use documented advisory defaults. Invalid or
    // foreign project settings fail atomically; these never cap gameplay work.
    [[nodiscard]] bool ReadScreenPerformanceBudget(const std::string& root,const std::string& projectId,
        ScreenPerformanceBudget& value,std::string& error);
    [[nodiscard]] bool WriteScreenPerformanceBudget(const std::string& root,const std::string& projectId,
        const ScreenPerformanceBudget& value,std::string& error,ProjectDocumentTransactionHook hook={});
}
