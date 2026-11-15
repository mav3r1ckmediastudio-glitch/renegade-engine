#include "renegade/bridge/DialogueAssetService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include <filesystem>
#include <fstream>
namespace renegade::bridge
{
namespace fs = std::filesystem;
std::string DialogueAssetPath(const std::string &root, const std::string &id)
{
    if (!IsValidStableId(id))
        return {};
    return (fs::u8path(root) / "Content/Dialogue" / (id + ".rdialogue")).generic_u8string();
}
bool LoadDialogueAsset(const std::string &root, const std::string &project, const std::string &id,
                       DialogueDocument &out, std::string &e)
{
    const auto path = DialogueAssetPath(root, id);
    if (path.empty())
    {
        e = "Invalid dialogue identity.";
        return false;
    }
    std::error_code ec;
    auto canonical = fs::weakly_canonical(fs::u8path(path), ec);
    auto base = fs::weakly_canonical(fs::u8path(root), ec);
    auto rel = canonical.lexically_relative(base);
    if (ec || rel.empty() || *rel.begin() == ".." || fs::file_size(canonical, ec) > 8 * 1024 * 1024 || ec)
    {
        e = "Dialogue file is missing, oversized or outside the project.";
        return false;
    }
    std::ifstream in(canonical, std::ios::binary);
    std::string b((std::istreambuf_iterator<char>(in)), {});
    DialogueDocument d;
    if (!DecodeDialogue(b, d, e))
        return false;
    if (d.projectId != project || d.id != id)
    {
        e = "Dialogue belongs to a different project or identity.";
        return false;
    }
    out = std::move(d);
    return true;
}
bool SaveDialogueAsset(const std::string &root, const DialogueDocument &d, std::string &e)
{
    if (!IsValidStableId(d.projectId) || !IsValidStableId(d.id) || !ValidateDialogue(d, e))
    {
        if (e.empty())
            e = "Open a project before saving dialogue.";
        return false;
    }
    auto b = EncodeDialogue(d);
    ProjectDocumentWrite w;
    w.destinationPath = DialogueAssetPath(root, d.id);
    w.content.assign(b.begin(), b.end());
    ProjectDocumentTransactionOptions o;
    o.allowedRoot = root;
    o.journalDirectory = (fs::u8path(root) / "Saved/Transactions").generic_u8string();
    auto r = ProjectDocumentTransaction().Execute({std::move(w)}, o);
    if (!r.success)
    {
        e = r.message;
        return false;
    }
    DialogueDocument check;
    return LoadDialogueAsset(root, d.projectId, d.id, check, e);
}
std::vector<DialogueDocument> ListDialogueAssets(const std::string &root, const std::string &project, std::string &e)
{
    std::vector<DialogueDocument> out;
    std::error_code ec;
    auto folder = fs::u8path(root) / "Content/Dialogue";
    if (!fs::exists(folder, ec))
        return out;
    for (fs::directory_iterator it(folder, ec), end; it != end && !ec; it.increment(ec))
    {
        if (it->path().extension() != ".rdialogue")
            continue;
        DialogueDocument d;
        std::string err;
        if (LoadDialogueAsset(root, project, it->path().stem().u8string(), d, err))
            out.push_back(std::move(d));
        else
            e = err;
    }
    if (ec)
        e = ec.message();
    return out;
}
std::string AssignedDialogue(const wi::scene::Scene &s, wi::ecs::Entity entity)
{
    auto *m = s.metadatas.GetComponent(entity);
    return m && m->string_values.has(DialogueMetadataKey) ? m->string_values.get(DialogueMetadataKey) : "";
}
AssignDialogueCommand::AssignDialogueCommand(wi::scene::Scene &s, wi::ecs::Entity entity, std::string id)
    : scene_(&s), entity_(entity), before_(AssignedDialogue(s, entity)), after_(std::move(id))
{
}
void AssignDialogueCommand::Apply(const std::string &id)
{
    auto &m = scene_->metadatas.Create(entity_);
    if (id.empty())
        m.string_values.erase(DialogueMetadataKey);
    else
        m.string_values.set(DialogueMetadataKey, id);
}
bool AssignDialogueCommand::Execute()
{
    if (!scene_->transforms.Contains(entity_) || (!after_.empty() && !IsValidStableId(after_)))
        return false;
    Apply(after_);
    return true;
}
void AssignDialogueCommand::Undo()
{
    Apply(before_);
}
} // namespace renegade::bridge
