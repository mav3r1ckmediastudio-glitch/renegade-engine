// Developer-only fixture utility. NEVER run against a user project.
// Authors a governed Lua attachment into the disposable Inventory-Proof copy
// created by G3-E build verification. No editor, asset or scene mutations.
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ScriptDocumentService.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace renegade::bridge;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: RenegadeInventoryRuntimeProofAuthor <disposable-Inventory-Proof-root>\n";
        return 2;
    }
    std::error_code ec;
    const fs::path root=fs::weakly_canonical(fs::u8path(argv[1]), ec);
    if (ec || root.filename() != "Inventory-Proof" ||
        root.generic_u8string().find("/BUILD/inventory-full/Runtime/Release/Content/Inventory-Proof")==std::string::npos ||
        !fs::exists(root / "ScreenProject.renegade")) {
        std::cerr << "Refusing to alter anything outside isolated Inventory-Proof build fixture\n";
        return 3;
    }
    const fs::path scripts = root / "Content" / "Scripts";
    fs::create_directories(scripts, ec);
    if (ec) return 4;
    const fs::path luaSource = scripts / "inventory_live_proof.lua";
    const std::string lua=R"LUA(
-- G3-E interactive developer probe. No standalone inventory UI yet.
return {
    on_start = function(self)
        if not renegade.inventory then
            error("Inventory proof: Lua capability missing")
        end
        local player, pe = renegade.inventory.list("player")
        local chest, ce = renegade.inventory.list("chest")
        if not player or not chest then
            error("Inventory proof: Player/chest authority missing " .. tostring(pe or ce))
        end
    end,
    on_update = function(self, dt)
        local inv = renegade.inventory
        if not inv then return end
        local p = inv.list("player")
        local c = inv.list("chest")
        if not p or not c then return end
        local function total(container)
            local quantity=0
            for i=1,#container.stacks do
                if container.stacks[i].definition_id=="health-potion" then
                    quantity=quantity+container.stacks[i].quantity
                end
            end
            return quantity
        end
        local playerCount=total(p)
        local chestCount=total(c)
        local prompt="INVENTORY PROOF | Player: " .. playerCount ..
            " potions | Chest: " .. chestCount ..
            " potions | Interact: transfer 1"
        renegade.ui.show_prompt(prompt)
        if not renegade.input.was_pressed("interact") then return end
        if playerCount > 0 then
            local first=p.stacks[1]
            local ok,why=inv.transfer("player","chest",first.instance_id,1)
            if not ok then error("Transfer failed: "..tostring(why)) end
        elseif chestCount > 0 then
            local first=c.stacks[1]
            local ok,why=inv.transfer("chest","player",first.instance_id,1)
            if not ok then error("Return transfer failed: "..tostring(why)) end
        end
    end
}
)LUA";
    {
        std::ofstream script(luaSource, std::ios::binary | std::ios::trunc);
        script << lua;
        if (!script.good()) return 5;
    }
    const StableId projectId="61111111-1111-4111-8111-111111111111";
    std::string error;
    for (const auto* level:{"LevelOne","LevelTwo"}) {
        const fs::path scene=root/"Content"/"Scenes"/(std::string(level)+".wiscene");
        DocumentEnvelope meta;
        if (!ReadDocumentEnvelope(scene.generic_u8string()+".rmeta",meta,error) ||
            meta.projectId!=projectId) {
            std::cerr << "Refusing scene metadata: " << error << "\n";
            return 6;
        }
        const auto sceneHint=std::string("Content/Scenes/")+level+".wiscene";
        ScriptDocument doc=CreateScriptDocument(projectId,meta.documentId,sceneHint,"inventory-runtime-proof");
        ScriptSourceBinding source;
        source.sourceId=GenerateStableId();
        source.sourcePath="Content/Scripts/inventory_live_proof.lua";
        source.presentation=ScriptPresentation::GlobalScript;
        source.apiVersion=1;
        source.provenance.kind=ScriptProvenanceKind::Project;
        source.provenance.contentHash="inventory-runtime-proof";
        ScriptAttachment attachment=CreateScriptAttachment(ScriptScope::Level,{},source);
        const auto id=attachment.scriptInstanceId;
        if (!AddScriptAttachment(doc,std::move(attachment),error) ||
            !WriteScriptDocument(ScriptDocumentPathForScene(scene.generic_u8string()),doc,error)) {
            std::cerr << "Governed authoring failed: " << error << "\n";
            return 7;
        }
        std::ofstream access(scene.generic_u8string()+".rinventory-access",
                             std::ios::binary|std::ios::trunc);
        access<<"RENEGADE_INVENTORY_ACCESS 1 2\n"
              <<"CONTAINER \""<<id<<"\" \"-\" \"player\" 1 1 1\n"
              <<"CONTAINER \""<<id<<"\" \"-\" \"chest\" 1 1 1\n";
        if (!access.good()) return 8;
        std::cout << level << " authorised Lua script instance " << id << "\n";
    }
    std::cout << "Authored independent Lua inventory proof in disposable build fixture\n";
    return 0;
}
