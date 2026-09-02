#include "RenegadeAudioWorkspace.h"

#include "RenegadeStudioChrome.h"
#include "renegade/bridge/AudioService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using renegade::studio::RenegadeButton;
    using renegade::studio::SceneInspectorCheckBox;
    using renegade::studio::SceneInspectorComboBox;
    using renegade::studio::SceneInspectorSlider;

    constexpr float HeaderHeight = 66.0f;
    constexpr float RowHeight = 30.0f;
    constexpr float RowGap = 6.0f;
    constexpr float SectionGap = 12.0f;

    constexpr wi::Color Surface0 = wi::Color(8, 12, 16, 255);
    constexpr wi::Color Surface1 = wi::Color(12, 18, 22, 255);
    constexpr wi::Color Border = wi::Color(38, 52, 61, 255);
    constexpr wi::Color TextStrong = wi::Color(244, 244, 244, 255);
    constexpr wi::Color TextSecondary = wi::Color(214, 222, 226, 255);
    constexpr wi::Color Muted = wi::Color(139, 151, 158, 255);
    constexpr wi::Color Forge = wi::Color(210, 91, 29, 255);
    constexpr wi::Color Error = wi::Color(229, 92, 92, 255);

    void DrawRect(
        const float x,
        const float y,
        const float width,
        const float height,
        const wi::Color color,
        const wi::graphics::CommandList cmd)
    {
        if (width <= 0.0f || height <= 0.0f)
            return;
        wi::image::Params params(x, y, width, height, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
    }

    void DrawText(
        const std::string& value,
        const float x,
        const float y,
        const int size,
        const wi::Color color,
        const wi::graphics::CommandList cmd,
        const float bolden = 0.12f)
    {
        wi::font::Params params(
            x,
            y,
            size,
            wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP,
            color,
            wi::Color::Transparent());
        params.bolden = bolden;
        wi::font::Draw(value, params, cmd);
    }

    bool IsInsideProject(
        const fs::path& candidate,
        const fs::path& projectRoot)
    {
        std::error_code ec;
        const auto relative = fs::relative(candidate, projectRoot, ec);
        if (ec || relative.empty())
            return !ec && candidate == projectRoot;
        const std::string value = relative.generic_u8string();
        return value != ".." && value.rfind("../", 0) != 0 && !relative.is_absolute();
    }

    fs::path UniqueAudioDestination(
        const fs::path& folder,
        const fs::path& source)
    {
        fs::path candidate = folder / source.filename();
        if (!fs::exists(candidate))
            return candidate;

        const std::string stem = source.stem().generic_u8string();
        const std::string extension = source.extension().generic_u8string();
        for (int suffix = 2; suffix < 10000; ++suffix)
        {
            candidate = folder / fs::u8path(
                stem + "_" + std::to_string(suffix) + extension);
            if (!fs::exists(candidate))
                return candidate;
        }
        return {};
    }

    const char* ReverbName(const wi::audio::REVERB_PRESET preset) noexcept
    {
        using P = wi::audio::REVERB_PRESET;
        switch (preset)
        {
        case P::REVERB_PRESET_DEFAULT: return "DEFAULT";
        case P::REVERB_PRESET_GENERIC: return "GENERIC";
        case P::REVERB_PRESET_FOREST: return "FOREST";
        case P::REVERB_PRESET_PADDEDCELL: return "PADDED CELL";
        case P::REVERB_PRESET_ROOM: return "ROOM";
        case P::REVERB_PRESET_BATHROOM: return "BATHROOM";
        case P::REVERB_PRESET_LIVINGROOM: return "LIVING ROOM";
        case P::REVERB_PRESET_STONEROOM: return "STONE ROOM";
        case P::REVERB_PRESET_AUDITORIUM: return "AUDITORIUM";
        case P::REVERB_PRESET_CONCERTHALL: return "CONCERT HALL";
        case P::REVERB_PRESET_CAVE: return "CAVE";
        case P::REVERB_PRESET_ARENA: return "ARENA";
        case P::REVERB_PRESET_HANGAR: return "HANGAR";
        case P::REVERB_PRESET_CARPETEDHALLWAY: return "CARPETED HALLWAY";
        case P::REVERB_PRESET_HALLWAY: return "HALLWAY";
        case P::REVERB_PRESET_STONECORRIDOR: return "STONE CORRIDOR";
        case P::REVERB_PRESET_ALLEY: return "ALLEY";
        case P::REVERB_PRESET_CITY: return "CITY";
        case P::REVERB_PRESET_MOUNTAINS: return "MOUNTAINS";
        case P::REVERB_PRESET_QUARRY: return "QUARRY";
        case P::REVERB_PRESET_PLAIN: return "PLAIN";
        case P::REVERB_PRESET_PARKINGLOT: return "PARKING LOT";
        case P::REVERB_PRESET_SEWERPIPE: return "SEWER PIPE";
        case P::REVERB_PRESET_UNDERWATER: return "UNDERWATER";
        case P::REVERB_PRESET_SMALLROOM: return "SMALL ROOM";
        case P::REVERB_PRESET_MEDIUMROOM: return "MEDIUM ROOM";
        case P::REVERB_PRESET_LARGEROOM: return "LARGE ROOM";
        case P::REVERB_PRESET_MEDIUMHALL: return "MEDIUM HALL";
        case P::REVERB_PRESET_LARGEHALL: return "LARGE HALL";
        case P::REVERB_PRESET_PLATE: return "PLATE";
        default: return "DEFAULT";
        }
    }
}

