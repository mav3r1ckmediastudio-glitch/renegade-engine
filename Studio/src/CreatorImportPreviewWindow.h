#pragma once

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>

#include <WickedEngine.h>

#include "renegade/bridge/CreatorExternalAnimationImportService.h"
#include "renegade/bridge/StudioSession.h"
#include "RenegadeStudioChrome.h"

namespace renegade::studio
{
    // Decorative native shell behind the real stage controls. It is attached
    // last so Wicked renders it first; it never participates in hit testing.
    class CreatorImportInspectorShell final : public wi::gui::Widget
    {
    public:
        CreatorImportInspectorShell()
        {
            SetName("Importer Inspector Shell");
            SetShadowRadius(0.0f);
        }

        void SetBodyBounds(const float top, const float height) noexcept
        {
            bodyTop_ = top;
            bodyHeight_ = height;
        }

        void Render(const wi::Canvas&, const wi::graphics::CommandList cmd) const override
        {
            if (!IsVisible())
                return;
            const float x = translation.x;
            const float y = translation.y;
            const float width = scale.x;
            const auto draw = [cmd](const float left, const float top,
                const float w, const float h, const wi::Color color)
            {
                if (w <= 0.0f || h <= 0.0f)
                    return;
                wi::image::Params params(left, top, w, h, color);
                params.blendFlag = wi::enums::BLENDMODE_ALPHA;
                wi::image::Draw(nullptr, params, cmd);
            };

            // The title, status and help text remain genuine native labels.
            draw(x + 1.0f, y + 1.0f, width - 2.0f, 141.0f,
                wi::Color(13, 20, 26, 255));
            draw(x + 1.0f, y + 1.0f, 3.0f, 141.0f,
                wi::Color(209, 125, 73, 255));
            draw(x + 12.0f, y + 141.0f, width - 24.0f, 1.0f,
                wi::Color(54, 69, 80, 255));

            if (bodyHeight_ > 0.0f)
            {
                // A separate stage surface keeps editor-style controls from
                // appearing as loose rows underneath a highlighted heading.
                draw(x + 12.0f, y + bodyTop_, width - 24.0f, bodyHeight_,
                    wi::Color(26, 35, 43, 255));
                draw(x + 12.0f, y + bodyTop_, 2.0f, bodyHeight_,
                    wi::Color(161, 98, 60, 255));
            }
        }

    private:
        float bodyTop_ = 0.0f;
        float bodyHeight_ = 0.0f;
    };

    // The model importer is a presentation workspace, not part of the authored
    // level. Entering it therefore snapshots the scene's visible sky/fog state
    // and replaces only those presentation fields with a neutral backdrop.
    // Ambient light is deliberately left alone here because the existing
    // importer lighting transaction already snapshots/restores it.
    //
    // This class shadows Window::SetVisible() so every existing importer
    // enter/exit path (normal open, cancel and governed commit) gets the same
    // weather isolation without adding a second lifecycle to StudioRenderPath.
    class CreatorImportPreviewWindow final : public wi::gui::Window
    {
    public:
        // Caller owns this scene until after SetVisible(false).
        // Weather changes must never leak into the authored document.
        void SetPreviewScene(wi::scene::Scene* previewScene) noexcept
        {
            previewScene_ = previewScene;
        }

        void InitializeInspectorShell()
        {
            // Last child is rendered beneath the actual headings and inputs.
            AddWidget(&inspectorShell_);
            inspectorShell_.SetEnabled(false);
            inspectorShell_.priority_change = false;
        }

        void LayoutInspectorShell(const float width,
            const float bodyTop, const float bodyHeight)
        {
            inspectorShell_.SetPos(XMFLOAT2(0.0f, 0.0f));
            inspectorShell_.SetSize(XMFLOAT2(width, 142.0f));
            inspectorShell_.SetBodyBounds(bodyTop, bodyHeight);
        }

        void OffsetVisibleStageContent(const float offset)
        {
            for (wi::gui::Widget* widget : widgets)
            {
                if (widget == nullptr || !widget->IsVisible() ||
                    widget->GetName().rfind("Importer Stage ", 0) == 0 ||
                    widget->GetName() == "MODEL IMPORTER // PREVIEW BEFORE COMMIT")
                    continue;
                if (widget == &label || widget->GetPos().y < 178.0f)
                    continue;
                const XMFLOAT2 position = widget->GetPos();
                widget->SetPos(XMFLOAT2(position.x, position.y + offset));
            }
        }

