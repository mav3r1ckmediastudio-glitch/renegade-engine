static std::unordered_set<wi::ecs::Entity> TerrainChunks(const wi::scene::Scene& scene){
    std::unordered_set<wi::ecs::Entity> ids;
    for(size_t i=0;i<scene.terrains.GetCount();++i)for(const auto& entry:scene.terrains[i].chunks)ids.insert(entry.second.entity);
    return ids;
}
static std::vector<river_v2_classification::Surface> GatherBounds(const wi::scene::Scene& scene,wi::ecs::Entity river,
    const river_flow_detail::SourceBounds& bounds,std::vector<wi::ecs::Entity>* rocks=nullptr,bool includeSubmerged=false){
    auto terrain=TerrainChunks(scene);std::vector<river_v2_classification::Surface> result;
    const auto minX=bounds.minimum.x,minZ=bounds.minimum.z,maxX=bounds.maximum.x,maxZ=bounds.maximum.z;
    size_t inspected=0;
    for(size_t i=0;i<scene.objects.GetCount();++i){
        const auto entity=scene.objects.GetEntity(i);const auto& object=scene.objects[i];
        if(entity==river)continue;
        const auto* mesh=scene.meshes.GetComponent(object.meshID);
        if(!mesh||!river_flow_detail::IsPotentialSource(scene,entity,object,*mesh,&bounds,includeSubmerged))continue;
        river_v2_classification::Surface surface;surface.terrain=terrain.count(entity)||terrain.count(object.meshID);
        const auto world=scene.ComputeEntityMatrixRecursive(entity);
        uint32_t first=0,last=0;mesh->GetLODSubsetRange(0,first,last);
        for(uint32_t subset=first;subset<last;++subset){
            const auto& part=mesh->subsets[subset];
            const auto* material=scene.materials.GetComponent(part.materialID);
            if(material&&material->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER)continue;
            Require(size_t(part.indexOffset)+part.indexCount<=mesh->indices.size(),"Invalid native subset range");
            for(uint32_t j=part.indexOffset;j+2<part.indexOffset+part.indexCount;j+=3){
                Require(++inspected<=2000000,"G4 native triangle scan limit");
                river_v2_classification::Triangle triangle;
                XMFLOAT3* output[]={&triangle.a,&triangle.b,&triangle.c};
                for(int k=0;k<3;++k){
                    auto index=mesh->indices[j+k];Require(index<mesh->vertex_positions.size(),"Invalid native mesh index");
                    XMStoreFloat3(output[k],XMVector3TransformCoord(XMLoadFloat3(&mesh->vertex_positions[index]),world));
                    Require(river_v2_classification::Finite(*output[k]),"Nonfinite native geometry");
                }
                const auto& a=triangle.a;const auto& b=triangle.b;const auto& c=triangle.c;
                if(std::max({a.x,b.x,c.x})<minX-.5f||std::min({a.x,b.x,c.x})>maxX+.5f||
                    std::max({a.z,b.z,c.z})<minZ-.5f||std::min({a.z,b.z,c.z})>maxZ+.5f)continue;
                surface.triangles.push_back(triangle);
            }
        }
        if(!surface.triangles.empty()){
            if(!surface.terrain&&rocks)rocks->push_back(entity);
            result.push_back(std::move(surface));
        }
    }
    return result;
}
static std::vector<river_v2_classification::Surface> Gather(const wi::scene::Scene& scene,wi::ecs::Entity river,
    const RiverFlowField& flow,std::vector<wi::ecs::Entity>* rocks=nullptr){return GatherBounds(scene,river,river_flow_detail::CaptureSourceBounds(flow),rocks);}
