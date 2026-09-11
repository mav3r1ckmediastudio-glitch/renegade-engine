#include "RenegadePhysicsLabStudioChrome.h"

#include "renegade/bridge/AudioService.h"
#include "renegade/bridge/CollisionService.h"
#include "renegade/bridge/NavigationService.h"
#include "renegade/bridge/ParticleEmitterService.h"
#include "renegade/bridge/PhysicsLuaService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    constexpr wi::Color Surface0 = wi::Color(8, 12, 16, 255);
    constexpr wi::Color Surface2 = wi::Color(16, 23, 28, 255);
    constexpr wi::Color Border = wi::Color(38, 52, 61, 255);
    constexpr wi::Color TextSecondary = wi::Color(226, 226, 226, 255);
    constexpr wi::Color TextStrong = wi::Color(244, 244, 244, 255);
    constexpr wi::Color Forge = wi::Color(210, 91, 29, 255);

    constexpr float TopBarHeight = 64.0f;
    constexpr float AddPopupX = 328.0f;
    constexpr float AddPopupWidth = 226.0f;
    constexpr float AddItemHeight = 30.0f;
    constexpr int ExistingAddItemCount = 9;
    constexpr float AddMenuLabelRight = 377.0f;

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

    void DrawBorderedRect(
        const float x,
        const float y,
        const float width,
        const float height,
        const wi::Color fill,
        const wi::Color border,
        const wi::graphics::CommandList cmd)
    {
        DrawRect(x, y, width, height, border, cmd);
        DrawRect(x + 1.0f, y + 1.0f, width - 2.0f, height - 2.0f, fill, cmd);
    }

    void DrawText(
        const char* text,
        const float x,
        const float y,
        const wi::Color color,
        const wi::graphics::CommandList cmd)
    {
        wi::font::Params params(
            x,
            y,
            10,
            wi::font::WIFALIGN_LEFT,
            wi::font::WIFALIGN_TOP,
            color,
            wi::Color::Transparent());
        params.spacingX = 1.1f;
        params.bolden = 0.12f;
        wi::font::Draw(text, params, cmd);
    }
}

namespace renegade::studio
{
    void RenegadePhysicsLabStudioChrome::Create()
    {
        CreatorAssetStudioChrome::Create();

        // Studio reaches this point only after wi::Application::Initialize()
        // has let the engine initialize its one Lua VM on the main thread.
        if (auto* session = bridge::StudioSession::Current(); session != nullptr)
            (void)bridge::BindPhysicsLua(session->Scenes().GetScene());

        physicsLab_.Create();
        physicsLab_.SetBounds(ViewportBounds());
        physicsLab_.SetActive(false);

        audioWorkspace_.Create();
        audioWorkspace_.SetBounds(AudioInspectorBounds());
        audioWorkspace_.SetActive(false);

        particleWorkspace_.Create();
        particleWorkspace_.SetBounds(AudioInspectorBounds());
        particleWorkspace_.SetActive(false);

        navigationWorkspace_.Create();
        navigationWorkspace_.SetBounds(AudioInspectorBounds());
        navigationWorkspace_.SetActive(false);
    }

    void RenegadePhysicsLabStudioChrome::SetLayout(
        const float width,
        const float height)
    {
        CreatorAssetStudioChrome::SetLayout(width, height);
        // Bounds only change when Studio layout changes. Relaying out every
        // frame would reset stateful GUI controls during interaction.
        physicsLab_.SetBounds(ViewportBounds());
        audioWorkspace_.SetBounds(AudioInspectorBounds());
        particleWorkspace_.SetBounds(AudioInspectorBounds());
        navigationWorkspace_.SetBounds(AudioInspectorBounds());
    }

    void RenegadePhysicsLabStudioChrome::OnAction(
        std::function<void(Action)> callback)
    {
        studioAction_ = std::move(callback);
        CreatorAssetStudioChrome::OnAction(
            [this](const Action action)
            {
                if (action == Action::ProjectHub ||
                    action == Action::SceneWorkspace ||
                    action == Action::EnvironmentWorkspace ||
                    action == Action::TerrainWorkspace ||
                    action == Action::RenderWorkspace)
                {
                    workspaceTransitionRequested_ = true;
                    particleAddMenuOpen_ = false;
                    SetActiveBottomTab(-1, true);
                    ResetDisclosureState();
                    SetPhysicsLabActive(false);
                    SetAudioWorkspaceActive(false);
                    SetParticleWorkspaceActive(false);
                    SetNavigationWorkspaceActive(false);
                }
                if (studioAction_)
                    studioAction_(action);
            });
    }

