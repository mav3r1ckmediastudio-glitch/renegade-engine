#include "renegade/bridge/RiverSplineService.h"
#include "renegade/bridge/IdentityService.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "wiHelper.h"
#include "wiBacklog.h"
#include "wiInitializer.h"
#include "wiResourceManager.h"
#include <utility>

namespace renegade::bridge
{
    namespace
    {
        constexpr std::size_t MaximumRiverNodes = 512;
        constexpr float MinimumPointDistanceSquared = 0.01f;

        [[nodiscard]] bool IsFinite(const XMFLOAT3& p) noexcept
        {
            return std::isfinite(p.x) && std::isfinite(p.y) &&
                std::isfinite(p.z);
        }

        [[nodiscard]] float DistanceSquaredXZ(
            const XMFLOAT3& a, const XMFLOAT3& b) noexcept
        {
            const float dx = a.x - b.x;
            const float dz = a.z - b.z;
            return dx * dx + dz * dz;
        }

        [[nodiscard]] bool EntityPresent(
            const wi::scene::Scene& scene,
            const wi::ecs::Entity entity)
        {
            return entity != wi::ecs::INVALID_ENTITY &&
                scene.names.Contains(entity);
        }
    }

    float RiverBedEndDistance(float lateralDistanceSquared,
        float along,float length,float radius) noexcept
    {
        if(!std::isfinite(lateralDistanceSquared)||!std::isfinite(along)||
            !std::isfinite(length)||!std::isfinite(radius)||radius<=0||length<=0)
            return INFINITY;
        const float inward=std::min(std::max(0.f,along),std::max(0.f,length-along));
        const float cap=std::max(0.f,radius-inward);
        return std::sqrt(std::max(0.f,lateralDistanceSquared)+cap*cap);
    }
    void RestoreRiverCaustics(wi::scene::Scene& scene)
    {
        for(std::size_t i=0;i<scene.splines.GetCount();++i)
        {
            const auto entity=scene.splines.GetEntity(i);
            auto* water=scene.materials.GetComponent(entity);
            if(!water||water->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER||
                PersistentEntityId(scene,entity).empty())continue;
            (void)BindRiverSurfaceShader(scene,entity);
            auto& metadata=*scene.metadatas.GetComponent(entity);
            if(metadata.string_values.has("renegade.river.native-caustics.v1"))continue;
            water->SetCastShadow(true);water->SetDirty();
            metadata.string_values.set("renegade.river.native-caustics.v1","1");
        }
    }

