#include "renegade/bridge/ParticleEmitterService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/MaterialTextureAssetService.h"
#include "renegade/bridge/ParticleBlendModeService.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
namespace {
int failures=0;
void Check(bool ok,const char* message) { if(!ok) {++failures;std::cerr<<"FAIL: "<<message<<std::endl;} }
LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProcW(h,m,w,l);}
}
int main(int argc,char** argv) {
 if(argc!=2)return 2;
 using namespace renegade::bridge;
 WNDCLASSEXW wc={};wc.cbSize=sizeof(wc);wc.lpfnWndProc=Proc;
 wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"ParticleDuplicateGraphicsProof";
 RegisterClassExW(&wc);
 HWND window=CreateWindowExW(0,wc.lpszClassName,L"Particle duplication proof",
     WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
 if(!window)return 3;
 {
 wi::Application app;app.allow_hdr=false;app.SetWindow(window);
 if(!wi::graphics::GetDevice())return 4;
 wi::initializer::InitializeComponentsImmediate();
    {
        wi::scene::Scene scene;
        CreateParticleEmitterCommand create(scene,XMFLOAT3(2,3,4));
        Check(create.Execute(),"duplicate fixture create");
        const auto source=create.CreatedEntity();
        auto state=CaptureParticleEmitter(scene,source);
        state.emitCount=51;state.size=0.42f;state.life=3.5f;
        state.framesX=6;state.framesY=6;state.frameCount=35;
        state.frameStart=2;state.frameRate=23;
        state.velocity=XMFLOAT3(1,2,3);state.gravity=XMFLOAT3(0,-2,0);
        state.color=XMFLOAT4(0.7f,0.4f,0.2f,0.9f);
        state.emissiveColor=XMFLOAT3(0.9f,0.3f,0.1f);state.emissiveStrength=2;
        state.sorted=true;state.frameBlending=true;
        state.opacityPeakStart=0.2f;state.opacityPeakEnd=0.7f;
        ApplyParticleEmitter(scene,source,state);
        SetParticleBlendModeCommand blend(scene,source,wi::enums::BLENDMODE_ADDITIVE);
        (void)blend.Execute();
        const auto expected=CaptureParticleEmitter(scene,source);
        // Managed texture bindings have a live GPU texture and no loose filename.
        std::ifstream input(argv[1],std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
        wi::Resource atlas=wi::resourcemanager::Load(
            "particle-managed-atlas-"+GenerateStableId()+".png",
            wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA,bytes.data(),bytes.size());
        Check(atlas.IsValid() && atlas.GetTexture().IsValid(),"real GPU atlas loaded");
        auto* originalMaterial=scene.materials.GetComponent(source);
        originalMaterial->textures[wi::scene::MaterialComponent::BASECOLORMAP].name.clear();
        originalMaterial->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource=atlas;
        const auto textureId=GenerateStableId();
        scene.metadatas.GetComponent(source)->string_values.set(
            MaterialBaseColorTextureAssetIdMetadataKey,textureId);
        bool companion=false;
        SetDuplicateEntityCompanionFactory([&](wi::scene::Scene&,wi::ecs::Entity,
            wi::ecs::Entity,DuplicateEntityCompanionCallbacks& callbacks,std::string&) {
            companion=true;callbacks.undo=[&]{companion=false;};
            callbacks.redo=[&]{companion=true;return true;};return true;
        });
        const auto verify=[&](wi::ecs::Entity entity) {
            Check(!HasParticleEmitterStateChange(expected,CaptureParticleEmitter(scene,entity)),
                "duplicate preserves all authored emitter settings");
            Check(CaptureParticleBlendMode(scene,entity)==wi::enums::BLENDMODE_ADDITIVE,
                "duplicate preserves blend mode");
            const auto* material=scene.materials.GetComponent(entity);
            const auto& texture=material->textures[wi::scene::MaterialComponent::BASECOLORMAP];
            Check(texture.name.empty() && texture.resource.IsValid() && texture.resource.GetTexture().IsValid(),
                "duplicate retains filename-free managed atlas resource");
            Check(texture.resource.IsValid() && texture.resource.GetFileData()==atlas.GetFileData(),
                "duplicate retains exact atlas payload");
            Check(scene.metadatas.GetComponent(entity)->string_values.get(
                MaterialBaseColorTextureAssetIdMetadataKey)==textureId,
                "duplicate preserves managed texture identity");
        };
        DuplicateEntityCommand duplicate(scene,source);
        Check(duplicate.Execute(),"duplicate emitter");
        const auto copy=duplicate.DuplicatedEntity();verify(copy);
        Check(companion,"duplicate script companion applied");
        Check(PersistentEntityId(scene,copy)!=PersistentEntityId(scene,source),
            "duplicate owns a different persistent entity identity");
        duplicate.Undo();Check(!IsParticleEmitter(scene,copy) && !companion,
            "duplicate undo removes emitter and companion");
        Check(duplicate.Execute(),"duplicate redo");verify(copy);
        Check(companion,"resource redo composes with script companion redo");
        ClearDuplicateEntityCompanionFactory();
        // Recursive model duplication must preserve an attached emitter's atlas.
        const auto parent=scene.Entity_CreateTransform("Brazier");
        scene.Component_Attach(source,parent,true);
        DuplicateEntityCommand parentCopy(scene,parent);
        Check(parentCopy.Execute(),"duplicate emitter parent");
        wi::ecs::Entity child=wi::ecs::INVALID_ENTITY;
        for(size_t i=0;i<scene.emitters.GetCount();++i) {
            const auto id=scene.emitters.GetEntity(i);
            if(ParticleEmitterParent(scene,id)==parentCopy.DuplicatedEntity())child=id;
        }
        Check(child!=wi::ecs::INVALID_ENTITY,"recursive duplicate contains emitter");
        if(child!=wi::ecs::INVALID_ENTITY)verify(child);
        parentCopy.Undo();Check(!IsParticleEmitter(scene,child),"recursive undo");
        Check(parentCopy.Execute(),"recursive redo");
        if(child!=wi::ecs::INVALID_ENTITY)verify(child);
    }

 while(wi::renderer::IsPipelineCreationActive()>0)Sleep(10);
 wi::graphics::GetDevice()->WaitForGPU();
 }
 DestroyWindow(window);
 if(failures)return 1;
 std::cout<<"PARTICLE DUPLICATION GRAPHICS PROOF PASS"<<std::endl;
 return 0;
}
