"""Optional G8E transported strand proof; accepted bulk simulation stays exact."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(root/'Tools/prepare_river_surface_graphics_proof.py'),str(out)],check=True)
s=(out/'RiverSurfaceMotionProof.cpp').read_text()
s=s.replace('Renegade River V2 - Surface Motion Proof','Renegade River V2 - Advected Foam Strands')
s=s.replace('SURFACE MOTION PROOF / ','ADVECTED FOAM STRANDS / ')
s=s.replace('XMUINT4 motion;};','XMUINT4 motion;XMUINT4 strands;};').replace('sizeof(Params)==96','sizeof(Params)==112')
s=s.replace('wi::graphics::Texture foam[2],flux,source,velocity;wi::graphics::Shader simulation;','''wi::graphics::Texture foam[2],flux,source,velocity;wi::graphics::Shader simulation;
    wi::graphics::Texture detail[2],detailForward;wi::graphics::Shader detailShader;
    unsigned detailState=0,detailStart=0,detailSteps=0;float detailStep=0;bool detailSimulate=true;''')
s=s.replace('start=state;step=steps?total/steps:0;clock+=total;state^=steps&1;','''start=state;step=steps?total/steps:0;clock+=total;state^=steps&1;
        detailStart=detailState;detailSteps=detailSimulate&&total>0?unsigned(std::ceil(total/.025f)):0;detailStep=detailSteps?total/detailSteps:0;detailState^=detailSteps&1;''')
s=s.replace('m->userdata.z=state;','m->userdata.z=state|(detailState<<1);')
anchor='        wi::RenderPath3D::Render();'
assert s.count(anchor)==1
s=s.replace(anchor,'''        if(detailStep>0){
            using namespace wi::graphics;auto* gpu=GetDevice();auto cmd=gpu->BeginCommandList();wi::renderer::BindCommonResources(cmd);
            const auto& desc=detail[0].GetDesc();
            gpu->BindComputeShader(&detailShader,cmd);
            unsigned currentDetail=detailStart;
            for(unsigned detailIteration=0;detailIteration<detailSteps;++detailIteration){
            gpu->BindResource(&detail[currentDetail],0,cmd);gpu->BindResource(&detail[currentDetail],1,cmd);
            gpu->BindResource(&source,2,cmd);gpu->BindResource(&velocity,3,cmd);gpu->BindResource(&foam[state],4,cmd);
            for(unsigned pass=0;pass<2;++pass){
                const auto* target=pass==0?&detailForward:&detail[1-currentDetail];
                auto barrier=GPUBarrier::Image(target,ResourceState::SHADER_RESOURCE,ResourceState::UNORDERED_ACCESS);gpu->Barrier(&barrier,1,cmd);
                gpu->BindUAV(target,0,cmd);
                Push push{{desc.width,desc.height,pass,sourceMask},bounds,{detailStep,clock-(detailSteps-1-detailIteration)*detailStep,6,speed}};
                gpu->PushConstants(&push,sizeof(push),cmd);gpu->Dispatch((desc.width+7)/8,(desc.height+7)/8,1,cmd);
                barrier=GPUBarrier::Image(target,ResourceState::UNORDERED_ACCESS,ResourceState::SHADER_RESOURCE);gpu->Barrier(&barrier,1,cmd);
                // Forward field becomes readable only after its dispatch/barrier.
                if(pass==0)gpu->BindResource(&detailForward,1,cmd);
            }currentDetail^=1;
            }
        }
'''+anchor)
anchor='        Params params{'
s=s.replace(anchor,'''        auto clearDetails=[&](){
            std::vector<float> blank(1024*512);
            wi::graphics::TextureDesc td;td.width=1024;td.height=512;td.format=wi::graphics::Format::R32_FLOAT;
            td.layout=wi::graphics::ResourceState::SHADER_RESOURCE;
            td.bind_flags=wi::graphics::BindFlag::SHADER_RESOURCE|wi::graphics::BindFlag::UNORDERED_ACCESS;
            wi::graphics::SubresourceData init;init.data_ptr=blank.data();init.row_pitch=1024*sizeof(float);init.slice_pitch=init.row_pitch*512;
            for(auto* texture:{&path.detail[0],&path.detail[1],&path.detailForward})Check(gpu->CreateTexture(&td,&init,texture),"Detail texture");
            path.detailState=0;path.detailStart=0;
        };clearDetails();
'''+anchor)
s=s.replace('{descriptor(waves),1,0,0}};','{descriptor(waves),1,0,0},{descriptor(path.detail[0]),descriptor(path.detail[1]),0,0}};')
anchor='        wi::renderer::CustomShader custom;'
s=s.replace(anchor,'''        Check(wi::helper::FileRead((fs::absolute(argv[3])/"RiverFoamStrandsCS.cso").generic_u8string(),bytes),"Strands CS bytecode");
        Check(gpu->CreateShader(wi::graphics::ShaderStage::CS,bytes.data(),bytes.size(),&path.detailShader),"Strands CS");
'''+anchor)
s=s.replace('0x52464c35,path.state,Bits(path.clock)','0x52464c35,path.state|(path.detailState<<1),Bits(path.clock)')
s=s.replace('params.foamControl.x=descriptor(path.foam[0]);','clearDetails();params.strands.x=descriptor(path.detail[0]);params.strands.y=descriptor(path.detail[1]);params.foamControl.x=descriptor(path.foam[0]);')
s=s.replace('float foamAmount=1;','float foamAmount=.3f;').replace('Create(0,4,1,400','Create(0,4,.3f,400').replace('Default (1x)','Default (0.3x)').replace('setFoamAmount(1);','setFoamAmount(.3f);')
s=s.replace('BUBBLY FOAM','ADVECTED STRANDS')
s=s.replace('Bounded kinematic surface proof; not a hydraulic wave solver. V toggles surface motion without resetting foam.','Source-born detail follows actual flow; same displaced WATER. B compares broad coverage with transported strands.')
# Check detail positivity, native-fluid exclusion and dynamic range alongside bulk GPU readbacks.
anchor='            std::cout<<"GPU_STATE="<<name'
assert s.count(anchor)==1
s=s.replace(anchor,'''            {
                wi::vector<uint8_t> detailRaw;Check(wi::helper::saveTextureToMemory(path.detail[path.detailState],detailRaw),"Detail readback");
                Check(detailRaw.size()==1024*512*sizeof(float),"Detail readback size");
                std::ofstream details(out/(name+"-detail.bin"),std::ios::binary);details.write(reinterpret_cast<const char*>(detailRaw.data()),detailRaw.size());
                const auto* field=reinterpret_cast<const float*>(detailRaw.data());float minimum=1,maximum=0;
                for(int z=0;z<512;++z)for(int x=0;x<1024;++x){float value=field[z*1024+x];
                    Check(std::isfinite(value)&&value>=0&&value<=1,"Finite bounded advected tracer");
                    int coarse=(z/2)*int(domain.mask.width)+x/2;
                    Check(domain.index[coarse]>=0||value==0,"Tracer outside native fluid");
                    minimum=std::min(minimum,value);maximum=std::max(maximum,value);
                }
                std::cout<<"STRAND_STATE="<<name<<" range="<<minimum<<","<<maximum<<"\\n";
                if(name=="rapid-only-established")Check(maximum>.4f,"Source-born tracer failed to establish");
            }
'''+anchor)
# Performance A/B includes both the changed PS and its two detail compute passes.
s=s.replace('for(bool wavesOn:{false,true,false,true}){','for(bool detailOn:{false,true,false,true}){')
s=s.replace('movingSurface=wavesOn;path.paused=true;','movingSurface=true;previous=!detailOn;path.detailSimulate=detailOn;path.paused=true;')
s=s.replace('"SURFACE_BENCH waves="<<wavesOn','"STRAND_BENCH enabled="<<detailOn')
# Capture deterministic in-motion sequence without expensive 256-frame capture alignment.
s=s.replace('wi::eventhandler::FireEvent(wi::eventhandler::EVENT_SET_VSYNC,0);',"""{
                mode=2;previous=false;movingSurface=true;path.sourceMask=15;path.speed=1;path.paused=true;wi::renderer::SetGameSpeed(0);clear();advance(18);pose(3);
                read("motion-start");
                for(int shot=0;shot<16;++shot){
                    if(shot)advance(.1f);frames(1);gpu->WaitForGPU();
                    Check(wi::helper::saveTextureToFile(path.GetRenderResult3D(),(out/("motion-"+std::to_string(shot)+".png")).generic_u8string()),"Motion sequence capture");
                }
                read("motion-end");
            }
            wi::eventhandler::FireEvent(wi::eventhandler::EVENT_SET_VSYNC,0);""")
(out/'RiverFoamStrandsProof.cpp').write_text(s)
print('G8E_STRANDS_HARNESS_STAGED')