    bool IsRiverSplineStrokeValid(
        const std::vector<XMFLOAT3>& points) noexcept
    {
        if (points.size() < 2 || points.size() > MaximumRiverNodes)
            return false;
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            if (!IsFinite(points[i]))
                return false;
            if (i > 0 &&
                DistanceSquaredXZ(points[i - 1], points[i]) <
                    MinimumPointDistanceSquared)
                return false;
        }
        return true;
    }

    CreateRiverSplineCommand::CreateRiverSplineCommand(
        wi::scene::Scene& scene,
        std::vector<XMFLOAT3> points,
        RiverSplineOptions options)
        : scene_(&scene),
          points_(std::move(points)),
          options_(options)
    {
    }

    bool CreateRiverSplineCommand::Restore()
    {
        if (!hasSnapshot_ || scene_ == nullptr ||
            EntityPresent(*scene_, entity_))
            return false;
        snapshot_.SetReadModeAndResetPos(true);
        wi::ecs::EntitySerializer serializer;
        serializer.allow_remap = false;
        return scene_->Entity_Serialize(snapshot_, serializer) == entity_;
    }

    bool CreateRiverSplineCommand::Execute()
    {
        if (scene_ == nullptr)
            return false;
        if (hasSnapshot_)
            return Restore();
        if (!IsRiverSplineStrokeValid(points_) ||
            !std::isfinite(options_.widthMeters) ||
            !std::isfinite(options_.surfaceOffsetMeters) ||
            options_.widthMeters <= 0)
            return false;

        // Wicked's WATER shader needs a real tiled normal map and UV
        // animation. Merely selecting SHADERTYPE_WATER without NORMALMAP
        // produces the flat translucent strip seen in owner testing.
        // The asset is copied beside Studio and Runtime at build time.
        constexpr const char* RiverNormalRelativePath =
            "Content/river/waterNormalMap.jpg";
        wi::Resource waterNormal;
        std::string resourcePath = RiverNormalRelativePath;
        if (wi::initializer::IsInitializeFinished())
        {
            resourcePath = (
                std::filesystem::path(wi::helper::GetExecutablePath())
                    .parent_path() / RiverNormalRelativePath).generic_string();
            waterNormal = wi::resourcemanager::Load(resourcePath,
                wi::resourcemanager::Flags::IMPORT_RETAIN_FILEDATA |
                wi::resourcemanager::Flags::IMPORT_NORMALMAP);
            if (!waterNormal.IsValid() ||
                !waterNormal.GetTexture().IsValid())
                return false; // Never silently create a fake water surface.
        }

        std::string name = "River Water";
        int suffix = 2;
        while (scene_->Entity_FindByName(name) != wi::ecs::INVALID_ENTITY)
            name = "River Water " + std::to_string(suffix++);

        entity_ = scene_->Entity_CreateTransform(name);
        if (entity_ == wi::ecs::INVALID_ENTITY)
            return false;

        auto& spline = scene_->splines.Create(entity_);
        spline.width = std::clamp(options_.widthMeters, 0.5f, 100.0f) * 0.5f;
        spline.mesh_generation_subdivision =
            std::clamp(options_.subdivisionsPerSegment, 1, 24);
        spline.mesh_generation_vertical_subdivision = 0;
        spline.terrain_modifier_amount = 0; // No terrain regeneration in V1.
        spline.SetLooped(false);
        spline.SetFilled(false);
        spline.SetDrawAligned(true);

        auto& material = scene_->materials.Create(entity_);
        material.shaderType = wi::scene::MaterialComponent::SHADERTYPE_WATER;
        // Single stock material definition shared with the inspector; native
        // WISCENE persistence and Redo preserve each river's authored edits.
        const RiverWaterMaterialState stockWater;
        material.SetBaseColor(stockWater.tintOpacity);
        material.SetExtinctionColor(XMFLOAT4(0.12f, 0.19f, 0.14f, 1.0f));
        material.SetRoughness(0.141421f);
        material.SetRefractionAmount(0.12f);
        material.SetNormalMapStrength(stockWater.normalStrength);
        auto& normal = material.textures[
            wi::scene::MaterialComponent::NORMALMAP];
        // Native scene serialization looks up retained bytes by this exact
        // resource-manager key. A different relative alias loses embedding.
        normal.name = resourcePath;
        normal.resource = std::move(waterNormal);
        // Wicked RunSplineUpdateSystem accumulates V as distance / full
        // ribbon width, while U spans [0,1]. Both axes therefore need the
        // same fullWidth / tileMeters multiplier for metric normal tiles.
        // Treating V as metres stretched waves by the river's full width.
        const float tilesAcross = (2.0f * spline.width) / 4.0f;
        material.texMulAdd = XMFLOAT4(tilesAcross, tilesAcross, 0.0f, 0.0f);
        // Wicked's scene material-update system scrolls the two sampled
        // normal layers in opposite directions along the spline's UVs.
        material.texAnimDirection = XMFLOAT2(0.0f, stockWater.flowUvV);
        material.texAnimFrameRate = 30.0f;
        // Wicked WATER transparent shadows project its animated caustic pattern
        // onto directly lit receiving geometry, including shallow stream beds.
        material.SetCastShadow(true);
        material.SetDirty();

        for (std::size_t i = 0; i < points_.size(); ++i)
        {
            const auto node = scene_->Entity_CreateTransform(
                name + " Point " + std::to_string(i + 1));
            if (node == wi::ecs::INVALID_ENTITY)
            {
                scene_->Entity_Remove(entity_, true);
                entity_ = wi::ecs::INVALID_ENTITY;
                return false;
            }
            auto* transform = scene_->transforms.GetComponent(node);
            if (transform == nullptr)
            {
                scene_->Entity_Remove(node, true);
                scene_->Entity_Remove(entity_, true);
                entity_ = wi::ecs::INVALID_ENTITY;
                return false;
            }
            transform->translation_local = points_[i];
            transform->translation_local.y +=
                std::clamp(options_.surfaceOffsetMeters, -2.0f, 2.0f);
            transform->SetDirty();
            transform->UpdateTransform();
            scene_->Component_Attach(node, entity_);
            spline.spline_node_entities.push_back(node);
            spline.spline_node_transforms.push_back(*transform);
        }
        spline.SetDirty();
        std::string identityError;
        if (!AssignNewPersistentEntityId(*scene_, entity_, identityError))
        {
            scene_->Entity_Remove(entity_, true);
            entity_ = wi::ecs::INVALID_ENTITY;
            return false;
        }

        scene_->metadatas.GetComponent(entity_)->string_values.set(
            "renegade.river.native-caustics.v1","1");
        if(!BindRiverSurfaceShader(*scene_,entity_))
        {scene_->Entity_Remove(entity_,true);entity_=wi::ecs::INVALID_ENTITY;return false;}
        snapshot_.SetReadModeAndResetPos(false);
        wi::ecs::EntitySerializer serializer;
        scene_->Entity_Serialize(snapshot_, serializer, entity_);
        hasSnapshot_ = true;
        return true;
    }

    void CreateRiverSplineCommand::Undo()
    {
        if (scene_ != nullptr && EntityPresent(*scene_, entity_))
            scene_->Entity_Remove(entity_, true);
    }

    bool CaptureRiverWaterMaterialState(const wi::scene::Scene& scene,
        const wi::ecs::Entity river, RiverWaterMaterialState& state) noexcept
    {
        const auto* material = scene.materials.GetComponent(river);
        if (!scene.splines.Contains(river) || !material ||
            material->shaderType != wi::scene::MaterialComponent::SHADERTYPE_WATER)
            return false;
        state.tintOpacity = material->baseColor;
        state.flowUvV = material->texAnimDirection.y;
        state.flowUvU = material->texAnimDirection.x;
        state.normalStrength = material->normalMapStrength;
        return true;
    }

    SetRiverWaterMaterialCommand::SetRiverWaterMaterialCommand(
        wi::scene::Scene& scene, const wi::ecs::Entity river,
        RiverWaterMaterialState after) noexcept
        : scene_(&scene), river_(river), after_(after)
    {
    }

    bool SetRiverWaterMaterialCommand::Apply(
        const RiverWaterMaterialState& state) noexcept
    {
        if (!scene_) return false;
        auto* material = scene_->materials.GetComponent(river_);
        if (!scene_->splines.Contains(river_) || !material ||
            material->shaderType != wi::scene::MaterialComponent::SHADERTYPE_WATER ||
            !std::isfinite(state.flowUvV) || !std::isfinite(state.flowUvU) ||
            !std::isfinite(state.normalStrength) || state.normalStrength<0 || state.normalStrength>3) return false;
        const auto& c = state.tintOpacity;
        if (!std::isfinite(c.x) || !std::isfinite(c.y) ||
            !std::isfinite(c.z) || !std::isfinite(c.w) ||
            c.x < 0 || c.x > 1 || c.y < 0 || c.y > 1 ||
            c.z < 0 || c.z > 1 || c.w < 0 || c.w > 16.0f)
            return false;
        material->SetBaseColor(c);
        material->texAnimDirection={state.flowUvU,state.flowUvV};
        material->SetNormalMapStrength(state.normalStrength);
        material->SetDirty();
        return true;
    }

    bool SetRiverWaterMaterialCommand::Execute()
    {
        if (!scene_) return false;
        if (!captured_)
        {
            if (!CaptureRiverWaterMaterialState(*scene_, river_, before_))
                return false;
            captured_ = true;
        }
        const auto& a = after_.tintOpacity;
        const auto& b = before_.tintOpacity;
        if (a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w &&
            after_.flowUvV == before_.flowUvV && after_.flowUvU == before_.flowUvU &&
            after_.normalStrength == before_.normalStrength) return false;
        return Apply(after_);
    }

    void SetRiverWaterMaterialCommand::Undo()
    {
        if (captured_) (void)Apply(before_);
    }

    OrientNearPlanarRiverWaterPlaneCommand::OrientNearPlanarRiverWaterPlaneCommand(
        wi::scene::Scene& scene,wi::ecs::Entity river) noexcept
        : scene_(&scene),river_(river)
    {
    }

    bool OrientNearPlanarRiverWaterPlaneCommand::ApplySaved(
        const TransformState& rootState,
        const std::vector<TransformState>& nodeStates) noexcept
    {
        if(!scene_ || nodes_.size()!=nodeStates.size()) return false;
        auto* root=scene_->transforms.GetComponent(river_);
        auto* spline=scene_->splines.GetComponent(river_);
        if(!root || !spline) return false;
        for(const auto id:nodes_)
            if(!scene_->transforms.Contains(id) ||
                !scene_->hierarchy.Contains(id) ||
                scene_->hierarchy.GetComponent(id)->parentID!=river_)
                return false;
        root->translation_local=rootState.translation;
        root->rotation_local=rootState.rotation;
        root->scale_local=rootState.scale;
        root->SetDirty();
        root->UpdateTransform();
        for(std::size_t i=0;i<nodes_.size();++i)
        {
            auto* node=scene_->transforms.GetComponent(nodes_[i]);
            node->translation_local=nodeStates[i].translation;
            node->rotation_local=nodeStates[i].rotation;
            node->scale_local=nodeStates[i].scale;
            node->SetDirty();
            node->UpdateTransform_Parented(*root);
            node->SetDirty(); // Required: Wicked regenerates spline only for DIRTY nodes.
        }
        spline->SetDirty();
        return true;
    }

    bool OrientNearPlanarRiverWaterPlaneCommand::Execute()
    {
        if(!scene_) return false;
        if(captured_) return ApplySaved(afterRoot_,afterNodes_);
        auto* spline=scene_->splines.GetComponent(river_);
        auto* root=scene_->transforms.GetComponent(river_);
        const auto* material=scene_->materials.GetComponent(river_);
        if(!spline || !root || !material ||
            material->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER ||
            material->IsCustomShader() ||
            scene_->hierarchy.Contains(river_) ||
            spline->IsLooped() || spline->spline_node_entities.size()<2 ||
            spline->spline_node_entities.size()>512 ||
            spline->terrain_modifier_amount!=0)
            return false;
        // Existing authoring relies on an identity root. Do NOT guess a
        // coordinate conversion for parented or already-tilted splines.
        const auto& q=root->rotation_local;
        if(std::abs(q.x)>0.0001f || std::abs(q.y)>0.0001f ||
            std::abs(q.z)>0.0001f || std::abs(std::abs(q.w)-1.f)>0.0001f ||
            std::abs(root->scale_local.x-1.f)>0.0001f ||
            std::abs(root->scale_local.y-1.f)>0.0001f ||
            std::abs(root->scale_local.z-1.f)>0.0001f)
            return false;
        const auto candidates=spline->spline_node_entities;
        std::vector<XMFLOAT3> originalWorld;
        std::vector<TransformState> oldLocals;
        originalWorld.reserve(candidates.size());
        oldLocals.reserve(candidates.size());
        for(const auto child:candidates)
        {
            const auto* hier=scene_->hierarchy.GetComponent(child);
            const auto* node=scene_->transforms.GetComponent(child);
            if(!hier || hier->parentID!=river_ || !node) return false;
            XMFLOAT3 pos;
            DirectX::XMStoreFloat3(&pos,DirectX::XMVector3TransformCoord(
                DirectX::XMVectorZero(),node->GetWorldMatrix()));
            if(!IsFinite(pos)) return false;
            originalWorld.push_back(pos);
            oldLocals.push_back(CaptureTransform(*node));
        }
        // Protect linked authoring tools and attached controls: reject any
        // unexpected child instead of detaching/moving it incidentally.
        for(std::size_t i=0;i<scene_->hierarchy.GetCount();++i)
        {
            if(scene_->hierarchy[i].parentID!=river_) continue;
            const auto child=scene_->hierarchy.GetEntity(i);
            if(std::find(candidates.begin(),candidates.end(),child)
                ==candidates.end()) return false;
        }
        const auto& first=originalWorld.front();
        const auto& last=originalWorld.back();
        const float dx=last.x-first.x,dz=last.z-first.z;
        const float flatLength=std::hypot(dx,dz);
        if(flatLength<3.f || !std::isfinite(flatLength))return false;
        const float ux=dx/flatLength,uz=dz/flatLength;
        // The water can curve in XZ as long as its centreline is close
        // to ONE consistent downhill plane. Retain all authored positions.
        std::vector<float> progress;
        progress.reserve(originalWorld.size());
        double sumS=0,sumY=0;
        for(std::size_t i=0;i<originalWorld.size();++i)
        {
            const auto& p=originalWorld[i];
            const float s=(p.x-first.x)*ux+(p.z-first.z)*uz;
            if(i>0 && s<progress.back()-.02f)return false;
            progress.push_back(s);
            sumS+=s;
            sumY+=p.y;
        }
        const double meanS=sumS/progress.size();
        const double meanY=sumY/progress.size();
        double sSq=0,sY=0;
        for(std::size_t i=0;i<originalWorld.size();++i)
        {
            const double deltaS=progress[i]-meanS;
            sSq+=deltaS*deltaS;
            sY+=deltaS*(originalWorld[i].y-meanY);
        }
        if(sSq<1.0)return false;
        const float grade=float(sY/sSq);
        if(!std::isfinite(grade))return false;
        const float signedPitch=std::atan(std::abs(grade));
        if(signedPitch<DirectX::XMConvertToRadians(2.f) ||
            signedPitch>DirectX::XMConvertToRadians(55.f))return false;
        double errorSq=0;
        float maxError=0;
        for(std::size_t i=0;i<originalWorld.size();++i)
        {
            const float error=std::abs(originalWorld[i].y-
                float(meanY+grade*(progress[i]-meanS)));
            maxError=std::max(maxError,error);
            errorSq+=double(error)*error;
        }
        // A single native reflection capture cannot model steeply
        // varying waterfalls or strongly nonplanar streams. Fail CLOSED.
        if(maxError>.75f ||
            std::sqrt(errorSq/originalWorld.size())>.40)return false;
        const auto A=DirectX::XMVector3Normalize(
            DirectX::XMVectorSet(dx,0,dz,0));
        const auto B=DirectX::XMVector3Normalize(
            DirectX::XMVectorSet(dx,grade*flatLength,dz,0));
        const auto cross=DirectX::XMVector3Cross(A,B);
        const float sine=DirectX::XMVectorGetX(
            DirectX::XMVector3Length(cross));
        if(sine<.001f)return false;
        const auto rotation=DirectX::XMQuaternionRotationAxis(
            DirectX::XMVector3Normalize(cross),
            std::atan2(sine,DirectX::XMVectorGetX(
                DirectX::XMVector3Dot(A,B))));
        XMFLOAT4 rotationResult;
        DirectX::XMStoreFloat4(&rotationResult,rotation);
        const XMFLOAT3 midpoint{
            (first.x+last.x)*.5f,
            (first.y+last.y)*.5f,
            (first.z+last.z)*.5f
        };
        nodes_.assign(candidates.begin(),candidates.end());
        beforeRoot_=CaptureTransform(*root);
        beforeNodes_=std::move(oldLocals);
        // Remove the old parent frame without changing any WORLD locations.
        for(const auto child:nodes_)scene_->Component_Detach(child);
        root->translation_local=midpoint;
        root->rotation_local=rotationResult;
        root->SetDirty();
        root->UpdateTransform();
        for(const auto child:nodes_)
        {
            scene_->Component_Attach(child,river_);
            auto* node=scene_->transforms.GetComponent(child);
            node->SetDirty(); // Component_Attach clears the dirty bit.
        }
        spline->SetDirty();
        afterRoot_=CaptureTransform(*root);
        afterNodes_.reserve(nodes_.size());
        for(const auto id:nodes_)
            afterNodes_.push_back(CaptureTransform(
                *scene_->transforms.GetComponent(id)));
        captured_=true;
        return true;
    }

    void OrientNearPlanarRiverWaterPlaneCommand::Undo()
    {
        if(captured_)(void)ApplySaved(beforeRoot_,beforeNodes_);
    }

    namespace
    {
        // Explicit, process-local visual experiment only. The accepted
        // production default stays native Wicked WATER with no custom shader.
        [[nodiscard]] bool LocalDepthDiagnosticRequested() noexcept
        {
#ifdef _WIN32
            const char* flag = std::getenv("RENEGADE_RIVER_LOCAL_DEPTH_TEST");
            return flag && flag[0]=='1' && flag[1]==0;
#else
            return false;
#endif
        }
        int LocalDepthDiagnosticShader()
        {
            static int id = -1;
            static wi::graphics::Shader pixel;
            if(id>=0) return id;
            if(!wi::initializer::IsInitializeFinished())return -1;
            auto* device=wi::graphics::GetDevice();
            if(!device)return -1;
            const auto path=(std::filesystem::path(wi::helper::GetExecutablePath())
                .parent_path()/"Content/river/RenegadeRiverLocalDepthTestPS.cso").generic_string();
            wi::vector<std::uint8_t> bytes;
            if(!wi::helper::FileRead(path,bytes) || bytes.empty() ||
                !device->CreateShader(wi::graphics::ShaderStage::PS,bytes.data(),
                    bytes.size(),&pixel))return -1;

            // Identical native WATER main VS/transparent pass and shadow
            // rendering: only proof-owned fragment optical terms differ.
            wi::renderer::CustomShader shader;
            shader.name="Renegade Local Depth Test (Opt-in)";
            shader.filterMask=wi::enums::FILTER_TRANSPARENT|wi::enums::FILTER_WATER;
            wi::graphics::PipelineStateDesc desc;
            desc.vs=wi::renderer::GetShader(wi::enums::VSTYPE_OBJECT_COMMON);
            desc.ps=&pixel;
            desc.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENT);
            desc.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_TRANSPARENT);
            desc.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_DOUBLESIDED);
            if(!device->CreatePipelineState(&desc,&shader.pso[wi::enums::RENDERPASS_MAIN]))
                return -1;
            desc.vs=wi::renderer::GetShader(wi::enums::VSTYPE_SHADOW_TRANSPARENT);
            desc.ps=wi::renderer::GetShader(wi::enums::PSTYPE_SHADOW_TRANSPARENT);
            desc.bs=wi::renderer::GetBlendState(wi::enums::BSTYPE_TRANSPARENTSHADOW);
            desc.dss=wi::renderer::GetDepthStencilState(wi::enums::DSSTYPE_DEPTHREAD);
            desc.rs=wi::renderer::GetRasterizerState(wi::enums::RSTYPE_SHADOW_DOUBLESIDED);
            if(!device->CreatePipelineState(&desc,&shader.pso[wi::enums::RENDERPASS_SHADOW]))
                return -1;
            id=wi::renderer::RegisterCustomShader(shader);
            return id;
        }
    }

    ScopedRiverLocalDepthArchiveMask::ScopedRiverLocalDepthArchiveMask(
        wi::scene::Scene& scene) : scene_(scene)
    {
        if(!LocalDepthDiagnosticRequested() ||
            !wi::initializer::IsInitializeFinished())return;
        const int diagnosticID=LocalDepthDiagnosticShader();
        if(diagnosticID<0)return;
        for(std::size_t i=0;i<scene.splines.GetCount();++i)
        {
            const auto entity=scene.splines.GetEntity(i);
            auto* material=scene.materials.GetComponent(entity);
            if(!material ||
                material->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER ||
                material->GetCustomShaderID()!=diagnosticID)continue;
            originals_.emplace_back(entity,diagnosticID);
        }
        // Collect first so any allocation failure leaves every material
        // unchanged. The save path catches a failed guard construction.
        for(const auto& [entity,customID] : originals_)
        {
            auto* material=scene_.materials.GetComponent(entity);
            if(material)
            {
                material->DisableCustomShader();
                material->SetDirty();
            }
        }
    }

    ScopedRiverLocalDepthArchiveMask::~ScopedRiverLocalDepthArchiveMask() noexcept
    {
        for(const auto& [entity,customID]:originals_)
        {
            auto* material=scene_.materials.GetComponent(entity);
            if(material &&
                material->shaderType==wi::scene::MaterialComponent::SHADERTYPE_WATER &&
                scene_.splines.Contains(entity))
            {
                material->SetCustomShaderID(customID);
                material->SetDirty();
            }
        }
    }

    bool BindRiverSurfaceShader(wi::scene::Scene& scene,wi::ecs::Entity river)
    {
        auto* material=scene.materials.GetComponent(river);
        if(!material || !scene.splines.Contains(river) ||
            material->shaderType!=wi::scene::MaterialComponent::SHADERTYPE_WATER)
            return false;
        material->DisableCustomShader();
        if(LocalDepthDiagnosticRequested() && wi::initializer::IsInitializeFinished())
        {
            const int proofShader=LocalDepthDiagnosticShader();
            if(proofShader>=0) material->SetCustomShaderID(proofShader);
            else wi::backlog::post("RIVER DEPTH TEST unavailable; using native WATER",
                wi::backlog::LogLevel::Warning);
        }
        material->SetDirty();
        return true;
    }

}
