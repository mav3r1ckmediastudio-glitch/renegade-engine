#include "renegade/bridge/TerrainPropEditService.h"
#include "renegade/bridge/ReusableAssetService.h"
#include "renegade/bridge/TerrainPropCollisionWorld.h"
#include <algorithm>
#include <map>

namespace renegade::bridge
{
    namespace
    {
        constexpr const char* RevisionKey = "renegade.terrain_props.revision";
        constexpr const char* LayerKey = "renegade.terrain_props.layer";
        using Cell = std::pair<std::int32_t, std::int32_t>;
        std::vector<TerrainPropPage> ChangedPages(const TerrainPropDocument& before,
            const TerrainPropDocument& after)
        {
            std::map<Cell, TerrainPropPage> old, next;
            for (const auto& p : before.pages) old[{p.cellX,p.cellZ}] = p;
            for (const auto& p : after.pages) next[{p.cellX,p.cellZ}] = p;
            std::vector<TerrainPropPage> changed;
            for (const auto& pair : next)
            {
                std::vector<std::uint8_t> a,b;
                std::string ignored;
                EncodeTerrainPropPage(pair.second,a,ignored);
                const auto found = old.find(pair.first);
                if (found != old.end()) EncodeTerrainPropPage(found->second,b,ignored);
                if (a != b) changed.push_back(pair.second);
                old.erase(pair.first);
            }
            for (const auto& pair : old)
            {
                TerrainPropPage empty;
                empty.cellX = pair.first.first; empty.cellZ = pair.first.second;
                changed.push_back(std::move(empty));
            }
            return changed;
        }
    }

    TerrainPropEditService::TerrainPropEditService(wi::scene::Scene& scene,
        std::filesystem::path root, StableId project, wi::ecs::Entity owner)
        : scene_(scene), root_(std::move(root)), project_(std::move(project)), owner_(owner),
          freshLayer_(GenerateStableId()) {}

    TerrainPropEditService::~TerrainPropEditService()
    {
        if (projection_) projection_->ReleaseOwnership();
    }

    std::string TerrainPropEditService::Revision() const
    {
        const auto* metadata = scene_.metadatas.GetComponent(owner_);
        return metadata && metadata->string_values.has(RevisionKey)
            ? metadata->string_values.get(RevisionKey) : std::string{};
    }

    bool TerrainPropEditService::Read(TerrainPropDocument& output, std::string& error)
    {
        if (!scene_.transforms.Contains(owner_))
        { error="Terrain prop layer owner no longer exists."; return false; }
        const auto revision = Revision();
        TerrainPropDocument doc;
        if (revision.empty()) doc.layerId = freshLayer_;
        else if (!ReadTerrainPropRevision(root_,project_,revision,doc,error)) return false;
        highWater_ = std::max(highWater_,doc.nextPlacementId);
        doc.nextPlacementId = highWater_;
        knownLayer_=doc.layerId;
        output = std::move(doc);
        return true;
    }

    bool ResolveTerrainPropAuthoringOwner(const wi::scene::Scene& scene,const StableId& project,
        wi::ecs::Entity fallback,wi::ecs::Entity& output,std::string& error)
    {
        error.clear();
        if(!IsValidStableId(project)) {error="Invalid terrain prop project.";return false;}
        auto owner=wi::ecs::INVALID_ENTITY;
        for(size_t i=0;i<scene.metadatas.GetCount();++i)
        {
            const auto& m=scene.metadatas[i];
            if(m.runtimeProjectionOnly || !m.string_values.has(RevisionKey)) continue;
            if(!m.string_values.has("renegade.terrain_props.project") ||
                m.string_values.get("renegade.terrain_props.project")!=project)
            {error="Terrain prop layer belongs to a different or invalid project.";return false;}
            const auto entity=scene.metadatas.GetEntity(i);
            if(!scene.transforms.Contains(entity))
            {error="Terrain prop layer has no authored owner.";return false;}
            if(owner!=wi::ecs::INVALID_ENTITY)
            {error="Multiple terrain prop layers require an explicit owner selection.";return false;}
            owner=entity;
        }
        if(owner==wi::ecs::INVALID_ENTITY) owner=fallback;
        if(!scene.transforms.Contains(owner))
        {error="Terrain prop authoring needs an existing terrain or layer owner.";return false;}
        output=owner;
        return true;
    }