    void RenegadePhysicsLabStudioChrome::RequestCurrentWorkspaceReconcile()
    {
        if (!physicsLab_.IsActive() && !audioWorkspace_.IsActive() &&
            !particleWorkspace_.IsActive() && !navigationWorkspace_.IsActive())
        {
            RenegadeStudioChrome::RequestCurrentWorkspaceReconcile();
            return;
        }

        if (studioAction_)
            studioAction_(Action::SceneWorkspace);
        physicsLab_.SetBounds(ViewportBounds());
        audioWorkspace_.SetBounds(AudioInspectorBounds());
        particleWorkspace_.SetBounds(AudioInspectorBounds());
        navigationWorkspace_.SetBounds(AudioInspectorBounds());
    }

    void RenegadePhysicsLabStudioChrome::SetPhysicsLabActive(const bool active)
    {
        physicsLab_.SetActive(active);
        physicsLab_.SetBounds(ViewportBounds());
        if (active)
        {
            audioWorkspace_.SetActive(false);
            particleWorkspace_.SetActive(false);
            navigationWorkspace_.SetActive(false);
            SetStatusText("PHYSICS LAB");
        }
    }

    void RenegadePhysicsLabStudioChrome::SetAudioWorkspaceActive(const bool active)
    {
        audioWorkspace_.SetActive(active);
        audioWorkspace_.SetBounds(AudioInspectorBounds());
        if (active)
        {
            physicsLab_.SetActive(false);
            particleWorkspace_.SetActive(false);
            navigationWorkspace_.SetActive(false);
            SetStatusText("AUDIO // NATIVE WICKED");
        }
    }

    void RenegadePhysicsLabStudioChrome::SetParticleWorkspaceActive(
        const bool active)
    {
        particleWorkspace_.SetActive(active);
        particleWorkspace_.SetBounds(AudioInspectorBounds());
        if (active)
        {
            physicsLab_.SetActive(false);
            audioWorkspace_.SetActive(false);
            navigationWorkspace_.SetActive(false);
            SetStatusText("PARTICLE EMITTER // NATIVE WICKED GPU PARTICLES");
        }
    }

    void RenegadePhysicsLabStudioChrome::SetNavigationWorkspaceActive(
        const bool active)
    {
        navigationWorkspace_.SetActive(active);
        navigationWorkspace_.SetBounds(AudioInspectorBounds());
        if (active)
        {
            physicsLab_.SetActive(false);
            audioWorkspace_.SetActive(false);
            particleWorkspace_.SetActive(false);
            SetStatusText("NAVIGATION // NATIVE WICKED VOXEL GRID");
        }
    }

    void RenegadePhysicsLabStudioChrome::SynchronizeRigidBodyOwnerSelection()
    {
        if (!physicsLab_.IsActive())
            return;

        using Page = RenegadePhysicsLabWorkspace::Page;
        const Page page = physicsLab_.ActivePage();
        if (page != Page::RigidBody &&
            page != Page::Character &&
            page != Page::Vehicle)
        {
            // Soft Body, Ragdoll and Secondary Collider can legitimately target
            // mesh/humanoid descendants. Do not globally promote every Physics
            // Lab selection to the reusable wrapper.
            return;
        }

        auto* session = bridge::StudioSession::Current();
        if (session == nullptr || !session->Selection().HasSelection())
            return;

        auto& scene = session->Scenes().GetScene();
        const wi::ecs::Entity selected = session->Selection().SelectedEntity();
        const wi::ecs::Entity target =
            bridge::ResolveCollisionAuthoringTarget(scene, selected);
        if (target != selected && target != wi::ecs::INVALID_ENTITY)
        {
            // Rigid Body, Character and Vehicle all share the same native
            // RigidBodyPhysicsComponent. Imported child nodes are therefore
            // promoted to the stable reusable root before any of those pages
            // can create or edit body state.
            session->Selection().Select(target);
            SetStatusText("PHYSICS LAB // ASSET ROOT SELECTED");
        }
    }

    bool RenegadePhysicsLabStudioChrome::PhysicsTabHit(
        const XMFLOAT4& pointer) const noexcept
    {
        const float sceneMetaX = LayoutWidth() - 360.0f;
        return pointer.x >= sceneMetaX + 292.0f &&
            pointer.x < sceneMetaX + 356.0f &&
            pointer.y >= 34.0f && pointer.y < 58.0f;
    }

