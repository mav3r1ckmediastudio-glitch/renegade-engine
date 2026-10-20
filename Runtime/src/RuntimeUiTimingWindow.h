#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace renegade::runtime
{
    // A fixed-memory, allocation-free advisory meter for Runtime UI binding
    // evaluation CPU time. It is not a GPU, draw-call or full-UI budget.
    class RuntimeUiTimingWindow final
    {
    public:
        static constexpr std::size_t Capacity=120;
        static constexpr std::uint64_t AdvisoryBudgetUs=2000; // provisional, not owner-approved
        static constexpr std::uint64_t MaxAcceptedUs=60'000'000;

        void Observe(std::uint64_t microseconds) noexcept
        {
            if(microseconds>MaxAcceptedUs)return; // corrupt/outlier sample
            if(count_==Capacity)
            {
                sum_-=samples_[index_];
                if(samples_[index_]>budgetUs_)--overBudget_;
            }
            else ++count_;
            samples_[index_]=microseconds;
            sum_+=microseconds;
            if(microseconds>budgetUs_)++overBudget_;
            index_=(index_+1)%Capacity;
            latest_=microseconds;
            peak_=std::max(peak_,microseconds);
            ++lifetimeSamples_;
        }
        bool SetBudgetUs(std::uint64_t value) noexcept
        {
            if(value<1||value>1'000'000)return false;
            budgetUs_=value;overBudget_=OverThresholdInWindow(value);return true;
        }
        [[nodiscard]] std::uint64_t BudgetUs() const noexcept{return budgetUs_;}
        [[nodiscard]] std::uint64_t OverThresholdInWindow(std::uint64_t threshold) const noexcept
        {std::uint64_t count=0;for(std::size_t i=0;i<count_;++i)if(samples_[i]>threshold)++count;return count;}
        [[nodiscard]] std::uint64_t PercentileUs(unsigned percent) const noexcept
        {
            if(!count_||percent<1||percent>100)return 0;
            auto sorted=samples_;std::sort(sorted.begin(),sorted.begin()+count_);
            return sorted[(percent*count_+99)/100-1];
        }
        void Reset() noexcept
        {
            samples_.fill(0);
            index_=count_=overBudget_=0;
            sum_=latest_=peak_=lifetimeSamples_=0;
        }
        [[nodiscard]] std::uint64_t LatestUs() const noexcept{return latest_;}
        [[nodiscard]] std::uint64_t WindowAverageUs() const noexcept
        {return count_?sum_/count_:0;}
        [[nodiscard]] std::uint64_t PeakUs() const noexcept{return peak_;}
        [[nodiscard]] std::uint64_t WindowCount() const noexcept{return count_;}
        [[nodiscard]] std::uint64_t OverBudgetInWindow() const noexcept{return overBudget_;}
        [[nodiscard]] std::uint64_t LifetimeSamples() const noexcept{return lifetimeSamples_;}
    private:
        std::array<std::uint64_t,Capacity> samples_{};
        std::size_t index_=0,count_=0,overBudget_=0;
        std::uint64_t sum_=0,latest_=0,peak_=0,lifetimeSamples_=0,budgetUs_=AdvisoryBudgetUs;
    };
}