namespace renegade::studio
{
    struct RenegadeAudioWorkspace::Impl
    {
        bool created = false;
        bool active = false;
        bool pointerConsumed = false;
        bool refreshPending = true;
        XMFLOAT4 bounds = {};
        bridge::StudioSession* session = nullptr;
        wi::ecs::Entity selected = wi::ecs::INVALID_ENTITY;
        std::uint64_t sceneRevision = 0;
        std::string status = "AUDIO // READY";
        bool statusError = false;
        std::string sourceFileDisplay = "NO SOUND SOURCE SELECTED";
        std::vector<wi::gui::Widget*> controls;

        RenegadeButton addSource;
        RenegadeButton chooseAudio;
        RenegadeButton previewPlay;
        RenegadeButton previewStop;
        SceneInspectorCheckBox playOnStart;
        SceneInspectorCheckBox looped;
        SceneInspectorCheckBox spatial;
        SceneInspectorCheckBox reverb;
        SceneInspectorSlider sourceVolume;
        SceneInspectorComboBox sourceBus;

        SceneInspectorSlider masterVolume;
        SceneInspectorSlider sfxVolume;
        SceneInspectorSlider musicVolume;
        SceneInspectorSlider ambienceVolume;
        SceneInspectorSlider voiceVolume;
        SceneInspectorComboBox reverbPreset;
        wi::Resource previewResource;
        wi::audio::SoundInstance previewInstance;

        ~Impl()
        {
            StopPreview();
        }

        [[nodiscard]] wi::scene::Scene* Scene() const noexcept
        {
            return session != nullptr ? &session->Scenes().GetScene() : nullptr;
        }

        [[nodiscard]] bool SelectedIsSource() const noexcept
        {
            const auto* scene = Scene();
            return scene != nullptr &&
                bridge::IsRenegadeSoundSource(*scene, selected);
        }

        void SetStatus(std::string value, const bool error = false)
        {
            status = std::move(value);
            statusError = error;
        }

        void StopPreview() noexcept
        {
            if (previewInstance.IsValid())
                wi::audio::Stop(&previewInstance);
            previewInstance = {};
            previewResource = {};
        }

        void StartPreview()
        {
            auto* scene = Scene();
            auto* sound = scene != nullptr && SelectedIsSource()
                ? scene->sounds.GetComponent(selected)
                : nullptr;
            if (sound == nullptr || !sound->soundResource.IsValid() ||
                !sound->soundinstance.IsValid())
            {
                SetStatus("PREVIEW // AUDIO INSTANCE IS NOT READY", true);
                return;
            }

            StopPreview();
            wi::audio::SoundInstance instance;
            instance.type = wi::audio::SUBMIX_TYPE_SOUNDEFFECT;
            instance.SetLooped(false);
            instance.SetEnableReverb(false);
            if (!wi::audio::CreateSoundInstance(
                    &sound->soundResource.GetSound(), &instance))
            {
                SetStatus("PREVIEW // WICKED COULD NOT CREATE INSTANCE", true);
                return;
            }

            // Keep the decoded asset alive independently of the authored
            // component. Never call Update3D on this instance: Studio Preview
            // is an explicit 2D audition path and cannot be stopped by the
            // Scene SoundComponent PLAYING lifecycle.
            previewResource = sound->soundResource;
            previewInstance = std::move(instance);
            wi::audio::SetVolume(sound->volume, &previewInstance);
            wi::audio::Play(&previewInstance);
            SetStatus("PREVIEW // PLAYING // 2D AUDITION");
        }

