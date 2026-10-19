#pragma once
#include "renegade/bridge/ScreenService.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace renegade::screen
{
    struct ScreenTextLine {std::string text;float width=0;};
    inline std::size_t ScreenTextCodepointEnd(std::string_view text,std::size_t begin) noexcept
    {
        auto end=begin+1;
        while(end<text.size()&&(static_cast<unsigned char>(text[end])&0xC0)==0x80)++end;
        return end;
    }
    // Font metrics are supplied by the shared native presenter. Never split an
    // UTF-8 codepoint; prefer word boundaries, then bound an oversized word.
    template<class Measure> std::vector<ScreenTextLine> WrapScreenText(
        std::string_view text,float width,Measure measure)
    {
        std::vector<ScreenTextLine> lines;
        if(!std::isfinite(width)||width<=0)return lines;
        std::string line,spaces;
        const auto append=[&](std::string value)
        {const auto measured=measure(value);lines.push_back({std::move(value),measured});};
        std::size_t cursor=0;
        while(cursor<text.size())
        {
            const char c=text[cursor];
            if(c=='\r'||c=='\n')
            {
                append(std::move(line));line.clear();spaces.clear();
                ++cursor;if(c=='\r'&&cursor<text.size()&&text[cursor]=='\n')++cursor;
                continue;
            }
            if(c==' '||c=='\t'){spaces+=c;++cursor;continue;}
            const auto begin=cursor;
            while(cursor<text.size()&&text[cursor]!=' '&&text[cursor]!='\t'&&text[cursor]!='\r'&&text[cursor]!='\n')
                cursor=ScreenTextCodepointEnd(text,cursor);
            std::string word(text.substr(begin,cursor-begin));
            auto candidate=line+spaces+word;
            if(measure(candidate)<=width){line=std::move(candidate);spaces.clear();continue;}
            if(!line.empty()){append(std::move(line));line.clear();}
            spaces.clear();
            while(!word.empty()&&measure(word)>width)
            {
                std::vector<std::size_t> ends;
                for(std::size_t n=0;n<word.size();) {n=ScreenTextCodepointEnd(word,n);ends.push_back(n);}
                std::size_t low=0,high=ends.size();
                while(low<high)
                {
                    const auto mid=(low+high)/2;
                    if(measure(word.substr(0,ends[mid]))<=width)low=mid+1;else high=mid;
                }
                const auto bytes=ends[low?low-1:0]; // even a too-wide glyph makes progress
                append(word.substr(0,bytes));word.erase(0,bytes);
            }
            line=std::move(word);
        }
        append(std::move(line));
        return lines;
    }
    struct ScreenTextLineOrigin {float x=0,y=0;};
    inline ScreenTextLineOrigin ResolveScreenTextLineOrigin(const bridge::ScreenRect& content,
        float lineWidth,float blockHeight,float lineOffset,
        bridge::ScreenHorizontalAlignment horizontal,bridge::ScreenVerticalAlignment vertical) noexcept
    {
        const float freeX=std::max(0.0f,content.width-lineWidth);
        const float freeY=std::max(0.0f,content.height-blockHeight);
        const float x=horizontal==bridge::ScreenHorizontalAlignment::Center?freeX*.5f:
            horizontal==bridge::ScreenHorizontalAlignment::Right?freeX:0;
        const float y=vertical==bridge::ScreenVerticalAlignment::Center?freeY*.5f:
            vertical==bridge::ScreenVerticalAlignment::Bottom?freeY:0;
        return {content.x+x,content.y+y+lineOffset};
    }
}
