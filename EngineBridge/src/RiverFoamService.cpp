#include "renegade/bridge/RiverFoamService.h"
#include "json.hpp"
#include <cmath>
namespace renegade::bridge
{
namespace {
bool Water(const wi::scene::Scene& s,wi::ecs::Entity e)
{
    const auto* m=s.materials.GetComponent(e);
    return m && m->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER;
}
bool Range(float v,float lo,float hi){return std::isfinite(v)&&v>=lo&&v<=hi;}
std::string Encode(const std::vector<RiverFoamRegion>& regions)
{
    nlohmann::json a=nlohmann::json::array();
    for(const auto& p:regions)a.push_back({{"centre",{p.centre.x,p.centre.y,p.centre.z}},
        {"heading",p.headingDegrees},{"radius",p.obstacleRadius},{"length",p.wakeLength},
        {"width",p.wakeWidth},{"speed",p.speed},{"strength",p.strength},
        {"contact",p.contactWidth},{"enabled",p.enabled}});
    return nlohmann::json{{"version",1},{"regions",a}}.dump();
}
}
bool ValidateRiverFoamRegions(const std::vector<RiverFoamRegion>& regions,std::string& error)
{
    error.clear();
    if(regions.size()>MaximumRiverFoamRegions){error="At most16 foam regions per water surface";return false;}
    for(const auto& p:regions)
        if(!Range(p.centre.x,-100000,100000)||!Range(p.centre.y,-100000,100000)||
           !Range(p.centre.z,-100000,100000)||!Range(p.headingDegrees,-360,360)||
           !Range(p.obstacleRadius,.05f,20)||!Range(p.wakeLength,.1f,40)||
           !Range(p.wakeWidth,.02f,5)||!Range(p.speed,0,5)||
           !Range(p.strength,0,1)||!Range(p.contactWidth,.01f,1))
        {error="Foam parameters are nonfinite or outside supported bounds";return false;}
    return true;
}
bool ReadRiverFoamRegions(const wi::scene::Scene& s,wi::ecs::Entity e,
    std::vector<RiverFoamRegion>& result,std::string& error)
{
    result.clear();error.clear();
    if(!Water(s,e)){error="Foam requires a WATER material";return false;}
    const auto* m=s.metadatas.GetComponent(e);
    if(!m||!m->string_values.has(RiverFoamMetadataKey))return true;
    const auto text=m->string_values.get(RiverFoamMetadataKey);
    if(text.size()>16384){error="Foam metadata exceeds limit";return false;}
    try{
        auto j=nlohmann::json::parse(text);
        if(j.at("version")!=1||!j.at("regions").is_array()||
            j.at("regions").size()>MaximumRiverFoamRegions)throw std::runtime_error("Unsupported foam schema");
        std::vector<RiverFoamRegion> parsed;
        for(const auto& a:j.at("regions")){
            RiverFoamRegion p;const auto& c=a.at("centre");
            if(!c.is_array()||c.size()!=3)throw std::runtime_error("Invalid centre");
            p.centre={c[0].get<float>(),c[1].get<float>(),c[2].get<float>()};
            p.headingDegrees=a.at("heading").get<float>();p.obstacleRadius=a.at("radius").get<float>();
            p.wakeLength=a.at("length").get<float>();p.wakeWidth=a.at("width").get<float>();
            p.speed=a.at("speed").get<float>();p.strength=a.at("strength").get<float>();
            p.contactWidth=a.at("contact").get<float>();p.enabled=a.at("enabled").get<bool>();
            parsed.push_back(p);
        }
        if(!ValidateRiverFoamRegions(parsed,error))return false;
        result=std::move(parsed);return true;
    }catch(const std::exception& ex){error=ex.what();return false;}
}
SetRiverFoamRegionsCommand::SetRiverFoamRegionsCommand(wi::scene::Scene& s,
    wi::ecs::Entity e,std::vector<RiverFoamRegion> p):scene_(&s),water_(e),regions_(std::move(p)){}
bool SetRiverFoamRegionsCommand::Execute()
{
    std::string error;
    if(!Water(*scene_,water_)||!ValidateRiverFoamRegions(regions_,error))return false;
    auto* m=scene_->metadatas.GetComponent(water_);
    if(!captured_){
        std::vector<RiverFoamRegion> old;
        if(!ReadRiverFoamRegions(*scene_,water_,old,error))return false;
        after_=Encode(regions_);hadMetadata_=m!=nullptr;
        hadKey_=m&&m->string_values.has(RiverFoamMetadataKey);
        if(hadKey_)before_=m->string_values.get(RiverFoamMetadataKey);
        captured_=true;
    }else if((m&&m->string_values.has(RiverFoamMetadataKey))!=hadKey_||
        (hadKey_&&m->string_values.get(RiverFoamMetadataKey)!=before_))return false;
    if(!m)m=&scene_->metadatas.Create(water_);
    m->string_values.set(RiverFoamMetadataKey,after_);return true;
}
void SetRiverFoamRegionsCommand::Undo()
{
    auto* m=scene_->metadatas.GetComponent(water_);
    if(!captured_||!m||m->string_values.get(RiverFoamMetadataKey)!=after_)return;
    if(hadKey_)m->string_values.set(RiverFoamMetadataKey,before_);
    else m->string_values.erase(RiverFoamMetadataKey);
    if(!hadMetadata_&&m->string_values.size()==0&&m->int_values.size()==0&&
        m->float_values.size()==0&&m->bool_values.size()==0)scene_->metadatas.Remove(water_);
}
std::vector<XMFLOAT4> PackRiverFoamRegions(const std::vector<RiverFoamRegion>& regions)
{
    std::string error;if(!ValidateRiverFoamRegions(regions,error))return {};
    std::vector<XMFLOAT4> packed;
    for(const auto& p:regions){
        const float a=DirectX::XMConvertToRadians(p.headingDegrees);
        packed.push_back({p.centre.x,p.centre.y,p.centre.z,p.obstacleRadius});
        packed.push_back({std::sin(a),std::cos(a),p.wakeLength,p.wakeWidth});
        packed.push_back({p.speed,p.enabled?p.strength:0,p.contactWidth,0});
        packed.push_back({0,0,0,0});
    }
    return packed;
}
}
