#include "renegade/bridge/RiverFlowFieldService.h"
#include "renegade/bridge/SpecialistComponentService.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace renegade::bridge;
using namespace DirectX;
namespace
{
    int assertions=0;
    void Check(bool ok,const std::string& message)
    {
        ++assertions; if (!ok) throw std::runtime_error(message);
    }
    void Near(float a,float b,float epsilon,const char* message)
    {
        Check(std::isfinite(a) && std::abs(a-b)<=epsilon,message);
    }
    wi::ecs::Entity Fixture(wi::scene::Scene& scene,
        const std::vector<XMFLOAT3>& points,float halfWidth=.2f,int subdivisions=24)
    {
        const auto root=scene.Entity_CreateTransform("CPU Flow River");
        scene.splines.Create(root);
        scene.materials.Create(root).shaderType=wi::scene::MaterialComponent::SHADERTYPE_WATER;
        for (const auto& p:points) {
            const auto node=scene.Entity_CreateTransform("Control");
            auto* t=scene.transforms.GetComponent(node);
            t->translation_local=p; t->SetDirty(); t->UpdateTransform();
            scene.Component_Attach(node,root);
            scene.splines.GetComponent(root)->spline_node_entities.push_back(node);
        }
        auto* spline=scene.splines.GetComponent(root);
        spline->width=halfWidth; spline->mesh_generation_subdivision=subdivisions;
        spline->SetDrawAligned(true);
        return root;
    }
    std::shared_ptr<const RiverFlowField> Build(const wi::scene::Scene& scene,
        wi::ecs::Entity river,RiverFlowSettings settings={},
        const std::vector<float>& multipliers={})
    {
        std::shared_ptr<const RiverFlowField> result;
        std::string error;
        Check(BuildRiverFlowField(scene,river,settings,result,error,multipliers),
            "Build unexpectedly refused: "+error);
        Check(bool(result) && error.empty(),"Success output/error contract");
        return result;
    }
    void Refuse(const wi::scene::Scene& scene,wi::ecs::Entity river,
        std::shared_ptr<const RiverFlowField> existing, RiverFlowSettings settings={},
        const std::vector<float>& multipliers={})
    {
        const auto before=existing;
        std::string error="old";
        Check(!BuildRiverFlowField(scene,river,settings,existing,error,multipliers),
            "Expected malformed/unsupported input refusal");
        Check(existing==before && !error.empty(),"Refusal changed immutable output or omitted reason");
    }
    void Invariants(const RiverFlowField& field,float step)
    {
        const auto& v=field.Samples();
        Check(v.size()>=2 && v.size()<=8192,"Sample cap");
        for (std::size_t i=0;i<v.size();++i) {
            const auto& s=v[i];
            Near(std::hypot(s.worldXZUnitTangent.x,s.worldXZUnitTangent.y),1,2e-5f,"Unit tangent");
            Near(s.worldXZUnitTangent.x*s.worldXZUnitRight.x+
                s.worldXZUnitTangent.y*s.worldXZUnitRight.y,0,1e-5f,"Perpendicular right");
            Check(std::isfinite(s.grade) && s.widthMeters>0 && s.speedMetersPerSecond>=0,"Finite sample");
            if (i) {
                const auto& a=v[i-1];
                Check(s.arcLengthMeters>a.arcLengthMeters && s.nativeParameter>a.nativeParameter,
                    "Monotonic horizontal arc and native parameters");
                Check(std::hypot(s.worldPosition.x-a.worldPosition.x,
                    s.worldPosition.z-a.worldPosition.z)<=step+1e-4f,"Adaptive horizontal step");
                Check(a.worldXZUnitTangent.x*s.worldXZUnitTangent.x+
                    a.worldXZUnitTangent.y*s.worldXZUnitTangent.y>.95f,"Tangent continuity");
            }
        }
    }
}
int main()
{
    try {
        Check(wi::graphics::GetDevice()==nullptr,"Unit suite must be CPU-only");
        wi::scene::Scene straight;
        const auto river=Fixture(straight,{{0,5,0},{20,5,0}},2);
        const auto original=*straight.transforms.GetComponent(river);
        const auto material=*straight.materials.GetComponent(river);
        auto field=Build(straight,river);
        Invariants(*field,.5f);
        Near(field->LengthMeters(),20,1e-5f,"Straight horizontal length");
        auto q=field->Evaluate({10,1});
        Check(q.valid && !q.ambiguous,"Straight inside query");
        Near(q.directionXZ.x,1,1e-5f,"Straight +X");
        Near(q.directionXZ.y,0,1e-5f,"Straight Z zero");
        Near(q.speedMetersPerSecond,1,1e-5f,"Flat flow stays positive");
        Near(q.grade,0,1e-5f,"Flat grade");
        Near(q.lateralFraction,.5f,1e-5f,"World +Z is right of +X");
        Near(q.alongMeters,10,1e-5f,"Along metres");
        Near(q.distanceToBankMeters,1,1e-5f,"Native bank distance");
        Near(q.surfaceHeight,5,1e-5f,"Height");
        Check(!field->Evaluate({-0.01f,0}).valid && !field->Evaluate({20.01f,0}).valid,
            "No phantom flow beyond native end caps");
        Check(field->Evaluate({0,0}).valid && field->Evaluate({20,0}).valid,"Endpoints included");
        Near(field->Evaluate({0,0}).distanceToBankMeters,0,1e-5f,"End cap distance");
        Check(!field->Evaluate({10,2.01f}).valid,"Outside native banks invalid");
        Check(!field->Evaluate({NAN,0}).valid,"Nonfinite query invalid");
        RiverFlowSettings reverse; reverse.downstreamReversed=true;
        auto reversed=Build(straight,river,reverse);
        q=reversed->Evaluate({3,1});
        Near(q.directionXZ.x,-1,1e-5f,"Reversal");
        Near(q.alongMeters,17,1e-4f,"Reversed along");
        Near(q.lateralFraction,-.5f,1e-5f,"Reversed lateral side");
        Check(field->DeterministicHash()!=reversed->DeterministicHash(),"Reverse changes hash");
        Check(field->DeterministicHash()==Build(straight,river)->DeterministicHash(),"Repeat hash");
        std::cout<<"STRAIGHT_FLAT_REVERSE_BANKS_CAPS_HASH=PASS\n";

        for (int side:{-1,1}) {
            wi::scene::Scene curve;
            const auto id=Fixture(curve,{{0,4,0},{10,4,0},{20,4,float(side)*10},
                {20,4,float(side)*25}},.15f);
            auto bent=Build(curve,id);
            Invariants(*bent,.5f);
            const auto& samples=bent->Samples();
            Check(samples.back().worldXZUnitTangent.y*side>.99f,"90 degree turn follows native curve");
            for (const auto& row:bent->NativeRibbon()) {
                const auto query=bent->Evaluate({row.centre.x,row.centre.z});
                Check(query.valid,"Native row centre must be inside native ribbon");
            }
            const auto shared=SampleSplineWorldPoints(curve,id,bent->NativeRibbon().size());
            for (std::size_t i=0;i<shared.size();++i) {
                const auto& p=bent->NativeRibbon()[i].centre;
                Near(p.x,shared[i].x,1e-5f,"Existing shared sampler X parity");
                Near(p.z,shared[i].z,1e-5f,"Existing shared sampler Z parity");
            }
        }
        wi::scene::Scene crossingAngle;
        auto angle=Fixture(crossingAngle,{{0,0,0},{-10,0,.4f},{-20,0,0},{-30,0,-.4f}},.1f);
        auto angleField=Build(crossingAngle,angle); Invariants(*angleField,.5f);
        for (const auto& sample:angleField->Samples())
            Check(sample.worldXZUnitTangent.x<-.98f,"+/-180 crossing must not flip");
        std::cout<<"LEFT_RIGHT_90DEG_ANGLE_WRAP_NATIVE_EVALUATOR=PASS\n";

        wi::scene::Scene grade;
        auto slope=Fixture(grade,{{0,10,0},{10,8,0},{20,8,0},{30,4,0}},.1f);
        auto graded=Build(grade,slope,{}, {1,2,2,.5f});
        auto backward=Build(grade,slope,reverse,{1,2,2,.5f});
        Check(graded->Samples().front().grade>0 && graded->Samples().back().grade>0,"Signed downhill grade");
        Near(graded->Samples().front().grade,-backward->Samples().front().grade,1e-6f,"Grade reversal");
        Near(graded->Samples().front().speedMetersPerSecond,1,1e-5f,"Profile start");
        Near(graded->Samples().back().speedMetersPerSecond,.5f,1e-5f,"Profile end");
        Near(graded->Samples().back().arcLengthMeters,30,1e-4f,"Arc ignores elevation");
        auto zeroSettings=RiverFlowSettings{};zeroSettings.baseSpeedMetersPerSecond=0;
        Near(Build(grade,slope,zeroSettings)->Evaluate({5,0}).speedMetersPerSecond,0,0,"Zero authored speed");
        std::cout<<"GRADE_PROFILE_ZERO_SPEED_HORIZONTAL_ARC=PASS\n";

        wi::scene::Scene width;
        const auto variable=Fixture(width,{{0,0,0},{20,0,0}},1);
        const auto last=width.splines.GetComponent(variable)->spline_node_entities.back();
        width.transforms.GetComponent(last)->scale_local.x=2;
        auto wide=Build(width,variable);
        Near(wide->Samples().front().widthMeters,2,1e-5f,"Full start width");
        Near(wide->Samples().back().widthMeters,4,1e-5f,"Full end width");
        Near(wide->Evaluate({10,1.5f}).lateralFraction,1,1e-4f,"Interpolated native bank");
        Check(!wide->Evaluate({10,1.51f}).valid,"Varying-width bank exterior");
        wi::scene::Scene tiny;
        const auto tinyID=Fixture(tiny,{{0,0,0},{.002f,0,0}},.0001f,2);
        RiverFlowSettings tinySettings;tinySettings.sampleStepMeters=.001f;
        Near(Build(tiny,tinyID,tinySettings)->LengthMeters(),.002f,1e-6f,"Very short finite river");
        wi::scene::Scene overshoot;
        const auto overshootID=Fixture(overshoot,{{0,0,0},{10,0,0},{20,0,0},{30,0,0}});
        const auto overshootNodes=overshoot.splines.GetComponent(overshootID)->spline_node_entities;
        overshoot.transforms.GetComponent(overshootNodes[1])->scale_local.x=.01f;
        overshoot.transforms.GetComponent(overshootNodes[2])->scale_local.x=.01f;
        Refuse(overshoot,overshootID,field); // Positive controls, negative Catmull width between them.
        wi::scene::Scene hairpin;
        const auto uturn=Fixture(hairpin,{{0,0,0},{10,0,0},{20,0,10},{10,0,20},{0,0,20}},.1f);
        const auto ufield=Build(hairpin,uturn);
        Invariants(*ufield,.5f);
        Check(ufield->Samples().front().worldXZUnitTangent.x>.99f &&
            ufield->Samples().back().worldXZUnitTangent.x<-.99f,"Genuine broad hairpin follows authored order");
        wi::scene::Scene inverted;
        const auto upsideDown=Fixture(inverted,{{0,0,0},{10,0,0}});
        XMStoreFloat4(&inverted.transforms.GetComponent(upsideDown)->rotation_local,
            XMQuaternionRotationRollPitchYaw(XM_PI,0,0));
        Refuse(inverted,upsideDown,field);
        std::cout<<"WIDTH_INTERPOLATION_OVERSHOOT_VERY_SHORT_BROAD_HAIRPIN_INVERTED=PASS\n";

        wi::scene::Scene world;
        const auto transformed=Fixture(world,{{0,0,0},{8,0,0},{18,0,6},{28,0,8}},.1f);
        const auto parent=world.Entity_CreateTransform("Parent");
        world.Component_Attach(transformed,parent);
        auto* root=world.transforms.GetComponent(transformed);
        root->scale_local={2,1,.7f};root->SetDirty();
        auto* ancestor=world.transforms.GetComponent(parent);
        ancestor->translation_local={30,7,-20};
        XMStoreFloat4(&ancestor->rotation_local,XMQuaternionRotationRollPitchYaw(0,.7f,0));
        ancestor->SetDirty(); // Intentionally do not publish world caches.
        const auto beforeRoot=*root, beforeAncestor=*ancestor;
        const auto beforeSpline=*world.splines.GetComponent(transformed);
        auto transformedField=Build(world,transformed);
        const auto nativePoints=SampleSplineWorldPoints(world,transformed,transformedField->NativeRibbon().size());
        for (std::size_t i=0;i<nativePoints.size();++i) {
            Near(transformedField->NativeRibbon()[i].centre.x,nativePoints[i].x,2e-5f,"Nonuniform local-first X");
            Near(transformedField->NativeRibbon()[i].centre.z,nativePoints[i].z,2e-5f,"Parent rotation Z");
        }
        Check(std::memcmp(&root->world,&beforeRoot.world,sizeof(root->world))==0 &&
            root->IsDirty()==beforeRoot.IsDirty() &&
            std::memcmp(&ancestor->world,&beforeAncestor.world,sizeof(ancestor->world))==0 &&
            world.splines.GetComponent(transformed)->spline_node_transforms.size()==beforeSpline.spline_node_transforms.size(),
            "CPU build must not publish transforms or native spline caches");
        std::cout<<"PARENT_NONUNIFORM_STALE_CACHE_READONLY=PASS\n";

        RiverFlowSettings bad;
        bad.maxSamples=1; Refuse(straight,river,field,bad);
        bad={};bad.maxSamples=8193;Refuse(straight,river,field,bad);
        bad={};bad.sampleStepMeters=NAN;Refuse(straight,river,field,bad);
        bad={};bad.baseSpeedMetersPerSecond=INFINITY;Refuse(straight,river,field,bad);
        bad={};bad.baseSpeedMetersPerSecond=-1;Refuse(straight,river,field,bad);
        bad={};bad.maxSamples=24;Refuse(straight,river,field,bad);
        Refuse(straight,river,field,{}, {1});
        Refuse(straight,river,field,{}, {1,NAN});
        Refuse(straight,river,field,{}, {1,-1});
        wi::scene::Scene invalid;
        auto dup=Fixture(invalid,{{0,0,0},{0,1,0}});
        Refuse(invalid,dup,field);
        const auto end=invalid.splines.GetComponent(dup)->spline_node_entities.back();
        auto* node=invalid.transforms.GetComponent(end);
        node->translation_local={NAN,0,0};Refuse(invalid,dup,field);
        node->translation_local={1,0,0}; node->scale_local.x=0;Refuse(invalid,dup,field);
        node->scale_local.x=1; node->rotation_local={0,0,0,0};Refuse(invalid,dup,field);
        node->rotation_local={0,0,0,1};
        invalid.splines.GetComponent(dup)->SetLooped(true);Refuse(invalid,dup,field);
        invalid.splines.GetComponent(dup)->SetLooped(false);
        invalid.splines.GetComponent(dup)->terrain_modifier_amount=1;Refuse(invalid,dup,field);
        invalid.splines.GetComponent(dup)->terrain_modifier_amount=0;
        invalid.hierarchy.GetComponent(end)->parentID=end;Refuse(invalid,dup,field);
        wi::scene::Scene cycle;
        const auto cyclic=Fixture(cycle,{{0,0,0},{1,0,0}});
        cycle.hierarchy.Create(cyclic).parentID=cyclic;
        Refuse(cycle,cyclic,field);
        wi::scene::Scene crossed;
        auto cross=Fixture(crossed,{{-10,0,-10},{10,0,10},{-10,0,10},{10,0,-10}},.05f);
        Refuse(crossed,cross,field);
        wi::scene::Scene sharp;
        auto sharpID=Fixture(sharp,{{0,0,0},{10,0,0},{.1f,0,.1f},{10,0,.2f}},2);
        Refuse(sharp,sharpID,field);
        wi::scene::Scene many;
        std::vector<XMFLOAT3> points;
        for (int i=0;i<513;++i) points.push_back({float(i)*2,0,0});
        auto manyID=Fixture(many,points,.1f,24);Refuse(many,manyID,field);
        std::cout<<"NAN_LIMIT_DUPLICATE_CYCLE_INTERSECTION_SHARP_EDIT_REJECTION=PASS\n";

        wi::scene::Scene longCurve;
        points.clear();
        for (int i=0;i<160;++i) points.push_back({float(i)*10,0,std::sin(i*.05f)*10});
        const auto longID=Fixture(longCurve,points,.1f,6);
        const auto started=std::chrono::steady_clock::now();
        RiverFlowSettings longSettings;longSettings.sampleStepMeters=2;
        auto longField=Build(longCurve,longID,longSettings);
        Invariants(*longField,2);
        Check(longField->Samples().size()<8192,"Long curve bounded sample memory");
        const auto milliseconds=std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-started).count();
        bad={};bad.sampleStepMeters=.001f;
        Refuse(longCurve,longID,longField,bad);
        Check(straight.transforms.GetComponent(river)->translation_local.y==original.translation_local.y &&
            straight.materials.GetComponent(river)->baseColor.x==material.baseColor.x &&
            straight.meshes.GetCount()==0 && straight.terrains.GetCount()==0 &&
            wi::graphics::GetDevice()==nullptr,"No rendering, terrain or material side effects");
        std::cout<<"LONG_CURVE=PASS samples="<<longField->Samples().size()<<" ms="<<milliseconds
            <<" sampleBytes="<<longField->Samples().size()*sizeof(RiverFlowSample)<<"\n";
        std::cout<<"SAMPLE_TABLE t,x,y,z,tx,tz,arc,width,speed,grade\n";
        const auto& table=angleField->Samples();
        for (std::size_t i=0;i<table.size();i+=std::max<std::size_t>(1,table.size()/6)) {
            const auto& s=table[i];
            std::cout<<std::setprecision(8)<<s.nativeParameter<<","<<s.worldPosition.x<<","<<s.worldPosition.y
                <<","<<s.worldPosition.z<<","<<s.worldXZUnitTangent.x<<","<<s.worldXZUnitTangent.y
                <<","<<s.arcLengthMeters<<","<<s.widthMeters<<","<<s.speedMetersPerSecond<<","<<s.grade<<"\n";
        }
        std::cout<<"RIVER_FLOW_FIELD_CPU=PASS assertions="<<assertions<<" hash="<<field->DeterministicHash()<<"\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n";return 1; }
}
