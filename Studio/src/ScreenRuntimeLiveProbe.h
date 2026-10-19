#pragma once
#include <charconv>
#include "renegade/bridge/ScreenLiveBinding.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace renegade::studio
{
    // Transport identity and application freshness are deliberately separate:
    // a background Runtime can answer the endpoint while its game loop stops.
    struct ScreenRuntimeLiveStatus
    {
        bool connected=false;
        bool gameplayFresh=false;
        bool projectMatched=false;
        bool hudLoaded=false;
        bool bindingsReady=false;
        std::uint64_t pid=0,processStartedUnixMs=0,heartbeatAgeMs=0;
        std::uint64_t bindingRevision=0,bindingIssues=0;
        std::string projectId;
        std::string detail="LIVE TEST // DISCONNECTED / STALE";
    };
    inline std::string_view LiveJsonObject(std::string_view json,std::string_view key)
    {
        const auto marker=std::string("\"")+std::string(key)+"\":{";
        const auto begin=json.find(marker);
        if(begin==std::string_view::npos)return {};
        const auto content=begin+marker.size();
        bool quoted=false,escaped=false;unsigned depth=1;
        for(auto end=content;end<json.size()&&end-content<131072;++end)
        {
            const char c=json[end];
            if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;continue;}
            if(c=='"'){quoted=true;continue;}
            if(c=='{')++depth;
            if(c=='}'&&!--depth)return json.substr(content,end-content);
        }
        return {};
    }
    inline bool LiveJsonNumber(std::string_view json,std::string_view key,std::uint64_t& result)
    {
        const auto marker=std::string("\"")+std::string(key)+"\":";
        const auto start=json.find(marker);
        if(start==std::string_view::npos)return false;
        const auto first=start+marker.size();
        if(first>=json.size()||json[first]<'0'||json[first]>'9')return false;
        auto last=first;
        while(last<json.size()&&json[last]>='0'&&json[last]<='9')++last;
        const auto parsed=std::from_chars(json.data()+first,json.data()+last,result);
        return parsed.ec==std::errc{}&&parsed.ptr==json.data()+last;
    }
    inline bool LiveJsonBool(std::string_view json,std::string_view key,bool& result)
    {
        const auto marker=std::string("\"")+std::string(key)+"\":";
        const auto start=json.find(marker);
        if(start==std::string_view::npos)return false;
        const auto first=start+marker.size();
        if(json.substr(first,4)=="true"){result=true;return true;}
        if(json.substr(first,5)=="false"){result=false;return true;}
        return false;
    }
    // Project identity is an unescaped, bounded stable ID emitted by our own
    // diagnostics contract, not an arbitrary user-facing JSON string.
    inline bool LiveProjectId(std::string_view json,std::string& result)
    {
        const auto marker=std::string_view("\"project_id\":\"");
        const auto begin=json.find(marker);
        if(begin==std::string_view::npos)return false;
        const auto start=begin+marker.size();
        const auto end=json.find('"',start);
        if(end==std::string_view::npos||end-start==0||end-start>128)return false;
        const auto id=json.substr(start,end-start);
        for(const auto ch:id)
            if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='z')||
                 (ch>='A'&&ch<='Z')||ch=='-'||ch=='_'))return false;
        result.assign(id);
        return true;
    }
    inline ScreenRuntimeLiveStatus ProbeScreenRuntimeLive(
        std::string_view json,std::string_view expectedProjectId={},
        std::uint64_t maxHeartbeatAgeMs=2000)
    {
        ScreenRuntimeLiveStatus state;
        if(json.empty())return state;
        if(json.find("\"schema\":\"renegade.diagnostics.v2\"")==std::string_view::npos)
        {state.detail="LIVE TEST // UNRECOGNISED DIAGNOSTIC CONTRACT";return state;}
        bool transport=false;
        if(!LiveJsonBool(json,"transport_alive",transport)||!transport)
        {state.detail="LIVE TEST // DIAGNOSTIC TRANSPORT NOT AVAILABLE";return state;}
        const auto process=LiveJsonObject(json,"process");
        if(process.empty()||process.find("\"type\":\"runtime\"")==std::string_view::npos||
           !LiveJsonNumber(process,"pid",state.pid)||state.pid==0||
           !LiveJsonNumber(process,"started_unix_ms",state.processStartedUnixMs)||
           state.processStartedUnixMs==0)
        {state.detail="LIVE TEST // RUNTIME IDENTITY UNVERIFIED";return state;}
        // Receiving a fresh loopback response proves transport connectivity.
        // It DOES NOT prove the render/gameplay loop is advancing.
        state.connected=true;
        if(!LiveJsonNumber(json,"application_heartbeat_age_ms",state.heartbeatAgeMs))
            if(!LiveJsonNumber(json,"heartbeat_age_ms",state.heartbeatAgeMs))
            {state.detail="RUNTIME CONNECTED // GAMEPLAY FRESHNESS UNKNOWN";return state;}
        state.gameplayFresh=state.heartbeatAgeMs<=maxHeartbeatAgeMs;
        const auto ui=LiveJsonObject(json,"ui");
        if(!ui.empty())
        {
            if(LiveProjectId(ui,state.projectId))
                state.projectMatched=!expectedProjectId.empty()&&state.projectId==expectedProjectId;
            LiveJsonBool(ui,"hud_loaded",state.hudLoaded);
            LiveJsonBool(ui,"hud_bindings_ready",state.bindingsReady);
            LiveJsonNumber(ui,"hud_binding_issues",state.bindingIssues);
            LiveJsonNumber(ui,"ui_binding_revision",state.bindingRevision);
        }
        const auto prefix="RUNTIME CONNECTED // PID "+std::to_string(state.pid)+
            "\nBOOT "+std::to_string(state.processStartedUnixMs);
        if(expectedProjectId.empty()||state.projectId.empty())
            state.detail=prefix+"\nPROJECT IDENTITY UNVERIFIED; NO LIVE VALUE CLAIM";
        else if(!state.projectMatched)
            state.detail=prefix+"\nOTHER PROJECT; LIVE TEST DATA NOT APPLICABLE";
        else if(!state.gameplayFresh)
            state.detail=prefix+"\nGAMEPLAY NOT ADVANCING ("+
                std::to_string(state.heartbeatAgeMs)+"ms since app update)"+
                "\nHUD/BINDING SNAPSHOT STALE; NO LIVE VALUE CLAIM";
        else
            state.detail="MATCHED LIVE RUNTIME // PID "+std::to_string(state.pid)+
                "\nHEARTBEAT "+std::to_string(state.heartbeatAgeMs)+
                "ms // HUD "+(state.hudLoaded?"LOADED":"UNAVAILABLE")+
                "\nBINDINGS "+(state.bindingsReady?"READY":"UNRESOLVED")+
                " / ISSUES "+std::to_string(state.bindingIssues)+
                " / REV "+std::to_string(state.bindingRevision)+
                "\nREAD-ONLY; NO GAMEPLAY WRITES";
        return state;
    }
    inline bool ReadScreenRuntimeLiveBinding(std::string_view json,
        const ScreenRuntimeLiveStatus& status,bridge::ScreenLiveBinding& result,std::string& error)
    {
        const auto fail=[&](const char* message){error=message;return false;};
        if(!status.connected||!status.gameplayFresh||!status.projectMatched)
            return fail("Live values unavailable: Runtime is disconnected, stale or from another project.");
        const auto group=LiveJsonObject(json,"ui_live_binding");
        bool available=false;std::uint64_t count=0,published=0,now=0;
        if(!LiveJsonBool(group,"available",available)||!available||
            !LiveJsonNumber(group,"chunk_count",count)||!count||count>60||
            !LiveJsonNumber(group,"published_elapsed_ms",published)||
            !LiveJsonNumber(json,"elapsed_ms",now)||published>now||now-published>2000)
            return fail("Live binding snapshot unavailable or stale.");
        std::string hex;
        for(std::uint64_t i=0;i<count;++i)
        {
            const auto marker="\"chunk_"+std::to_string(i)+"\":\"";
            const auto at=group.find(marker);
            if(at==std::string_view::npos)return fail("Live snapshot chunk missing.");
            const auto begin=at+marker.size(),end=group.find('"',begin);
            if(end==std::string_view::npos||end==begin||end-begin>960)
                return fail("Live snapshot chunk invalid.");
            hex.append(group.substr(begin,end-begin));
        }
        return bridge::DecodeScreenLiveBinding(hex,result,error);
    }
}
