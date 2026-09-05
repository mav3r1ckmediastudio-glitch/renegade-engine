#include "RenegadeImGuiBridge.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <wiShaderCompiler.h>

namespace renegade::studio
{
    using namespace wi::graphics;

    RenegadeImGuiBridge::~RenegadeImGuiBridge()
    {
        Shutdown();
    }

    bool RenegadeImGuiBridge::CompileShader(
        const ShaderStage stage,
        const std::string& sourceFile,
        Shader& shader,
        std::string& error)
    {
        auto* device = wi::graphics::GetDevice();
        if (!device)
        {
            error = "Wicked graphics device is unavailable.";
            return false;
        }

        wi::shadercompiler::CompilerInput input;
        input.format = device->GetShaderFormat();
        input.stage = stage;
        input.minshadermodel = ShaderModel::SM_6_0;
        input.flags = wi::shadercompiler::Flags::STRIP_REFLECTION;
        input.shadersourcefilename =
            wi::helper::GetCurrentPath() + "/Content/shaders/" + sourceFile;

        wi::shadercompiler::CompilerOutput output;
        wi::shadercompiler::Compile(input, output);
        if (!output.IsValid())
        {
            error = "Could not compile " + sourceFile + ": " + output.error_message;
            return false;
        }
        if (!device->CreateShader(
                stage,
                output.shaderdata,
                output.shadersize,
                &shader))
        {
            error = "Wicked rejected compiled shader " + sourceFile + '.';
            return false;
        }
        error.clear();
        return true;
    }

    bool RenegadeImGuiBridge::CreateDeviceObjects(std::string& error)
    {
        auto* device = wi::graphics::GetDevice();
        if (!device || ImGui::GetCurrentContext() == nullptr)
        {
            error = "ImGui renderer initialization requires Wicked and ImGui contexts.";
            return false;
        }

        if (!CompileShader(
                ShaderStage::VS,
                "RenegadeImGuiVS.hlsl",
                vertexShader_,
                error) ||
            !CompileShader(
                ShaderStage::PS,
                "RenegadeImGuiPS.hlsl",
                pixelShader_,
                error))
        {
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        unsigned char* pixels = nullptr;
        int width = 0;
        int height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        if (!pixels || width <= 0 || height <= 0)
        {
            error = "Dear ImGui did not produce a valid font atlas.";
            return false;
        }

        TextureDesc textureDesc;
        textureDesc.width = static_cast<uint32_t>(width);
        textureDesc.height = static_cast<uint32_t>(height);
        textureDesc.mip_levels = 1;
        textureDesc.array_size = 1;
        textureDesc.format = Format::R8G8B8A8_UNORM;
        textureDesc.bind_flags = BindFlag::SHADER_RESOURCE;

        SubresourceData textureData;
        textureData.data_ptr = pixels;
        textureData.row_pitch =
            static_cast<uint32_t>(width) * GetFormatStride(textureDesc.format);
        textureData.slice_pitch = textureData.row_pitch * static_cast<uint32_t>(height);
        if (!device->CreateTexture(&textureDesc, &textureData, &fontTexture_))
        {
            error = "Wicked could not create the ImGui font texture.";
            return false;
        }

        SamplerDesc samplerDesc;
        samplerDesc.address_u = TextureAddressMode::CLAMP;
        samplerDesc.address_v = TextureAddressMode::CLAMP;
        samplerDesc.address_w = TextureAddressMode::CLAMP;
        samplerDesc.filter = Filter::MIN_MAG_MIP_LINEAR;
        if (!device->CreateSampler(&samplerDesc, &sampler_))
        {
            error = "Wicked could not create the ImGui font sampler.";
            return false;
        }

        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(&fontTexture_));

        inputLayout_.elements =
        {
            {"POSITION", 0, Format::R32G32_FLOAT, 0,
                static_cast<uint32_t>(IM_OFFSETOF(ImDrawVert, pos)),
                InputClassification::PER_VERTEX_DATA},
            {"TEXCOORD", 0, Format::R32G32_FLOAT, 0,
                static_cast<uint32_t>(IM_OFFSETOF(ImDrawVert, uv)),
                InputClassification::PER_VERTEX_DATA},
            {"COLOR", 0, Format::R8G8B8A8_UNORM, 0,
                static_cast<uint32_t>(IM_OFFSETOF(ImDrawVert, col)),
                InputClassification::PER_VERTEX_DATA},
        };

        PipelineStateDesc pipelineDesc;
        pipelineDesc.vs = &vertexShader_;
        pipelineDesc.ps = &pixelShader_;
        pipelineDesc.il = &inputLayout_;
        pipelineDesc.dss = wi::renderer::GetDepthStencilState(
            wi::enums::DSSTYPE_DEPTHDISABLED);
        pipelineDesc.rs = wi::renderer::GetRasterizerState(
            wi::enums::RSTYPE_DOUBLESIDED);
        pipelineDesc.bs = wi::renderer::GetBlendState(
            wi::enums::BSTYPE_TRANSPARENT);
        pipelineDesc.pt = PrimitiveTopology::TRIANGLELIST;
        if (!device->CreatePipelineState(&pipelineDesc, &pipelineState_))
        {
            error = "Wicked could not create the ImGui pipeline state.";
            return false;
        }

        error.clear();
        return true;
    }

