#include "renegade/bridge/RiverFlowNetworkService.h"
#include "renegade/bridge/RiverFlowSpanService.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>

namespace renegade::bridge
{
namespace
{
    constexpr float HeightTolerance = .005f, JoinTolerance = .02f;
    void Require(bool condition, const std::string& reason)
    {
        if (!condition) throw std::runtime_error(reason);
    }
    bool ValidId(const std::string& id)
    {
        if (id.empty() || id.size() > 128) return false;
        return std::all_of(id.begin(), id.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == ':';
        });
    }
    float Dot(XMFLOAT2 a, XMFLOAT2 b) { return a.x*b.x + a.y*b.y; }
    RiverFlowNetworkPort Port(const RiverFlowField& field, bool authoredFirst)
    {
        const auto& row = authoredFirst ? field.NativeRibbon().front() : field.NativeRibbon().back();
        const auto& sample = authoredFirst ? field.Samples().front() : field.Samples().back();
        return {row.centre, row.rightBank, row.leftBank, sample.worldXZUnitTangent,
            sample.widthMeters, sample.speedMetersPerSecond};
    }
}
    const RiverFlowNetworkReach* RiverFlowNetwork::FindReach(const std::string& id) const noexcept
    {
        const auto it = std::lower_bound(reaches_.begin(), reaches_.end(), id,
            [](const auto& reach, const auto& key) { return reach.id < key; });
        return it != reaches_.end() && it->id == id ? &*it : nullptr;
    }

    std::uint64_t RiverFlowNetwork::DeterministicHash() const noexcept
    {
        // Explicit little-endian lengths and values; no ECS identity/pointers/padding.
        std::uint64_t hash = 14695981039346656037ull;
        auto byte = [&](unsigned char v) { hash = (hash ^ v)*1099511628211ull; };
        auto integer = [&](std::uint64_t value) {
            for (int n=0; n<8; ++n) byte(static_cast<unsigned char>(value >> (n*8)));
        };
        auto string = [&](const std::string& value) {
            integer(value.size()); for (unsigned char c : value) byte(c);
        };
        integer(1); integer(reaches_.size());
        for (const auto& reach : reaches_) { string(reach.id); integer(reach.field->DeterministicHash()); }
        integer(junctions_.size());
        for (const auto& junction : junctions_) {
            string(junction.id); integer(junction.incoming.size());
            for (const auto& id : junction.incoming) string(id);
            string(junction.outgoing);
        }
        return hash;
    }

    bool BuildRiverFlowNetwork(const wi::scene::Scene& scene,
        const std::vector<RiverFlowNetworkReachInput>& inputs,
        const std::vector<RiverFlowNetworkJunctionInput>& joins,
        std::shared_ptr<const RiverFlowNetwork>& result, std::string& error)
    {
        try {
            Require(!inputs.empty() && inputs.size() <= 16, "Network requires 1..16 reaches");
            Require(joins.size() <= 15, "Network junction limit is 15");
            // Check nested container/string caps before making any input copies.
            for (const auto& input : inputs)
                Require(ValidId(input.id), "Reach identity is missing or malformed");
            for (const auto& join : joins) {
                Require(ValidId(join.id), "Missing or malformed junction identity");
                Require(!join.incoming.empty() && join.incoming.size() <= 8,
                    "Junction "+join.id+": requires 1..8 incoming reaches and exactly one outgoing");
                Require(ValidId(join.outgoing), "Junction "+join.id+": unknown outgoing reach identity");
                for (const auto& id : join.incoming)
                    Require(ValidId(id), "Junction "+join.id+": unknown incoming reach identity");
            }
            auto network = std::make_shared<RiverFlowNetwork>();
            std::map<std::string, std::size_t> indices;
            std::map<wi::ecs::Entity,std::vector<std::pair<float,float>>> entities;
            auto sorted = inputs;
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
            for (const auto& input : sorted) {
                Require(ValidId(input.id), "Reach identity is missing or malformed");
                Require(indices.emplace(input.id, indices.size()).second, "Duplicate reach identity: "+input.id);
                Require(input.river != wi::ecs::INVALID_ENTITY,"Invalid native river entity: "+input.id);
                Require(std::isfinite(input.firstNativeParameter)&&std::isfinite(input.lastNativeParameter)&&
                    input.firstNativeParameter>=0&&input.lastNativeParameter<=1&&
                    input.lastNativeParameter-input.firstNativeParameter>=1e-6f,"Invalid native reach span");
                auto& spans=entities[input.river];
                for(auto interval:spans)Require(std::max(interval.first,input.firstNativeParameter)>=
                    std::min(interval.second,input.lastNativeParameter),"Overlapping or duplicate native reach spans");
                spans.push_back({input.firstNativeParameter,input.lastNativeParameter});
                RiverFlowNetworkReach reach; reach.id = input.id;
                std::string reason;
                const bool built = BuildRiverFlowField(scene, input.river, input.flow, reach.field, reason);
                Require(built, "Reach "+input.id+": "+reason);
                if(input.firstNativeParameter!=0||input.lastNativeParameter!=1){
                    auto full=reach.field;
                    const bool clipped=BuildRiverFlowFieldSpan(*full,input.firstNativeParameter,input.lastNativeParameter,reach.field,reason);
                    Require(clipped,"Reach "+input.id+": "+reason);
                }
                const float level = reach.field->NativeRibbon().front().centre.y;
                for (const auto& row : reach.field->NativeRibbon()) {
                    Require(std::abs(row.leftBank.y-level) <= HeightTolerance &&
                        std::abs(row.rightBank.y-level) <= HeightTolerance,
                        "Reach "+input.id+": non-flat native water surface is unsupported");
                }
                reach.upstream = Port(*reach.field, !input.flow.downstreamReversed);
                reach.downstream = Port(*reach.field, input.flow.downstreamReversed);
                network->reaches_.push_back(std::move(reach));
            }
            auto canonical = joins;
            std::sort(canonical.begin(), canonical.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
            std::set<std::string> junctionIds, usedUpstream, usedDownstream;
            std::vector<std::vector<std::size_t>> edges(inputs.size());
            std::vector<std::size_t> indegree(inputs.size(), 0);
            for (auto& join : canonical) {
                Require(ValidId(join.id) && junctionIds.insert(join.id).second,
                    "Missing, malformed or duplicate junction identity: "+join.id);
                Require(!join.incoming.empty() && join.incoming.size() <= 8,
                    "Junction "+join.id+": requires 1..8 incoming reaches and exactly one outgoing");
                Require(indices.count(join.outgoing) != 0, "Junction "+join.id+": unknown outgoing reach");
                Require(usedUpstream.insert(join.outgoing).second,
                    "Junction "+join.id+": upstream port already connected");
                std::sort(join.incoming.begin(), join.incoming.end());
                Require(std::adjacent_find(join.incoming.begin(), join.incoming.end()) == join.incoming.end(),
                    "Junction "+join.id+": duplicate incoming reach");
                const auto target = indices.at(join.outgoing);
                for (const auto& id : join.incoming) {
                    Require(indices.count(id) != 0, "Junction "+join.id+": unknown incoming reach "+id);
                    Require(id != join.outgoing, "Junction "+join.id+": self connection");
                    Require(usedDownstream.insert(id).second,
                        "Junction "+join.id+": downstream port already connected (split unsupported)");
                    edges[indices.at(id)].push_back(target); ++indegree[target];
                }
                const auto& outlet = network->reaches_[target].upstream;
                network->junctions_.push_back({join.id, join.incoming, join.outgoing, outlet.centre});
            }
            // Canonical Kahn traversal: deterministic regardless of input/entity order.
            std::set<std::size_t> ready;
            for (std::size_t n=0; n<indegree.size(); ++n) if (!indegree[n]) ready.insert(n);
            while (!ready.empty()) {
                const auto n = *ready.begin(); ready.erase(ready.begin());
                network->order_.push_back(network->reaches_[n].id);
                for (const auto next : edges[n]) if (--indegree[next] == 0) ready.insert(next);
            }
            Require(network->order_.size() == inputs.size(), "Directed waterway cycle is unsupported");
            for (const auto& join : network->junctions_) {
                const auto& outlet = network->FindReach(join.outgoing)->upstream;
                std::vector<const RiverFlowNetworkPort*> ports{&outlet};
                for (const auto& id : join.incoming) {
                    const auto& inlet = network->FindReach(id)->downstream;
                    Require(inlet.speedMetersPerSecond > 0 && outlet.speedMetersPerSecond > 0,
                        "Junction "+join.id+": zero-speed connection is unsupported");
                    Require(Dot(inlet.directionXZ, outlet.directionXZ) > .05f,
                        "Junction "+join.id+": incoming flow opposes or is perpendicular to outlet");
                    ports.push_back(&inlet);
                }
                for (std::size_t a=0; a<ports.size(); ++a) for (std::size_t b=a+1; b<ports.size(); ++b) {
                    const auto& x = ports[a]->centre; const auto& y = ports[b]->centre;
                    Require(std::hypot(x.x-y.x, x.z-y.z) <= JoinTolerance,
                        "Junction "+join.id+": native cap centres do not meet within 20 mm");
                    Require(std::abs(x.y-y.y) <= HeightTolerance,
                        "Junction "+join.id+": native water levels differ by more than 5 mm");
                    if (a > 0)
                        Require(Dot(ports[a]->directionXZ, ports[b]->directionXZ) < .9999f,
                            "Junction "+join.id+": coincident parallel incoming directions are ambiguous");
                }
            }
            result = std::move(network); error.clear(); return true;
        } catch (const std::exception& ex) {
            error = ex.what(); return false;
        }
    }
}
