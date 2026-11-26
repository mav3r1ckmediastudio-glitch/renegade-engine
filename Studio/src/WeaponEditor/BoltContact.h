#pragma once
struct BoltContact {
    Scene* scene=nullptr;
    Entity bone=0,parent=0,socket=0;
    TransformComponent rest;
    XMFLOAT3 rail{};
    XMFLOAT4X4 handOffset={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float grab=.82f,release=.96f;
    bool captured=false,hasRail=false;
    void clear(){bone=parent=socket=0;captured=hasRail=false;grab=.82f;release=.96f;rail={};}
    void init(Scene& s,Entity instance,const std::map<std::string,Entity>& clips){
        clear();scene=&s;bone=s.Entity_FindByName("bolt");
        if(!bone||!s.Entity_IsDescendant(bone,instance)){bone=0;return;}
        auto h=s.hierarchy.GetComponent(bone);auto t=s.transforms.GetComponent(bone);
        if(!h||!t){bone=0;return;}parent=h->parentID;rest=*t;socket=s.Entity_FindByName("socket_bolt_grip");
        auto it=clips.find("Reload");if(it==clips.end())return;
        auto animation=s.animations.GetComponent(it->second);if(!animation)return;
        for(auto& channel:animation->channels){
            if(channel.target!=bone||channel.path!=AnimationComponent::AnimationChannel::Path::TRANSLATION||channel.samplerIndex<0)continue;
            auto& sampler=animation->samplers[channel.samplerIndex];
            auto source=sampler.scene?static_cast<const Scene*>(sampler.scene):&s;
            auto data=source->animation_datas.GetComponent(sampler.data);if(!data)data=&sampler.backwards_compatibility_data;
            size_t stride=sampler.mode==AnimationComponent::AnimationSampler::CUBICSPLINE?9:3,offset=stride==9?3:0;
            if(data->keyframe_data.size()<offset+3)continue;
            rest.translation_local={data->keyframe_data[offset],data->keyframe_data[offset+1],data->keyframe_data[offset+2]};
            float farthest=0;
            for(size_t k=offset;k+2<data->keyframe_data.size();k+=stride){
                XMFLOAT3 d={data->keyframe_data[k]-rest.translation_local.x,data->keyframe_data[k+1]-rest.translation_local.y,data->keyframe_data[k+2]-rest.translation_local.z};
                float distance=ArmFit::length(XMLoadFloat3(&d));if(distance>farthest){farthest=distance;rail=d;}
            }
            hasRail=farthest>1e-5f;break;
        }
    }
    static float smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
    float stroke(float phase) const {
        if(phase<grab)return 0;
        if(phase<release)return smooth((phase-grab)/std::max(.0001f,(release-grab)*.72f));
        return 1-smooth((phase-release)/.025f);
    }
    XMMATRIX worldAt(float amount) const {
        auto local=rest;
        local.translation_local.x+=rail.x*amount;local.translation_local.y+=rail.y*amount;local.translation_local.z+=rail.z*amount;
        auto p=scene->transforms.GetComponent(parent);
        return local.GetLocalMatrix()*(p?XMLoadFloat4x4(&p->world):XMMatrixIdentity());
    }
    void apply(float phase,bool reload){
        if(!bone||!hasRail||!reload)return;
        auto t=scene->transforms.GetComponent(bone);*t=rest;
        auto d=XMLoadFloat3(&rail)*stroke(phase);t->Translate(d);t->SetDirty();
    }
    bool active(float phase,float seat,bool reload) const {
        return bone&&captured&&reload&&phase>=std::max(seat,grab-.04f)&&phase<release+.04f;
    }
    void capture(XMMATRIX handWorld){
        if(!bone)return;
        handOffset=ArmFitMatrix(handWorld*XMMatrixInverse(nullptr,XMLoadFloat4x4(&scene->transforms.GetComponent(bone)->world)));captured=true;
    }
    static XMFLOAT4X4 ArmFitMatrix(XMMATRIX m){XMFLOAT4X4 result;XMStoreFloat4x4(&result,m);return result;}
    bool target(ArmFit& fit,float phase,float seat,bool reload){
        if(!active(phase,seat,reload))return false;
        auto desired=XMLoadFloat4x4(&handOffset)*(phase>release?worldAt(1):XMLoadFloat4x4(&scene->transforms.GetComponent(bone)->world));
        float weight=1;
        if(phase<grab)weight=smooth((phase-std::max(seat,grab-.04f))/std::max(.0001f,grab-std::max(seat,grab-.04f)));
        if(phase>release)weight=1-smooth((phase-release)/.04f);
        auto target=scene->transforms.GetComponent(fit.chain[0].target);
        XMVECTOR sc,rotation,position;XMMatrixDecompose(&sc,&rotation,&position,desired);
        XMVECTOR baseScale,fromRotation,fromPosition;XMMatrixDecompose(&baseScale,&fromRotation,&fromPosition,XMLoadFloat4x4(&fit.chain[0].handWorld));
        auto profile=fit.settings(0);fromPosition+=XMLoadFloat3(&profile.position);fromRotation=XMQuaternionMultiply(fromRotation,XMLoadFloat4(&profile.rotation));
        target->ClearTransform();target->Rotate(XMQuaternionSlerp(fromRotation,rotation,weight));target->Translate(XMVectorLerp(fromPosition,position,weight));target->UpdateTransform();
        return true;
    }
    void editTarget(ArmFit& fit){
        auto target=scene->transforms.GetComponent(fit.chain[0].target);auto hand=scene->transforms.GetComponent(fit.chain[0].wrist);
        XMVECTOR scale,rotation,position;XMMatrixDecompose(&scale,&rotation,&position,XMLoadFloat4x4(&hand->world));
        capture(XMMatrixScalingFromVector(scale)*XMMatrixRotationQuaternion(target->GetRotationV())*XMMatrixTranslationFromVector(target->GetPositionV()));
    }
    json write() const {
        json j={{"grab",grab},{"release",release},{"captured",captured}};
        if(captured){j["handOffset"]=json::array();for(int r=0;r<4;r++)for(int c=0;c<4;c++)j["handOffset"].push_back(handOffset.m[r][c]);}
        return j;
    }
    void read(const json& j){
        grab=j.value("grab",.82f);release=j.value("release",.96f);captured=j.value("captured",false);
        if(!std::isfinite(grab)||!std::isfinite(release)||grab<0||grab>=release||release>1)throw std::runtime_error("Invalid bolt contact interval.");
        if(captured){
            auto v=j.at("handOffset").get<std::array<float,16>>();for(float f:v)if(!std::isfinite(f))throw std::runtime_error("Invalid bolt grip.");
            for(int r=0;r<4;r++)for(int c=0;c<4;c++)handOffset.m[r][c]=v[r*4+c];
        }
    }
};
