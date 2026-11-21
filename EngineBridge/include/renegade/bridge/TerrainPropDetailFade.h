#pragma once

// Reserved in ObjectComponent::_flags and ShaderMeshInstance::flags only for
// transient projected terrain geometry which is currently changing detail.
#define RENEGADE_TERRAIN_PROP_DETAIL_FADE (1u << 22u)