    bool TerrainPropEditService::Reconcile(std::string& error)
    {
        error_.clear();
        if(!scene_.transforms.Contains(owner_))
        {
            if (!knownLayer_.empty() && (projection_ || !projectionRevision_.empty()))
                if (auto world=TerrainPropCollisionWorld::Find(scene_))
                {
                    TerrainPropDocument empty;empty.layerId=knownLayer_;
                    auto pending=world->Stage(empty,error);
                    if (!pending) return false;
                    world->Commit(*pending);
                }
            // The UI retains this service through same-document deletion so
            // Undo can restore its authored owner and immutable revision.
            projection_.reset();
            if(!knownLayer_.empty())
            {
                std::vector<wi::ecs::Entity> orphaned;
                for(size_t i=0;i<scene_.metadatas.GetCount();++i)
                {
                    const auto& m=scene_.metadatas[i];
                    if(m.runtimeProjectionOnly && m.string_values.has(LayerKey) &&
                        m.string_values.get(LayerKey)==knownLayer_)
                        orphaned.push_back(scene_.metadatas.GetEntity(i));
                }
                for(auto entity:orphaned) scene_.Entity_Remove(entity,false);
            }
            projectionRevision_.clear();
            error.clear();
            return true;
        }
        if(projection_ && projectionRevision_==Revision())
        {error.clear();return true;}
        TerrainPropDocument authoritative;
        if(!Read(authoritative,error_) || !PrepareProjection(authoritative,error_))
        {error=error_;return false;}
        error.clear();
        return true;
    }

    bool TerrainPropEditService::PrepareProjection(const TerrainPropDocument& doc, std::string& error)
    {
        if (projection_ && projectionRevision_ == Revision()) return true;
        std::vector<wi::ecs::Entity> old;
        for (size_t i=0;i<scene_.metadatas.GetCount();++i)
        {
            const auto& m=scene_.metadatas[i];
            if (m.runtimeProjectionOnly && m.string_values.has(LayerKey) &&
                m.string_values.get(LayerKey)==doc.layerId)
                old.push_back(scene_.metadatas.GetEntity(i));
        }
        auto collisionWorld=TerrainPropCollisionWorld::Find(scene_);
        auto collisionEdit=collisionWorld?collisionWorld->Stage(doc,error):nullptr;
        if(collisionWorld&&!collisionEdit)return false;
        auto replacement=std::make_unique<TerrainPropProjection>(scene_,
            !collisionWorld);
        replacement->SetLayerIdentity(doc.layerId);
        for (const auto& profile:doc.profiles)
        {
            auto model=ReusableAssetService().PrepareModelAssetPlacement(
                {root_.generic_u8string(),project_,profile.modelAssetId});
            if (!model.IsReady()) { error=model.Result().error; return false; }
            if (!replacement->Register(profile.id,*model.PeekScene(),profile.collision,error)) return false;
        }
        for (const auto& page:doc.pages) if (!replacement->Place(page,error)) return false;
        projection_.reset();
        for (auto entity:old) scene_.Entity_Remove(entity,false);
        projection_=std::move(replacement);
        if(collisionWorld)collisionWorld->Commit(*collisionEdit);
        displayed_=doc;
        projectionRevision_=Revision();
        return true;
    }

