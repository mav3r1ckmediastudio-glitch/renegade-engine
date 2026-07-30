#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include "renegade/bridge/CommandService.h"
#include "renegade/bridge/ProjectService.h"
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

    struct TemporaryDirectory
    {
        std::filesystem::path path;

        ~TemporaryDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };
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

    commands.Clear();
    childTransform.translation_local = XMFLOAT3{};
    childTransform.SetDirty();
    childTransform.UpdateTransform();

    for (int step = 1; step <= 10; ++step)
    {
        if (!commands.Execute(
                std::make_unique<renegade::bridge::SetTranslationCommand>(
                    scenes.GetScene(),
                    child,
                    XMFLOAT3(static_cast<float>(step), 0.0f, 0.0f))))
        {
            return Fail("repeated transform command did not execute");
        }
    }

    if (commands.UndoCount() != 10 || commands.RedoCount() != 0)
    {
        return Fail("repeated transform history counts are incorrect");
    }

    for (int expected = 9; expected >= 0; --expected)
    {
        if (!commands.Undo() ||
            !NearlyEqual(
                childTransform.translation_local.x,
                static_cast<float>(expected)))
        {
            return Fail("repeated undo did not restore the expected transform");
        }
    }

    if (commands.CanUndo() || commands.UndoCount() != 0 ||
        commands.RedoCount() != 10)
    {
        return Fail("repeated undo history counts are incorrect");
    }

    for (int expected = 1; expected <= 10; ++expected)
    {
        if (!commands.Redo() ||
            !NearlyEqual(
                childTransform.translation_local.x,
                static_cast<float>(expected)))
        {
            return Fail("repeated redo did not restore the expected transform");
        }
    }

    if (commands.UndoCount() != 10 || commands.CanRedo())
    {
        return Fail("repeated redo history counts are incorrect");
    }

    const auto undoCountBeforeNoOp = commands.UndoCount();
    if (commands.Execute(
            std::make_unique<renegade::bridge::SetTranslationCommand>(
                scenes.GetScene(),
                child,
                XMFLOAT3(10.0f, 0.0f, 0.000001f))) ||
        commands.UndoCount() != undoCountBeforeNoOp)
    {
        return Fail("microscopic transform polluted the undo history");
    }

    TemporaryDirectory projectFixture;
    projectFixture.path =
        std::filesystem::temp_directory_path() /
        (
            "renegade-project-service-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto projectParent = projectFixture.path / "Projects";
    const auto templateScene = projectFixture.path / "ProvingGround.wiscene";
    std::filesystem::create_directories(projectParent);
    {
        std::ofstream templateFile(templateScene, std::ios::binary);
        templateFile << "Renegade project service scene fixture";
    }

    renegade::bridge::ProjectService projects;
    const auto stateFile = projectFixture.path / "State" / "Studio.ini";
    projects.Initialize(stateFile.generic_string());
    if (!projects.CreateProject(
            projectParent.generic_string(),
            "Test Project",
            templateScene.generic_string()))
    {
        return Fail("project service did not create a project");
    }

    const auto descriptor =
        projectParent / "Test Project" / "Test Project.renegade";
    const auto startupScene =
        projectParent / "Test Project" / "Content" / "Scenes" / "Main.wiscene";
    if (!projects.HasProject() ||
        projects.CurrentProject().name != "Test Project" ||
        projects.RecentProjects().size() != 1 ||
        !std::filesystem::is_regular_file(descriptor) ||
        !std::filesystem::is_regular_file(startupScene))
    {
        return Fail("created project structure or metadata is incorrect");
    }

    renegade::bridge::ProjectService reopenedProjects;
    reopenedProjects.Initialize(stateFile.generic_string());
    if (reopenedProjects.RecentProjects().size() != 1 ||
        !reopenedProjects.OpenProject(descriptor.generic_string()) ||
        reopenedProjects.StartupScenePath() !=
            std::filesystem::absolute(startupScene)
                .lexically_normal()
                .generic_string())
    {
        return Fail("project descriptor or recent-project state did not reopen");
    }

    std::cout
        << "PASS: hierarchy, selection, transform command, repeated undo/redo, "
           "no-op filtering, and project lifecycle\n";
    return 0;
}
