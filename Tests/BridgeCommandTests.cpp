#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/SceneService.h"
#include "renegade/bridge/SelectionService.h"

namespace
{
    bool NearlyEqual(const float left, const float right)
    {
        return std::abs(left - right) < 0.0001f;
    }

    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return 1;
    }
}

int main()
{
    renegade::bridge::SceneService scenes;
    renegade::bridge::SelectionService selection;
    renegade::bridge::CommandService commands;

    const auto parent = wi::ecs::CreateEntity();
    scenes.GetScene().names.Create(parent) = "Parent";
    scenes.GetScene().transforms.Create(parent);

    const auto child = wi::ecs::CreateEntity();
    scenes.GetScene().names.Create(child) = "Child";
    auto& childTransform = scenes.GetScene().transforms.Create(child);
    scenes.GetScene().Component_Attach(child, parent);

    const auto hierarchy = scenes.ListEntities();
    if (hierarchy.size() != 2 || hierarchy[0].entity != parent ||
        hierarchy[1].entity != child || hierarchy[1].depth != 1)
    {
        return Fail("hierarchy listing did not preserve parent/child order");
    }

    selection.Select(child);
    if (!selection.HasSelection() || selection.SelectedEntity() != child)
    {
        return Fail("selection service did not retain the selected entity");
    }

    if (!commands.Execute(std::make_unique<renegade::bridge::SetTranslationCommand>(
            scenes.GetScene(),
            child,
            XMFLOAT3(4.0f, 5.0f, 6.0f))))
    {
        return Fail("transform command did not execute");
    }

    if (!NearlyEqual(childTransform.translation_local.x, 4.0f) ||
        !commands.CanUndo() || commands.CanRedo())
    {
        return Fail("execute state is incorrect");
    }

    if (!commands.Undo() ||
        !NearlyEqual(childTransform.translation_local.x, 0.0f) ||
        !commands.CanRedo())
    {
        return Fail("undo did not restore the original transform");
    }

    if (!commands.Redo() ||
        !NearlyEqual(childTransform.translation_local.x, 4.0f) ||
        commands.CanRedo())
    {
        return Fail("redo did not restore the edited transform");
    }

    const auto savePath =
        std::filesystem::temp_directory_path() /
        ("renegade-persistence-" + std::to_string(child) + ".wiscene");
    std::filesystem::remove(savePath);

    if (!scenes.SaveScene(savePath.string()))
    {
        return Fail("scene service did not save the edited scene");
    }

    renegade::bridge::SceneService reopened;
    if (!reopened.LoadScene(savePath.string()))
    {
        std::filesystem::remove(savePath);
        return Fail("scene service did not reopen the saved scene");
    }

    bool persisted = false;
    for (const auto& entity : reopened.ListEntities())
    {
        if (entity.name != "Child")
        {
            continue;
        }
        const auto* transform =
            reopened.GetScene().transforms.GetComponent(entity.entity);
        persisted =
            transform != nullptr &&
            NearlyEqual(transform->translation_local.x, 4.0f) &&
            NearlyEqual(transform->translation_local.y, 5.0f) &&
            NearlyEqual(transform->translation_local.z, 6.0f);
        break;
    }
    std::filesystem::remove(savePath);

    if (!persisted)
    {
        return Fail("saved transform did not survive reopen");
    }

    std::cout
        << "PASS: hierarchy, selection, transform command, undo, redo, save and reopen\n";
    return 0;
}
