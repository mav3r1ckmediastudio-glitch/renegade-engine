#include "renegade/bridge/RiverFlowFieldService.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <unordered_set>

namespace renegade::bridge
{
namespace
{
    using namespace DirectX;
    constexpr std::size_t MaximumNodes = 512, MaximumSamples = 8192;
    constexpr std::size_t MaximumEvaluations = 131072, MaximumPairs = 8000000;
    constexpr float CurveTolerance = 0.002f;
    void Require(bool value, const char* message)
    {
        if (!value) throw std::runtime_error(message);
    }
    bool Finite(XMFLOAT3 p)
    {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
            std::abs(p.x) <= 100000 && std::abs(p.y) <= 100000 && std::abs(p.z) <= 100000;
    }
    XMFLOAT2 XZ(XMFLOAT3 p) { return {p.x,p.z}; }
    double Cross(XMFLOAT2 a, XMFLOAT2 b, XMFLOAT2 c)
    {
        return (double(b.x)-a.x)*(double(c.y)-a.y) -
            (double(b.y)-a.y)*(double(c.x)-a.x);
    }
    float Distance(XMFLOAT2 a, XMFLOAT2 b)
    {
        return std::hypot(a.x-b.x,a.y-b.y);
    }
    float Projection(XMFLOAT2 p, XMFLOAT2 a, XMFLOAT2 b)
    {
        const double x=double(b.x)-a.x, z=double(b.y)-a.y;
        const double d=x*x+z*z;
        return d > 0 ? float(std::clamp(((double(p.x)-a.x)*x+
            (double(p.y)-a.y)*z)/d,0.0,1.0)) : 0;
    }
    XMFLOAT2 Lerp(XMFLOAT2 a, XMFLOAT2 b, float t)
    {
        return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};
    }
    float SegmentDistance(XMFLOAT2 p, XMFLOAT2 a, XMFLOAT2 b)
    {
        return Distance(p,Lerp(a,b,Projection(p,a,b)));
    }
    bool Barycentric(XMFLOAT2 p, XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c,
        double& u, double& v, double& w)
    {
        const double det=Cross(XZ(a),XZ(b),XZ(c));
        if (std::abs(det) < 1e-10) return false;
        u=Cross(p,XZ(b),XZ(c))/det;
        v=Cross(XZ(a),p,XZ(c))/det;
        w=1-u-v;
        return u>=-1e-6 && v>=-1e-6 && w>=-1e-6;
    }
    bool OnSegment(XMFLOAT2 a, XMFLOAT2 b, XMFLOAT2 p)
    {
        return std::abs(Cross(a,b,p))<=1e-8 &&
            p.x>=std::min(a.x,b.x)-1e-6 && p.x<=std::max(a.x,b.x)+1e-6 &&
            p.y>=std::min(a.y,b.y)-1e-6 && p.y<=std::max(a.y,b.y)+1e-6;
    }
    bool Intersects(XMFLOAT2 a, XMFLOAT2 b, XMFLOAT2 c, XMFLOAT2 d)
    {
        const double abC=Cross(a,b,c), abD=Cross(a,b,d);
        const double cdA=Cross(c,d,a), cdB=Cross(c,d,b);
        if (((abC>0 && abD<0)||(abC<0 && abD>0)) &&
            ((cdA>0 && cdB<0)||(cdA<0 && cdB>0))) return true;
        return OnSegment(a,b,c)||OnSegment(a,b,d)||OnSegment(c,d,a)||OnSegment(c,d,b);
    }
    struct Quad
    {
        XMFLOAT3 p[4];
        float minX, maxX, minZ, maxZ;
        std::size_t index;
    };
    Quad MakeQuad(const RiverFlowRibbonRow& a, const RiverFlowRibbonRow& b, std::size_t index)
    {
        Quad q{{a.rightBank,a.leftBank,b.rightBank,b.leftBank},0,0,0,0,index};
        q.minX=q.maxX=q.p[0].x; q.minZ=q.maxZ=q.p[0].z;
        for (const auto& p:q.p) {
            q.minX=std::min(q.minX,p.x); q.maxX=std::max(q.maxX,p.x);
            q.minZ=std::min(q.minZ,p.z); q.maxZ=std::max(q.maxZ,p.z);
        }
        return q;
    }
    bool TrianglesOverlap(const XMFLOAT3* a, const XMFLOAT3* b)
    {
        for (int i=0;i<3;++i) for (int j=0;j<3;++j)
            if (Intersects(XZ(a[i]),XZ(a[(i+1)%3]),XZ(b[j]),XZ(b[(j+1)%3])))
                return true;
        double u,v,w;
        return Barycentric(XZ(a[0]),b[0],b[1],b[2],u,v,w) ||
            Barycentric(XZ(b[0]),a[0],a[1],a[2],u,v,w);
    }
    void ValidateTransform(const wi::scene::TransformComponent& t)
    {
        const auto& q=t.rotation_local;
        const double norm=double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w;
        Require(Finite(t.translation_local) && Finite(t.scale_local) &&
            t.scale_local.x>1e-5f && t.scale_local.y>1e-5f && t.scale_local.z>1e-5f &&
            std::isfinite(norm) && norm>1e-8 && std::abs(norm-1)<0.001,
            "Flow requires finite positions, positive scales and normalized rotations");
    }
    void ValidateAncestry(const wi::scene::Scene& scene, wi::ecs::Entity entity)
    {
        std::unordered_set<wi::ecs::Entity> visited;
        while (entity!=wi::ecs::INVALID_ENTITY) {
            Require(visited.size()<512 && visited.insert(entity).second,
                "Flow root ancestry is cyclic or exceeds 512 transforms");
            const auto* t=scene.transforms.GetComponent(entity);
            Require(t!=nullptr,"Flow root ancestor transform is missing");
            ValidateTransform(*t);
            const auto* h=scene.hierarchy.GetComponent(entity);
            if (!h) break;
            entity=h->parentID;
        }
    }
    void ValidateTopology(const std::vector<RiverFlowSample>& samples,
        const std::vector<RiverFlowRibbonRow>& rows)
    {
        std::size_t pairs=0;
        // Sweep projected segment bounds rather than an unbounded all-pairs scan.
        struct Bound { float minX,maxX,minZ,maxZ; std::size_t index; };
        std::vector<Bound> segments;
        for (std::size_t i=1;i<samples.size();++i) {
            const auto a=XZ(samples[i-1].worldPosition), b=XZ(samples[i].worldPosition);
            segments.push_back({std::min(a.x,b.x),std::max(a.x,b.x),
                std::min(a.y,b.y),std::max(a.y,b.y),i-1});
        }
        std::sort(segments.begin(),segments.end(),[](const auto& a,const auto& b) {
            return a.minX==b.minX ? a.index<b.index : a.minX<b.minX; });
        for (std::size_t i=0;i<segments.size();++i) {
            const auto& a=segments[i];
            for (std::size_t j=i+1;j<segments.size() && segments[j].minX<=a.maxX;++j) {
                Require(++pairs<=MaximumPairs,"Flow topology work budget exceeded");
                const auto& b=segments[j];
                if (a.index+1==b.index || b.index+1==a.index ||
                    a.maxZ<b.minZ || b.maxZ<a.minZ) continue;
                Require(!Intersects(XZ(samples[a.index].worldPosition),
                    XZ(samples[a.index+1].worldPosition),XZ(samples[b.index].worldPosition),
                    XZ(samples[b.index+1].worldPosition)),
                    "Projected flow curve self-intersects; branching is unsupported");
            }
        }
        std::vector<Quad> quads;
        for (std::size_t i=1;i<rows.size();++i) {
            auto q=MakeQuad(rows[i-1],rows[i],i-1);
            const auto area0=Cross(XZ(q.p[0]),XZ(q.p[1]),XZ(q.p[2]));
            const auto area1=Cross(XZ(q.p[2]),XZ(q.p[1]),XZ(q.p[3]));
            Require(std::abs(area0)>1e-10 && std::abs(area1)>1e-10 && area0*area1>0,
                "Native ribbon folds or has degenerate projected triangles");
            quads.push_back(q);
        }
        std::sort(quads.begin(),quads.end(),[](const auto& a,const auto& b) {
            return a.minX==b.minX ? a.index<b.index : a.minX<b.minX; });
        for (std::size_t i=0;i<quads.size();++i) {
            const auto& a=quads[i];
            for (std::size_t j=i+1;j<quads.size() && quads[j].minX<=a.maxX;++j) {
                Require(++pairs<=MaximumPairs,"Flow ribbon overlap work budget exceeded");
                const auto& b=quads[j];
                if (a.index+1==b.index || b.index+1==a.index ||
                    a.maxZ<b.minZ || b.maxZ<a.minZ) continue;
                const XMFLOAT3 at[2][3]={{a.p[0],a.p[1],a.p[2]},{a.p[2],a.p[1],a.p[3]}};
                const XMFLOAT3 bt[2][3]={{b.p[0],b.p[1],b.p[2]},{b.p[2],b.p[1],b.p[3]}};
                for (const auto& x:at) for (const auto& y:bt)
                    Require(!TrianglesOverlap(x,y),
                        "Native river banks overlap; ambiguous hairpins/junctions are unsupported");
            }
        }
    }
}
bool BuildRiverFlowField(const wi::scene::Scene& scene, wi::ecs::Entity river,
    const RiverFlowSettings& settings, std::shared_ptr<const RiverFlowField>& result,
    std::string& error, const std::vector<float>& multipliers)
{
    error.clear();
    try {
        Require(std::isfinite(settings.baseSpeedMetersPerSecond) &&
            settings.baseSpeedMetersPerSecond>=0 && settings.baseSpeedMetersPerSecond<=50 &&
            std::isfinite(settings.sampleStepMeters) && settings.sampleStepMeters>=.001f &&
            settings.sampleStepMeters<=100 && settings.maxSamples>=2 &&
            settings.maxSamples<=MaximumSamples,"Flow settings exceed finite speed/step/sample limits");
        const auto* source=scene.splines.GetComponent(river);
        const auto* material=scene.materials.GetComponent(river);
        Require(source && material &&
            material->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER,
            "Flow requires a native WATER spline");
        const auto count=source->spline_node_entities.size();
        Require(count>=2 && count<=MaximumNodes && !source->IsLooped() && !source->IsFilled() &&
            source->mesh_generation_vertical_subdivision==0 &&
            source->mesh_generation_subdivision>=1 && source->mesh_generation_subdivision<=100 &&
            source->terrain_modifier_amount==0 && std::isfinite(source->width) &&
            source->width>0 && source->width<=50 && std::isfinite(source->rotation),
            "Unsupported native ribbon topology, node count, width or terrain modifier");
        const auto rowCount=(count-1)*std::size_t(source->mesh_generation_subdivision);
        Require(rowCount>=2 && rowCount<=MaximumSamples && rowCount<=settings.maxSamples,
            "Native ribbon row count exceeds flow sample budget");
        Require(multipliers.empty() || multipliers.size()==count,
            "Flow speed profile must have one multiplier per control node");
        for (float speed:multipliers) Require(std::isfinite(speed) && speed>=0 && speed<=10 &&
            speed*settings.baseSpeedMetersPerSecond<=50,"Flow speed multiplier is outside [0,10] or speed exceeds 50m/s");
        ValidateAncestry(scene,river);
        const auto rootWorld=scene.ComputeEntityMatrixRecursive(river);
        wi::scene::SplineComponent curve;
        std::unordered_set<wi::ecs::Entity> nodes;
        XMFLOAT3 previous{};
        for (auto entity:source->spline_node_entities) {
            const auto* node=scene.transforms.GetComponent(entity);
            const auto* hierarchy=scene.hierarchy.GetComponent(entity);
            Require(node && hierarchy && hierarchy->parentID==river && nodes.insert(entity).second,
                "Flow controls must be unique direct children of the spline root");
            ValidateTransform(*node);
            auto transform=*node;
            transform.SetDirty(); transform.UpdateTransform(); transform.ApplyTransform();
            XMFLOAT3 world;
            XMStoreFloat3(&world,XMVector3TransformCoord(XMLoadFloat3(&transform.translation_local),rootWorld));
            Require(Finite(world),"Flow control world position is nonfinite or outside +/-100km");
            if (!curve.spline_node_transforms.empty())
                Require(Distance(XZ(previous),XZ(world))>=.0001f,
                    "Duplicate or vertically coincident projected flow controls");
            previous=world;
            curve.spline_node_transforms.push_back(transform);
        }
        curve.PrecomputeSplineNodeDistances();
        Require(std::isfinite(curve.precomputed_total_distance) && curve.precomputed_total_distance>0,
            "Native spline distance is degenerate");
        std::vector<float> knots{0};
        float sum=0;
        for (std::size_t i=0;i+1<count;++i) {
            sum+=curve.precomputed_node_distances[i];
            knots.push_back(sum/curve.precomputed_total_distance);
        }
        auto field=std::shared_ptr<RiverFlowField>(new RiverFlowField);
        field->reversed_=settings.downstreamReversed;
        std::size_t evaluations=0;
        struct Eval { RiverFlowSample sample; RiverFlowRibbonRow row; XMFLOAT3 localRight,localLeft,localNormal; };
        auto evaluate=[&](float t) {
            Require(++evaluations<=MaximumEvaluations,"Flow native evaluation budget exceeded");
            const auto m=curve.EvaluateSplineAt(t);
            const auto tangent=XMVector3Normalize(m.r[2]);
            const auto rotation=XMQuaternionRotationAxis(tangent,source->rotation);
            const auto offset=XMVector3Rotate(m.r[0]*source->width,rotation);
            Eval e{};
            e.row.nativeParameter=e.sample.nativeParameter=t;
            XMStoreFloat3(&e.localRight,m.r[3]+offset);
            XMStoreFloat3(&e.localLeft,m.r[3]-offset);
            XMStoreFloat3(&e.localNormal,XMVector3Normalize(XMVector3Rotate(m.r[1],rotation)));
            XMStoreFloat3(&e.row.centre,XMVector3TransformCoord(m.r[3],rootWorld));
            XMStoreFloat3(&e.row.bankOffset,XMVector3TransformNormal(offset,rootWorld));
            XMStoreFloat3(&e.row.rightBank,XMVector3TransformCoord(XMLoadFloat3(&e.localRight),rootWorld));
            XMStoreFloat3(&e.row.leftBank,XMVector3TransformCoord(XMLoadFloat3(&e.localLeft),rootWorld));
            Require(Finite(e.row.centre) && Finite(e.row.bankOffset) &&
                Finite(e.row.rightBank) && Finite(e.row.leftBank) &&
                Finite(e.localRight) && Finite(e.localLeft) && Finite(e.localNormal),
                "Native spline evaluation produced invalid geometry");
            XMFLOAT3 worldForward;
            XMStoreFloat3(&worldForward,XMVector3TransformNormal(m.r[2],rootWorld));
            Require(-worldForward.z*e.row.bankOffset.x+worldForward.x*e.row.bankOffset.z>1e-10f,
                "Native river is inverted or has no positive horizontal bank orientation");
            e.sample.worldPosition=e.row.centre;
            e.sample.surfaceHeight=e.row.centre.y;
            e.sample.widthMeters=Distance(XZ(e.row.rightBank),XZ(e.row.leftBank));
            Require(e.sample.widthMeters>=.0001f && e.sample.widthMeters<=1000,
                "Native interpolated width is zero, negative or exceeds world limits");
            auto key=std::upper_bound(knots.begin(),knots.end(),t);
            const auto k=std::min<std::size_t>(count-2,
                key==knots.begin()?0:std::size_t(key-knots.begin()-1));
            const float u=std::clamp((t-knots[k])/(knots[k+1]-knots[k]),0.f,1.f);
            const auto& transforms=curve.spline_node_transforms;
            const float nativeWidthScale=count==2 ?
                transforms[0].scale_local.x+(transforms[1].scale_local.x-transforms[0].scale_local.x)*t :
                XMVectorGetX(XMVectorCatmullRom(
                    XMVectorReplicate(transforms[k?k-1:0].scale_local.x),
                    XMVectorReplicate(transforms[k].scale_local.x),
                    XMVectorReplicate(transforms[k+1].scale_local.x),
                    XMVectorReplicate(transforms[std::min(k+2,count-1)].scale_local.x),u));
            Require(std::isfinite(nativeWidthScale) && nativeWidthScale>0,
                "Native Catmull width overshoots through zero");
            const float multiplier=multipliers.empty()?1.f:
                multipliers[k]+(multipliers[k+1]-multipliers[k])*u;
            e.sample.speedMetersPerSecond=settings.baseSpeedMetersPerSecond*multiplier;
            return e;
        };
        // Include every EXACT native mesh parameter. Detect native backturn
        // correction before publishing an analytic centreline that disagrees.
        XMVECTOR previousPlane=XMVectorZero();
        for (std::size_t i=0;i<rowCount;++i) {
            const auto e=evaluate(float(i)/float(rowCount-1));
            const auto right=XMLoadFloat3(&e.localRight), left=XMLoadFloat3(&e.localLeft);
            if (i>0) Require(XMVectorGetX(XMPlaneDotCoord(previousPlane,right))>=0 &&
                XMVectorGetX(XMPlaneDotCoord(previousPlane,left))>=0,
                "Native backturn correction changes ribbon banks; smooth or reroute the bend before flow");
            const auto mid=(right+left)*.5f;
            previousPlane=XMPlaneFromPointNormal(mid,
                XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&e.localNormal),right-left)));
            field->ribbon_.push_back(e.row);
        }
        auto append=[&](const Eval& e) {
            Require(field->samples_.size()<settings.maxSamples,"Adaptive flow sample budget exceeded");
            field->samples_.push_back(e.sample);
        };
        // Native row parameters plus control knots seed adaptive native sampling.
        std::vector<float> seeds=knots;
        for (const auto& row:field->ribbon_) seeds.push_back(row.nativeParameter);
        std::sort(seeds.begin(),seeds.end());
        seeds.erase(std::unique(seeds.begin(),seeds.end()),seeds.end());
        std::function<void(const Eval&,const Eval&,unsigned)> refine;
        refine=[&](const Eval& a,const Eval& b,unsigned depth) {
            const float t0=a.sample.nativeParameter,t1=b.sample.nativeParameter;
            const auto q=evaluate(t0+(t1-t0)*.25f), m=evaluate((t0+t1)*.5f),
                r=evaluate(t0+(t1-t0)*.75f);
            const auto p0=XZ(a.row.centre),p1=XZ(b.row.centre);
            const float length=Distance(p0,XZ(q.row.centre))+Distance(XZ(q.row.centre),XZ(m.row.centre))+
                Distance(XZ(m.row.centre),XZ(r.row.centre))+Distance(XZ(r.row.centre),p1);
            const float deviation=std::max({SegmentDistance(XZ(q.row.centre),p0,p1),
                SegmentDistance(XZ(m.row.centre),p0,p1),SegmentDistance(XZ(r.row.centre),p0,p1)});
            if (length<=settings.sampleStepMeters && deviation<=CurveTolerance) {
                Require(Distance(p0,p1)>=.00001f,"Flow sample segment is projected-degenerate");
                append(b); return;
            }
            Require(depth<24 && m.sample.nativeParameter>t0 && m.sample.nativeParameter<t1,
                "Native flow curve cannot resolve within precision/depth limits");
            refine(a,m,depth+1); refine(m,b,depth+1);
        };
        auto a=evaluate(seeds.front()); append(a);
        for (std::size_t i=1;i<seeds.size();++i) {
            auto b=evaluate(seeds[i]); refine(a,b,0); a=b;
        }
        double arc=0;
        for (std::size_t i=0;i<field->samples_.size();++i) {
            auto& s=field->samples_[i];
            if (i) arc+=Distance(XZ(field->samples_[i-1].worldPosition),XZ(s.worldPosition));
            Require(arc<=100000 && std::isfinite(arc),"Flow length exceeds 100km");
            s.arcLengthMeters=float(arc);
            if (i) Require(s.arcLengthMeters>field->samples_[i-1].arcLengthMeters,
                "Flow arc length exceeds float32 precision");
            const auto& before=field->samples_[i?i-1:0].worldPosition;
            const auto& after=field->samples_[std::min(i+1,field->samples_.size()-1)].worldPosition;
            const float length=Distance(XZ(before),XZ(after));
            Require(length>=.00001f,"Flow tangent cusp is undefined");
            const float sign=field->reversed_?-1.f:1.f;
            s.worldXZUnitTangent={sign*(after.x-before.x)/length,sign*(after.z-before.z)/length};
            s.worldXZUnitRight={-s.worldXZUnitTangent.y,s.worldXZUnitTangent.x};
            s.grade=sign*(before.y-after.y)/length;
            Require(std::isfinite(s.grade),"Nonfinite flow grade");
        }
        ValidateTopology(field->samples_,field->ribbon_);
        result=std::move(field);
        return true;
    } catch (const std::exception& e) {
        error=e.what(); return false;
    }
}

