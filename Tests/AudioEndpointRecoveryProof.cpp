#include "AudioEndpointFaultInjection.h"
#include <WickedEngine.h>
#include <cstdlib>
#include <iostream>
#include <string>
HRESULT RenegadeTestMasteringVoiceResult=E_FAIL;
namespace wi::audio { bool RenegadeTestAudioReady(); }
int main(int argc,char**argv)
{
    if(argc!=2)return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_error_mode(_OUT_TO_STDERR);
    RenegadeTestMasteringVoiceResult=std::string(argv[1])=="missing"?HRESULT_FROM_WIN32(ERROR_NOT_FOUND):E_FAIL;
    std::cout<<"INJECTED_MASTERING_HRESULT 0x"<<std::hex<<static_cast<unsigned>(RenegadeTestMasteringVoiceResult)<<std::dec<<std::endl;
    wi::audio::Initialize();
    if(wi::audio::RenegadeTestAudioReady()){std::cerr<<"AUDIO_ENDPOINT_PROOF_FAIL unavailable endpoint became ready"<<std::endl;return 3;}
    std::cout<<wi::backlog::getText()<<std::endl;
    std::cout<<"AUDIO_ENDPOINT_UNAVAILABLE_NOT_READY_PASS"<<std::endl;
    return 0;
}
