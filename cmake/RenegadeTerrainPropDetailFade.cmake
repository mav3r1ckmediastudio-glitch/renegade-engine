# Keep both baked shader backends and the native instance upload in agreement.
# This overlay composes with the pinned scene and shadow shader sources.
configure_file(
    "${CMAKE_SOURCE_DIR}/EngineBridge/include/renegade/bridge/TerrainPropDetailFade.h"
    "${_ns_wicked}/shaders/renegadeTerrainPropDetailFade.h" COPYONLY)
_ns_replace("wiScene.cpp" "#include \"wiScene.h\""
    "#include \"wiScene.h\"\n#include \"shaders/renegadeTerrainPropDetailFade.h\""
    "#include \"shaders/renegadeTerrainPropDetailFade.h\"")
_ns_replace("wiScene.cpp" "inst.color = wi::math::pack_half4(object.color);"
    "inst.flags |= object._flags & RENEGADE_TERRAIN_PROP_DETAIL_FADE;\n\t\t\t\tinst.color = wi::math::pack_half4(object.color);"
    "inst.flags |= object._flags & RENEGADE_TERRAIN_PROP_DETAIL_FADE;")
_ns_replace("shaders/shadowPS_alphatest.hlsl" "#include \"objectHF.hlsli\""
    "#include \"objectHF.hlsli\"\n#include \"renegadeTerrainPropDetailFade.h\""
    "#include \"renegadeTerrainPropDetailFade.h\"")
_ns_replace("shaders/shadowPS_alphatest.hlsl"
    "clip(alpha - material.GetAlphaTest() - meshinstance.GetAlphaTest());"
    [=[if ((meshinstance.flags & RENEGADE_TERRAIN_PROP_DETAIL_FADE) != 0)
    {
        clip(dither((min16uint2)input.pos.xy) - (1 - meshinstance.GetColor().a));
    }
    clip(alpha - material.GetAlphaTest() - meshinstance.GetAlphaTest());]=]
    "meshinstance.flags & RENEGADE_TERRAIN_PROP_DETAIL_FADE")
