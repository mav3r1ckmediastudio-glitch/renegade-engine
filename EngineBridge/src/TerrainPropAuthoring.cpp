#include "renegade/bridge/TerrainPropAuthoring.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace renegade::bridge
{
namespace
{
    constexpr std::size_t Limit = 1000000;
    using Profile = std::array<std::uint8_t,16>;
    using Cell = std::pair<std::int32_t,std::int32_t>;
    struct Item { TerrainPropPlacement placement; Profile profile; };
    bool Finite(float x) { return std::isfinite(x); }
    bool Position(const std::array<float,3>& p)
    { return Finite(p[0]) && Finite(p[1]) && Finite(p[2]); }
    bool In(const TerrainPropBounds& b, float x, float z)
    { return x >= b.minX && x <= b.maxX && z >= b.minZ && z <= b.maxZ; }
    bool InDisc(float x, float z, float cx, float cz, float radius)
    { const double dx = double(x)-cx, dz = double(z)-cz; return dx*dx+dz*dz <= double(radius)*radius; }
    std::uint64_t Hash(std::uint64_t x)
    {
        x += 0x9e3779b97f4a7c15ull;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
        return x ^ (x >> 31);
    }
    float Unit(std::uint64_t x) { return float(x >> 40) * (1.f / 16777216.f); }
    std::uint64_t CandidateKey(const TerrainPropRecipe& r, std::int64_t x, std::int64_t z, const Profile& p)
    {
        std::uint32_t stepBits = 0;
        std::memcpy(&stepBits, &r.candidateStep, sizeof(stepBits));
        auto key = Hash(r.seed ^ Hash(stepBits) ^ Hash(static_cast<std::uint64_t>(x)) ^
            Hash(static_cast<std::uint64_t>(z) + 0x8123456789abcdefull));
        for (auto byte : p) key = Hash(key ^ byte);
        return key ? key : 1;
    }
    bool SamePage(const TerrainPropPage& a, const TerrainPropPage& b)
    {
        std::vector<std::uint8_t> x,y; std::string error;
        return EncodeTerrainPropPage(a,x,error) && EncodeTerrainPropPage(b,y,error) && x == y;
    }
    // A disc occupies one centre bucket at the smallest power-of-two scale
    // containing its radius. Point queries need only neighbouring buckets at
    // occupied scales; a huge clearing does not fan out over the whole map.
    class DiscIndex
    {
        using Key=std::tuple<int,std::int64_t,std::int64_t>;
        std::map<Key,std::vector<TerrainPropClearing>> buckets_;
        std::array<bool,18> occupied_{};
        TerrainPropEditWork& work_;
    public:
        explicit DiscIndex(TerrainPropEditWork& work) : work_(work) {}
        bool Add(const TerrainPropClearing& c,const TerrainPropBounds& bounds,
            const std::function<bool()>& guard)
        {
            ++work_.discIndexEntries;
            if (!guard()) return false;
            // Double bounds also safely reject finite, extreme legacy centres.
            if (double(c.x)+c.radius<bounds.minX || double(c.x)-c.radius>bounds.maxX ||
                double(c.z)+c.radius<bounds.minZ || double(c.z)-c.radius>bounds.maxZ) return true;
            int level=0; double size=1;
            while(size<c.radius) {size*=2;++level;}
            const auto x=static_cast<std::int64_t>(std::floor(double(c.x)/size));
            const auto z=static_cast<std::int64_t>(std::floor(double(c.z)/size));
            occupied_[level]=true;
            buckets_[{level,x,z}].push_back(c);
            return true;
        }
        bool Contains(float x,float z,bool& found,const std::function<bool()>& guard) const
        {
            found=false;
            double size=1;
            for(int level=0;level<int(occupied_.size());++level,size*=2)
            {
                if(!occupied_[level])continue;
                const auto bx=static_cast<std::int64_t>(std::floor(double(x)/size));
                const auto bz=static_cast<std::int64_t>(std::floor(double(z)/size));
                for(auto iz=bz-1;iz<=bz+1;++iz) for(auto ix=bx-1;ix<=bx+1;++ix)
                {
                    ++work_.discBucketLookups;
                    if(!guard())return false;
                    const auto bucket=buckets_.find({level,ix,iz});
                    if(bucket==buckets_.end())continue;
                    for(const auto& c:bucket->second)
                    {
                        ++work_.discTests;
                        if(!guard())return false;
                        if(InDisc(x,z,c.x,c.z,c.radius)){found=true;return true;}
                    }
                }
            }
            return true;
        }
    };
    bool ValidateMix(const TerrainPropMix& mix)
    {
        if (mix.name.size() > 256 || mix.name.find('\0') != std::string::npos || mix.entries.size() > 64) return false;
        std::set<Profile> ids;
        for (const auto& e : mix.entries)
            if (e.profileId == Profile{} || !ids.insert(e.profileId).second ||
                !Finite(e.densityPerSquareMetre) || e.densityPerSquareMetre < 0 || e.densityPerSquareMetre > 16 ||
                !Finite(e.minSpacing) || e.minSpacing < 0 || e.minSpacing > 128 ||
                !Finite(e.minScale) || !Finite(e.maxScale) || e.minScale <= 0 || e.maxScale < e.minScale ||
                e.maxScale > 1000) return false;
        return true;
    }
}
bool ValidateTerrainPropAuthoring(const TerrainPropAuthoringState& s, std::string& error)
{
    error.clear();
    const auto& r = s.recipe;
    if (!Finite(r.cellSize) || r.cellSize < 1 || r.cellSize > 65536 ||
        !Finite(r.candidateStep) || r.candidateStep < .25f || r.candidateStep > 128 ||
        !ValidateMix(r.mix) || s.savedMixes.size() > 1024 ||
        s.bindings.size() > Limit || s.tombstones.size() > Limit ||
        s.clearings.size() > Limit || s.overrides.size() > Limit)
    { error = "Invalid terrain prop recipe or authoring capacity."; return false; }
    std::set<std::string> names;
    for (const auto& mix : s.savedMixes)
        if (!ValidateMix(mix) || mix.name.empty() || !names.insert(mix.name).second)
        { error = "Invalid or duplicate saved terrain mix."; return false; }
    std::unordered_set<std::uint64_t> keys, ids;
    for (const auto& b : s.bindings)
        if (!b.candidateKey || !b.placementId || !keys.insert(b.candidateKey).second || !ids.insert(b.placementId).second)
        { error = "Invalid terrain candidate binding."; return false; }
    keys.clear();
    for (auto key : s.tombstones)
        if (!key || !keys.insert(key).second)
        { error = "Invalid terrain tombstone."; return false; }
    for (const auto& c : s.clearings)
        if (!Finite(c.x) || !Finite(c.z) || !Finite(c.radius) || c.radius <= 0 || c.radius > 100000)
        { error = "Invalid terrain clearing."; return false; }
    keys.clear(); ids.clear();
    for (const auto& o : s.overrides)
        if (!o.placementId || !ids.insert(o.placementId).second ||
            (o.candidateKey && !keys.insert(o.candidateKey).second) || o.profileId == Profile{} ||
            !Position(o.position) || !Finite(o.yawRadians) || !Finite(o.scale) || o.scale <= 0)
        { error = "Invalid terrain placement override."; return false; }
    return true;
}
bool SampleTerrainPropStroke(const std::vector<std::array<float,3>>& path,
    float distance, std::vector<std::array<float,3>>& samples, std::string& error)
{
    error.clear();
    if (!Finite(distance) || distance < .01f || path.size() > Limit ||
        !std::all_of(path.begin(),path.end(),Position))
    { error = "Invalid terrain brush path or sample distance."; return false; }
    std::vector<std::array<float,3>> result;
    if (path.empty()) { samples.clear(); return true; }
    result.push_back(path.front());
    double until = distance;
    for (std::size_t i=1;i<path.size();++i)
    {
        const auto& a=path[i-1]; const auto& b=path[i];
        const double dx=double(b[0])-a[0], dz=double(b[2])-a[2];
        const double length=std::hypot(dx,dz);
        if (!std::isfinite(length) || length > 10000000)
        { error="Terrain brush segment exceeds authoring limits."; return false; }
        double walked=0;
        while (length-walked >= until)
        {
            walked += until;
            const double t = walked/length;
            result.push_back({float(a[0]+dx*t),float(a[1]+(double(b[1])-a[1])*t),float(a[2]+dz*t)});
            if (result.size() >= Limit) { error="Terrain brush sample capacity exceeded."; return false; }
            until=distance;
        }
        until -= length-walked;
    }
    // The pointer endpoint is deliberately not added: event frequency must not
    // change the stamps. A click still has its first stamp.
    samples.swap(result);
    return true;
}
bool BuildTerrainPropEdit(const std::vector<TerrainPropPage>& pages,
    const TerrainPropAuthoringState& source, std::uint64_t nextPlacementId,
    const TerrainPropEditRequest& request, const TerrainPropHeightSampler& height,
    TerrainPropEdit& result, std::string& error)
{
    error.clear();
    const auto& b=request.bounds;
    if (request.expectedRevision != request.currentRevision)
    { error="Terrain prop layer changed while the edit was being prepared."; return false; }
    if (!ValidateTerrainPropAuthoring(source,error)) return false;
    if (!nextPlacementId || !Finite(b.minX) || !Finite(b.minZ) || !Finite(b.maxX) || !Finite(b.maxZ) ||
        b.minX>b.maxX || b.minZ>b.maxZ || std::abs(b.minX)>10000000 || std::abs(b.maxX)>10000000 ||
        std::abs(b.minZ)>10000000 || std::abs(b.maxZ)>10000000 || !height ||
        !Finite(request.radius) || request.radius<=0 || request.radius>100000 ||
        static_cast<int>(request.operation)<0 || static_cast<int>(request.operation)>3)
    { error="Invalid finite terrain edit bounds or brush."; return false; }
    TerrainPropEdit edit; edit.authoring=source; edit.nextPlacementId=nextPlacementId;
    auto& s=edit.authoring; const auto& recipe=s.recipe;
    std::unordered_map<std::uint64_t,std::uint64_t> bindings;
    for (const auto& binding : s.bindings)
    {
        if (binding.placementId >= nextPlacementId)
        { error="Candidate binding exceeds placement ID high-water mark."; return false; }
        bindings.emplace(binding.candidateKey,binding.placementId);
    }
    std::map<Cell,TerrainPropPage> before;
    std::map<std::uint64_t,Item> items;
    std::unordered_set<std::uint64_t> currentKeys;
    for (const auto& page : pages)
    {
        if (!ValidateTerrainPropPage(page,error)) return false;
        if (!before.emplace(Cell{page.cellX,page.cellZ},page).second)
        { error="Duplicate terrain prop cell."; return false; }
        for (const auto& p : page.placements)
        {
            if (p.id>=nextPlacementId || std::abs(p.position[0])>10000000 || std::abs(p.position[2])>10000000 ||
                !items.emplace(p.id,Item{p,page.profiles[p.profileIndex]}).second ||
                (p.candidateKey && !currentKeys.insert(p.candidateKey).second))
            { error="Duplicate terrain placement or invalid high-water mark."; return false; }
            if (p.candidateKey)
            {
                const auto found=bindings.find(p.candidateKey);
                if (found!=bindings.end() && found->second!=p.id)
                { error="Terrain placement disagrees with its candidate binding."; return false; }
                bindings[p.candidateKey]=p.id;
            }
        }
    }
    for (const auto& o:s.overrides)
    {
        const auto binding=bindings.find(o.candidateKey);
        const auto existing=items.find(o.placementId);
        if (o.placementId>=nextPlacementId || std::abs(o.position[0])>10000000 || std::abs(o.position[2])>10000000 ||
            (o.candidateKey && binding!=bindings.end() && binding->second!=o.placementId) ||
            (existing!=items.end() && existing->second.placement.candidateKey!=o.candidateKey))
        { error="Terrain override disagrees with placement identity."; return false; }
        if (o.candidateKey) bindings[o.candidateKey]=o.placementId;
    }
    std::vector<std::array<float,3>> stamps;
    if (request.operation==TerrainPropEditOperation::Paint || request.operation==TerrainPropEditOperation::Erase)
    {
        if (!SampleTerrainPropStroke(request.path,request.sampleDistance,stamps,error)) return false;
        if (stamps.empty()) { error="Paint and erase need a terrain brush path."; return false; }
        if(request.operation==TerrainPropEditOperation::Erase && stamps.size()>Limit-s.clearings.size())
        {error="Terrain clearing capacity exceeded.";return false;}
    }
    float currentProgress=0;
    const auto progress=[&](float value) {
        currentProgress=value;
        if (!request.progress || request.progress(value)) return true;
        error="Terrain prop edit cancelled."; return false;
    };
    if (!progress(0)) return false;
    bool discFailed=false;
    const std::function<bool()> discGuard=[&] {
        if(discFailed)return false;
        const auto work=edit.work.discIndexEntries+edit.work.discBucketLookups+edit.work.discTests;
        if(work>20000000)
        {error="Terrain brush/clearing lookup work budget exceeded.";discFailed=true;return false;}
        if((work%4096)==0 && !progress(currentProgress)){discFailed=true;return false;}
        return true;
    };
    DiscIndex strokeIndex(edit.work),clearingIndex(edit.work);
    for(const auto& stamp:stamps)
        if(!strokeIndex.Add({stamp[0],stamp[2],request.radius},b,discGuard))return false;
    if(request.operation==TerrainPropEditOperation::Generate || request.operation==TerrainPropEditOperation::Paint)
        for(const auto& clearing:s.clearings)
            if(!clearingIndex.Add(clearing,b,discGuard))return false;
    const auto covered=[&](float x,float z) {
        if(discFailed || !In(b,x,z))return false;
        if(stamps.empty())return true;
        bool found=false;
        if(!strokeIndex.Contains(x,z,found,discGuard))discFailed=true;
        return found;
    };
    std::unordered_set<std::uint64_t> tombstones(s.tombstones.begin(),s.tombstones.end());
    if (request.operation==TerrainPropEditOperation::Erase)
    {
        for (const auto& stamp:stamps)
            s.clearings.push_back({stamp[0],stamp[2],request.radius});
        for (auto it=items.begin();it!=items.end();)
        {
            const auto& p=it->second.placement;
            if (covered(p.position[0],p.position[2]))
            {
                if (p.candidateKey) tombstones.insert(p.candidateKey);
                it=items.erase(it);
            } else ++it;
            if(discFailed)return false;
        }
        s.overrides.erase(std::remove_if(s.overrides.begin(),s.overrides.end(),
            [&](const auto& o){return covered(o.position[0],o.position[2]);}),s.overrides.end());
        if(discFailed)return false;
    }
    else if (request.operation==TerrainPropEditOperation::Reproject)
    {
        for (auto& pair:items)
        {
            auto& p=pair.second.placement;
            const bool inside=covered(p.position[0],p.position[2]);
            if(discFailed)return false;
            if(!inside)continue;
            float y=0;
            if (height(p.position[0],p.position[2],y) && Finite(y)) p.position[1]=y;
        }
        for (auto& o:s.overrides)
        {
            const auto found=items.find(o.placementId);
            if (found!=items.end()) o.position=found->second.placement.position;
        }
    }
    else
    {
        if (request.operation==TerrainPropEditOperation::Generate)
            for (auto it=items.begin();it!=items.end();)
                if (it->second.placement.candidateKey && covered(it->second.placement.position[0],it->second.placement.position[2]))
                    it=items.erase(it);
                else ++it;
        // Moved overrides are obstacles at their authored positions while new
        // candidates are evaluated, including across the edge of this edit.
        for (const auto& o:s.overrides)
        {
            if (o.candidateKey && tombstones.count(o.candidateKey)) continue;
            TerrainPropPlacement p;
            p.id=o.placementId; p.candidateKey=o.candidateKey; p.position=o.position;
            p.yawRadians=o.yawRadians; p.scale=o.scale;
            items[p.id]={p,o.profileId};
        }
        TerrainPropBounds scan=b;
        if (request.operation==TerrainPropEditOperation::Paint)
        {
            TerrainPropBounds brush{INFINITY,INFINITY,-INFINITY,-INFINITY};
            for (const auto& stamp:stamps)
            {
                brush.minX=std::min(brush.minX,stamp[0]-request.radius);
                brush.minZ=std::min(brush.minZ,stamp[2]-request.radius);
                brush.maxX=std::max(brush.maxX,stamp[0]+request.radius);
                brush.maxZ=std::max(brush.maxZ,stamp[2]+request.radius);
            }
            scan={std::max(b.minX,brush.minX),std::max(b.minZ,brush.minZ),
                std::min(b.maxX,brush.maxX),std::min(b.maxZ,brush.maxZ)};
        }
        const bool emptyScan=scan.minX>scan.maxX || scan.minZ>scan.maxZ;
        const double step=recipe.candidateStep;
        const auto x0=emptyScan?0:static_cast<std::int64_t>(std::floor(scan.minX/step));
        const auto z0=emptyScan?0:static_cast<std::int64_t>(std::floor(scan.minZ/step));
        const auto x1=emptyScan?-1:static_cast<std::int64_t>(std::floor(scan.maxX/step));
        const auto z1=emptyScan?-1:static_cast<std::int64_t>(std::floor(scan.maxZ/step));
        const double count=emptyScan?0:double(x1-x0+1)*double(z1-z0+1)*recipe.mix.entries.size();
        if (count>Limit) { error="Terrain edit exceeds one million candidates; use a smaller region."; return false; }
        float largestSpacing=0;
        std::map<Profile,float> profileSpacing;
        for (const auto& entry:recipe.mix.entries)
        {
            largestSpacing=std::max(largestSpacing,entry.minSpacing);
            profileSpacing.emplace(entry.profileId,entry.minSpacing);
        }
        const double bucketSize=std::max(.25f,largestSpacing);
        using Bucket=std::pair<std::int64_t,std::int64_t>;
        std::map<Bucket,std::vector<const Item*>> retained;
        for (const auto& pair:items)
        {
            const auto& p=pair.second.placement;
            retained[{static_cast<std::int64_t>(std::floor(p.position[0]/bucketSize)),
                static_cast<std::int64_t>(std::floor(p.position[2]/bucketSize))}].push_back(&pair.second);
        }
        struct Candidate { std::uint64_t key; float x,z; const TerrainPropMixEntry* entry; };
        const auto candidate=[&](std::int64_t x,std::int64_t z,const TerrainPropMixEntry& e,Candidate& c) {
            c={CandidateKey(recipe,x,z,e.profileId),0,0,&e};
            if (Unit(Hash(c.key+3)) >= std::min(1.0,double(e.densityPerSquareMetre)*step*step)) return false;
            c.x=float((double(x)+Unit(Hash(c.key+1)))*step);
            c.z=float((double(z)+Unit(Hash(c.key+2)))*step);
            return true;
        };
        std::size_t completed=0,work=0;
        const auto spacingGuard=[&] {
            if(++work>50000000)
            {error="Terrain spacing work budget exceeded; increase candidate spacing.";return false;}
            return (work%4096)!=0 || progress(currentProgress);
        };
        for (auto z=z0;z<=z1;++z)
        for (auto x=x0;x<=x1;++x)
        for (const auto& entry:recipe.mix.entries)
        {
            if ((++completed%128)==0 && !progress(float(completed/std::max(1.0,count))*.9f)) return false;
            Candidate c;
            if (!candidate(x,z,entry,c))continue;
            const bool inside=covered(c.x,c.z);
            if(discFailed)return false;
            if(!inside || tombstones.count(c.key))continue;
            bool blocked=false;
            if(!clearingIndex.Contains(c.x,c.z,blocked,discGuard))return false;
            if (blocked) continue;
            // Global candidate priorities, including the adjacent cells outside
            // the requested region, make spacing independent of cell traversal.
            const auto range=static_cast<std::int64_t>(std::ceil(largestSpacing/step))+1;
            for (auto nz=z-range;nz<=z+range && !blocked;++nz)
            for (auto nx=x-range;nx<=x+range && !blocked;++nx)
            for (const auto& other:recipe.mix.entries)
            {
                if(!spacingGuard())return false;
                Candidate n;
                if (!candidate(nx,nz,other,n) || n.key>=c.key) continue;
                const float spacing=std::max(entry.minSpacing,other.minSpacing);
                if (spacing>0 && InDisc(c.x,c.z,n.x,n.z,spacing)) { blocked=true; break; }
            }
            if (blocked) continue;
            // Retained placements from earlier strokes/seeds are authoritative.
            const auto bx=static_cast<std::int64_t>(std::floor(c.x/bucketSize));
            const auto bz=static_cast<std::int64_t>(std::floor(c.z/bucketSize));
            for (auto rz=bz-1;rz<=bz+1 && !blocked;++rz)
            for (auto rx=bx-1;rx<=bx+1 && !blocked;++rx)
            {
                const auto bucket=retained.find({rx,rz});
                if (bucket==retained.end()) continue;
                for (const auto* item:bucket->second)
                {
                    // Dense legacy/manual placements may share one bucket.
                    // Charge each visited item, including same-key skips.
                    if(!spacingGuard())return false;
                    const auto& p=item->placement;
                    if (p.candidateKey==c.key) continue;
                    float spacing=entry.minSpacing;
                    const auto retainedSpacing=profileSpacing.find(item->profile);
                    if(retainedSpacing!=profileSpacing.end())spacing=std::max(spacing,retainedSpacing->second);
                    if (spacing>0 && InDisc(c.x,c.z,p.position[0],p.position[2],spacing)) { blocked=true; break; }
                }
            }
            if (blocked) continue;
            float y=0;
            if (!height(c.x,c.z,y) || !Finite(y)) continue;
            auto found=bindings.find(c.key);
            std::uint64_t id=0;
            if (found!=bindings.end()) id=found->second;
            else
            {
                if (edit.nextPlacementId==UINT64_MAX) { error="Terrain placement ID capacity exhausted."; return false; }
                id=edit.nextPlacementId++; bindings.emplace(c.key,id);
            }
            TerrainPropPlacement p;
            p.id=id; p.position={c.x,y,c.z}; p.candidateKey=c.key;
            p.yawRadians=Unit(Hash(c.key+4))*6.28318530718f;
            p.scale=entry.minScale+(entry.maxScale-entry.minScale)*Unit(Hash(c.key+5));
            items[id]={p,entry.profileId};
            if (items.size()>Limit) { error="Terrain placement capacity exceeded."; return false; }
        }
        for (const auto& o:s.overrides)
        {
            // Overrides are explicit authored state and survive regeneration.
            if (o.candidateKey && tombstones.count(o.candidateKey)) continue;
            TerrainPropPlacement p;
            p.id=o.placementId; p.candidateKey=o.candidateKey; p.position=o.position;
            p.yawRadians=o.yawRadians; p.scale=o.scale;
            items[p.id]={p,o.profileId};
        }
    }
    if(discFailed)return false;
    s.bindings.clear();
    std::map<std::uint64_t,std::uint64_t> orderedBindings(bindings.begin(),bindings.end());
    for (const auto& pair:orderedBindings) s.bindings.push_back({pair.first,pair.second});
    s.tombstones.assign(tombstones.begin(),tombstones.end());
    std::sort(s.tombstones.begin(),s.tombstones.end());
    if (!ValidateTerrainPropAuthoring(s,error)) return false;
    std::map<Cell,TerrainPropPage> after;
    for (const auto& pair:items)
    {
        const auto& item=pair.second; auto p=item.placement;
        if (p.id>=edit.nextPlacementId) { error="Override exceeds terrain placement ID high-water mark."; return false; }
        const double cx=std::floor(double(p.position[0])/recipe.cellSize);
        const double cz=std::floor(double(p.position[2])/recipe.cellSize);
        if (cx<INT32_MIN || cx>INT32_MAX || cz<INT32_MIN || cz>INT32_MAX)
        { error="Terrain placement cell coordinate overflow."; return false; }
        const Cell key{static_cast<std::int32_t>(cx),static_cast<std::int32_t>(cz)};
        auto& page=after[key]; page.cellX=key.first; page.cellZ=key.second;
        auto profile=std::find(page.profiles.begin(),page.profiles.end(),item.profile);
        if (profile==page.profiles.end()) { page.profiles.push_back(item.profile); p.profileIndex=std::uint32_t(page.profiles.size()-1); }
        else p.profileIndex=std::uint32_t(profile-page.profiles.begin());
        page.placements.push_back(p);
    }
    std::set<Cell> cells;
    for (const auto& pair:before) cells.insert(pair.first);
    for (const auto& pair:after) cells.insert(pair.first);
    bool hasBounds=false;
    for (const auto& cell:cells)
    {
        const auto old=before.find(cell), now=after.find(cell);
        if (old!=before.end() && now!=after.end() && SamePage(old->second,now->second)) continue;
        TerrainPropPageDelta delta;
        delta.cellX=cell.first; delta.cellZ=cell.second;
        delta.hadBefore=old!=before.end(); delta.hasAfter=now!=after.end();
        if (delta.hadBefore) delta.before=old->second;
        if (delta.hasAfter) delta.after=now->second;
        const TerrainPropBounds cb{cell.first*recipe.cellSize,cell.second*recipe.cellSize,
            float((double(cell.first)+1)*recipe.cellSize),float((double(cell.second)+1)*recipe.cellSize)};
        if (!hasBounds) { edit.changedBounds=cb; hasBounds=true; }
        else
        {
            edit.changedBounds.minX=std::min(edit.changedBounds.minX,cb.minX);
            edit.changedBounds.minZ=std::min(edit.changedBounds.minZ,cb.minZ);
            edit.changedBounds.maxX=std::max(edit.changedBounds.maxX,cb.maxX);
            edit.changedBounds.maxZ=std::max(edit.changedBounds.maxZ,cb.maxZ);
        }
        // Legacy pages and a changed cell size need not match today's grid.
        // Include real anchors from both revisions, especially deleted last
        // placements whose old page is the only remaining spatial evidence.
        for(const auto* page:{&delta.before,&delta.after})
            for(const auto& placement:page->placements)
            {
                edit.changedBounds.minX=std::min(edit.changedBounds.minX,placement.position[0]);
                edit.changedBounds.minZ=std::min(edit.changedBounds.minZ,placement.position[2]);
                edit.changedBounds.maxX=std::max(edit.changedBounds.maxX,placement.position[0]);
                edit.changedBounds.maxZ=std::max(edit.changedBounds.maxZ,placement.position[2]);
            }
        edit.changedPages.push_back(std::move(delta));
    }
    for (auto& pair:after) edit.pages.push_back(std::move(pair.second));
    if (!progress(1)) return false;
    result=std::move(edit);
    return true;
}
}
