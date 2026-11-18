#include "renegade/bridge/TerrainPropCollisionIndex.h"
#include <algorithm>
#include <cstring>
#include "renegade/bridge/ResourceAssetCacheIdentityService.h"
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <tuple>

namespace renegade::bridge
{
    namespace
    {
        bool Finite(const XMFLOAT3& p)
        { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
        bool Valid(const TerrainPropCollisionBounds& b)
        {
            return Finite(b.minimum)&&Finite(b.maximum)&&b.minimum.x<=b.maximum.x&&
                b.minimum.y<=b.maximum.y&&b.minimum.z<=b.maximum.z;
        }
        bool Overlap(const TerrainPropCollisionBounds& a,const TerrainPropCollisionBounds& b)
        {
            return a.minimum.x<=b.maximum.x&&a.maximum.x>=b.minimum.x&&
                a.minimum.y<=b.maximum.y&&a.maximum.y>=b.minimum.y&&
                a.minimum.z<=b.maximum.z&&a.maximum.z>=b.minimum.z;
        }
        TerrainPropCollisionBounds Union(TerrainPropCollisionBounds a,const TerrainPropCollisionBounds& b)
        {
            a.minimum={std::min(a.minimum.x,b.minimum.x),std::min(a.minimum.y,b.minimum.y),
                std::min(a.minimum.z,b.minimum.z)};
            a.maximum={std::max(a.maximum.x,b.maximum.x),std::max(a.maximum.y,b.maximum.y),
                std::max(a.maximum.z,b.maximum.z)};
            return a;
        }
        float Axis(const XMFLOAT3& p,int axis) { return axis==0?p.x:axis==1?p.y:p.z; }
        bool SegmentBounds(const XMFLOAT3& a,const XMFLOAT3& b,
            const TerrainPropCollisionBounds& bounds,float radius,double maximum,
            double* startFraction=nullptr,double* endFraction=nullptr)
        {
            double enter=0,leave=maximum;
            for(int axis=0;axis<3;++axis)
            {
                const double start=Axis(a,axis), delta=double(Axis(b,axis))-start;
                const double low=double(Axis(bounds.minimum,axis))-radius;
                const double high=double(Axis(bounds.maximum,axis))+radius;
                if(delta==0) { if(start<low||start>high)return false;continue; }
                double first=(low-start)/delta, last=(high-start)/delta;
                if(first>last)std::swap(first,last);
                enter=std::max(enter,first);leave=std::min(leave,last);
                if(enter>leave)return false;
            }
            if(startFraction)*startFraction=enter;
            if(endFraction)*endFraction=leave;
            return true;
        }
        XMFLOAT3 Local(const XMFLOAT3& p,const TerrainPropCollisionIndex::Record& r)
        {
            const double c=std::cos(double(r.yaw)),s=std::sin(double(r.yaw));
            const double x=double(p.x)-r.center.x,z=double(p.z)-r.center.z;
            return {float((c*x-s*z)/r.scale),float((double(p.y)-r.center.y)/r.scale),
                float((s*x+c*z)/r.scale)};
        }
        XMFLOAT3 Rotate(const XMFLOAT3& p,float yaw)
        {
            const float c=std::cos(yaw),s=std::sin(yaw);
            return {c*p.x+s*p.z,p.y,-s*p.x+c*p.z};
        }
    }

