#include "RuntimeApplication.h"
using namespace DirectX;using namespace wi::graphics;
namespace renegade::runtime {
void RuntimeRenderPath::RenderWaterfallTransparents(CommandList cmd) const {
#include "renegade/bridge/WaterfallTransparentPass.inl"
}
}
