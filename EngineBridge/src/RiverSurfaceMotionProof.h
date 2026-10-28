#pragma once
// Optional G8D proof only. Bounded kinematic surface, not a free-surface solver.
// Native WATER triangles and native-spline-derived transport are the authorities.
#include "RiverRapidsTransport.h"
#include <map>
#include <queue>
#include <iostream>
namespace renegade::bridge::river_surface_motion {
using river_rapids_transport::Check;
struct Refined {
    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT2> uv;
    std::vector<uint32_t> indices;
    double area=0;
};
inline double Area(const std::vector<XMFLOAT3>& p,const std::vector<uint32_t>& ix){
    double a=0;for(size_t k=0;k<ix.size();k+=3){auto x=p.at(ix[k]),y=p.at(ix[k+1]),z=p.at(ix[k+2]);
        a+=std::abs(double(y.x-x.x)*(z.z-x.z)-double(y.z-x.z)*(z.x-x.x))*.5;}return a;
}
inline Refined Refine(const wi::scene::MeshComponent& mesh,float edge=.35f){
    Check(mesh.subsets.size()==1&&!mesh.IsSkinned(),"Surface proof requires one rigid native WATER subset");
    Check(mesh.vertex_positions.size()==mesh.vertex_uvset_0.size(),"Native WATER UV stream mismatch");
    Refined out;out.positions.assign(mesh.vertex_positions.begin(),mesh.vertex_positions.end());
    out.uv.assign(mesh.vertex_uvset_0.begin(),mesh.vertex_uvset_0.end());out.indices.assign(mesh.indices.begin(),mesh.indices.end());
    const double original=Area(out.positions,out.indices); std::cout<<"SURFACE_INPUT vertices="<<out.positions.size()<<" triangles="<<out.indices.size()/3<<" area="<<original<<std::endl;
    using Edge=std::pair<uint32_t,uint32_t>;
    auto key=[](uint32_t a,uint32_t b){return std::pair<uint32_t,uint32_t>{std::min(a,b),std::max(a,b)};};
    std::map<Edge,std::set<size_t>> adjacency;
    std::vector<std::array<uint32_t,3>> triangles;
    std::vector<bool> alive;
    using Candidate=std::pair<float,Edge>;
    std::priority_queue<Candidate> longest;
    auto length=[&](Edge e){auto a=out.positions[e.first],b=out.positions[e.second];return std::hypot(a.x-b.x,a.z-b.z);};
    auto add=[&](std::array<uint32_t,3> t){
        Check(triangles.size()<800000,"Surface refinement work budget");
        size_t id=triangles.size();triangles.push_back(t);alive.push_back(true);
        for(int j=0;j<3;++j){Edge e=key(t[j],t[(j+1)%3]);auto [it,fresh]=adjacency.try_emplace(e);
            it->second.insert(id);if(fresh){float len=length(e);if(len>edge)longest.emplace(len,e);}}
    };
    for(size_t k=0;k<out.indices.size();k+=3)add({out.indices[k],out.indices[k+1],out.indices[k+2]});
    while(!longest.empty()){
        const auto candidate=longest.top();longest.pop();auto found=adjacency.find(candidate.second);
        if(found==adjacency.end())continue;
        const Edge e=candidate.second;auto incident=found->second;
        Check(incident.size()<=2,"Nonmanifold native surface edge");
        Check(out.positions.size()<160000,"Surface vertex budget exceeded");
        uint32_t midpoint=uint32_t(out.positions.size());auto a=out.positions[e.first],b=out.positions[e.second];
        out.positions.push_back({(a.x+b.x)*.5f,(a.y+b.y)*.5f,(a.z+b.z)*.5f});
        auto u=out.uv[e.first],v=out.uv[e.second];out.uv.push_back({(u.x+v.x)*.5f,(u.y+v.y)*.5f});
        for(size_t id:incident){
            Check(alive[id],"Invalid surface adjacency");auto t=triangles[id];alive[id]=false;
            for(int j=0;j<3;++j){auto it=adjacency.find(key(t[j],t[(j+1)%3]));Check(it!=adjacency.end(),"Missing native edge");it->second.erase(id);if(it->second.empty())adjacency.erase(it);}
            for(int j=0;j<3;++j)if(key(t[j],t[(j+1)%3])==e){auto c=t[(j+2)%3];add({t[j],midpoint,c});add({midpoint,t[(j+1)%3],c});break;}
        }
    }
    out.indices.clear();for(size_t k=0;k<triangles.size();++k)if(alive[k])out.indices.insert(out.indices.end(),triangles[k].begin(),triangles[k].end());
    Check(out.indices.size()/3<=250000,"Surface triangle budget exceeded");
    out.area=Area(out.positions,out.indices);
    Check(std::abs(out.area-original)<std::max(1e-5,original*2e-6),"Refinement changed native WATER footprint area");
    return out;
}
struct Field {std::vector<XMFLOAT4> texels;float maximumAmplitude=0;};
inline Field Build(const RiverFlowNetwork& network,const river_rapids_transport::Domain& domain){
    const auto& mask=domain.mask;const int w=int(mask.width),h=int(mask.height);
    const float dx=mask.bounds.z/w,dz=mask.bounds.w/h;
    Field out;out.texels.resize(mask.cells.size());
    // Conservative cell-edge distance to all dry/solid/cap boundaries.
    std::vector<float> distance(mask.cells.size(),INFINITY);
    using Item=std::pair<float,int>;std::priority_queue<Item,std::vector<Item>,std::greater<Item>> queue;
    for(int k:domain.active){int x=k%w,z=k/w;
        if(x==0||z==0||x==w-1||z==h-1||domain.index[k-1]<0||domain.index[k+1]<0||domain.index[k-w]<0||domain.index[k+w]<0){
            distance[k]=0;queue.emplace(0.f,k);}
    }
    while(!queue.empty()){auto [cost,k]=queue.top();queue.pop();if(cost!=distance[k])continue;
        const int x=k%w,z=k/w;const int neighbors[]={k-1,k+1,k-w,k+w};const float step[]={dx,dx,dz,dz};
        for(int j=0;j<4;++j){int n=neighbors[j];if(n<0||n>=w*h||(j==0&&x==0)||(j==1&&x==w-1)||(j==2&&z==0)||(j==3&&z==h-1)||domain.index[n]<0)continue;
            float c=cost+step[j];if(c<distance[n]){distance[n]=c;queue.emplace(c,n);}}
    }
    for(int k:domain.active){
        XMFLOAT2 p{mask.bounds.x+(k%w+.5f)*dx,mask.bounds.y+(k/w+.5f)*dz};
        double phase=0,weight=0;
        for(const auto& reach:network.Reaches()){auto q=reach.field->Evaluate(p);if(!q.valid)continue;
            const float local=q.alongMeters-(reach.id=="outlet"?0:reach.field->LengthMeters());
            const float blend=std::max(.02f,1-q.lateralFraction*q.lateralFraction);phase+=local*blend;weight+=blend;}
        if(weight==0){const auto* outlet=network.FindReach("outlet");Check(outlet!=nullptr,"Proof outlet missing");
            phase=(p.x-outlet->upstream.centre.x)*outlet->upstream.directionXZ.x+(p.y-outlet->upstream.centre.z)*outlet->upstream.directionXZ.y;weight=1;}
        const float t=std::clamp((distance[k]-.15f)/.6f,0.f,1.f),fade=t*t*(3-2*t);
        const float rough=std::clamp(domain.froude[k]/2.f,0.f,1.f);
        const float amp=std::min(.10f,domain.depth[k]*.15f)*fade*(.15f+.85f*rough);
        Check(std::isfinite(amp)&&std::isfinite(phase),"Nonfinite surface field");
        out.texels[k]={float(phase/weight),amp,distance[k],1};out.maximumAmplitude=std::max(out.maximumAmplitude,amp);
    }
    // One shared continuous scalar phase across the union, not separate branch wave clocks.
    for(int n=0;n<8;++n){auto old=out.texels;
        for(int k:domain.active){double sum=old[k].x*4,weight=4;const int x=k%w,z=k/w;
            for(int off:{-1,1,-w,w}){int j=k+off;if(j<0||j>=w*h||(off==-1&&x==0)||(off==1&&x==w-1)||domain.index[j]<0)continue;sum+=old[j].x;weight++;}
            out.texels[k].x=float(sum/weight);}}
    Check(out.maximumAmplitude<=.100001f,"Surface amplitude bound");return out;
}
}