    bool RenegadeImGuiBridge::Initialize(std::string& error)
    {
        if (initialized_)
        {
            error.clear();
            return true;
        }

        if (ImGui::GetCurrentContext() != nullptr)
        {
            error = "Renegade Graph requires exclusive ownership of the Studio ImGui context.";
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ownsContext_ = true;

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.BackendRendererName = "Renegade-Wicked";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
        ImGui::StyleColorsDark();

        if (!CreateDeviceObjects(error))
        {
            Shutdown();
            return false;
        }

        initialized_ = true;
        error.clear();
        return true;
    }

    void RenegadeImGuiBridge::Shutdown() noexcept
    {
        frameOpen_ = false;
        frameReady_ = false;
        initialized_ = false;
        fontTexture_ = {};
        sampler_ = {};
        vertexShader_ = {};
        pixelShader_ = {};
        pipelineState_ = {};
        inputLayout_ = {};

        if (ownsContext_ && ImGui::GetCurrentContext() != nullptr)
            ImGui::DestroyContext();
        ownsContext_ = false;
    }

    bool RenegadeImGuiBridge::BeginFrame(
        const wi::Canvas& canvas,
        const float dt,
        const bool acceptMouseInput,
        std::string& error)
    {
        if (!initialized_ && !Initialize(error))
            return false;
        if (frameOpen_)
        {
            error = "Dear ImGui frame was begun twice.";
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(
            std::max(1.0f, canvas.GetLogicalWidth()),
            std::max(1.0f, canvas.GetLogicalHeight()));
        const float dpi = std::max(0.01f, canvas.GetDPIScaling());
        io.DisplayFramebufferScale = ImVec2(dpi, dpi);
        io.DeltaTime = std::max(0.0001f, dt);

        const XMFLOAT4 pointer = wi::input::GetPointer();
        io.MousePos = ImVec2(pointer.x, pointer.y);
        io.MouseDown[0] = acceptMouseInput &&
            wi::input::Down(wi::input::MOUSE_BUTTON_LEFT);
        io.MouseDown[1] = acceptMouseInput &&
            wi::input::Down(wi::input::MOUSE_BUTTON_RIGHT);
        io.MouseDown[2] = acceptMouseInput &&
            wi::input::Down(wi::input::MOUSE_BUTTON_MIDDLE);
        io.MouseWheel = acceptMouseInput ? pointer.z : 0.0f;
        io.MouseWheelH = 0.0f;
        io.KeyCtrl = wi::input::Down(wi::input::KEYBOARD_BUTTON_LCONTROL) ||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_RCONTROL);
        io.KeyShift = wi::input::Down(wi::input::KEYBOARD_BUTTON_LSHIFT) ||
            wi::input::Down(wi::input::KEYBOARD_BUTTON_RSHIFT);
        io.KeyAlt = wi::input::Down(wi::input::KEYBOARD_BUTTON_ALT);

        ImGui::NewFrame();
        frameOpen_ = true;
        frameReady_ = false;
        error.clear();
        return true;
    }

    void RenegadeImGuiBridge::EndFrame()
    {
        if (!frameOpen_)
            return;
        ImGui::Render();
        frameOpen_ = false;
        frameReady_ = true;
    }

    void RenegadeImGuiBridge::Render(const CommandList cmd) const
    {
        if (!initialized_ || !frameReady_ || ImGui::GetCurrentContext() == nullptr)
            return;

        const ImDrawData* drawData = ImGui::GetDrawData();
        if (!drawData || drawData->TotalVtxCount <= 0 || drawData->TotalIdxCount <= 0)
            return;

        const int framebufferWidth = static_cast<int>(
            drawData->DisplaySize.x * drawData->FramebufferScale.x);
        const int framebufferHeight = static_cast<int>(
            drawData->DisplaySize.y * drawData->FramebufferScale.y);
        if (framebufferWidth <= 0 || framebufferHeight <= 0)
            return;

        auto* device = wi::graphics::GetDevice();
        if (!device)
            return;

        const uint64_t vertexBytes =
            sizeof(ImDrawVert) * static_cast<uint64_t>(drawData->TotalVtxCount);
        const uint64_t indexBytes =
            sizeof(ImDrawIdx) * static_cast<uint64_t>(drawData->TotalIdxCount);
        auto vertexAllocation = device->AllocateGPU(vertexBytes, cmd);
        auto indexAllocation = device->AllocateGPU(indexBytes, cmd);
        if (!vertexAllocation.data || !indexAllocation.data)
            return;

        auto* vertexDestination =
            reinterpret_cast<ImDrawVert*>(vertexAllocation.data);
        auto* indexDestination =
            reinterpret_cast<ImDrawIdx*>(indexAllocation.data);
        for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex)
        {
            const ImDrawList* list = drawData->CmdLists[listIndex];
            std::memcpy(
                vertexDestination,
                list->VtxBuffer.Data,
                static_cast<size_t>(list->VtxBuffer.Size) * sizeof(ImDrawVert));
            std::memcpy(
                indexDestination,
                list->IdxBuffer.Data,
                static_cast<size_t>(list->IdxBuffer.Size) * sizeof(ImDrawIdx));
            vertexDestination += list->VtxBuffer.Size;
            indexDestination += list->IdxBuffer.Size;
        }

        struct Constants
        {
            float mvp[4][4];
        } constants{};

        const float left = drawData->DisplayPos.x;
        const float right = drawData->DisplayPos.x + drawData->DisplaySize.x;
        const float top = drawData->DisplayPos.y;
        const float bottom = drawData->DisplayPos.y + drawData->DisplaySize.y;
        const float matrix[4][4] =
        {
            {2.0f / (right - left), 0.0f, 0.0f, 0.0f},
            {0.0f, 2.0f / (top - bottom), 0.0f, 0.0f},
            {0.0f, 0.0f, 0.5f, 0.0f},
            {(right + left) / (left - right),
             (top + bottom) / (bottom - top), 0.5f, 1.0f},
        };
        std::memcpy(constants.mvp, matrix, sizeof(matrix));
        device->BindDynamicConstantBuffer(constants, 0, cmd);

        const GPUBuffer* vertexBuffers[] = {&vertexAllocation.buffer};
        const uint32_t strides[] = {sizeof(ImDrawVert)};
        const uint64_t offsets[] = {vertexAllocation.offset};
        device->BindVertexBuffers(vertexBuffers, 0, 1, strides, offsets, cmd);
        device->BindIndexBuffer(
            &indexAllocation.buffer,
            sizeof(ImDrawIdx) == 2
                ? IndexBufferFormat::UINT16
                : IndexBufferFormat::UINT32,
            indexAllocation.offset,
            cmd);

        Viewport viewport;
        viewport.width = static_cast<float>(framebufferWidth);
        viewport.height = static_cast<float>(framebufferHeight);
        device->BindViewports(1, &viewport, cmd);
        device->BindPipelineState(&pipelineState_, cmd);
        device->BindSampler(&sampler_, 0, cmd);

        const ImVec2 clipOffset = drawData->DisplayPos;
        const ImVec2 clipScale = drawData->FramebufferScale;
        int globalVertexOffset = 0;
        uint32_t globalIndexOffset = 0;
        for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex)
        {
            const ImDrawList* list = drawData->CmdLists[listIndex];
            for (int commandIndex = 0;
                 commandIndex < list->CmdBuffer.Size;
                 ++commandIndex)
            {
                const ImDrawCmd& drawCommand = list->CmdBuffer[commandIndex];
                if (drawCommand.UserCallback)
                {
                    if (drawCommand.UserCallback != ImDrawCallback_ResetRenderState)
                        drawCommand.UserCallback(list, &drawCommand);
                    continue;
                }

                ImVec2 clipMin(
                    (drawCommand.ClipRect.x - clipOffset.x) * clipScale.x,
                    (drawCommand.ClipRect.y - clipOffset.y) * clipScale.y);
                ImVec2 clipMax(
                    (drawCommand.ClipRect.z - clipOffset.x) * clipScale.x,
                    (drawCommand.ClipRect.w - clipOffset.y) * clipScale.y);
                clipMin.x = std::clamp(
                    clipMin.x, 0.0f, static_cast<float>(framebufferWidth));
                clipMin.y = std::clamp(
                    clipMin.y, 0.0f, static_cast<float>(framebufferHeight));
                clipMax.x = std::clamp(
                    clipMax.x, 0.0f, static_cast<float>(framebufferWidth));
                clipMax.y = std::clamp(
                    clipMax.y, 0.0f, static_cast<float>(framebufferHeight));
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                    continue;

                Rect scissor;
                scissor.left = static_cast<int32_t>(clipMin.x);
                scissor.top = static_cast<int32_t>(clipMin.y);
                scissor.right = static_cast<int32_t>(clipMax.x);
                scissor.bottom = static_cast<int32_t>(clipMax.y);
                device->BindScissorRects(1, &scissor, cmd);

                const Texture* texture = drawCommand.TextureId
                    ? reinterpret_cast<const Texture*>(drawCommand.TextureId)
                    : &fontTexture_;
                device->BindResource(texture, 0, cmd);
                device->DrawIndexed(
                    drawCommand.ElemCount,
                    globalIndexOffset + drawCommand.IdxOffset,
                    globalVertexOffset + static_cast<int32_t>(drawCommand.VtxOffset),
                    cmd);
            }
            globalIndexOffset += static_cast<uint32_t>(list->IdxBuffer.Size);
            globalVertexOffset += list->VtxBuffer.Size;
        }
    }
}