    bool TerrainPropEditService::Apply(TerrainPropDocument doc,
        const std::string& expected, const std::function<bool()>& cancelled)
    {
        error_.clear();
        if (Revision()!=expected) { error_="Terrain props changed; restage the operation."; return false; }
        if (cancelled && cancelled()) { error_="Terrain prop edit cancelled."; return false; }
        TerrainPropDocument before;
        if (!Read(before,error_)) return false;
        if (doc.layerId!=before.layerId) { error_="Terrain prop layer identity changed."; return false; }
        doc.nextPlacementId=std::max({doc.nextPlacementId, before.nextPlacementId,highWater_});
        // Validate every page before touching native projection state.
        for (const auto& page:doc.pages) if (!ValidateTerrainPropPage(page,error_)) return false;
        if (!PrepareProjection(before,error_)) return false;
        auto collisionWorld=TerrainPropCollisionWorld::Find(scene_);
        auto collisionEdit=collisionWorld?collisionWorld->Stage(doc,error_):nullptr;
        if(collisionWorld&&!collisionEdit)return false;
        bool changedCollision=false;
        for(const auto& profile:doc.profiles)
        {
            const auto existing=std::find_if(before.profiles.begin(),before.profiles.end(),
                [&](const auto& old){return old.id==profile.id;});
            if(existing==before.profiles.end())continue;
            if(existing->modelAssetId!=profile.modelAssetId)
            {error_="Replace a model with a new profile identity.";return false;}
            const auto& a=existing->collision;const auto& b=profile.collision;
            changedCollision=changedCollision||a.shape!=b.shape||a.friction!=b.friction||a.surface!=b.surface||
                a.center.x!=b.center.x||a.center.y!=b.center.y||a.center.z!=b.center.z||
                a.halfExtents.x!=b.halfExtents.x||a.halfExtents.y!=b.halfExtents.y||a.halfExtents.z!=b.halfExtents.z;
        }
        if(changedCollision)
        {
            // Profile edits preserve every placement/candidate identity. Stage
            // complete replacement objects at this same native safe point;
            // RAII destroys only that replacement if publication fails.
            auto replacement=std::make_unique<TerrainPropProjection>(scene_,
                !collisionWorld);
            replacement->SetLayerIdentity(doc.layerId);
            for(const auto& profile:doc.profiles)
            {
                auto model=ReusableAssetService().PrepareModelAssetPlacement(
                    {root_.generic_u8string(),project_,profile.modelAssetId});
                if(!model.IsReady()){error_=model.Result().error;return false;}
                if(!replacement->Register(profile.id,*model.PeekScene(),profile.collision,error_))return false;
            }
            for(const auto& page:doc.pages)if(!replacement->Place(page,error_))return false;
            if((cancelled&&cancelled())||Revision()!=expected||
                (collisionWorld&&!collisionWorld->CanCommit(*collisionEdit)))
            {error_="Collision edit cancelled or stale; previous display retained.";return false;}
            if(!PublishTerrainPropRevision(root_,project_,doc,scene_,owner_,error_,options_))return false;
            projection_=std::move(replacement);
            if(collisionWorld)collisionWorld->Commit(*collisionEdit);
            highWater_=doc.nextPlacementId;displayed_=doc;projectionRevision_=Revision();
            return true;
        }
        bool committed=false;
        std::vector<TerrainPropProfileId> newProfiles;
        auto registrationRollback=std::shared_ptr<void>(nullptr,[&](void*)
        {
            if(committed) return;
            for(const auto& id:newProfiles)
            {
                projection_->UnregisterUnused(id);
                displayed_.profiles.erase(std::remove_if(displayed_.profiles.begin(),displayed_.profiles.end(),
                    [&](const TerrainPropProfile& p){return p.id==id;}),displayed_.profiles.end());
            }
        });
        for (const auto& p:doc.profiles)
        {
            auto existing=std::find_if(displayed_.profiles.begin(),displayed_.profiles.end(),
                [&](const TerrainPropProfile& old){return old.id==p.id;});
            if (existing!=displayed_.profiles.end())
            {
                if (existing->modelAssetId!=p.modelAssetId ||
                    existing->collision.shape!=p.collision.shape ||
                    existing->collision.center.x!=p.collision.center.x ||
                    existing->collision.center.y!=p.collision.center.y ||
                    existing->collision.center.z!=p.collision.center.z ||
                    existing->collision.halfExtents.x!=p.collision.halfExtents.x ||
                    existing->collision.halfExtents.y!=p.collision.halfExtents.y ||
                    existing->collision.halfExtents.z!=p.collision.halfExtents.z ||
                    existing->collision.friction!=p.collision.friction || existing->collision.surface!=p.collision.surface)
                {error_="Replace a model with a new profile identity.";return false;}
                continue;
            }
            auto model=ReusableAssetService().PrepareModelAssetPlacement(
                {root_.generic_u8string(),project_,p.modelAssetId});
            if (!model.IsReady()) {error_=model.Result().error;return false;}
            if (!projection_->Register(p.id,*model.PeekScene(),p.collision,error_)) return false;
            // Registered prototypes remain reusable even after an undone addition.
            newProfiles.push_back(p.id);
            displayed_.profiles.push_back(p);
        }
        auto pending=projection_->PreparePages(ChangedPages(before,doc),error_);
        if (!pending || (cancelled && cancelled()))
        {if(error_.empty()) error_="Terrain prop edit cancelled.";return false;}
        // This entire method is one scene-safe operation. The staged objects
        // cannot render or enter physics before either commit or RAII rollback.
        if (!projection_->InstallPages(*pending,error_)) return false;
        if(Revision()!=expected||(collisionWorld&&!collisionWorld->CanCommit(*collisionEdit)))
        {error_="Collision edit became stale; restage the operation.";return false;}
        if (!PublishTerrainPropRevision(root_,project_,doc,scene_,owner_,error_,options_)) return false;
        projection_->CommitPages(*pending);
        if(collisionWorld)collisionWorld->Commit(*collisionEdit);
        committed=true;
        highWater_=doc.nextPlacementId;
        displayed_.pages=doc.pages;
        projectionRevision_=Revision();
        return true;
    }

