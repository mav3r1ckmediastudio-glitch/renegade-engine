#pragma once
#include <vector>
#include <cctype>
#include <string>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <array>
#include <algorithm>
namespace renegade::bridge::detail
{
    // Recontainerise supported mono/stereo extensible PCM. Sample bytes are
    // unchanged; the original file never reaches the incompatible native loader.
    inline bool NormaliseUiPcmWave(const std::string& source,std::vector<std::uint8_t>& output)
    {
        namespace fs=std::filesystem;
        auto extension=fs::u8path(source).extension().u8string();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
        if(extension!=".wav")return false;
        std::ifstream in(fs::u8path(source),std::ios::binary|std::ios::ate);
        const auto size=in?in.tellg():std::streampos(-1);
        if(size<68||size>128*1024*1024)return false;
        std::vector<std::uint8_t> bytes(static_cast<size_t>(size));in.seekg(0);
        if(!in.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))return false;
        const auto u16=[&](size_t p){return std::uint16_t(bytes[p])|std::uint16_t(bytes[p+1])<<8;};
        const auto u32=[&](size_t p){return std::uint32_t(bytes[p])|std::uint32_t(bytes[p+1])<<8|std::uint32_t(bytes[p+2])<<16|std::uint32_t(bytes[p+3])<<24;};
        const auto tag=[&](size_t p,const char* name){return std::equal(name,name+4,bytes.begin()+p);};
        if(!tag(0,"RIFF")||!tag(8,"WAVE"))return false;
        const auto end=std::uint64_t(u32(4))+8;if(end>bytes.size()||end<12)return false;
        size_t format=0,data=0;std::uint32_t dataSize=0;
        for(std::uint64_t p=12;p+8<=end;)
        {
            const auto count=u32(size_t(p+4));const auto payload=p+8;
            if(count>end-payload)return false;
            if(tag(size_t(p),"fmt ")){if(format||count!=40)return false;format=size_t(payload);}
            if(tag(size_t(p),"data")){if(data||!count)return false;data=size_t(payload);dataSize=count;}
            p=payload+count+(count&1u);
        }
        if(!format||!data||u16(format)!=0xfffe||u16(format+16)!=22)return false;
        constexpr std::array<std::uint8_t,16> pcm{1,0,0,0,0,0,16,0,128,0,0,170,0,56,155,113};
        if(!std::equal(pcm.begin(),pcm.end(),bytes.begin()+format+24))return false;
        const auto channels=u16(format+2),bits=u16(format+14);const auto rate=u32(format+4),mask=u32(format+20);
        if((channels!=1&&channels!=2)||(bits!=8&&bits!=16&&bits!=24&&bits!=32)||
            u16(format+18)!=bits||rate<1000||rate>200000||
            (mask!=0&&mask!=(channels==1?4u:3u)))return false;
        const auto align=channels*bits/8;
        if(u16(format+12)!=align||u32(format+8)!=rate*align||dataSize%align)return false;
        std::vector<std::uint8_t> next{'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0};
        next.insert(next.end(),bytes.begin()+format,bytes.begin()+format+16);next[20]=1;next[21]=0;
        next.insert(next.end(),{'d','a','t','a'});
        for(unsigned i=0;i<4;++i)next.push_back(std::uint8_t(dataSize>>(8*i)));
        next.insert(next.end(),bytes.begin()+data,bytes.begin()+data+dataSize);
        if(dataSize&1)next.push_back(0);
        const auto riffSize=std::uint32_t(next.size()-8);for(unsigned i=0;i<4;++i)next[4+i]=std::uint8_t(riffSize>>(8*i));
        output=std::move(next);return true;
    }
}
