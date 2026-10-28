"""G10B owned native falling reach and receiving impact proof."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(root/'Tools/prepare_river_graded_graphics_proof.py'),str(out)],check=True)
s=(out/'RiverGradedDropProof.cpp').read_text()
s=s.replace('#include "RiverGradedFixture.h"','#include "RiverCascadeFixture.h"').replace('graded_proof::','cascade_proof::')
s=s.replace('G10A graded native WATER','G10B receiving native WATER').replace('Renegade River V2 - Graded Drop','Renegade River V2 - Cascade').replace('GRADED DROP / ','CASCADE / ')
s=s.replace('G cycles 0 / 0.6 / 0.9 / 1.15m','G cycles 0.5 / 1.1 / 1.6m')
s=s.replace('std::abs(scene.transforms.GetComponent(controlNodes.back())->translation_local.y-scene.transforms.GetComponent(controlNodes.front())->translation_local.y)','std::abs(scene.transforms.GetComponent(controlNodes.front())->translation_local.y)')
s=s.replace('nodes(n){','nodes(n){const float oldDrop=-s.transforms.GetComponent(n.front())->translation_local.y;')
s=s.replace('a.translation.y=k<3?0:(k==3?-.45f*d:-d);','a.translation.y-=d-oldDrop;')
s=s.replace("const float next=nodeDrop<.1f?.6f:(nodeDrop<.75f?.9f:(nodeDrop<1?1.15f:0));","const float next=nodeDrop<.8f?1.1f:(nodeDrop<1.3f?1.6f:.5f);")
s=s.replace('const XMFLOAT3 eyes[]={{8,1.4f,4},{20,18,6},{31,.4f,-5},{12,1.2f,3.5f},{24,.5f,3.5f}};','const XMFLOAT3 eyes[]={{15,.8f,4.5f},{13,10,7},{16,.1f,-4.5f},{13,.2f,3.5f},{7,1.5f,4}};')
s=s.replace('const XMFLOAT3 targets[]={{21,-.7f,-1},{20,-.5f,-1},{18,-.6f,-1},{21,-.7f,-1},{18,-.6f,-1}};','const XMFLOAT3 targets[]={{10,-.5f,0},{11,-.5f,0},{10,-.5f,0},{10,-.5f,0},{12,-.5f,0}};')
anchor='            foamAmountSlider.SetEnabled(mode==2);'
assert s.count(anchor)==1
s=s.replace(anchor,'''            const auto upper=scene.Entity_FindByName("Cascade upstream native WATER"),fall=scene.Entity_FindByName("Cascade falling native WATER");
            auto* upMesh=scene.meshes.GetComponent(scene.objects.GetComponent(upper)->meshID);
            auto upField=river_cascade::Build(*upMesh,scene.ComputeEntityMatrixRecursive(upper),1.f,float(domain.inflow),false);
            for(auto e:{upper,fall}){auto* m=scene.materials.GetComponent(e);
                if(mode){m->SetCustomShaderID(shader);
                    auto* mesh=scene.meshes.GetComponent(scene.objects.GetComponent(e)->meshID);
                    float total=e==fall?mesh->vertex_uvset_1.back().y:upField.rows.back().travel;
                    m->userdata={uint32_t(gpu->GetDescriptorIndex(&buffer,wi::graphics::SubresourceType::SRV)),e==fall?0x43415343u:0x43555052u,Bits(total),0};
                }else{m->DisableCustomShader();m->userdata={};}m->SetDirty();}
'''+anchor)
# Existing native material restoration assertions include the main receiving reach.
# Extra native reaches are bound/restored in the same explicit mode boundary.
s=s.replace('NATIVE_GRADED_EDIT_UNDO_REDO=PASS','NATIVE_CASCADE_EDIT_UNDO_REDO=PASS')
(out/'RiverCascadeProof.cpp').write_text(s)
print('CASCADE_NATIVE_HARNESS_STAGED')

# Exercise actual vertical native geometry, not just a 2D approximation.
anchor='          if(!interactive){'
shift='''          auto setRun=[&](float run){
            class Shift final:public ICommand {
                wi::scene::Scene& s;std::vector<wi::ecs::Entity> nodes;float delta;
            public:Shift(wi::scene::Scene& scene,const std::vector<wi::ecs::Entity>& n,float run):s(scene),nodes(n),delta(10+run-scene.transforms.GetComponent(n.front())->translation_local.x){}
                bool Execute()override{for(auto e:nodes){auto* t=s.transforms.GetComponent(e);t->translation_local.x+=delta;t->SetDirty();}return true;}
                void Undo()override{for(auto e:nodes){auto* t=s.transforms.GetComponent(e);t->translation_local.x-=delta;t->SetDirty();}}
            };
            Check(nodeHistory.Execute(std::make_unique<Shift>(scene,controlNodes,run)),"Native cascade forward-offset edit");rebuildNodes();
          };
'''
assert s.count(anchor)==1;s=s.replace(anchor,shift+anchor)
anchor='const auto originalHash=built.native->DeterministicHash();setDrop(.6f);'
replace='''const auto originalHash=built.native->DeterministicHash();
            setRun(0);Check(nodeHistory.Undo(),"Vertical offset Undo");rebuildNodes();Check(built.native->DeterministicHash()==originalHash,"Vertical native Undo hash");
            setRun(.5f);Check(nodeHistory.Undo(),"Forward offset Undo");rebuildNodes();Check(built.native->DeterministicHash()==originalHash,"Forward native Undo hash");
            setDrop(.6f);'''
assert s.count(anchor)==1;s=s.replace(anchor,replace)
s=s.replace("if(key=='G'){","if(key=='J'){float run=scene.transforms.GetComponent(controlNodes.front())->translation_local.x-10;setRun(run<.02f?.5f:(run>.2f?.04f:0));}\n                          if(key=='G'){")
s=s.replace('N control spheres. U Undo; Y Redo.','J forward offset (direct / tight / sloped). N control spheres. U Undo; Y Redo.')
(out/'RiverCascadeProof.cpp').write_text(s)

s=s.replace('{wi::Archive archive((out/"native-rapids.wiscene")','{cascade_proof::ArchiveMask nativeMask(scene);wi::Archive archive((out/"native-rapids.wiscene")')
(out/'RiverCascadeProof.cpp').write_text(s)

anchor='        auto built=cascade_proof::Refresh(scene,river);'
guard='''        for(const char* name:{"Cascade upstream native WATER","Cascade falling native WATER"}){
            const auto* m=scene.materials.GetComponent(scene.Entity_FindByName(name));
            Check(m&&m->GetCustomShaderID()<0&&m->userdata.x==0&&m->userdata.y==0&&m->userdata.z==0&&m->userdata.w==0,"Transient cascade binding serialized");
        }
        std::cout<<"CASCADE_ARCHIVE_NATIVE_BINDINGS=PASS\\n";
'''
assert s.count(anchor)==1;s=s.replace(anchor,guard+anchor)
s=s.replace('if(!interactive){','if(!interactive&&std::string(argv[argc-1])!="--inspect"){')
(out/'RiverCascadeProof.cpp').write_text(s)

s=s.replace('bool showNodes=false;','bool showNodes=false;wi::ecs::Entity upstreamNodeRoot=wi::ecs::INVALID_ENTITY;')
s=s.replace('if(showNodes)for(auto e:scene->splines.GetComponent(river)->spline_node_entities)','if(showNodes)for(auto root:{river,upstreamNodeRoot})for(auto e:scene->splines.GetComponent(root)->spline_node_entities)')
s=s.replace('Path path;path.scene=&scene;path.river=river;','Path path;path.scene=&scene;path.river=river;path.upstreamNodeRoot=scene.Entity_FindByName("Cascade upstream native WATER");')
(out/'RiverCascadeProof.cpp').write_text(s)

# Diagnostic-only early exit permits inspection of float-channel error scales.
s=s.replace('if(!interactive&&std::string(argv[argc-1])!="--inspect"){\n            const auto originalHash','if(!interactive&&std::string(argv[argc-1])!="--inspect"&&std::string(argv[argc-1])!="--parity"){\n            const auto originalHash')
anchor='            Check(maxError<.0001f,"Rapids GPU CPU multi-step parity");'
diagnostic='''            float componentError[4]={},relativeError[4]={};double massError=0,massTotal=0;
            for(size_t k=0;k<actual.size();++k){for(int c=0;c<4;++c){
                float e=std::abs((&actual[k].x)[c]-(&reference[k].x)[c]);componentError[c]=std::max(componentError[c],e);
                relativeError[c]=std::max(relativeError[c],e/std::max(1.f,std::abs((&reference[k].x)[c])));
            }massError+=std::abs(actual[k].x-reference[k].x);massTotal+=std::abs(reference[k].x);}
            std::cout<<"CASCADE_PARITY_CHANNELS abs="<<componentError[0]<<","<<componentError[1]<<","<<componentError[2]<<","<<componentError[3]<<" rel="<<relativeError[0]<<","<<relativeError[1]<<","<<relativeError[2]<<","<<relativeError[3]<<" massL1="<<massError/std::max(1.,massTotal)<<"\\n"<<std::flush;
            if(std::string(argv[argc-1])=="--parity"){
                std::ofstream oracle(out/"cpu-reference.bin",std::ios::binary);oracle.write(reinterpret_cast<const char*>(reference.data()),reference.size()*sizeof(XMFLOAT4));oracle.close();
                mode=0;bind();path.GetGUI().RemoveWidget(&foamAmountSlider);path.GetGUI().RemoveWidget(&foamDefault);path.GetGUI().RemoveWidget(&label);return 0;
            }
'''
assert s.count(anchor)==1;s=s.replace(anchor,diagnostic+anchor)
(out/'RiverCascadeProof.cpp').write_text(s)

# Density/age stay absolute; carried source-coordinate moments scale with mass.
s=s.replace('Check(maxError<.0001f,"Rapids GPU CPU multi-step parity");','Check(componentError[0]<2e-5f&&componentError[1]<2e-6f&&relativeError[2]<2e-5f&&relativeError[3]<2e-5f&&massError/std::max(1.,massTotal)<5e-6,"Cascade component-scaled GPU CPU multi-step parity");')
(out/'RiverCascadeProof.cpp').write_text(s)