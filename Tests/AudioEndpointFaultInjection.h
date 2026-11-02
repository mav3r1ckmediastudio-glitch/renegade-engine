#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <xaudio2.h>
extern HRESULT RenegadeTestMasteringVoiceResult;
inline HRESULT RenegadeTestCreateMasteringVoice(IXAudio2*,IXAudio2MasteringVoice** output)
{
    if(output)*output=nullptr;
    return RenegadeTestMasteringVoiceResult;
}
