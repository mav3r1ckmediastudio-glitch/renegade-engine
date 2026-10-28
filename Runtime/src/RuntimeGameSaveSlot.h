#pragma once
// G5D: one governed, player-owned quick-save file. Never writes a creator's
// WISCENE, inventory companion or a packaged GameData tree.
#include <string>

namespace renegade::runtime {
struct RuntimeGameSaveState {
    std::string projectId;
    std::string sceneId;
    std::string inventoryBytes;
    std::string retiredWorldBytes;
    float playerHealth=100.0f;
    float playerMaxHealth=100.0f;
    bool playerDead=false;
    float playerX=0.0f,playerY=0.0f,playerZ=0.0f;
    float playerYaw=0.0f,playerPitch=0.0f;
};
// Pure bounded validation; caller must validate both payloads against their
// live gameplay services before applying them to a freshly loaded scene.
bool ValidateRuntimeGameSaveState(const RuntimeGameSaveState& state,
                                  std::string& error);
class RuntimeGameSaveSlot final {
public:
    // localAppData must be the actual per-user directory. Stable saveDataId
    // is the packaged identity, or the authoring project ID in Test Game.
    static bool WriteQuick(const std::string& localAppData,
        const std::string& saveDataId,const RuntimeGameSaveState& state,
        std::string& error);
    static bool ReadQuick(const std::string& localAppData,
        const std::string& saveDataId,
        const std::string& expectedProjectId,const std::string& expectedSceneId,
        RuntimeGameSaveState& state,std::string& error);
};
}
