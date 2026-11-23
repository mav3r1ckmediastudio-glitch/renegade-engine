#include "RetargetAvatarReference.inc"

// Animation packs often ship the neutral skeleton as a separate one-frame take.
static bool LoadCompanionRest(wi::scene::Scene& src, const fs::path& file)
{
    if (!fs::is_regular_file(file)) return false;
    wi::scene::Scene pose;
    bridge::ClearWickedModelImporterFailureDiagnostic();
    ImportModel_FBX(file.u8string(), pose);
    if (!bridge::ConsumeWickedModelImporterFailureDiagnostic().empty() || pose.animations.GetCount() != 1) return false;
    auto& clip = pose.animations[0];
    clip.Pause(); clip.amount = 1; clip.timer = clip.start;
    pose.Update(0);
    std::map<std::string,wi::ecs::Entity> names;
    for (size_t i=0; i<pose.transforms.GetCount(); ++i) {
        const auto e=pose.transforms.GetEntity(i);
        const auto* n=pose.names.GetComponent(e);
        if (n && !names.emplace(n->name,e).second) return false;
    }
    const auto parentName=[](const wi::scene::Scene& scene, wi::ecs::Entity joint) {
        const auto* h=scene.hierarchy.GetComponent(joint);
        if (h && scene.hierarchy.Contains(h->parentID)) {
            if (auto* n=scene.names.GetComponent(h->parentID)) return n->name;
        }
        return std::string{};
    };
    std::vector<std::pair<wi::ecs::Entity,wi::ecs::Entity>> copies;
    for (auto e:src.armatures[0].boneCollection) {
        const auto it=names.find(src.names.GetComponent(e)->name);
        if (it==names.end() || parentName(src,e)!=parentName(pose,it->second)) return false;
        copies.emplace_back(e,it->second);
    }
    for (auto pair:copies) {
        auto* t=src.transforms.GetComponent(pair.first);
        const auto* ref=pose.transforms.GetComponent(pair.second);
        t->rotation_local=ref->rotation_local; t->translation_local=ref->translation_local;
        t->scale_local=ref->scale_local; t->SetDirty();
    }
    return true;
}

static XMVECTOR RestPoint(const wi::scene::Scene& scene, wi::ecs::Entity e)
{
    return (scene.transforms.GetComponent(e)->GetLocalMatrix()*scene.ComputeParentMatrixRecursive(e)).r[3];
}

static bool RestBodyFrame(const wi::scene::Scene& scene, const bridge::HumanoidMappingState& map,
                          XMMATRIX& frame, float& height)
{
    using B=bridge::HumanoidBone;
    const auto bone=[&](B b){return map.bones[size_t(b)];};
    for(auto b:{B::Hips,B::Head,B::LeftUpperLeg,B::RightUpperLeg})
        if(!scene.transforms.Contains(bone(b))) return false;
    const auto origin=RestPoint(scene,bone(B::Hips));
    auto up=RestPoint(scene,bone(B::Head))-origin;
    auto side=RestPoint(scene,bone(B::LeftUpperLeg))-RestPoint(scene,bone(B::RightUpperLeg));
    height=XMVectorGetX(XMVector3Length(up));
    auto front=XMVector3Cross(side,up);
    if(!(height>0.00001f && XMVectorGetX(XMVector3Length(front))>0.000001f)) return false;
    up=XMVector3Normalize(up);front=XMVector3Normalize(front);
    frame=XMMATRIX(XMVector3Cross(up,front),up,front,XMVectorSetW(origin,1));
    return true;
}

static std::string AlignRestRigs(wi::scene::Scene& src,const wi::scene::Scene& dst,
        const bridge::HumanoidMappingState& sm,const bridge::HumanoidMappingState& dm,const std::vector<int>& pairs)
{
    XMMATRIX from,to;float a,b;
    if(!RestBodyFrame(src,sm,from,a) || !RestBodyFrame(dst,dm,to,b))
        return "Body axes need manual review.";
    const float ratio=b/a;
    const auto correction=XMMatrixInverse(nullptr,from)*XMMatrixScaling(ratio,ratio,ratio)*to;
    std::vector<wi::ecs::Entity> roots;
    for(size_t i=0;i<src.transforms.GetCount();++i){
        auto e=src.transforms.GetEntity(i);
        if(!src.hierarchy.Contains(e)) roots.push_back(e);
    }
    const auto wrapper=wi::ecs::CreateEntity();
    src.transforms.Create(wrapper).MatrixTransform(correction);
    for(auto e:roots) src.Component_Attach(e,wrapper,true);

    const auto& sb=src.armatures[0].boneCollection;
    const auto& tb=dst.armatures[0].boneCollection;
    std::vector<std::pair<size_t,size_t>> order;
    for(size_t i=0;i<sb.size();++i){
        size_t depth=0;auto e=sb[i];
        while(auto* h=src.hierarchy.GetComponent(e)){e=h->parentID;++depth;}
        order.emplace_back(depth,i);
    }
    std::sort(order.begin(),order.end());
    int adjusted=0;
    for(auto entry:order){
        const size_t i=entry.second;
        if(pairs[i]<0) continue;
        const auto bone=sb[i], target=tb[pairs[i]];
        // Single-child chains constrain swing. Branches and leaves retain authored twist.
        size_t child=sb.size();
        for(size_t j=0;j<sb.size();++j){
            if(pairs[j]<0)continue;
            auto* h=src.hierarchy.GetComponent(sb[j]);
            auto* k=dst.hierarchy.GetComponent(tb[pairs[j]]);
            if(!h || h->parentID!=bone || !k || k->parentID!=target) continue;
            const auto& n=src.names.GetComponent(sb[j])->name;
            if(n.find("twist")!=std::string::npos || n.find("ik_")==0)continue;
            if(child!=sb.size()){child=sb.size();break;}
            child=j;
        }
        if(child==sb.size())continue;
        auto inv=XMMatrixInverse(nullptr,src.ComputeParentMatrixRecursive(bone));
        auto u=XMVector3TransformNormal(RestPoint(src,sb[child])-RestPoint(src,bone),inv);
        auto v=XMVector3TransformNormal(RestPoint(dst,tb[pairs[child]])-RestPoint(dst,target),inv);
        if(XMVectorGetX(XMVector3LengthSq(u))<1e-10f || XMVectorGetX(XMVector3LengthSq(v))<1e-10f)continue;
        u=XMVector3Normalize(u);v=XMVector3Normalize(v);
        const float dot=std::clamp(XMVectorGetX(XMVector3Dot(u,v)),-1.f,1.f);
        if(dot>0.99999f || dot< -0.99999f)continue;
        const auto q=XMQuaternionRotationAxis(XMVector3Cross(u,v),std::acos(dot));
        auto* tr=src.transforms.GetComponent(bone);
        XMStoreFloat4(&tr->rotation_local,XMQuaternionNormalize(XMQuaternionMultiply(XMLoadFloat4(&tr->rotation_local),q)));
        tr->SetDirty();++adjusted;
    }
    src.Update(0);
    std::ostringstream status;
    status<<"Source to target scale "<<std::setprecision(4)<<ratio<<"x / "<<adjusted<<" joint swings adjusted.";
    return status.str();
}