        template <typename Command, typename... Args>
        bool ExecuteCommand(Args&&... args)
        {
            if (session == nullptr)
                return false;
            const bool changed = session->Commands().Execute(
                std::make_unique<Command>(std::forward<Args>(args)...));
            if (changed)
                refreshPending = true;
            return changed;
        }

        template <typename Fn>
        void EditSource(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr || !SelectedIsSource())
                return;
            StopPreview();
            const auto before = bridge::CaptureSoundSource(*scene, selected);
            auto after = before;
            edit(after);
            after = bridge::SanitizeSoundSourceState(after);
            if (ExecuteCommand<bridge::SetSoundSourceCommand>(
                    *scene, selected, before, after))
            {
                SetStatus("SOUND SOURCE // APPLIED");
            }
        }

        template <typename Fn>
        void EditMix(Fn&& edit)
        {
            auto* scene = Scene();
            if (scene == nullptr)
                return;
            const auto before = bridge::CaptureSceneAudioMix(*scene);
            auto after = before;
            edit(after);
            after = bridge::SanitizeSceneAudioMixState(after);
            if (ExecuteCommand<bridge::SetSceneAudioMixCommand>(
                    *scene, before, after))
            {
                SetStatus("SCENE MIX // APPLIED");
            }
        }

        void CreateControls()
        {
            addSource.Create("Gate 3 Add Sound Source");
            addSource.SetText("ADD SOUND SOURCE...");
            addSource.SetRenderTextSize(11);
            addSource.SetTooltip(
                "Choose a WAV/OGG, copy it into Content/Audio when needed, then create one native Wicked Sound source.");
            addSource.OnClick([this](const wi::gui::EventArgs&)
            {
                ChooseAudio(true);
            });

            chooseAudio.Create("Gate 3 Choose Audio");
            chooseAudio.SetText("AUDIO ASSET...");
            chooseAudio.SetRenderTextSize(11);
            chooseAudio.OnClick([this](const wi::gui::EventArgs&)
            {
                ChooseAudio(false);
            });

            previewPlay.Create("Gate 3 Audio Preview Play");
            previewPlay.SetText("PREVIEW PLAY");
            previewPlay.SetRenderTextSize(10);
            previewPlay.OnClick([this](const wi::gui::EventArgs&)
            {
                StartPreview();
            });

            previewStop.Create("Gate 3 Audio Preview Stop");
            previewStop.SetText("STOP");
            previewStop.SetRenderTextSize(10);
            previewStop.OnClick([this](const wi::gui::EventArgs&)
            {
                StopPreview();
                SetStatus("PREVIEW // STOPPED");
            });

            playOnStart.Create("Play on start: ");
            playOnStart.OnClick([this](const wi::gui::EventArgs& args)
            {
                EditSource([&](bridge::SoundSourceState& state)
                { state.playOnStart = args.bValue; });
            });
            looped.Create("Loop: ");
            looped.OnClick([this](const wi::gui::EventArgs& args)
            {
                EditSource([&](bridge::SoundSourceState& state)
                { state.looped = args.bValue; });
            });
            spatial.Create("Spatial 3D: ");
            spatial.OnClick([this](const wi::gui::EventArgs& args)
            {
                EditSource([&](bridge::SoundSourceState& state)
                { state.spatial = args.bValue; });
            });
            reverb.Create("Reverb send: ");
            reverb.OnClick([this](const wi::gui::EventArgs& args)
            {
                EditSource([&](bridge::SoundSourceState& state)
                { state.reverb = args.bValue; });
            });

            sourceVolume.Create(0.0f, 1.0f, 1.0f, 1000.0f,
                "Gate 3 Source Volume", "SOURCE VOLUME");
            sourceVolume.OnValueCommitted([this](const float value)
            {
                EditSource([&](bridge::SoundSourceState& state)
                { state.volume = value; });
            });

            sourceBus.Create("Gate 3 Audio Bus");
            sourceBus.AddItem("SFX", static_cast<std::uint64_t>(bridge::AudioBus::SoundEffect));
            sourceBus.AddItem("MUSIC", static_cast<std::uint64_t>(bridge::AudioBus::Music));
            sourceBus.AddItem("AMBIENCE", static_cast<std::uint64_t>(bridge::AudioBus::Ambience));
            sourceBus.AddItem("VOICE", static_cast<std::uint64_t>(bridge::AudioBus::Voice));
            sourceBus.OnSelect([this](const wi::gui::EventArgs& args)
            {
                EditSource([&](bridge::SoundSourceState& state)
                { state.bus = static_cast<bridge::AudioBus>(args.userdata); });
            });

            const auto makeMixSlider = [this](
                SceneInspectorSlider& slider,
                const char* name,
                const char* label,
                std::function<void(bridge::SceneAudioMixState&, float)> edit)
            {
                slider.Create(0.0f, 1.0f, 1.0f, 1000.0f, name, label);
                slider.OnValueCommitted(
                    [this, edit = std::move(edit)](const float value)
                    {
                        EditMix([&](bridge::SceneAudioMixState& state)
                        { edit(state, value); });
                    });
            };
            makeMixSlider(masterVolume, "Gate 3 Master Volume", "MASTER",
                [](bridge::SceneAudioMixState& s, float v) { s.masterVolume = v; });
            makeMixSlider(sfxVolume, "Gate 3 SFX Volume", "SFX",
                [](bridge::SceneAudioMixState& s, float v) { s.soundEffectVolume = v; });
            makeMixSlider(musicVolume, "Gate 3 Music Volume", "MUSIC",
                [](bridge::SceneAudioMixState& s, float v) { s.musicVolume = v; });
            makeMixSlider(ambienceVolume, "Gate 3 Ambience Volume", "AMBIENCE",
                [](bridge::SceneAudioMixState& s, float v) { s.ambienceVolume = v; });
            makeMixSlider(voiceVolume, "Gate 3 Voice Volume", "VOICE",
                [](bridge::SceneAudioMixState& s, float v) { s.voiceVolume = v; });

            reverbPreset.Create("Gate 3 Reverb Preset");
            for (int value = static_cast<int>(wi::audio::REVERB_PRESET_DEFAULT);
                value <= static_cast<int>(wi::audio::REVERB_PRESET_PLATE);
                ++value)
            {
                const auto preset = static_cast<wi::audio::REVERB_PRESET>(value);
                reverbPreset.AddItem(ReverbName(preset), static_cast<std::uint64_t>(value));
            }
            reverbPreset.OnSelect([this](const wi::gui::EventArgs& args)
            {
                EditMix([&](bridge::SceneAudioMixState& state)
                {
                    state.reverbPreset =
                        static_cast<wi::audio::REVERB_PRESET>(args.userdata);
                });
            });

            controls = {
                &addSource, &chooseAudio, &previewPlay, &previewStop,
                &playOnStart, &looped, &spatial, &reverb,
                &sourceVolume, &sourceBus,
                &masterVolume, &sfxVolume, &musicVolume,
                &ambienceVolume, &voiceVolume, &reverbPreset,
            };
            for (auto* control : controls)
                control->SetVisible(false);
            created = true;
        }

