#pragma once
// G4 fork-lightning preview: deterministic, damage-free, world-space geometry.
// Does not touch the owner-approved G4 sheet-lightning shader or controller.
#include <WickedEngine.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace renegade::bridge
{
    struct ForkLightningSettings
    {
        bool enabled = false;
        float eventsPerMinute = 9.0f;
        float cloudBaseMetres = 2280.0f;
        float branchDensity=1.0f;
        float minDistanceMetres = 2800.0f;
        float maxDistanceMetres = 4800.0f;
        std::uint32_t seed = 0x5EEDB017u;
    };

    struct ForkLightningSegment
    {
        XMFLOAT3 from{};
        XMFLOAT3 to{};
        float radiusMetres = 1.1f;
        float energy = 1.0f;
        std::uint8_t order = 0;
    };

    struct ForkLightningFrame
    {
        std::uint64_t id = 0;
        XMFLOAT3 origin{};
        XMFLOAT3 contact{};
        float ageSeconds = 0.0f;
        float brightness = 0.0f;
        bool visible = false;
        std::vector<ForkLightningSegment> segments;
    };

    class ForkLightningController
    {
    public:
        void Configure(const ForkLightningSettings& requested) noexcept
        {
            config_ = requested;
            auto clamp = [](float v,float def,float low,float high){
                return std::clamp(std::isfinite(v)?v:def, low,high);
            };
            config_.eventsPerMinute=clamp(requested.eventsPerMinute,9.0f,0.0f,30.0f);
            config_.cloudBaseMetres=clamp(requested.cloudBaseMetres,2280.0f,200.0f,5000.0f);
            config_.branchDensity=clamp(requested.branchDensity,1,0,2);
            config_.minDistanceMetres=clamp(requested.minDistanceMetres,2800.0f,500.0f,9000.0f);
            config_.maxDistanceMetres=clamp(requested.maxDistanceMetres,4800.0f,
                                           config_.minDistanceMetres,12000.0f);
            Reset();
        }

        void Reset() noexcept
        {
            frame_={};
            seed_=config_.seed ? config_.seed : 0x5EEDB017u;
            next_=config_.enabled && config_.eventsPerMinute>0.0f ?
                2.0f+Random01()*1.5f : 0.0f;
        }

        void Advance(float deltaSeconds,const XMFLOAT3& observer,
                     const XMFLOAT3& forward, float groundY=0.0f) noexcept
        {
            if(!config_.enabled || config_.eventsPerMinute<=0.0f ||
               !std::isfinite(deltaSeconds) || deltaSeconds<=0.0f) return;
            const float dt=std::min(deltaSeconds,0.25f);
            if(frame_.id!=0)
            {
                frame_.ageSeconds+=dt;
                const float age=frame_.ageSeconds;
                // Rapid return-stroke, short dark gap, weaker second stroke.
                const float first=age<0.11f ? 1.0f : 0.0f;
                const float second=age>=0.15f && age<0.265f ? 0.65f : 0.0f;
                const float tail=age>=0.265f && age<0.40f ?
                    0.08f*(0.40f-age)/0.135f : 0.0f;
                frame_.brightness=std::max(first,std::max(second,tail));
                frame_.visible=frame_.brightness>0.001f;
            }
            next_-=dt;
            if(next_<=0.0f)
            {
                Create(observer,forward,std::isfinite(groundY)?groundY:0.0f);
                next_=60.0f/config_.eventsPerMinute*(0.65f+0.75f*Random01());
            }
        }

        [[nodiscard]] const ForkLightningFrame& Frame() const noexcept {return frame_;}
        [[nodiscard]] float TimeToNextStrike() const noexcept {return next_;}

    private:
        [[nodiscard]] std::uint32_t Next() noexcept
        {
            seed_^=seed_<<13u;seed_^=seed_>>17u;seed_^=seed_<<5u;
            return seed_;
        }
        [[nodiscard]] float Random01() noexcept
        {
            return float(Next()&0x00ffffffu)/16777216.0f;
        }

        // Short Catmull-Rom interpolation smooths the otherwise noticeable
        // straight-line elbows, without turning a bolt into a perfect arc.
        [[nodiscard]] static XMFLOAT3 Curve(const XMFLOAT3& p0,
            const XMFLOAT3& p1,const XMFLOAT3& p2,const XMFLOAT3& p3,
            float t) noexcept
        {
            // Exact knot identity is essential: fork roots attach to the
            // trunk endpoint, not an approximate floating-point neighbour.
            if(t<=0.0f)return p1;
            if(t>=1.0f)return p2;
            const float t2=t*t,t3=t2*t;
            auto axis=[=](float a,float b,float c,float d) noexcept
            {
                return 0.5f*(2.0f*b + (-a+c)*t +
                    (2.0f*a-5.0f*b+4.0f*c-d)*t2 +
                    (-a+3.0f*b-3.0f*c+d)*t3);
            };
            return {axis(p0.x,p1.x,p2.x,p3.x),
                p1.y+(p2.y-p1.y)*t, // always moves downwards
                axis(p0.z,p1.z,p2.z,p3.z)};
        }

        [[nodiscard]] static float Smooth(float a,float b,float v) noexcept
        {
            const float t=std::clamp((v-a)/(b-a),0.0f,1.0f);
            return t*t*(3.0f-2.0f*t);
        }

        void Create(const XMFLOAT3& observer,const XMFLOAT3& forward,float groundY) noexcept
        {
            const float heading=std::hypot(forward.x,forward.z)>0.01f ?
                std::atan2(forward.z,forward.x) : 0.0f;
            const float theta=heading+(Random01()-0.5f)*0.9f;
            const float distance=config_.minDistanceMetres+
                (config_.maxDistanceMetres-config_.minDistanceMetres)*Random01();
            const XMFLOAT3 contact{
                observer.x+std::cos(theta)*distance,groundY,
                observer.z+std::sin(theta)*distance};
            const XMFLOAT3 source{
                contact.x+(Random01()-0.5f)*340.0f,
                groundY+config_.cloudBaseMetres+(Random01()-0.5f)*160.0f,
                contact.z+(Random01()-0.5f)*340.0f};
            const std::uint64_t id=frame_.id+1;
            frame_={};
            frame_.id=id;frame_.origin=source;frame_.contact=contact;
            frame_.visible=true;frame_.brightness=1.0f;

            constexpr unsigned TrunkIntervals=28;
            constexpr unsigned Samples=3;
            XMFLOAT3 knots[TrunkIntervals+1]{};
            knots[0]=source;
            const float drop=source.y-groundY;
            float driftX=0.0f,driftZ=0.0f;
            for(unsigned k=1;k<=TrunkIntervals;++k)
            {
                const float t=float(k)/TrunkIntervals;
                XMFLOAT3 point{
                    source.x+(contact.x-source.x)*t,
                    source.y-drop*t,
                    source.z+(contact.z-source.z)*t};
                if(k<TrunkIntervals)
                {
                    driftX=std::clamp(driftX*0.56f+(Random01()-0.5f)*drop*0.047f,-90.0f,90.0f);
                    driftZ=std::clamp(driftZ*0.56f+(Random01()-0.5f)*drop*0.047f,-90.0f,90.0f);
                    point.x+=driftX*std::sin(XM_PI*t);
                    point.z+=driftZ*std::sin(XM_PI*t);
                }
                knots[k]=point;
            }

            frame_.segments.reserve(180);
            XMFLOAT3 previous=source;
            for(unsigned k=0;k<TrunkIntervals;++k)
            {
                const XMFLOAT3& p0=knots[k>0?k-1:0];
                const XMFLOAT3& p1=knots[k];
                const XMFLOAT3& p2=knots[k+1];
                const XMFLOAT3& p3=knots[std::min(k+2,TrunkIntervals)];
                for(unsigned s=1;s<=Samples;++s)
                {
                    const float t=float(s)/Samples;
                    const XMFLOAT3 next=Curve(p0,p1,p2,p3,t);
                    const float progress=(float(k)+t)/TrunkIntervals;
                    // The first ~18% of the descending leader is embedded
                    // in the glowing cloud, not rendered as an overlay on it.
                    const float emerge=Smooth(0.02f,0.20f,progress);
                    const float energy=emerge*(1.0f-0.18f*progress);
                    frame_.segments.push_back({previous,next,
                        1.40f*(1.0f-0.38f*progress),
                        energy,0});
                    previous=next;
                }
            }

            const unsigned forkCount=std::min(12u,static_cast<unsigned>(std::lround(float(3u+(Next()%3u))*config_.branchDensity)));
            for(unsigned f=0;f<forkCount;++f)
            {
                const unsigned anchor=4u+(Next()%17u);
                constexpr unsigned BranchIntervals=5;
                XMFLOAT3 curve[BranchIntervals+1]{};
                curve[0]=knots[anchor];
                const float bearing=Random01()*XM_2PI;
                const float side=Random01()<0.5f?-1.0f:1.0f;
                const float reach=drop*(0.07f+0.09f*Random01());
                const float down=drop*(0.09f+0.11f*Random01());
                float wanderX=0.0f,wanderZ=0.0f;
                for(unsigned j=1;j<=BranchIntervals;++j)
                {
                    const float t=float(j)/BranchIntervals;
                    wanderX=0.60f*wanderX+(Random01()-0.5f)*17.0f;
                    wanderZ=0.60f*wanderZ+(Random01()-0.5f)*17.0f;
                    curve[j]={
                        curve[0].x+side*std::cos(bearing)*reach*t+wanderX,
                        std::max(groundY+70.0f,curve[0].y-down*t),
                        curve[0].z+side*std::sin(bearing)*reach*t+wanderZ};
                }
                XMFLOAT3 from=curve[0];
                const float cloudEmergence=Smooth(0.02f,0.20f,
                    float(anchor)/TrunkIntervals);
                for(unsigned j=0;j<BranchIntervals;++j)
                {
                    for(unsigned s=1;s<=Samples;++s)
                    {
                        const float t=float(s)/Samples;
                        const XMFLOAT3 to=Curve(
                            curve[j>0?j-1:0],curve[j],curve[j+1],
                            curve[std::min(j+2,BranchIntervals)],t);
                        const float progress=(float(j)+t)/BranchIntervals;
                        const float fade=std::pow(1.0f-progress,1.35f);
                        frame_.segments.push_back({from,to,
                            std::max(0.08f,0.70f*(1.0f-0.76f*progress)),
                            0.64f*cloudEmergence*fade,1});
                        from=to;
                    }
                }
            }
        }

        ForkLightningSettings config_{};
        ForkLightningFrame frame_{};
        std::uint32_t seed_=0x5EEDB017u;
        float next_=0.0f;
    };
}