    XMFLOAT4 RenegadePhysicsLabStudioChrome::AudioViewportToolBounds() const noexcept
    {
        const XMFLOAT4 viewport = ViewportBounds();

        // RenegadeStudioChrome owns PERSPECTIVE/LIT/SHOW at viewport.x + 12.
        // Gate 3 appends AUDIO immediately after SHOW instead of stealing the
        // RENDER workspace hitbox in the fixed 360 px scene-tab strip.
        constexpr float audioOffsetX = 237.0f;
        constexpr float audioWidth = 61.5f;
        return XMFLOAT4(
            viewport.x + audioOffsetX,
            viewport.y + 10.0f,
            audioWidth,
            28.0f);
    }

    bool RenegadePhysicsLabStudioChrome::AudioViewportToolHit(
        const XMFLOAT4& pointer) const noexcept
    {
        const XMFLOAT4 bounds = AudioViewportToolBounds();
        return pointer.x >= bounds.x && pointer.x < bounds.x + bounds.z &&
            pointer.y >= bounds.y && pointer.y < bounds.y + bounds.w;
    }

    XMFLOAT4 RenegadePhysicsLabStudioChrome::AudioInspectorBounds() const noexcept
    {
        const XMFLOAT4 viewport = ViewportBounds();

        // ViewportBounds is a rectangle expressed as left/top/right/bottom,
        // not x/y/width/height. The Inspector begins at the viewport's right
        // edge and extends down only to the viewport's bottom edge.
        return XMFLOAT4(
            viewport.z,
            viewport.y,
            InspectorWidth(),
            std::max(0.0f, viewport.w - viewport.y));
    }

    bool RenegadePhysicsLabStudioChrome::AddMenuLabelHit(
        const XMFLOAT4& pointer) const noexcept
    {
        // Mirrors the existing base chrome's menu-position arithmetic:
        // menuPositions[ADD] = 336, with its hitbox extending 8 px left.
        return pointer.x >= AddPopupX && pointer.x < AddMenuLabelRight &&
            pointer.y >= 0.0f && pointer.y < TopBarHeight;
    }

    XMFLOAT4 RenegadePhysicsLabStudioChrome::ParticleAddMenuItemBounds() const noexcept
    {
        return XMFLOAT4(
            AddPopupX,
            TopBarHeight + ExistingAddItemCount * AddItemHeight,
            AddPopupWidth,
            AddItemHeight);
    }

    bool RenegadePhysicsLabStudioChrome::ParticleAddMenuItemHit(
        const XMFLOAT4& pointer) const noexcept
    {
        if (!particleAddMenuOpen_)
            return false;
        const XMFLOAT4 bounds = ParticleAddMenuItemBounds();
        return pointer.x >= bounds.x && pointer.x < bounds.x + bounds.z &&
            pointer.y >= bounds.y && pointer.y < bounds.y + bounds.w;
    }

    XMFLOAT4 RenegadePhysicsLabStudioChrome::NavigationAddMenuItemBounds() const noexcept
    {
        return XMFLOAT4(
            AddPopupX,
            TopBarHeight + (ExistingAddItemCount + 1) * AddItemHeight,
            AddPopupWidth,
            AddItemHeight);
    }

    bool RenegadePhysicsLabStudioChrome::NavigationAddMenuItemHit(
        const XMFLOAT4& pointer) const noexcept
    {
        if (!particleAddMenuOpen_)
            return false;
        const XMFLOAT4 bounds = NavigationAddMenuItemBounds();
        return pointer.x >= bounds.x && pointer.x < bounds.x + bounds.z &&
            pointer.y >= bounds.y && pointer.y < bounds.y + bounds.w;
    }

    bool RenegadePhysicsLabStudioChrome::ConsumedPointerThisFrame() const noexcept
    {
        return physicsTabConsumed_ || audioToolConsumed_ ||
            particleMenuConsumed_ || navigationMenuConsumed_ ||
            physicsLab_.ConsumedPointerThisFrame() ||
            particleWorkspace_.ConsumedPointerThisFrame() ||
            navigationWorkspace_.ConsumedPointerThisFrame() ||
            audioWorkspace_.ConsumedPointerThisFrame() ||
            CreatorAssetStudioChrome::ConsumedPointerThisFrame();
    }

