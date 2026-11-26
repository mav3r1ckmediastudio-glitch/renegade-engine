#pragma once
// Contact-specific finger poses and hand-local fingertip goals. Bone lengths never change.
struct FingerFit {
    struct Pose {
        bool enabled=false,ik=false;
        std::array<XMFLOAT4,3> anchor={XMFLOAT4{0,0,0,1},XMFLOAT4{0,0,0,1},XMFLOAT4{0,0,0,1}};
        std::array<float,3> curl={0,0,0};
        float spread=0,opposition=0;
        XMFLOAT3 target{};
    };
    struct Finger {
        Entity joints[3]={},hand=0,target=0,jointTarget[3]={};
        TransformComponent visible[3]; XMFLOAT3 visibleTip{}; XMFLOAT4X4 visibleHand{}; bool hasVisible=false;
        TransformComponent base[3];
        XMFLOAT3 endLocal{};
        bool valid=false,captured=false;
        float error=0;
    } fingers[2][5];
    using Hands=std::array<std::array<Pose,5>,2>;
    std::map<std::string,Hands> poses;
    Scene* scene=nullptr;
    bool enabled=true;
    static const char* name(int index){static const char* names[]={"Thumb","Index","Middle","Ring","Little"};return names[index];}
    static const char* key(int index){static const char* names[]={"Foregrip","Magazine","Charging handle"};return names[index];}
    void init(Scene& s){
        scene=&s;
        const char* names[]={"thumb","index","middle","ring","pinky"};
        for(int side=0;side<2;side++)for(int n=0;n<5;n++){
            auto& f=fingers[side][n];f.hand=s.Entity_FindByName(side?"hand_r":"hand_l");f.valid=f.hand!=0;
            for(int j=0;j<3;j++){
                f.joints[j]=s.Entity_FindByName(std::string(names[n])+"_0"+std::to_string(j+1)+(side?"_r":"_l"));
                f.valid=f.valid&&s.transforms.Contains(f.joints[j]);
                auto h=s.hierarchy.GetComponent(f.joints[j]);f.valid=f.valid&&h&&h->parentID==(j?f.joints[j-1]:f.hand);
            }
            if(f.valid){
                auto d=s.transforms.GetComponent(f.joints[2])->translation_local;
                // Export has no terminal tip bones: infer the pad from the last phalanx length.
                float length=ArmFit::length(XMLoadFloat3(&d))*.7f;
                f.endLocal={d.x<0?-length:length,0,0};
            }
            for(int j=0;j<3;j++){f.jointTarget[j]=wi::ecs::CreateEntity();s.transforms.Create(f.jointTarget[j]);}
            f.target=wi::ecs::CreateEntity();s.transforms.Create(f.target);s.names.Create(f.target).name=std::string(side?"Right ":"Left ")+name(n)+" fingertip target";
        }
    }
    void restoreBase(){for(auto& hand:fingers)for(auto& f:hand)if(f.captured&&f.valid)for(int j=0;j<3;j++){auto t=scene->transforms.GetComponent(f.joints[j]);*t=f.base[j];t->SetDirty();}}
    void captureBase(){for(auto& hand:fingers)for(auto& f:hand)if(f.valid){for(int j=0;j<3;j++)f.base[j]=*scene->transforms.GetComponent(f.joints[j]);f.captured=true;}}
    void rebuild(Finger& f){
        for(int j=0;j<3;j++)scene->transforms.GetComponent(f.joints[j])->UpdateTransform_Parented(*scene->transforms.GetComponent(j?f.joints[j-1]:f.hand));
    }
    XMVECTOR tip(const Finger& f) const {return XMVector3TransformCoord(XMLoadFloat3(&f.endLocal),XMLoadFloat4x4(&scene->transforms.GetComponent(f.joints[2])->world));}
    XMMATRIX handWorld(const Finger& f) const {return XMLoadFloat4x4(&scene->transforms.GetComponent(f.hand)->world);}
    void cacheVisible(){for(auto& hand:fingers)for(auto& f:hand)if(f.valid){
        for(int j=0;j<3;j++)f.visible[j]=*scene->transforms.GetComponent(f.joints[j]);
        f.visibleTip=ArmFit::f3(tip(f));XMStoreFloat4x4(&f.visibleHand,handWorld(f));f.hasVisible=true;
    }}
    Pose& ensure(const std::string& grip,int side,int index){
        auto& p=poses[grip][side][index];auto& f=fingers[side][index];
        if(!p.enabled&&f.valid){
            for(int j=0;j<3;j++)p.anchor[j]=f.hasVisible?f.visible[j].rotation_local:f.captured?f.base[j].rotation_local:scene->transforms.GetComponent(f.joints[j])->rotation_local;
            p.enabled=true;
        }
        return p;
    }
    void enableIK(const std::string& grip,int side,int index){
        auto& p=ensure(grip,side,index);auto& f=fingers[side][index];if(!f.valid)return;
        p.target=ArmFit::f3(XMVector3TransformCoord(f.hasVisible?XMLoadFloat3(&f.visibleTip):tip(f),XMMatrixInverse(nullptr,f.hasVisible?XMLoadFloat4x4(&f.visibleHand):handWorld(f))));p.ik=true;
    }
    XMVECTOR goal(const Pose& p,const Finger& f) const {return XMVector3TransformCoord(XMLoadFloat3(&p.target),handWorld(f));}
    void syncTargets(const std::string& grip,int dragging=-1){
        for(int side=0;side<2;side++)for(int n=0;n<5;n++){
            auto& f=fingers[side][n];if(!f.valid)continue;
            auto it=poses.find(grip);const Pose* p=it==poses.end()?nullptr:&it->second[side][n];
            if(side*5+n!=dragging){auto t=scene->transforms.GetComponent(f.target);t->ClearTransform();t->Translate(p&&p->enabled&&p->ik?goal(*p,f):tip(f));t->UpdateTransform();}
            for(int j=0;j<3;j++)if(10+(side*5+n)*3+j!=dragging){
                auto t=scene->transforms.GetComponent(f.jointTarget[j]);t->ClearTransform();t->MatrixTransform(XMLoadFloat4x4(&scene->transforms.GetComponent(f.joints[j])->world));t->UpdateTransform();
            }
        }
    }
    void edited(const std::string& grip,int side,int n){
        auto& f=fingers[side][n];auto& p=ensure(grip,side,n);p.ik=true;
        p.target=ArmFit::f3(XMVector3TransformCoord(scene->transforms.GetComponent(f.target)->GetPositionV(),XMMatrixInverse(nullptr,handWorld(f))));
    }
    void prepareVisiblePose(const std::string& grip,int side,int n){
        auto& f=fingers[side][n];auto& p=ensure(grip,side,n);
        for(int j=0;j<3;j++)p.anchor[j]=f.hasVisible?f.visible[j].rotation_local:scene->transforms.GetComponent(f.joints[j])->rotation_local;
        p.curl={0,0,0};p.spread=p.opposition=0;p.ik=false;
    }
    void prepareJoint(const std::string& grip,int side,int n){
        auto& f=fingers[side][n];auto& p=ensure(grip,side,n);
        for(int j=0;j<3;j++)p.anchor[j]=scene->transforms.GetComponent(f.joints[j])->rotation_local;
        p.curl={0,0,0};p.spread=p.opposition=0;p.ik=false;
    }
    void editJoint(const std::string& grip,int side,int n,int joint){
        auto& f=fingers[side][n];auto& p=ensure(grip,side,n);p.ik=false;
        auto parent=scene->transforms.GetComponent(joint?f.joints[joint-1]:f.hand);
        auto proxy=scene->transforms.GetComponent(f.jointTarget[joint]);
        XMVECTOR sc,q,pos;XMMatrixDecompose(&sc,&q,&pos,XMLoadFloat4x4(&proxy->world)*XMMatrixInverse(nullptr,XMLoadFloat4x4(&parent->world)));
        p.anchor[joint]=ArmFit::f4(XMQuaternionNormalize(q));
    }
    XMVECTOR posedRotation(const Pose& p,int joint,int finger) const {
        auto delta=XMMatrixRotationZ(XMConvertToRadians(p.curl[joint]));
        if(joint==0){delta=delta*XMMatrixRotationY(XMConvertToRadians(p.spread));if(finger==0)delta=delta*XMMatrixRotationX(XMConvertToRadians(p.opposition));}
        return XMQuaternionNormalize(XMQuaternionRotationMatrix(delta*XMMatrixRotationQuaternion(XMLoadFloat4(&p.anchor[joint]))));
    }
    void solve(Finger& f,XMVECTOR requested){
        rebuild(f);
        auto root=scene->transforms.GetComponent(f.joints[0])->GetPositionV();
        float reach=0;
        for(int j=0;j<3;j++){
            auto a=scene->transforms.GetComponent(f.joints[j])->GetPositionV();
            auto b=j<2?scene->transforms.GetComponent(f.joints[j+1])->GetPositionV():tip(f);reach+=ArmFit::length(b-a);
        }
        auto goal=requested,offset=goal-root;
        if(ArmFit::length(offset)>reach)goal=root+XMVector3Normalize(offset)*reach;
        XMFLOAT4 reference[3];for(int j=0;j<3;j++)reference[j]=scene->transforms.GetComponent(f.joints[j])->rotation_local;
        for(int iteration=0;iteration<40;iteration++){
            if(ArmFit::length(tip(f)-goal)<.00008f)break;
            for(int j=2;j>=0;j--){
                auto t=scene->transforms.GetComponent(f.joints[j]);auto origin=t->GetPositionV(),from=tip(f)-origin,to=goal-origin;
                if(ArmFit::length(from)<1e-7f||ArmFit::length(to)<1e-7f)continue;
                auto delta=ArmFit::between(from,to);
                float angle=2*acosf(std::clamp(std::abs(XMVectorGetW(delta)),0.f,1.f));
                if(angle>.22f)delta=XMQuaternionSlerp(XMQuaternionIdentity(),delta,.22f/angle);
                auto world=XMLoadFloat4x4(&t->world);world.r[3]=XMVectorSet(0,0,0,1);
                world=world*XMMatrixRotationQuaternion(delta);world.r[3]=XMVectorSetW(origin,1);
                auto parent=scene->transforms.GetComponent(j?f.joints[j-1]:f.hand);
                XMVECTOR scale,rotation,position;XMMatrixDecompose(&scale,&rotation,&position,world*XMMatrixInverse(nullptr,XMLoadFloat4x4(&parent->world)));
                auto referenceRotation=XMLoadFloat4(&reference[j]);
                float bend=2*acosf(std::clamp(std::abs(XMVectorGetX(XMQuaternionDot(referenceRotation,rotation))),0.f,1.f));
                const float limit=XMConvertToRadians(j==0?110.f:120.f);
                if(bend>limit)rotation=XMQuaternionSlerp(referenceRotation,rotation,limit/bend);
                t->rotation_local=ArmFit::f4(XMQuaternionNormalize(rotation));t->SetDirty();rebuild(f);
            }
        }
        f.error=ArmFit::length(tip(f)-requested);
    }
    void applyPose(Finger& f,const Pose& p,int n,float weight){
        if(!p.enabled||weight<.00001f)return;
        for(int j=0;j<3;j++){
            auto t=scene->transforms.GetComponent(f.joints[j]);t->rotation_local=ArmFit::f4(XMQuaternionSlerp(XMLoadFloat4(&t->rotation_local),posedRotation(p,j,n),weight));t->SetDirty();
        }
        rebuild(f);
        if(p.ik){
            XMFLOAT4 before[3];for(int j=0;j<3;j++)before[j]=scene->transforms.GetComponent(f.joints[j])->rotation_local;
            solve(f,goal(p,f));
            for(int j=0;j<3;j++){auto t=scene->transforms.GetComponent(f.joints[j]);t->rotation_local=ArmFit::f4(XMQuaternionSlerp(XMLoadFloat4(&before[j]),XMLoadFloat4(&t->rotation_local),weight));t->SetDirty();}
            rebuild(f);
        }
    }
    void apply(float magazine,float bolt,const std::string& preview={},const std::map<int,std::string>& overrides={}){
        if(!scene)return;
        for(int side=0;side<2;side++)for(int n=0;n<5;n++){
            auto& f=fingers[side][n];if(!f.valid||!f.captured)continue;f.error=0;
            for(int j=0;j<3;j++){auto t=scene->transforms.GetComponent(f.joints[j]);*t=f.base[j];t->SetDirty();}
            rebuild(f);if(!enabled)continue;
            auto forced=overrides.find(side*5+n);
            auto context=forced!=overrides.end()?forced->second:preview;
            if(!context.empty()){auto it=poses.find(context);if(it!=poses.end())applyPose(f,it->second[side][n],n,1);continue;}
            auto fore=poses.find(key(0)),mag=poses.find(key(1)),handle=poses.find(key(2));
            if(fore!=poses.end())applyPose(f,fore->second[side][n],n,1);
            if(mag!=poses.end())applyPose(f,mag->second[side][n],n,magazine);
            if(handle!=poses.end())applyPose(f,handle->second[side][n],n,bolt);
        }
    }
    json write() const {
        json result={{"enabled",enabled},{"poses",json::object()}};
        for(auto& entry:poses)for(int side=0;side<2;side++)for(int n=0;n<5;n++){
            auto& p=entry.second[side][n];json anchors=json::array();for(auto& q:p.anchor)anchors.push_back({q.x,q.y,q.z,q.w});
            result["poses"][entry.first][side][n]={{"enabled",p.enabled},{"ik",p.ik},{"anchor",anchors},{"curl",p.curl},{"spread",p.spread},{"opposition",p.opposition},{"target",{p.target.x,p.target.y,p.target.z}}};
        }
        return result;
    }
    void read(const json& data){
        std::map<std::string,Hands> parsed;bool enabledValue=data.value("enabled",true);
        if(data.contains("poses"))for(auto& entry:data.at("poses").items()){
            if(entry.key()!=key(0)&&entry.key()!=key(1)&&entry.key()!=key(2))throw std::runtime_error("Unknown finger contact pose.");
            if(entry.value().size()!=2)throw std::runtime_error("Invalid finger hands.");
            for(int side=0;side<2;side++){
                if(entry.value().at(side).size()!=5)throw std::runtime_error("Invalid finger count.");
                for(int n=0;n<5;n++){
                    auto& v=entry.value().at(side).at(n);auto& p=parsed[entry.key()][side][n];p.enabled=v.value("enabled",false);p.ik=v.value("ik",false);
                    auto a=v.at("anchor").get<std::array<std::array<float,4>,3>>();
                    for(int j=0;j<3;j++){for(float x:a[j])if(!std::isfinite(x))throw std::runtime_error("Invalid finger rotation.");auto q=XMVectorSet(a[j][0],a[j][1],a[j][2],a[j][3]);if(XMVectorGetX(XMVector4LengthSq(q))<1e-8f)throw std::runtime_error("Invalid finger quaternion.");p.anchor[j]=ArmFit::f4(std::abs(XMVectorGetX(XMVector4LengthSq(q))-1)>1e-5f?XMQuaternionNormalize(q):q);}
                    p.curl=v.at("curl").get<std::array<float,3>>();for(float x:p.curl)if(!std::isfinite(x)||std::abs(x)>120)throw std::runtime_error("Invalid finger curl.");
                    p.spread=v.value("spread",0.f);p.opposition=v.value("opposition",0.f);
                    if(!std::isfinite(p.spread)||std::abs(p.spread)>60||!std::isfinite(p.opposition)||std::abs(p.opposition)>90)throw std::runtime_error("Invalid finger spread or thumb opposition.");
                    auto t=v.at("target").get<std::array<float,3>>();for(float x:t)if(!std::isfinite(x)||std::abs(x)>10000)throw std::runtime_error("Invalid fingertip target.");p.target={t[0],t[1],t[2]};
                }
            }
        }
        poses=std::move(parsed);enabled=enabledValue;
    }
};
