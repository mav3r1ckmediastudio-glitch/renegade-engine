void StudioRenderPath::DrawRetargetSource(wi::graphics::CommandList cmd) const
{
    const auto& src=*rtScene_;const auto& dst=modelImportPreview_->InspectionScene();
    const auto viewport=modelImportPreviewImage_.GetPos(),size=modelImportPreviewImage_.GetSize();
    const auto& sb=src.armatures[0].boneCollection;
    const auto& tb=dst.armatures[0].boneCollection;
    // Source reference positions were aligned in the candidate rig's world
    // space. Show them through the same mesh and import transforms as the skin.
    const auto& candidate=*modelImportCandidate_->PeekScene();
    const auto rawRig=candidate.armatures.GetEntity(0);
    const auto previewRig=dst.armatures.GetEntity(0);
    const auto rigToObject=ImportRigToObject(dst);
    const auto sourceToPreview=XMMatrixInverse(nullptr,XMLoadFloat4x4(&candidate.transforms.GetComponent(rawRig)->world))
        * XMLoadFloat4x4(&dst.transforms.GetComponent(previewRig)->world) * rigToObject;
    wi::primitive::AABB sourceBounds,targetBounds;
    for(auto e:sb){const auto p=src.transforms.GetComponent(e)->GetPosition();sourceBounds.AddPoint(XMVector3TransformCoord(XMLoadFloat3(&p),sourceToPreview));}
    for(auto e:tb){const auto p=dst.transforms.GetComponent(e)->GetPosition();targetBounds.AddPoint(XMVector3TransformCoord(XMLoadFloat3(&p),rigToObject));}
    const auto sc=sourceBounds.getCenter(),tc=targetBounds.getCenter();
    const float offset=size.x*0.27f;
    std::map<wi::ecs::Entity,XMFLOAT3> points;
    for(auto e:sb){
        const auto p=src.transforms.GetComponent(e)->GetPosition();
        const auto oriented=XMVector3TransformCoord(XMLoadFloat3(&p),sourceToPreview);
        XMFLOAT3 world;XMStoreFloat3(&world,(oriented-XMLoadFloat3(&sc))+XMLoadFloat3(&tc));
        auto projected=ProjectImportPoint(world);projected.x-=offset;points.emplace(e,projected);
    }
    const float dpi=GetPhysicalWidth()/std::max(1.f,GetLogicalWidth());
    const wi::graphics::Rect clip={long(viewport.x*dpi),long(viewport.y*dpi),long((viewport.x+size.x)*dpi),long((viewport.y+size.y)*dpi)};
    auto* device=wi::graphics::GetDevice();device->BindScissorRects(1,&clip,cmd);
    const int selected=rtSource_.GetSelected();
    for(size_t i=0;i<sb.size();++i){
        const auto a=points.at(sb[i]);
        if(!std::isfinite(a.x)||!std::isfinite(a.y)||a.z<0||a.z>1)continue;
        const auto color=int(i)==selected?wi::Color::White():rtPairs_[i]>=0?wi::Color(255,163,71):wi::Color(220,100,117);
        auto* h=src.hierarchy.GetComponent(sb[i]);
        if(h && points.count(h->parentID)){
            const auto b=points.at(h->parentID);const float dx=b.x-a.x,dy=b.y-a.y;
            if(b.z>=0 && b.z<=1 && std::isfinite(dx) && std::isfinite(dy)){
                wi::image::Params p;p.pos={(a.x+b.x)/2,(a.y+b.y)/2,0};p.siz={std::hypot(dx,dy),2};
                p.pivot={0.5f,0.5f};p.rotation=std::atan2(dy,dx);p.color=color;wi::image::Draw(nullptr,p,cmd);
            }
        }
        const float r=int(i)==selected?5.f:2.f;
        wi::image::Draw(nullptr,wi::image::Params(a.x-r,a.y-r,2*r,2*r,color),cmd);
    }
    wi::font::Params legend(int(viewport.x+12),int(viewport.y+10),16);
    legend.color=wi::Color(255,163,71);legend.shadowColor=wi::Color::Black();
    wi::font::Draw("SOURCE",legend,cmd);
    legend.posX+=85;legend.color=wi::Color(98,217,231);wi::font::Draw("TARGET",legend,cmd);
    const wi::graphics::Rect full={0,0,long(GetPhysicalWidth()),long(GetPhysicalHeight())};device->BindScissorRects(1,&full,cmd);
}
