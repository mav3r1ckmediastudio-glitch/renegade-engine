#pragma once
#include "renegade/bridge/ScreenBindingFrame.h"
#include <cstring>
#include <set>
#include <tuple>

namespace renegade::bridge
{
    // Diagnostic-only v1 wire format: little-endian integers, IEEE double bits,
    // length-prefixed UTF-8, then hex for the existing bounded JSON transport.
    // No commands, document mutations or substitute samples exist on this path.
    inline constexpr std::size_t ScreenLiveWireLimit = 28 * 1024;
    struct ScreenLiveBinding
    {
        ScreenBindingSnapshot snapshot;
        ScreenBindingContext context;
    };
    namespace screen_live_detail
    {
        inline bool Utf8(std::string_view s)
        {
            for(std::size_t i=0;i<s.size();)
            {
                const auto c=static_cast<unsigned char>(s[i++]);
                if(c<128)continue;
                unsigned n=0,cp=0,min=0;
                if(c>=0xc2&&c<=0xdf){n=1;cp=c&31;min=128;}
                else if(c>=0xe0&&c<=0xef){n=2;cp=c&15;min=2048;}
                else if(c>=0xf0&&c<=0xf4){n=3;cp=c&7;min=65536;}
                else return false;
                if(n>s.size()-i)return false;
                while(n--){const auto d=static_cast<unsigned char>(s[i++]);if((d&192)!=128)return false;cp=(cp<<6)|(d&63);}
                if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;
            }
            return true;
        }
        inline bool Validate(const ScreenLiveBinding& f)
        {
            if(!f.snapshot.session||!f.snapshot.revision||f.context.session!=f.snapshot.session||
                f.snapshot.values.size()>256||f.context.generations.size()>256)return false;
            std::set<std::tuple<int,StableId,std::string>> seen;
            std::string error;
            for(const auto& [target,generation]:f.context.generations)
            {
                ScreenBindingSource source{target.first,target.second,"value"};
                if(!ValidateScreenBindingSource(source,error)||!generation)return false;
            }
            for(const auto& s:f.snapshot.values)
            {
                if(!ValidateScreenBindingSource(s.source,error)||!Utf8(s.source.attributeId)||
                    !seen.emplace(int(s.source.scope),s.source.targetId,s.source.attributeId).second)return false;
                if(s.source.scope!=ScreenBindingScope::ProjectAttribute&&
                    (!s.generation||s.generation!=f.context.Generation(s.source)))return false;
                if(const auto* n=std::get_if<double>(&s.value);n&&!std::isfinite(*n))return false;
                if(const auto* t=std::get_if<std::string>(&s.value);t&&(t->size()>4096||!Utf8(*t)))return false;
            }
            return true;
        }
        struct Writer
        {
            std::string bytes;
            void Number(std::uint64_t n){for(int i=0;i<8;++i){bytes+=char(n&255);n>>=8;}}
            void Text(const std::string& s){Number(s.size());bytes+=s;}
        };
        struct Reader
        {
            std::string_view bytes;std::size_t pos=0;bool ok=true;
            std::uint64_t Number(){if(bytes.size()-pos<8){ok=false;return 0;}std::uint64_t n=0;for(int i=0;i<8;++i)n|=std::uint64_t(static_cast<unsigned char>(bytes[pos++]))<<(i*8);return n;}
            std::string Text(std::size_t limit){const auto n=Number();if(!ok||n>limit||n>bytes.size()-pos){ok=false;return {};}auto s=std::string(bytes.substr(pos,std::size_t(n)));pos+=std::size_t(n);return s;}
        };
    }
    inline bool EncodeScreenLiveBinding(const ScreenLiveBinding& frame,std::string& hex,std::string& error)
    {
        hex.clear();
        if(!screen_live_detail::Validate(frame)){error="Invalid live binding snapshot or source budget.";return false;}
        screen_live_detail::Writer w;w.Number(1);w.Number(frame.snapshot.session);w.Number(frame.snapshot.revision);
        w.Number(frame.context.generations.size());
        for(const auto& [target,generation]:frame.context.generations)
        {w.Number(std::uint64_t(target.first));w.Text(target.second);w.Number(generation);}
        w.Number(frame.snapshot.values.size());
        for(const auto& s:frame.snapshot.values)
        {
            w.Number(std::uint64_t(s.source.scope));w.Text(s.source.targetId);w.Text(s.source.attributeId);
            w.Number(s.generation);w.Number(s.value.index());
            if(const auto* n=std::get_if<double>(&s.value)){std::uint64_t bits;std::memcpy(&bits,n,8);w.Number(bits);}
            else if(const auto* b=std::get_if<bool>(&s.value))w.Number(*b?1:0);
            else w.Text(std::get<std::string>(s.value));
            if(w.bytes.size()>ScreenLiveWireLimit){error="Live binding snapshot exceeds diagnostic wire budget.";return false;}
        }
        if(w.bytes.size()>ScreenLiveWireLimit){error="Live binding context exceeds diagnostic wire budget.";return false;}
        constexpr char digits[]="0123456789abcdef";hex.reserve(w.bytes.size()*2);
        for(unsigned char c:w.bytes){hex+=digits[c>>4];hex+=digits[c&15];}
        error.clear();return true;
    }
    inline bool DecodeScreenLiveBinding(std::string_view hex,ScreenLiveBinding& frame,std::string& error)
    {
        const auto fail=[&](){error="Malformed, unsupported or oversized live binding snapshot.";return false;};
        if(hex.empty()||hex.size()>ScreenLiveWireLimit*2||hex.size()%2)return fail();
        std::string bytes;bytes.reserve(hex.size()/2);
        const auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
        for(std::size_t i=0;i<hex.size();i+=2){int a=digit(hex[i]),b=digit(hex[i+1]);if(a<0||b<0)return fail();bytes+=char(a*16+b);}
        screen_live_detail::Reader r{bytes};ScreenLiveBinding next;
        if(r.Number()!=1)return fail();next.snapshot.session=next.context.session=r.Number();next.snapshot.revision=r.Number();
        const auto generations=r.Number();if(generations>256)return fail();
        for(std::uint64_t i=0;i<generations&&r.ok;++i)
        {
            const auto scope=r.Number();const auto id=r.Text(128);const auto g=r.Number();
            if(scope>2||!next.context.generations.emplace(ScreenBindingContext::Target{ScreenBindingScope(scope),id},g).second)return fail();
        }
        const auto samples=r.Number();if(samples>256)return fail();
        for(std::uint64_t i=0;i<samples&&r.ok;++i)
        {
            ScreenBindingSample s;const auto scope=r.Number();if(scope>2)return fail();s.source.scope=ScreenBindingScope(scope);
            s.source.targetId=r.Text(128);s.source.attributeId=r.Text(128);s.generation=r.Number();const auto type=r.Number();
            if(type==0){auto bits=r.Number();double n;std::memcpy(&n,&bits,8);s.value=n;}
            else if(type==1){auto b=r.Number();if(b>1)return fail();s.value=b!=0;}
            else if(type==2)s.value=r.Text(4096);else return fail();
            next.snapshot.values.push_back(std::move(s));
        }
        if(!r.ok||r.pos!=bytes.size()||!screen_live_detail::Validate(next))return fail();
        frame=std::move(next);error.clear();return true;
    }
}
