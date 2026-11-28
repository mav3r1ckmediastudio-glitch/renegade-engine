// Included in Preview. Offsets are relative to the mounted support socket.
    using GripCurve = std::map<float,float>;
    struct GripFit {
        bool bindOnPreview=true;
        XMFLOAT3 offset{};
        XMFLOAT4 rotation{0,0,0,1};
        std::map<std::string,GripCurve> curves;
    } gripFit;
    Entity gripSocket=0;
    bool gripCalibrated=false;
    std::optional<XMFLOAT4X4> wristToBind;
    bool canBindVisibleGrip() const {
        return !armFit.enabled&&hasSupportGrip()&&armFit.chain[0].captured&&gripWeight(gripCurve(),posePhase())>=.99999f;
    }
    void previewHandFit(bool bindVisible){
        if(fullIKEnabled())return;
        pauseForFit();rememberFit();
        if((bindVisible||gripFit.bindOnPreview)&&canBindVisibleGrip()){
            // Snapshot the visible source wrist; resolve the socket after main-hand IK.
            wristToBind=arms.transforms.GetComponent(armFit.chain[0].wrist)->world;
        }
        setFullIK(true,false);
    }
    void bindVisibleGrip(){
        if(!wristToBind)return;
        if(hasSupportGrip()){
            const auto wrist=XMLoadFloat4x4(&*wristToBind);
            const auto frame=gripFrame();
            gripFit.offset=ArmFit::f3(XMVector3TransformCoord(wrist.r[3],XMMatrixInverse(nullptr,frame)));
            gripFit.rotation=ArmFit::f4(XMQuaternionNormalize(XMQuaternionMultiply(gripBasis(wrist),XMQuaternionInverse(gripBasis(frame)))));
            gripFit.bindOnPreview=false;
            status.SetText("Grip set from the current hand pose. Save setup to keep it.");
        }
        wristToBind.reset();
    }
    bool hasSupportGrip() const {return instance&&!editing&&(gripSocket||points[1].has_value());}

    XMMATRIX gripFrame() const {
        auto root=arms.transforms.GetComponent(instance);
        if(gripSocket)return XMLoadFloat4x4(&arms.transforms.GetComponent(gripSocket)->world);
        return XMMatrixTranslationFromVector(XMLoadFloat3(&*points[1]))*XMLoadFloat4x4(&root->world);
    }
    // Orthonormalise axes so a nonuniform weapon scale cannot shear the wrist.
    static XMVECTOR gripBasis(XMMATRIX frame){
        auto x=XMVector3Normalize(frame.r[0]);
        auto z=XMVector3Normalize(XMVector3Cross(x,frame.r[1]));
        return XMQuaternionNormalize(XMQuaternionRotationMatrix(XMMATRIX(x,XMVector3Cross(z,x),z,XMVectorSet(0,0,0,1))));
    }
    void findSupportGrip(){
        gripSocket=0;
        for(size_t i=0;i<arms.names.GetCount();++i){
            auto e=arms.names.GetEntity(i);
            if(arms.names[i].name=="socket_grip_support"&&arms.Entity_IsDescendant(e,instance)&&arms.transforms.Contains(e)){gripSocket=e;break;}
        }
        if(hasSupportGrip()&&armFit.valid(armFit.chain[0])){
            gripCalibrated=true;
            auto wrist=arms.transforms.GetComponent(armFit.chain[0].wrist);
            gripFit.rotation=ArmFit::f4(XMQuaternionMultiply(wrist->GetRotationV(),XMQuaternionInverse(gripBasis(gripFrame()))));
        }
    }
    GripCurve gripCurve() const {
        auto clip=currentClip();
        auto it=gripFit.curves.find(clip);
        if(it!=gripFit.curves.end())return it->second;
        if(clip!="Reload")return {{0,1},{1,1}};
        GripCurve curve{{0,1},{1,1}};
        curve[pickupPhase*.35f]=1;
        curve[pickupPhase]=0;
        curve[boltContact.release]=0;
        return curve;
    }
    static float gripWeight(const GripCurve& curve,float phase){
        auto to=curve.lower_bound(phase);
        if(to==curve.begin())return to->second;
        if(to==curve.end())return curve.rbegin()->second;
        auto from=std::prev(to);
        float t=(phase-from->first)/(to->first-from->first);
        return from->second+(to->second-from->second)*t;
    }
    float activeGripWeight() const {
        return armFit.enabled&&hasSupportGrip()?gripWeight(gripCurve(),posePhase()):0.f;
    }
    void gripEndpoints(XMVECTOR& freePos,XMVECTOR& freeRot,XMVECTOR& pinnedPos,XMVECTOR& pinnedRot) const {
        XMVECTOR scale;
        XMMatrixDecompose(&scale,&freeRot,&freePos,XMLoadFloat4x4(&armFit.chain[0].handWorld));
        const auto correction=armFit.settings(0);
        freePos+=XMLoadFloat3(&correction.position);
        freeRot=XMQuaternionMultiply(freeRot,XMLoadFloat4(&correction.rotation));
        auto frame=gripFrame();
        pinnedPos=XMVector3TransformCoord(XMLoadFloat3(&gripFit.offset),frame);
        pinnedRot=XMQuaternionMultiply(XMLoadFloat4(&gripFit.rotation),gripBasis(frame));
    }
    bool applySupportGrip(){
        float weight=activeGripWeight();
        if(weight<=0||!armFit.chain[0].captured)return false;
        XMVECTOR p,q,g,r;
        gripEndpoints(p,q,g,r);
        auto target=arms.transforms.GetComponent(armFit.chain[0].target);
        target->ClearTransform();
        target->Translate(XMVectorLerp(p,g,weight));
        target->Rotate(XMQuaternionSlerp(q,r,weight));
        target->UpdateTransform();
        return true;
    }
    void saveGripDrag(){
        float w=activeGripWeight();
        if(w<=.0001f){armFit.edited(0);return;}
        XMVECTOR p,q,g,r;
        gripEndpoints(p,q,g,r);
        auto target=arms.transforms.GetComponent(armFit.chain[0].target);
        auto frame=gripFrame();
        gripFit.bindOnPreview=false;
        gripFit.offset=ArmFit::f3(XMVector3TransformCoord(p+(target->GetPositionV()-p)/w,XMMatrixInverse(nullptr,frame)));
        auto rot=XMQuaternionSlerp(q,target->GetRotationV(),1.f/w);
        gripFit.rotation=ArmFit::f4(XMQuaternionNormalize(XMQuaternionMultiply(rot,XMQuaternionInverse(gripBasis(frame)))));
    }
    void solveSupport(bool keepDrag=false){
        bindVisibleGrip();
        bool force=keepDrag||applySupportGrip();
        if(!keepDrag&&boltContact.target(armFit,posePhase(),seatPhase,currentClip()=="Reload"))force=true;
        if(force&&!keepDrag){
            const auto& chain=armFit.chain[0];
            auto wrist=arms.transforms.GetComponent(chain.wrist);
            auto target=arms.transforms.GetComponent(chain.target);
            auto pose=armFit.settings(0);
            // An already satisfied wrist target needs no two-bone reconstruction.
            // Keep the original elbow bend and wrist twist exactly at the bind pose.
            if(wrist&&target&&ArmFit::length(XMLoadFloat3(&pose.pole))<1e-7f&&
               ArmFit::length(wrist->GetPositionV()-target->GetPositionV())<.000005f&&
               std::abs(XMVectorGetX(XMQuaternionDot(gripBasis(XMLoadFloat4x4(&wrist->world)),target->GetRotationV())))>.9999999f){
                armFit.chain[0].error=ArmFit::length(wrist->GetPositionV()-target->GetPositionV());
                return;
            }
        }
        armFit.solve(1,force?1:0);
    }
    json writeGripFit() const {
        const auto& p=gripFit.offset;const auto& q=gripFit.rotation;
        json data={{"bindOnPreview",gripFit.bindOnPreview},{"offset",{p.x,p.y,p.z}},{"rotation",{q.x,q.y,q.z,q.w}},{"curves",json::object()}};
        for(const auto& clip:gripFit.curves)for(auto k:clip.second)data["curves"][clip.first].push_back({k.first,k.second});
        return data;
    }
    void readGripFit(const json& j){
        GripFit fit;
        auto p=j.at("offset").get<std::array<float,3>>();
        auto q=j.at("rotation").get<std::array<float,4>>();
        for(float v:p)if(!std::isfinite(v))throw std::runtime_error("Invalid support grip offset.");
        for(float v:q)if(!std::isfinite(v))throw std::runtime_error("Invalid support grip rotation.");
        fit.offset={p[0],p[1],p[2]};
        fit.bindOnPreview=j.value("bindOnPreview",p[0]==0&&p[1]==0&&p[2]==0);
        auto rot=XMVectorSet(q[0],q[1],q[2],q[3]);
        if(XMVectorGetX(XMVector4LengthSq(rot))<1e-8f)throw std::runtime_error("Empty support grip rotation.");
        fit.rotation=ArmFit::f4(XMQuaternionNormalize(rot));
        for(auto& entry:j.at("curves").items())for(auto& value:entry.value()){
            auto k=value.get<std::array<float,2>>();
            if(!(k[0]>=0&&k[0]<=1&&k[1]>=0&&k[1]<=1))throw std::runtime_error("Invalid support grip key.");
            fit.curves[entry.key()][k[0]]=k[1];
        }
        gripFit=std::move(fit);gripCalibrated=true;
    }