        void SetVisible(const bool visible)
        {
            const bool openingPreview = visible && !previewWeatherCaptured_;
            const bool closingPreview = !visible && previewWeatherCaptured_;
            if (openingPreview)
            {
                CaptureAndNeutralizePreviewWeather();
                if (auto* session = bridge::StudioSession::Current();
                    session != nullptr && session->Projects().HasProject())
                {
                    bridge::BeginCreatorExternalAnimationImportSession(
                        session->Projects().CurrentProject().rootPath);
                }
            }
            else if (!visible && previewWeatherCaptured_)
            {
                RestorePreviewWeather();
            }

            wi::gui::Window::SetVisible(visible);
            if (closingPreview)
            {
                bridge::ClearCreatorExternalAnimationImportSession();
                externalAnimationSignature_.clear();
                externalAnimationSelected_ = 0;
                externalAnimationMessage_.clear();

                // DismissImportScalePanel refreshes the Inspector and then
                // makes its parent Window visible again. Wicked propagates that
                // visibility to the Window children, which can temporarily
                // reveal both Environment and Terrain specialist controls.
                // Re-submit the already-active workspace action here. The
                // Studio action queue processes it after the importer dismiss
                // action completes, exactly matching the user's manual heading
                // click that restores the correct specialist visibility.
                if (auto* chrome = CreatorAssetStudioChrome::Current())
                {
                    chrome->RequestCurrentWorkspaceReconcile();
                }
            }
        }

        void Update(const wi::Canvas& canvas, const float dt) override
        {
            EnsureExternalAnimationControls();
            RefreshExternalAnimationControls();
            // Review-page geometry is owned by StudioApplication's layout pass.
            // Do not reflow these widgets here: doing so races that layout and
            // can place the thumbnail surface over the capture/import controls.
            wi::gui::Window::Update(canvas, dt);
        }
    private:
        struct WeatherPresentationState
        {
            bool valid = false;
            std::uint32_t flags = 0;
            XMFLOAT3 horizon = {};
            XMFLOAT3 zenith = {};
            float skyExposure = 1.0f;
            float fogDensity = 0.0f;
            float stars = 0.0f;
            std::string skyMapName;
            wi::Resource skyMap;
        };

