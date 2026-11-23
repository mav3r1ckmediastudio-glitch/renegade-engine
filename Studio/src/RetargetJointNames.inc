// Normalize common DCC humanoid conventions before falling back to role inference.
static std::string RetargetJointKey(std::string name)
{
    const auto ns=name.find_last_of(":|");
    if(ns!=std::string::npos)name.erase(0,ns+1);
    for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
    for(const std::string prefix:{"humanoid_","bip001 ","bip01 ","mixamorig"})
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
        {"arm","upperarm"},{"shoulder","clavicle"}
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

static void SuggestNamedPairs(const std::vector<std::string>& source,const std::vector<std::string>& target,
                             std::vector<int>& pairs,std::set<int>& used)
{
    std::map<std::string,std::vector<size_t>> s,t;
    for(size_t i=0;i<source.size();++i)s[RetargetJointKey(source[i])].push_back(i);
    for(size_t i=0;i<target.size();++i)t[RetargetJointKey(target[i])].push_back(i);
    // Some rigs have both Clavicle and Shoulder. In that hierarchy Shoulder is the upper arm.
    for(const auto side:{"l","r"}){
        for(auto* dict:{&s,&t}){
            auto& clav=(*dict)[std::string("clavicle")+side];
            if(clav.size()!=2)continue;
            const auto& names=dict==&s?source:target;
            for(auto it=clav.begin();it!=clav.end();++it){
                auto n=names[*it];for(char& c:n)c=char(std::tolower(static_cast<unsigned char>(c)));
                if(n.find("shoulder")==std::string::npos)continue;
                (*dict)[std::string("upperarm")+side].push_back(*it);clav.erase(it);break;
            }
        }
    }
    for(const auto& group:s){
        const auto it=t.find(group.first);
        if(group.first.empty() || group.second.size()!=1 || it==t.end() || it->second.size()!=1)continue;
        auto i=group.second.front();int j=int(it->second.front());
        if(pairs[i]<0 && used.insert(j).second)pairs[i]=j;
    }
}