    std::size_t TerrainPropCollisionIndex::Bytes() const noexcept
    {
        std::size_t bytes=sizeof(*this)+navigationDigest_.capacity()+1+records_.capacity()*sizeof(Record)+
            profiles_.capacity()*sizeof(Profile)+order_.capacity()*sizeof(std::uint32_t)+
            nodes_.capacity()*sizeof(Node)+layers_.capacity()*sizeof(StableId);
        for(const auto& layer:layers_)bytes+=layer.capacity()+1;
        for(const auto& profile:profiles_)bytes+=profile.source.modelAssetId.capacity()+1;
        return bytes;
    }
    TerrainPropCollisionBounds TerrainPropCollisionIndex::Bounds(std::uint32_t index) const
    {
        const auto& r=records_.at(index);
        const auto& p=profiles_.at(r.profile).source.collision;
        XMFLOAT3 extent=p.halfExtents;
        if(p.shape==TerrainPropShape::TrunkCapsule)
            extent={p.halfExtents.x,p.halfExtents.y+p.halfExtents.x,p.halfExtents.x};
        const double c=std::abs(std::cos(double(r.yaw))),s=std::abs(std::sin(double(r.yaw)));
        const float x=float((c*extent.x+s*extent.z)*r.scale);
        const float y=extent.y*r.scale;
        const float z=float((s*extent.x+c*extent.z)*r.scale);
        return {{r.center.x-x,r.center.y-y,r.center.z-z},
            {r.center.x+x,r.center.y+y,r.center.z+z}};
    }

    std::uint32_t TerrainPropCollisionIndex::BuildNode(std::uint32_t first,std::uint32_t count)
    {
        const auto index=std::uint32_t(nodes_.size());
        nodes_.push_back({});
        auto bounds=Bounds(order_[first]);
        for(std::uint32_t i=1;i<count;++i)bounds=Union(bounds,Bounds(order_[first+i]));
        nodes_[index].bounds=bounds;
        if(count<=8) { nodes_[index].first=first;nodes_[index].count=count;return index; }
        XMFLOAT3 size{bounds.maximum.x-bounds.minimum.x,bounds.maximum.y-bounds.minimum.y,
            bounds.maximum.z-bounds.minimum.z};
        const int axis=size.x>=size.y&&size.x>=size.z?0:size.y>=size.z?1:2;
        const auto middle=first+count/2;
        std::nth_element(order_.begin()+first,order_.begin()+middle,order_.begin()+first+count,
            [&](auto a,auto b) {
                const auto ca=Axis(records_[a].center,axis),cb=Axis(records_[b].center,axis);
                if(ca!=cb)return ca<cb;
                return std::tie(records_[a].layer,records_[a].id)<
                    std::tie(records_[b].layer,records_[b].id);
            });
        const auto left=BuildNode(first,count/2),right=BuildNode(middle,count-count/2);
        nodes_[index].left=left;nodes_[index].right=right;
        return index;
    }

