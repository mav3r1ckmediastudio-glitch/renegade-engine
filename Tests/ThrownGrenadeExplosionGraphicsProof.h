#pragma once
#include "../Runtime/src/RuntimeGrenadeExplosionFX.h"
// GPU-only stand-alone diagnostic: compare blast morphology over time before
// asking the player to assess a new Runtime. No gameplay/project mutation.
static bool ThrownGrenadeExplosionGraphicsProof(const fs::path& output) {
    fs::create_directories(output);
    wi::scene::Scene source;
    const auto ground=source.Entity_CreateCube("Blast neutral ground plane");
    if(auto* t=source.transforms.GetComponent(ground)){
        t->Scale(XMFLOAT3{6.f,.04f,6.f});
        t->Translate(XMFLOAT3{0,-.09f,0});
        t->UpdateTransform();
    }
    renegade::studio::ModelImportPreview preview;
    std::string error;
    if(!preview.Prepare(source,error)) {
        std::cerr<<"Blast preview setup failed "<<error<<"\n";return false;
    }
    preview.camera->CreatePerspective(512,320,.005f,20.f,XMConvertToRadians(65.f));
    const XMVECTOR eye=XMVectorSet(0,1.15f,-4.7f,1);
    const XMVECTOR at=XMVectorSet(0,.36f,0,1);
    preview.camera->TransformCamera(
        XMMatrixInverse(nullptr,XMMatrixLookAtLH(eye,at,XMVectorSet(0,1,0,0))));
    preview.camera->UpdateCamera();
    const std::array<unsigned,6> frames{2,5,10,17,30,58};
    unsigned stored=0,blastFrame=0;
    bool started=false;
    for(unsigned frame=0;frame<240;++frame) {
        // Wicked compiles pipelines during the initial empty-scene frames.
        // Begin the burst only after preview resources and GPU shaders exist,
        // otherwise the proof only sees the smoke tail.
        if(!started && preview.IsReady()) {
            const auto layers=renegade::runtime::RuntimeGrenadeExplosionFX::Create(
                *preview.scene,{0,.14f,0});
            if(layers.size()!=renegade::runtime::RuntimeGrenadeExplosionFX::Count)
                return false;
            started=true;blastFrame=0;
        }
        wi::eventhandler::FireEvent(wi::eventhandler::EVENT_THREAD_SAFE_POINT,0);
        preview.PreUpdate();
        preview.Update(1.f/60.f);
        preview.PreRender();
        preview.Render();
        wi::graphics::GetDevice()->SubmitCommandLists();
        wi::renderer::UpdateGPUSuballocator();
        Sleep(12);
        if(!started||stored>=frames.size()||blastFrame<frames[stored]) {
            if(started)++blastFrame;
            continue;
        }
        std::vector<std::uint8_t> imageData;
        if(!preview.CapturePng(imageData,error)) {
            std::cerr<<"Blast GPU capture failed "<<error<<"\n";return false;
        }
        const auto file=output/("blast-"+std::to_string(blastFrame)+".png");
        std::ofstream stream(file,std::ios::binary);
        stream.write(reinterpret_cast<const char*>(imageData.data()),imageData.size());
        if(!stream.good()||imageData.size()<1000)return false;
        std::cout<<"BLAST_GPU_FRAME "<<blastFrame<<" "<<file.generic_u8string()
                 <<" bytes="<<imageData.size()<<"\n";
        ++stored;++blastFrame;
    }
    return stored==frames.size();
}
