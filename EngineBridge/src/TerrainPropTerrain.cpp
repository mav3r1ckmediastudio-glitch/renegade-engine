#include "renegade/bridge/TerrainPropTerrain.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <limits>

namespace renegade::bridge
{
    namespace
    {
        constexpr int Side = wi::terrain::chunk_width;
        constexpr float Bucket = 128.f;
        struct Grid
        {
            DirectX::XMFLOAT4X4 inverse, world;
            float x, z, dx, dz;
            std::vector<float> heights;
        };
        struct Snapshot
        {
            std::vector<Grid> grids;
            std::map<std::pair<int,int>,std::vector<std::size_t>> buckets;
            bool Sample(float x, float z, float& height) const
            {
                if (!std::isfinite(x) || !std::isfinite(z) ||
                    std::abs(x)>10000000 || std::abs(z)>10000000) return false;
                auto found=buckets.find({int(std::floor(x/Bucket)),int(std::floor(z/Bucket))});
                if(found==buckets.end()) return false;
                for(auto index:found->second)
                {
                    const auto& g=grids[index];
                    DirectX::XMFLOAT3 local;
                    DirectX::XMStoreFloat3(&local,DirectX::XMVector3TransformCoord(
                        DirectX::XMVectorSet(x,0,z,1),DirectX::XMLoadFloat4x4(&g.inverse)));
                    float u=(local.x-g.x)/g.dx, v=(local.z-g.z)/g.dz;
                    if(u<0 || v<0 || u>Side-1 || v>Side-1) continue;
                    const int ix=std::min(int(u),Side-2), iz=std::min(int(v),Side-2);
                    const float a=u-ix,b=v-iz;
                    const auto n=std::size_t(ix+iz*Side);
                    const auto& h=g.heights;
                    // Same diagonal as native LOD0 indices.
                    const float y=a+b<=1 ? h[n]*(1-a-b)+h[n+1]*a+h[n+Side]*b :
                        h[n+Side]*(1-a)+h[n+1]*(1-b)+h[n+Side+1]*(a+b-1);
                    DirectX::XMFLOAT3 world;
                    DirectX::XMStoreFloat3(&world,DirectX::XMVector3TransformCoord(
                        DirectX::XMVectorSet(local.x,y,local.z,1),DirectX::XMLoadFloat4x4(&g.world)));
                    height=world.y;
                    return std::isfinite(height);
                }
                return false;
            }
        };
    }
    bool CaptureTerrainPropTerrain(const wi::scene::Scene& scene,
        wi::ecs::Entity entity, TerrainPropTerrainSnapshot& output, std::string& error)
    {
        error.clear();
        const auto* terrain=scene.terrains.GetComponent(entity);
        if(!terrain || terrain->IsGenerationBusy())
        { error="Wait for terrain generation to finish."; return false; }
        if(terrain->chunks.empty() || terrain->chunks.size()>1089)
        { error="Terrain has no complete authored height grid."; return false; }
        auto snapshot=std::make_shared<Snapshot>();
        TerrainPropBounds bounds{INFINITY,INFINITY,-INFINITY,-INFINITY};
        // Sort native chunk coordinates so shared edges choose deterministically.
        std::map<std::pair<int,int>,wi::ecs::Entity> chunks;
        for(const auto& chunk:terrain->chunks)
            chunks[{chunk.first.x,chunk.first.z}]=chunk.second.entity;
        for(const auto& chunk:chunks)
        {
            const auto* mesh=scene.meshes.GetComponent(chunk.second);
            if(!mesh || mesh->vertex_positions.size()!=Side*Side)
            { error="Terrain chunk is not ready; retry after generation."; return false; }
            Grid grid{};
            const auto world=scene.ComputeEntityMatrixRecursive(chunk.second);
            DirectX::XMStoreFloat4x4(&grid.world,world);
            const auto& m=grid.world;
            for(int row=0;row<4;++row) for(int column=0;column<4;++column)
                if(!std::isfinite(m.m[row][column]))
                { error="Terrain transform contains a nonfinite value."; return false; }
            if(std::abs(m._12)+std::abs(m._32)+std::abs(m._21)+std::abs(m._23)>.00001f ||
                std::abs(m._14)+std::abs(m._24)+std::abs(m._34)>.00001f ||
                std::abs(m._44-1)>.00001f ||
                std::abs(DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(world)))<1e-8f)
            { error="Props require an upright, nonsingular terrain transform."; return false; }
            DirectX::XMStoreFloat4x4(&grid.inverse,DirectX::XMMatrixInverse(nullptr,world));
            grid.x=mesh->vertex_positions[0].x; grid.z=mesh->vertex_positions[0].z;
            grid.dx=mesh->vertex_positions[1].x-grid.x;
            grid.dz=mesh->vertex_positions[Side].z-grid.z;
            if(!(grid.dx>0) || !(grid.dz>0))
            { error="Invalid terrain grid spacing."; return false; }
            for(std::size_t i=0;i<mesh->vertex_positions.size();++i)
            {
                const auto& p=mesh->vertex_positions[i];
                if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z) ||
                    std::abs(p.x-(grid.x+float(i%Side)*grid.dx))>.001f ||
                    std::abs(p.z-(grid.z+float(i/Side)*grid.dz))>.001f)
                { error="Terrain horizontal topology differs from its native grid."; return false; }
                grid.heights.push_back(p.y);
            }
            float minX=INFINITY,minZ=INFINITY,maxX=-INFINITY,maxZ=-INFINITY;
            for(int i:{0,Side-1,Side*(Side-1),Side*Side-1})
            {
                DirectX::XMFLOAT3 p;
                DirectX::XMStoreFloat3(&p,DirectX::XMVector3TransformCoord(
                    DirectX::XMLoadFloat3(&mesh->vertex_positions[i]),world));
                minX=std::min(minX,p.x);minZ=std::min(minZ,p.z);
                maxX=std::max(maxX,p.x);maxZ=std::max(maxZ,p.z);
            }
            if(!std::isfinite(minX)||!std::isfinite(maxX)||!std::isfinite(minZ)||!std::isfinite(maxZ) ||
                std::max({std::abs(minX),std::abs(maxX),std::abs(minZ),std::abs(maxZ)})>10000000 ||
                (maxX-minX)*(maxZ-minZ)>16000000)
            { error="Terrain chunk exceeds the authoring extent budget."; return false; }
            for(int x=int(std::floor(minX/Bucket));x<=int(std::floor(maxX/Bucket));++x)
                for(int z=int(std::floor(minZ/Bucket));z<=int(std::floor(maxZ/Bucket));++z)
                    snapshot->buckets[{x,z}].push_back(snapshot->grids.size());
            snapshot->grids.push_back(std::move(grid));
            bounds.minX=std::min(bounds.minX,minX);bounds.minZ=std::min(bounds.minZ,minZ);
            bounds.maxX=std::max(bounds.maxX,maxX);bounds.maxZ=std::max(bounds.maxZ,maxZ);
        }
        TerrainPropTerrainSnapshot candidate;
        candidate.bounds=bounds;
        candidate.sample=[snapshot](float x,float z,float& y){ return snapshot->Sample(x,z,y); };
        output=std::move(candidate);
        return true;
    }
}
