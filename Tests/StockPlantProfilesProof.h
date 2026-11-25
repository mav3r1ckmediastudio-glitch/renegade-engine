#pragma once
#include "renegade/bridge/GroundPaint.h"

// Runs inside the registered grass-palette native proof, after its Custom checks.
namespace stock_plant_profiles_proof
{
    using namespace renegade::bridge;
    namespace fs=std::filesystem;
    inline void Check(bool ok,const std::string& message)
    {if(!ok)throw std::runtime_error(message);}
    inline wi::ecs::Entity Find(const wi::scene::Scene& scene,const GrassAtlas& atlas)
    {
        for(size_t i=0;i<scene.hairs.GetCount();++i)
        {
            const auto e=scene.hairs.GetEntity(i);const auto* m=scene.materials.GetComponent(e);
            if(m && fs::u8path(m->textures[wi::scene::MaterialComponent::BASECOLORMAP].name).filename()==fs::u8path(atlas.path).filename())return e;
        }
        return wi::ecs::INVALID_ENTITY;
    }
    inline void Run(wi::scene::Scene& scene,wi::terrain::Terrain& terrain,const fs::path& root,
        wi::RenderPath3D& render,bool hosted)
    {
        const float heights[]={.5f,.5f,.5f,.25f,.45f,.65f,.12f,.30f};
        const float densities[]={50.f/3,25.f/3,50.f/3,25.f/3,25.f/3,5,20.f/3,4.f/3};
        std::vector<GrassAtlas> atlases(StockGrassPresetCount);std::string error;
        const auto shared=CaptureWickedVegetationSettings(terrain);
        const size_t originalHairCount=scene.hairs.GetCount();
        std::vector<uint32_t> counts;
        for(unsigned i=0;i<StockGrassPresetCount;++i)
        {
            Check(BuildStockGrassAtlas(root,static_cast<StockGrassPreset>(i),atlases[i],error),error);
            const auto settings=CaptureWickedVegetationSettings(scene,terrain,atlases[i]);
            Check(std::abs(settings.length-heights[i])<.00001f && std::abs(settings.density-densities[i])<.00001f,"Wrong stock plant defaults");
            VegetationStrokeState before;
            const auto result=PaintWickedVegetation(scene,terrain,{0,0,4},5,VegetationBrushMode::Paint,&before,atlases[i]);
            Check(result.changed,"Stock plant paint failed: "+result.error);
            FinalizeWickedVegetationStroke(scene,terrain,before);
            auto after=CaptureWickedVegetationAfter(scene,terrain,before);
            WickedVegetationStrokeCommand command(scene,terrain.terrainEntity,before,after);
            command.Undo();Check(Find(scene,atlases[i])==wi::ecs::INVALID_ENTITY,"Stock paint Undo left a plant");
            Check(command.Execute(),"Stock paint Redo failed");
            Check(scene.hairs.GetCount()==originalHairCount+i+1,"Stock planting replaced another species");
            for(unsigned j=0;j<=i;++j)
            {
                const auto e=Find(scene,atlases[j]);const auto* h=scene.hairs.GetComponent(e);const auto* m=scene.materials.GetComponent(e);
                Check(h&&m&&h->strandCount>0&&h->atlas_rects.size()==6&&h->segmentCount==2&&h->billboardCount==2,"Stock geometry missing");
                Check(std::abs(h->length-heights[j])<.00001f && std::abs(h->width-.9f)<.00001f,"One plant resized another");
                Check(std::abs(m->alphaRef-.5f)<.00001f&&std::abs(m->roughness-.9f)<.00001f&&std::abs(m->reflectance-.02f)<.00001f&&m->GetEmissiveStrength()==0,"Approved material changed");
                Check(std::abs(m->baseColor.x-((j==1||j==2)?.670896f:1.f))<.00001f,"Approved colour changed");
                if(j<i)Check(h->strandCount==counts[j],"Adding a species changed existing density");
                else counts.push_back(h->strandCount);
            }
        }
        Check(atlases[1].path!=atlases[2].path,"Full and Very Full must keep independent densities");
        const auto cloverBefore=CaptureWickedVegetationSettings(scene,terrain,atlases[6]);auto cloverAfter=cloverBefore;
        cloverAfter.length=.18f;cloverAfter.density*=.5f;
        SetWickedVegetationSettingsCommand edit(scene,terrain.terrainEntity,cloverBefore,cloverAfter,atlases[6]);
        Check(edit.Execute(),"Clover settings failed");
        const auto clover=Find(scene,atlases[6]);
        Check(std::abs(scene.hairs.GetComponent(clover)->length-.18f)<.00001f,"Clover height not applied");
        for(unsigned i=0;i<StockGrassPresetCount;++i)if(i!=6)
            Check(scene.hairs.GetComponent(Find(scene,atlases[i]))->strandCount==counts[i],"Clover settings changed another plant");
        edit.Undo();Check(scene.hairs.GetComponent(clover)->strandCount==counts[6],"Clover settings Undo lost density");
        Check(edit.Execute(),"Clover settings Redo failed");edit.Undo();
        auto custom=shared;custom.length=.83f;custom.density=2;
        ApplyWickedVegetationSettings(scene,terrain,custom);
        for(unsigned i=0;i<StockGrassPresetCount;++i)
        {
            const auto* h=scene.hairs.GetComponent(Find(scene,atlases[i]));
            Check(std::abs(h->length-heights[i])<.00001f && h->strandCount==counts[i],"Custom settings changed stock plants");
        }
        ApplyWickedVegetationSettings(scene,terrain,shared);
        Check(!terrain.materialEntities.empty(),"Missing ground base material");
        const auto grounds=GroundPalette(root);Check(!grounds.empty(),"Missing ground palette fixture");
        const auto material=ResolveGroundMaterial(scene,grounds.front(),root,error);
        Check(material!=wi::ecs::INVALID_ENTITY,error);
        auto base=SetGroundBase(scene,terrain.terrainEntity,material,terrain.materialEntities[1],{});
        Check(base->Execute(),"Ground base fixture failed");
        auto coverage=SetGroundSettings(scene,terrain.terrainEntity,material,4,0);
        Check(coverage->Execute(),"Ground coverage edit failed");
        for(const auto& atlas:atlases)Check(scene.hairs.GetComponent(Find(scene,atlas))->strandCount==0,"Ground coverage did not hide stock plants");
        coverage->Undo();
        for(unsigned i=0;i<StockGrassPresetCount;++i)
            Check(scene.hairs.GetComponent(Find(scene,atlases[i]))->strandCount==counts[i],"Ground coverage restore changed a species density");
        base->Undo();
        VegetationStrokeState eraseBefore;
        Check(PaintWickedVegetation(scene,terrain,{0,0,4},5,VegetationBrushMode::Delete,&eraseBefore,{}).changed,"Stock eraser failed");
        FinalizeWickedVegetationStroke(scene,terrain,eraseBefore);
        auto eraseAfter=CaptureWickedVegetationAfter(scene,terrain,eraseBefore);
        WickedVegetationStrokeCommand eraser(scene,terrain.terrainEntity,eraseBefore,eraseAfter);
        for(const auto& atlas:atlases)Check(Find(scene,atlas)==wi::ecs::INVALID_ENTITY,"Eraser missed a stock species");
        eraser.Undo();
        for(unsigned i=0;i<StockGrassPresetCount;++i)
            Check(scene.hairs.GetComponent(Find(scene,atlases[i]))->strandCount==counts[i],"Erase Undo lost stock density");
        const auto path=(root/"Content/Scenes/StockProfiles.wiscene").generic_u8string();
        {wi::Archive a(path,false,false);scene.Serialize(a);Check(a.SaveFile(path),"Stock profiles save failed");}
        SceneService cold;Check(cold.LoadScene(path),cold.LastError());auto& reopened=cold.GetScene();
        render.scene=&reopened;
        for(int frame=0;frame<(hosted?3:30);++frame)
        {render.PreUpdate();render.Update(1.f/60);render.PreRender();render.Render();wi::graphics::GetDevice()->SubmitCommandLists();wi::renderer::UpdateGPUSuballocator();}
        wi::graphics::GetDevice()->WaitForGPU();
        for(unsigned i=0;i<StockGrassPresetCount;++i)
        {
            const auto e=Find(reopened,atlases[i]);const auto* h=reopened.hairs.GetComponent(e);
            Check(h&&std::abs(h->length-heights[i])<.00001f&&h->strandCount==counts[i]&&!h->indices.empty(),"Cold/native update changed stock geometry or density");
            Check(std::abs(CaptureWickedVegetationSettings(reopened,reopened.terrains[0],atlases[i]).density-densities[i])<.00001f,"Cold reopen lost stock profile");
        }
        Check(wi::helper::saveTextureToFile(render.GetRenderResult3D(),(root/"stock-profiles.png").generic_u8string()),"Stock profiles capture failed");
        render.scene=&scene;
        std::cout<<"STOCK_EIGHT_PLANTS_OVERLAP_SETTINGS_GROUND_HISTORY_REOPEN_PASS"<<std::endl;
    }
}
