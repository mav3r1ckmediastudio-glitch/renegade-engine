#pragma once
#include <WickedEngine.h>
#include <array>
#include <vector>
#include <cstdint>

namespace renegade::bridge
{
    // Convex triangular column: five outward planes, dot(n,p)+w <= 0.
    // Native ribbon triangles define the top; linked channel depth bounds below.
    struct RiverWaterPrism
    {
        std::array<XMFLOAT4,5> planes{};
        XMFLOAT4 scattering{};
        XMFLOAT4 extinction{};
        XMFLOAT3 minimum{}, maximum{};
    };
    bool MakeRiverWaterPrism(const XMFLOAT3& a, const XMFLOAT3& b,
        const XMFLOAT3& c, float depth, const XMFLOAT4& color,
        const XMFLOAT3& extinction, RiverWaterPrism& result);
    bool IntersectRiverWaterPrism(const RiverWaterPrism& prism,
        const XMFLOAT3& origin, const XMFLOAT3& direction,
        float maximumDistance, float& entry, float& exit);
    // Read-only, bounded. Unlinked ribbons have no implied underwater volume.
    std::vector<RiverWaterPrism> CaptureRiverWaterPrisms(const wi::scene::Scene& scene);

    // Per-render-path resources. No global ocean/weather state or scene mutation.
    class RiverUnderwaterRenderer
    {
    public:
        void Update(wi::RenderPath3D& path);
        const std::string& Error() const noexcept { return error_; }
        std::size_t PrismCount() const noexcept { return prisms_.size(); }
    private:
        wi::graphics::GPUBuffer buffer_;
        wi::graphics::Shader shader_;
        std::vector<RiverWaterPrism> prisms_;
        std::uint64_t fingerprint_ = 0;
        std::uint32_t nodeCount_ = 0;
        bool shaderAttempted_ = false;
        std::string error_;
    };
}
