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
        // has let the engine initialize its one Lua VM on the main thread.
        if (auto* session = bridge::StudioSession::Current(); session != nullptr)
            (void)bridge::BindPhysicsLua(session->Scenes().GetScene());

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
        // frame would reset the stateful GUI controls during interaction.
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
                    action == Action::TerrainWorkspace ||
                    action == Action::RenderWorkspace)
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
            if (studioAction_)
                studioAction_(Action::SceneWorkspace);
            SetPhysicsLabActive(true);
        }

        SynchronizeRigidBodyOwnerSelection();

        // Do not call SetBounds here. Physics Lab controls are stateful widgets;
        // relayout during Update cancels click and slider state mid-interaction.
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

        DrawRect(sceneMetaX + 12.0f, 56.0f, 344.0f, 4.0f, Surface0, cmd);
        DrawRect(sceneMetaX + 292.0f, 57.0f, 64.0f, 2.0f, Forge, cmd);
    }
}
