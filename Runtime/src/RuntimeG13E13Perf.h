#pragma once
// G13E13: strictly opt-in technical runtime loop profiler for disposable
// local validation. It is not an FPS overlay and cannot measure GPU time.
// No file is written and no DXGI adapter is opened unless the environment
// variable RENEGADE_G13E13_PERF_CSV is explicitly supplied.
#include <Windows.h>
#include <dxgi1_4.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <string>
#include <vector>
#pragma comment(lib, "dxgi.lib")

namespace renegade::runtime::detail
{
    class G13E13PerfProbe
    {
    public:
        explicit G13E13PerfProbe(const char* output) :
            csvPath_(output && *output ? output : "")
        {
            if (csvPath_.empty()) return;
            if (const char* count=std::getenv("RENEGADE_G13E13_FRAMES"))
            {
                const int parsed=std::atoi(count);
                if (parsed >= 180 && parsed <= 3600) sampleTarget_=parsed;
            }
            IDXGIFactory1* factory=nullptr;
            if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                reinterpret_cast<void**>(&factory))))
            {
                for(UINT i=0;;++i)
                {
                    IDXGIAdapter1* adapter=nullptr;
                    if (factory->EnumAdapters1(i,&adapter)!=S_OK)break;
                    DXGI_ADAPTER_DESC1 desc{};
                    if(SUCCEEDED(adapter->GetDesc1(&desc)) &&
                        !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
                        desc.VendorId==0x10DE)
                    {
                        if(SUCCEEDED(adapter->QueryInterface(
                            __uuidof(IDXGIAdapter3),
                            reinterpret_cast<void**>(&adapter_))))
                        {
                            adapterName_="NVIDIA dedicated adapter";
                            adapter->Release();
                            break;
                        }
                    }
                    adapter->Release();
                }
                factory->Release();
            }
        }
        ~G13E13PerfProbe()
        {
            if(adapter_)adapter_->Release();
        }
        bool Enabled() const noexcept {return !csvPath_.empty();}
        bool Complete() const noexcept {return complete_;}
        void Frame(bool sceneReady, unsigned long long graphicsFrame)
        {
            if(!Enabled() || complete_ || !sceneReady)return;
            const auto now=std::chrono::steady_clock::now();
            if(graphicsFrame<=lastGraphicsFrame_)return;
            lastGraphicsFrame_=graphicsFrame;
            if(!started_)
            {
                started_=true;previous_=now;return;
            }
            const double ms=std::chrono::duration<double,std::milli>(
                now-previous_).count();
            previous_=now;
            if(++warmup_<=120)return;
            if(!std::isfinite(ms) || ms<=0)return;
            frames_.push_back(ms);
            if(frames_.size()==1 || frames_.size()%30==0)
            {
                if(adapter_)
                {
                    DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
                    if(SUCCEEDED(adapter_->QueryVideoMemoryInfo(
                        0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&memory)))
                    {
                        const double mib=double(memory.CurrentUsage)/(1024.*1024.);
                        memoryMiB_.push_back(mib);
                        budgetMiB_=double(memory.Budget)/(1024.*1024.);
                    }
                }
            }
            if(frames_.size()>=std::size_t(sampleTarget_))
            {
                Write();
                complete_=true;
            }
        }
    private:
        void Write()
        {
            // The output path belongs to a disposable test. No production
            // package content, user project or renderer setting is modified.
            std::ofstream csv(std::filesystem::u8path(csvPath_),std::ios::trunc);
            if(csv)
            {
                csv<<"frame,host_loop_ms\n";
                csv<<std::fixed<<std::setprecision(4);
                for(std::size_t i=0;i<frames_.size();++i)
                    csv<<i+1<<','<<frames_[i]<<'\n';
                csv.close();
            }
            auto sorted=frames_;
            std::sort(sorted.begin(),sorted.end());
            const auto pct=[&](double p){
                const auto idx=std::min(sorted.size()-1,
                    std::size_t(std::ceil(p*sorted.size()))-1);
                return sorted[idx];
            };
            const double total=std::accumulate(frames_.begin(),frames_.end(),0.0);
            const double mean=total/frames_.size();
            const auto spikes=std::count_if(frames_.begin(),frames_.end(),
                [](double ms){return ms>33.3333;});
            std::ofstream out(std::filesystem::u8path(csvPath_+".summary.txt"),
                std::ios::trunc);
            if(!out)return;
            out<<std::fixed<<std::setprecision(3)
                <<"schema=g13e13-optin-runtime-host-loop-v1\n"
                <<"sample_frames="<<frames_.size()<<"\n"
                <<"warmup_frames=120\n"
                <<"mean_host_loop_ms="<<mean<<"\n"
                <<"mean_host_loop_fps_equivalent="<<(1000.0/mean)<<"\n"
                <<"median_host_loop_ms="<<pct(.5)<<"\n"
                <<"p95_host_loop_ms="<<pct(.95)<<"\n"
                <<"p99_host_loop_ms="<<pct(.99)<<"\n"
                <<"max_host_loop_ms="<<sorted.back()<<"\n"
                <<"frames_over_33ms="<<spikes<<"\n"
                <<"dxgi_adapter="<<adapterName_<<"\n"
                <<"gpu_local_memory_samples="<<memoryMiB_.size()<<"\n";
            if(!memoryMiB_.empty())
            {
                auto bounds=std::minmax_element(memoryMiB_.begin(),memoryMiB_.end());
                out<<"gpu_local_process_usage_start_mib="<<memoryMiB_.front()<<"\n"
                   <<"gpu_local_process_usage_end_mib="<<memoryMiB_.back()<<"\n"
                   <<"gpu_local_process_usage_peak_mib="<<*bounds.second<<"\n"
                   <<"gpu_local_budget_mib="<<budgetMiB_<<"\n";
            }
            out<<"scope=stationary_or_bounded_camera_runtime_scene_not_gpu_frametime\n"
                <<"limitation=host_loop_interval_includes_present_cpu_waits_not_gpu_execution_time\n";
        }
        std::string csvPath_;
        std::string adapterName_="unavailable";
        IDXGIAdapter3* adapter_=nullptr;
        int sampleTarget_=720;
        bool started_=false, complete_=false;
        unsigned warmup_=0;
        unsigned long long lastGraphicsFrame_=0;
        std::chrono::steady_clock::time_point previous_{};
        std::vector<double> frames_;
        std::vector<double> memoryMiB_;
        double budgetMiB_=0;
    };
}
