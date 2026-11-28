#pragma once
// Post-animation, two-bone arm fitting with optional timed correction keys.
struct ArmFit {
    struct Settings {
        XMFLOAT3 position{},pole{};
        XMFLOAT4 rotation={0,0,0,1};
    };
    struct Chain {
        Entity upper=0,lower=0,wrist=0,target=0,pole=0;
        TransformComponent base[3];
        XMFLOAT4X4 handWorld;
        XMFLOAT3 basePole{};
        float upperLength=0,lowerLength=0,error=0;
        bool captured=false;
    } chain[2];
    std::map<std::string,std::array<std::map<float,Settings>,2>> keys;
    float phase=0;
    bool keyEditing=false;
    static Settings blend(const Settings& a,const Settings& b,float t){
        Settings p;p.position=f3(XMVectorLerp(XMLoadFloat3(&a.position),XMLoadFloat3(&b.position),t));p.pole=f3(XMVectorLerp(XMLoadFloat3(&a.pole),XMLoadFloat3(&b.pole),t));p.rotation=f4(XMQuaternionSlerp(XMLoadFloat4(&a.rotation),XMLoadFloat4(&b.rotation),t));return p;
    }
    Settings settings(int side) const {
        Settings fallback;auto profile=profiles.find(clip);if(profile!=profiles.end())fallback=profile->second[side];
        auto found=keys.find(clip);if(found==keys.end()||found->second[side].empty())return fallback;
        auto& curve=found->second[side];auto next=curve.lower_bound(phase);
        if(next==curve.begin())return next->second;if(next==curve.end())return curve.rbegin()->second;
        auto previous=std::prev(next);float t=(phase-previous->first)/std::max(.000001f,next->first-previous->first);t=t*t*(3-2*t);return blend(previous->second,next->second,t);
    }
    void store(int side,const Settings& p){
        if(keyEditing||keys.count(clip)&&!keys[clip][side].empty()){
            auto& curve=keys[clip][side];if(curve.empty()){curve[0]=profiles[clip][side];curve[1]=profiles[clip][side];}
            curve[phase]=p;
        }else profiles[clip][side]=p;
    }
    void addKey(int side){auto p=settings(side);bool was=keyEditing;keyEditing=true;store(side,p);keyEditing=was;}
    Scene* scene=nullptr;
    bool enabled=true,visible=true;
    std::string clip="Idle";
    std::map<std::string,std::array<Settings,2>> profiles;
    static XMFLOAT3 f3(XMVECTOR v){XMFLOAT3 f;XMStoreFloat3(&f,v);return f;}
    static XMFLOAT4 f4(XMVECTOR v){XMFLOAT4 f;XMStoreFloat4(&f,v);return f;}
    static float length(XMVECTOR v){return XMVectorGetX(XMVector3Length(v));}
    static bool active(const Settings& s){
        return std::abs(s.position.x)+std::abs(s.position.y)+std::abs(s.position.z)+std::abs(s.pole.x)+std::abs(s.pole.y)+std::abs(s.pole.z)+
            std::abs(s.rotation.x)+std::abs(s.rotation.y)+std::abs(s.rotation.z)+std::abs(s.rotation.w-1)>1e-7f;
    }
    void init(Scene& s){
        scene=&s;
        for(int side=0;side<2;side++){
            auto suffix=side==0?"_l":"_r";auto& c=chain[side];
            c.upper=s.Entity_FindByName(std::string("upperarm")+suffix);c.lower=s.Entity_FindByName(std::string("lowerarm")+suffix);c.wrist=s.Entity_FindByName(std::string("hand")+suffix);
            c.target=wi::ecs::CreateEntity();s.transforms.Create(c.target);s.names.Create(c.target).name=side==0?"Left hand target":"Right hand target";
            c.pole=wi::ecs::CreateEntity();s.transforms.Create(c.pole);s.names.Create(c.pole).name=side==0?"Left elbow pole":"Right elbow pole";
        }
    }
    bool valid(const Chain& c) const {return scene&&scene->transforms.Contains(c.upper)&&scene->transforms.Contains(c.lower)&&scene->transforms.Contains(c.wrist);}
    Entity control(int index) const {return index<2?chain[index].target:chain[index-2].pole;}
    void restoreBase(){
        for(auto& c:chain)if(c.captured&&valid(c)){
            Entity e[]={c.upper,c.lower,c.wrist};
            for(int k=0;k<3;k++){auto t=scene->transforms.GetComponent(e[k]);*t=c.base[k];t->SetDirty();}
        }
    }
    void hierarchy(){
        wi::jobsystem::context ctx;scene->RunHierarchyUpdateSystem(ctx);wi::jobsystem::Wait(ctx);
    }
    void captureBase(){
        for(auto& c:chain)if(valid(c)){
            Entity e[]={c.upper,c.lower,c.wrist};
            for(int k=0;k<3;k++)c.base[k]=*scene->transforms.GetComponent(e[k]);
            c.handWorld=c.base[2].world;c.captured=true;
            auto a=c.base[0].GetPositionV(),b=c.base[1].GetPositionV(),w=c.base[2].GetPositionV();
            c.upperLength=length(b-a);c.lowerLength=length(w-b);
            auto d=XMVector3Normalize(w-a);auto bend=(b-a)-d*XMVector3Dot(b-a,d);
            if(length(bend)<1e-6f)bend=XMVector3Cross(d,XMVectorSet(0,0,1,0));
            if(length(bend)<1e-6f)bend=XMVectorSet(1,0,0,0);
            c.basePole=f3(b+XMVector3Normalize(bend)*.12f);
        }
    }
    void targets(int draggingControl=-1){

        for(int side=0;side<2;side++){
            auto& c=chain[side];if(!c.captured)continue;
            XMVECTOR sc,rot,pos;XMMatrixDecompose(&sc,&rot,&pos,XMLoadFloat4x4(&c.handWorld));
            auto p=settings(side);
            if(draggingControl!=side){
                auto t=scene->transforms.GetComponent(c.target);t->ClearTransform();
                t->Rotate(XMQuaternionNormalize(XMQuaternionMultiply(rot,XMLoadFloat4(&p.rotation))));
                t->Translate(pos+XMLoadFloat3(&p.position));t->UpdateTransform();
            }
            if(draggingControl!=side+2){
                auto t=scene->transforms.GetComponent(c.pole);t->ClearTransform();t->Translate(XMLoadFloat3(&c.basePole)+XMLoadFloat3(&p.pole));t->UpdateTransform();
            }
        }
    }
    void edited(int controlIndex){
        int side=controlIndex%2;auto& c=chain[side];if(!c.captured)return;
        auto p=settings(side);auto t=scene->transforms.GetComponent(control(controlIndex));
        if(controlIndex<2){
            XMVECTOR sc,rot,pos;XMMatrixDecompose(&sc,&rot,&pos,XMLoadFloat4x4(&c.handWorld));
            p.position=f3(t->GetPositionV()-pos);
            p.rotation=f4(XMQuaternionNormalize(XMQuaternionMultiply(XMQuaternionInverse(rot),t->GetRotationV())));
        }else p.pole=f3(t->GetPositionV()-XMLoadFloat3(&c.basePole));
        store(side,p);
    }
    static XMVECTOR between(XMVECTOR a,XMVECTOR b){
        a=XMVector3Normalize(a);b=XMVector3Normalize(b);
        float dot=std::clamp(XMVectorGetX(XMVector3Dot(a,b)),-1.f,1.f);
        if(dot>.999999f)return XMQuaternionIdentity();
        auto cross=XMVector3Cross(a,b);
        if(dot<-.999999f){cross=XMVector3Cross(a,XMVectorSet(1,0,0,0));if(length(cross)<1e-5f)cross=XMVector3Cross(a,XMVectorSet(0,1,0,0));return XMQuaternionRotationAxis(XMVector3Normalize(cross),XM_PI);}
        return XMQuaternionNormalize(XMVectorSetW(cross,1+dot));
    }
    void world(Entity e,XMMATRIX value){
        auto h=scene->hierarchy.GetComponent(e);XMMATRIX parent=XMMatrixIdentity();
        if(h)if(auto t=scene->transforms.GetComponent(h->parentID))parent=XMLoadFloat4x4(&t->world);
        auto t=scene->transforms.GetComponent(e);t->ClearTransform();t->MatrixTransform(value*XMMatrixInverse(nullptr,parent));
        if(h)t->UpdateTransform_Parented(*scene->transforms.GetComponent(h->parentID));else t->UpdateTransform();
    }
    void turn(Entity e,XMVECTOR source,XMVECTOR destination){
        auto t=scene->transforms.GetComponent(e);auto p=t->GetPositionV();auto f=f3(p);
        auto value=XMLoadFloat4x4(&t->world)*XMMatrixTranslation(-f.x,-f.y,-f.z)*XMMatrixRotationQuaternion(between(source,destination))*XMMatrixTranslation(f.x,f.y,f.z);
        world(e,value);hierarchy();
    }
    void solve(uint32_t sideMask=3,uint32_t forceMask=0){
        if(!enabled)return;
        for(int side=0;side<2;side++){
            if(!(sideMask&(1u<<side)))continue;
            auto& c=chain[side];if(!c.captured||(!active(settings(side))&&!(forceMask&(1u<<side))))continue;
            auto root=scene->transforms.GetComponent(c.upper)->GetPositionV();
            auto elbow=scene->transforms.GetComponent(c.lower)->GetPositionV();
            auto wrist=scene->transforms.GetComponent(c.wrist)->GetPositionV();
            auto target=scene->transforms.GetComponent(c.target);
            auto desired=target->GetPositionV(),direction=desired-root;
            float requested=length(direction);
            if(requested<1e-6f)direction=wrist-root;
            direction=XMVector3Normalize(direction);
            float l1=c.upperLength,l2=c.lowerLength;
            if(l1<1e-5f||l2<1e-5f)continue;
            float distance=std::clamp(requested,std::abs(l1-l2)+1e-5f,l1+l2-1e-5f);
            auto goal=root+direction*distance;
            auto pole=scene->transforms.GetComponent(c.pole)->GetPositionV()-root;
            auto bend=pole-direction*XMVector3Dot(pole,direction);
            if(length(bend)<1e-6f)bend=(elbow-root)-direction*XMVector3Dot(elbow-root,direction);
            if(length(bend)<1e-6f)bend=XMVector3Cross(direction,XMVectorSet(0,0,1,0));
            if(length(bend)<1e-6f)bend=XMVector3Cross(direction,XMVectorSet(0,1,0,0));
            bend=XMVector3Normalize(bend);
            float along=(l1*l1-l2*l2+distance*distance)/(2*distance);
            float height=std::sqrt(std::max(0.f,l1*l1-along*along));
            auto newElbow=root+direction*along+bend*height;
            turn(c.upper,elbow-root,newElbow-root);
            elbow=scene->transforms.GetComponent(c.lower)->GetPositionV();
            wrist=scene->transforms.GetComponent(c.wrist)->GetPositionV();
            turn(c.lower,wrist-elbow,goal-elbow);
            auto hand=scene->transforms.GetComponent(c.wrist);XMVECTOR scale,rotation,position;
            XMMatrixDecompose(&scale,&rotation,&position,XMLoadFloat4x4(&hand->world));
            world(c.wrist,XMMatrixScalingFromVector(scale)*XMMatrixRotationQuaternion(target->GetRotationV())*XMMatrixTranslationFromVector(position));
            hierarchy();c.error=length(hand->GetPositionV()-desired);
        }
    }
    void reset(int index){
        if(index<0){profiles.erase(clip);return;}
        int side=index%2;auto p=settings(side);
        if(index<2){p.position={};p.rotation={0,0,0,1};}else p.pole={};store(side,p);
    }
    json write() const {
        json result={{"enabled",enabled},{"visible",visible},{"profiles",json::object()}};
        for(auto& entry:profiles){
            for(int side=0;side<2;side++){
                auto& p=entry.second[side];
                result["profiles"][entry.first][side]={{"position",{p.position.x,p.position.y,p.position.z}},{"rotation",{p.rotation.x,p.rotation.y,p.rotation.z,p.rotation.w}},{"pole",{p.pole.x,p.pole.y,p.pole.z}}};
            }
        }
        result["keys"]=json::object();
        for(auto& clipKeys:keys){result["keys"][clipKeys.first]=json::array({json::array(),json::array()});for(int side=0;side<2;side++)for(auto& frame:clipKeys.second[side]){auto& p=frame.second;result["keys"][clipKeys.first][side].push_back({{"phase",frame.first},{"position",{p.position.x,p.position.y,p.position.z}},{"rotation",{p.rotation.x,p.rotation.y,p.rotation.z,p.rotation.w}},{"pole",{p.pole.x,p.pole.y,p.pole.z}}});}}
        return result;
    }
    void read(const json& data){
        profiles.clear();keys.clear();enabled=data.value("enabled",true);visible=data.value("visible",true);
        if(data.contains("keys"))for(auto& entry:data.at("keys").items())for(int side=0;side<2;side++){
            if(entry.value().size()!=2)throw std::runtime_error("Invalid IK key hands.");auto& frames=entry.value().at(side);if(frames.is_null())continue;
            for(auto& v:frames){float at=v.at("phase");if(!std::isfinite(at)||at<0||at>1)throw std::runtime_error("Invalid IK key time.");Settings p;auto a=v.at("position").get<std::array<float,3>>(),b=v.at("pole").get<std::array<float,3>>();auto q=v.at("rotation").get<std::array<float,4>>();for(float x:a)if(!std::isfinite(x))throw std::runtime_error("Invalid IK key position.");for(float x:b)if(!std::isfinite(x))throw std::runtime_error("Invalid IK key pole.");for(float x:q)if(!std::isfinite(x))throw std::runtime_error("Invalid IK key rotation.");auto rot=XMVectorSet(q[0],q[1],q[2],q[3]);if(XMVectorGetX(XMVector4LengthSq(rot))<1e-8f)throw std::runtime_error("Invalid IK key quaternion.");p.position={a[0],a[1],a[2]};p.pole={b[0],b[1],b[2]};p.rotation=f4(XMQuaternionNormalize(rot));keys[entry.key()][side][at]=p;}
        }
        if(!data.contains("profiles"))return;
        for(auto& entry:data.at("profiles").items()){
            if(entry.value().size()!=2)throw std::runtime_error("Invalid arm IK profile.");
            for(int side=0;side<2;side++){
                auto& v=entry.value().at(side);auto& p=profiles[entry.key()][side];
                auto pos=v.at("position").get<std::array<float,3>>(),pole=v.at("pole").get<std::array<float,3>>();
                auto rot=v.at("rotation").get<std::array<float,4>>();
                for(float f:pos)if(!std::isfinite(f))throw std::runtime_error("Invalid IK position.");
                for(float f:pole)if(!std::isfinite(f))throw std::runtime_error("Invalid elbow pole.");
                for(float f:rot)if(!std::isfinite(f))throw std::runtime_error("Invalid IK rotation.");
                p.position={pos[0],pos[1],pos[2]};p.pole={pole[0],pole[1],pole[2]};
                auto q=XMVectorSet(rot[0],rot[1],rot[2],rot[3]);
                if(XMVectorGetX(XMVector4LengthSq(q))<1e-8f)throw std::runtime_error("Invalid IK quaternion.");
                p.rotation=f4(XMQuaternionNormalize(q));
            }
        }
    }
    // Refresh existing GPU slots after solving; never allocate a second skinning range.
    void refreshSkin(){
        hierarchy();
        for(size_t i=0;i<scene->armatures.GetCount();i++){
            auto& rig=scene->armatures[i];auto root=scene->transforms.GetComponent(scene->armatures.GetEntity(i));if(!root)continue;
            auto inverse=XMMatrixInverse(nullptr,XMLoadFloat4x4(&root->world));
            for(size_t b=0;b<rig.boneCollection.size();b++){
                auto bone=scene->transforms.GetComponent(rig.boneCollection[b]);if(!bone||b>=rig.boneData.size())continue;
                XMFLOAT4X4 value;XMStoreFloat4x4(&value,XMLoadFloat4x4(&rig.inverseBindMatrices[b])*XMLoadFloat4x4(&bone->world)*inverse);
                rig.boneData[b].Create(value);
                size_t offset=rig.gpuBoneOffset+b*sizeof(rig.boneData[b]);
                if(scene->skinningDataMapped&&offset+sizeof(rig.boneData[b])<=scene->skinningDataSize)
                    std::memcpy((uint8_t*)scene->skinningDataMapped+offset,&rig.boneData[b],sizeof(rig.boneData[b]));
            }
        }
        wi::jobsystem::context ctx;scene->RunObjectUpdateSystem(ctx);wi::jobsystem::Wait(ctx);
    }
};
