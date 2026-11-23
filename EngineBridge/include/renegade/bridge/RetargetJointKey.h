#pragma once
#include <algorithm>
#include <cctype>
#include <map>
#include <string>

namespace renegade::bridge {
// Normalize common DCC humanoid conventions before falling back to role inference.
inline std::string RetargetJointKey(std::string name)
{
    const auto ns=name.find_last_of(":|");
    if(ns!=std::string::npos)name.erase(0,ns+1);
    for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
    // Biped accepts an arbitrary instance number and several export separators.
    if(name.rfind("bip",0)==0 && name.size()>3 && std::isdigit(static_cast<unsigned char>(name[3]))) {
        size_t end=3;
        while(end<name.size() && std::isdigit(static_cast<unsigned char>(name[end])))++end;
        if(end==name.size())return "root";
        if(name[end]==' ' || name[end]=='_' || name[end]=='-' || name[end]=='.')name.erase(0,end+1);
    }
    for(const std::string prefix:{"humanoid_","mixamorig"})
        if(name.rfind(prefix,0)==0){name.erase(0,prefix.size());break;}
    const auto start=name.find_first_not_of(" _");
    if(start==std::string::npos)return {};
    name.erase(0,start);
    std::string side;
    if(name.rfind("left",0)==0){side="l";name.erase(0,4);}
    else if(name.rfind("right",0)==0){side="r";name.erase(0,5);}
    else if(name.size()>2 && (name[0]=='l'||name[0]=='r') && (name[1]==' '||name[1]=='_')){
        side=name.substr(0,1);name.erase(0,2);
    }else if(name.size()>2 && name[name.size()-2]=='_' && (name.back()=='r'||name.back()=='l')){
        side=name.substr(name.size()-1);name.resize(name.size()-2);
    }
    name.erase(std::remove_if(name.begin(),name.end(),[](char c){return c=='_'||c==' '||c=='-';}),name.end());
    static const std::map<std::string,std::string> aliases={
        {"hips","pelvis"},{"spine","spine01"},{"spine1","spine02"},{"spine2","spine03"},
        {"neck","neck01"},{"upperleg","thigh"},{"upleg","thigh"},{"lowerleg","calf"},
        {"forearm","lowerarm"},{"elbow","lowerarm"},{"ankle","foot"},{"toe0","ball"},
        {"toes","ball"},{"leg","calf"},{"arm","upperarm"},{"shoulder","clavicle"}
    };
    const auto found=aliases.find(name);
    if(found!=aliases.end())name=found->second;
    if(name.rfind("finger",0)==0 && (name.size()==7||name.size()==8) &&
       name[6]>='0' && name[6]<='4' && (name.size()==7 || name[7]=='1'||name[7]=='2')){
        static const char* digits[]={"thumb","index","middle","ring","pinky"};
        name=std::string(digits[name[6]-'0'])+"0"+char(name.size()==7?'1':name[7]+1);
    }
    if(name.rfind("indexfinger",0)==0)name.erase(5,6);
    return name.empty()?name:name+side;
}

}
