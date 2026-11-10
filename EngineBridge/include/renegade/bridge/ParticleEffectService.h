#pragma once
#include <array>
#include <iosfwd>
#include "renegade/bridge/ParticleEmitterService.h"
#include "renegade/bridge/MaterialService.h"
#include "renegade/bridge/IdentityService.h"
namespace renegade::bridge {
inline constexpr const char* ParticleEffectMetadataKey="renegade.particle.effect";
enum class ParticlePlayback { Continuous, StationaryLoop };
enum class ParticleFieldType { Swirl, Turbulence, Updraft, Directional };
struct ParticleMovementField {
 StableId id;
 std::string name="Swirl";
 ParticleFieldType type=ParticleFieldType::Swirl;
 TransformState transform;
 float strength=1, falloff=.3f, lift=0, noiseScale=1, animationSpeed=.5f;
 bool enabled=true, clockwise=false;
};
XMFLOAT3 SampleParticleMovementField(const ParticleMovementField&,const XMFLOAT3& effectPosition,float time);
bool EnsureParticleMovementShaders(std::string& error);
void HydrateParticleMovementFields(wi::scene::Scene&);
struct ParticleLifetimeKey {
 float time=0,size=1;
 XMFLOAT4 colour={1,1,1,1};
};
struct ParticleLifetimeCurve {
 bool enabled=false;
 std::vector<ParticleLifetimeKey> keys={{0,1,{1,1,1,1}},{1,1,{1,1,1,1}}};
};
bool ValidateParticleLifetime(const ParticleLifetimeCurve&,std::string&);
ParticleLifetimeKey SampleParticleLifetime(const ParticleLifetimeCurve&,float time);
void WriteParticleLifetime(std::ostream&,const ParticleLifetimeCurve&);
bool ReadParticleLifetime(std::istream&,ParticleLifetimeCurve&);
void HydrateParticleLifetime(wi::scene::Scene&,wi::ecs::Entity);
enum class ParticleShapeType { Point, Box, Sphere, Cone, Disc, Ring, Cylinder };
struct ParticleEmissionShape {
 ParticleShapeType type=ParticleShapeType::Point;
 float radius=.5f,height=1,angle=20,thickness=.1f;
 bool surface=false;
};
struct ParticleShapeSample {XMFLOAT3 position={},normal={};};
bool ValidateParticleShape(const ParticleEmissionShape&,std::string&);
ParticleShapeSample SampleParticleShape(const ParticleEmissionShape&,XMFLOAT3 unitRandom);
void WriteParticleShape(std::ostream&,const ParticleEmissionShape&);
bool ReadParticleShape(std::istream&,ParticleEmissionShape&);
void HydrateParticleShape(wi::scene::Scene&,wi::ecs::Entity);
struct ParticleEffectLayer {
 ParticleEmissionShape shape;
 ParticleLifetimeCurve lifetime;
 float sizeVariation=0,speedVariation=0;
 std::array<float,4> fieldInfluence={1,1,1,1};
 ParticlePlayback playback=ParticlePlayback::Continuous;
 StableId id;
 std::string name="Emitter";
 ParticleEmitterState emitter;
 MaterialState material;
 StableId texture;
 std::array<StableId,4> extraTextures; // normal, surface, emissive, occlusion
 TransformState transform;
 bool enabled=true;
};
struct ParticleEffectTiming {
 bool enabled=false,loop=true;
 float duration=5,delay=0;
 uint32_t burst=0;
};
struct ParticleTimingSample {bool active=false,burst=false,completed=false;};
bool ValidateParticleTiming(const ParticleEffectTiming&,std::string&);
ParticleTimingSample SampleParticleTiming(const ParticleEffectTiming&,double elapsed,double previous);
void WriteParticleTiming(std::ostream&,const ParticleEffectTiming&);
bool ReadParticleTiming(std::istream&,ParticleEffectTiming&);
void SetParticleTimingMetadata(wi::scene::Scene&,wi::ecs::Entity,const ParticleEffectTiming&);
void BeginParticleTimingFrame(wi::scene::Scene&,float dt);
void ApplyParticleTimingLayer(wi::scene::Scene&,wi::ecs::Entity);
void ControlParticleTiming(wi::scene::Scene&,wi::ecs::Entity,const std::string& operation);
void ResetParticleTiming(wi::scene::Scene&,wi::ecs::Entity);
struct ParticleEffect {
 ParticleEffectTiming timing;
 StableId id;
 std::string name="New Effect";
 std::vector<ParticleEffectLayer> layers;
 std::vector<ParticleMovementField> fields;
};
void UpdateParticleEffectSprites(wi::scene::Scene&,float dt);
ParticleEffect CaptureParticleEffect(const wi::scene::Scene&,wi::ecs::Entity);
wi::ecs::Entity ParticleEffectRoot(const wi::scene::Scene&,wi::ecs::Entity) noexcept;
bool ValidateParticleEffect(const ParticleEffect&,std::string&);
bool SaveParticleEffect(const std::string&,const StableId&,ParticleEffect&,bool,std::string&);
std::vector<ParticleEffect> ListParticleEffects(const std::string&,const StableId&,std::string&);
ParticleEffectLayer ScaleParticleLayer(const ParticleEffectLayer&,float size,float amount,float height,float spread);
class ParticleEffectCommand final : public ICommand {
public:
 ParticleEffectCommand(wi::scene::Scene&,std::string root,StableId project,ParticleEffect,
    wi::ecs::Entity target=wi::ecs::INVALID_ENTITY,XMFLOAT3 position={});
 bool Execute() override;
 void Undo() override;
 wi::ecs::Entity Entity() const noexcept {return entity_;}
 const std::string& Error() const noexcept {return error_;}
private:
 wi::scene::Scene* scene_;std::string root_,error_;StableId project_;
 ParticleEffect effect_;wi::ecs::Entity entity_;XMFLOAT3 position_;
 wi::Archive before_,after_;bool captured_=false,existing_=false;
 std::vector<std::pair<wi::ecs::Entity,wi::scene::MaterialComponent>> beforeMaterials_,afterMaterials_;
};
}