RiverFlowQuery RiverFlowField::Evaluate(XMFLOAT2 point) const noexcept
{
    RiverFlowQuery query;
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) return query;
    std::size_t hit=std::numeric_limits<std::size_t>::max();
    float lateral=0,height=0;
    float bankDistance=std::numeric_limits<float>::max();
    for (std::size_t i=1;i<ribbon_.size();++i) {
        const auto q=MakeQuad(ribbon_[i-1],ribbon_[i],i-1);
        bankDistance=std::min({bankDistance,SegmentDistance(point,XZ(q.p[0]),XZ(q.p[2])),
            SegmentDistance(point,XZ(q.p[1]),XZ(q.p[3]))});
        const int indices[2][3]={{0,1,2},{2,1,3}};
        for (const auto& triangle:indices) {
            double u,v,w;
            if (!Barycentric(point,q.p[triangle[0]],q.p[triangle[1]],q.p[triangle[2]],u,v,w)) continue;
            if (hit!=std::numeric_limits<std::size_t>::max()) {
                if (i-1>hit+1) { query.ambiguous=true; return query; }
                continue; // Stable lowest row/triangle index at shared edges.
            }
            hit=i-1;
            const float signs[]={1,-1,1,-1};
            lateral=float(u*signs[triangle[0]]+v*signs[triangle[1]]+w*signs[triangle[2]]);
            height=float(u*q.p[triangle[0]].y+v*q.p[triangle[1]].y+w*q.p[triangle[2]].y);
        }
    }
    if (hit==std::numeric_limits<std::size_t>::max()) return query;
    const auto& first=ribbon_.front(); const auto& last=ribbon_.back();
    bankDistance=std::min({bankDistance,SegmentDistance(point,XZ(first.rightBank),
        XZ(first.leftBank)),SegmentDistance(point,XZ(last.rightBank),XZ(last.leftBank))});
    float best=std::numeric_limits<float>::max(), blend=0;
    std::size_t segment=0;
    for (std::size_t i=1;i<samples_.size();++i) {
        const auto a=XZ(samples_[i-1].worldPosition), b=XZ(samples_[i].worldPosition);
        const float t=Projection(point,a,b), distance=Distance(point,Lerp(a,b,t));
        if (distance<best) { best=distance; blend=t; segment=i-1; }
    }
    const auto& a=samples_[segment]; const auto& b=samples_[segment+1];
    const auto direction=Lerp(a.worldXZUnitTangent,b.worldXZUnitTangent,blend);
    const float magnitude=std::hypot(direction.x,direction.y);
    if (!(magnitude>1e-5f)) return query;
    query.directionXZ={direction.x/magnitude,direction.y/magnitude};
    query.speedMetersPerSecond=a.speedMetersPerSecond+(b.speedMetersPerSecond-a.speedMetersPerSecond)*blend;
    query.alongMeters=a.arcLengthMeters+(b.arcLengthMeters-a.arcLengthMeters)*blend;
    if (reversed_) query.alongMeters=LengthMeters()-query.alongMeters;
    query.lateralFraction=std::clamp(lateral*(reversed_?-1.f:1.f),-1.f,1.f);
    query.distanceToBankMeters=bankDistance;
    query.surfaceHeight=height;
    query.grade=a.grade+(b.grade-a.grade)*blend;
    query.valid=true;
    return query;
}

