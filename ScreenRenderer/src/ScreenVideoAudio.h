#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace renegade::screen
{
    class ScreenVideoAudio
    {
    public:
        ScreenVideoAudio();
        ~ScreenVideoAudio();
        bool Open(const std::vector<std::uint8_t>& mp4);
        void Update(float seconds, bool active);
    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
