#pragma once

#include "renegade/bridge/ScriptDocumentService.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <WickedEngine.h>

namespace renegade::runtime
{
    struct RuntimeScriptDiagnostic
    {
        bridge::StableId scriptInstanceId;
        bridge::StableId ownerEntityId;
        std::string sourcePath;
        std::string callback;
        std::string message;
        bool disabledInstance = false;
    };

    // S3 Runtime owner for creator Lua.
    //
    // This class owns one dedicated lua_State for the Runtime process. It does
    // not borrow Wicked's broad global Lua VM. Each ScriptInstanceId receives
    // its own environment, lifecycle table, context table and module cache even
    // when many instances share one governed sourceId.
    class RuntimeScriptRuntime final
    {
    public:
        static constexpr std::uint32_t ApiVersion = 1;
        static constexpr std::size_t DefaultMemoryBudgetBytes = 64u * 1024u * 1024u;
        static constexpr std::size_t DefaultCallbackInstructionBudget = 500000u;

        RuntimeScriptRuntime();
        ~RuntimeScriptRuntime();
        RuntimeScriptRuntime(const RuntimeScriptRuntime&) = delete;
        RuntimeScriptRuntime& operator=(const RuntimeScriptRuntime&) = delete;

        [[nodiscard]] bool Initialize(std::string& error);

        // Start from an already validated S2 document. Per-instance Lua/source
        // failures are isolated into diagnostics and do not fail this call;
        // document/runtime initialization failures do.
        [[nodiscard]] bool StartScene(
            const bridge::ScriptDocument& document,
            wi::scene::Scene& scene,
            std::string projectRoot,
            std::string& error);

        // Runtime convenience seam. If a Scene has no adjacent .rscripts file,
        // the Scene simply starts with zero creator scripts. If a companion
        // exists, its owning Scene ID is resolved from <scene>.rmeta before the
        // S2 document is accepted.
        [[nodiscard]] bool StartSceneFromCompanion(
            const std::string& scenePath,
            const bridge::StableId& expectedProjectId,
            wi::scene::Scene& scene,
            std::string projectRoot,
            std::string& error);

        void Update(float dt) noexcept;
        void Pause() noexcept;
        void Resume() noexcept;
        void ResetScene() noexcept;
        void StopScene() noexcept;

        // Explicit process-exit seam. RuntimeApplication calls this before
        // Wicked shutdown so creator Lua never survives the engine subsystems
        // it may eventually reference through renegade.*.
        void Shutdown() noexcept;

        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] bool IsPaused() const noexcept;
        [[nodiscard]] std::uint64_t Generation() const noexcept;
        [[nodiscard]] std::size_t ActiveInstanceCount() const noexcept;
        [[nodiscard]] std::size_t DisabledInstanceCount() const noexcept;
        [[nodiscard]] const std::vector<RuntimeScriptDiagnostic>&
            Diagnostics() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
