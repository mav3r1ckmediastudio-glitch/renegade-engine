#include "renegade/screen/ScreenRenderer.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    bool Near(const float left, const float right)
    {
        return std::abs(left - right) < 0.001f;
    }

    int Fail(const char* message)
    {
        std::cerr << "RenegadeScreenRendererTests: " << message << '\n';
        return 1;
    }
}

int main()
{
    using namespace renegade;

    screen::ScreenRenderer inputModeProbe;
    if (!inputModeProbe.IsInputEnabled())
        return Fail("Screen renderer Runtime input must default enabled");
    inputModeProbe.SetInputEnabled(false);
    if (inputModeProbe.IsInputEnabled())
        return Fail("Screen renderer authoring input suppression did not latch");
    inputModeProbe.SetInputEnabled(true);
    if (!inputModeProbe.IsInputEnabled())
        return Fail("Screen renderer input could not be restored for Runtime use");

    bridge::ScreenDocument document;
    document.envelope = bridge::CreateDocumentEnvelope(
        bridge::GenerateStableId(),
        bridge::RuntimeScreenDocumentType,
        "Content/UI/Renderer.renegade-screen",
        "Gate 8B shared renderer proof");
    document.designWidth = 1280.0f;
    document.designHeight = 720.0f;

    bridge::ScreenWidget background;
    background.id = bridge::GenerateStableId();
    background.kind = bridge::ScreenWidgetKind::Image;
    background.name = "Background";
    background.rect = {0.0f, 0.0f, 1280.0f, 720.0f};
    background.enabled = false;
    background.resourcePath = "Content/UI/background.png";
    background.style = bridge::MakeScreenWidgetStyleTemplate(
        background.kind, background.rect.height);

    bridge::ScreenWidget button;
    button.id = bridge::GenerateStableId();
    button.kind = bridge::ScreenWidgetKind::Button;
    button.name = "Continue";
    button.rect = {440.0f, 320.0f, 400.0f, 80.0f};
    button.text = "CONTINUE";
    button.actionId = "continue";
    button.style = bridge::MakeScreenWidgetStyleTemplate(
        button.kind, button.rect.height);
    button.style.borderColor = {10, 20, 30, 255};
    button.style.borderWidth = 2.0f;
    button.style.cornerRadius = 12.0f;
    button.style.text.fontSize = 24.0f;
    button.style.text.characterSpacing = 2.0f;
    button.style.text.lineSpacing = 4.0f;
    button.style.text.shadowOffsetX = 3.0f;
    button.style.text.shadowOffsetY = 5.0f;
    button.style.normal.background = {1, 2, 3, 255};
    button.style.hover.background = {4, 5, 6, 255};
    button.style.pressed.background = {7, 8, 9, 255};
    button.style.focused.background = {10, 11, 12, 255};
    button.style.disabled.background = {13, 14, 15, 255};

    document.actions = {{"continue"}};
    document.widgets = {background, button};
    document.focusOrder = {button.id};

    std::unordered_map<bridge::StableId, screen::ScreenInteractionState> states;
    states.emplace(button.id, screen::ScreenInteractionState::Focused);
    std::vector<screen::ScreenRenderItem> items;
    std::string error;
    if (!screen::BuildScreenRenderItems(
            document, 1920.0f, 1080.0f, states, items, error))
        return Fail("valid render frame was rejected");
    if (items.size() != 2 || items[0].widgetId != background.id ||
        items[1].widgetId != button.id)
        return Fail("authored back-to-front order was not preserved");

    const auto& rendered = items[1];
    if (!Near(rendered.logicalRect.x, 660.0f) ||
        !Near(rendered.logicalRect.y, 480.0f) ||
        !Near(rendered.logicalRect.width, 600.0f) ||
        !Near(rendered.logicalRect.height, 120.0f) ||
        !Near(rendered.scaleX, 1.5f) || !Near(rendered.scaleY, 1.5f) ||
        !Near(rendered.borderWidth, 3.0f) ||
        !Near(rendered.cornerRadius, 18.0f) ||
        !Near(rendered.fontSize, 36.0f) ||
        !Near(rendered.characterSpacing, 3.0f) ||
        !Near(rendered.lineSpacing, 6.0f) ||
        !Near(rendered.shadowOffsetX, 4.5f) ||
        !Near(rendered.shadowOffsetY, 7.5f))
        return Fail("geometry, border or typography did not share one scale");
    if (rendered.interaction != screen::ScreenInteractionState::Focused ||
        rendered.visual.background.red != 10)
        return Fail("focused visual state was not selected");

    const bridge::ScreenRect editorViewport{300.0f, 200.0f, 960.0f, 540.0f};
    if (!screen::BuildScreenRenderItemsInViewport(
            document, editorViewport, states, items, error))
        return Fail("valid Screen Editor preview viewport was rejected");
    const auto& previewButton = items[1];
    if (!Near(previewButton.logicalRect.x, 630.0f) ||
        !Near(previewButton.logicalRect.y, 440.0f) ||
        !Near(previewButton.logicalRect.width, 300.0f) ||
        !Near(previewButton.logicalRect.height, 60.0f) ||
        !Near(previewButton.scaleX, 0.75f) ||
        !Near(previewButton.scaleY, 0.75f))
        return Fail("Screen Editor viewport diverged from shared render geometry");

    const bridge::ScreenRect invalidViewport{0.0f, 0.0f, 0.0f, 540.0f};
    if (screen::BuildScreenRenderItemsInViewport(
            document, invalidViewport, states, items, error))
        return Fail("invalid Screen Editor preview viewport was accepted");

    states[button.id] = screen::ScreenInteractionState::Pressed;
    if (!screen::BuildScreenRenderItems(
            document, 1280.0f, 720.0f, states, items, error) ||
        items[1].visual.background.red != 7)
        return Fail("pressed visual state was not selected");

    document.widgets[1].enabled = false;
    states[button.id] = screen::ScreenInteractionState::Hover;
    if (!screen::BuildScreenRenderItems(
            document, 1280.0f, 720.0f, states, items, error) ||
        items[1].interaction != screen::ScreenInteractionState::Disabled ||
        items[1].visual.background.red != 13)
        return Fail("disabled state did not override pointer state exactly");
    if (items[0].interaction == screen::ScreenInteractionState::Disabled)
        return Fail("non-interactive image was incorrectly faded as disabled");

    bridge::ScreenDocument packagedFixture;
    const std::filesystem::path fixturePath =
        std::filesystem::path(RENEGADE_SOURCE_DIR) /
        "Runtime/fixtures/LP03/Valid Screen/Content/UI/Main.renegade-screen";
    if (!bridge::ReadScreenDocument(
            fixturePath.generic_u8string(),
            "61111111-1111-4111-8111-111111111111",
            packagedFixture,
            error))
        return Fail("packaged visual fixture is not a valid schema-v2 Screen");
    if (packagedFixture.schemaVersion !=
            bridge::ScreenDocument::CurrentSchemaVersion ||
        packagedFixture.widgets.size() != 4 ||
        packagedFixture.widgets[1].style.borderWidth <= 0.0f ||
        packagedFixture.widgets[2].style.borderWidth <= 0.0f ||
        packagedFixture.widgets[2].style.focused.background.red ==
            packagedFixture.widgets[2].style.hover.background.red)
        return Fail("packaged fixture does not visibly prove Gate 8B fidelity");

    std::cout << "PASS: Gate 8B shared Screen render contract\n";
    return 0;
}
