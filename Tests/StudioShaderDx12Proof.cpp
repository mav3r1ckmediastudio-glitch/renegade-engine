// Manual Windows GPU regression: compile shipped Studio shaders and exercise
// Wicked's real DX12 CreateShader / indirect-command-signature path.
#include <WickedEngine.h>
#include <wiShaderCompiler.h>
#include <windows.h>
#include <iostream>
#include <string>

LRESULT CALLBACK ShaderProofWindow(HWND h, UINT m, WPARAM w, LPARAM l)
{
    return DefWindowProcW(h, m, w, l);
}

int main(int argc, char** argv)
{
    if (argc != 2) return 2; // Directory containing the four Studio HLSL files.
    std::cout << std::unitbuf;
    wi::arguments::Parse(L"debugdevice");
    WNDCLASSEXW cls = {};
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = ShaderProofWindow;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"RenegadeStudioShaderProof";
    if (!RegisterClassExW(&cls)) return 3;
    HWND window = CreateWindowExW(0, cls.lpszClassName, L"Studio Shader Proof",
        WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, cls.hInstance, nullptr);
    if (!window) return 4;
    int result = 0;
    {
        wi::Application app;
        app.allow_hdr = false;
        app.SetWindow(window);
        auto* device = wi::graphics::GetDevice();
        if (!device || device->GetShaderFormat() != wi::graphics::ShaderFormat::HLSL6)
            return 5;
        for (const char* pair : {"RenegadeGrid", "RenegadeImGui"})
        {
            wi::graphics::Shader shaders[2];
            for (int stage = 0; stage < 2; ++stage)
            {
                const std::string file = std::string(argv[1]) + "/" + pair +
                    (stage == 0 ? "VS.hlsl" : "PS.hlsl");
                wi::shadercompiler::CompilerInput input;
                input.format = device->GetShaderFormat();
                input.stage = stage == 0 ? wi::graphics::ShaderStage::VS :
                    wi::graphics::ShaderStage::PS;
                input.minshadermodel = wi::graphics::ShaderModel::SM_6_0;
                input.flags = wi::shadercompiler::Flags::STRIP_REFLECTION;
                input.shadersourcefilename = file;
                wi::shadercompiler::CompilerOutput output;
                wi::shadercompiler::Compile(input, output);
                if (!output.IsValid() || !device->CreateShader(input.stage,
                    output.shaderdata, output.shadersize, &shaders[stage]))
                {
                    std::cerr << file << ": " << output.error_message << "\n";
                    return 6;
                }
                std::cout << file << ": compiled and created\n";
            }
            wi::graphics::PipelineStateDesc desc;
            desc.vs = &shaders[0];
            desc.ps = &shaders[1];
            wi::graphics::PipelineState pipeline;
            if (!device->CreatePipelineState(&desc, &pipeline)) return 7;
            std::cout << pair << ": shader pair pipeline created\n";
        }
        device->WaitForGPU();
    }
    DestroyWindow(window);
    UnregisterClassW(cls.lpszClassName, cls.hInstance);
    std::cout << "Studio DX12 shader proof passed\n";
    return result;
}
