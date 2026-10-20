#include "renegade/bridge/ScreenAuthoringSession.h"
#include "../Runtime/src/RuntimeScreen.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace fs = std::filesystem;
using namespace renegade::bridge;
static std::string Bytes(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
static int Fail(const char* message, const std::string& error)
{
    std::cerr << message << ": " << error << '\n';
    return 1;
}
int main()
{
    const fs::path root = fs::temp_directory_path() / ("renegade-usage-" + GenerateStableId());
    struct Cleanup { fs::path p; ~Cleanup() { std::error_code e; fs::remove_all(p,e); } } cleanup{root};
    fs::create_directories(root / "Content/UI");
    const auto projectId = GenerateStableId();
    ScreenDocument document;
    document.envelope = CreateDocumentEnvelope(projectId, RuntimeScreenDocumentType,
        "Content/UI/Hud.renegade-screen", "Screen usage test");
    document.usage = ScreenUsage::Hud;
    std::string error;
    const auto path = root / "Content/UI/Hud.renegade-screen";
    if (!ValidateScreenDocument(document, projectId, error) ||
        !WriteScreenDocument(path.generic_u8string(), document, error))
        return Fail("Empty passive HUD rejected", error);
    renegade::runtime::RuntimeScreenController runtime;
    if (!runtime.Initialize(document, error) || runtime.HasFocus() ||
        runtime.FocusNext() || runtime.FocusPrevious())
        return Fail("Passive HUD invented focus/input", error);

    ScreenAuthoringSession session;
    if (!session.Open(path.generic_u8string(), projectId, error))
        return Fail("HUD authoring open failed", error);
    const auto openedRevision = session.Revision();
    if (openedRevision == 0) return Fail("Opening did not publish a document revision", error);
    if (session.UpdateScreenSettings(ScreenUsage::Menu, true, error) || session.IsDirty() ||
        session.Revision() != openedRevision)
        return Fail("Invalid HUD-to-menu conversion mutated history", error);
    if (!session.UpdateScreenSettings(ScreenUsage::Overlay, false, error) ||
        session.UndoCount() != 1 || !session.Save(error))
        return Fail("Usage change did not save as one command", error);
    const auto changedRevision = session.Revision();
    if (changedRevision <= openedRevision || !session.Save(error) ||
        session.Revision() != changedRevision)
        return Fail("Mutation/save revision contract failed", error);
    ScreenDocument reopened;
    if (!ReadScreenDocument(path.generic_u8string(), projectId, reopened, error) ||
        reopened.usage != ScreenUsage::Overlay || reopened.enabledInGame)
        return Fail("Usage/availability did not round-trip", error);
    if (runtime.Initialize(reopened, error))
        return Fail("Disabled Screen was instantiated", error);
    if (!session.Undo(error) || session.Document().usage != ScreenUsage::Hud ||
        !session.Document().enabledInGame || !session.Redo(error))
        return Fail("Usage Undo/Redo lost state", error);

    if (session.Revision() <= changedRevision)
        return Fail("Undo/Redo did not publish document revisions", error);
    const auto beforeClear = session.Revision();
    session.Clear();
    if (session.Revision() <= beforeClear || session.IsLoaded())
        return Fail("Clear did not invalidate document observers", error);

    // Existing schema-v2 documents retain their schema and exact bytes on open/save.
    ScreenDocument menu;
    menu.schemaVersion = 2;
    menu.envelope = CreateDocumentEnvelope(projectId, RuntimeScreenDocumentType,
        "Content/UI/Menu.renegade-screen", "Legacy compatibility test");
    ScreenWidget button;
    button.id = GenerateStableId();
    button.name = "Continue";
    button.kind = ScreenWidgetKind::Button;
    button.rect = {100,100,200,60};
    button.text = "Continue";
    button.actionId = "continue";
    button.style = MakeScreenWidgetStyleTemplate(button.kind,60);
    menu.widgets = {button};
    menu.actions = {{"continue"}};
    menu.focusOrder = {button.id};
    const auto legacyPath = root / "Content/UI/Menu.renegade-screen";
    if (!WriteScreenDocument(legacyPath.generic_u8string(),menu,error))
        return Fail("Legacy fixture could not be written",error);
    const auto original = Bytes(legacyPath);
    if (!session.Open(legacyPath.generic_u8string(),projectId,error) ||
        session.Document().schemaVersion != 2 || session.Document().usage != ScreenUsage::Menu ||
        Bytes(legacyPath) != original || !session.Save(error) || Bytes(legacyPath) != original)
        return Fail("Legacy Screen silently upgraded or changed on reopen",error);
    if (!session.UpdateScreenSettings(ScreenUsage::Hud,true,error) ||
        session.Document().schemaVersion != 3 || !session.Save(error) ||
        !ReadScreenDocument(legacyPath.generic_u8string(),projectId,reopened,error) ||
        reopened.usage != ScreenUsage::Hud || reopened.schemaVersion != 3)
        return Fail("Explicit usage upgrade did not persist",error);

    if (Bytes(fs::u8path(legacyPath.generic_u8string() + ".pre-ui-schema2")) != original)
        return Fail("Upgrade did not retain a byte-exact reopenable original",error);

    // A creator can undo an upgrade, edit the legacy menu, and upgrade again.
    // Both original revisions must survive; the first archive is never replaced.
    menu.widgets.front().text = "Changed legacy revision";
    if (!WriteScreenDocument(legacyPath.generic_u8string(), menu, error) ||
        !session.Open(legacyPath.generic_u8string(),projectId,error) ||
        !session.UpdateScreenSettings(ScreenUsage::Hud,true,error) ||
        !session.Save(error) ||
        Bytes(fs::u8path(legacyPath.generic_u8string() + ".pre-ui-schema2")) != original)
        return Fail("A second explicit upgrade lost or overwrote the first original",error);
    std::size_t originals = 0;
    for (const auto& entry : fs::directory_iterator(legacyPath.parent_path()))
        if (entry.path().filename().generic_u8string().find("Menu.renegade-screen.pre-ui-schema2") == 0)
            ++originals;
    if (originals != 2) return Fail("Distinct legacy revisions did not retain distinct originals",error);

    auto corrupt = Bytes(path);
    const auto usage = corrupt.find("usage = overlay");
    if (usage == std::string::npos) return Fail("Fixture has no explicit usage",error);
    corrupt.replace(usage,15,"usage = invalid");
    { std::ofstream out(path,std::ios::binary); out << corrupt; }
    if (ReadScreenDocument(path.generic_u8string(),projectId,reopened,error))
        return Fail("Unknown usage was silently accepted",error);
    const auto schema = corrupt.find("schema_version = 3");
    if (schema == std::string::npos) return Fail("Fixture has no explicit schema",error);
    corrupt.replace(schema,18,"schema_version = 4");
    { std::ofstream out(path,std::ios::binary); out << corrupt; }
    if (WriteScreenDocument(path.generic_u8string(),document,error) || Bytes(path) != corrupt)
        return Fail("An older writer overwrote an unknown future Screen schema",error);
    std::cout << "PASS: passive HUD, legacy menu parity, explicit upgrade, history and disabled Runtime policy\n";
    return 0;
}
