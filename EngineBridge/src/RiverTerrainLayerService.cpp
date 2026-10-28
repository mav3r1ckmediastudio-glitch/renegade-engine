#include "renegade/bridge/RiverWaterwayService.h"
#include "renegade/bridge/RiverTerrainLayerService.h"
#include "renegade/bridge/RiverSplineService.h"
#include "renegade/bridge/RiverCourseService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/SpecialistComponentService.h"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace renegade::bridge
{
    namespace
    {
        using Json = nlohmann::json;
        constexpr std::size_t MaximumChunks = 81;
        constexpr std::size_t MaximumPayload = 32 * 1024 * 1024;
        struct Chunk
        {
            int x, z;
            wi::ecs::Entity entity;
            std::vector<float> base, last;
            std::vector<float> matrix;
        };
        struct Layer { StableId river; float depth, bank; };
        struct Document { std::vector<Chunk> chunks; std::vector<Layer> layers; };
        void Check(bool value, const char* error)
        {
            if (!value) throw std::runtime_error(error);
        }
        std::vector<float> MatrixValues(const XMMATRIX& matrix)
        {
            XMFLOAT4X4 value;
            DirectX::XMStoreFloat4x4(&value, matrix);
            std::vector<float> values;
            values.reserve(16);
            for (const auto& row : value.m)
                for (const float element : row) values.push_back(element);
            return values;
        }
        wi::terrain::Terrain& ValidateTerrain(wi::scene::Scene& scene,
            wi::ecs::Entity entity)
        {
            auto* terrain = scene.terrains.GetComponent(entity);
            Check(terrain && terrain->scene == &scene && terrain->terrainEntity == entity,
                "Terrain is not bound to this active scene");
            Check(!terrain->IsCenterToCamEnabled() && !terrain->IsRemovalEnabled() &&
                terrain->generation >= 1 && terrain->chunks.size() <= MaximumChunks,
                "River layers require finite resident terrain of at most 81 chunks");
            terrain->Generation_Cancel();
            const auto side = std::size_t(TerrainChunkCountPerSide(terrain->generation));
            Check(terrain->chunks.size() == side * side,
                "Terrain generation is incomplete");
            const auto matrix = scene.ComputeEntityMatrixRecursive(entity);
            XMFLOAT3 up;
            DirectX::XMStoreFloat3(&up, DirectX::XMVector3TransformNormal(
                DirectX::XMVectorSet(0,1,0,0), matrix));
            Check(std::abs(up.x) < 0.00001f && std::abs(up.y - 1) < 0.00001f &&
                std::abs(up.z) < 0.00001f,
                "River layers require an unrotated unit vertical terrain axis");
            return *terrain;
        }
        std::string Payload(const wi::scene::Scene& scene, wi::ecs::Entity entity)
        {
            const auto* metadata = scene.metadatas.GetComponent(entity);
            if (!metadata || !metadata->string_values.has(RiverTerrainLayersMetadataKey))
                return {};
            return metadata->string_values.get(RiverTerrainLayersMetadataKey);
        }
        Document Read(wi::scene::Scene& scene, wi::ecs::Entity entity, bool initialize)
        {
            auto& terrain = ValidateTerrain(scene, entity);
            const auto payload = Payload(scene, entity);
            Check(!HasRiverTerrainLayers(scene,entity) || !payload.empty(),
                "River layer metadata is empty or malformed");
            Document doc;
            Json saved;
            if (!payload.empty())
            {
                Check(payload.size() <= MaximumPayload, "River layer document exceeds bound");
                saved = Json::parse(payload);
                Check(saved.at("version").get<int>() == 1, "Unsupported river layer schema");
                Check(saved.at("chunks").is_array() &&
                    saved.at("chunks").size() == terrain.chunks.size(),
                    "River baseline topology differs from terrain");
                Check(saved.at("layers").is_array() && saved.at("layers").size() <= 32,
                    "Invalid river layer count");
                std::unordered_set<std::string> ids;
                for (const auto& layer : saved.at("layers"))
                {
                    Layer value{layer.at("river").get<std::string>(),
                        layer.at("depth").get<float>(), layer.at("bank").get<float>()};
                    Check(IsValidStableId(value.river) && ids.insert(value.river).second &&
                        std::isfinite(value.depth) && value.depth >= .05f && value.depth <= 5 &&
                        std::isfinite(value.bank) && value.bank >= .5f && value.bank <= 20,
                        "Invalid or duplicate river layer settings");
                    doc.layers.push_back(std::move(value));
                }
            }
            else Check(initialize, "Terrain has no river baseline");
            std::unordered_set<std::uint64_t> seen;
            for (const auto& entry : terrain.chunks)
            {
                const auto* mesh = scene.meshes.GetComponent(entry.second.entity);
                Check(!entry.second.invalidated && mesh &&
                    mesh->vertex_positions.size() == wi::terrain::vertexCount,
                    "Terrain chunk is invalid or has unsupported topology");
                Chunk chunk{entry.first.x, entry.first.z, entry.second.entity, {}, {},
                    MatrixValues(scene.ComputeEntityMatrixRecursive(entry.second.entity))};
                if (!payload.empty())
                {
                    const Json* item = nullptr;
                    for (const auto& candidate : saved.at("chunks"))
                        if (candidate.at("x").get<int>() == chunk.x &&
                            candidate.at("z").get<int>() == chunk.z)
                        {
                            Check(!item, "Duplicate baseline chunk coordinate");
                            item = &candidate;
                        }
                    Check(item != nullptr && seen.insert(entry.first.raw).second,
                        "River baseline is missing a resident chunk");
                    chunk.base = item->at("base").get<std::vector<float>>();
                    chunk.last = item->at("last").get<std::vector<float>>();
                    const auto storedMatrix = item->at("matrix").get<std::vector<float>>();
                    Check(storedMatrix.size() == 16 && chunk.base.size() == wi::terrain::vertexCount &&
                        chunk.last.size() == chunk.base.size(), "Invalid baseline vertex/matrix count");
                    for (std::size_t i=0; i<16; ++i)
                        Check(std::isfinite(storedMatrix[i]) &&
                            std::abs(storedMatrix[i]-chunk.matrix[i]) < .00001f,
                            "Terrain transform changed outside the layer transaction");
                }
                else
                    for (const auto& p : mesh->vertex_positions)
                    { chunk.base.push_back(p.y); chunk.last.push_back(p.y); }
                for (std::size_t i=0; i<chunk.base.size(); ++i)
                    Check(std::isfinite(chunk.base[i]) && std::isfinite(chunk.last[i]) &&
                        chunk.base[i] >= terrain.bottomLevel && chunk.base[i] <= terrain.topLevel &&
                        mesh->vertex_positions[i].y == chunk.last[i],
                        "Terrain differs from last layer result; refusing to overwrite external edits");
                doc.chunks.push_back(std::move(chunk));
            }
            std::sort(doc.chunks.begin(), doc.chunks.end(), [](const Chunk& a,const Chunk& b) {
                return a.x < b.x || (a.x == b.x && a.z < b.z); });
            return doc;
        }
        struct Curve {
            std::vector<XMFLOAT3> points;
            std::vector<float> radii; // native half-width per sampled world point
            float depth, bank, radius, length;
            bool openEnds;
        };
        std::vector<Curve> Curves(wi::scene::Scene& scene, const Document& doc)
        {
            std::vector<Curve> curves;
            for (const auto& layer : doc.layers)
            {
                wi::ecs::Entity river = wi::ecs::INVALID_ENTITY;
                for (std::size_t i=0; i<scene.metadatas.GetCount(); ++i)
                {
                    const auto entity = scene.metadatas.GetEntity(i);
                    if (PersistentEntityId(scene,entity) != layer.river) continue;
                    Check(river == wi::ecs::INVALID_ENTITY, "Duplicate persistent river identity");
                    river = entity;
                }
                if (river == wi::ecs::INVALID_ENTITY) continue; // Deleted river: restore footprint.
                const auto* spline = scene.splines.GetComponent(river);
                const auto* water = scene.materials.GetComponent(river);
                Check(spline && water && water->shaderType ==
                    wi::scene::MaterialComponent::SHADERTYPE_WATER &&
                    spline->terrain_modifier_amount == 0 && std::isfinite(spline->width) &&
                    spline->width > 0 && spline->width <= 50,
                    "Linked entity is not a supported native water spline");
                const auto matrix = MatrixValues(scene.ComputeEntityMatrixRecursive(river));
                for (std::size_t i=0;i<12;++i)
                    Check(std::isfinite(matrix[i]) && std::abs(matrix[i] -
                        ((i==0 || i==5 || i==10) ? 1.0f : 0.0f)) < .00001f,
                        "Linked river roots must be unscaled and unrotated");
                const auto count = std::max<std::size_t>(2,
                    (spline->spline_node_entities.size() > 1 ? spline->spline_node_entities.size()-1 : 1) *
                    std::size_t(std::clamp(spline->mesh_generation_subdivision,1,24)));
                const bool elevated=RiverCourseHasElevation(scene,river);
                auto points = SampleSplineWorldPoints(scene,river,count);
                std::vector<float> radii;
                radii.reserve(points.size());
                wi::scene::SplineComponent widthCurve;
                for(const auto id:spline->spline_node_entities){
                    const auto* node=scene.transforms.GetComponent(id);
                    Check(node && std::isfinite(node->scale_local.x) &&
                        node->scale_local.x >= .125f && node->scale_local.x <= 4.f &&
                        std::abs(node->scale_local.y-1.f)<.00001f &&
                        std::abs(node->scale_local.z-1.f)<.00001f,
                        "River node Scale X must remain between 0.125 and 4; Y/Z cannot change");
                    auto pose=*node;pose.SetDirty();pose.UpdateTransform();
                    widthCurve.spline_node_transforms.push_back(pose);
                }
                widthCurve.PrecomputeSplineNodeDistances();
                for(std::size_t i=0;i<points.size();++i){
                    const float t=points.size()==1?0.f:float(i)/float(points.size()-1);
                    const auto evaluated=widthCurve.EvaluateSplineAt(t);
                    const float width=spline->width*DirectX::XMVectorGetX(
                        DirectX::XMVector3Length(evaluated.r[0]));
                    Check(std::isfinite(width) && width>=.25f && width<=50.f,
                        "River node width is outside the supported terrain channel range");
                    radii.push_back(width);
                }
                if(elevated){
                    RiverCourseSurface surface;std::string error;
                    if(!BuildRiverCourseSurface(scene,river,1,surface,error))throw std::runtime_error(error);
                    const auto& rows=surface.flow->NativeRibbon();
                    // Bounded 1cm-error polyline of ACTUAL native generated row centres.
                    std::vector<bool> keep(rows.size());keep.front()=keep.back()=true;
                    std::vector<std::pair<size_t,size_t>> spans{{0,rows.size()-1}};
                    while(!spans.empty()){auto [a,b]=spans.back();spans.pop_back();auto p=rows[a].centre,q=rows[b].centre;
                        float dx=q.x-p.x,dz=q.z-p.z,length=dx*dx+dz*dz,best=.01f;size_t split=a;
                        for(size_t k=a+1;k<b;++k){auto v=rows[k].centre;float t=length>1e-10f?std::clamp(((v.x-p.x)*dx+(v.z-p.z)*dz)/length,0.f,1.f):0;
                            float error=std::max(std::hypot(v.x-p.x-dx*t,v.z-p.z-dz*t),std::abs(v.y-p.y-(q.y-p.y)*t));
                            if(error>best){best=error;split=k;}}
                        if(split!=a){keep[split]=true;spans.push_back({a,split});spans.push_back({split,b});}
                    }
                    points.clear();radii.clear();
                    for(size_t k=0;k<rows.size();++k)if(keep[k]){
                        points.push_back(rows[k].centre);
                        const auto& row=rows[k];
                        const float width=std::hypot(
                            row.rightBank.x-row.leftBank.x,
                            row.rightBank.z-row.leftBank.z)*.5f;
                        Check(std::isfinite(width)&&width>=.25f&&width<=50.f,
                            "Elevated river width exceeds linked terrain support");
                        radii.push_back(width);
                    }
                }
                Check(!points.empty(), "Linked river has invalid control points");
                // Match every section of the NATIVE spline ribbon. Retaining
                // constant-width carving here would expose dry ground when the
                // creator scales an individual river control point.
                for(float& radius:radii)
                    radius-=std::min(.35f,radius*.15f);
                const float cutRadius=radii.front();
                const float effectiveBank=std::min(layer.bank,cutRadius);
            float length=0;
            for(std::size_t i=1;i<points.size();++i)
                length+=std::hypot(points[i].x-points[i-1].x,points[i].z-points[i-1].z);
            RiverWaterwayControls controls; std::string connectionError;
            Check(ReadRiverWaterwayControls(scene,river,controls,connectionError),
                "Cannot compose a riverbed with invalid connection metadata");
            curves.push_back({std::move(points),std::move(radii),
                layer.depth,effectiveBank,cutRadius,length,controls.connected});
            }
            return curves;
        }
        std::string Write(const Document& doc)
        {
            Json out; out["version"]=1; out["chunks"]=Json::array(); out["layers"]=Json::array();
            for (const auto& chunk : doc.chunks)
                out["chunks"].push_back({{"x",chunk.x},{"z",chunk.z},{"base",chunk.base},
                    {"last",chunk.last},{"matrix",chunk.matrix}});
            for (const auto& layer : doc.layers)
                out["layers"].push_back({{"river",layer.river},{"depth",layer.depth},{"bank",layer.bank}});
            auto text = out.dump();
            Check(text.size() <= MaximumPayload, "River baseline document exceeds size bound");
            return text;
        }

        // Validate against the complete proposed triangle grid BEFORE GPU,
        // collision, height textures or metadata are published.
        float ComposedHeight(wi::scene::Scene& scene,const Document& doc,float x,float z)
        {
            constexpr int side=wi::terrain::chunk_width;
            for(const auto& chunk:doc.chunks)
            {
                const auto* mesh=scene.meshes.GetComponent(chunk.entity);
                const auto world=scene.ComputeEntityMatrixRecursive(chunk.entity);
                XMFLOAT3 local;
                DirectX::XMStoreFloat3(&local,DirectX::XMVector3TransformCoord(
                    DirectX::XMVectorSet(x,0,z,1),DirectX::XMMatrixInverse(nullptr,world)));
                const auto& origin=mesh->vertex_positions[0];
                const float dx=mesh->vertex_positions[1].x-origin.x;
                const float dz=mesh->vertex_positions[side].z-origin.z;
                const float u=(local.x-origin.x)/dx,v=(local.z-origin.z)/dz;
                if(u<0 || v<0 || u>side-1 || v>side-1)continue;
                const int ix=std::min(int(u),side-2),iz=std::min(int(v),side-2);
                const float a=u-ix,b=v-iz;
                const auto index=std::size_t(ix+iz*side);
                const auto& h=chunk.last;
                const float y=a+b<=1
                    ? h[index]*(1-a-b)+h[index+1]*a+h[index+side]*b
                    : h[index+side]*(1-a)+h[index+1]*(1-b)+h[index+side+1]*(a+b-1);
                XMFLOAT3 p;
                DirectX::XMStoreFloat3(&p,DirectX::XMVector3TransformCoord(
                    DirectX::XMVectorSet(local.x,y,local.z,1),world));
                return p.y;
            }
            throw std::runtime_error("Proposed stream leaves the resident triangle grid");
        }
        void ValidateFittedChannel(wi::scene::Scene& scene,const Document& doc,
            wi::ecs::Entity river)
        {
            const auto* source=scene.splines.GetComponent(river);
            wi::scene::SplineComponent curve;
            for(auto node:source->spline_node_entities)
            {
                auto pose=*scene.transforms.GetComponent(node);
                pose.SetDirty();pose.UpdateTransform();curve.spline_node_transforms.push_back(pose);
            }
            curve.PrecomputeSplineNodeDistances();
            const auto world=scene.ComputeEntityMatrixRecursive(river);
            const auto count=(curve.spline_node_transforms.size()-1)*6;
            for(std::size_t i=0;i<count;++i)
            {
                const float t=float(i)/float(count-1);
                const auto m=curve.EvaluateSplineAt(t)*world;
                XMFLOAT3 p,r;
                DirectX::XMStoreFloat3(&p,m.r[3]);DirectX::XMStoreFloat3(&r,m.r[0]);
                for(int side : {-1,1})
                {
                    const float x=p.x+side*r.x*source->width,z=p.z+side*r.z*source->width;
                    Check(p.y-ComposedHeight(scene,doc,x,z)<=.08f,
                        "Terrain grid or overlapping channels expose this edge; widen or reroute the stream");
                }
                if(t>.125f && t<.875f)
                    Check(p.y-ComposedHeight(scene,doc,p.x,p.z)>.15f,
                        "Terrain resolution cannot form this channel; use a wider stream or deeper bed");
            }
        }

        bool Commit(wi::scene::Scene& scene, wi::ecs::Entity entity, Document doc,
            bool refreshPhysics, wi::ecs::Entity fittedRiver = wi::ecs::INVALID_ENTITY)
        {
            auto& terrain = ValidateTerrain(scene,entity);
            const auto curves = Curves(scene,doc);
            std::size_t segments=0;
            for(const auto& curve:curves) segments+=curve.points.size()-1;
            Check(segments * doc.chunks.size() * wi::terrain::vertexCount <= 32000000,
                "River composition exceeds the bounded interactive work budget");
            TerrainSculptState next;
            for (auto& chunk : doc.chunks)
            {
                const auto* mesh = scene.meshes.GetComponent(chunk.entity);
                const auto world = scene.ComputeEntityMatrixRecursive(chunk.entity);
                TerrainChunkHeights heights; heights.entity=chunk.entity;
                heights.heights.reserve(chunk.base.size());
                for (std::size_t i=0; i<chunk.base.size(); ++i)
                {
                    auto local=mesh->vertex_positions[i]; local.y=chunk.base[i];
                    XMFLOAT3 p;
                    DirectX::XMStoreFloat3(&p,DirectX::XMVector3TransformCoord(
                        DirectX::XMLoadFloat3(&local),world));
                    float result=chunk.base[i];
                    for (const auto& curve : curves)
                    {
                        float nearest=std::numeric_limits<float>::max(), waterY=0, along=0, arc=0;
                        float localRadius=curve.radius;
                        for (std::size_t j=1;j<curve.points.size();++j)
                        {
                            const auto& a=curve.points[j-1]; const auto& b=curve.points[j];
                            const float dx=b.x-a.x,dz=b.z-a.z,len=dx*dx+dz*dz;
                            if (len < .000001f) continue;
                            const float t=std::clamp(((p.x-a.x)*dx+(p.z-a.z)*dz)/len,0.0f,1.0f);
                            const float ex=p.x-a.x-dx*t,ez=p.z-a.z-dz*t,d=ex*ex+ez*ez;
                            const float segmentLength=std::sqrt(len);
                            if (d<nearest) {
                                nearest=d;waterY=a.y+(b.y-a.y)*t;along=arc+t*segmentLength;
                                localRadius=curve.radii[j-1]+(curve.radii[j]-curve.radii[j-1])*t;
                            }
                            arc+=segmentLength;
                        }
                        // Connected networks require wet open boundary caps. The ordinary
                        // radial channel cut continues through each terminal cap;
                        // never tell the transport solver to flow through solid terrain.
                        // Unconnected rivers retain their accepted concealed end taper.
                        const float distance=curve.openEnds ? std::sqrt(nearest) :
                            RiverBedEndDistance(nearest,along,curve.length,localRadius);
                        if (distance >= localRadius) continue;
                        const float bank=std::max(.01f,std::min(curve.bank,localRadius));
                        const float edge=std::clamp((localRadius-distance)/bank,0.0f,1.0f);
                        const float weight=edge*edge*(3-2*edge);
                        const float candidate=chunk.base[i]+weight*std::min(0.0f,waterY-curve.depth-p.y);
                        result=std::min(result,candidate);
                    }
                    if(!std::isfinite(result) || result < terrain.bottomLevel || result > terrain.topLevel){
                        throw std::runtime_error(std::string("Composed riverbed exceeds terrain height encoding range")+
                            " (local y="+std::to_string(result)+
                            ", base="+std::to_string(chunk.base[i])+
                            ", world="+std::to_string(p.x)+","+std::to_string(p.y)+","+std::to_string(p.z)+
                            ", river y="+std::to_string(curves.empty()?0.f:curves.front().points.front().y)+
                            ", terrain bottom="+std::to_string(terrain.bottomLevel)+")");
                    }
                    heights.heights.push_back(result);
                }
                chunk.last=heights.heights;
                next.chunks.push_back(std::move(heights));
            }
            if(fittedRiver!=wi::ecs::INVALID_ENTITY) ValidateFittedChannel(scene,doc,fittedRiver);
            next.riverLayerDocument = doc.layers.empty() ? std::string{} : Write(doc);
            auto previous=CaptureTerrainSculpt(scene,terrain);
            const bool metadataChanged=previous.riverLayerDocument != next.riverLayerDocument;
            const auto changed=RetainChangedTerrainSculpt(previous,next);
            if (!changed && !metadataChanged) return true;
            const auto rollback=CaptureTerrainSculpt(scene,terrain);
            try
            {
                if (ApplyTerrainSculpt(scene,terrain,next,refreshPhysics)) return true;
                ApplyTerrainSculpt(scene,terrain,rollback,refreshPhysics);
                return false;
            }
            catch (...)
            {
                ApplyTerrainSculpt(scene,terrain,rollback,refreshPhysics);
                throw;
            }
        }
        template<class Work> bool Run(std::string& error, Work work)
        {
            error.clear();
            try { return work(); }
            catch (const std::exception& e) {error=e.what();return false;}
        }
    }
    bool ReadRiverTerrainLayerSettings(
        const wi::scene::Scene& scene, wi::ecs::Entity terrainEntity,
        wi::ecs::Entity riverEntity, RiverTerrainLayerSettings& settings,
        std::string& error)
    {
        RiverTerrainLayerSettings candidate;
        const bool valid = Run(error,[&] {
            const auto* terrain = scene.terrains.GetComponent(terrainEntity);
            const auto* spline = scene.splines.GetComponent(riverEntity);
            const auto* water = scene.materials.GetComponent(riverEntity);
            Check(terrain && spline && water &&
                water->shaderType == wi::scene::MaterialComponent::SHADERTYPE_WATER,
                "Selected entity is not a river on this terrain");
            const auto id = PersistentEntityId(scene,riverEntity);
            Check(IsValidStableId(id), "River is missing its persistent identity");
            const auto payload = Payload(scene,terrainEntity);
            if (payload.empty())
            {
                Check(!HasRiverTerrainLayers(scene,terrainEntity),
                    "Riverbed metadata is present but empty");
                return true; // Not linked: expose stock settings without mutation.
            }
            Check(payload.size() <= MaximumPayload,
                "Riverbed metadata exceeds the safe size limit");
            const auto document = Json::parse(payload);
            Check(document.at("version").get<int>() == 1 &&
                document.at("chunks").is_array() &&
                document.at("chunks").size() <= MaximumChunks &&
                document.at("layers").is_array() &&
                document.at("layers").size() <= 32,
                "Riverbed metadata version or layer count is invalid");
            std::unordered_set<std::string> seen;
            for (const auto& item : document.at("layers"))
            {
                const auto layerId = item.at("river").get<std::string>();
                const auto depth = item.at("depth").get<float>();
                const auto bank = item.at("bank").get<float>();
                Check(IsValidStableId(layerId) && seen.insert(layerId).second &&
                    std::isfinite(depth) && depth >= .05f && depth <= 5 &&
                    std::isfinite(bank) && bank >= .5f && bank <= 20,
                    "Riverbed metadata contains duplicate or invalid settings");
                if (layerId == id)
                {
                    candidate.linked = true;
                    candidate.depthMeters = depth;
                    candidate.bankWidthMeters = bank;
                }
            }
            return true;
        });
        if (valid) settings = candidate;
        return valid;
    }

    bool SceneHasRiverTerrainLayers(const wi::scene::Scene& scene)
    {
        for(std::size_t i=0;i<scene.terrains.GetCount();++i)
            if(HasRiverTerrainLayers(scene,scene.terrains.GetEntity(i))) return true;
        return false;
    }
    bool HasRiverTerrainLayers(const wi::scene::Scene& scene,wi::ecs::Entity entity)
    {
        const auto* meta=scene.metadatas.GetComponent(entity);
        return meta && meta->string_values.has(RiverTerrainLayersMetadataKey);
    }
    bool SetRiverTerrainLayer(wi::scene::Scene& scene,wi::ecs::Entity terrain,
        wi::ecs::Entity river,float depth,float bank,bool enabled,std::string& error)
    {
        return Run(error,[&] {
            const auto id=PersistentEntityId(scene,river);
            Check(IsValidStableId(id),"Save/assign a persistent river identity before linking terrain");
            Check(std::isfinite(depth)&&depth>=.05f&&depth<=5 &&
                std::isfinite(bank)&&bank>=.5f&&bank<=20,"Invalid riverbed depth/bank width");
            auto doc=Read(scene,terrain,true);
            auto found=std::find_if(doc.layers.begin(),doc.layers.end(),[&](const Layer& l){return l.river==id;});
            if (enabled)
            {
                if(found==doc.layers.end()) {Check(doc.layers.size()<32,"Too many river layers");doc.layers.push_back({id,depth,bank});}
                else {found->depth=depth;found->bank=bank;}
            }
            else if(found!=doc.layers.end()) doc.layers.erase(found);
            else {error="River is not linked to this terrain";return false;}
            return Commit(scene,terrain,std::move(doc),true);
        });
    }
    bool RecomposeRiverTerrainLayers(wi::scene::Scene& scene,wi::ecs::Entity terrain,std::string& error)
    {return Run(error,[&]{return Commit(scene,terrain,Read(scene,terrain,false),true);});}
    bool ReadRiverTerrainBaseline(wi::scene::Scene& scene,wi::ecs::Entity terrain,
        TerrainSculptState& baseline,std::string& error)
    {
        return Run(error,[&]{
            const auto doc=Read(scene,terrain,false);
            TerrainSculptState result;
            for(const auto& chunk:doc.chunks) result.chunks.push_back({chunk.entity,chunk.base});
            baseline=std::move(result);return true;
        });
    }
    bool SetRiverTerrainBaseline(wi::scene::Scene& scene,wi::ecs::Entity terrain,
        const TerrainSculptState& baseline,std::string& error,bool refreshPhysics)
    {
        return Run(error,[&]{
            auto doc=Read(scene,terrain,false);
            Check(baseline.chunks.size()==doc.chunks.size(),"Incomplete baseline edit");
            for(auto& chunk:doc.chunks)
            {
                const TerrainChunkHeights* source=nullptr;
                for(const auto& item:baseline.chunks) if(item.entity==chunk.entity)
                {Check(!source,"Duplicate baseline edit chunk");source=&item;}
                Check(source && source->heights.size()==chunk.base.size(),"Baseline edit topology mismatch");
                chunk.base=source->heights;
                const auto& nativeTerrain = *scene.terrains.GetComponent(terrain);
                for(float y:chunk.base) Check(std::isfinite(y) &&
                    y >= nativeTerrain.bottomLevel && y <= nativeTerrain.topLevel,
                    "Baseline edit exceeds terrain encoding range");
            }
            return Commit(scene,terrain,std::move(doc),refreshPhysics);
        });
    }
    bool RiverTransformNeedsTerrainTransaction(const wi::scene::Scene& scene,wi::ecs::Entity entity)
    {
        if(entity==wi::ecs::INVALID_ENTITY)return false;
        auto affected=[&](wi::ecs::Entity target){return target==entity || scene.Entity_IsDescendant(target,entity);};
        for(size_t i=0;i<scene.splines.GetCount();++i){
            const auto root=scene.splines.GetEntity(i);const auto* water=scene.materials.GetComponent(root);
            if(!water || water->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER)continue;
            if(affected(root))return true;
            for(auto node:scene.splines[i].spline_node_entities)if(affected(node))return true;
        }
        for(size_t i=0;i<scene.terrains.GetCount();++i){
            const auto terrain=scene.terrains.GetEntity(i);
            if(HasRiverTerrainLayers(scene,terrain)&&affected(terrain))return true;
        }
        return false;
    }

    bool IsRiverSplineControlNode(const wi::scene::Scene& scene,wi::ecs::Entity entity)
    {
        for(size_t i=0;i<scene.splines.GetCount();++i)
        {
            const auto& spline=scene.splines[i];
            const auto* water=scene.materials.GetComponent(scene.splines.GetEntity(i));
            if(water && water->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER &&
                !spline.IsLooped() && !spline.IsFilled() &&
                std::find(spline.spline_node_entities.begin(),spline.spline_node_entities.end(),entity)
                    !=spline.spline_node_entities.end())return true;
        }
        return false;
    }

    RiverNodeMovePreview::RiverNodeMovePreview(wi::scene::Scene& scene,
        wi::ecs::Entity entity,const TransformState& before):scene_(&scene),selected_(entity)
    {
        for(size_t i=0;i<scene.splines.GetCount();++i)
        {
            const auto& spline=scene.splines[i];
            const auto* water=scene.materials.GetComponent(scene.splines.GetEntity(i));
            if(!water || water->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER ||
                spline.IsLooped() || spline.IsFilled() ||
                spline.spline_node_entities.size()>512)continue;
            const auto it=std::find(spline.spline_node_entities.begin(),
                spline.spline_node_entities.end(),entity);
            if(it==spline.spline_node_entities.end())continue;
            root_=scene.splines.GetEntity(i);
            selectedIndex_=size_t(it-spline.spline_node_entities.begin());
            width_=spline.width;
            break;
        }
        if(root_==wi::ecs::INVALID_ENTITY)
        {
            nodes_.push_back({entity,before,0});
            return;
        }
        const auto& spline=*scene.splines.GetComponent(root_);
        const auto world=scene.ComputeEntityMatrixRecursive(root_);
        float distance=0;XMFLOAT3 previous{};
        for(const auto node:spline.spline_node_entities)
        {
            const auto* transform=scene.transforms.GetComponent(node);
            if(!transform){nodes_.clear();return;}
            const auto state=node==entity?before:CaptureTransform(*transform);
            XMFLOAT3 p;
            DirectX::XMStoreFloat3(&p,DirectX::XMVector3TransformCoord(
                DirectX::XMLoadFloat3(&state.translation),world));
            if(!nodes_.empty())
            {
                const float x=p.x-previous.x,y=p.y-previous.y,z=p.z-previous.z;
                distance+=std::sqrt(x*x+y*y+z*z);
            }
            nodes_.push_back({node,state,distance});
            previous=p;
        }
    }

    bool RiverNodeMovePreview::Apply(const TransformState& after)
    {
        if(nodes_.empty() || selectedIndex_>=nodes_.size())return false;
        const auto& before=nodes_[selectedIndex_].before;
        const XMFLOAT3 delta{after.translation.x-before.translation.x,
            after.translation.y-before.translation.y,after.translation.z-before.translation.z};
        const float length=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
        if(!std::isfinite(length))return false;
        for(const auto& node:nodes_)
            if(!scene_->transforms.Contains(node.entity))return false;
        // Always evaluate from the drag-start snapshot, including when its
        // influence radius shrinks again after a larger preview displacement.
        Restore();
        // Spread dense fit-node displacement over a physically meaningful reach.
        // Compact quintic falloff has zero first/second derivatives at its edge.
        const float radius=std::clamp(std::max({6.f,width_*4.f,length*2.5f}),6.f,48.f);
        bool changed=false;
        for(size_t i=0;i<nodes_.size();++i)
        {
            const auto& node=nodes_[i];
            if(!scene_->transforms.Contains(node.entity))return false;
            auto target=node.before;
            if(i==selectedIndex_)target=after;
            else if(root_!=wi::ecs::INVALID_ENTITY)
            {
                const float d=std::abs(node.along-nodes_[selectedIndex_].along);
                float w=std::clamp(1.f-d/radius,0.f,1.f);
                w=w*w*w*(w*(w*6.f-15.f)+10.f);
                target.translation.x+=delta.x*w;
                target.translation.y+=delta.y*(i>selectedIndex_?1.f:0.f);
                target.translation.z+=delta.z*w;
            }
            // Use the existing transform publisher and native mesh invalidation.
            SetTransformCommand move(*scene_,node.entity,node.before,target);
            changed=move.Execute() || changed;
        }
        return changed;
    }

    void RiverNodeMovePreview::Restore()
    {
        for(const auto& node:nodes_)
            if(const auto* current=scene_->transforms.GetComponent(node.entity))
            {
                SetTransformCommand restore(*scene_,node.entity,CaptureTransform(*current),node.before);
                (void)restore.Execute();
            }
    }

    namespace
    {
        class SmoothRiverNodeTransformCommand final:public ICommand
        {
        public:
            SmoothRiverNodeTransformCommand(wi::scene::Scene& scene,wi::ecs::Entity entity,
                const TransformState& before,const TransformState& after)
                : move_(scene,entity,before),after_(after){}
            bool Execute() override{return move_.Apply(after_);}
            void Undo() override{move_.Restore();}
        private:
            RiverNodeMovePreview move_;
            TransformState after_;
        };
    }

    SetWatercourseTransformCommand::SetWatercourseTransformCommand(
        wi::scene::Scene& scene,wi::ecs::Entity entity,
        const TransformState& before,const TransformState& after)
        :WatercourseSceneCommand(scene,MakeRiverWaterwayEditCommand(scene,entity,
            std::make_unique<SmoothRiverNodeTransformCommand>(scene,entity,before,after))){}

    WatercourseSceneCommand::WatercourseSceneCommand(
        wi::scene::Scene& scene, std::unique_ptr<ICommand> command)
        : scene_(&scene), command_(std::move(command)) {}
    bool WatercourseSceneCommand::Execute()
    {
        std::string error;
        if (!captured_)
        {
            terrains_.clear();
            for (std::size_t i=0;i<scene_->terrains.GetCount();++i)
            {
                const auto id=scene_->terrains.GetEntity(i);
                if (!HasRiverTerrainLayers(*scene_,id)) continue;
                TerrainSculptState baseline;
                if (!ReadRiverTerrainBaseline(*scene_,id,baseline,error)) return false;
                terrains_.push_back({id,CaptureTerrainSculpt(*scene_,scene_->terrains[i]),{}});
            }
        }
        if (!command_ || !command_->Execute()) return false;
        for (auto& edit:terrains_)
        {
            auto* terrain=scene_->terrains.GetComponent(edit.entity);
            const bool success=terrain && (captured_
                ? ApplyTerrainSculpt(*scene_,*terrain,edit.after)
                : RecomposeRiverTerrainLayers(*scene_,edit.entity,error));
            if (!success)
            {
                command_->Undo();
                for (const auto& rollback:terrains_)
                    if (auto* previous=scene_->terrains.GetComponent(rollback.entity))
                        ApplyTerrainSculpt(*scene_,*previous,rollback.before);
                return false;
            }
            if (!captured_) edit.after=CaptureTerrainSculpt(*scene_,*terrain);
        }
        captured_=true;
        return true;
    }
    void WatercourseSceneCommand::Undo()
    {
        if (!captured_) return;
        command_->Undo();
        for (const auto& edit:terrains_)
            if (auto* terrain=scene_->terrains.GetComponent(edit.entity))
                ApplyTerrainSculpt(*scene_,*terrain,edit.before);
    }

    SetRiverTerrainLayerCommand::SetRiverTerrainLayerCommand(wi::scene::Scene& scene,
        wi::ecs::Entity terrain,wi::ecs::Entity river,float depth,float bank,bool enabled)
        :scene_(&scene),terrain_(terrain),river_(river),depth_(depth),bank_(bank),enabled_(enabled) {}
    bool SetRiverTerrainLayerCommand::Execute()
    {
        auto* terrain=scene_->terrains.GetComponent(terrain_);
        if(!terrain) return false;
        if(captured_) return ApplyTerrainSculpt(*scene_,*terrain,after_);
        before_=CaptureTerrainSculpt(*scene_,*terrain);
        if(!SetRiverTerrainLayer(*scene_,terrain_,river_,depth_,bank_,enabled_,error_)) return false;
        after_=CaptureTerrainSculpt(*scene_,*terrain);captured_=true;return true;
    }
    void SetRiverTerrainLayerCommand::Undo()
    {
        auto* terrain=scene_->terrains.GetComponent(terrain_);
        if(terrain && captured_) ApplyTerrainSculpt(*scene_,*terrain,before_);
    }

    bool PlanMountainStream(wi::scene::Scene& scene, wi::ecs::Entity terrainID,
        wi::ecs::Entity river, MountainStreamFit& result, std::string& error)
    {
        MountainStreamFit plan;
        const bool ok = Run(error,[&] {
            const auto doc = Read(scene,terrainID,true); // validates exact last result
            const auto* source = scene.splines.GetComponent(river);
            const auto* material = scene.materials.GetComponent(river);
            Check(source && material && material->shaderType ==
                wi::scene::MaterialComponent::SHADERTYPE_WATER &&
                !source->IsLooped() && !source->IsFilled() &&
                source->mesh_generation_vertical_subdivision == 0 &&
                source->terrain_modifier_amount == 0 &&
                std::abs(source->rotation) < .00001f &&
                std::isfinite(source->width) && source->width >= .25f &&
                source->width <= 4.f,
                "Mountain fit requires an open WATER ribbon, 0.5 to 8m wide, without rotation/modifiers");
            const auto matrix = MatrixValues(scene.ComputeEntityMatrixRecursive(river));
            for (std::size_t i=0;i<12;++i)
                Check(std::isfinite(matrix[i]) && std::abs(matrix[i] -
                    ((i==0 || i==5 || i==10) ? 1.f : 0.f)) < .00001f,
                    "Mountain fit requires an unscaled, unrotated river root");
            Check(source->spline_node_entities.size() >= 2 &&
                source->spline_node_entities.size() <= 128,
                "Mountain fit supports 2 to 128 editable nodes");
            wi::scene::SplineComponent path;
            for (auto id : source->spline_node_entities)
            {
                const auto* node=scene.transforms.GetComponent(id);
                const auto* hierarchy=scene.hierarchy.GetComponent(id);
                Check(node && hierarchy && hierarchy->parentID==river && std::abs(node->scale_local.x-1)<.00001f &&
                    std::abs(node->rotation_local.x)<.00001f &&
                    std::abs(node->rotation_local.y)<.00001f &&
                    std::abs(node->rotation_local.z)<.00001f,
                    "Mountain fit requires upright unit-width control points");
                auto copy=*node; copy.SetDirty(); copy.UpdateTransform();
                path.spline_node_transforms.push_back(copy);
            }
            path.PrecomputeSplineNodeDistances();
            const auto root=scene.ComputeEntityMatrixRecursive(river);
            auto evaluate=[&](float t) {
                XMFLOAT3 p;
                DirectX::XMStoreFloat3(&p,DirectX::XMVector3TransformCoord(
                    path.EvaluateSplineAt(t).r[3],root));
                Check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),
                    "Non-finite native spline");
                return p;
            };
            // Exact LOD0 triangle interpolation on the authored baseline,
            // not a ray against a previously cut bed or procedural height noise.
            struct Grid { const Chunk* chunk; XMFLOAT4X4 inverse, world;
                float x,z,dx,dz; };
            std::vector<Grid> grids;
            for(const auto& chunk:doc.chunks)
            {
                const auto* mesh=scene.meshes.GetComponent(chunk.entity);
                Grid grid{}; grid.chunk=&chunk;
                const auto world=scene.ComputeEntityMatrixRecursive(chunk.entity);
                const auto values=MatrixValues(world);
                Check(std::abs(values[1])+std::abs(values[2])+std::abs(values[4])+
                    std::abs(values[6])+std::abs(values[8])+std::abs(values[9])<.00001f &&
                    values[0]>0 && values[10]>0,
                    "Mountain fit requires an axis-aligned terrain grid");
                DirectX::XMStoreFloat4x4(&grid.world,world);
                DirectX::XMStoreFloat4x4(&grid.inverse,DirectX::XMMatrixInverse(nullptr,world));
                grid.x=mesh->vertex_positions[0].x; grid.z=mesh->vertex_positions[0].z;
                grid.dx=mesh->vertex_positions[1].x-grid.x;
                grid.dz=mesh->vertex_positions[wi::terrain::chunk_width].z-grid.z;
                Check(grid.dx>0 && grid.dz>0,"Invalid terrain grid spacing");
                for(std::size_t index=0;index<mesh->vertex_positions.size();++index)
                {
                    const auto& p=mesh->vertex_positions[index];
                    Check(std::isfinite(p.x) && std::isfinite(p.z) &&
                        std::abs(p.x-(grid.x+(index%wi::terrain::chunk_width)*grid.dx))<.0001f &&
                        std::abs(p.z-(grid.z+(index/wi::terrain::chunk_width)*grid.dz))<.0001f,
                        "Terrain horizontal topology differs from its native grid");
                }
                grids.push_back(grid);
            }
            auto height=[&](float x,float z) {
                for(const auto& grid:grids)
                {
                    XMFLOAT3 local;
                    DirectX::XMStoreFloat3(&local,DirectX::XMVector3TransformCoord(
                        DirectX::XMVectorSet(x,0,z,1),DirectX::XMLoadFloat4x4(&grid.inverse)));
                    const float u=(local.x-grid.x)/grid.dx,v=(local.z-grid.z)/grid.dz;
                    constexpr int side=wi::terrain::chunk_width;
                    if(u<0 || v<0 || u>side-1 || v>side-1) continue;
                    const int ix=std::min(int(u),side-2),iz=std::min(int(v),side-2);
                    const float a=u-ix,b=v-iz;
                    const auto& h=grid.chunk->base;
                    const auto index=std::size_t(ix+iz*side);
                    // Native lower-left / upper-right cells share TL--LR.
                    const float y=a+b<=1
                        ? h[index]*(1-a-b)+h[index+1]*a+h[index+side]*b
                        : h[index+side]*(1-a)+h[index+1]*(1-b)+h[index+side+1]*(a+b-1);
                    XMFLOAT3 world;
                    DirectX::XMStoreFloat3(&world,DirectX::XMVector3TransformCoord(
                        DirectX::XMVectorSet(local.x,y,local.z,1),
                        DirectX::XMLoadFloat4x4(&grid.world)));
                    return world.y;
                }
                throw std::runtime_error("Stream or either bank leaves the resident terrain; shorten or move the path");
            };
            struct Section { XMFLOAT3 p; float low,high; };
            auto section=[&](float t) {
                const auto p=evaluate(t);
                const auto a=evaluate(std::max(0.f,t-.0001f));
                const auto b=evaluate(std::min(1.f,t+.0001f));
                const float dx=b.x-a.x,dz=b.z-a.z,len=std::hypot(dx,dz);
                Check(len>.00001f,"Vertical or zero-distance reach requires a waterfall, not a ribbon");
                Section s{p,INFINITY,-INFINITY};
                const int steps=std::max(4,int(std::ceil(2*source->width/.25f)));
                for(int j=0;j<=steps;++j)
                {
                    const float lateral=source->width*(2.f*j/steps-1);
                    const float y=height(p.x-dz/len*lateral,p.z+dx/len*lateral);
                    s.low=std::min(s.low,y); s.high=std::max(s.high,y);
                }
                return s;
            };
            // Start at every authored control interval, then subdivide for
            // distance, plan curvature and terrain/bank curvature. Hard caps
            // prevent per-frame or pathological recursive work.
            std::vector<Section> sections; sections.reserve(128);
            auto append=[&](auto&& self,float ta,const Section& a,float tb,
                const Section& b,int depth)->void {
                const float tm=(ta+tb)*.5f; const auto mid=section(tm);
                const float distance=std::hypot(b.p.x-a.p.x,b.p.z-a.p.z);
                const float bend=std::hypot(mid.p.x-(a.p.x+b.p.x)*.5f,
                    mid.p.z-(a.p.z+b.p.z)*.5f);
                const bool split=distance>1.f || bend>.05f ||
                    std::abs(mid.low-(a.low+b.low)*.5f)>.04f;
                if(split)
                {
                    Check(depth<10,"Terrain discontinuity requires a separate waterfall reach");
                    self(self,ta,a,tm,mid,depth+1); self(self,tm,mid,tb,b,depth+1);
                }
                else { Check(sections.size()<128,"Stream exceeds 128-node fitting budget; author shorter reaches");
                    sections.push_back(b); }
            };
            const int intervals=int(source->spline_node_entities.size()-1);
            sections.push_back(section(0));
            for(int i=0;i<intervals;++i)
                append(append,float(i)/intervals,sections.back(),
                    float(i+1)/intervals,section(float(i+1)/intervals),0);
            Check(sections.front().low >= sections.back().low-.01f,
                "Draw the stream downhill from source to outlet");
            std::vector<float> y;
            for(const auto& s:sections) y.push_back(s.low-.15f);
            // A lower-envelope low-pass suppresses humps without raising the
            // water above the low bank. Downstream propagation prevents uphill
            // flow; cliffs and excessive excavation are refused below.
            for(int pass=0;pass<6;++pass)
            {
                auto next=y;
                for(std::size_t i=1;i+1<y.size();++i)
                    next[i]=std::min(y[i],(y[i-1]+2*y[i]+y[i+1])*.25f);
                y=std::move(next);
            }
            for(std::size_t i=0;i<sections.size();++i)
            {
                auto p=sections[i].p;
                if(i>0)
                {
                    const auto& previous=plan.points.back();
                    const float d=std::hypot(p.x-previous.x,p.z-previous.z);
                    Check(d>=.05f,"Repeated horizontal positions or tight cusp in stream");
                    y[i]=std::min(y[i],y[i-1]-.002f*d);
                    Check((y[i-1]-y[i])/d<=1.5f,
                        "Reach exceeds 150% downhill grade; split off a waterfall");
                    plan.lengthMeters+=d;
                }
                p.y=y[i]; plan.points.push_back(p);
                plan.maximumCutMeters=std::max(plan.maximumCutMeters,sections[i].high-p.y);
            }
            Check(plan.maximumCutMeters<=4.f,
                "Full-width channel needs over 4m of excavation; narrow or reroute the stream");
            // Validate the ACTUAL native interpolant, including positions
            // between the new nodes and bank width, before any scene mutation.
            wi::scene::SplineComponent fitted;
            for(auto p:plan.points) { wi::scene::TransformComponent tr;
                tr.translation_local=p;tr.UpdateTransform();fitted.spline_node_transforms.push_back(tr); }
            fitted.PrecomputeSplineNodeDistances();
            float last=INFINITY;
            const auto samples=(plan.points.size()-1)*6;
            for(std::size_t i=0;i<samples;++i)
            {
                const auto m=fitted.EvaluateSplineAt(float(i)/float(samples-1));
                XMFLOAT3 p,r;
                DirectX::XMStoreFloat3(&p,m.r[3]); DirectX::XMStoreFloat3(&r,m.r[0]);
                Check(std::isfinite(p.y) && p.y<=last+.003f,
                    "Native interpolation turns uphill; shorten or smooth this reach");
                last=p.y;
                for(int j=0;j<=8;++j)
                {
                    const float lateral=source->width*(2.f*j/8-1);
                    const float ground=height(p.x+r.x*lateral,p.z+r.z*lateral);
                    Check(p.y<=ground+.02f && ground-p.y<=4.1f,
                        "Interpolated water leaves the bank envelope; narrow or shorten this reach");
                }
            }
            return true;
        });
        if(ok) result=std::move(plan);
        return ok;
    }

    FitMountainStreamCommand::FitMountainStreamCommand(wi::scene::Scene& scene,
        wi::ecs::Entity terrain,wi::ecs::Entity river,float depth,float bank)
        :scene_(&scene),terrain_(terrain),river_(river),depth_(depth),bank_(bank) {}
    bool FitMountainStreamCommand::ApplyNodes(bool fitted)
    {
        auto* spline=scene_->splines.GetComponent(river_);
        if(!spline) return false;
        const auto& nodes=fitted?afterNodes_:beforeNodes_;
        if(!fitted)
            for(std::size_t i=beforeNodes_.size();i<afterNodes_.size();++i)
                scene_->Entity_Remove(afterNodes_[i].entity,true);
        spline->spline_node_entities.clear();
        for(std::size_t i=0;i<nodes.size();++i)
        {
            const auto& node=nodes[i];
            if(!scene_->transforms.Contains(node.entity))
            {
                scene_->transforms.Create(node.entity);
                scene_->names.Create(node.entity).name="Stream Fit Point "+std::to_string(i+1);
                scene_->Component_Attach(node.entity,river_);
            }
            auto* transform=scene_->transforms.GetComponent(node.entity);
            transform->translation_local=node.pose.translation;
            transform->rotation_local=node.pose.rotation;
            transform->scale_local=node.pose.scale;
            scene_->transforms.GetComponent(node.entity)->SetDirty();
            spline->spline_node_entities.push_back(node.entity);
        }
        spline->mesh_generation_subdivision=fitted?6:beforeSubdivisions_;
        spline->prev_mesh_generation_nodes=-1;spline->SetDirty();
        return true;
    }
    bool FitMountainStreamCommand::Execute()
    {
        auto* terrain=scene_->terrains.GetComponent(terrain_);
        if(!terrain) {error_="Terrain no longer exists";return false;}
        if(captured_)
        {
            if(!ApplyNodes(true)) return false;
            if(ApplyTerrainSculpt(*scene_,*terrain,afterTerrain_)) return true;
            ApplyNodes(false);return false;
        }
        MountainStreamFit fit;
        if(!PlanMountainStream(*scene_,terrain_,river_,fit,error_)) return false;
        beforeTerrain_=CaptureTerrainSculpt(*scene_,*terrain);
        auto* spline=scene_->splines.GetComponent(river_);
        beforeSubdivisions_=spline->mesh_generation_subdivision;
        beforeNodes_.clear();afterNodes_.clear();
        for(auto node:spline->spline_node_entities)
            beforeNodes_.push_back({node,CaptureTransform(*scene_->transforms.GetComponent(node))});
        const auto inverse=DirectX::XMMatrixInverse(nullptr,scene_->ComputeEntityMatrixRecursive(river_));
        for(std::size_t i=0;i<fit.points.size();++i)
        {
            TransformState pose{};
            DirectX::XMStoreFloat3(&pose.translation,DirectX::XMVector3TransformCoord(
                DirectX::XMLoadFloat3(&fit.points[i]),inverse));
            const auto id=i<beforeNodes_.size()?beforeNodes_[i].entity:wi::ecs::CreateEntity();
            afterNodes_.push_back({id,pose});
        }
        if(!ApplyNodes(true)) return false;
        const bool committed=Run(error_,[&] {
            Check(std::isfinite(depth_) && depth_>=.05f && depth_<=5 &&
                std::isfinite(bank_) && bank_>=.5f && bank_<=20,
                "Invalid fitted channel depth/bank settings");
            const auto id=PersistentEntityId(*scene_,river_);
            Check(IsValidStableId(id),"River has no stable identity");
            auto doc=Read(*scene_,terrain_,true);
            auto found=std::find_if(doc.layers.begin(),doc.layers.end(),
                [&](const Layer& l){return l.river==id;});
            if(found==doc.layers.end())
            { Check(doc.layers.size()<32,"Too many river layers");doc.layers.push_back({id,depth_,bank_}); }
            else {found->depth=depth_;found->bank=bank_;}
            return Commit(*scene_,terrain_,std::move(doc),true,river_);
        });
        if(!committed) { ApplyNodes(false);return false; }
        afterTerrain_=CaptureTerrainSculpt(*scene_,*terrain);captured_=true;return true;
    }
    void FitMountainStreamCommand::Undo()
    {
        if(!captured_) return;
        ApplyNodes(false);
        if(auto* terrain=scene_->terrains.GetComponent(terrain_))
            ApplyTerrainSculpt(*scene_,*terrain,beforeTerrain_);
    }

}