    std::shared_ptr<const TerrainPropCollisionIndex> TerrainPropCollisionIndex::Build(
        const std::vector<TerrainPropDocument>& documents,const Limits& limits,std::string& error)
    {
        if(limits.placements>1000000||limits.queryNodes==0||limits.queryShapes==0)
        { error="Invalid terrain collision index budgets.";return {}; }
        auto index=std::shared_ptr<TerrainPropCollisionIndex>(new TerrainPropCollisionIndex);
        index->limits_=limits;
        std::set<StableId> layers;
        std::size_t total=0;
        std::size_t profileCount=0;
        for(const auto& doc:documents)
        {
            if(!IsValidStableId(doc.layerId)||!layers.insert(doc.layerId).second)
            { error="Invalid or repeated terrain collision layer identity.";return {}; }
            if(doc.profiles.size()>65536-profileCount)
            { error="Terrain collision profile budget exceeded.";return {}; }
            profileCount+=doc.profiles.size();
            std::map<TerrainPropProfileId,TerrainPropShape> lookup;
            for(const auto& profile:doc.profiles)
                if(!lookup.emplace(profile.id,profile.collision.shape).second)
                { error="Repeated terrain collision profile.";return {}; }
            for(const auto& page:doc.pages)
            {
                if(!ValidateTerrainPropPage(page,error))return {};
                for(const auto& placement:page.placements)
                {
                    auto profile=lookup.find(page.profiles.at(placement.profileIndex));
                    if(profile==lookup.end())
                    { error="Missing terrain collision profile.";return {}; }
                    if(profile->second==TerrainPropShape::None)continue;
                    if(total>=limits.placements)
                    { error="Terrain collision placement budget exceeded.";return {}; }
                    ++total;
                }
            }
        }
        // Reserve worst-case storage before creating any native body or index publication.
        const std::size_t estimate=sizeof(*index)+total*(sizeof(Record)+sizeof(Node)+sizeof(std::uint32_t));
        if(estimate>limits.bytes) { error="Terrain collision index memory budget exceeded.";return {}; }
        index->records_.reserve(total);
        index->order_.reserve(total);
        index->nodes_.reserve(total?total/2+1:0);
        using ShapeKey=std::tuple<StableId,int,float,float,float>;
        std::map<ShapeKey,wi::physics::terrain_props::Shape> shapes;
        for(const auto& doc:documents)
        {
            const auto layer=std::uint32_t(index->layers_.size());
            index->layers_.push_back(doc.layerId);
            std::map<TerrainPropProfileId,std::uint32_t> profiles;
            std::set<std::uint64_t> ids;
            for(const auto& source:doc.profiles)
            {
                if(source.id==TerrainPropProfileId{}||!IsValidStableId(source.modelAssetId)||
                    profiles.count(source.id)||index->profiles_.size()>=65536)
                { error="Invalid terrain collision profile identity.";return {}; }
                const auto& collision=source.collision;
                const auto& e=collision.halfExtents;
                if(!Finite(collision.center)||!Finite(e)||e.x<=0||e.y<=0||e.z<=0||
                    !std::isfinite(collision.friction)||collision.friction<0||collision.friction>1||
                    static_cast<unsigned>(collision.surface)>static_cast<unsigned>(ImpactSurfaceType::Character)||
                    (collision.shape!=TerrainPropShape::None&&
                     collision.shape!=TerrainPropShape::TrunkCapsule&&collision.shape!=TerrainPropShape::BoulderBox))
                { error="Invalid terrain collision profile shape.";return {}; }
                Profile profile;profile.source=source;
                if(collision.shape!=TerrainPropShape::None)
                {
                    ShapeKey key{source.modelAssetId,int(collision.shape),e.x,e.y,e.z};
                    auto cached=shapes.find(key);
                    if(cached!=shapes.end())profile.shape=cached->second;
                    else
                    {
                        if(!wi::physics::terrain_props::CreateShape(
                            collision.shape==TerrainPropShape::TrunkCapsule?
                                wi::physics::terrain_props::ShapeKind::Capsule:wi::physics::terrain_props::ShapeKind::Box,
                            collision.halfExtents,profile.shape,error))return {};
                        shapes.emplace(std::move(key),profile.shape);
                        ++index->shapeCount_;
                    }
                }
                profiles.emplace(source.id,std::uint32_t(index->profiles_.size()));
                index->profiles_.push_back(std::move(profile));
            }
            std::set<std::pair<std::int32_t,std::int32_t>> cells;
            for(const auto& page:doc.pages)
            {
              if(!cells.emplace(page.cellX,page.cellZ).second)
              { error="Repeated terrain collision page cell.";return {}; }
              for(const auto& p:page.placements)
              {
                if(!ids.insert(p.id).second||p.id>=doc.nextPlacementId)
                { error="Invalid terrain collision placement identity.";return {}; }
                auto found=profiles.find(page.profiles.at(p.profileIndex));
                if(found==profiles.end()) { error="Missing terrain collision profile.";return {}; }
                const auto& shape=index->profiles_[found->second].source.collision;
                if(shape.shape==TerrainPropShape::None)continue;
                Record record;record.layer=layer;record.profile=found->second;record.id=p.id;
                record.yaw=p.yawRadians;record.scale=p.scale;
                const auto rotated=Rotate(shape.center,record.yaw);
                record.center={p.position[0]+rotated.x*p.scale,p.position[1]+rotated.y*p.scale,
                    p.position[2]+rotated.z*p.scale};
                if(!Finite(record.center)) { error="Terrain collision transform exceeds finite range.";return {}; }
                index->records_.push_back(record);
                if(!Valid(index->Bounds(std::uint32_t(index->records_.size()-1))))
                { error="Terrain collision bounds exceed finite range.";return {}; }
              }
            }
        }
        index->order_.resize(index->records_.size());
        std::iota(index->order_.begin(),index->order_.end(),0u);
        if(!index->records_.empty())index->BuildNode(0,std::uint32_t(index->records_.size()));
        if(index->Bytes()>limits.bytes) { error="Terrain collision index memory budget exceeded.";return {}; }
        // Canonical collision geometry, independent of camera, body residency,
        // authoring recipes, friction and impact surface. This stamps baked nav.
        auto canonical=index->order_;
        std::sort(canonical.begin(),canonical.end(),[&](auto a,auto b){
            const auto& x=index->records_[a];const auto& y=index->records_[b];
            return std::tie(index->layers_[x.layer],x.id)<std::tie(index->layers_[y.layer],y.id);
        });
        std::vector<std::uint8_t> geometry;geometry.reserve(canonical.size()*33);
        const auto number=[&](float value){
            if(value==0)value=0; // canonical positive zero
            std::uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));
            for(int i=0;i<4;++i)geometry.push_back(std::uint8_t(bits>>(8*i)));
        };
        for(auto id:canonical)
        {
            const auto& record=index->records_[id];
            const auto& c=index->profiles_[record.profile].source.collision;
            geometry.push_back(std::uint8_t(c.shape));
            number(record.center.x);number(record.center.y);number(record.center.z);
            number(record.yaw);number(record.scale);
            number(c.halfExtents.x);number(c.halfExtents.y);number(c.halfExtents.z);
        }
        if(!ComputePayloadSha256(geometry,index->navigationDigest_,error))return {};
        if(index->Bytes()>limits.bytes){error="Terrain collision index memory budget exceeded.";return {};}
        error.clear();
        return index;
    }

    bool TerrainPropCollisionIndex::Equivalent(const TerrainPropCollisionIndex& other) const noexcept
    {
        if(records_.size()!=other.records_.size())return false;
        for(std::size_t i=0;i<records_.size();++i)
        {
            const auto& a=records_[i];const auto& b=other.records_[i];
            const auto& x=profiles_[a.profile].source;const auto& y=other.profiles_[b.profile].source;
            if(a.id!=b.id||layers_[a.layer]!=other.layers_[b.layer]||x.id!=y.id||
                x.modelAssetId!=y.modelAssetId||a.center.x!=b.center.x||a.center.y!=b.center.y||
                a.center.z!=b.center.z||a.yaw!=b.yaw||a.scale!=b.scale||
                x.collision.shape!=y.collision.shape||x.collision.friction!=y.collision.friction||
                TerrainPropSurface(x.collision)!=TerrainPropSurface(y.collision)||
                x.collision.halfExtents.x!=y.collision.halfExtents.x||
                x.collision.halfExtents.y!=y.collision.halfExtents.y||
                x.collision.halfExtents.z!=y.collision.halfExtents.z)return false;
        }
        return true;
    }

    bool TerrainPropCollisionIndex::Cast(const XMFLOAT3& from,const XMFLOAT3& to,float radius,
        TerrainPropCollisionHit& output,std::string& error,TerrainPropQueryWork* work) const
    {
        if(work)*work={};
        if(!Finite(from)||!Finite(to)||!std::isfinite(radius)||radius<0)
        { error="Invalid terrain collision segment.";return false; }
        TerrainPropQueryWork measured;
        TerrainPropCollisionHit result;
        double closest=1;
        std::vector<std::uint32_t> stack;
        if(!nodes_.empty())stack.push_back(0);
        while(!stack.empty())
        {
            const auto n=stack.back();stack.pop_back();
            if(++measured.nodes>limits_.queryNodes)
            { if(work)*work=measured;error="Terrain collision query node budget exceeded; query is blocked.";return false; }
            const auto& node=nodes_[n];
            if(!SegmentBounds(from,to,node.bounds,radius,closest))continue;
            if(!node.count) { stack.push_back(node.left);stack.push_back(node.right);continue; }
            for(std::uint32_t i=0;i<node.count;++i)
            {
                const auto recordIndex=order_[node.first+i];
                double begin=0,end=1;
                if(!SegmentBounds(from,to,Bounds(recordIndex),radius,1,&begin,&end))continue;
                const double dx=double(to.x)-from.x,dy=double(to.y)-from.y,dz=double(to.z)-from.z;
                const double length=std::hypot(dx,dy,dz);
                if(length>0)
                {
                    begin=std::max(0.,begin-.0001/length);
                    end=std::min(1.,end+.0001/length);
                }
                const XMFLOAT3 clippedFrom{float(from.x+dx*begin),float(from.y+dy*begin),float(from.z+dz*begin)};
                const XMFLOAT3 clippedTo{float(from.x+dx*end),float(from.y+dy*end),float(from.z+dz*end)};
                if(++measured.shapes>limits_.queryShapes)
                { if(work)*work=measured;error="Terrain collision shape budget exceeded; query is blocked.";return false; }
                const auto& record=records_[recordIndex];
                wi::physics::terrain_props::CastHit hit;
                if(!wi::physics::terrain_props::CastSegment(profiles_[record.profile].shape,
                    Local(clippedFrom,record),Local(clippedTo,record),radius/record.scale,hit,error))
                { if(work)*work=measured;return false; }
                if(!hit.hit)continue;
                const auto& layer=layers_[record.layer];
                const double fraction=begin+(end-begin)*hit.fraction;
                if(result.hit&&(fraction>closest||
                    (fraction==closest&&
                     std::tie(layer,record.id)>=std::tie(result.layer,result.placement))))continue;
                result.hit=true;result.layer=layer;result.placement=record.id;
                result.profile=profiles_[record.profile].source.id;
                result.surface=TerrainPropSurface(profiles_[record.profile].source.collision);
                result.friction=profiles_[record.profile].source.collision.friction;
                closest=fraction;result.fraction=std::clamp(float(fraction),0.f,1.f);result.normal=Rotate(hit.normal,record.yaw);
                result.position={
                    clippedFrom.x+(clippedTo.x-clippedFrom.x)*hit.fraction-result.normal.x*radius,
                    clippedFrom.y+(clippedTo.y-clippedFrom.y)*hit.fraction-result.normal.y*radius,
                    clippedFrom.z+(clippedTo.z-clippedFrom.z)*hit.fraction-result.normal.z*radius};
            }
        }
        if(work)*work=measured;
        output=std::move(result);error.clear();return true;
    }

    bool TerrainPropCollisionIndex::Select(const TerrainPropCollisionBounds& bounds,std::size_t maximum,
        std::vector<std::uint32_t>& output,std::string& error,TerrainPropQueryWork* work) const
    {
        if(work)*work={};
        if(!Valid(bounds)) { error="Invalid terrain collision interest bounds.";return false; }
        TerrainPropQueryWork measured;
        std::vector<std::uint32_t> result,stack;
        if(!nodes_.empty())stack.push_back(0);
        while(!stack.empty())
        {
            auto n=stack.back();stack.pop_back();
            if(++measured.nodes>limits_.queryNodes)
            { if(work)*work=measured;error="Terrain collision interest query budget exceeded.";return false; }
            const auto& node=nodes_[n];
            if(!Overlap(bounds,node.bounds))continue;
            if(!node.count) { stack.push_back(node.left);stack.push_back(node.right);continue; }
            for(std::uint32_t i=0;i<node.count;++i)
            {
                const auto id=order_[node.first+i];
                if(!Overlap(bounds,Bounds(id)))continue;
                if(result.size()>=maximum)
                { if(work)*work=measured;error="Required terrain collision exceeds resident capacity.";return false; }
                result.push_back(id);
            }
        }
        if(work)*work=measured;
        output=std::move(result);error.clear();return true;
    }
}
