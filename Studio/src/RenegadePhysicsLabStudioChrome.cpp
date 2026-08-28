#include "RenegadePhysicsLabStudioChrome.h"

#include "renegade/bridge/CollisionService.h"
#include "renegade/bridge/PhysicsLuaService.h"
#include "renegade/bridge/StudioSession.h"

#include <algorithm>
#include <utility>

namespace
{
    constexpr wi::Color Surface0 = wi::Color(8, 12, 16, 255);
    constexpr wi::Color TextSecondary = wi::Color(226, 226, 226, 255);
    constexpr wi::Color TextStrong = wi::Color(244, 244, 244, 255);
    constexpr wi::Color Forge = wi::Color(210, 91, 29, 255);

    void DrawRect(
        const float x,
        const float y,
        const float width,
        const float height,
        const wi::Color color,
        const wi::graphics::CommandList cmd)
    {
        wi::image::Params params(x, y, width, height, color);
        params.blendFlag = wi::enums::BLENDMODE_ALPHA;
        wi::image::Draw(nullptr, params, cmd);
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
        // has let Wicked initialize its one Lua VM on the main thread. JP01
        // installs the Renegade namespace into that existing VM; it never
        // bootstraps Lua from SceneService construction.
        if (auto* session = bridge::StudioSession::Current(); session != nullptr)
        {
            (void)bridge::BindPhysicsLua(session->Scenes().GetScene());
        }

        physicsLab_.Create();
        physicsLab_.SetBounds(ViewportBounds());
        physicsLab_.SetActive(false);
    }

    void RenegadePhysicsLabStudioChrome::SetLayout(
        const float width,
        const float height)
    {
        CreatorAssetStudioChrome::SetLayout(width, height);
        // Bounds only change when Studio layout changes. Relaying out every
        // frame would hide/show every Physics Lab widget and Wicked resets a
        // widget to IDLE when SetVisible(false) is called, destroying active
        // mouse-down/drag state before a click or slider drag can complete.
        physicsLab_.SetBounds(ViewportBounds());
    }

    void RenegadePhysicsLabStudioChrome::OnAction(
        std::function<void(Action)> callback)
    {
        studioAction_ = std::move(callback);
        CreatorAssetStudioChrome::OnAction(
            [this](const Action action)
            {
                if (action == Action::SceneWorkspace ||
                    action == Action::EnvironmentWorkspace ||
                    action == Action::TerrainWorkspace)
                {
                    SetPhysicsLabActive(false);
                }
                if (studioAction_)
                    studioAction_(action);
            });
    }

    void RenegadePhysicsLabStudioChrome::RequestCurrentWorkspaceReconcile()
    {
        if (!physicsLab_.IsActive())
        {
            RenegadeStudioChrome::RequestCurrentWorkspaceReconcile();
            return;
        }

        // Physics Lab overlays the Scene viewport while sharing the Scene
        // hierarchy and command history. Reconcile the underlying Scene mode
        // without closing the Lab when importer/workspace lifecycle asks for it.
        if (studioAction_)
            studioAction_(Action::SceneWorkspace);
        physicsLab_.SetBounds(ViewportBounds());
    }

    void RenegadePhysicsLabStudioChrome::SetPhysicsLabActive(const bool active)
    {
        physicsLab_.SetActive(active);
        physicsLab_.SetBounds(ViewportBounds());
        if (active)
            SetStatusText("PHYSICS LAB");
    }

    bool RenegadePhysicsLabStudioChrome::PhysicsTabHit(
        const XMFLOAT4& pointer) const noexcept
    {
        const float sceneMetaX = LayoutWidth() - 360.0f;
        return pointer.x >= sceneMetaX + 292.0f &&
            pointer.x < sceneMetaX + 356.0f &&
            pointer.y >= 34.0f && pointer.y < 58.0f;
    }

    bool RenegadePhysicsLabStudioChrome::ConsumedPointerThisFrame() const noexcept
    {
        return physicsTabConsumed_ ||
            physicsLab_.ConsumedPointerThisFrame() ||
            CreatorAssetStudioChrome::ConsumedPointerThisFrame();
    }

    void RenegadePhysicsLabStudioChrome::Update(
        const wi::Canvas& canvas,
        const float dt)
    {
        physicsTabConsumed_ = false;
        CreatorAssetStudioChrome::Update(canvas, dt);

        const XMFLOAT4 pointer = wi::input::GetPointer();
        if (PhysicsTabHit(pointer) &&
            wi::input::Press(wi::input::MOUSE_BUTTON_LEFT))
        {
            physicsTabConsumed_ = true;
            // Environment/Terrain are semantic modes in StudioRenderPath. Put
            // the underlying editor in Scene mode first, then keep Physics Lab
            // as the Renegade-owned overlay on that same authoritative Scene.
            if (studioAction_)
                studioAction_(Action::SceneWorkspace);
            SetPhysicsLabActive(true);
        }

        if (physicsLab_.IsActive())
        {
            // Creator-facing reusable assets own physics on their stable
            // instance wrapper, never on a replaceable imported payload node.
            // CreateCollisionCommand resolves a nested Add request to that
            // wrapper. Follow the selection to the real body owner before the
            // Lab refreshes so the controls immediately edit what was created.
            if (auto* session = bridge::StudioSession::Current();
                session != nullptr && session->Selection().HasSelection())
            {
                auto& scene = session->Scenes().GetScene();
                const wi::ecs::Entity selected =
                    session->Selection().SelectedEntity();
                const wi::ecs::Entity target =
                    bridge::ResolveCollisionAuthoringTarget(scene, selected);
                if (target != selected &&
                    target != wi::ecs::INVALID_ENTITY &&
                    !scene.rigidbodies.Contains(selected) &&
                    scene.rigidbodies.Contains(target))
                {
                    session->Selection().Select(target);
                    SetStatusText(
                        "PHYSICS LAB // ASSET ROOT SELECTED");
                }
            }
        }

        // Do not call SetBounds here. Physics Lab controls are stateful Wicked
        // widgets: SetBounds -> Layout -> HideAll toggles visibility and resets
        // ACTIVE/DEACTIVATING every frame, which makes every control appear
        // correctly while preventing buttons, sliders and combos from ever
        // completing an interaction. SetLayout/activation own relayout instead.
        physicsLab_.Update(canvas, dt);
    }

    void RenegadePhysicsLabStudioChrome::Render(
        const wi::Canvas& canvas,
        const wi::graphics::CommandList cmd) const
    {
        CreatorAssetStudioChrome::Render(canvas, cmd);
        RenderPhysicsTab(cmd);
        physicsLab_.Render(canvas, cmd);
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

        // Base chrome sees Scene as active after we reconcile its semantic
        // mode. Cover only its 2px workspace underline, then draw Physics as
        // the visible Renegade workspace owner. No base-shell geometry moves.
        DrawRect(sceneMetaX + 12.0f, 56.0f, 344.0f, 4.0f, Surface0, cmd);
        DrawRect(sceneMetaX + 292.0f, 57.0f, 64.0f, 2.0f, Forge, cmd);
    }
}
