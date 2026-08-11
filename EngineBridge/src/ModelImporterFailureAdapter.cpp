#include "renegade/bridge/ModelImporterFailureAdapter.h"

#include <utility>

namespace
{
    thread_local std::string g_wickedModelImporterFailureDiagnostic;
}

// The accepted Wicked FBX and GLTF converter translation units are compiled
// by Renegade with the token `messageBox` source-locally renamed to this
// function. Wicked's real wi::helper::messageBox() remains untouched everywhere
// else in the pinned engine. Converter parse/read failures therefore record a
// diagnostic and return normally instead of opening a native modal dialog.
namespace wi::helper
{
    void RenegadeImporterMessageBox(
        const std::string& message,
        const std::string& caption)
    {
        if (caption.empty())
        {
            g_wickedModelImporterFailureDiagnostic = message;
        }
        else if (message.empty())
        {
            g_wickedModelImporterFailureDiagnostic = caption;
        }
        else
        {
            g_wickedModelImporterFailureDiagnostic = caption + ": " + message;
        }
    }
}

namespace renegade::bridge
{
    void ClearWickedModelImporterFailureDiagnostic() noexcept
    {
        g_wickedModelImporterFailureDiagnostic.clear();
    }

    std::string ConsumeWickedModelImporterFailureDiagnostic()
    {
        std::string diagnostic =
            std::move(g_wickedModelImporterFailureDiagnostic);
        g_wickedModelImporterFailureDiagnostic.clear();
        return diagnostic;
    }
}