    void RenegadePhysicsLabStudioChrome::Update(
        const wi::Canvas& canvas,
        const float dt)
    {
        physicsTabConsumed_ = false;
        audioToolConsumed_ = false;
        particleMenuConsumed_ = false;
        navigationMenuConsumed_ = false;
        workspaceTransitionRequested_ = false;

        const XMFLOAT4 pointerBeforeBase = wi::input::GetPointer();
        const bool leftPressed = wi::input::Press(wi::input::MOUSE_BUTTON_LEFT);
        const bool addMenuPressed = leftPressed && AddMenuLabelHit(pointerBeforeBase);
        if (addMenuPressed)
        {
            particleAddMenuOpen_ = !particleAddMenuOpen_;
        }
        if (wi::input::Press(wi::input::KEYBOARD_BUTTON_ESCAPE))
        {
            particleAddMenuOpen_ = false;
        }

        bool skipBaseUpdate = false;
        if (leftPressed && ParticleAddMenuItemHit(pointerBeforeBase))
        {
            // The native ADD popup owns nine rows. This tenth row is drawn and
            // handled by the bounded particle wrapper. Close the base popup and
            // skip its update for this one frame so the click cannot leak into
            // the viewport beneath the extended popup.
            particleMenuConsumed_ = true;
            particleAddMenuOpen_ = false;
            ResetDisclosureState();
            SetActiveBottomTab(-1, true);
            SetPhysicsLabActive(false);
            SetAudioWorkspaceActive(false);
            SetNavigationWorkspaceActive(false);
            if (studioAction_)
                studioAction_(Action::SceneWorkspace);
            SetParticleWorkspaceActive(true);
            particleWorkspace_.CreateEmitterInFrontOfCamera();
            skipBaseUpdate = true;
        }
        else if (leftPressed && NavigationAddMenuItemHit(pointerBeforeBase))
        {
            // Navigation extends the same ADD popup with an eleventh row. The
            // grid itself is a persistent native Wicked VoxelGrid entity.
            navigationMenuConsumed_ = true;
            particleAddMenuOpen_ = false;
            ResetDisclosureState();
            SetActiveBottomTab(-1, true);
            SetPhysicsLabActive(false);
            SetAudioWorkspaceActive(false);
            SetParticleWorkspaceActive(false);
            if (studioAction_)
                studioAction_(Action::SceneWorkspace);
            SetNavigationWorkspaceActive(true);
            navigationWorkspace_.CreateNavigationGridForScene();
            skipBaseUpdate = true;
        }
        else if (leftPressed && particleAddMenuOpen_ && !addMenuPressed)
        {
            // Native ADD rows or any click outside the ADD label close the
            // mirrored specialist-row state while the base chrome handles its
            // own popup selection normally.
            particleAddMenuOpen_ = false;
        }

        if (!skipBaseUpdate)
            CreatorAssetStudioChrome::Update(canvas, dt);

        const XMFLOAT4 pointer = wi::input::GetPointer();
        if (PhysicsTabHit(pointer) &&
            wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {
            physicsTabConsumed_ = true;
            particleAddMenuOpen_ = false;
            if (studioAction_)
                studioAction_(Action::SceneWorkspace);
            SetPhysicsLabActive(true);
        }
        else if (!HasOpenMenuPopup() &&
            AudioViewportToolHit(pointer) &&
            wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {
            audioToolConsumed_ = true;
            particleAddMenuOpen_ = false;
            if (studioAction_)
                studioAction_(Action::SceneWorkspace);
            SetAudioWorkspaceActive(true);
        }

        // Selecting a native sound/emitter/navigation entity from the hierarchy
        // routes directly to the matching specialist Inspector.
        if (!workspaceTransitionRequested_ && IsSceneWorkspaceActive() &&
            !physicsLab_.IsActive())
        {
            auto* session = bridge::StudioSession::Current();
            if (session != nullptr && session->Selection().HasSelection())
            {
                auto& scene = session->Scenes().GetScene();
                const auto selected = session->Selection().SelectedEntity();
                if (bridge::IsParticleEmitter(scene, selected))
                {
                    if (!particleWorkspace_.IsActive())
                    {
                        if (studioAction_)
                            studioAction_(Action::SceneWorkspace);
                        SetParticleWorkspaceActive(true);
                    }
                }
                else if (bridge::IsRenegadeNavigationEntity(scene, selected))
                {
                    if (!navigationWorkspace_.IsActive())
                    {
                        if (studioAction_)
                            studioAction_(Action::SceneWorkspace);
                        SetNavigationWorkspaceActive(true);
                    }
                }
                else if (bridge::IsRenegadeSoundSource(scene, selected))
                {
                    if (!audioWorkspace_.IsActive())
                    {
                        if (studioAction_)
                            studioAction_(Action::SceneWorkspace);
                        SetAudioWorkspaceActive(true);
                    }
                }
                else
                {
                    if (particleWorkspace_.IsActive())
                        SetParticleWorkspaceActive(false);
                    if (navigationWorkspace_.IsActive())
                        SetNavigationWorkspaceActive(false);
                }
            }
            else
            {
                if (particleWorkspace_.IsActive())
                    SetParticleWorkspaceActive(false);
                if (navigationWorkspace_.IsActive())
                    SetNavigationWorkspaceActive(false);
            }
        }

        SynchronizeRigidBodyOwnerSelection();

        // Do not call SetBounds here. Specialist workspaces own stateful
        // widgets; relayout during Update can cancel click/slider state.
        physicsLab_.Update(canvas, dt);
        particleWorkspace_.Update(canvas, dt);
        navigationWorkspace_.Update(canvas, dt);
    }

    void RenegadePhysicsLabStudioChrome::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        CreatorAssetStudioChrome::Render(canvas, cmd);
        RenderAudioViewportTool(cmd);
        RenderPhysicsTab(cmd);
        RenderParticleAddMenuItem(cmd);
        RenderNavigationAddMenuItem(cmd);
        physicsLab_.Render(canvas, cmd);
        particleWorkspace_.Render(canvas, cmd);
        navigationWorkspace_.Render(canvas, cmd);
    }

    void RenegadePhysicsLabStudioChrome::RenderParticleAddMenuItem(
        const wi::graphics::CommandList cmd) const
    {
        if (!particleAddMenuOpen_)
            return;

        const XMFLOAT4 bounds = ParticleAddMenuItemBounds();
        const XMFLOAT4 pointer = wi::input::GetPointer();
        const bool hovered = ParticleAddMenuItemHit(pointer);
        DrawBorderedRect(
            bounds.x,
            bounds.y,
            bounds.z,
            bounds.w,
            hovered ? wi::Color(28, 20, 16, 255) : Surface2,
            hovered ? Forge : Border,
            cmd);
        DrawText(
            "PARTICLE EMITTER",
            bounds.x + 12.0f,
            bounds.y + 9.0f,
            hovered ? TextStrong : TextSecondary,
            cmd);
    }

    void RenegadePhysicsLabStudioChrome::RenderNavigationAddMenuItem(
        const wi::graphics::CommandList cmd) const
    {
        if (!particleAddMenuOpen_)
            return;

        const XMFLOAT4 bounds = NavigationAddMenuItemBounds();
        const XMFLOAT4 pointer = wi::input::GetPointer();
        const bool hovered = NavigationAddMenuItemHit(pointer);
        DrawBorderedRect(
            bounds.x,
            bounds.y,
            bounds.z,
            bounds.w,
            hovered ? wi::Color(28, 20, 16, 255) : Surface2,
            hovered ? Forge : Border,
            cmd);
        DrawText(
            "NAVIGATION GRID",
            bounds.x + 12.0f,
            bounds.y + 9.0f,
            hovered ? TextStrong : TextSecondary,
            cmd);
    }

    void RenegadePhysicsLabStudioChrome::RenderPhysicsTab(
        const wi::graphics::CommandList cmd) const
    {
        const float sceneMetaX = LayoutWidth() - 360.0f;
        const bool active = physicsLab_.IsActive();
        DrawText(
            "PHYSICS",
            sceneMetaX + 294.0f,
            38.0f,
            active ? TextStrong : TextSecondary,
            cmd);

        if (!active)
            return;

        DrawRect(sceneMetaX + 12.0f, 56.0f, 344.0f, 4.0f, Surface0, cmd);
        DrawRect(sceneMetaX + 292.0f, 57.0f, 64.0f, 2.0f, Forge, cmd);
    }

    void RenegadePhysicsLabStudioChrome::RenderAudioViewportTool(
        const wi::graphics::CommandList cmd) const
    {
        if (HasOpenMenuPopup())
            return;
        const XMFLOAT4 bounds = AudioViewportToolBounds();
        const bool active = audioWorkspace_.IsActive();
        DrawBorderedRect(
            bounds.x,
            bounds.y,
            bounds.z,
            bounds.w,
            Surface0,
            active ? Forge : Border,
            cmd);
        DrawText(
            "AUDIO",
            bounds.x + 10.0f,
            bounds.y + 8.0f,
            active ? TextStrong : TextSecondary,
            cmd);
    }
}