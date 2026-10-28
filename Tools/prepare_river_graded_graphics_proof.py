"""G10A native-node graded drop proof; accepted flat production path untouched."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(root/'Tools/prepare_river_refined_foam_graphics_proof.py'),str(out)],check=True)
s=(out/'RiverRefinedFoamProof.cpp').read_text()
s=s.replace('#include "RiverJunctionFixture.h"','#include "RiverJunctionFixture.h"\n#include "RiverGradedFixture.h"')
# Same bounded spatial fade, with phase expressed as local travel time.
h=(root/'EngineBridge/src/RiverSurfaceMotionProof.h').read_text()
a=h.index('inline Field Build(');z=h.rindex('\n}')
fn=h[a:z].replace('const RiverFlowNetwork& network','const river_graded::Field& graded')
a=fn.index('        double phase=0,weight=0;');z=fn.index('        const float t=',a)
fn=fn[:a]+'''        const auto section=graded.Nearest(p);
        double phase=section.travel*1.9,weight=1;
'''+fn[z:]
s=s.replace('static uint32_t Bits(float f)', 'namespace renegade::bridge::river_surface_motion {\n'+fn+'\n}\nstatic uint32_t Bits(float f)',1)
s=s.replace('Renegade River V2 - Refined Foam','Renegade River V2 - Graded Drop')
s=s.replace('REFINED FOAM / ','GRADED DROP / ')
s=s.replace(' / PREVIOUS FOAM',' / GLOBAL DEPTH DIAGNOSTIC').replace(' / REFINED FROTH',' / LOCAL GRADED DEPTH')
s=s.replace('B previous/new foam','B global/local depth diagnostic')
s=s.replace('Original foam artwork with actual source and velocity fields. B original detail / refined froth. Space pause; V waves.','Native spline-node drop: G cycles 0 / 0.6 / 0.9 / 1.15m. N control spheres. U Undo; Y Redo. Space pause; V waves.')
a=s.index('        const auto river=scene.Entity_FindByName(')
z=s.index('        Path path;path.scene=&scene;path.river=river;',a)
s=s[:a]+'''        auto river=scene.Entity_FindByName("G10A graded native WATER");
        if(river==wi::ecs::INVALID_ENTITY)river=graded_proof::Create(scene);
        auto built=graded_proof::Refresh(scene,river);auto& domain=built.domain;
        auto* material=scene.materials.GetComponent(river);const auto optics=*material;
        const auto objects=scene.objects.GetCount(),meshes=scene.meshes.GetCount();
        auto refineWater=[&](){
            auto* waterMesh=scene.meshes.GetComponent(scene.objects.GetComponent(river)->meshID);
            auto refined=river_surface_motion::Refine(*waterMesh);
            waterMesh->vertex_positions.assign(refined.positions.begin(),refined.positions.end());
            waterMesh->vertex_uvset_0.assign(refined.uv.begin(),refined.uv.end());
            waterMesh->vertex_uvset_1.clear();waterMesh->vertex_tangents.clear();
            waterMesh->indices.assign(refined.indices.begin(),refined.indices.end());
            waterMesh->subsets[0].indexOffset=0;waterMesh->subsets[0].indexCount=uint32_t(refined.indices.size());
            waterMesh->ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);waterMesh->CreateRenderData();
            waterMesh->aabb._min.y-=.12f;waterMesh->aabb._max.y+=.12f;
        };refineWater();
        auto motionField=river_surface_motion::Build(*built.field,domain);
        std::cout<<"GRADED_WAVE_BOUND="<<motionField.maximumAmplitude<<"\\n";
'''+s[z:]
a=s.index('        for(const auto& reach:Inputs(scene))')
z=s.index('path.bounds=domain.mask.bounds;',a)
s=s[:a]+'        '+s[z:]
s=s.replace('auto q=surface->Evaluate(', 'auto q=built.field->Evaluate(')
s=s.replace('float speed=std::hypot(q.velocityXZ.x,q.velocityXZ.y);normal[k]={q.velocityXZ.x/speed,q.velocityXZ.y/speed,speed,1};','normal[k]={q.directionXZ.x,q.directionXZ.y,q.speedMetersPerSecond,1};')
# Retain the accepted G8H default coverage of 0.3; appearance tuning stays separate.
s=s.replace('const XMFLOAT3 eyes[]={{7,1.15f,2.5f},{12,18,12},{21,1.05f,-5},{11,0.8f,1.5f},{26,1.05f,3.5f}};','const XMFLOAT3 eyes[]={{8,1.4f,4},{20,18,6},{31,.4f,-5},{12,1.2f,3.5f},{24,.5f,3.5f}};')
s=s.replace('const XMFLOAT3 targets[]={{13,0,-1},{12,0,-1},{12,0,-1},{13,0,-1},{18,0,-2}};','const XMFLOAT3 targets[]={{21,-.7f,-1},{20,-.5f,-1},{18,-.6f,-1},{21,-.7f,-1},{18,-.6f,-1}};')
# Rebuild only after explicit structural node editing, never every frame.
anchor='        if(std::string(argv[argc-1])=="--inspect"){'
assert s.count(anchor)==1
code='''          const auto controlNodes=scene.splines.GetComponent(river)->spline_node_entities;
          CommandService nodeHistory;bool nodesVisible=true;
          float nodeDrop=std::abs(scene.transforms.GetComponent(controlNodes.back())->translation_local.y-scene.transforms.GetComponent(controlNodes.front())->translation_local.y);
          auto rebuildNodes=[&](){
            gpu->WaitForGPU();built=graded_proof::Refresh(scene,river);refineWater();
            path.bounds=domain.mask.bounds;params.bounds=domain.mask.bounds;path.maximumRate=domain.maximumRate;
            zero.assign(domain.faces.size(),{});normal.assign(zero.size(),{});
            for(size_t k=0;k<normal.size();++k){auto q=built.field->Evaluate({domain.mask.bounds.x+(k%domain.mask.width+.5f)/domain.mask.width*domain.mask.bounds.z,domain.mask.bounds.y+(k/domain.mask.width+.5f)/domain.mask.height*domain.mask.bounds.w});if(q.valid)normal[k]={q.directionXZ.x,q.directionXZ.y,q.speedMetersPerSecond,1};}
            for(int k:domain.active)normal[k]=domain.velocity[k];
            upload(normals,normal);upload(path.flux,domain.faces);upload(path.source,domain.source);upload(path.velocity,domain.velocity);
            motionField=river_surface_motion::Build(*built.field,domain);upload(waves,motionField.texels);
            params.control.x=descriptor(normals);params.motion.x=descriptor(waves);
            params.foamControl.z=descriptor(path.source);params.foamControl.w=descriptor(path.velocity);
            clear();bind();nodeDrop=std::abs(scene.transforms.GetComponent(controlNodes.back())->translation_local.y-scene.transforms.GetComponent(controlNodes.front())->translation_local.y);
            std::cout<<"NATIVE_NODE_DROP="<<nodeDrop<<" Q="<<domain.inflow<<" speedMax="<<domain.maximumSpeed<<"\\n";
          };
          auto setDrop=[&](float drop){
            class DropCommand final:public ICommand {
              wi::scene::Scene& scene;std::vector<wi::ecs::Entity> nodes;std::vector<TransformState> before,after;
            public:DropCommand(wi::scene::Scene& s,const std::vector<wi::ecs::Entity>& n,float d):scene(s),nodes(n){
              for(size_t k=0;k<nodes.size();++k){auto a=CaptureTransform(*scene.transforms.GetComponent(nodes[k]));before.push_back(a);a.translation.y=k<3?0:(k==3?-.45f*d:-d);after.push_back(a);}}
              bool Execute()override{for(size_t k=0;k<nodes.size();++k){auto* t=scene.transforms.GetComponent(nodes[k]);t->translation_local=after[k].translation;t->SetDirty();t->UpdateTransform();}return true;}
              void Undo()override{for(size_t k=0;k<nodes.size();++k){auto* t=scene.transforms.GetComponent(nodes[k]);t->translation_local=before[k].translation;t->SetDirty();t->UpdateTransform();}}
            };
            Check(nodeHistory.Execute(std::make_unique<DropCommand>(scene,controlNodes,drop)),"Native node height command");rebuildNodes();
          };
          if(!interactive){
            const auto originalHash=built.native->DeterministicHash();setDrop(.6f);
            Check(nodeHistory.Undo(),"Native node drop Undo");rebuildNodes();Check(built.native->DeterministicHash()==originalHash,"Native geometry Undo parity");
            Check(nodeHistory.Redo(),"Native node drop Redo");rebuildNodes();
            Check(nodeHistory.Undo(),"Restore native graded drop");rebuildNodes();Check(built.native->DeterministicHash()==originalHash,"Native geometry final restore");
            std::cout<<"NATIVE_GRADED_EDIT_UNDO_REDO=PASS\\n";
          }
'''
s=s.replace(anchor,code+anchor)
# Native node spheres are transient renderer diagnostics, never overlay geometry.
s=s.replace("if(key=='P'){",'''if(key=='G'){const float next=nodeDrop<.1f?.6f:(nodeDrop<.75f?.9f:(nodeDrop<1?1.15f:0));setDrop(next);}
                          if(key=='U'&&nodeHistory.Undo())rebuildNodes();
                          if(key=='Y'&&nodeHistory.Redo())rebuildNodes();
                          if(key=='N')nodesVisible=!nodesVisible;
                          if(key=='P'){''')
# Mark controls only during live frames; do not contaminate regression screenshots.
anchor='                app.Run();'
# Input loop has its own frames call; inject via Path::Render instead.
s=s.replace('std::vector<wi::ecs::Entity> hiddenRibbons;','bool showNodes=false;std::vector<wi::ecs::Entity> hiddenRibbons;')
s=s.replace('        wi::RenderPath3D::Render();','''        if(showNodes)for(auto e:scene->splines.GetComponent(river)->spline_node_entities){
            const auto p=scene->ComputeEntityMatrixRecursive(e).r[3];XMFLOAT3 world;XMStoreFloat3(&world,p);
            wi::renderer::DrawSphere(wi::primitive::Sphere(world,.12f),XMFLOAT4(.05f,1,.8f,1),false);
        }
        wi::RenderPath3D::Render();''')
# Draw toggle updates directly in input handler, avoiding a per-frame scene scan.
s=s.replace("if(key=='N')nodesVisible=!nodesVisible;","if(key=='N'){nodesVisible=!nodesVisible;path.showNodes=nodesVisible;}")
s=s.replace('ShowWindow(window,SW_SHOW);SetForegroundWindow(window);std::cout<<"RAPIDS_INTERACTIVE_READY=1','path.showNodes=true;ShowWindow(window,SW_SHOW);SetForegroundWindow(window);std::cout<<"RAPIDS_INTERACTIVE_READY=1')
(out/'RiverGradedDropProof.cpp').write_text(s)
print('G10A_NATIVE_GRADED_HARNESS_STAGED')
