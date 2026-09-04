from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


inspector_path = Path("Studio/src/S4BScriptAttachmentInspector.cpp")
inspector = inspector_path.read_text(encoding="utf-8")

inspector = replace_once(
    inspector,
    "            std::size_t selectedSource = 0;\n            std::uint64_t sourceSceneRevision =\n",
    "            std::size_t selectedSource = 0;\n            std::string selectedSourcePath;\n            std::uint64_t sourceSceneRevision =\n",
    "selected source path state",
)

inspector = replace_once(
    inspector,
    "                        Role(presentation).selectedSource =\n                            static_cast<std::size_t>(args.userdata);\n",
    "                        auto& role = Role(presentation);\n                        role.selectedSource =\n                            static_cast<std::size_t>(args.userdata);\n                        if (role.selectedSource < role.sources.size())\n                            role.selectedSourcePath =\n                                role.sources[role.selectedSource].sourcePath;\n",
    "source selection callback",
)

inspector = replace_once(
    inspector,
    "                controls.addSource.SetTooltip(\n                    \"Choose a governed Content/Scripts Lua source whose S4A metadata role is \" +\n                    role + \".\");\n",
    "                controls.addSource.SetTooltip(\n                    \"Choose the next governed Content/Scripts Lua source to ADD. \"\n                    \"Already attached scripts are listed below. Required metadata role: \" +\n                    role + \".\");\n",
    "source chooser tooltip",
)

old_refresh_tail = '''                for (std::size_t index = 0; index < controls.sources.size(); ++index)\n                {\n                    const auto& source = controls.sources[index];\n                    std::string label = source.metadata.category.empty()\n                        ? source.metadata.name\n                        : source.metadata.category + " / " + source.metadata.name;\n                    controls.addSource.AddItem(\n                        label,\n                        static_cast<std::uint64_t>(index));\n                }\n                if (!controls.sources.empty())\n                    controls.addSource.SetSelectedWithoutCallback(0);\n'''
new_refresh_tail = '''                for (std::size_t index = 0; index < controls.sources.size(); ++index)\n                {\n                    const auto& source = controls.sources[index];\n                    std::string label = source.metadata.category.empty()\n                        ? source.metadata.name\n                        : source.metadata.category + " / " + source.metadata.name;\n                    controls.addSource.AddItem(\n                        label,\n                        static_cast<std::uint64_t>(index));\n                }\n                if (!controls.sources.empty())\n                {\n                    std::size_t restored = 0;\n                    if (!controls.selectedSourcePath.empty())\n                    {\n                        const auto selected = std::find_if(\n                            controls.sources.begin(), controls.sources.end(),\n                            [&](const ScriptAuthoringSource& candidate)\n                            {\n                                return candidate.sourcePath ==\n                                    controls.selectedSourcePath;\n                            });\n                        if (selected != controls.sources.end())\n                        {\n                            restored = static_cast<std::size_t>(\n                                std::distance(controls.sources.begin(), selected));\n                        }\n                    }\n                    controls.selectedSource = restored;\n                    controls.selectedSourcePath = controls.sources[restored].sourcePath;\n                    controls.addSource.SetSelectedWithoutCallback(\n                        static_cast<int>(restored));\n                }\n                else\n                {\n                    controls.selectedSource = 0;\n                    controls.selectedSourcePath.clear();\n                }\n'''
inspector = replace_once(
    inspector,
    old_refresh_tail,
    new_refresh_tail,
    "source refresh selection restoration",
)

inspector_path.write_text(inspector, encoding="utf-8")


test_path = Path("Tests/S4BScriptAuthoringTests.cpp")
test = test_path.read_text(encoding="utf-8")

test = replace_once(
    test,
    "    if (actionSource == nullptr || scriptSource == nullptr)\n    {\n        fs::remove_all(root, ec);\n        return 1;\n    }\n\n    StableId actionOne;\n",
    "    if (actionSource == nullptr || scriptSource == nullptr)\n    {\n        fs::remove_all(root, ec);\n        return 1;\n    }\n    const StableId scriptSourceId = scriptSource->binding.sourceId;\n\n    StableId actionOne;\n",
    "capture SCRIPT source authority",
)

test = replace_once(
    test,
    "    ok = Expect(\n        FindScriptAttachment(persisted, actionOne) != nullptr &&\n        FindScriptAttachment(persisted, actionTwo) != nullptr &&\n        FindScriptAttachment(persisted, scriptOne) != nullptr,\n        \"save preserves ScriptInstanceIds\") && ok;\n\n    ok = Expect(session.ReloadScene(), \"reopen saved Scene\") && ok;\n",
    "    ok = Expect(\n        FindScriptAttachment(persisted, actionOne) != nullptr &&\n        FindScriptAttachment(persisted, actionTwo) != nullptr &&\n        FindScriptAttachment(persisted, scriptOne) != nullptr,\n        \"save preserves ScriptInstanceIds\") && ok;\n    const ScriptAttachment* persistedScript =\n        FindScriptAttachment(persisted, scriptOne);\n    ok = Expect(\n        persistedScript != nullptr && persistedScript->sourcePath == scriptPath &&\n        persistedScript->sourceId == scriptSourceId,\n        \"saved companion preserves exact SCRIPT sourcePath/sourceId\") && ok;\n\n    ok = Expect(\n        session.LoadScene(scenePath),\n        \"Story Flow-style reopen saved Scene\") && ok;\n",
    "Story Flow style reopen",
)

test = replace_once(
    test,
    "    ok = Expect(\n        session.Scripts().EntityAttachments(\n            reopenedOwnerId, ScriptPresentation::Action).size() == 2 &&\n        session.Scripts().EntityAttachments(\n            reopenedOwnerId, ScriptPresentation::Script).size() == 1,\n        \"Scene reopen restores ACTION and SCRIPT attachments\") && ok;\n\n    actions.clear();\n",
    "    ok = Expect(\n        session.Scripts().EntityAttachments(\n            reopenedOwnerId, ScriptPresentation::Action).size() == 2 &&\n        session.Scripts().EntityAttachments(\n            reopenedOwnerId, ScriptPresentation::Script).size() == 1,\n        \"Scene reopen restores ACTION and SCRIPT attachments\") && ok;\n    const auto reopenedScripts = session.Scripts().EntityAttachments(\n        reopenedOwnerId, ScriptPresentation::Script);\n    ok = Expect(\n        reopenedScripts.size() == 1 &&\n        reopenedScripts[0]->sourcePath == scriptPath &&\n        reopenedScripts[0]->sourceId == scriptSourceId,\n        \"Story Flow-style reopen preserves exact SCRIPT sourcePath/sourceId\") && ok;\n\n    actions.clear();\n",
    "exact source reopen assertion",
)

test_path.write_text(test, encoding="utf-8")
