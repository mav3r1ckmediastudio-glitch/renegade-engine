#pragma once

#include <string>

namespace renegade::bridge
{
    // Renegade compiles the accepted Wicked FBX/GLTF converter translation
    // units with their legacy messageBox() call redirected to a non-modal
    // adapter. These helpers expose only diagnostic evidence for tests and
    // logging; they are not a second importer or an error-policy authority.
    void ClearWickedModelImporterFailureDiagnostic() noexcept;

    [[nodiscard]] std::string ConsumeWickedModelImporterFailureDiagnostic();
}
