#include "renegade/bridge/DiagnosticService.h"

#include <iostream>

int main()
{
    renegade::bridge::DiagnosticService diagnostics(2);
    diagnostics.Record(renegade::bridge::DiagnosticSeverity::Info,
        "test", "startup", "ready");
    diagnostics.Record(renegade::bridge::DiagnosticSeverity::Warning,
        "ui", "focus", "control has no focus");
    diagnostics.Record(renegade::bridge::DiagnosticSeverity::Error,
        "audio", "inspect.failed", "path \"missing\"");

    if (diagnostics.Snapshot().size() != 2 || diagnostics.ErrorCount() != 1 ||
        diagnostics.WarningCount() != 1)
        return 1;
    const auto json = diagnostics.SnapshotJson();
    if (json.find("renegade.diagnostics.v1") == std::string::npos ||
        json.find("inspect.failed") == std::string::npos ||
        json.find("path \\\"missing\\\"") == std::string::npos)
        return 2;

    std::cout << "PASS: diagnostic evidence buffer and JSON snapshot\n";
    return 0;
}
