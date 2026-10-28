"""Optional G8D native WATER surface proof, reusing the full rapids regression."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(root/'Tools/prepare_river_bubbly_graphics_proof.py'),str(out)],check=True)
s=(out/'RiverBubblyFoamProof.cpp').read_text()
s=s.replace('#include "RiverJunctionFixture.h"','#include "RiverJunctionFixture.h"\n#include "../EngineBridge/src/RiverSurfaceMotionProof.h"')
s=s.replace('XMFLOAT4 foamSettings;};','XMFLOAT4 foamSettings;XMUINT4 motion;};').replace('sizeof(Params)==80','sizeof(Params)==96')
s=s.replace('Renegade River V2 - Bubbling Foam Review','Renegade River V2 - Surface Motion Proof')
s=s.replace('BUBBLING FOAM REVIEW / ','SURFACE MOTION PROOF / ')
s=s.replace('Bubble lighting and softer breakup preview. Flow and foam generation unchanged; no particles or free-surface waves.','Bounded kinematic surface proof; not a hydraulic wave solver. V toggles surface motion without resetting foam.')
anchor='        Path path;path.scene=&scene;path.river=river;'
assert s.count(anchor)==1
s=s.replace(anchor,'''        auto* waterObject=scene.objects.GetComponent(river);
        auto* waterMesh=scene.meshes.GetComponent(waterObject->meshID);
        Check(waterMesh!=nullptr,"Native WATER geometry missing");
        auto refined=river_surface_motion::Refine(*waterMesh);
        std::cout<<"SURFACE_NATIVE_REFINED vertices="<<refined.positions.size()<<" triangles="<<refined.indices.size()/3<<" area="<<refined.area<<"\\n";
        waterMesh->vertex_positions.assign(refined.positions.begin(),refined.positions.end());
        waterMesh->vertex_uvset_0.assign(refined.uv.begin(),refined.uv.end());
        waterMesh->vertex_uvset_1.clear();waterMesh->vertex_tangents.clear();
        waterMesh->indices.assign(refined.indices.begin(),refined.indices.end());
        waterMesh->subsets[0].indexOffset=0;waterMesh->subsets[0].indexCount=uint32_t(refined.indices.size());
        waterMesh->ComputeNormals(wi::scene::MeshComponent::COMPUTE_NORMALS_SMOOTH_FAST);waterMesh->CreateRenderData();
        // Symmetric culling guard keeps the planar-reflection mean plane unchanged.
        waterMesh->aabb._min.y-=.12f;waterMesh->aabb._max.y+=.12f;
        auto motionField=river_surface_motion::Build(*network,domain);
        std::cout<<"SURFACE_BOUND maxAmplitude="<<motionField.maximumAmplitude<<"\\n";
'''+anchor)
anchor='        Params params{'
s=s.replace(anchor,'        wi::graphics::Texture waves;upload(waves,motionField.texels);bool movingSurface=true;\n'+anchor)
s=s.replace('{1,0,0,0}};','{1,0,0,0},{descriptor(waves),1,0,0}};')
anchor='        wi::renderer::CustomShader custom;'
assert s.count(anchor)==1
s=s.replace(anchor,'''        wi::graphics::Shader surfaceVS,surfaceShadowVS;
        Check(wi::helper::FileRead((fs::absolute(argv[3])/"RiverSurfaceVS.cso").generic_u8string(),bytes),"Surface VS bytecode");
        Check(gpu->CreateShader(wi::graphics::ShaderStage::VS,bytes.data(),bytes.size(),&surfaceVS),"Surface VS");
        Check(wi::helper::FileRead((fs::absolute(argv[3])/"RiverSurfaceShadowVS.cso").generic_u8string(),bytes),"Surface shadow VS bytecode");
        Check(gpu->CreateShader(wi::graphics::ShaderStage::VS,bytes.data(),bytes.size(),&surfaceShadowVS),"Surface shadow VS");
'''+anchor)
s=s.replace('pso.vs=wi::renderer::GetShader(wi::enums::VSTYPE_OBJECT_COMMON);','pso.vs=&surfaceVS;')
s=s.replace('pso.vs=wi::renderer::GetShader(wi::enums::VSTYPE_SHADOW_TRANSPARENT);','pso.vs=&surfaceShadowVS;')
s=s.replace('params.foamSettings.x=mode>=2?foamAmount:0.f;','params.motion.y=movingSurface?1u:0u;params.foamSettings.x=mode>=2?foamAmount:0.f;')
s=s.replace('B previous/new foam   F native/flow/foam','V surface on/off   B previous/new foam   F native/flow/foam')
s=s.replace('(mode>=2?(previous?', '(movingSurface?" / WAVES ON":" / WAVES OFF")+(mode>=2?(previous?')
anchor="if(key=='R'){"
s=s.replace(anchor,'''if(key=='V'){movingSurface=!movingSurface;bind();std::cout<<"SURFACE_MOTION="<<movingSurface<<" clock="<<path.clock<<"\\n";}
                          '''+anchor)
anchor='            if(previousShader!=shader){'
s=s.replace(anchor,'''            {
                auto before=read("surface-history-before");
                pose(3);movingSurface=false;bind();capture("surface-flat");
                movingSurface=true;bind();capture("surface-moving");
                auto after=read("surface-history-after");
                Check(before.size()==after.size()&&std::memcmp(before.data(),after.data(),before.size()*sizeof(XMFLOAT4))==0,"Surface toggle changed foam history");
                std::cout<<"SURFACE_HISTORY=PASS\\n";
            }
'''+anchor)
# Flat and displaced performance use the same refined geometry and visible foam.
anchor='            const auto start=std::chrono::steady_clock::now();frames(180);'
assert s.count(anchor)==1
s=s.replace(anchor,'''            for(bool wavesOn:{false,true,false,true}){
                movingSurface=wavesOn;bind();frames(60);
                const auto begin=std::chrono::steady_clock::now();frames(240);gpu->WaitForGPU();
                double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/240;
                std::cout<<"SURFACE_BENCH waves="<<wavesOn<<" frameMS="<<ms<<" fps="<<1000/ms<<"\\n";
            }
'''+anchor)
# Disable presentation pacing for paired measurements; no production setting changes.
s=s.replace('for(bool wavesOn:{false,true,false,true}){','wi::eventhandler::FireEvent(wi::eventhandler::EVENT_SET_VSYNC,0);\n            for(bool wavesOn:{false,true,false,true}){')
s=s.replace('movingSurface=true;bind();capture("surface-moving");','movingSurface=true;bind();capture("surface-moving");\n                const float frozenClock=path.clock;path.clock+=.2f;bind();capture("surface-next-phase");path.clock=frozenClock;bind();')
# Fixed 75 Hz simulation cost with uncapped presentation, rather than fewer
# transport substeps merely because the proof renders faster than the display.
s=s.replace('movingSurface=wavesOn;bind();frames(60);','movingSurface=wavesOn;path.paused=true;wi::renderer::SetGameSpeed(0);bind();\n                auto benchFrames=[&](int count){for(int n=0;n<count;++n){path.manual=1.f/75;app.Run();}};benchFrames(60);')
s=s.replace('const auto begin=std::chrono::steady_clock::now();frames(240);gpu->WaitForGPU();','const auto begin=std::chrono::steady_clock::now();benchFrames(240);gpu->WaitForGPU();')
(out/'RiverSurfaceMotionProof.cpp').write_text(s)
print('G8D_SURFACE_HARNESS_STAGED')
