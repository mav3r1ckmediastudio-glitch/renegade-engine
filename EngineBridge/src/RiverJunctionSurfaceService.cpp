#include "renegade/bridge/RiverJunctionSurfaceService.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
namespace renegade::bridge
{
namespace
{
    constexpr double Grid=1e-5;
    struct Point { double x,z; };
    struct Edge { Point a,b; std::size_t reach; };
    struct Interval { double low,high; std::size_t lower,upper; };
    struct Cell { std::size_t event; std::size_t lower,upper; };
    double Cross(Point a,Point b,Point c) { return (b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x); }
    double At(const Edge& e,double x)
    {
        return e.a.z+(e.b.z-e.a.z)*std::clamp((x-e.a.x)/(e.b.x-e.a.x),0.0,1.0);
    }
    double Snap(double v) { return std::round(v/Grid)*Grid; }
    void Require(bool value,const char* reason) { if(!value)throw std::runtime_error(reason); }
    void Canonical(std::vector<double>& values)
    {
        for(auto& v:values)v=Snap(v);
        std::sort(values.begin(),values.end());
        values.erase(std::unique(values.begin(),values.end(),[](double a,double b){return std::abs(a-b)<Grid*.5;}),values.end());
    }
    XMFLOAT2 Velocity(const RiverFlowNetwork& network,XMFLOAT2 p,float radius)
    {
        const auto& join=network.Junctions().front();
        const auto& outlet=network.FindReach(join.outgoing)->upstream;
        double x=0,z=0,weight=0;
        for(const auto& reach:network.Reaches()) {
            const auto q=reach.field->Evaluate(p);
            if(!q.valid)continue;
            const double w=std::max(double(q.distanceToBankMeters),1e-7);
            x+=w*q.directionXZ.x*q.speedMetersPerSecond;
            z+=w*q.directionXZ.y*q.speedMetersPerSecond;weight+=w;
        }
        // Local welding can move an edge query by micrometres. A nearest native
        // centreline fallback supplies a finite boundary trace only, never coverage.
        if(weight==0) {
            double nearest=INFINITY;
            for(const auto& reach:network.Reaches())for(const auto& sample:reach.field->Samples()) {
                const double d=std::hypot(double(p.x)-sample.worldPosition.x,double(p.y)-sample.worldPosition.z);
                if(d<nearest) { nearest=d;x=sample.worldXZUnitTangent.x*sample.speedMetersPerSecond;
                    z=sample.worldXZUnitTangent.y*sample.speedMetersPerSecond; }
            }
        } else { x/=weight;z/=weight; }
        const double distance=std::hypot(double(p.x)-join.anchor.x,double(p.y)-join.anchor.z);
        double blend=std::clamp(1-distance/radius,0.0,1.0);blend=blend*blend*(3-2*blend);
        return {float(x*(1-blend)+outlet.directionXZ.x*outlet.speedMetersPerSecond*blend),
            float(z*(1-blend)+outlet.directionXZ.y*outlet.speedMetersPerSecond*blend)};
    }
}
    RiverJunctionQuery RiverJunctionSurface::Evaluate(XMFLOAT2 p) const noexcept
    {
        if(!std::isfinite(p.x)||!std::isfinite(p.y))return {};
        for(std::size_t k=0;k<indices_.size();k+=3) {
            const auto ia=indices_[k],ib=indices_[k+1],ic=indices_[k+2];
            const auto& a=positions_[ia];const auto& b=positions_[ib];const auto& c=positions_[ic];
            const Point aa{a.x,a.z},bb{b.x,b.z},cc{c.x,c.z},q{p.x,p.y};
            const double det=Cross(aa,bb,cc);
            const double u=Cross(q,bb,cc)/det,v=Cross(aa,q,cc)/det,w=1-u-v;
            if(u>=-1e-7 && v>=-1e-7 && w>=-1e-7)
                return {{float(u*velocities_[ia].x+v*velocities_[ib].x+w*velocities_[ic].x),
                    float(u*velocities_[ia].y+v*velocities_[ib].y+w*velocities_[ic].y)},true};
        }
        return {};
    }
    double RiverJunctionSurface::AreaSquareMetres() const noexcept
    {
        double area=0;
        for(std::size_t k=0;k<indices_.size();k+=3) {
            const auto& a=positions_[indices_[k]];const auto& b=positions_[indices_[k+1]];const auto& c=positions_[indices_[k+2]];
            area+=std::abs(Cross({a.x,a.z},{b.x,b.z},{c.x,c.z}))*.5;
        }
        return area;
    }
    bool BuildRiverJunctionSurface(const RiverFlowNetwork& network,
        std::shared_ptr<const RiverJunctionSurface>& result,std::string& error)
    {
        try {
            Require(network.Reaches().size()==3 && network.Junctions().size()==1 &&
                network.Junctions().front().incoming.size()==2,"G7B requires one two-tributary confluence");
            const auto anchor=network.Junctions().front().anchor;
            float radius=0;
            for(const auto& reach:network.Reaches()) {
                radius=std::max({radius,reach.upstream.widthMeters*3,reach.downstream.widthMeters*3});
                for(const auto& row:reach.field->NativeRibbon())
                    Require(std::abs(row.rightBank.y-anchor.y)<.0001f && std::abs(row.leftBank.y-anchor.y)<.0001f,
                        "G7B requires an exact common flat plane (0.1 mm tolerance)");
            }
            for(const auto& id:network.Junctions().front().incoming) {
                const auto p=network.FindReach(id)->downstream.centre;
                Require(std::hypot(p.x-anchor.x,p.z-anchor.z)<.0001f,"G7B does not bridge endpoint gaps");
            }
            std::vector<Edge> edges;std::vector<double> events{0};
            auto point=[&](XMFLOAT3 p){return Point{double(p.x)-anchor.x,double(p.z)-anchor.z};};
            auto add=[&](XMFLOAT3 a,XMFLOAT3 b,std::size_t r) {
                edges.push_back({point(a),point(b),r});events.push_back(edges.back().a.x);events.push_back(edges.back().b.x);
            };
            for(std::size_t r=0;r<network.Reaches().size();++r) {
                const auto& rows=network.Reaches()[r].field->NativeRibbon();
                Require(edges.size()+rows.size()*2<=1024,"G7B native boundary edge budget exceeded");
                add(rows.front().leftBank,rows.front().rightBank,r);
                add(rows.back().leftBank,rows.back().rightBank,r);
                for(std::size_t n=1;n<rows.size();++n) {
                    add(rows[n-1].leftBank,rows[n].leftBank,r);add(rows[n-1].rightBank,rows[n].rightBank,r);
                }
            }
            std::size_t work=0;
            auto tick=[&](){Require(++work<=16000000,"G7B bounded geometry work budget exceeded");};
            // All source vertices and boundary crossings partition X into slabs
            // in which the native union boundary has a fixed edge ordering.
            for(std::size_t a=0;a<edges.size();++a)for(std::size_t b=a+1;b<edges.size();++b) {
                tick();const auto& e=edges[a];const auto& f=edges[b];
                if(e.reach==f.reach)continue;
                const Point d{e.b.x-e.a.x,e.b.z-e.a.z},v{f.b.x-f.a.x,f.b.z-f.a.z};
                const double det=d.x*v.z-d.z*v.x;
                if(std::abs(det)<1e-12)continue;
                const Point q{f.a.x-e.a.x,f.a.z-e.a.z};
                const double t=(q.x*v.z-q.z*v.x)/det,u=(q.x*d.z-q.z*d.x)/det;
                if(t>=0 && t<=1 && u>=0 && u<=1) {
                    const double x=e.a.x+t*d.x,z=e.a.z+t*d.z;
                    Require(std::hypot(x,z)<=radius,"Undeclared native crossing outside confluence neighbourhood");
                    events.push_back(x);
                }
            }
            // Keep exact sweep crossings for boundary evaluation. Rounding X
            // before evaluating Z makes two approaching intervals overlap on a
            // shared event plane, creating a three-face edge after welding.
            // Only exported boundary vertices use the spatial weld grid.
            std::sort(events.begin(),events.end());
            events.erase(std::unique(events.begin(),events.end(),[](double a,double b){return std::abs(a-b)<1e-10;}),events.end());
            Require(events.size()>=2 && events.size()<=4096,"G7B sweep event budget exceeded");
            std::vector<Cell> cells;std::vector<std::vector<double>> cuts(events.size());
            for(std::size_t n=0;n+1<events.size();++n) {
                const double mid=(events[n]+events[n+1])*.5;
                std::vector<Interval> intervals;
                for(std::size_t r=0;r<network.Reaches().size();++r) {
                    std::vector<std::pair<double,std::size_t>> hits;
                    for(std::size_t k=0;k<edges.size();++k) {
                        tick();const auto& e=edges[k];
                        if(e.reach==r && mid>std::min(e.a.x,e.b.x) && mid<std::max(e.a.x,e.b.x))
                            hits.push_back({At(e,mid),k});
                    }
                    std::sort(hits.begin(),hits.end());
                    Require(hits.size()%2==0,"Native boundary sweep is not a closed simple ribbon");
                    for(std::size_t k=0;k<hits.size();k+=2)
                        intervals.push_back({hits[k].first,hits[k+1].first,hits[k].second,hits[k+1].second});
                }
                std::sort(intervals.begin(),intervals.end(),[](const auto& a,const auto& b){return a.low<b.low;});
                std::vector<Interval> merged;
                for(const auto& span:intervals) {
                    if(merged.empty() || span.low>merged.back().high+1e-9)merged.push_back(span);
                    else if(span.high>merged.back().high) {merged.back().high=span.high;merged.back().upper=span.upper;}
                }
                for(const auto& span:merged) {
                    cells.push_back({n,span.lower,span.upper});
                    for(auto e:{n,n+1}) {
                        cuts[e].push_back(At(edges[span.lower],events[e]));
                        cuts[e].push_back(At(edges[span.upper],events[e]));
                    }
                }
            }
            for(std::size_t n=0;n<events.size();++n)if(std::abs(events[n])<Grid*.5)cuts[n].push_back(0);
            // Event planes which export to the same X share all vertical
            // cuts. This keeps subdivisions conforming after float/grid welding.
            std::map<long long,std::vector<double>> planeCuts;
            for(size_t n=0;n<events.size();++n){auto& group=planeCuts[std::llround(events[n]/Grid)];group.insert(group.end(),cuts[n].begin(),cuts[n].end());}
            for(auto& group:planeCuts)Canonical(group.second);
            for(size_t n=0;n<events.size();++n)cuts[n]=planeCuts.at(std::llround(events[n]/Grid));
            auto surface=std::make_shared<RiverJunctionSurface>();
            std::map<std::pair<long long,long long>,std::uint32_t> vertices;
            auto vertex=[&](Point p,bool interior=false) {
                const auto key=std::make_pair(std::llround(p.x/Grid),std::llround(p.z/Grid));
                auto it=vertices.find(key);if(!interior && it!=vertices.end())return it->second;
                Require(surface->positions_.size()<65536,"G7B vertex budget exceeded");
                const auto index=static_cast<std::uint32_t>(surface->positions_.size());if(!interior)vertices[key]=index;
                XMFLOAT3 world{float(anchor.x+(interior?p.x:key.first*Grid)),anchor.y,float(anchor.z+(interior?p.z:key.second*Grid))};
                surface->positions_.push_back(world);
                const auto velocity=Velocity(network,{world.x,world.z},radius);
                Require(std::isfinite(velocity.x)&&std::isfinite(velocity.y)&&std::hypot(velocity.x,velocity.y)>1e-5,
                    "G7B produced stopped or invalid flow");
                surface->velocities_.push_back(velocity);return index;
            };
            for(const auto& cell:cells) {
                const auto n=cell.event;const double x0=Snap(events[n]),x1=Snap(events[n+1]);
                if(x1-x0<Grid*.5)continue;
                const double a=Snap(At(edges[cell.lower],events[n])),b=Snap(At(edges[cell.lower],events[n+1]));
                const double c=Snap(At(edges[cell.upper],events[n+1])),d=Snap(At(edges[cell.upper],events[n]));
                std::vector<Point> contour{{x0,a},{x1,b}};
                for(auto z:cuts[n+1])if(z>b+Grid*.5 && z<c-Grid*.5)contour.push_back({x1,z});
                contour.push_back({x1,c});contour.push_back({x0,d});
                for(auto it=cuts[n].rbegin();it!=cuts[n].rend();++it)
                    if(*it>a+Grid*.5 && *it<d-Grid*.5)contour.push_back({x0,*it});
                const auto centre=vertex({(x0+x1)*.5,(a+b+c+d)*.25},true);
                for(std::size_t k=0;k<contour.size();++k) {
                    const auto va=vertex(contour[k]),vb=vertex(contour[(k+1)%contour.size()]);
                    const auto& p=surface->positions_[centre];const auto& q=surface->positions_[vb];const auto& t=surface->positions_[va];
                    const double area=Cross({p.x,p.z},{q.x,q.z},{t.x,t.z});
                    if(centre==va || centre==vb || va==vb || std::abs(area)<1e-12)continue;
                    Require(area<0,"G7B float precision inverted a native union triangle");
                    Require(surface->indices_.size()/3<65536,"G7B triangle budget exceeded");
                    surface->indices_.insert(surface->indices_.end(),{centre,vb,va});
                }
            }
            Require(!surface->indices_.empty(),"Empty junction surface");
            std::map<std::pair<std::uint32_t,std::uint32_t>,unsigned> adjacency;
            for(std::size_t k=0;k<surface->indices_.size();k+=3)for(int e=0;e<3;++e) {
                auto a=surface->indices_[k+e],b=surface->indices_[k+(e+1)%3];if(a>b)std::swap(a,b);
                if(++adjacency[{a,b}]>2) {
                    const auto& p=surface->positions_[a];const auto& q=surface->positions_[b];
                    std::string reason="G7B non-manifold union edge ids="+std::to_string(a)+","+std::to_string(b)+" from="+std::to_string(p.x)+","+std::to_string(p.z)+" to="+std::to_string(q.x)+","+std::to_string(q.z);
                    for(size_t n=0;n<surface->indices_.size();n+=3){
                     bool aa=false,bb=false;for(int t=0;t<3;++t){aa|=surface->indices_[n+t]==a;bb|=surface->indices_[n+t]==b;}
                     if(aa&&bb){reason+=" tri";for(int t=0;t<3;++t){const auto& v=surface->positions_[surface->indices_[n+t]];reason+=" "+std::to_string(v.x)+","+std::to_string(v.z);}}
                    }
                    throw std::runtime_error(reason);
                }
            }
            std::map<std::uint32_t,unsigned> boundaryDegree;
            for(const auto& edge:adjacency)if(edge.second==1) {++boundaryDegree[edge.first.first];++boundaryDegree[edge.first.second];}
            for(const auto& v:boundaryDegree)if(v.second!=2) {
                const auto& p=surface->positions_[v.first];
                std::string reason="G7B union has an open seam or T junction at "+std::to_string(p.x)+","+std::to_string(p.z)+" degree="+std::to_string(v.second);
                for(const auto& edge:adjacency)if(edge.second==1 && (edge.first.first==v.first||edge.first.second==v.first)) {
                    const auto& q=surface->positions_[edge.first.first==v.first?edge.first.second:edge.first.first];
                    reason+=" to "+std::to_string(q.x)+","+std::to_string(q.z);
                }
                throw std::runtime_error(reason);
            }
            result=std::move(surface);error.clear();return true;
        } catch(const std::exception& ex) {error=ex.what();return false;}
    }
}
