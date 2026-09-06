#include "renegade/bridge/DiagnosticService.h"
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

using namespace renegade::bridge;
static bool Check(bool okay, const char* message)
{
    if (!okay) std::cerr << "FAIL: " << message << '\n';
    return okay;
}
#ifdef _WIN32
static std::string Query(std::uint16_t port, const std::string& path = "/snapshot")
{
    // StartLocalEndpoint has an active Winsock reference during this test.
    SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == INVALID_SOCKET) return {};
    DWORD timeout = 2000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) { closesocket(client); return {}; }
    const std::string request = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    send(client, request.data(), static_cast<int>(request.size()), 0);
    std::string result; char buffer[4096]; int count;
    while ((count = recv(client, buffer, sizeof(buffer), 0)) > 0) result.append(buffer, count);
    closesocket(client);
    return result;
}
#endif
int main()
{
    DiagnosticService diagnostics(2);
    diagnostics.Identify("studio");
    diagnostics.Record(DiagnosticSeverity::Info, "test", "startup", "ready");
    diagnostics.Record(DiagnosticSeverity::Warning, "ui", "focus", "control has no focus");
    diagnostics.Record(DiagnosticSeverity::Error, "audio", "inspect.failed", "path \"missing\"");
    diagnostics.Record(DiagnosticSeverity::Error, "audio", "inspect.failed", "path \"missing\"");
    if (!Check(diagnostics.Snapshot().size() == 2 && diagnostics.ErrorCount() == 1 &&
        diagnostics.WarningCount() == 1 && diagnostics.Snapshot().back().repetitions == 2, "bounded/coalesced events")) return 1;
    diagnostics.SetState("editor", {{"workspace", std::string("terrain")}, {"selection", nullptr}, {"open", true}});
    const auto json = diagnostics.SnapshotJson();
    if (!Check(json.find("renegade.diagnostics.v2") != std::string::npos &&
        json.find("path \\\"missing\\\"") != std::string::npos && json.find("\"selection\":null") != std::string::npos,
        "JSON identity and typed state")) return 2;
    diagnostics.SetState("editor", {{"workspace", std::string("scene")}});
    if (!Check(diagnostics.SnapshotJson().find("\"selection\"") == std::string::npos, "replace clears stale state")) return 3;
    std::string controlBytes;
    for (int i = 0; i < 32; ++i) controlBytes += static_cast<char>(i);
    diagnostics.Record(DiagnosticSeverity::Info, "test", "escape", controlBytes);
    if (!Check(diagnostics.SnapshotJson().find("\\u0000") != std::string::npos &&
        diagnostics.SnapshotJson().find("\\u001f") != std::string::npos, "all JSON control bytes escaped")) return 4;
    std::thread writer([&] { for (std::uint64_t i = 0; i < 1000; ++i) diagnostics.SetState("counter", {{"value", i}}); });
    for (int i = 0; i < 1000; ++i) (void)diagnostics.SnapshotJson();
    writer.join();
    diagnostics.Clear();
    diagnostics.Observe("test", {{"value", true}}, "test");
    diagnostics.Observe("test", {{"value", true}}, "test");
    if (!Check(diagnostics.Snapshot().size() == 1, "unchanged observations do not flood events")) return 5;
#ifdef _WIN32
    if (!Check(diagnostics.StartLocalEndpoint(0), "live endpoint binds")) return 6;
    DiagnosticService conflict;
    if (!Check(!conflict.StartLocalEndpoint(diagnostics.EndpointPort()), "bind conflict reports failure")) return 7;
    if (!Check(Query(diagnostics.EndpointPort()).find("\"workspace\":\"scene\"") != std::string::npos, "live state query")) return 8;
    diagnostics.SetState("editor", {{"workspace", std::string("terrain")}});
    if (!Check(Query(diagnostics.EndpointPort()).find("\"workspace\":\"terrain\"") != std::string::npos, "live state changes")) return 9;
    // Safe test-only failure: no real editor action or project data is touched.
    diagnostics.SetState("last_action", {{"name", std::string("DiagnosticTestAction")},
        {"stage", std::string("blocked")}, {"reason", std::string("test consumer unavailable")},
        {"source", std::string("Tests/DiagnosticServiceTests.cpp")}});
    if (!Check(Query(diagnostics.EndpointPort()).find("test consumer unavailable") != std::string::npos, "controlled live transport failure evidence")) return 10;
    diagnostics.SetState("last_action", {});
    if (!Check(Query(diagnostics.EndpointPort()).find("test consumer unavailable") == std::string::npos, "test fault removed")) return 11;
    if (!Check(Query(diagnostics.EndpointPort(), "/execute").find("404 Not Found") != std::string::npos, "commands rejected")) return 12;
    diagnostics.StopLocalEndpoint();
    if (!Check(diagnostics.StartLocalEndpoint(0), "endpoint restart")) return 13;
    diagnostics.StopLocalEndpoint();
    std::cout << "PASS: Windows live transport, conflict, change visibility and test-only controlled failure\n";
#else
    std::cout << "SKIP: Windows live transport (host is not Windows)\n";
#endif
    std::cout << "PASS: bounded diagnostics, typed snapshots, escaping, replacement and concurrent publication\n";
}
