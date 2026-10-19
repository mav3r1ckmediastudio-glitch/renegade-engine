#pragma once
// Diagnostic only: read an existing owner-private crossbow project and test
// mount rotations. Do not edit stored assembly or gameplay code.
static bool NewCrossbowMountSweep(const fs::path& project,const fs::path& output){
    using namespace renegade::bridge;
    nlohmann::json j;{
        std::ifstream f(project/"crossbow-new-assets.json");
        if(!f || !(f>>j))return false;
    }
    const std::string id=j.at("projectId"),assembly=j.at("assemblyId");
    std::string error;
    FirstPersonAssemblyService service;
    FirstPersonAssemblySettings baseline;
    if(!service.ReadSettings(project.generic_u8string(),id,assembly,baseline,error)){
        std::cerr<<error<<"\n";return false;
    }
    const float q=0.7071067812f;
    const std::vector<std::pair<std::string,XMFLOAT4>> rotations{
        {"source",{0,0,0,1}},
        {"xPlus",{q,0,0,q}},
        {"xMinus",{-q,0,0,q}},
        {"yPlus",{0,q,0,q}},
        {"yMinus",{0,-q,0,q}},
        {"zPlus",{0,0,q,q}},
        {"zMinus",{0,0,-q,q}}};
    if(fs::exists(output/"source-orbit.png")){
        std::cerr<<"Use an empty crossbow mount sweep output\n";return false;
    }
    fs::create_directories(output);
    for(const auto& variant:rotations){
        auto s=baseline;
        s.weaponRotation=variant.second;
        wi::scene::Scene scene;
        if(!service.Prepare(project.generic_u8string(),id,s,scene,error)||
           !service.Pose(scene,"Idle",.25f,error)){
            std::cerr<<"CROSSBOW SWEEP "<<variant.first<<" "<<error<<"\n";return false;
        }
        scene.Entity_CreateLight("Preview-only working light",
          XMFLOAT3{0,3,-2},XMFLOAT3{1,1,1},20.f,25.f,wi::scene::LightComponent::POINT);
        scene.Update(1.f/60.f);
        if(!Capture(scene,output/(variant.first+"-orbit.png"))||
           !Capture(scene,output/(variant.first+"-fps.png"),true,true,true,.9f))
            return false;
        std::cout<<"CROSSBOW_SWEEP "<<variant.first<<" GPU_PASS\n";
    }
    return true;
}
