#pragma once
#include <cmath>
#include <array>
// Same alpha quantization and nearest-neighbour grid as the existing PNG metric.
// PGM avoids storing HDR RGB noise during large calibration runs.
inline bool SaveCloudOpacityGrid(const wi::graphics::Texture& texture,
    const std::filesystem::path& output)
{
    using namespace wi::graphics;
    if(texture.desc.format!=Format::R16G16B16A16_FLOAT) return false;
    wi::vector<uint8_t> bytes;
    if(!wi::helper::saveTextureToMemory(texture,bytes) ||
        bytes.size()!=size_t(texture.desc.width)*texture.desc.height*8) return false;
    std::array<uint8_t,320*180> grid{};
    for(uint32_t y=0;y<180;++y) for(uint32_t x=0;x<320;++x)
    {
        const uint32_t sx=std::min(texture.desc.width-1,uint32_t((double(x)+.5)*texture.desc.width/320));
        const uint32_t sy=std::min(texture.desc.height-1,uint32_t((double(y)+.5)*texture.desc.height/180));
        const size_t offset=(size_t(sy)*texture.desc.width+sx)*8+6;
        const uint16_t half=uint16_t(bytes[offset])|(uint16_t(bytes[offset+1])<<8);
        const uint32_t exponent=(half>>10)&31,mantissa=half&1023;
        if(exponent==31) return false;
        float alpha=exponent ? std::ldexp(float(1024+mantissa),int(exponent)-25) :
            std::ldexp(float(mantissa),-24);
        if(half&0x8000) alpha=-alpha;
        alpha=std::clamp(alpha,0.0f,1.0f);
        grid[y*320+x]=uint8_t(alpha*255.0f);
    }
    std::ofstream file(output,std::ios::binary);
    file<<"P5\n320 180\n255\n";
    file.write(reinterpret_cast<const char*>(grid.data()),grid.size());
    return bool(file);
}
