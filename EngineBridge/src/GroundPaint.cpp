#include "renegade/bridge/GroundPaint.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "renegade/bridge/CreatorSurfaceBuilderService.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>

namespace renegade::bridge
{
    namespace fs = std::filesystem;
    using Entity = wi::ecs::Entity;
    using Material = wi::scene::MaterialComponent;
    namespace
    {
        constexpr char Coverage[]="renegade.ground.coverage";
        constexpr char Tile[]="renegade.ground.tile_m";
        constexpr char Rule[]="renegade.ground.slope_start";
        constexpr char End[]="renegade.ground.slope_full";
        constexpr char High[]="renegade.ground.height_enabled";
        constexpr char HighStart[]="renegade.ground.height_start";
        constexpr char HighFull[]="renegade.ground.height_full";
        constexpr char Authored[]="renegade.ground.original_grass";
        float Smooth(float v) { v=std::clamp(v,0.f,1.f); return v*v*(3-2*v); }
        wi::terrain::Chunk Coord(int x,int z) { wi::terrain::Chunk c; c.x=x;c.z=z;return c; }
        bool Image(const fs::path& p)
        {
            auto ext=p.extension().u8string();
            std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});
            return ext==".png" || ext==".jpg" || ext==".tga" || ext==".dds";
        }
        fs::path Companion(const fs::path& colour,const std::string& name,std::initializer_list<const char*> suffixes)
        {
            for(auto suffix:suffixes)for(auto ext:{".png",".jpg",".tga",".dds"})
            {
                auto path=colour.parent_path()/fs::u8path(name+suffix+ext);
                if(fs::is_regular_file(path))return path;
            }
            return {};
        }
        GroundMaterial Discover(const fs::path& colour)
        {
            std::string name=colour.stem().u8string();
            for(const std::string suffix:{"_basecolor","_BaseColor","_albedo","_Albedo","_ALB","_color"})
                if(name.size()>suffix.size() && name.compare(name.size()-suffix.size(),suffix.size(),suffix)==0)
                {name.resize(name.size()-suffix.size()); break;}
            GroundMaterial result{name,colour};
            result.normal=Companion(colour,name,{"_normal_dx","_NormalDX","_nor","_normal","_NRM"});
            result.surface=Companion(colour,name,{"_surface","_orm","_ORM","_ARM"});
            return result;
        }
        bool PublishMaterial(const fs::path& project,const GroundMaterial& input,const fs::path& folder,
            GroundMaterial& output,std::string& error)
        {
            output=input;
            std::vector<ProjectDocumentWrite> writes;
            auto collect=[&](fs::path& source,const char* suffix)
            {
                if(source.empty()) return true;
                const auto dest=folder/fs::u8path(input.name+suffix+source.extension().u8string());
                if(source.lexically_normal()!=dest.lexically_normal())
                {
                    std::ifstream file(source,std::ios::binary);
                    if(!file){error="Cannot open "+source.u8string();return false;}
                    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
                    if(bytes.empty() || file.bad()){error="Cannot read "+source.u8string();return false;}
                    // Identical project-owned sources can be reused.
                    std::ifstream existing(dest,std::ios::binary);
                    if(!existing || std::vector<uint8_t>(std::istreambuf_iterator<char>(existing),{})!=bytes)
                        writes.push_back({dest.generic_u8string(),std::move(bytes)});
                }
                source=dest;
                return true;
            };
            if(!collect(output.colour,"_basecolor")||!collect(output.normal,"_normal")||!collect(output.surface,"_surface"))return false;
            if(writes.empty())return true;
            ProjectDocumentTransactionOptions opts;
            opts.allowedRoot=project.generic_u8string();opts.journalDirectory=(project/"Intermediate/Transactions").generic_u8string();
            const auto result=ProjectDocumentTransaction{}.Execute(std::move(writes),opts);
            if(!result.success)error=result.message;
            return result.success;
        }
        void Upload(wi::terrain::Terrain& t,wi::terrain::ChunkData& c)
        {
            c.blendmap={}; t.CreateChunkRegionTexture(c);
            if(c.vt)c.vt->invalidate();
            RefreshGroundGrass(*t.scene,t,c);
        }
        auto& Metadata(wi::scene::Scene& s,Entity e)
        {
            auto* m=s.metadatas.GetComponent(e);return m?*m:s.metadatas.Create(e);
        }
    }
    std::vector<GroundMaterial> GroundPalette(const fs::path& project)
    {
        const auto root=fs::u8path(wi::helper::GetExecutablePath()).parent_path()/"Content/terrain/ground";
        struct Stock {const char* id;const char* name;float grass;};
        constexpr Stock stock[]={{"base","Meadow",1},{"ground","Forest floor",.85f},{"ground2","Earth",.2f},
            {"low_altitude","Sand",.05f},{"slope","Cliff stone",0},{"rock","Weathered rock",0},
            {"darkrock","Dark rock",0},{"high_altitude","Snow",.05f},{"pavingstone","Paving stones",0},{"bricks","Brick path",0}};
        std::vector<GroundMaterial> result;
        for(auto& s:stock)result.push_back({s.name,root/(std::string(s.id)+".jpg"),root/(std::string(s.id)+"_nor.jpg"),{},s.grass});
        auto gravel=root.parent_path()/"riverbed/rocky_rugged";
        result.back()={"Rocky gravel",gravel/"rocky_basecolor.png",gravel/"rocky_normal_dx.png",gravel/"rocky_surface.png",0.f,3.f};
        std::error_code ec;
        auto custom=project/GroundCustomFolder;
        fs::create_directories(custom,ec);
        for(fs::directory_iterator it(custom,ec),end; !ec && it!=end; it.increment(ec))
        {
            if(!it->is_directory(ec))continue;
            for(fs::directory_iterator entry(it->path(),ec); !ec && entry!=end; entry.increment(ec))
            {
                auto& p=entry->path();
                if(Image(p) && p.stem().u8string().find("_basecolor")!=std::string::npos)
                    result.push_back(Discover(p));
            }
        }
        std::sort(result.begin()+std::size(stock),result.end(),[](const auto& a,const auto& b){return a.name<b.name;});
        return result;
    }
    bool ImportGroundMaterial(const fs::path& project,const fs::path& path,GroundMaterial& output,std::string& error)
    {
        if(!Image(path)){error="Choose a ground colour texture.";return false;}
        auto input=Discover(path);
        for(const auto& file:{input.colour,input.normal,input.surface})
            if(!file.empty() && !wi::resourcemanager::Load(file.generic_u8string()).IsValid())
            {error="Could not decode "+file.filename().u8string();return false;}
        auto rough=Companion(path,input.name,{"_roughness","_rough","_RGH"});
        auto metal=Companion(path,input.name,{"_metalness","_metallic","_MET"});
        auto ao=Companion(path,input.name,{"_ao","_occlusion","_AmbientOcclusion"});
        fs::path packed;
        if(!rough.empty() || !metal.empty() || !ao.empty())
        {
            packed=project/"Intermediate"/("ground-surface-"+std::to_string(wi::ecs::CreateEntity())+".png");
            CreatorSurfaceBuildRequest request;
            request.roughnessPath=rough.u8string();request.metalnessPath=metal.u8string();request.occlusionPath=ao.u8string();
            request.surfacePath=input.surface.u8string();request.outputPath=packed.u8string();request.defaultRoughness=230;
            const auto built=BuildCreatorSurfaceMap(request);
            if(!built.succeeded){error=built.error;return false;}
            input.surface=packed;
        }
        const bool ok=PublishMaterial(project,input,project/GroundCustomFolder/fs::u8path(input.name),output,error);
        if(!packed.empty()){std::error_code ec;fs::remove(packed,ec);}
        return ok;
    }
    namespace
    {
        bool IsStock(const GroundMaterial& source)
        { return source.colour.parent_path().filename()=="ground" || source.colour.parent_path().filename()=="rocky_rugged"; }
        std::string MaterialName(const GroundMaterial& source)
        { return std::string("Ground / ")+(IsStock(source)?"Stock/":"Custom/")+source.name; }
    }
    Entity FindGroundMaterial(const wi::scene::Scene& s,const GroundMaterial& source)
    {
        const auto name=MaterialName(source);
        for(size_t i=0;i<s.materials.GetCount();++i)
        {
            const auto id=s.materials.GetEntity(i);const auto* n=s.names.GetComponent(id);
            if(n && n->name==name)return id;
        }
        return wi::ecs::INVALID_ENTITY;
    }
    void ReadGroundSettings(const wi::scene::Scene& s,GroundMaterial& m)
    {
        const auto* meta=s.metadatas.GetComponent(FindGroundMaterial(s,m));
        if(meta && meta->float_values.has(Tile))
        {m.tileMetres=meta->float_values.get(Tile);m.grassCoverage=meta->float_values.get(Coverage);}
    }
    Entity ResolveGroundMaterial(wi::scene::Scene& s,const GroundMaterial& source,const fs::path& project,std::string& error)
    {
        const auto existing=FindGroundMaterial(s,source);
        if(existing!=wi::ecs::INVALID_ENTITY)return existing;
        auto folder=IsStock(source)?project/"Content/Terrain/Materials/Stock"/fs::u8path(source.name):source.colour.parent_path();
        const auto identity=MaterialName(source);
        GroundMaterial m;
        if(!PublishMaterial(project,source,folder,m,error))return wi::ecs::INVALID_ENTITY;
        Material material;
        const auto bind=[&](Material::TEXTURESLOT slot,const fs::path& p)
        {
            if(p.empty())return true;
            auto& tex=material.textures[slot];tex.name=p.generic_u8string();tex.resource=wi::resourcemanager::Load(tex.name);
            return tex.resource.IsValid() && tex.resource.GetTexture().IsValid();
        };
        if(!bind(Material::BASECOLORMAP,m.colour)||!bind(Material::NORMALMAP,m.normal)||!bind(Material::SURFACEMAP,m.surface))
        {error="A ground material texture could not be loaded.";return wi::ecs::INVALID_ENTITY;}
        material.SetRoughness(m.surface.empty()?.9f:1.f);material.SetMetalness(m.surface.empty()?0.f:1.f);
        material.SetOcclusionEnabled_Primary(true);
        material.texMulAdd={std::clamp(m.tileMetres,.25f,64.f),1,0,-2718};
        material.CreateRenderData();material.SetDirty(false);
        auto id=wi::ecs::CreateEntity();s.materials.Create(id)=std::move(material);s.names.Create(id)=identity;
        auto& meta=Metadata(s,id);meta.float_values.set(Coverage,std::clamp(m.grassCoverage,0.f,1.f));
        meta.float_values.set(Tile,std::clamp(m.tileMetres,.25f,64.f));
        return id;
    }
    std::vector<float> AuthoredPlantLengths(const wi::scene::Scene& s,Entity id,const wi::HairParticleSystem& hair)
    {
        auto* meta=s.metadatas.GetComponent(id);
        if(meta && meta->string_values.has(Authored))
        {
            const auto& text=meta->string_values.get(Authored);
            // Palette brush masks are binary. This packed snapshot is independent of terrain coverage.
            if(text.size()==(hair.vertex_lengths.size()+3)/4)
            {
                std::vector<float> out(hair.vertex_lengths.size());
                for(size_t i=0;i<out.size();++i)out[i]=((text[i/4]-'A')>>(i%4))&1;
                return out;
            }
        }
        return hair.vertex_lengths;
    }
    void SetAuthoredPlantLengths(wi::scene::Scene& s,Entity id,const std::vector<float>& lengths)
    {
        std::string text((lengths.size()+3)/4,'A');
        for(size_t i=0;i<lengths.size();++i)if(lengths[i]>0)text[i/4]+=char(1u<<(i%4));
        Metadata(s,id).string_values.set(Authored,text);
    }
    void RefreshGroundGrass(wi::scene::Scene& s,wi::terrain::Terrain& t,wi::terrain::ChunkData& c)
    {
        if(c.blendmap_layers.empty() || !s.hairs.GetCount())return;
        std::vector<float> weights;
        for(size_t layer=0;layer<c.blendmap_layers.size() && layer<t.materialEntities.size();++layer)
        {
            auto* meta=s.metadatas.GetComponent(t.materialEntities[layer]);
            const float coverage=meta && meta->float_values.has(Coverage)?meta->float_values.get(Coverage):1.f;
            if(weights.empty() && coverage==1)continue;
            if(weights.empty())weights.assign(wi::terrain::vertexCount,1.f);
            auto& mask=c.blendmap_layers[layer].pixels;
            if(mask.size()!=weights.size())continue;
            for(size_t i=0;i<weights.size();++i)weights[i]+=(coverage-weights[i])*mask[i]/255.f;
        }
        for(size_t h=0;h<s.hairs.GetCount();++h)
        {
            auto& hair=s.hairs[h];const auto id=s.hairs.GetEntity(h);
            if(hair.meshID!=c.entity || hair.vertex_lengths.size()!=wi::terrain::vertexCount)continue;
            auto* meta=s.metadatas.GetComponent(id);
            const bool palette=meta && meta->int_values.has("renegade.grass.atlas_frames.v1");
            if(!palette && (id!=c.grass_entity || c.grass.vertex_lengths.size()!=hair.vertex_lengths.size()))continue;
            if(weights.empty() && (!meta || !meta->string_values.has(Authored)) && id!=c.grass_entity)continue;
            auto authored=palette?AuthoredPlantLengths(s,id,hair):c.grass.vertex_lengths;
            if(palette && !meta->string_values.has(Authored))SetAuthoredPlantLengths(s,id,authored);
            size_t count=0;
            for(size_t i=0;i<authored.size();++i)
            {
                // Stable world grid noise makes fractional coverage remove strands and restores the same patch on erase.
                const int x=int(std::lround(c.position.x/t.chunk_scale))+int(i%wi::terrain::chunk_width);
                const int z=int(std::lround(c.position.z/t.chunk_scale))+int(i/wi::terrain::chunk_width);
                uint32_t hash=uint32_t(x)*73856093u ^ uint32_t(z)*19349663u;
                hash^=hash>>16;hash*=0x7feb352du;hash^=hash>>15;
                if(!weights.empty() && (hash&65535u)+.5f>=weights[i]*65536.f)authored[i]=0;
                if(authored[i]>0)++count;
            }
            const auto strands=uint32_t(float(count)*3*t.chunk_scale*t.chunk_scale*t.grass_density);
            if(hair.vertex_lengths!=authored || hair.strandCount!=strands)
            {
                hair.vertex_lengths=std::move(authored);hair.strandCount=strands;
                hair._flags|=wi::HairParticleSystem::REBUILD_BUFFERS;hair.SetDirty();
            }
        }
    }
    GroundRules ReadGroundRules(const wi::scene::Scene& s,Entity terrain)
    {
        GroundRules r;auto* m=s.metadatas.GetComponent(terrain);
        if(m && m->float_values.has(Rule))
        {
            r.rockStart=m->float_values.get(Rule);r.rockFull=m->float_values.get(End);
            r.highEnabled=m->bool_values.get(High);r.highStart=m->float_values.get(HighStart);r.highFull=m->float_values.get(HighFull);
        }
        return r;
    }
    void RefreshGroundSlope(wi::terrain::Terrain& t,wi::terrain::ChunkData& c,const wi::scene::MeshComponent& mesh)
    {
        auto* meta=t.scene?t.scene->metadatas.GetComponent(t.terrainEntity):nullptr;
        if(!meta || !meta->float_values.has(Rule) || c.blendmap_layers.size()<4 ||
            mesh.vertex_normals.size()!=wi::terrain::vertexCount)return;
        auto r=ReadGroundRules(*t.scene,t.terrainEntity);
        for(size_t i=0;i<wi::terrain::vertexCount;++i)
        {
            const float angle=DirectX::XMConvertToDegrees(std::acos(std::clamp(mesh.vertex_normals[i].y,0.f,1.f)));
            float rock=Smooth((angle-r.rockStart)/std::max(.01f,r.rockFull-r.rockStart));
            if(r.highEnabled)rock=std::max(rock,Smooth((mesh.vertex_positions[i].y-r.highStart)/std::max(.01f,r.highFull-r.highStart)));
            c.blendmap_layers[0].pixels[i]=255;c.blendmap_layers[1].pixels[i]=uint8_t(std::lround(255*rock));
            c.blendmap_layers[2].pixels[i]=c.blendmap_layers[3].pixels[i]=0;
        }
        c.blendmap={}; if(c.vt)c.vt->invalidate();
    }
    GroundStroke::GroundStroke(wi::scene::Scene& scene,Entity terrain,Entity material,GroundTool tool,
        float radius,float strength,float softness):scene_(scene),terrain_(terrain),material_(material),tool_(tool),
        radius_(std::max(.1f,radius)),strength_(std::clamp(strength,0.f,1.f)),softness_(std::clamp(softness,0.f,1.f)){}
    bool GroundStroke::Dab(const XMFLOAT3& point)
    {
        auto* t=scene_.terrains.GetComponent(terrain_);
        if(!t || !scene_.materials.Contains(material_) || strength_==0)return false;
        if(t->IsGenerationBusy())t->Generation_Cancel();
        if(t->materialEntities.size()<4)return false;
        const XMFLOAT3 start=haveLast_?last_:point;
        const float dx=point.x-start.x,dz=point.z-start.z,dist2=dx*dx+dz*dz;
        if(haveLast_ && dist2<.000001f)return false;
        last_=point;haveLast_=true;
        if(tool_==GroundTool::Paint && (paintSlot_==SIZE_MAX || paintSlot_+1<t->materialEntities.size()))
        {
            if(t->materialEntities.size()>4 && t->materialEntities.back()==material_)paintSlot_=t->materialEntities.size()-1;
            else
            {
                if(t->materialEntities.size()>=256){error_="This terrain has reached 256 paint layers.";return false;}
                paintSlot_=t->materialEntities.size();
                // Reuse the species' existing layer when nothing would obscure this stroke.
                for(size_t n=t->materialEntities.size();n>4;--n)
                {
                    const size_t candidate=n-1;
                    if(t->materialEntities[candidate]!=material_)continue;
                    bool covered=false;
                    const float half=(wi::terrain::chunk_width-1)*t->chunk_scale*.5f;
                    for(const auto& entry:t->chunks)
                    {
                        const auto& c=entry.second;
                        if(c.position.x+half<std::min(start.x,point.x)-radius_ || c.position.x-half>std::max(start.x,point.x)+radius_ ||
                           c.position.z+half<std::min(start.z,point.z)-radius_ || c.position.z-half>std::max(start.z,point.z)+radius_)continue;
                        for(size_t upper=n;upper<c.blendmap_layers.size() && !covered;++upper)
                            covered=std::any_of(c.blendmap_layers[upper].pixels.begin(),c.blendmap_layers[upper].pixels.end(),[](uint8_t w){return w!=0;});
                        if(covered)break;
                    }
                    if(!covered)paintSlot_=candidate;
                    break;
                }
            }
        }
        // Neighbours for Soften come from the mask at the beginning of this gesture, including across chunk boundaries.
        auto sample=[&](int cx,int cz,int x,int z,size_t layer)
        {
            const int stride=wi::terrain::chunk_width-1;
            if(x<0){--cx;x+=stride;}else if(x>stride){++cx;x-=stride;}
            if(z<0){--cz;z+=stride;}else if(z>stride){++cz;z-=stride;}
            const auto it=t->chunks.find(Coord(cx,cz));
            if(it==t->chunks.end() || layer>=it->second.blendmap_layers.size())return 0;
            const auto edit=edits_.find({cx,cz,layer});
            const auto& p=edit==edits_.end()?it->second.blendmap_layers[layer].pixels:edit->second.before;
            return int(p[size_t(x+z*wi::terrain::chunk_width)]);
        };
        bool changed=false;
        const float half=(wi::terrain::chunk_width-1)*t->chunk_scale*.5f;
        for(auto& [coord,c]:t->chunks)
        {
            if(c.position.x+half<std::min(start.x,point.x)-radius_ || c.position.x-half>std::max(start.x,point.x)+radius_ ||
               c.position.z+half<std::min(start.z,point.z)-radius_ || c.position.z-half>std::max(start.z,point.z)+radius_)continue;
            auto* mesh=scene_.meshes.GetComponent(c.entity);
            if(!mesh || mesh->vertex_positions.size()!=wi::terrain::vertexCount || c.invalidated)continue;
            bool dirty=false;
            const size_t end=tool_==GroundTool::Paint?paintSlot_+1:c.blendmap_layers.size();
            for(size_t layer=tool_==GroundTool::Paint?paintSlot_:4;layer<end;++layer)
            {
                if(tool_!=GroundTool::Paint && t->materialEntities[layer]!=material_)continue;
                for(size_t i=0;i<mesh->vertex_positions.size();++i)
                {
                    const auto& v=mesh->vertex_positions[i];const float px=v.x+c.position.x-start.x,pz=v.z+c.position.z-start.z;
                    const float along=dist2>0?std::clamp((px*dx+pz*dz)/dist2,0.f,1.f):0.f;
                    const float x=px-along*dx,z=pz-along*dz;
                    const float d=std::sqrt(x*x+z*z);
                    if(d>radius_)continue;
                    const auto key=Key{coord.x,coord.z,layer};
                    auto found=edits_.find(key);
                    const float alpha=strength_*(softness_>0?Smooth((1-d/radius_)/softness_):1.f);
                    const auto current=layer<c.blendmap_layers.size()?c.blendmap_layers[layer].pixels[i]:0;
                    const auto initial=found==edits_.end()?current:found->second.before[i];
                    float target=tool_==GroundTool::Paint?255.f:0.f;
                    if(tool_==GroundTool::Soften)
                    {
                        const int vx=int(i%wi::terrain::chunk_width),vz=int(i/wi::terrain::chunk_width);
                        int sum=0;for(int oz=-1;oz<=1;++oz)for(int ox=-1;ox<=1;++ox)sum+=sample(coord.x,coord.z,vx+ox,vz+oz,layer);
                        target=sum/9.f;
                    }
                    const int value=int(std::lround(initial+(target-initial)*alpha));
                    if(std::abs(value-initial)<=std::abs(current-initial))continue;
                    if(layer==t->materialEntities.size())
                    {
                        addedSlot_=layer;
                        t->materialEntities.push_back(material_);
                        for(auto& kv:t->chunks)kv.second.enable_blendmap_layer(layer);
                    }
                    c.enable_blendmap_layer(layer);auto& pixels=c.blendmap_layers[layer].pixels;
                    if(found==edits_.end())found=edits_.emplace(key,Edit{pixels,pixels}).first;
                    pixels[i]=found->second.after[i]=uint8_t(value);
                    dirty=changed=true;
                }
            }
            if(dirty)Upload(*t,c);
        }
        return changed;
    }
    bool GroundStroke::Restore(bool after)
    {
        auto* t=scene_.terrains.GetComponent(terrain_);
        if(!t)return false;
        t->Generation_Cancel();
        const bool append=after && addedSlot_!=SIZE_MAX && addedSlot_==t->materialEntities.size();
        for(auto& [key,e]:edits_)
        {
            auto [x,z,l]=key;auto it=t->chunks.find(Coord(x,z));
            if(it==t->chunks.end())return false;
            if(append && l==addedSlot_)continue;
            if(l>=it->second.blendmap_layers.size() || l>=t->materialEntities.size() || t->materialEntities[l]!=material_)return false;
        }
        if(append)
        {
            t->materialEntities.push_back(material_);
            for(auto& entry:t->chunks)entry.second.enable_blendmap_layer(addedSlot_);
        }
        std::set<std::pair<int,int>> dirty;
        for(auto& [key,e]:edits_)
        {
            auto [x,z,l]=key;auto& c=t->chunks.find(Coord(x,z))->second;
            c.blendmap_layers[l].pixels=after?e.after:e.before;dirty.emplace(x,z);
        }
        if(!after && addedSlot_!=SIZE_MAX && addedSlot_+1==t->materialEntities.size())
        {
            t->materialEntities.pop_back();
            for(auto& entry:t->chunks)
            {
                if(entry.second.blendmap_layers.size()>addedSlot_)entry.second.blendmap_layers.pop_back();
                // An untouched chunk's GPU blendmap can also have acquired the extra
                // layer between Execute and Undo. Match the native material-index table.
                if(entry.second.blendmap.desc.array_size>entry.second.blendmap_layers.size()+entry.second.spline_blendmap_layers.size())
                    dirty.emplace(entry.first.x,entry.first.z);
            }
        }
        for(auto [x,z]:dirty)Upload(*t,t->chunks.find(Coord(x,z))->second);
        return true;
    }
    bool GroundStroke::Execute(){return Restore(true);}
    bool GroundStroke::TryUndo(){return Restore(false);}
    namespace
    {
        class SettingsCommand final : public ICommand
        {
        public:
            SettingsCommand(wi::scene::Scene& s,Entity terrain,Entity material,float metres,float coverage)
                :scene(s),terrain(terrain),material(material),metres(metres),coverage(coverage){}
            bool Execute() override
            {
                auto* meta=scene.metadatas.GetComponent(material);
                if(!meta || !meta->float_values.has(Tile))return false;
                if(!captured){oldMetres=meta->float_values.get(Tile);oldCoverage=meta->float_values.get(Coverage);captured=true;}
                return apply(metres,coverage);
            }
            void Undo() override {if(captured)apply(oldMetres,oldCoverage);}
        private:
            bool apply(float size,float grass)
            {
                auto* t=scene.terrains.GetComponent(terrain);auto* mat=scene.materials.GetComponent(material);
                auto* meta=scene.metadatas.GetComponent(material);if(!t || !mat || !meta)return false;
                t->Generation_Cancel();meta->float_values.set(Tile,size);meta->float_values.set(Coverage,grass);
                mat->texMulAdd={size,1,0,-2718};mat->CreateRenderData();mat->SetDirty(false);
                for(auto& entry:t->chunks){if(entry.second.vt)entry.second.vt->invalidate();RefreshGroundGrass(scene,*t,entry.second);}
                return true;
            }
            wi::scene::Scene& scene;Entity terrain,material;float metres,coverage,oldMetres=0,oldCoverage=0;bool captured=false;
        };
        class BaseCommand final : public ICommand
        {
        public:
            BaseCommand(wi::scene::Scene& s,Entity e,Entity base,Entity rock,GroundRules rules)
                :s(s),e(e),base(base),rock(rock),rules(rules){}
            bool Execute() override
            {
                auto* t=s.terrains.GetComponent(e);
                if(!t || t->materialEntities.size()<4 || !s.materials.Contains(base) || !s.materials.Contains(rock))return false;
                t->Generation_Cancel();
                auto& m=Metadata(s,e);
                if(!captured)
                {
                    oldBase=t->materialEntities[0];oldRock=t->materialEntities[1];had=m.float_values.has(Rule);
                    oldRules=ReadGroundRules(s,e);
                    for(auto& [key,c]:t->chunks)
                        if(c.blendmap_layers.size()>=4)for(int l=0;l<4;++l)original[{key.x,key.z,l}]=c.blendmap_layers[l].pixels;
                    captured=true;
                }
                t->materialEntities[0]=base;t->materialEntities[1]=rock;set(rules);
                for(auto& entry:t->chunks)
                {
                    auto& c=entry.second;auto* mesh=s.meshes.GetComponent(c.entity);
                    if(mesh){RefreshGroundSlope(*t,c,*mesh);Upload(*t,c);}
                }
                return true;
            }
            void Undo() override
            {
                auto* t=s.terrains.GetComponent(e);if(!t || !captured)return;
                t->Generation_Cancel();t->materialEntities[0]=oldBase;t->materialEntities[1]=oldRock;
                if(had)set(oldRules);else
                {
                    auto& m=Metadata(s,e);
                    for(auto key:{Rule,End,HighStart,HighFull})m.float_values.erase(key);
                    m.bool_values.erase(High);
                }
                for(auto& [key,c]:t->chunks)
                {
                    for(int l=0;l<4;++l)
                    {
                        auto it=original.find({key.x,key.z,l});
                        if(it!=original.end() && c.blendmap_layers.size()>size_t(l))c.blendmap_layers[l].pixels=it->second;
                    }
                    Upload(*t,c);
                }
            }
        private:
            void set(GroundRules r)
            {
                auto& m=Metadata(s,e);m.float_values.set(Rule,r.rockStart);m.float_values.set(End,r.rockFull);
                m.bool_values.set(High,r.highEnabled);m.float_values.set(HighStart,r.highStart);m.float_values.set(HighFull,r.highFull);
            }
            wi::scene::Scene& s;Entity e,base,rock,oldBase{},oldRock{};GroundRules rules,oldRules;
            bool captured=false,had=false;
            std::map<std::tuple<int,int,int>,std::vector<uint8_t>> original;
        };
    }
    std::unique_ptr<ICommand> SetGroundSettings(wi::scene::Scene& s,Entity e,Entity material,float metres,float coverage)
    {return std::make_unique<SettingsCommand>(s,e,material,std::clamp(metres,.25f,64.f),std::clamp(coverage,0.f,1.f));}
    std::unique_ptr<ICommand> SetGroundBase(wi::scene::Scene& s,Entity e,Entity base,Entity rock,GroundRules r)
    {return std::make_unique<BaseCommand>(s,e,base,rock,r);}
}
