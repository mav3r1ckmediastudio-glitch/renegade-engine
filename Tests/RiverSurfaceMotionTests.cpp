#include "RiverJunctionFixture.h"
#include "../EngineBridge/src/RiverSurfaceMotionProof.h"
#include <iostream>
#include <cstring>
using namespace renegade::bridge;
using namespace river_surface_motion;
int main(){try{
    wi::scene::Scene scene;junction_proof::Create(scene);
    auto network=junction_proof::Network(scene);
    std::shared_ptr<const RiverJunctionSurface> surface;std::string error;
    Check(BuildRiverJunctionSurface(*network,surface,error),error);
    wi::scene::MeshComponent mesh;
    mesh.vertex_positions.assign(surface->Positions().begin(),surface->Positions().end());
    mesh.indices.assign(surface->Indices().begin(),surface->Indices().end());
    for(auto p:mesh.vertex_positions)mesh.vertex_uvset_0.push_back({p.x*.25f,p.z*.25f});
    mesh.subsets.resize(1);
    auto refined=Refine(mesh);auto repeat=Refine(mesh);
    Check(refined.positions.size()==repeat.positions.size()&&refined.indices==repeat.indices,"Nondeterministic native refinement");
    Check(std::memcmp(refined.positions.data(),repeat.positions.data(),refined.positions.size()*sizeof(XMFLOAT3))==0,"Nondeterministic native vertices");
    Check(std::memcmp(refined.positions.data(),mesh.vertex_positions.data(),mesh.vertex_positions.size()*sizeof(XMFLOAT3))==0,"Native spline-derived vertices moved");
    Check(std::abs(refined.area-surface->AreaSquareMetres())<.001,"Actual native-footprint union area changed");
    std::map<std::pair<uint32_t,uint32_t>,int> edges;float maximum=0;
    for(size_t k=0;k<refined.indices.size();k+=3){
        auto a=refined.positions[refined.indices[k]],b=refined.positions[refined.indices[k+1]],c=refined.positions[refined.indices[k+2]];
        Check(junction_proof::Cross(a,b,c)<=1e-9,"Native triangle winding changed");
        for(int j=0;j<3;++j){auto x=refined.indices[k+j],y=refined.indices[k+(j+1)%3];auto p=refined.positions[x],q=refined.positions[y];
            maximum=std::max(maximum,std::hypot(p.x-q.x,p.z-q.z));++edges[{std::min(x,y),std::max(x,y)}];}
    }
    for(auto edge:edges)Check(edge.second==1||edge.second==2,"Nonmanifold refined native surface");
    Check(maximum<=.35001f,"Refined edge sampling bound exceeded");
    river_rapids_transport::Surface bed;bed.terrain=true;
    bed.triangles={{{-40,-.3f,-30},{-40,-.3f,30},{40,-.3f,-30}},{{40,-.3f,-30},{-40,-.3f,30},{40,-.3f,30}}};
    auto domain=river_rapids_transport::Build(*network,*surface,{bed},256,128);
    auto field=Build(*network,domain),again=Build(*network,domain);
    Check(std::memcmp(field.texels.data(),again.texels.data(),field.texels.size()*sizeof(XMFLOAT4))==0,"Nondeterministic shared motion field");
    int bank=0,wet=0;
    for(size_t k=0;k<field.texels.size();++k){auto q=field.texels[k];
        Check(std::isfinite(q.x)&&std::isfinite(q.y)&&q.y>=0&&q.y<=domain.depth[k]*.15f+.00001f,"Invalid bounded wave");
        if(domain.index[k]<0)Check(q.y==0,"Waves in excluded native cells");
        else {++wet;if(q.z<=.15f){++bank;Check(q.y==0,"Boundary wave damping failed");}}
    }
    Check(bank>0&&wet>bank&&field.maximumAmplitude>0,"Surface field missing wet interior/bank controls");
    bool refused=false;try{auto invalid=mesh;invalid.subsets.resize(2);Refine(invalid);}catch(...){refused=true;}
    Check(refused,"Unexpected multi-material mesh accepted");
    std::cout<<"SURFACE_CPU=PASS vertices="<<refined.positions.size()<<" triangles="<<refined.indices.size()/3
        <<" nativeArea="<<refined.area<<" maximumEdge="<<maximum<<" wet="<<wet<<" pinnedBank="<<bank<<" maximumAmplitude="<<field.maximumAmplitude<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<"SURFACE_CPU=FAIL "<<e.what()<<"\n";return 1;}}
