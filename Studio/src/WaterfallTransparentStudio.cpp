#include "StudioApplication.h"
using namespace DirectX;using namespace wi::graphics;
namespace renegade::studio {
void StudioRenderPath::RenderWaterfallTransparents(CommandList cmd) const {
#include "renegade/bridge/WaterfallTransparentPass.inl"
}
}
