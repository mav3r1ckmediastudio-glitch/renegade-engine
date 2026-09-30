#include "renegade/bridge/ModelImportCandidateService.h"
#include "renegade/bridge/ModelImporterFailureAdapter.h"

#include <ModelImporter.h>

#include <filesystem>
#include <exception>
#include <fstream>

namespace
{
    bool FingerprintFile(const std::string& path, std::uint64_t& bytes,
        std::uint64_t& fingerprint)
    {
        std::ifstream stream(std::filesystem::u8path(path), std::ios::binary);
        if (!stream) return false;
        bytes = 0;
        fingerprint = 1469598103934665603ull;
        char buffer[64 * 1024];
        while (stream)
        {
            stream.read(buffer, sizeof(buffer));
            const std::streamsize count = stream.gcount();
            for (std::streamsize index = 0; index < count; ++index)
            {
                fingerprint ^= static_cast<unsigned char>(buffer[index]);
                fingerprint *= 1099511628211ull;
            }
            bytes += static_cast<std::uint64_t>(count);
        }
        return stream.eof() && bytes != 0;
    }
}

namespace renegade::bridge
{
    ModelImportCandidate ModelImportCandidateService::PrepareGlb(
        const std::string& sourcePath) const
    {
        ModelImportCandidate candidate;
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path source = fs::weakly_canonical(
            fs::absolute(fs::u8path(sourcePath), ec), ec);
        if (ec || !fs::is_regular_file(source, ec) || ec)
        {
            candidate.error_ = "Selected GLB source does not exist.";
            return candidate;
        }
        candidate.sourcePath_ = source.generic_u8string();
        if (ImportService::ClassifyModelSourceFormat(candidate.sourcePath_) !=
            ModelSourceFormat::Glb)
        {
            candidate.error_ = "This first model import gate accepts GLB files only.";
            return candidate;
        }
        if (wi::graphics::GetDevice() == nullptr)
        {
            candidate.error_ = "Model conversion requires an initialized graphics device.";
            return candidate;
        }
        if (!FingerprintFile(candidate.sourcePath_, candidate.sourceBytes_,
                candidate.sourceFingerprint_))
        {
            candidate.error_ = "Selected GLB source could not be read.";
            return candidate;
        }

        candidate.scene_ = wi::allocator::make_shared_single<wi::scene::Scene>();
        ClearWickedModelImporterFailureDiagnostic();
        try
        {
            ImportModel_GLTF(candidate.sourcePath_, *candidate.scene_);
        }
        catch (const std::exception& exception)
        {
            candidate.error_ = std::string("GLB conversion failed: ") + exception.what();
        }
        catch (...)
        {
            candidate.error_ = "GLB conversion failed with an unknown error.";
        }
        const std::string importerFailure = ConsumeWickedModelImporterFailureDiagnostic();
        if (!importerFailure.empty() && candidate.error_.empty())
            candidate.error_ = "GLB converter reported: " + importerFailure;
        if (!candidate.error_.empty())
        {
            candidate.scene_.reset();
            return candidate;
        }
        std::uint64_t afterBytes = 0;
        std::uint64_t afterFingerprint = 0;
        if (!FingerprintFile(candidate.sourcePath_, afterBytes, afterFingerprint) ||
            afterBytes != candidate.sourceBytes_ ||
            afterFingerprint != candidate.sourceFingerprint_)
        {
            candidate.error_ = "GLB source changed during conversion.";
            candidate.scene_.reset();
            return candidate;
        }

        candidate.summary_ = ImportService::Summarize(*candidate.scene_);
        candidate.evidence_ = ImportService::SummarizeModelEvidence(*candidate.scene_);
        if (candidate.summary_.meshes == 0 || candidate.summary_.objects == 0)
        {
            candidate.error_ = "GLB conversion produced no mesh and object content.";
            candidate.scene_.reset();
        }
        return candidate;
    }
}
