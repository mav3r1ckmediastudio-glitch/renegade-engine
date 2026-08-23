#pragma once

#include <string>

#include <WickedEngine.h>
#include <imgui.h>

namespace renegade::studio
{
    // Studio-only Dear ImGui renderer/input bridge. Renegade's production UI
    // remains Wicked wiGUI; this bridge exists solely so the Graph Flow canvas
    // can use the established ImNodes interaction engine without introducing a
    // browser, Node.js or a second application shell.
    class RenegadeImGuiBridge final
    {
    public:
        ~RenegadeImGuiBridge();

        [[nodiscard]] bool Initialize(std::string& error);
        void Shutdown() noexcept;

        [[nodiscard]] bool BeginFrame(
            const wi::Canvas& canvas,
            float dt,
            bool acceptMouseInput,
            std::string& error);
        void EndFrame();

        void Render(wi::graphics::CommandList cmd) const;

        [[nodiscard]] bool IsInitialized() const noexcept
        {
            return initialized_;
        }

    private:
        [[nodiscard]] bool CreateDeviceObjects(std::string& error);
        [[nodiscard]] bool CompileShader(
            wi::graphics::ShaderStage stage,
            const std::string& sourceFile,
            wi::graphics::Shader& shader,
            std::string& error);

        wi::graphics::Shader vertexShader_;
        wi::graphics::Shader pixelShader_;
        wi::graphics::Texture fontTexture_;
        wi::graphics::Sampler sampler_;
        wi::graphics::InputLayout inputLayout_;
        wi::graphics::PipelineState pipelineState_;
        bool initialized_ = false;
        bool ownsContext_ = false;
        bool frameOpen_ = false;
        bool frameReady_ = false;
    };
}
