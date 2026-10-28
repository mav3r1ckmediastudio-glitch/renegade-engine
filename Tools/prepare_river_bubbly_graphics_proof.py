"""Reuse the rapids regression harness with a separate title and genuine foam benchmark."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
s=(root/'Tests/RiverRapidsProof.cpp').read_text()
s=s.replace('Renegade River V2 - Rocky Rapids Prototype','Renegade River V2 - Bubbling Foam Review')
s=s.replace('ROCKY RAPIDS / MEASURED BED DEPTH / ','BUBBLING FOAM REVIEW / ')
s=s.replace('R toggles rapid aeration. Depth-weighted steady flow prototype; free-surface waves and editor integration deferred.','Bubble lighting and softer breakup preview. Flow and foam generation unchanged; no particles or free-surface waves.')
a='            const auto start=std::chrono::steady_clock::now();frames(180);'
assert s.count(a)==1
s=s.replace(a,'''            // Measure the actual visible foam path, not the native-restored mode.
            pose(4);mode=2;path.speed=1;path.sourceMask=15;bind();clear();advance(18);
            path.paused=false;path.frozen=false;wi::renderer::SetGameSpeed(1);frames(30);
            const auto start=std::chrono::steady_clock::now();frames(180);''')

# Same-state appearance A/B; previous pixel shader is a separate optional proof input.
a='        Check(gpu->CreatePipelineState(&pso,&custom.pso[wi::enums::RENDERPASS_MAIN]),"Native WATER main PSO");'
assert s.count(a)==1;s=s.replace(a,'        const auto mainDescriptor=pso;'+chr(10)+a)
a='        const auto shader=wi::renderer::RegisterCustomShader(custom);wi::graphics::GPUBuffer buffer;'
assert s.count(a)==1
s=s.replace(a,'''        const auto shader=wi::renderer::RegisterCustomShader(custom);
        wi::graphics::Shader previousPixel;int previousShader=shader;bool previous=false;
        if(wi::helper::FileRead((fs::absolute(argv[3])/"PreviousFoamPS.cso").generic_u8string(),bytes)){
            Check(gpu->CreateShader(wi::graphics::ShaderStage::PS,bytes.data(),bytes.size(),&previousPixel),"Previous foam shader");
            auto previousCustom=custom;previousCustom.name="Previous foam comparison";
            auto desc=mainDescriptor;desc.ps=&previousPixel;
            Check(gpu->CreatePipelineState(&desc,&previousCustom.pso[wi::enums::RENDERPASS_MAIN]),"Previous foam pipeline");
            previousShader=wi::renderer::RegisterCustomShader(previousCustom);
        }
        wi::graphics::GPUBuffer buffer;''')
s=s.replace('material->SetCustomShaderID(shader);','material->SetCustomShaderID(previous?previousShader:shader);')
s=s.replace('(path.paused?" / PAUSED":" / RUNNING")','(mode>=2?(previous?" / PREVIOUS FOAM":" / BUBBLY FOAM"):"")+(path.paused?" / PAUSED":" / RUNNING")')
s=s.replace('F native/flow/foam   Space pause','B previous/new foam   F native/flow/foam   Space pause')
a='            for(int view=0;view<5;++view){pose(view);capture("rapid-view-"+std::to_string(view));}'
assert s.count(a)==1
s=s.replace(a,'''            if(previousShader!=shader){
                auto before=read("appearance-history-before");previous=true;bind();pose(3);capture("appearance-previous");
                auto after=read("appearance-history-after");
                Check(before.size()==after.size()&&std::memcmp(before.data(),after.data(),before.size()*sizeof(XMFLOAT4))==0,"Appearance A/B changed density history");
                previous=false;bind();capture("appearance-bubbly");std::cout<<"APPEARANCE_HISTORY=PASS\\n";
            }
'''+a)
a="if(key=='R'){"
assert s.count(a)==1
s=s.replace(a,'''                          if(key=='B'){
                              wi::vector<uint8_t> before,after;gpu->WaitForGPU();
                              Check(wi::helper::saveTextureToMemory(path.foam[path.state],before),"A/B history before");
                              const auto clock=path.clock;const auto state=path.state;previous=!previous;bind();
                              Check(wi::helper::saveTextureToMemory(path.foam[path.state],after),"A/B history after");
                              Check(before==after&&clock==path.clock&&state==path.state,"A/B must retain history and clock");
                              std::cout<<"APPEARANCE_AB="<<(previous?"PREVIOUS":"BUBBLY")<<" HISTORY_UNCHANGED=1\\n";
                          }
'''+a)
s=s.replace('SetTemporalAAEnabled(true);pose(2);bind();','SetTemporalAAEnabled(true);pose(3);bind();')

(out/'RiverBubblyFoamProof.cpp').write_text(s)
print('G8C_GRAPHICS_HARNESS_STAGED')
