#pragma once
#include <WickedEngine.h>
#include "renegade/bridge/SceneService.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace terrain_collision_surface_proof
{
    inline void Check(bool ok,const char* message)
    {if(!ok)throw std::runtime_error(message);}

    inline void Settle(wi::scene::Scene& scene)
    {
        auto* gpu=wi::graphics::GetDevice();
        for(int n=0;n<8;++n)
        {
            wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
            scene.Update(1.f/60.f);gpu->SubmitCommandLists();
            wi::renderer::UpdateGPUSuballocator();
        }
        gpu->WaitForGPU();
    }

    inline wi::ecs::Entity CreateGrid(wi::scene::Scene& scene,bool precompute,bool flat)
    {
        const auto id=wi::ecs::CreateEntity();
        const auto material=wi::ecs::CreateEntity();
        scene.names.Create(id)="Off-grid terrain collision regression";
        scene.transforms.Create(id);
        scene.materials.Create(material);
        scene.objects.Create(id).meshID=id;
        auto& mesh=scene.meshes.Create(id);
        for(int z=0;z<4;++z)for(int x=0;x<4;++x)
        {
            const float y=flat?0.f:((x==2&&z==1)||(x==1&&z==2)?1.f:0.f);
            mesh.vertex_positions.push_back(XMFLOAT3(float(x),y,float(z)));
            mesh.vertex_normals.push_back(XMFLOAT3(0,1,0));
            mesh.vertex_uvset_0.push_back(XMFLOAT2(float(x)/3,float(z)/3));
        }
        for(unsigned z=0;z<3;++z)for(unsigned x=0;x<3;++x)
        {
            const unsigned a=z*4+x,b=a+1,c=a+4,d=c+1;
            // Exact pinned terrain LOD0 diagonal: (0,1) to (1,0).
            for(unsigned i:{c,a,b,c,b,d})mesh.indices.push_back(i);
        }
        wi::scene::MeshComponent::MeshSubset subset;
        subset.materialID=material;subset.indexOffset=0;
        subset.indexCount=unsigned(mesh.indices.size());
        mesh.subsets.push_back(subset);
        mesh.SetDoubleSided(true);mesh.SetBVHEnabled(true);mesh.CreateRenderData();
        auto& body=scene.rigidbodies.Create(id);
        body.shape=wi::scene::RigidBodyPhysicsComponent::HEIGHTFIELD;body.mass=0;
        if(precompute)
        {
            auto& shape=mesh.precomputed_rigidbody_physics_shape;
            shape.shape=wi::scene::RigidBodyPhysicsComponent::HEIGHTFIELD;shape.mass=0;
            wi::physics::CreateRigidBodyShape(shape,XMFLOAT3(1,1,1),&mesh);
            Check(shape.shape==wi::scene::RigidBodyPhysicsComponent::HEIGHTFIELD,
                "Collision correction changed the saved heightfield shape type");
        }
        return id;
    }

    inline void Contacts(wi::scene::Scene& scene,wi::ecs::Entity id,bool flat,const char* phase)
    {
        for(const XMFLOAT2 f:{XMFLOAT2(.25f,.25f),XMFLOAT2(.75f,.75f),
            XMFLOAT2(.25f,.75f),XMFLOAT2(.75f,.25f),XMFLOAT2(.2f,.3f),XMFLOAT2(.7f,.8f)})
        {
            const wi::primitive::Ray ray(XMFLOAT3(1+f.x,10,1+f.y),XMFLOAT3(0,-1,0),0,20);
            const auto visual=wi::scene::Pick(ray,wi::enums::FILTER_ALL,~0u,scene);
            const auto physics=wi::physics::Intersects(scene,ray);
            Check(visual.entity==id&&physics.IsValid()&&physics.entity==id,
                "Off-grid collider/raster query missed the test mesh");
            const float expected=flat?0.f:1.f-std::abs(f.x+f.y-1.f);
            Check(std::abs(visual.position.y-expected)<.0001f,
                "Regression mesh does not reproduce the rendered terrain diagonal");
            const float error=std::abs(visual.position.y-physics.position.y);
            std::cout<<"OFF_GRID_CONTACT phase="<<phase<<" x="<<f.x<<" z="<<f.y
                <<" rendered="<<visual.position.y<<" physics="<<physics.position.y
                <<" error="<<error<<std::endl;
            Check(error<.002f,"Jolt collision differs inside a rendered terrain cell");
        }
    }

    inline void Run(const std::filesystem::path& out)
    {
        const bool enabled=wi::physics::IsEnabled();
        const bool simulation=wi::physics::IsSimulationEnabled();
        wi::physics::SetEnabled(true);wi::physics::SetSimulationEnabled(false);
        struct Restore{bool e,s;~Restore(){wi::physics::SetEnabled(e);wi::physics::SetSimulationEnabled(s);}} restore{enabled,simulation};
        for(bool precompute:{true,false})for(bool flat:{false,true})
        {
            renegade::bridge::SceneService service;
            auto& scene=service.GetScene();const auto id=CreateGrid(scene,precompute,flat);
            Settle(scene);Contacts(scene,id,flat,precompute?"precomputed":"fallback");
            const auto file=out/(std::string("offgrid-")+(precompute?"precomputed-":"fallback-")+(flat?"flat":"saddle")+".wiscene");
            wi::Archive disk(file.generic_string(),false,false);scene.Serialize(disk);
            Check(disk.SaveFile(file.generic_string()),"Off-grid fixture save failed");
            renegade::bridge::SceneService reopened;
            Check(reopened.LoadScene(file.generic_string()),"Off-grid fixture cold load failed");
            auto& loaded=reopened.GetScene();
            Check(loaded.rigidbodies.GetCount()==1,"Cold fixture rigid body missing");
            const auto coldID=loaded.rigidbodies.GetEntity(0);
            Settle(loaded);Contacts(loaded,coldID,flat,"cold");
            Check(loaded.rigidbodies[0].shape==wi::scene::RigidBodyPhysicsComponent::HEIGHTFIELD,
                "Cold collision correction changed serialized shape type");
        }
        std::cout<<"OFF_GRID_TERRAIN_COLLISION_PRECOMPUTE_FALLBACK_COLD_PASS"<<std::endl;
    }
}
