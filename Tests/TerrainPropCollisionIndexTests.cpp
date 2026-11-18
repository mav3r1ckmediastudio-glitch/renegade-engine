#include "renegade/bridge/TerrainPropCollisionIndex.h"
#include <wiPhysics.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace b=renegade::bridge;
namespace
{
    int checks=0;
    void Require(bool value,const char* message)
    { ++checks;if(!value)throw std::runtime_error(message); }
    bool Near(float a,float c) { return std::abs(a-c)<.002f; }
    b::TerrainPropDocument Document()
    {
        b::TerrainPropDocument d;
        d.layerId="11111111-1111-4111-8111-111111111111";
        d.nextPlacementId=1000001;
        b::TerrainPropProfile tree;
        tree.id[0]=1;
        tree.modelAssetId="22222222-2222-4222-8222-222222222222";
        tree.collision={b::TerrainPropShape::TrunkCapsule,{0,1.5f,0},{.25f,1.25f,1}};
        auto rock=tree;rock.id[0]=2;
        rock.modelAssetId="33333333-3333-4333-8333-333333333333";
        rock.collision={b::TerrainPropShape::BoulderBox,{0,0,0},{1,1,1}};
        auto bush=tree;bush.id[0]=3;bush.collision.shape=b::TerrainPropShape::None;
        d.profiles={tree,rock,bush};
        b::TerrainPropPage p;
        p.cellX=-1;p.profiles={tree.id,rock.id,bush.id};
        p.placements={{1,0,{-10,0,0},0,1},{2,1,{50,0,0},0,8},
            {3,2,{0,0,0},0,1}};
        d.pages={p};
        return d;
    }
}
int main()
{
    try
    {
        wi::physics::Initialize();
        auto d=Document();
        std::string error;
        auto index=b::TerrainPropCollisionIndex::Build({d},error);
        Require(index!=nullptr,"index preparation");
        Require(index->Count()==2&&index->ShapeCount()==2,"bush does not allocate collision record/shape");
        b::TerrainPropCollisionHit hit;
        b::TerrainPropQueryWork work;
        Require(index->Cast({-20,1,0},{100,1,0},0,hit,error,&work)&&hit.hit&&
            hit.placement==1&&hit.layer==d.layerId&&hit.profile==d.profiles[0].id,
            "nearest long shot has stable identity");
        Require(index->Cast({-20,4,0},{0,4,0},0,hit,error)&&!hit.hit,"tree canopy noncolliding");
        Require(index->Cast({-1,0,0},{1,0,0},0,hit,error)&&!hit.hit,"bush noncolliding");
        Require(index->Cast({0,0,0},{100,0,0},0,hit,error)&&hit.hit&&
            hit.placement==2&&Near(hit.position.x,42),"small rock scales to boulder");
        Require(index->Cast({0,0,0},{100,0,0},1,hit,error)&&hit.hit&&
            hit.placement==2&&Near(hit.fraction,.41f),"swept projectile hits nonresident boulder");
        Require(index->Cast({-10,1,0},{-10,1,0},0,hit,error)&&
            hit.hit&&hit.fraction==0,"zero-length overlap");
        const auto retained=hit;
        Require(!index->Cast({0,0,0},{100,0,0},-1,hit,error)&&
            hit.placement==retained.placement,"invalid query retains previous output");
        std::vector<std::uint32_t> selected{123};
        Require(index->Select({{40,-10,-10},{60,10,10}},4,selected,error)&&
            selected.size()==1&&index->Placement(selected[0]).id==2,"bounds use scaled footprint");
        const auto previous=selected;
        Require(!index->Select({{-100,-100,-100},{100,100,100}},1,selected,error)&&
            selected==previous,"resident-capacity query fails without partial result");
        auto invalid=d;invalid.pages.push_back(d.pages[0]);
        Require(!b::TerrainPropCollisionIndex::Build({invalid},error),"duplicate pages rejected");
        invalid=d;invalid.profiles[0].collision.halfExtents.x=-1;
        Require(!b::TerrainPropCollisionIndex::Build({invalid},error),"invalid shape rejected");
        Require(!b::TerrainPropCollisionIndex::Build({d,d},error),"duplicate layer rejected");
        auto other=d;other.layerId="44444444-4444-4444-8444-444444444444";
        auto shared=b::TerrainPropCollisionIndex::Build({d,other},error);
        Require(shared&&shared->Count()==4&&shared->ShapeCount()==2,"layers share immutable shape products");
        Require(shared->Cast({-20,1,0},{0,1,0},0,hit,error)&&hit.layer==d.layerId,
            "equal-distance hits use deterministic stable-ID tie break");
        b::TerrainPropCollisionIndex::Limits limits;
        limits.bytes=1;
        Require(!b::TerrainPropCollisionIndex::Build({d},limits,error),"memory budget fails closed");
        limits={};limits.queryShapes=1;
        auto bounded=b::TerrainPropCollisionIndex::Build({d,other},limits,error);
        hit=retained;
        Require(bounded&&!bounded->Cast({-20,1,0},{0,1,0},0,hit,error)&&
            hit.placement==retained.placement,"incomplete query is blocked, never clear");
        auto rotated=d;rotated.pages[0].placements={{2,1,{0,0,0},1.57079632679f,1}};
        rotated.profiles[1].collision.halfExtents={3,1,.5f};
        auto turns=b::TerrainPropCollisionIndex::Build({rotated},error);
        Require(turns&&turns->Cast({-5,0,0},{5,0,0},0,hit,error)&&
            hit.hit&&Near(hit.position.x,-.5f),"yaw rotates shape and normal consistently");
        auto large=Document();large.pages[0].placements.clear();
        for(std::uint64_t i=0;i<100000;++i)
            large.pages[0].placements.push_back({i+1,0,
                {float(i%1000)*4,0,float(i/1000)*4},0,1});
        const auto start=std::chrono::steady_clock::now();
        auto forest=b::TerrainPropCollisionIndex::Build({large},error);
        const auto milliseconds=std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-start).count();
        Require(forest&&forest->Count()==100000,"100k compact query records");
        Require(forest->Cast({-2,1,200},{2,1,200},0,hit,error,&work)&&hit.hit&&
            hit.placement==50001,"negative-coordinate ray crosses indexed cells");
        Require(work.nodes<256&&work.shapes<32,"short query work does not follow total placement count");
        Require(forest->Cast({2,1,0},{std::numeric_limits<float>::max(),1,0},0,hit,error)&&
            hit.hit&&hit.placement==2&&Near(hit.position.x,3.75f),
            "finite maximum-range rays clip to local shape bounds before native math");
        wi::scene::Scene empty;
        Require(wi::physics::terrain_props::BodyCapacity(empty).allocated==0,
            "all-map query index requires no resident Jolt bodies");
        std::cout<<"PASS "<<checks<<" assertions; 100k index bytes="<<forest->Bytes()
            <<" build_ms="<<milliseconds<<" query_nodes="<<work.nodes
            <<" query_shapes="<<work.shapes<<'\n';
        return 0;
    }
    catch(const std::exception& e)
    { std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1; }
}