        void ChooseAudio(const bool createNew)
        {
            if (session == nullptr || !session->Projects().HasProject())
            {
                SetStatus("AUDIO // OPEN A PROJECT FIRST", true);
                return;
            }

            wi::helper::FileDialogParams params;
            params.type = wi::helper::FileDialogParams::OPEN;
            params.extensions = {"wav", "ogg"};
            wi::helper::FileDialog(
                params,
                [this, createNew](const std::string& selectedPath)
                {
                    ImportAudio(selectedPath, createNew);
                });
        }

        bool ResolveProjectAudioPath(
            const std::string& selectedPath,
            std::string& projectAudioPath,
            std::string& error)
        {
            projectAudioPath.clear();
            if (session == nullptr || selectedPath.empty())
            {
                error = "No audio file was selected.";
                return false;
            }
            const auto& project = session->Projects().CurrentProject();
            if (project.rootPath.empty())
            {
                error = "The active project has no root path.";
                return false;
            }

            std::error_code ec;
            const fs::path source = fs::weakly_canonical(fs::u8path(selectedPath), ec);
            if (ec || !fs::is_regular_file(source, ec) || ec)
            {
                error = "The selected audio file is not readable.";
                return false;
            }
            if (!bridge::ValidateAudioAssetForWicked(
                    source.generic_u8string(), error))
            {
                return false;
            }
            const fs::path root = fs::weakly_canonical(fs::u8path(project.rootPath), ec);
            if (ec)
            {
                error = "The project root could not be resolved.";
                return false;
            }

            if (IsInsideProject(source, root))
            {
                projectAudioPath = source.generic_u8string();
                error.clear();
                return true;
            }

            const fs::path folder = root / "Content" / "Audio";
            fs::create_directories(folder, ec);
            if (ec)
            {
                error = "Content/Audio could not be created.";
                return false;
            }
            const fs::path destination = UniqueAudioDestination(folder, source);
            if (destination.empty())
            {
                error = "A unique project audio filename could not be chosen.";
                return false;
            }
            fs::copy_file(source, destination, fs::copy_options::none, ec);
            if (ec)
            {
                error = "The audio file could not be copied into Content/Audio.";
                return false;
            }
            projectAudioPath = destination.generic_u8string();
            error.clear();
            return true;
        }

