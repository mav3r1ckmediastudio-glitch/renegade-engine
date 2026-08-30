#include "StudioApplication.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
    constexpr wi::Color RenderMuted = wi::Color(178, 178, 176, 255);
}

namespace renegade::studio
{
    void StudioRenderPath::CreateGate8BakeControls()
    {
        const auto createSection = [this](
            wi::gui::Label& label,
            const char* name,
            const char* text)
        {
            label.Create(name);
            label.SetText(text);
            label.font.params.size = 13;
            label.font.params.color = RenderMuted;
            label.font.params.h_align = wi::font::WIFALIGN_LEFT;
            label.SetColor(wi::Color::Transparent());
            renderWorkspacePanel_.AddWidget(&label);
        };

        createSection(
            renderLightmapBakeLabel_,
            "Render Lightmap Bake Section",
            "LIGHTMAP // SELECTED STATIC GEOMETRY");

        renderLightmapBakeStatus_.Create("Render Lightmap Bake Status");
        renderLightmapBakeStatus_.font.params.size = 12;
        renderLightmapBakeStatus_.font.params.color = RenderMuted;
        renderLightmapBakeStatus_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        renderLightmapBakeStatus_.SetColor(wi::Color::Transparent());
        renderLightmapBakeStatus_.SetWrapEnabled(true);
        renderWorkspacePanel_.AddWidget(&renderLightmapBakeStatus_);

        renderLightmapResolution_.Create("Render Lightmap Resolution");
        for (std::uint32_t resolution = 32; resolution <= 8192; resolution *= 2)
        {
            renderLightmapResolution_.AddItem(
                std::to_string(resolution) + " x " + std::to_string(resolution),
                static_cast<std::uint64_t>(resolution));
        }
        renderLightmapResolution_.SetSelectedByUserdataWithoutCallback(512);
        renderLightmapResolution_.SetTooltip(
            "Native lightmap resolution. Wicked supports powers of two from 32 through 8192.");
        renderLightmapResolution_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            lightmapBakeSettings_.resolution = static_cast<std::uint32_t>(args.userdata);
        });
        renderWorkspacePanel_.AddWidget(&renderLightmapResolution_);

        renderLightmapUvSource_.Create("Render Lightmap UV Source");
        renderLightmapUvSource_.AddItem(
            "GENERATE ATLAS",
            static_cast<std::uint64_t>(bridge::LightmapUvSource::GenerateAtlas));
        renderLightmapUvSource_.AddItem(
            "KEEP ATLAS",
            static_cast<std::uint64_t>(bridge::LightmapUvSource::KeepAtlas));
        renderLightmapUvSource_.AddItem(
            "COPY UV 0",
            static_cast<std::uint64_t>(bridge::LightmapUvSource::CopyUv0));
        renderLightmapUvSource_.AddItem(
            "COPY UV 1",
            static_cast<std::uint64_t>(bridge::LightmapUvSource::CopyUv1));
        renderLightmapUvSource_.SetSelectedByUserdataWithoutCallback(
            static_cast<std::uint64_t>(bridge::LightmapUvSource::GenerateAtlas));
        renderLightmapUvSource_.SetTooltip(
            "Choose the native Wicked lightmap atlas source. Generate Atlas uses pinned xatlas.");
        renderLightmapUvSource_.OnSelect([this](const wi::gui::EventArgs& args)
        {
            lightmapBakeSettings_.uvSource =
                static_cast<bridge::LightmapUvSource>(args.userdata);
        });
        renderWorkspacePanel_.AddWidget(&renderLightmapUvSource_);

        renderLightmapBlockCompression_.Create("BC6H block compression: ");
        renderLightmapBlockCompression_.SetCheck(true);
        renderLightmapBlockCompression_.SetTooltip(
            "Store the baked lightmap using Wicked BC6H compression. Disable for native R11G11B10_FLOAT storage.");
        renderLightmapBlockCompression_.OnClick([this](const wi::gui::EventArgs& args)
        {
            lightmapBakeSettings_.blockCompression = args.bValue;
        });
        renderWorkspacePanel_.AddWidget(&renderLightmapBlockCompression_);

        renderLightmapStart_.Create("Render Lightmap Start");
        renderLightmapStart_.SetText("START BAKE");
        renderLightmapStart_.SetTooltip(
            "Prepare the selected static mesh and start Wicked's native progressive lightmap bake.");
        renderLightmapStart_.OnClick([this](const wi::gui::EventArgs&)
        {
            BeginSelectedLightmapBake();
        });
        renderWorkspacePanel_.AddWidget(&renderLightmapStart_);

        renderLightmapStop_.Create("Render Lightmap Stop");
        renderLightmapStop_.SetText("STOP & SAVE");
        renderLightmapStop_.SetTooltip(
            "Stop the native progressive bake and save/compress the current result. Wicked's native denoiser finalisation remains authoritative when available.");
        renderLightmapStop_.OnClick([this](const wi::gui::EventArgs&)
        {
            FinishSelectedLightmapBake();
        });
        renderWorkspacePanel_.AddWidget(&renderLightmapStop_);

        renderLightmapClear_.Create("Render Lightmap Clear");
        renderLightmapClear_.SetText("CLEAR LIGHTMAP");
        renderLightmapClear_.SetTooltip(
            "Remove the selected object's native baked lightmap. This is Undo/Redo capable.");
        renderLightmapClear_.OnClick([this](const wi::gui::EventArgs&)
        {
            ClearSelectedLightmap();
        });
        renderWorkspacePanel_.AddWidget(&renderLightmapClear_);

        renderLightmapPreview_.Create("Render Lightmap Preview");
        renderLightmapPreview_.SetText("");
        renderLightmapPreview_.SetTooltip(
            "Preview of the selected object's current native Wicked lightmap.");
        renderLightmapPreview_.SetEnabled(false);
        renderLightmapPreview_.SetVisible(false);
        renderWorkspacePanel_.AddWidget(&renderLightmapPreview_);

        createSection(
            renderVertexAoBakeLabel_,
            "Render Vertex AO Section",
            "VERTEX AO // SELECTED STATIC GEOMETRY");

        renderVertexAoBakeStatus_.Create("Render Vertex AO Status");
        renderVertexAoBakeStatus_.font.params.size = 12;
        renderVertexAoBakeStatus_.font.params.color = RenderMuted;
        renderVertexAoBakeStatus_.font.params.h_align = wi::font::WIFALIGN_LEFT;
        renderVertexAoBakeStatus_.SetColor(wi::Color::Transparent());
        renderWorkspacePanel_.AddWidget(&renderVertexAoBakeStatus_);

        renderVertexAoRayCount_.Create(
            8.0f, 1024.0f, 256.0f, 1016.0f,
            "Render Vertex AO Rays", "RAY COUNT");
        renderVertexAoRayCount_.SetTooltip(
            "Native Wicked Vertex AO rays per vertex. Higher values increase bake quality and cost.");
        renderVertexAoRayCount_.OnValueCommitted([this](const float value)
        {
            vertexAoBakeSettings_.rayCount = std::clamp(
                static_cast<std::uint32_t>(std::lround(value)), 8u, 1024u);
        });
        renderWorkspacePanel_.AddWidget(&renderVertexAoRayCount_);

        renderVertexAoRayLength_.Create(
            0.0f, 1000.0f, 100.0f, 1000.0f,
            "Render Vertex AO Ray Length", "RAY LENGTH");
        renderVertexAoRayLength_.SetTooltip(
            "Native Wicked Vertex AO ray length. Shorter rays reduce occlusion from large distant features.");
        renderVertexAoRayLength_.OnValueCommitted([this](const float value)
        {
            vertexAoBakeSettings_.rayLength = std::clamp(value, 0.0f, 1000.0f);
        });
        renderWorkspacePanel_.AddWidget(&renderVertexAoRayLength_);

        renderVertexAoCompute_.Create("Render Vertex AO Compute");
        renderVertexAoCompute_.SetText("COMPUTE VERTEX AO");
        renderVertexAoCompute_.SetTooltip(
            "Bake Wicked's native per-vertex ambient occlusion for the selected static mesh.");
        renderVertexAoCompute_.OnClick([this](const wi::gui::EventArgs&)
        {
            ComputeSelectedVertexAo();
        });
        renderWorkspacePanel_.AddWidget(&renderVertexAoCompute_);

        renderVertexAoClear_.Create("Render Vertex AO Clear");
        renderVertexAoClear_.SetText("CLEAR VERTEX AO");
        renderVertexAoClear_.SetTooltip(
            "Remove baked Vertex AO from the selected object. This is Undo/Redo capable.");
        renderVertexAoClear_.OnClick([this](const wi::gui::EventArgs&)
        {
            ClearSelectedVertexAo();
        });
        renderWorkspacePanel_.AddWidget(&renderVertexAoClear_);

        RefreshGate8BakeControls();
    }

    void StudioRenderPath::LayoutGate8BakeControls(
        const float fieldWidth,
        float& y)
    {
        const auto section = [fieldWidth, &y](wi::gui::Widget& widget)
        {
            widget.SetPos(XMFLOAT2(12.0f, y));
            widget.SetSize(XMFLOAT2(fieldWidth, 20.0f));
            y += 24.0f;
        };
        const auto full = [fieldWidth, &y](wi::gui::Widget& widget)
        {
            widget.SetPos(XMFLOAT2(12.0f, y));
            widget.SetSize(XMFLOAT2(fieldWidth, 28.0f));
            y += 32.0f;
        };

        section(renderLightmapBakeLabel_);
        renderLightmapBakeStatus_.SetPos(XMFLOAT2(12.0f, y));
        renderLightmapBakeStatus_.SetSize(XMFLOAT2(fieldWidth, 42.0f));
        y += 46.0f;
        full(renderLightmapResolution_);
        full(renderLightmapUvSource_);
        full(renderLightmapBlockCompression_);
        full(renderLightmapStart_);
        full(renderLightmapStop_);
        full(renderLightmapClear_);

        renderLightmapPreview_.SetPos(XMFLOAT2(12.0f, y));
        renderLightmapPreview_.SetSize(XMFLOAT2(
            fieldWidth,
            std::min(180.0f, fieldWidth * 0.5625f)));
        if (renderLightmapPreview_.IsVisible())
            y += renderLightmapPreview_.GetSize().y + 8.0f;

        section(renderVertexAoBakeLabel_);
        full(renderVertexAoBakeStatus_);
        full(renderVertexAoRayCount_);
        full(renderVertexAoRayLength_);
        full(renderVertexAoCompute_);
        full(renderVertexAoClear_);
    }

    std::vector<wi::ecs::Entity> StudioRenderPath::SelectedBakeTargets() const
    {
        std::vector<wi::ecs::Entity> targets;
        if (session_ == nullptr || !session_->Selection().HasSelection())
            return targets;
        const wi::ecs::Entity selected = session_->Selection().SelectedEntity();
        if (selected != wi::ecs::INVALID_ENTITY)
            targets.push_back(selected);
        return targets;
    }

    void StudioRenderPath::RefreshGate8BakeControls()
    {
        if (session_ == nullptr)
        {
            renderLightmapBakeStatus_.SetText("NO ACTIVE STUDIO SESSION");
            renderVertexAoBakeStatus_.SetText("NO ACTIVE STUDIO SESSION");
            renderLightmapStart_.SetEnabled(false);
            renderLightmapStop_.SetEnabled(false);
            renderLightmapClear_.SetEnabled(false);
            renderVertexAoCompute_.SetEnabled(false);
            renderVertexAoClear_.SetEnabled(false);
            renderLightmapPreview_.SetVisible(false);
            return;
        }

        auto& scene = session_->Scenes().GetScene();
        const bool active = lightmapBakeSession_.IsActive();
        const std::vector<wi::ecs::Entity> selected = SelectedBakeTargets();
        const std::vector<wi::ecs::Entity>& statusTargets =
            active ? lightmapBakeSession_.Targets() : selected;
        const bridge::LightmapBakeStatus status =
            bridge::LightmapBakeService::CaptureStatus(scene, statusTargets);

        std::ostringstream bakeText;
        if (!renderBakeLastMessage_.empty())
            bakeText << renderBakeLastMessage_ << " // ";
        if (active)
        {
            bakeText << "BAKING " << status.activeCount << "/" << status.targetCount
                     << " // ITERATION " << status.highestIteration;
        }
        else if (status.eligible)
        {
            bakeText << "READY // " << status.bakedCount << "/" << status.targetCount
                     << " TARGETS HAVE SAVED LIGHTMAPS";
        }
        else
        {
            bakeText << status.message;
        }
        renderLightmapBakeStatus_.SetText(bakeText.str());

        std::ostringstream aoText;
        if (status.eligible)
        {
            aoText << status.vertexAoCount << "/" << status.targetCount
                   << " TARGETS HAVE VERTEX AO";
        }
        else
        {
            aoText << status.message;
        }
        renderVertexAoBakeStatus_.SetText(aoText.str());

        const bool realtime = !pathTracePreviewActive_;
        renderPathTraceToggle_.SetEnabled(!active);
        renderLightmapResolution_.SetEnabled(status.eligible && !active && realtime);
        renderLightmapUvSource_.SetEnabled(status.eligible && !active && realtime);
        renderLightmapBlockCompression_.SetEnabled(status.eligible && !active && realtime);
        renderLightmapStart_.SetEnabled(status.eligible && !active && realtime);
        renderLightmapStop_.SetEnabled(active);
        renderLightmapClear_.SetEnabled(
            status.eligible && !active && status.bakedCount > 0);
        renderVertexAoRayCount_.SetEnabled(status.eligible && !active);
        renderVertexAoRayLength_.SetEnabled(status.eligible && !active);
        renderVertexAoCompute_.SetEnabled(status.eligible && !active);
        renderVertexAoClear_.SetEnabled(
            status.eligible && !active && status.vertexAoCount > 0);

        bool previewVisible = false;
        if (!selected.empty())
        {
            if (const auto* object = scene.objects.GetComponent(selected.front()))
            {
                if (object->lightmap.IsValid())
                {
                    wi::Resource resource;
                    resource.SetTexture(object->lightmap);
                    renderLightmapPreview_.SetImage(resource);
                    previewVisible = true;
                }
            }
        }
        if (!previewVisible)
        {
            wi::Resource empty;
            renderLightmapPreview_.SetImage(empty);
        }
        if (renderLightmapPreview_.IsVisible() != previewVisible)
        {
            renderLightmapPreview_.SetVisible(previewVisible);
            LayoutRenderWorkspace();
        }
    }

    void StudioRenderPath::TickGate8BakeControls()
    {
        if (!renderWorkspaceActive_)
            return;
        RefreshGate8BakeControls();
    }

    void StudioRenderPath::BeginSelectedLightmapBake()
    {
        if (session_ == nullptr)
            return;
        if (pathTracePreviewActive_)
        {
            renderBakeLastMessage_ = "RETURN TO REALTIME BEFORE LIGHTMAP BAKING";
            RefreshGate8BakeControls();
            return;
        }

        std::string error;
        if (!bridge::LightmapBakeService::BeginBake(
                session_->Scenes().GetScene(),
                SelectedBakeTargets(),
                lightmapBakeSettings_,
                lightmapBakeSession_,
                error))
        {
            renderBakeLastMessage_ = error;
            studioChrome_.SetStatusText("RENDER // LIGHTMAP BAKE FAILED");
            RefreshGate8BakeControls();
            return;
        }

        renderBakeLastMessage_ = "LIGHTMAP BAKE STARTED";
        studioChrome_.SetStatusText("RENDER // LIGHTMAP BAKING");
        RefreshGate8BakeControls();
    }

    void StudioRenderPath::FinishSelectedLightmapBake()
    {
        if (session_ == nullptr)
            return;
        std::string error;
        if (!bridge::LightmapBakeService::StopAndSave(
                session_->Scenes().GetScene(),
                lightmapBakeSession_,
                session_->Commands(),
                error))
        {
            renderBakeLastMessage_ = error;
            studioChrome_.SetStatusText("RENDER // LIGHTMAP SAVE FAILED");
            RefreshGate8BakeControls();
            return;
        }

        renderBakeLastMessage_ = "LIGHTMAP SAVED // UNDO AVAILABLE";
        studioChrome_.SetStatusText("RENDER // LIGHTMAP SAVED");
        RefreshGate8BakeControls();
    }

    void StudioRenderPath::CancelGate8Bake()
    {
        if (session_ == nullptr || !lightmapBakeSession_.IsActive())
            return;
        bridge::LightmapBakeService::Cancel(
            session_->Scenes().GetScene(),
            lightmapBakeSession_);
        renderBakeLastMessage_ = "LIGHTMAP BAKE CANCELLED";
        RefreshGate8BakeControls();
    }

    void StudioRenderPath::ClearSelectedLightmap()
    {
        if (session_ == nullptr)
            return;
        std::string error;
        if (!bridge::LightmapBakeService::ClearLightmaps(
                session_->Scenes().GetScene(),
                SelectedBakeTargets(),
                session_->Commands(),
                error))
        {
            renderBakeLastMessage_ = error;
            RefreshGate8BakeControls();
            return;
        }
        renderBakeLastMessage_ = "LIGHTMAP CLEARED // UNDO AVAILABLE";
        studioChrome_.SetStatusText("RENDER // LIGHTMAP CLEARED");
        RefreshGate8BakeControls();
    }

    void StudioRenderPath::ComputeSelectedVertexAo()
    {
        if (session_ == nullptr)
            return;
        std::string error;
        studioChrome_.SetStatusText("RENDER // COMPUTING VERTEX AO");
        if (!bridge::LightmapBakeService::ComputeVertexAo(
                session_->Scenes().GetScene(),
                SelectedBakeTargets(),
                vertexAoBakeSettings_,
                session_->Commands(),
                error))
        {
            renderBakeLastMessage_ = error;
            studioChrome_.SetStatusText("RENDER // VERTEX AO FAILED");
            RefreshGate8BakeControls();
            return;
        }
        renderBakeLastMessage_ = "VERTEX AO BAKED // UNDO AVAILABLE";
        studioChrome_.SetStatusText("RENDER // VERTEX AO COMPLETE");
        RefreshGate8BakeControls();
    }

    void StudioRenderPath::ClearSelectedVertexAo()
    {
        if (session_ == nullptr)
            return;
        std::string error;
        if (!bridge::LightmapBakeService::ClearVertexAo(
                session_->Scenes().GetScene(),
                SelectedBakeTargets(),
                session_->Commands(),
                error))
        {
            renderBakeLastMessage_ = error;
            RefreshGate8BakeControls();
            return;
        }
        renderBakeLastMessage_ = "VERTEX AO CLEARED // UNDO AVAILABLE";
        studioChrome_.SetStatusText("RENDER // VERTEX AO CLEARED");
        RefreshGate8BakeControls();
    }
}
