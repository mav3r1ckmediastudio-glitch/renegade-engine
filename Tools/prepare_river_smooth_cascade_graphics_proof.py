"""G10C single native continuous WATER surface. Prior proof sources unchanged."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(root/'Tools/prepare_river_cascade_graphics_proof.py'),str(out)],check=True)
s=(out/'RiverCascadeProof.cpp').read_text()
s=s.replace('#include "RiverCascadeFixture.h"','#include "RiverSmoothCascadeFixture.h"').replace('cascade_proof::','smooth_cascade_proof::')
s=s.replace('Renegade River V2 - Cascade','Renegade River V2 - Smooth Cascade')
s=s.replace('XMUINT4 strands;XMUINT4 artwork;','XMUINT4 strands;XMUINT4 artwork;XMFLOAT4 smooth;')
s=s.replace('sizeof(Params)==128','sizeof(Params)==144')
s=s.replace('wi::ecs::Entity upstreamNodeRoot=wi::ecs::INVALID_ENTITY;','wi::ecs::Entity upstreamNodeRoot=wi::ecs::INVALID_ENTITY,visibleWater=wi::ecs::INVALID_ENTITY;')
anchor='        for(auto id:hiddenRibbons)if(auto* o=scene->objects.GetComponent(id))o->SetRenderable(false);'
s=s.replace(anchor,'''        if(visibleWater!=wi::ecs::INVALID_ENTITY){
            auto* src=scene->materials.GetComponent(river);auto* dst=scene->materials.GetComponent(visibleWater);
            dst->userdata=src->userdata;dst->SetDirty();
        }
'''+anchor,1)
a=s.index('        auto refineWater=[&](){')
z=s.index('        auto motionField=',a)
s=s[:a]+'''        // Refresh already produced the bounded, refined, single native surface.
        auto refineWater=[&](){};
'''+s[z:]
anchor='        path.bounds=domain.mask.bounds;path.maximumRate=domain.maximumRate;'
assert s.count(anchor)==1
s=s.replace(anchor,'''        path.visibleWater=scene.Entity_FindByName(smooth_cascade_proof::connectedName);
        path.hiddenRibbons.push_back(river);
        path.hiddenRibbons.push_back(scene.Entity_FindByName("G10C receiving domain native WATER"));
        path.hiddenRibbons.push_back(scene.Entity_FindByName("Cascade upstream native WATER"));
        path.hiddenRibbons.push_back(scene.Entity_FindByName("Cascade falling native WATER"));
'''+anchor)
a=s.index('            const auto upper=scene.Entity_FindByName("Cascade upstream native WATER"),fall=')
z=s.index('            foamAmountSlider.SetEnabled',a)
s=s[:a]+'''            auto* connected=scene.materials.GetComponent(path.visibleWater);
            if(mode)connected->SetCustomShaderID(previous?previousShader:shader);else connected->DisableCustomShader();
            connected->userdata=material->userdata;connected->SetDirty();
'''+s[z:]
anchor='            if(mode){wi::graphics::GPUBufferDesc bd;'
assert s.count(anchor)==1
s=s.replace(anchor,'            params.smooth=smooth_cascade_proof::Shape(scene,river);params.smooth.w=(path.sourceMask&8)?path.speed:0;\n'+anchor)
# A fresh archive must contain native material state for the connected surface too.
s=s.replace('for(const char* name:{"Cascade upstream native WATER","Cascade falling native WATER"})',
 'for(const char* name:{"Cascade upstream native WATER","Cascade falling native WATER"})')
# No user-facing selection between cascade styles; J remains automated test coverage only.
s=s.replace('J forward offset (direct / tight / sloped). ','')
s=s.replace("if(key=='J'){float run=scene.transforms.GetComponent(controlNodes.front())->translation_local.x-10;setRun(run<.02f?.5f:(run>.2f?.04f:0));}","")
old='Check(componentError[0]<2e-5f&&componentError[1]<2e-6f&&relativeError[2]<2e-5f&&relativeError[3]<2e-5f&&massError/std::max(1.,massTotal)<5e-6,"Cascade component-scaled GPU CPU multi-step parity");'
new='''double momentError[2]={},momentTotal[2]={};float centroidError=0;
            for(size_t k=0;k<actual.size();++k)for(int c=2;c<4;++c){
                momentError[c-2]+=std::abs((&actual[k].x)[c]-(&reference[k].x)[c]);
                momentTotal[c-2]+=std::abs((&reference[k].x)[c]);
                if(std::min(actual[k].x,reference[k].x)>1e-4f)
                    centroidError=std::max(centroidError,std::abs((&actual[k].x)[c]/actual[k].x-(&reference[k].x)[c]/reference[k].x));
            }
            std::cout<<"SMOOTH_POSITION_PARITY metres="<<centroidError<<" momentL1="<<momentError[0]/std::max(1.,momentTotal[0])<<","<<momentError[1]/std::max(1.,momentTotal[1])<<"\\n";
            Check(componentError[0]<2e-5f&&componentError[1]<2e-6f&&massError/std::max(1.,massTotal)<5e-6&&
                centroidError<.001f&&momentError[0]/std::max(1.,momentTotal[0])<5e-6&&momentError[1]/std::max(1.,momentTotal[1])<5e-6,
                "Smooth density/age/position GPU CPU parity");'''
assert s.count(old)==1
s=s.replace(old,new)
(out/'RiverSmoothCascadeProof.cpp').write_text(s)
print('SMOOTH_CONNECTED_NATIVE_HARNESS_STAGED')