        void ImportAudio(const std::string& selectedPath, const bool createNew)
        {
            if (session == nullptr)
                session = bridge::StudioSession::Current();
            if (session == nullptr)
                return;

            StopPreview();

            std::string audioPath;
            std::string error;
            if (!ResolveProjectAudioPath(selectedPath, audioPath, error))
            {
                SetStatus("AUDIO // " + error, true);
                return;
            }

            auto& scene = session->Scenes().GetScene();
            if (createNew)
            {
                bridge::TransformState transform;
                if (session->Selection().HasSelection())
                {
                    const auto entity = session->Selection().SelectedEntity();
                    if (const auto* selectedTransform =
                            scene.transforms.GetComponent(entity))
                    {
                        transform = bridge::CaptureTransform(*selectedTransform);
                        transform.rotation = XMFLOAT4(0, 0, 0, 1);
                        transform.scale = XMFLOAT3(1, 1, 1);
                    }
                }

                bridge::SoundSourceState state;
                state.filename = audioPath;
                auto command = std::make_unique<bridge::CreateSoundSourceCommand>(
                    scene, state, transform);
                auto* createdCommand = command.get();
                if (!session->Commands().Execute(std::move(command)))
                {
                    SetStatus("AUDIO // SOUND SOURCE CREATION FAILED", true);
                    return;
                }
                selected = createdCommand->CreatedEntity();
                session->Selection().Select(selected);
                refreshPending = true;
                SetStatus("SOUND SOURCE // CREATED");
                return;
            }

            if (!SelectedIsSource())
            {
                SetStatus("AUDIO // SELECT A SOUND SOURCE FIRST", true);
                return;
            }
            const auto before = bridge::CaptureSoundSource(scene, selected);
            auto after = before;
            after.filename = audioPath;
            if (ExecuteCommand<bridge::SetSoundSourceCommand>(
                    scene, selected, before, after))
            {
                SetStatus("SOUND SOURCE // AUDIO ASSET ASSIGNED");
            }
        }

