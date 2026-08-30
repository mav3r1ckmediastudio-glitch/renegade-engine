from pathlib import Path

path = Path('.github/scripts/stage_phase5_gate7.py')
text = path.read_text(encoding='utf-8')

old_decl = '''replace_once(source,\n''' + "'''        wi::RenderPath3D::AO ambientOcclusion = wi::RenderPath3D::AO_DISABLED;\\n        switch (safe.ambientOcclusion)''',\n'''        const bool hardwareRayTracing = IsHardwareRayTracingAvailable();\\n        wi::RenderPath3D::AO ambientOcclusion = wi::RenderPath3D::AO_DISABLED;\\n        switch (safe.ambientOcclusion)''')"
new_decl = '''text = read(source)\nold_ao_decl = ''' + "'''        wi::RenderPath3D::AO ambientOcclusion = wi::RenderPath3D::AO_DISABLED;\\n        switch (safe.ambientOcclusion)'''\nif text.count(old_ao_decl) != 2:\n    raise RuntimeError(f\"{source}: expected two AO declaration anchors, found {text.count(old_ao_decl)}\")\nwrite(source, text.replace(old_ao_decl,\n'''        const bool hardwareRayTracing = IsHardwareRayTracingAvailable();\\n        wi::RenderPath3D::AO ambientOcclusion = wi::RenderPath3D::AO_DISABLED;\\n        switch (safe.ambientOcclusion)''', 1))"
if text.count(old_decl) != 1:
    raise RuntimeError(f'expected one AO declaration staging block, found {text.count(old_decl)}')
text = text.replace(old_decl, new_decl, 1)

old_switch = '''replace_once(source,\n''' + "'''        case RenderAmbientOcclusion::MSAO:\\n            ambientOcclusion = wi::RenderPath3D::AO_MSAO;\\n            break;\\n        case RenderAmbientOcclusion::Off:''',\n'''        case RenderAmbientOcclusion::MSAO:\\n            ambientOcclusion = wi::RenderPath3D::AO_MSAO;\\n            break;\\n        case RenderAmbientOcclusion::RTAO:\\n            if (hardwareRayTracing)\\n                ambientOcclusion = wi::RenderPath3D::AO_RTAO;\\n            break;\\n        case RenderAmbientOcclusion::Off:''')"
new_switch = '''text = read(source)\nold_ao_switch = ''' + "'''        case RenderAmbientOcclusion::MSAO:\\n            ambientOcclusion = wi::RenderPath3D::AO_MSAO;\\n            break;\\n        case RenderAmbientOcclusion::Off:'''\nif text.count(old_ao_switch) != 2:\n    raise RuntimeError(f\"{source}: expected two AO switch anchors, found {text.count(old_ao_switch)}\")\nwrite(source, text.replace(old_ao_switch,\n'''        case RenderAmbientOcclusion::MSAO:\\n            ambientOcclusion = wi::RenderPath3D::AO_MSAO;\\n            break;\\n        case RenderAmbientOcclusion::RTAO:\\n            if (hardwareRayTracing)\\n                ambientOcclusion = wi::RenderPath3D::AO_RTAO;\\n            break;\\n        case RenderAmbientOcclusion::Off:''', 1))"
if text.count(old_switch) != 1:
    raise RuntimeError(f'expected one AO switch staging block, found {text.count(old_switch)}')
text = text.replace(old_switch, new_switch, 1)

path.write_text(text, encoding='utf-8', newline='\n')
print('Gate 7 staging helper multi-anchor guards corrected')
