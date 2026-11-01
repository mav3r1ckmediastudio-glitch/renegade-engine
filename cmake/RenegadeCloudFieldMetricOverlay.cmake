# Derived cache format change: four RGBA16F value/derivative tiles.
# See docs/CLOUD_FORMATION_GATE1_FIELD_METRIC_2026-10-29.md.
_ns_replace("wiRenderer.cpp"
"body.width = body.height = body.depth = 128;"
"body.width = body.height = 256;\n        body.depth = 128;"
"body.width = body.height = 256;")
_ns_replace("wiRenderer.cpp"
[=[device->BindUAV(&texture_renegadeCloudBodies, 0, cmd);
        device->Dispatch(16, 16, 16, cmd);]=]
[=[device->BindUAV(&texture_renegadeCloudBodies, 0, cmd);
        // Renegade metric-field dispatch follows the actual atlas dimensions.
        const auto& fieldDesc = texture_renegadeCloudBodies.desc;
        device->Dispatch((fieldDesc.width + 7) / 8,
            (fieldDesc.height + 7) / 8, (fieldDesc.depth + 7) / 8, cmd);]=]
"// Renegade metric-field dispatch follows")
