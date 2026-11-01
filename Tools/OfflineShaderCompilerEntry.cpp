#include "wiRenderer.h"

// Keep the upstream compiler implementation intact. Renegade supplies the
// source directory explicitly because its build may be on another drive.
int RenegadeOfflineShaderCompilerMain(int argc, char* argv[]);

int main(int argc, char* argv[])
{
    wi::renderer::SetShaderSourcePath(RENEGADE_SHADER_SOURCE_DIRECTORY);
    return RenegadeOfflineShaderCompilerMain(argc, argv);
}