        void Refresh()
        {
            if (session == nullptr)
                session = bridge::StudioSession::Current();

            wi::ecs::Entity next = wi::ecs::INVALID_ENTITY;
            if (session != nullptr && session->Selection().HasSelection())
                next = session->Selection().SelectedEntity();
            selected = next;

            const bool sourceSelected = SelectedIsSource();
            addSource.SetVisible(active);
            chooseAudio.SetVisible(active && sourceSelected);
            previewPlay.SetVisible(active && sourceSelected);
            previewStop.SetVisible(active && sourceSelected);
            playOnStart.SetVisible(active && sourceSelected);
            looped.SetVisible(active && sourceSelected);
            spatial.SetVisible(active && sourceSelected);
            reverb.SetVisible(active && sourceSelected);
            sourceVolume.SetVisible(active && sourceSelected);
            sourceBus.SetVisible(active && sourceSelected);

            const bool mixVisible = active && session != nullptr;
            masterVolume.SetVisible(mixVisible);
            sfxVolume.SetVisible(mixVisible);
            musicVolume.SetVisible(mixVisible);
            ambienceVolume.SetVisible(mixVisible);
            voiceVolume.SetVisible(mixVisible);
            reverbPreset.SetVisible(mixVisible);

            if (sourceSelected)
            {
                const auto source = bridge::CaptureSoundSource(*Scene(), selected);
                sourceFileDisplay = fs::u8path(source.filename).filename().generic_u8string();
                if (sourceFileDisplay.empty())
                    sourceFileDisplay = "NO AUDIO ASSET";
                playOnStart.SetCheck(source.playOnStart);
                looped.SetCheck(source.looped);
                spatial.SetCheck(source.spatial);
                reverb.SetCheck(source.reverb);
                sourceVolume.SetValue(source.volume);
                sourceBus.SetSelectedWithoutCallback(static_cast<int>(source.bus));
            }
            else
            {
                sourceFileDisplay = "NO SOUND SOURCE SELECTED";
            }

            if (mixVisible)
            {
                const auto mix = bridge::CaptureSceneAudioMix(*Scene());
                masterVolume.SetValue(mix.masterVolume);
                sfxVolume.SetValue(mix.soundEffectVolume);
                musicVolume.SetValue(mix.musicVolume);
                ambienceVolume.SetValue(mix.ambienceVolume);
                voiceVolume.SetValue(mix.voiceVolume);
                reverbPreset.SetSelectedWithoutCallback(
                    static_cast<int>(mix.reverbPreset));
            }
            refreshPending = false;
        }

