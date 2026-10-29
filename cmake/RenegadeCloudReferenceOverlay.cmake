# Keep the analytic geometry oracle out of production ray kernels.
# Both shader packages contain an explicit reference permutation for proofs.
foreach(_cloud_shader volumetricCloud_renderCS volumetricCloud_renderCS_capture volumetricCloud_shadow_renderCS)
    _ns_replace("offlineshadercompiler.cpp"
        "{\"${_cloud_shader}\", wi::graphics::ShaderStage::CS },"
        "{\"${_cloud_shader}\", wi::graphics::ShaderStage::CS, wi::graphics::ShaderModel::SM_6_0, {{}, {\"RENEGADE_CLOUD_ANALYTIC_REFERENCE\"}} },"
        "{\"${_cloud_shader}\", wi::graphics::ShaderStage::CS, wi::graphics::ShaderModel::SM_6_0, {{},")
endforeach()
_ns_replace("wiRenderer.cpp"
"void BindRenegadeCloudFormationResources(CommandList cmd)"
[=[wi::vector<std::string> RenegadeCloudProofPermutation()
{
    if (wi::arguments::HasArgument("cloud-analytic-bodies") ||
        (wi::arguments::HasArgument("single-cloud-body-study") &&
         !wi::arguments::HasArgument("cloud-cached-body")))
        return {"RENEGADE_CLOUD_ANALYTIC_REFERENCE"};
    return {};
}
void BindRenegadeCloudFormationResources(CommandList cmd)]=]
"wi::vector<std::string> RenegadeCloudProofPermutation()")
foreach(_cloud_shader volumetricCloud_renderCS volumetricCloud_renderCS_capture volumetricCloud_shadow_renderCS)
    _ns_replace("wiRenderer.cpp"
        "\"${_cloud_shader}.cso\");"
        "\"${_cloud_shader}.cso\", ShaderModel::SM_6_0, RenegadeCloudProofPermutation());"
        "\"${_cloud_shader}.cso\", ShaderModel::SM_6_0, RenegadeCloudProofPermutation());")
endforeach()

# Name rejected PSOs. Optional begin/end tracing also identifies compiler crashes.
_ns_replace("wiRenderer.cpp"
[=[return device->CreateShader(stage, decompressed.data(), decompressed.size(), &shader, entrypoint.c_str());]=]
[=[const bool audit = wi::arguments::HasArgument("cloud-shader-audit") && stage == ShaderStage::CS;
                if (audit)
                {
                    uint64_t hash = 1469598103934665603ull;
                    for (uint8_t byte : decompressed) hash = (hash ^ byte) * 1099511628211ull;
                    wi::backlog::post("CS_BEGIN " + shaderbinaryfilename + " bytes=" +
                        std::to_string(decompressed.size()) + " fnv=" + std::to_string(hash));
                }
                const bool created = device->CreateShader(stage, decompressed.data(),
                    decompressed.size(), &shader, entrypoint.c_str());
                if (!created)
                    wi::backlog::post("GPU shader creation FAILED: " + shaderbinaryfilename,
                        wi::backlog::LogLevel::Error);
                if (audit) wi::backlog::post("CS_END " + shaderbinaryfilename +
                    " created=" + std::to_string(created));
                return created;]=]
"GPU shader creation FAILED: ")
