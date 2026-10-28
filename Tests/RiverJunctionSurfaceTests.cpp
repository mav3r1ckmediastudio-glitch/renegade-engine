#include "RiverJunctionFixture.h"
#include <iostream>
#include <limits>
using namespace renegade::bridge;
using namespace junction_proof;
int main()
{
    try {
        Check(wi::graphics::GetDevice()==nullptr,"CPU-only test");
        wi::scene::Scene scene;Create(scene);
        const auto network=Network(scene);
        std::shared_ptr<const RiverJunctionSurface> surface;std::string error;
        Check(BuildRiverJunctionSurface(*network,surface,error),error);
        Check(surface->AreaSquareMetres()>250 && surface->AreaSquareMetres()<400,"Plausible exact union area");
        std::size_t covered=0,checks=0;
        for(int z=0;z<150;++z)for(int x=0;x<260;++x) {
            const XMFLOAT2 p{-28+(x+.371f)*.24f,-15+(z+.619f)*.2f};
            bool native=false;for(const auto& reach:network->Reaches())native|=reach.field->Evaluate(p).valid;
            auto q=surface->Evaluate(p);
            Check(q.valid==native,"Native footprint union coverage mismatch");
            ++checks;if(!q.valid)continue;++covered;
            Check(std::isfinite(q.velocityXZ.x)&&std::hypot(q.velocityXZ.x,q.velocityXZ.y)>.1f,"Finite moving flow");
            Check(q.velocityXZ.x>0,"Downstream flow through join");
            unsigned count=0;
            const auto& v=surface->Positions();const auto& ind=surface->Indices();
            for(std::size_t k=0;k<ind.size();k+=3)
                count+=Inside(p,v[ind[k]],v[ind[k+1]],v[ind[k+2]],true);
            Check(count<=1,"Overlapping output water triangles");
        }
        Check(covered>1000,"Coverage proof sufficient");
        // At shared vertices both adjacent triangle interpolants have identical flow.
        for(std::size_t n=0;n<surface->Positions().size();++n) {
            const auto p=surface->Positions()[n];const auto q=surface->Evaluate({p.x,p.z});
            Check(q.valid,"Joined mesh vertex uncovered");
            Check(std::hypot(q.velocityXZ.x-surface->Velocities()[n].x,
                q.velocityXZ.y-surface->Velocities()[n].y)<.002f,"Shared vertex velocity seam");
        }
        Check(!surface->Evaluate({std::numeric_limits<float>::quiet_NaN(),0}).valid,"Nonfinite query refused");
        const auto q=surface->Evaluate({0,0});
        Check(q.valid&&q.velocityXZ.x>.99f&&std::abs(q.velocityXZ.y)<.01f,"Join flows toward outlet");
        for(float yaw:{-.9f,1.5707963f,2.4f}) {
            wi::scene::Scene transformed;Create(transformed);
            const auto parent=transformed.Entity_CreateTransform("Junction parent");
            for(const auto& input:Inputs(transformed))transformed.Component_Attach(input.river,parent);
            auto* t=transformed.transforms.GetComponent(parent);t->translation_local={35,7,-23};t->scale_local={1.3f,1,.8f};
            XMStoreFloat4(&t->rotation_local,XMQuaternionRotationRollPitchYaw(0,yaw,0));t->SetDirty();t->UpdateTransform();
            auto n=Network(transformed);std::shared_ptr<const RiverJunctionSurface> other;std::string reason;
            Check(BuildRiverJunctionSurface(*n,other,reason),reason);
            Check(std::abs(other->AreaSquareMetres()-surface->AreaSquareMetres()*1.04)<.02,"Transformed union area");
            for(const auto& reach:n->Reaches()) {
                const auto& samples=reach.field->Samples();
                for(std::size_t k=1;k+1<samples.size();++k)
                    Check(other->Evaluate({samples[k].worldPosition.x,samples[k].worldPosition.z}).valid,"Transformed native centre uncovered");
            }
        }
        auto same=surface;
        Check(BuildRiverJunctionSurface(*network,same,error)&&same->Indices()==surface->Indices(),"Repeat topology deterministic");
        // G7A tolerance does not authorize this geometry service to create a bridge.
        auto* t=scene.transforms.GetComponent(scene.splines.GetComponent(scene.Entity_FindByName("west"))->spline_node_entities.front());
        t->translation_local.x=.01f;t->SetDirty();t->UpdateTransform();
        const auto gap=Network(scene);auto preserved=surface;
        Check(!BuildRiverJunctionSurface(*gap,preserved,error)&&preserved==surface,"Gap refuses without replacement");
        Check(error.find("gaps")!=std::string::npos,"Clear gap refusal");
        std::cout<<"JUNCTION_CPU=PASS sampled="<<checks<<" covered="<<covered<<" vertices="<<surface->Positions().size()
            <<" triangles="<<surface->Indices().size()/3<<" area="<<surface->AreaSquareMetres()<<"\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