std::uint64_t RiverFlowField::DeterministicHash() const noexcept
{
    std::uint64_t hash=14695981039346656037ull;
    auto word=[&](std::uint32_t bits) {
        for (int i=0;i<4;++i) { hash=(hash^std::uint8_t(bits&255))*1099511628211ull; bits>>=8; }
    };
    auto scalar=[&](float value) {
        std::uint32_t bits=0; if (value!=0) std::memcpy(&bits,&value,sizeof(bits));
        word(bits); // Canonical little endian float32; normalize signed zero.
    };
    word(1); word(reversed_?1:0); word(std::uint32_t(samples_.size()));
    for (const auto& s:samples_) {
        scalar(s.worldPosition.x); scalar(s.worldPosition.y); scalar(s.worldPosition.z);
        scalar(s.worldXZUnitTangent.x); scalar(s.worldXZUnitTangent.y);
        scalar(s.worldXZUnitRight.x); scalar(s.worldXZUnitRight.y);
        scalar(s.nativeParameter); scalar(s.arcLengthMeters); scalar(s.widthMeters);
        scalar(s.speedMetersPerSecond); scalar(s.surfaceHeight); scalar(s.grade);
    }
    word(std::uint32_t(ribbon_.size()));
    for (const auto& row:ribbon_) {
        scalar(row.centre.x); scalar(row.centre.y); scalar(row.centre.z);
        scalar(row.bankOffset.x); scalar(row.bankOffset.y); scalar(row.bankOffset.z);
        scalar(row.rightBank.x); scalar(row.rightBank.y); scalar(row.rightBank.z);
        scalar(row.leftBank.x); scalar(row.leftBank.y); scalar(row.leftBank.z);
        scalar(row.nativeParameter);
    }
    return hash;
}
}
