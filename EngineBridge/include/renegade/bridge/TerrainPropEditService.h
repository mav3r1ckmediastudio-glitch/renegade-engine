#pragma once
#include "renegade/bridge/TerrainPropDocument.h"
#include "renegade/bridge/CommandService.h"
#include <functional>

namespace renegade::bridge
{
    // Lifetime belongs to one open document. Call mutations only at the same
    // native scene-safe point used by ordinary editor commands.
    class TerrainPropEditService : public std::enable_shared_from_this<TerrainPropEditService>
    {
    public:
        TerrainPropEditService(wi::scene::Scene&, std::filesystem::path projectRoot,
            StableId projectId, wi::ecs::Entity layerOwner);
        ~TerrainPropEditService();
        bool Read(TerrainPropDocument&, std::string& error);
        std::string Revision() const;
        // One command per staged stroke/Generate Apply. The revision token is
        // checked again at Execute, Undo and Redo; stale edits never overwrite.
        std::unique_ptr<ICommand> MakeCommand(const TerrainPropDocument& after,
            const std::string& expectedRevision, std::function<bool()> cancelled = {});
        const std::string& LastError() const noexcept { return error_; }
        void SetTransactionOptions(ProjectDocumentTransactionOptions options) { options_ = std::move(options); }
    private:
        class EditCommand;
        bool Apply(TerrainPropDocument, const std::string& expectedRevision,
            const std::function<bool()>& cancelled);
        bool PrepareProjection(const TerrainPropDocument&, std::string& error);
        wi::scene::Scene& scene_;
        std::filesystem::path root_;
        StableId project_;
        wi::ecs::Entity owner_;
        StableId freshLayer_;
        std::uint64_t highWater_ = 1;
        std::unique_ptr<TerrainPropProjection> projection_;
        TerrainPropDocument displayed_;
        std::string projectionRevision_;
        std::string error_;
        ProjectDocumentTransactionOptions options_;
    };
}
