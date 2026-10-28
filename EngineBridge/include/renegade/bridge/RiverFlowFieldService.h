#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <WickedEngine.h>

namespace renegade::bridge
{
    struct RiverFlowSettings
    {
        float baseSpeedMetersPerSecond = 1.0f;
        float sampleStepMeters = 0.5f;
        std::size_t maxSamples = 8192;
        bool downstreamReversed = false;
    };

    struct RiverFlowSample
    {
        XMFLOAT3 worldPosition{};
        XMFLOAT2 worldXZUnitTangent{}, worldXZUnitRight{};
        float nativeParameter = 0;
        float arcLengthMeters = 0; // Increasing authored order, horizontal world metres.
        float widthMeters = 0;     // FULL native bank-to-bank world XZ extent.
        float speedMetersPerSecond = 0;
        float surfaceHeight = 0;
        float grade = 0;           // Signed drop per horizontal metre, in flow direction.
    };

    struct RiverFlowRibbonRow
    {
        XMFLOAT3 centre{}, bankOffset{};
        XMFLOAT3 rightBank{}, leftBank{}; // Transform native endpoints individually: float32 order matters.
        float nativeParameter = 0;
    };

    struct RiverFlowQuery
    {
        XMFLOAT2 directionXZ{};
        float speedMetersPerSecond = 0;
        float lateralFraction = 0; // [-1,+1], positive toward (-tz,tx), reversed with flow.
        float alongMeters = 0;    // Distance from the selected upstream end.
        float distanceToBankMeters = 0; // Minimum distance to native banks OR end caps.
        float surfaceHeight = 0;
        float grade = 0;
        bool valid = false;
        bool ambiguous = false;
    };

    // Immutable transient CPU oracle. No scene reference, graphics handle,
    // serializer, terrain operation, material edit or automatic Runtime binding.
    class RiverFlowField final
    {
    public:
        const std::vector<RiverFlowSample>& Samples() const noexcept { return samples_; }
        const std::vector<RiverFlowRibbonRow>& NativeRibbon() const noexcept { return ribbon_; }
        float LengthMeters() const noexcept { return samples_.back().arcLengthMeters; }
        std::uint64_t DeterministicHash() const noexcept;
        RiverFlowQuery Evaluate(XMFLOAT2 worldXZ) const noexcept;
        RiverFlowField(const RiverFlowField&) = delete;
        RiverFlowField& operator=(const RiverFlowField&) = delete;
    private:
        RiverFlowField() = default;
        friend bool BuildRiverFlowFieldSpan(const RiverFlowField&,float,float,
            std::shared_ptr<const RiverFlowField>&,std::string&);
        std::vector<RiverFlowSample> samples_;
        std::vector<RiverFlowRibbonRow> ribbon_;
        bool reversed_ = false;
        friend bool BuildRiverFlowField(const wi::scene::Scene&, wi::ecs::Entity,
            const RiverFlowSettings&, std::shared_ptr<const RiverFlowField>&,
            std::string&, const std::vector<float>&);
    };

    // Call at a stable read point. Copies CURRENT local transforms and invokes
    // Wicked's real evaluator, then ComputeEntityMatrixRecursive, in the same
    // order as native mesh generation / SampleSplineWorldPoints. Output is
    // replaced only on success. Optional multipliers correspond to control nodes;
    // they are linearly interpolated over native local chord-distance knots.
    // Width follows Wicked's own scale.x Catmull interpolation, never a new curve.
    // Limits: 512 nodes, 8192 samples/mesh rows, 131072 evaluations, 8M overlap
    // candidates; malformed, folded, looping and intersecting ribbons fail closed.
    [[nodiscard]] bool BuildRiverFlowField(
        const wi::scene::Scene& scene, wi::ecs::Entity river,
        const RiverFlowSettings& settings,
        std::shared_ptr<const RiverFlowField>& result, std::string& error,
        const std::vector<float>& speedMultipliers = {});
}
