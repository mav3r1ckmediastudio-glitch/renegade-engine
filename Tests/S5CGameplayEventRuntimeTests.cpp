#include "RuntimeScriptRuntime.h"

#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptDocumentService.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    using namespace renegade::bridge;
    using namespace renegade::runtime;

    int Fail(const std::string& message)
    {
        std::cerr << "S5C gameplay event Runtime test failed: " << message << '\n';
        return 1;
    }

    bool WriteText(
        const fs::path& path,
        const std::string& text,
        std::string& error)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        if (ec)
        {
            error = ec.message();
            return false;
        }
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "could not open " + path.generic_u8string();
            return false;
        }
        stream << text;
        return static_cast<bool>(stream);
    }

    ScriptSourceBinding Source(
        const std::string& path,
        const std::string& hash)
    {
        ScriptSourceBinding source;
        source.sourceId = GenerateStableId();
        source.sourcePath = path;
        source.presentation = ScriptPresentation::Script;
        source.apiVersion = RuntimeScriptRuntime::ApiVersion;
        source.provenance.kind = ScriptProvenanceKind::Project;
        source.provenance.contentHash = hash;
        return source;
    }
}

int main()
{
    const fs::path root = fs::temp_directory_path() /
        fs::u8path("renegade-s5c-events-" + GenerateStableId());
    const fs::path scriptRoot = root / "Content" / "Scripts";

    std::string error;
    if (!WriteText(
            scriptRoot / "sender.lua",
            "return {\n"
            " on_start=function(self)\n"
            "  local ok,e=renegade.events.emit('s5c.broadcast','hello')\n"
            "  assert(ok and not e)\n"
            " end,\n"
            " on_event=function(self,event)\n"
            "  if event.name=='s5c.reply' then\n"
            "   assert(event.payload=='ack')\n"
            "   assert(event.target and renegade.entity.equals(event.target,self.entity))\n"
            "   local ok,e=renegade.transform.translate_local(self.entity,{x=0,y=0,z=3})\n"
            "   assert(ok and not e)\n"
            "  end\n"
            " end\n"
            "}\n",
            error))
        return Fail("write sender: " + error);

    if (!WriteText(
            scriptRoot / "receiver.lua",
            "return {\n"
            " on_event=function(self,event)\n"
            "  if event.name=='s5c.broadcast' then\n"
            "   assert(event.payload=='hello')\n"
            "   assert(event.sender and event.target==nil)\n"
            "   local ok,e=renegade.transform.translate_local(self.entity,{x=2,y=0,z=0})\n"
            "   assert(ok and not e)\n"
            "   local sent,se=renegade.events.send(event.sender,'s5c.reply','ack')\n"
            "   assert(sent and not se)\n"
            "  elseif event.name=='s5c.reply' then\n"
            "   error('targeted reply leaked to receiver')\n"
            "  end\n"
            " end\n"
            "}\n",
            error))
        return Fail("write receiver: " + error);

    if (!WriteText(
            scriptRoot / "failing.lua",
            "return {\n"
            " on_event=function(self,event)\n"
            "  if event.name=='s5c.broadcast' then error('intentional S5C recipient failure') end\n"
            " end\n"
            "}\n",
            error))
        return Fail("write failing receiver: " + error);

    wi::scene::Scene scene;
    const auto senderEntity = scene.Entity_CreateTransform("S5C Sender");
    const auto receiverEntity = scene.Entity_CreateTransform("S5C Receiver");
    const auto failingEntity = scene.Entity_CreateTransform("S5C Failing Receiver");
    const StableId senderId = GenerateStableId();
    const StableId receiverId = GenerateStableId();
    const StableId failingId = GenerateStableId();
    if (!AssignPersistentEntityId(scene, senderEntity, senderId, error) ||
        !AssignPersistentEntityId(scene, receiverEntity, receiverId, error) ||
        !AssignPersistentEntityId(scene, failingEntity, failingId, error))
        return Fail("assign entity identity: " + error);

    const StableId projectId = GenerateStableId();
    const StableId sceneId = GenerateStableId();
    ScriptDocument document = CreateScriptDocument(
        projectId, sceneId, "Content/Scenes/S5C.wiscene", "s5c-tests");
    if (!AddScriptAttachment(
            document,
            CreateScriptAttachment(
                ScriptScope::Entity,
                senderId,
                Source("Content/Scripts/sender.lua", "s5c-sender")),
            error) ||
        !AddScriptAttachment(
            document,
            CreateScriptAttachment(
                ScriptScope::Entity,
                receiverId,
                Source("Content/Scripts/receiver.lua", "s5c-receiver")),
            error) ||
        !AddScriptAttachment(
            document,
            CreateScriptAttachment(
                ScriptScope::Entity,
                failingId,
                Source("Content/Scripts/failing.lua", "s5c-failing")),
            error))
        return Fail("attach S5C fixtures: " + error);

    RuntimeScriptRuntime runtime;
    if (!runtime.StartScene(document, scene, root.generic_u8string(), error))
        return Fail("StartScene: " + error);

    if (runtime.ActiveInstanceCount() != 3 ||
        runtime.DisabledInstanceCount() != 0 ||
        runtime.PendingEventCount() != 1)
        return Fail("sender on_start did not enqueue the initial broadcast");

    runtime.Update(0.016f);

    const auto* receiverTransform = scene.transforms.GetComponent(receiverEntity);
    const auto* senderTransform = scene.transforms.GetComponent(senderEntity);
    if (receiverTransform == nullptr || receiverTransform->translation_local.x != 2.0f)
        return Fail("broadcast did not reach the healthy receiver");
    if (senderTransform == nullptr || senderTransform->translation_local.z != 0.0f)
        return Fail("reentrant targeted event was not deferred to the next phase");
    if (runtime.PendingEventCount() != 1 ||
        runtime.DispatchedEventCount() != 1 ||
        runtime.EventDeliveryAttemptCount() != 3)
        return Fail("broadcast dispatch evidence is incorrect");
    if (runtime.ActiveInstanceCount() != 2 || runtime.DisabledInstanceCount() != 1)
        return Fail("failing recipient was not isolated from healthy recipients");

    runtime.Update(0.016f);

    senderTransform = scene.transforms.GetComponent(senderEntity);
    if (senderTransform == nullptr || senderTransform->translation_local.z != 3.0f)
        return Fail("targeted reply did not reach only the sender on the next phase");
    if (runtime.PendingEventCount() != 0 ||
        runtime.DispatchedEventCount() != 2 ||
        runtime.EventDeliveryAttemptCount() != 4 ||
        runtime.LastEventName() != "s5c.reply" ||
        runtime.LastEventTarget() != senderId)
        return Fail("targeted/reentrant event evidence is incorrect");

    bool sawFailure = false;
    for (const auto& diagnostic : runtime.Diagnostics())
    {
        if (diagnostic.disabledInstance && diagnostic.callback == "on_event" &&
            diagnostic.message.find("intentional S5C recipient failure") != std::string::npos)
        {
            sawFailure = true;
            break;
        }
    }
    if (!sawFailure)
        return Fail("on_event recipient failure was not diagnosed");

    std::error_code ec;
    fs::remove_all(root, ec);
    return 0;
}