        void EnsureExternalAnimationControls()
        {
            if (externalAnimationControlsCreated_)
                return;

            externalAnimationHeader_.Create("Creator Character External Animation Header");
            externalAnimationHeader_.SetText("EXTERNAL ANIMATIONS");
            externalAnimationHeader_.SetColor(wi::Color::Transparent());
            externalAnimationHeader_.SetFitTextEnabled(false);
            AddWidget(&externalAnimationHeader_);

            externalAnimationAdd_.Create("Creator Character Add Animations");
            externalAnimationAdd_.SetText("+ ADD ANIMATIONS...");
            externalAnimationAdd_.SetTooltip(
                "Add one or many external animation sources to this Character import. "
                "WISCENE / FBX / GLTF / GLB / VRM / VRMA are inspected with Wicked's native importers.");
            externalAnimationAdd_.OnClick([this](const wi::gui::EventArgs&)
            {
                BrowseExternalAnimations();
            });
            AddWidget(&externalAnimationAdd_);

            externalAnimationClips_.Create("Creator Character External Animation Clips");
            externalAnimationClips_.SetTooltip(
                "External source actions queued for this Character. One-action files default to the source filename.");
            externalAnimationClips_.OnSelect([this](const wi::gui::EventArgs& args)
            {
                externalAnimationSelected_ = static_cast<std::size_t>(args.userdata);
                externalAnimationSignature_.clear();
            });
            AddWidget(&externalAnimationClips_);

            externalAnimationName_.Create("Creator Character External Animation Name");
            externalAnimationName_.SetDescription("CLIP NAME  ");
            externalAnimationName_.SetPlaceholder("Animation clip name");
            externalAnimationName_.SetCancelInputEnabled(false);
            externalAnimationName_.OnInputAccepted([this](const wi::gui::EventArgs& args)
            {
                std::string error;
                if (!bridge::RenameCreatorExternalAnimationClip(
                        externalAnimationSelected_, args.sValue, error))
                {
                    externalAnimationMessage_ = error;
                }
                else
                {
                    externalAnimationMessage_ = "External animation clip renamed.";
                }
                externalAnimationSignature_.clear();
            });
            AddWidget(&externalAnimationName_);

            externalAnimationIncluded_.Create("Creator Character External Animation Included");
            externalAnimationIncluded_.AddItem("INCLUDE", 1);
            externalAnimationIncluded_.AddItem("EXCLUDE", 0);
            externalAnimationIncluded_.SetTooltip(
                "Included clips are retained in the Character recipe. Excluded rows remain as reimport provenance.");
            externalAnimationIncluded_.OnSelect([this](const wi::gui::EventArgs& args)
            {
                std::string error;
                if (!bridge::SetCreatorExternalAnimationClipEnabled(
                        externalAnimationSelected_, args.userdata != 0, error))
                {
                    externalAnimationMessage_ = error;
                }
                externalAnimationSignature_.clear();
            });
            AddWidget(&externalAnimationIncluded_);

            externalAnimationRemove_.Create("Creator Character Remove External Animation");
            externalAnimationRemove_.SetText("REMOVE CLIP");
            externalAnimationRemove_.SetTooltip(
                "Remove this external action from the current Character import queue.");
            externalAnimationRemove_.OnClick([this](const wi::gui::EventArgs&)
            {
                std::string error;
                if (!bridge::RemoveCreatorExternalAnimationClip(
                        externalAnimationSelected_, error))
                {
                    externalAnimationMessage_ = error;
                }
                else
                {
                    const auto snapshot = bridge::CaptureCreatorExternalAnimationQueue();
                    if (externalAnimationSelected_ >= snapshot.clips.size() &&
                        externalAnimationSelected_ > 0)
                    {
                        --externalAnimationSelected_;
                    }
                    externalAnimationMessage_ = "External animation clip removed.";
                }
                externalAnimationSignature_.clear();
            });
            AddWidget(&externalAnimationRemove_);

            externalAnimationStatus_.Create("Creator Character External Animation Status");
            externalAnimationStatus_.SetColor(wi::Color::Transparent());
            externalAnimationStatus_.SetFitTextEnabled(true);
            AddWidget(&externalAnimationStatus_);

            externalAnimationControlsCreated_ = true;
            SetExternalAnimationControlsVisible(false);
        }

        [[nodiscard]] int ImportSectionIndex() const
        {
            for (wi::gui::Widget* widget : widgets)
            {
                if (widget != nullptr && widget->GetName() == "Importer Section")
                {
                    return static_cast<wi::gui::ComboBox*>(widget)->GetSelected();
                }
            }
            return -1;
        }

        [[nodiscard]] int ImportAssetKindIndex() const
        {
            for (wi::gui::Widget* widget : widgets)
            {
                if (widget != nullptr &&
                    widget->GetName() == "Creator Asset Import Kind")
                {
                    return static_cast<wi::gui::ComboBox*>(widget)->GetSelected();
                }
            }
            return -1;
        }

        void SetExternalAnimationControlsVisible(const bool visible)
        {
            if (!externalAnimationControlsCreated_)
                return;
            externalAnimationHeader_.SetVisible(visible);
            externalAnimationAdd_.SetVisible(visible);
            externalAnimationClips_.SetVisible(visible);
            externalAnimationName_.SetVisible(visible);
            externalAnimationIncluded_.SetVisible(visible);
            externalAnimationRemove_.SetVisible(visible);
            externalAnimationStatus_.SetVisible(visible);
        }

