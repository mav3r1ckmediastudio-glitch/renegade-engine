#include "renegade/bridge/TerrainPropAuthoring.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
using namespace renegade::bridge;
namespace
{
    int checks=0;
    void Check(bool pass,const char* message) { ++checks; if (!pass) throw std::runtime_error(message); }
    std::map<std::uint64_t,TerrainPropPlacement> Placements(const TerrainPropEdit& edit)
    {
        std::map<std::uint64_t,TerrainPropPlacement> out;
        for (const auto& page:edit.pages) for (const auto& p:page.placements) out.emplace(p.candidateKey,p);
        return out;
    }
    std::vector<std::uint8_t> Bytes(const TerrainPropEdit& edit)
    {
        std::vector<std::uint8_t> out,part; std::string error;
        for (const auto& page:edit.pages)
        {
            if (!EncodeTerrainPropPage(page,part,error)) throw std::runtime_error(error);
            out.insert(out.end(),part.begin(),part.end());
        }
        return out;
    }
}
int main()
{
    try
    {
        TerrainPropAuthoringState authoring;
        authoring.recipe.cellSize=16;
        authoring.recipe.seed=981;
        authoring.recipe.mix.name="Woodland";
        authoring.recipe.mix.entries={{{{1}},.16f,2, .8f,1.3f},{{{2}},.10f,2,1,1}};
        authoring.savedMixes.push_back(authoring.recipe.mix);
        TerrainPropEditRequest request;
        request.bounds={-32,-32,32,32};
        request.expectedRevision=request.currentRevision="revision-a";
        const auto ground=[](float x,float z,float& y){y=.05f*x+.03f*z; return true;};
        std::string error;
        TerrainPropEdit first;
        Check(BuildTerrainPropEdit({},authoring,1,request,ground,first,error),error.c_str());
        const auto original=Placements(first);
        Check(original.size()>50,"representative populated result");
        Check(first.pages.size()>4 && !first.changedPages.empty(),"multiple cell deltas");
        Check(first.changedBounds.minX<=-32 && first.changedBounds.maxX>=32,"changed bounds span adjacent cells");
        for (const auto& pair:original)
        {
            const auto& p=pair.second;
            Check(p.id && p.candidateKey && p.scale>=.8f && p.scale<=1.3f,"generated stable identity and scale");
            Check(std::abs(p.position[1]-(p.position[0]*.05f+p.position[2]*.03f))<.0001f,"finite terrain height");
            for (auto next=original.upper_bound(pair.first);next!=original.end();++next)
            {
                const double dx=double(p.position[0])-next->second.position[0];
                const double dz=double(p.position[2])-next->second.position[2];
                Check(dx*dx+dz*dz>3.9999,"cross-profile and cross-cell spacing");
            }
        }
        TerrainPropEdit repeated,fromEmpty;
        Check(BuildTerrainPropEdit(first.pages,first.authoring,first.nextPlacementId,request,ground,repeated,error),error.c_str());
        Check(Bytes(first)==Bytes(repeated) && repeated.changedPages.empty(),"regenerate preserves IDs and exact pages");
        Check(repeated.nextPlacementId==first.nextPlacementId,"regenerate does not consume IDs");
        Check(BuildTerrainPropEdit({},authoring,1,request,ground,fromEmpty,error),error.c_str());
        Check(Bytes(first)==Bytes(fromEmpty),"deterministic same seed and bounds");
        auto reverse=authoring;
        std::reverse(reverse.recipe.mix.entries.begin(),reverse.recipe.mix.entries.end());
        TerrainPropEdit reversed;
        Check(BuildTerrainPropEdit({},reverse,1,request,ground,reversed,error),error.c_str());
        const auto backwards=Placements(reversed);
        Check(backwards.size()==original.size(),"mix ordering preserves candidate population");
        for (const auto& pair:original)
            Check(backwards.count(pair.first) && backwards.at(pair.first).position==pair.second.position,"candidate keys independent of profile order");
        auto narrow=request;
        narrow.bounds.maxX=0;
        TerrainPropEdit left;
        Check(BuildTerrainPropEdit({},authoring,1,narrow,ground,left,error),error.c_str());
        narrow.bounds.minX=0; narrow.bounds.maxX=32;
        TerrainPropEdit expanded;
        Check(BuildTerrainPropEdit(left.pages,left.authoring,left.nextPlacementId,narrow,ground,expanded,error),error.c_str());
        const auto expandedMap=Placements(expanded),leftMap=Placements(left);
        Check(expandedMap.size()==original.size(),"expansion equals full-region generation");
        for (const auto& pair:original)
            Check(expandedMap.count(pair.first) && expandedMap.at(pair.first).position==pair.second.position,"terrain expansion deterministic positions");
        for (const auto& pair:leftMap)
            Check(expandedMap.at(pair.first).id==pair.second.id,"expansion retains existing IDs");
        std::vector<std::array<float,3>> coarse{{-20,0,0},{20,0,0}},dense;
        for (int x=-20;x<=20;++x) dense.push_back({float(x),0,0});
        std::vector<std::array<float,3>> coarseSamples,denseSamples;
        Check(SampleTerrainPropStroke(coarse,2,coarseSamples,error),error.c_str());
        Check(SampleTerrainPropStroke(dense,2,denseSamples,error),error.c_str());
        Check(coarseSamples==denseSamples && coarseSamples.size()==21,"stroke samples by travelled distance");
        auto paint=request;
        paint.operation=TerrainPropEditOperation::Paint; paint.path=coarse; paint.radius=4; paint.sampleDistance=2;
        TerrainPropEdit coarsePaint,densePaint;
        Check(BuildTerrainPropEdit({},authoring,1,paint,ground,coarsePaint,error),error.c_str());
        paint.path=dense;
        Check(BuildTerrainPropEdit({},authoring,1,paint,ground,densePaint,error),error.c_str());
        Check(Bytes(coarsePaint)==Bytes(densePaint),"paint independent of event rate");
        auto hugePaint=paint; hugePaint.bounds={-1000000,-1000000,1000000,1000000};
        TerrainPropEdit sparsePaint;
        Check(BuildTerrainPropEdit({},authoring,1,hugePaint,ground,sparsePaint,error),error.c_str());
        Check(Bytes(sparsePaint)==Bytes(densePaint),"sparse brush work is bounded by stroke, not terrain area");
        auto erase=paint; erase.operation=TerrainPropEditOperation::Erase;
        erase.path={{0,0,0}}; erase.radius=8;
        TerrainPropEdit erased,regrown;
        Check(BuildTerrainPropEdit(first.pages,first.authoring,first.nextPlacementId,erase,ground,erased,error),error.c_str());
        Check(Placements(erased).size()<original.size() && !erased.authoring.tombstones.empty(),"erase creates tombstones");
        auto reseed=erased.authoring; reseed.recipe.seed+=1;
        Check(BuildTerrainPropEdit(erased.pages,reseed,erased.nextPlacementId,request,ground,regrown,error),error.c_str());
        for (const auto& pair:Placements(regrown))
            Check(pair.second.position[0]*pair.second.position[0]+pair.second.position[2]*pair.second.position[2]>64,"manual clearing survives seed change");
        auto overrideState=first.authoring;
        const auto witness=original.begin()->second;
        TerrainPropOverride override;
        override.candidateKey=witness.candidateKey; override.placementId=witness.id;
        override.profileId={{1}}; override.position={20,7,20}; override.scale=3; override.yawRadians=2;
        overrideState.overrides.push_back(override);
        TerrainPropEdit overridden;
        Check(BuildTerrainPropEdit(first.pages,overrideState,first.nextPlacementId,request,ground,overridden,error),error.c_str());
        Check(Placements(overridden).at(witness.candidateKey).position==override.position &&
            Placements(overridden).at(witness.candidateKey).id==witness.id,"override survives regeneration across cell bounds");
        auto reproject=request; reproject.operation=TerrainPropEditOperation::Reproject;
        TerrainPropEdit projected;
        Check(BuildTerrainPropEdit(overridden.pages,overridden.authoring,overridden.nextPlacementId,reproject,
            [](float,float,float& y){y=12;return true;},projected,error),error.c_str());
        for (const auto& pair:Placements(projected))
            Check(pair.second.position[1]==12 && pair.second.id==Placements(overridden).at(pair.first).id,"reprojection preserves IDs");
        Check(projected.authoring.overrides.front().position[1]==12,"reproject override persistence");
        TerrainPropEdit edge;
        Check(BuildTerrainPropEdit({},authoring,1,request,
            [](float x,float z,float& y){y=0;return x>=0 && x<=8 && z>=0 && z<=8;},edge,error),error.c_str());
        for (const auto& pair:Placements(edge))
            Check(pair.second.position[0]>=0 && pair.second.position[0]<=8 &&
                pair.second.position[2]>=0 && pair.second.position[2]<=8,"terrain edge admits no extrapolation");
        auto cancel=request; cancel.progress=[](float progress){return progress<.2f;};
        TerrainPropEdit untouched=first;
        Check(!BuildTerrainPropEdit(first.pages,first.authoring,first.nextPlacementId,cancel,ground,untouched,error),"cancel staging");
        Check(Bytes(untouched)==Bytes(first) && untouched.nextPlacementId==first.nextPlacementId,"cancel leaves output and highwater intact");
        cancel=request; cancel.expectedRevision="stale";
        Check(!BuildTerrainPropEdit(first.pages,first.authoring,first.nextPlacementId,cancel,ground,untouched,error),"stale revision rejected");
        Check(Bytes(untouched)==Bytes(first),"stale edit leaves output intact");
        auto bad=authoring; bad.recipe.candidateStep=std::numeric_limits<float>::quiet_NaN();
        Check(!ValidateTerrainPropAuthoring(bad,error),"nonfinite recipe rejected");
        bad=authoring; bad.savedMixes.push_back(bad.savedMixes.front());
        Check(!ValidateTerrainPropAuthoring(bad,error),"duplicate mix name rejected");
        bad=authoring; bad.bindings={{10,7},{11,7}};
        Check(!ValidateTerrainPropAuthoring(bad,error),"duplicate bound IDs rejected");
        auto invalidPages=first.pages;
        invalidPages.push_back(invalidPages.front());
        Check(!BuildTerrainPropEdit(invalidPages,first.authoring,first.nextPlacementId,request,ground,untouched,error),"duplicate cells rejected");
        invalidPages=first.pages;
        invalidPages.front().placements.front().position[0]=std::numeric_limits<float>::max();
        Check(!BuildTerrainPropEdit(invalidPages,first.authoring,first.nextPlacementId,request,ground,untouched,error),
            "finite out-of-budget coordinates rejected before spatial integer conversion");
        TerrainPropEdit highwater;
        Check(BuildTerrainPropEdit({},authoring,first.nextPlacementId,paint,ground,highwater,error),error.c_str());
        for (const auto& pair:Placements(highwater))
            Check(pair.second.id>=first.nextPlacementId,"post-undo allocations never reuse retired IDs");
        auto lowered=first.authoring;
        lowered.recipe.mix.entries[0].densityPerSquareMetre=0;
        TerrainPropEdit low,restored;
        Check(BuildTerrainPropEdit(first.pages,lowered,first.nextPlacementId,request,ground,low,error),error.c_str());
        low.authoring.recipe=first.authoring.recipe;
        Check(BuildTerrainPropEdit(low.pages,low.authoring,low.nextPlacementId,request,ground,restored,error),error.c_str());
        for (const auto& pair:original)
            Check(Placements(restored).at(pair.first).id==pair.second.id,"density down/up retains original candidate IDs");
        auto eraseAll=request;
        eraseAll.operation=TerrainPropEditOperation::Erase; eraseAll.path={{0,0,0}}; eraseAll.radius=100;
        TerrainPropEdit empty;
        Check(BuildTerrainPropEdit(first.pages,first.authoring,first.nextPlacementId,eraseAll,ground,empty,error),error.c_str());
        Check(empty.pages.empty() && empty.nextPlacementId==first.nextPlacementId,"erase final placements preserves highwater");
        Check(empty.changedPages.size()==first.pages.size() &&
            std::all_of(empty.changedPages.begin(),empty.changedPages.end(),[](const auto& d){return d.hadBefore && !d.hasAfter;}),
            "empty final page removal delta");
        std::cout<<checks<<" terrain prop authoring checks passed\n";
        return 0;
    }
    catch (const std::exception& e)
    { std::cerr<<"Terrain prop authoring failure: "<<e.what()<<"\n"; return 1; }
}
