#include "renegade/bridge/ScreenBinding.h"
#include "renegade/bridge/ScreenAuthoringSession.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <limits>
using namespace renegade::bridge;

int main()
{
    int failures = 0;
    const auto check = [&](bool valid, const char* message)
    {
        if (!valid) { std::cerr << message << '\n'; ++failures; }
    };
    ScreenBinding binding;
    binding.source.attributeId = "health.current";
    binding.denominator.attributeId = "health.maximum";
    binding.operation = ScreenBindingOperation::Ratio;
    binding.property = ScreenBindingProperty::Fill;
    ScreenBindingSnapshot snapshot{41, 1, {
        {binding.source, 25.0, 7}, {binding.denominator, 100.0, 7}}};
    const auto evaluate = [&]() { return EvaluateScreenBinding(binding, snapshot, 41, 7, 0.0); };
    auto result = evaluate();
    check(result.resolved && std::get<double>(result.value) == .25, "Ratio did not resolve coherent health data");
    snapshot.values[0].value = 150.0;
    check(std::get<double>(evaluate().value) == 1.0, "Fill was not bounded");
    snapshot.values[0].value = -10.0;
    check(std::get<double>(evaluate().value) == 0.0, "Negative fill was not bounded");
    snapshot.values[0].value = 25.0;
    snapshot.values[1].value = 0.0;
    result = evaluate();
    check(!result.resolved && result.diagnostic == ScreenBindingDiagnostic::ZeroDenominator &&
        std::get<double>(result.value) == 0.0, "Zero maximum fabricated a healthy value");
    snapshot.values[1].value = 100.0;
    snapshot.session = 40;
    check(evaluate().diagnostic == ScreenBindingDiagnostic::StaleSession, "Previous session was accepted");
    snapshot.session = 41;
    snapshot.values[0].generation = 6;
    check(evaluate().diagnostic == ScreenBindingDiagnostic::StaleGeneration, "Replaced entity was accepted");
    snapshot.values[0].generation = 7;
    snapshot.values.push_back(snapshot.values.front());
    check(evaluate().diagnostic == ScreenBindingDiagnostic::DuplicateSource, "Ambiguous snapshot was accepted");
    snapshot.values.pop_back();
    snapshot.values[0].value = std::numeric_limits<double>::quiet_NaN();
    check(evaluate().diagnostic == ScreenBindingDiagnostic::NonFinite, "NaN was accepted");
    snapshot.values[0].value = true;
    check(evaluate().diagnostic == ScreenBindingDiagnostic::WrongType, "Type change was accepted");
    snapshot.values[0].value = 25.0;

    binding.property = ScreenBindingProperty::Text;
    binding.decimalPlaces = 2;
    binding.prefix = "Health ";
    binding.suffix = " fraction";
    result = evaluate();
    check(result.resolved && std::get<std::string>(result.value) == "Health 0.25 fraction",
        "Formatter was not bounded deterministic text");
    binding.operation = ScreenBindingOperation::AtMost;
    binding.property = ScreenBindingProperty::Visible;
    binding.comparison = 30.0;
    check(std::get<bool>(evaluate().value), "Visibility threshold did not resolve");
    binding.comparison = 10.0;
    check(!std::get<bool>(evaluate().value), "False visibility threshold did not resolve");
    binding.operation = ScreenBindingOperation::Equals;
    binding.comparison = false;
    check(evaluate().diagnostic == ScreenBindingDiagnostic::WrongType, "Equality coerced a numeric value to bool");
    snapshot.values[0].value = false;
    check(std::get<bool>(evaluate().value), "Typed equality failed");
    binding.operation = ScreenBindingOperation::Direct;
    binding.property = ScreenBindingProperty::Enabled;
    check(!std::get<bool>(evaluate().value), "Boolean source was not preserved");
    snapshot.values.clear();
    binding.missingPolicy = ScreenBindingMissingPolicy::Hide;
    result = evaluate();
    check(!result.resolved && result.hide && result.diagnostic == ScreenBindingDiagnostic::MissingSource,
        "Missing source hide policy failed");

    binding.source.scope = ScreenBindingScope::Character;
    binding.source.targetId = GenerateStableId();
    binding.denominator.scope = ScreenBindingScope::Character;
    binding.denominator.targetId = GenerateStableId();
    binding.operation = ScreenBindingOperation::Ratio;
    binding.property = ScreenBindingProperty::Fill;
    snapshot.values = {{binding.source, 10.0, 7}, {binding.denominator, 40.0, 9}};
    check(evaluate().diagnostic == ScreenBindingDiagnostic::StaleGeneration,
        "Cross-entity denominator guessed a generation");
    result = EvaluateScreenBinding(binding, snapshot, 41, 7, 0.0, 9);
    check(result.resolved && std::get<double>(result.value) == .25,
        "Explicit cross-entity generations did not resolve");
    binding.source.scope = ScreenBindingScope::ProjectAttribute;
    std::string error;
    check(!ValidateScreenBinding(binding, error), "Project attribute accepted an entity identity");
    binding.source.targetId.clear();
    snapshot.values = {{binding.source, 50.0, 0}};
    binding.operation = ScreenBindingOperation::Direct;
    result = evaluate();
    check(result.resolved && std::get<double>(result.value) == 1.0, "Gameplay-owned project attribute failed");
    binding.decimalPlaces = 7;
    check(evaluate().diagnostic == ScreenBindingDiagnostic::InvalidBinding, "Unbounded formatter was accepted");
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("renegade-binding-" + GenerateStableId());
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code e; fs::remove_all(path,e); } } cleanup{root};
    fs::create_directories(root / "Content/UI");
    const auto file = root / "Content/UI/Bindings.renegade-screen";
    const auto project = GenerateStableId();
    ScreenDocument document;
    document.schemaVersion = 2;
    document.envelope = CreateDocumentEnvelope(project, RuntimeScreenDocumentType,
        "Content/UI/Bindings.renegade-screen", "Binding persistence test");
    ScreenWidget button;
    button.id = GenerateStableId();
    button.kind = ScreenWidgetKind::Button;
    button.name = "Continue";
    button.text = "Continue";
    button.actionId = "continue";
    button.rect = {100,100,200,60};
    button.style = MakeScreenWidgetStyleTemplate(button.kind,60);
    document.widgets = {button};
    document.actions = {{"continue"}};
    document.focusOrder = {button.id};
    check(WriteScreenDocument(file.generic_u8string(), document, error), "Could not write old document fixture");
    ScreenAuthoringSession session;
    check(session.Open(file.generic_u8string(),project,error), "Could not open binding session");
    ScreenBinding textBinding;
    textBinding.source.attributeId = "health.current";
    textBinding.prefix = "Health ";
    textBinding.suffix = " HP";
    textBinding.decimalPlaces = 3;
    textBinding.comparison = 123456.789012345;
    const auto revision = session.Revision();
    check(session.UpdateWidgetBindings(button.id,{textBinding},error) &&
        session.Document().schemaVersion == 3 && session.UndoCount() == 1 &&
        session.Revision() > revision, "Bindings did not enter one explicit upgrade/history command");
    const auto boundRevision = session.Revision();
    check(!session.UpdateWidgetBindings(button.id,{textBinding,textBinding},error) &&
        session.Revision() == boundRevision && session.UndoCount() == 1,
        "Duplicate target bindings partially mutated the document");
    check(session.Save(error), "Binding save failed");
    check(fs::exists(file.generic_u8string()+".pre-ui-schema2"), "Binding upgrade lost original version");
    ScreenDocument reopened;
    check(ReadScreenDocument(file.generic_u8string(),project,reopened,error), "Bindings did not reopen");
    if (reopened.widgets.size() == 1 && reopened.widgets[0].bindings.size() == 1)
    {
        const auto& persisted = reopened.widgets[0].bindings[0];
        check(persisted.source == textBinding.source && persisted.prefix == textBinding.prefix &&
            persisted.suffix == textBinding.suffix && persisted.decimalPlaces == 3 &&
            persisted.comparison == textBinding.comparison, "Typed binding serialization lost precision or identity");
    }
    else check(false, "Reopen lost binding record");
    check(session.Undo(error) && session.Document().schemaVersion == 2 &&
        session.Document().widgets[0].bindings.empty() && session.Revision() > boundRevision,
        "Binding Undo did not restore the old document and notify observers");
    check(session.Redo(error) && session.Document().widgets[0].bindings.size() == 1,
        "Binding Redo failed");
    // A draft with an unresolved gameplay source persists safely, while malformed
    // section counts/types are rejected without inventing valid defaults.
    std::ifstream input(file, std::ios::binary);
    std::string bytes{std::istreambuf_iterator<char>(input), {}};
    input.close();
    const auto countPosition = bytes.find("binding_count");
    check(countPosition != std::string::npos, "Serialized binding count is absent");
    if (countPosition != std::string::npos)
    {
        auto damaged = bytes;
        const auto end = damaged.find('\n',countPosition);
        damaged.replace(countPosition,end-countPosition,"binding_count = invalid");
        std::ofstream output(file,std::ios::binary|std::ios::trunc);
        output << damaged;
        output.close();
        check(!ReadScreenDocument(file.generic_u8string(),project,reopened,error),
            "Malformed binding count silently became a default");
    }
    if (failures) return 1;
    std::cout << "PASS: typed binding evaluation, formatting, conditions, coherent snapshots and lifecycle guards\n";
}
