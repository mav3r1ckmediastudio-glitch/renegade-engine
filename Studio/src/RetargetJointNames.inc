#include "renegade/bridge/RetargetJointKey.h"
using renegade::bridge::RetargetJointKey;

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