        void RefreshExternalAnimationControls()
        {
            if (!externalAnimationControlsCreated_)
                return;

            // Import kind index 1 is the CW-01 CHARACTER choice. If the creator
            // deliberately switches back to MODEL, discard the Character-only
            // external queue so it can never leak into a model recipe.
            const int assetKind = ImportAssetKindIndex();
            if (assetKind != 1)
            {
                const auto snapshot = bridge::CaptureCreatorExternalAnimationQueue();
                if (!snapshot.clips.empty())
                {
                    bridge::ClearCreatorExternalAnimationImportSession();
                    if (auto* session = bridge::StudioSession::Current();
                        session != nullptr && session->Projects().HasProject())
                    {
                        bridge::BeginCreatorExternalAnimationImportSession(
                            session->Projects().CurrentProject().rootPath);
                    }
                    externalAnimationSignature_.clear();
                    externalAnimationSelected_ = 0;
                }
                SetExternalAnimationControlsVisible(false);
                return;
            }

            // Current importer navigation index 4 is ANIMATION.
            if (ImportSectionIndex() != 4)
            {
                SetExternalAnimationControlsVisible(false);
                return;
            }

            SetExternalAnimationControlsVisible(true);
            constexpr float x = 12.0f;
            const float width = std::max(160.0f, GetSize().x - 24.0f);
            // Existing embedded animation controls end at y=396. External
            // ingestion deliberately continues beneath them in the same
            // scrollable ANIMATION page.
            float y = 404.0f;
            externalAnimationHeader_.SetPos(XMFLOAT2(x, y));
            externalAnimationHeader_.SetSize(XMFLOAT2(width, 22.0f));
            y += 26.0f;
            externalAnimationAdd_.SetPos(XMFLOAT2(x, y));
            externalAnimationAdd_.SetSize(XMFLOAT2(width, 30.0f));
            y += 36.0f;
            externalAnimationClips_.SetPos(XMFLOAT2(x, y));
            externalAnimationClips_.SetSize(XMFLOAT2(width, 30.0f));
            y += 36.0f;
            externalAnimationName_.SetPos(XMFLOAT2(x, y));
            externalAnimationName_.SetSize(XMFLOAT2(width, 30.0f));
            y += 36.0f;
            const float half = (width - 8.0f) * 0.5f;
            externalAnimationIncluded_.SetPos(XMFLOAT2(x, y));
            externalAnimationIncluded_.SetSize(XMFLOAT2(half, 30.0f));
            externalAnimationRemove_.SetPos(XMFLOAT2(x + half + 8.0f, y));
            externalAnimationRemove_.SetSize(XMFLOAT2(half, 30.0f));
            y += 36.0f;
            externalAnimationStatus_.SetPos(XMFLOAT2(x, y));
            externalAnimationStatus_.SetSize(XMFLOAT2(width, 42.0f));

            const auto snapshot = bridge::CaptureCreatorExternalAnimationQueue();
            std::ostringstream signature;
            signature << snapshot.clips.size() << '|';
            for (const auto& clip : snapshot.clips)
                signature << clip.name << '|' << clip.enabled << '|' << clip.localSourcePath << ';';
            const std::string signatureText = signature.str();

            if (signatureText != externalAnimationSignature_)
            {
                if (!snapshot.clips.empty())
                    externalAnimationSelected_ = std::min(
                        externalAnimationSelected_, snapshot.clips.size() - 1);
                else
                    externalAnimationSelected_ = 0;

                externalAnimationClips_.ClearItems();
                for (std::size_t i = 0; i < snapshot.clips.size(); ++i)
                {
                    const auto& clip = snapshot.clips[i];
                    externalAnimationClips_.AddItem(
                        clip.name + "  //  " + clip.sourceDisplayName,
                        static_cast<std::uint64_t>(i));
                }
                if (!snapshot.clips.empty())
                {
                    externalAnimationClips_.SetSelectedByUserdataWithoutCallback(
                        static_cast<std::uint64_t>(externalAnimationSelected_));
                    const auto& selected = snapshot.clips[externalAnimationSelected_];
                    externalAnimationName_.SetText(selected.name);
                    externalAnimationIncluded_.SetSelectedByUserdataWithoutCallback(
                        selected.enabled ? 1u : 0u);
                }
                else
                {
                    externalAnimationName_.SetText("");
                    externalAnimationIncluded_.SetSelectedByUserdataWithoutCallback(1u);
                }
                externalAnimationSignature_ = signatureText;
            }

            const bool hasSelection = !snapshot.clips.empty() &&
                externalAnimationSelected_ < snapshot.clips.size();
            externalAnimationName_.SetEnabled(hasSelection);
            externalAnimationIncluded_.SetEnabled(hasSelection);
            externalAnimationRemove_.SetEnabled(hasSelection);

            if (hasSelection)
            {
                const auto& selected = snapshot.clips[externalAnimationSelected_];
                std::ostringstream status;
                status << bridge::HumanoidAnimationSourceFormatName(selected.sourceFormat)
                    << " // source action " << (selected.sourceAnimationIndex + 1)
                    << " // " << selected.start << " - " << selected.end
                    << " // SOURCE READY";
                if (!selected.sourceActionName.empty())
                    status << " // " << selected.sourceActionName;
                externalAnimationStatus_.SetText(
                    externalAnimationMessage_.empty()
                        ? status.str()
                        : externalAnimationMessage_ + "  //  " + status.str());
            }
            else
            {
                externalAnimationStatus_.SetText(
                    externalAnimationMessage_.empty()
                        ? "No external animation files added. Embedded actions remain available above."
                        : externalAnimationMessage_);
            }
        }

