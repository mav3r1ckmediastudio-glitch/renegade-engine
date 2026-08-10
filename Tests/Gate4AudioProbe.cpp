#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <objbase.h>
#include <xaudio2.h>

#include <iomanip>
#include <iostream>

#pragma comment(lib, "xaudio2.lib")

namespace
{
    constexpr int AudioEndpointUnavailableExitCode = 77;

    void PrintHresult(const char* label, const HRESULT hr)
    {
        std::cout << label << "=0x"
                  << std::hex << std::uppercase
                  << static_cast<unsigned long>(hr)
                  << std::dec << '\n';
    }
}

int main()
{
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize = SUCCEEDED(comResult);
    if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE)
    {
        PrintHresult("GATE4_DEBUG_AUDIO_COM_FAILED", comResult);
        return 2;
    }

    IXAudio2* engine = nullptr;
    const HRESULT engineResult =
        XAudio2Create(&engine, 0, XAUDIO2_USE_DEFAULT_PROCESSOR);
    if (FAILED(engineResult) || engine == nullptr)
    {
        PrintHresult("GATE4_DEBUG_AUDIO_XAUDIO2_CREATE_FAILED", engineResult);
        if (shouldUninitialize)
            CoUninitialize();
        return 3;
    }

    IXAudio2MasteringVoice* masteringVoice = nullptr;
    const HRESULT masteringResult =
        engine->CreateMasteringVoice(&masteringVoice);
    if (FAILED(masteringResult) || masteringVoice == nullptr)
    {
        PrintHresult(
            "GATE4_DEBUG_AUDIO_ENDPOINT_UNAVAILABLE_HRESULT",
            masteringResult);
        std::cout << "GATE4_DEBUG_AUDIO_ENDPOINT_UNAVAILABLE\n";
        engine->Release();
        if (shouldUninitialize)
            CoUninitialize();
        return AudioEndpointUnavailableExitCode;
    }

    masteringVoice->DestroyVoice();
    engine->Release();
    if (shouldUninitialize)
        CoUninitialize();

    std::cout << "GATE4_DEBUG_AUDIO_ENDPOINT_AVAILABLE\n";
    return 0;
}
