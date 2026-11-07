#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
namespace renegade::bridge {
class TestLevelLaunchTiming {
 std::string root_, scope_; std::chrono::steady_clock::time_point start_, last_;
public:
 TestLevelLaunchTiming(std::string root,std::string scope):root_(std::move(root)),scope_(std::move(scope)),start_(std::chrono::steady_clock::now()),last_(start_){}
 void Mark(const char* stage) noexcept {try{if(root_.empty())return;auto now=std::chrono::steady_clock::now();auto dir=std::filesystem::u8path(root_)/"Intermediate";std::error_code ec;std::filesystem::create_directories(dir,ec);if(ec)return;std::ofstream f(dir/"TestLevelLaunchTimings.tsv",std::ios::app);f<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()<<'\t'<<scope_<<'.'<<stage<<'\t'<<std::chrono::duration_cast<std::chrono::milliseconds>(now-last_).count()<<'\t'<<std::chrono::duration_cast<std::chrono::milliseconds>(now-start_).count()<<'\n';last_=now;}catch(...){}}
 ~TestLevelLaunchTiming(){Mark("total");}
};
}