        void Layout()
        {
            const float x = bounds.x + 12.0f;
            const float width = std::max(80.0f, bounds.z - 24.0f);
            float y = bounds.y + HeaderHeight;
            const auto full = [&](wi::gui::Widget& widget)
            {
                widget.SetPos(XMFLOAT2(x, y));
                widget.SetSize(XMFLOAT2(width, RowHeight));
                y += RowHeight + RowGap;
            };
            const auto two = [&](wi::gui::Widget& left, wi::gui::Widget& right)
            {
                const float half = (width - RowGap) * 0.5f;
                left.SetPos(XMFLOAT2(x, y));
                right.SetPos(XMFLOAT2(x + half + RowGap, y));
                left.SetSize(XMFLOAT2(half, RowHeight));
                right.SetSize(XMFLOAT2(half, RowHeight));
                y += RowHeight + RowGap;
            };

            full(addSource);
            y += SectionGap;
            full(chooseAudio);
            two(previewPlay, previewStop);
            two(playOnStart, looped);
            two(spatial, reverb);
            full(sourceVolume);
            full(sourceBus);
            y += SectionGap + 18.0f;
            full(masterVolume);
            full(sfxVolume);
            full(musicVolume);
            full(ambienceVolume);
            full(voiceVolume);
            full(reverbPreset);
        }
    };

    RenegadeAudioWorkspace::RenegadeAudioWorkspace()
        : impl_(std::make_unique<Impl>())
    {
    }

    RenegadeAudioWorkspace::~RenegadeAudioWorkspace() = default;

    void RenegadeAudioWorkspace::Create()
    {
        if (!impl_->created)
        {
            SetName("Gate 3 Audio Workspace");
            wi::gui::Widget::SetVisible(false);
            impl_->CreateControls();
        }
    }

    void RenegadeAudioWorkspace::SetActive(const bool active)
    {
        impl_->active = active;
        impl_->refreshPending = true;
        wi::gui::Widget::SetVisible(active);
        if (!active)
        {
            impl_->StopPreview();
            for (auto* control : impl_->controls)
                control->SetVisible(false);
        }
    }

    bool RenegadeAudioWorkspace::IsActive() const noexcept
    {
        return impl_->active;
    }

    void RenegadeAudioWorkspace::SetBounds(const XMFLOAT4& bounds)
    {
        impl_->bounds = bounds;
        SetPos(XMFLOAT2(bounds.x, bounds.y));
        SetSize(XMFLOAT2(bounds.z, bounds.w));
        if (impl_->created)
            impl_->Layout();
    }

    bool RenegadeAudioWorkspace::ContainsPointer(
        const XMFLOAT4& pointer) const noexcept
    {
        const auto& b = impl_->bounds;
        return impl_->active && pointer.x >= b.x && pointer.x < b.x + b.z &&
            pointer.y >= b.y && pointer.y < b.y + b.w;
    }

    bool RenegadeAudioWorkspace::ConsumedPointerThisFrame() const noexcept
    {
        return impl_->pointerConsumed;
    }

    bool RenegadeAudioWorkspace::HasSelectedSoundSource() const noexcept
    {
        return impl_->SelectedIsSource();
    }

    void RenegadeAudioWorkspace::CreateSoundSource()
    {
        impl_->ChooseAudio(true);
    }

    void RenegadeAudioWorkspace::Update(
        const wi::Canvas& canvas,
        const float dt)
    {
        impl_->pointerConsumed = false;
        if (!impl_->created || !impl_->active)
            return;

        wi::gui::Widget::Update(canvas, dt);

        auto* currentSession = bridge::StudioSession::Current();
        const wi::ecs::Entity currentSelection =
            currentSession != nullptr && currentSession->Selection().HasSelection()
                ? currentSession->Selection().SelectedEntity()
                : wi::ecs::INVALID_ENTITY;
        const std::uint64_t currentRevision = currentSession != nullptr
            ? currentSession->Scenes().Revision()
            : 0;
        if (currentSession != impl_->session ||
            currentSelection != impl_->selected ||
            currentRevision != impl_->sceneRevision)
        {
            impl_->StopPreview();
            impl_->session = currentSession;
            impl_->sceneRevision = currentRevision;
            impl_->refreshPending = true;
        }
        if (impl_->refreshPending)
        {
            impl_->Refresh();
            impl_->Layout();
        }

        const XMFLOAT4 pointer = wi::input::GetPointer();
        impl_->pointerConsumed = ContainsPointer(pointer);
        if (impl_->pointerConsumed)
        {
            // The Audio surface is visually above the ordinary Inspector.
            // Propagate that same priority to Wicked's input scheduler so
            // hidden Inspector controls cannot react through it.
            Activate();
        }
        else
        {
            state = wi::gui::IDLE;
        }
        for (auto* control : impl_->controls)
        {
            if (control->IsVisible())
                control->Update(canvas, dt);
        }
    }

    void RenegadeAudioWorkspace::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        if (!impl_->created || !impl_->active)
            return;

        const auto& b = impl_->bounds;
        DrawRect(b.x, b.y, b.z, b.w, Surface0, cmd);
        DrawRect(b.x, b.y, b.z, 1.0f, Border, cmd);
        DrawText("SOUND SOURCE // NATIVE WICKED AUDIO",
            b.x + 12.0f, b.y + 10.0f, 13, TextStrong, cmd);
        DrawText(impl_->sourceFileDisplay,
            b.x + 12.0f, b.y + 32.0f, 10,
            impl_->SelectedIsSource() ? TextSecondary : Muted, cmd, 0.08f);

        const float mixLabelY =
            b.y + HeaderHeight +
            (6.0f * (RowHeight + RowGap)) +
            (2.0f * SectionGap) + 20.0f;
        DrawText("SCENE MIX // WICKED SUBMIX + REVERB",
            b.x + 12.0f, mixLabelY, 11, Forge, cmd);

        for (auto* control : impl_->controls)
        {
            if (control->IsVisible())
                control->Render(canvas, cmd);
        }

        const float statusY = b.y + b.w - 28.0f;
        DrawRect(b.x + 8.0f, statusY - 4.0f, b.z - 16.0f, 24.0f, Surface1, cmd);
        DrawText(impl_->status,
            b.x + 12.0f,
            statusY,
            9,
            impl_->statusError ? Error : Muted,
            cmd,
            0.08f);
    }
}