        void BrowseExternalAnimations()
        {
            auto* session = bridge::StudioSession::Current();
            if (session == nullptr || !session->Projects().HasProject())
            {
                externalAnimationMessage_ = "Open a project before adding Character animations.";
                return;
            }
            const std::string expectedProjectRoot =
                session->Projects().CurrentProject().rootPath;

            wi::helper::FileDialogParams params;
            params.type = wi::helper::FileDialogParams::OPEN;
            params.description =
                "Character animation sources (WISCENE, FBX, GLTF, GLB, VRM, VRMA)";
            params.extensions = {"wiscene", "fbx", "gltf", "glb", "vrm", "vrma"};
            params.multiselect = true;
            wi::helper::FileDialog(params,
                [this, expectedProjectRoot](const std::string& fileName)
                {
                    if (fileName.empty())
                        return;
                    wi::eventhandler::Subscribe_Once(
                        wi::eventhandler::EVENT_THREAD_SAFE_POINT,
                        [this, expectedProjectRoot, fileName](std::uint64_t)
                        {
                            auto* current = bridge::StudioSession::Current();
                            if (!IsVisible() || current == nullptr ||
                                !current->Projects().HasProject() ||
                                current->Projects().CurrentProject().rootPath != expectedProjectRoot ||
                                ImportAssetKindIndex() != 1)
                            {
                                return;
                            }

                            std::string error;
                            if (!bridge::QueueCreatorExternalAnimationSource(fileName, error))
                            {
                                externalAnimationMessage_ = error;
                            }
                            else
                            {
                                const auto snapshot = bridge::CaptureCreatorExternalAnimationQueue();
                                externalAnimationMessage_ =
                                    "Queued " + std::to_string(snapshot.clips.size()) +
                                    " external animation action(s).";
                            }
                            externalAnimationSignature_.clear();
                        });
                });
        }

        void ReflowFinalImportPage()
        {
            // The final IMPORT page also shows Asset Name and Content/Models.
            // The old fixed thumbnail block began at y=178/214, physically
            // underneath those fields (190..262). Keep the accepted square
            // preview size but place the whole final block after the fields.
            constexpr float actionBarY = 276.0f;
            constexpr float previewY = 312.0f;

            float previewSide = 244.0f;
            for (wi::gui::Widget* widget : widgets)
            {
                if (widget != nullptr &&
                    widget->GetName() == "Final Asset Thumbnail Preview")
                {
                    previewSide = std::max(1.0f, widget->GetSize().y);
                    break;
                }
            }

            const float panelWidth = GetSize().x;
            const float previewX =
                std::max(12.0f, (panelWidth - previewSide) * 0.5f);
            const float captureY = previewY + previewSide + 10.0f;
            const float statusY = captureY + 48.0f;
            const float confirmY = captureY + 96.0f;
            const float cancelY = captureY + 150.0f;

            for (wi::gui::Widget* widget : widgets)
            {
                if (widget == nullptr)
                    continue;

                const std::string& name = widget->GetName();
                if (name == "THUMBNAIL & IMPORT")
                {
                    widget->SetPos(XMFLOAT2(12.0f, actionBarY));
                }
                else if (name == "Final Asset Thumbnail Preview")
                {
                    widget->SetPos(XMFLOAT2(previewX, previewY));
                }
                else if (name == "Capture Asset Thumbnail")
                {
                    widget->SetPos(XMFLOAT2(12.0f, captureY));
                }
                else if (name == "THUMBNAIL NOT CAPTURED")
                {
                    widget->SetPos(XMFLOAT2(12.0f, statusY));
                }
                else if (name == "Import Model Commit")
                {
                    widget->SetPos(XMFLOAT2(12.0f, confirmY));
                }
                else if (name == "Cancel Model Import")
                {
                    widget->SetPos(XMFLOAT2(12.0f, cancelY));
                }
            }
        }