    class TerrainPropEditService::EditCommand final : public ICommand
    {
    public:
        EditCommand(std::shared_ptr<TerrainPropEditService> service, TerrainPropDocument after,
            std::string expected, std::function<bool()> cancelled)
            : service_(std::move(service)),after_(std::move(after)),expected_(std::move(expected)),
              cancelled_(std::move(cancelled)) {}
        bool Execute() override
        {
            TerrainPropDocument current;
            if (!service_->Read(current,service_->error_)) return false;
            if (!initialized_)
            {
                if (service_->Revision()!=expected_)
                { service_->error_="Terrain props changed; restage the operation."; return false; }
                if (!Initialize(current)) return false;
            }
            else if (!Matches(current,beforeHash_))
            { service_->error_="Terrain props changed; cannot redo this operation."; return false; }
            auto next=Rebuild(current,false);
            if (!service_->Apply(std::move(next),service_->Revision(),
                captured_ ? std::function<bool()>{}:cancelled_)) return false;
            captured_=true;
            return true;
        }
        bool TryUndo() override
        {
            TerrainPropDocument current;
            if (!captured_ || !service_->Read(current,service_->error_) || !Matches(current,afterHash_))
            { service_->error_="Terrain props changed; cannot undo this operation."; return false; }
            return service_->Apply(Rebuild(current,true),service_->Revision(),{});
        }
        void Undo() override { (void)TryUndo(); }
    private:
        bool Matches(const TerrainPropDocument& doc,const std::string& expected)
        {
            std::string hash;
            return ComputeTerrainPropDocumentContentDigest(doc,hash,service_->error_) && hash==expected;
        }
        bool Initialize(const TerrainPropDocument& current)
        {
            if (!ComputeTerrainPropDocumentContentDigest(current,beforeHash_,service_->error_) ||
                !ComputeTerrainPropDocumentContentDigest(after_,afterHash_,service_->error_)) return false;
            before_=current;
            std::map<Cell,const TerrainPropPage*> old,next;
            for (const auto& page:before_.pages)
            {
                const Cell cell{page.cellX,page.cellZ};
                beforeOrder_.push_back(cell);old.emplace(cell,&page);
            }
            for (const auto& page:after_.pages)
            {
                const Cell cell{page.cellX,page.cellZ};
                afterOrder_.push_back(cell);next.emplace(cell,&page);
            }
            for (const auto& changed:ChangedPages(before_,after_))
            {
                const Cell cell{changed.cellX,changed.cellZ};
                TerrainPropPageDelta delta;
                delta.cellX=cell.first;delta.cellZ=cell.second;
                auto a=old.find(cell),b=next.find(cell);
                if(a!=old.end()) {delta.hadBefore=true;delta.before=*a->second;}
                if(b!=next.end()) {delta.hasAfter=true;delta.after=*b->second;}
                deltas_.push_back(std::move(delta));
            }
            // History retains only changed page bytes. Unchanged pages are
            // recovered from the checked current revision when applying a delta.
            before_.pages.clear();after_.pages.clear();
            initialized_=true;
            return true;
        }
        TerrainPropDocument Rebuild(TerrainPropDocument current,bool undo)
        {
            const auto& state=undo?before_:after_;
            std::map<Cell,TerrainPropPage> pages;
            for(auto& page:current.pages)
                pages.emplace(Cell{page.cellX,page.cellZ},std::move(page));
            for(const auto& delta:deltas_)
            {
                const Cell cell{delta.cellX,delta.cellZ};
                if(undo?delta.hadBefore:delta.hasAfter)
                    pages[cell]=undo?delta.before:delta.after;
                else pages.erase(cell);
            }
            current.layerId=state.layerId;
            current.profiles=state.profiles;
            current.authoring=state.authoring;
            current.nextPlacementId=std::max(current.nextPlacementId,state.nextPlacementId);
            current.pages.clear();
            for(const auto& cell:undo?beforeOrder_:afterOrder_)
                current.pages.push_back(std::move(pages.at(cell)));
            return current;
        }
        std::shared_ptr<TerrainPropEditService> service_;
        TerrainPropDocument before_,after_; // Page vectors released after staging.
        std::vector<TerrainPropPageDelta> deltas_;
        std::vector<Cell> beforeOrder_,afterOrder_;
        std::string expected_,beforeHash_,afterHash_;
        std::function<bool()> cancelled_;
        bool initialized_=false,captured_=false;
    };

    std::unique_ptr<ICommand> TerrainPropEditService::MakeCommand(const TerrainPropDocument& after,
        const std::string& expected, std::function<bool()> cancelled)
    {
        return std::make_unique<EditCommand>(shared_from_this(),after,expected,std::move(cancelled));
    }
}