        static WeatherPresentationState Capture(
            const wi::scene::WeatherComponent& weather)
        {
            WeatherPresentationState state;
            state.valid = true;
            state.flags = weather._flags;
            state.horizon = weather.horizon;
            state.zenith = weather.zenith;
            state.skyExposure = weather.skyExposure;
            state.fogDensity = weather.fogDensity;
            state.stars = weather.stars;
            state.skyMapName = weather.skyMapName;
            state.skyMap = weather.skyMap;
            return state;
        }

        static void ApplyNeutral(
            wi::scene::WeatherComponent& weather)
        {
            weather.SetRealisticSky(false);
            weather.SetVolumetricClouds(false);
            weather.SetHeightFog(false);
            weather.SetOverrideFogColor(false);
            weather.horizon = XMFLOAT3(0.065f, 0.070f, 0.075f);
            weather.zenith = XMFLOAT3(0.065f, 0.070f, 0.075f);
            weather.skyExposure = 1.0f;
            weather.fogDensity = 0.0f;
            weather.stars = 0.0f;
            weather.skyMapName.clear();
            weather.skyMap = {};
        }

        static void Restore(
            wi::scene::WeatherComponent& weather,
            const WeatherPresentationState& state)
        {
            if (!state.valid)
                return;

            weather._flags = state.flags;
            weather.horizon = state.horizon;
            weather.zenith = state.zenith;
            weather.skyExposure = state.skyExposure;
            weather.fogDensity = state.fogDensity;
            weather.stars = state.stars;
            weather.skyMapName = state.skyMapName;
            weather.skyMap = state.skyMap;
        }

        void CaptureAndNeutralizePreviewWeather()
        {
            auto* session = bridge::StudioSession::Current();
            if (session == nullptr)
                return;

            auto& scene = previewScene_ != nullptr
                ? *previewScene_ : session->Scenes().GetScene();
            sceneWeatherBefore_ = Capture(scene.weather);
            weatherEntity_ = previewScene_ != nullptr
                ? wi::ecs::INVALID_ENTITY : session->Scenes().WeatherEntity();
            entityWeatherBefore_ = {};
            if (auto* weather = scene.weathers.GetComponent(weatherEntity_))
            {
                entityWeatherBefore_ = Capture(*weather);
            }

            ApplyNeutral(scene.weather);
            if (auto* weather = scene.weathers.GetComponent(weatherEntity_))
            {
                ApplyNeutral(*weather);
            }
            previewWeatherCaptured_ = true;
        }

        void RestorePreviewWeather()
        {
            auto* session = bridge::StudioSession::Current();
            if (session != nullptr)
            {
                auto& scene = previewScene_ != nullptr
                    ? *previewScene_ : session->Scenes().GetScene();
                Restore(scene.weather, sceneWeatherBefore_);
                if (auto* weather = scene.weathers.GetComponent(weatherEntity_))
                {
                    Restore(*weather, entityWeatherBefore_);
                }
            }

            previewWeatherCaptured_ = false;
            weatherEntity_ = wi::ecs::INVALID_ENTITY;
            sceneWeatherBefore_ = {};
            entityWeatherBefore_ = {};
        }

        CreatorImportInspectorShell inspectorShell_;
        wi::scene::Scene* previewScene_ = nullptr;
        bool previewWeatherCaptured_ = false;
        wi::ecs::Entity weatherEntity_ = wi::ecs::INVALID_ENTITY;
        WeatherPresentationState sceneWeatherBefore_;
        WeatherPresentationState entityWeatherBefore_;

        bool externalAnimationControlsCreated_ = false;
        std::size_t externalAnimationSelected_ = 0;
        std::string externalAnimationSignature_;
        std::string externalAnimationMessage_;
        wi::gui::Label externalAnimationHeader_;
        RenegadeButton externalAnimationAdd_;
        RenegadeComboBox externalAnimationClips_;
        RenegadeTextInputField externalAnimationName_;
        RenegadeComboBox externalAnimationIncluded_;
        RenegadeButton externalAnimationRemove_;
        wi::gui::Label externalAnimationStatus_;
    };
}
