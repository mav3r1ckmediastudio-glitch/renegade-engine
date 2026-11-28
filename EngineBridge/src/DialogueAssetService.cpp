#include "renegade/bridge/DialogueAssetService.h"
#include "renegade/bridge/IdentityService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "renegade/bridge/AudioService.h"
#include "PcmWaveImport.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
namespace renegade::bridge
{
namespace fs = std::filesystem;
static bool ImportProjectAudio(const std::string &root, const std::string &source, const char* folder, std::string &relative, std::string &error)
{
    relative.clear();
    error.clear();
    if(source.empty())return false;
    std::vector<std::uint8_t> normalised;
    if(!ValidateAudioAssetForWicked(source,error))
    {
        if(std::string(folder)!="Content/Audio/UI/"||!detail::NormaliseUiPcmWave(source,normalised))return false;
        error.clear();
    }
    std::error_code ec;
    const auto base = fs::weakly_canonical(fs::u8path(root), ec);
    if (ec || root.empty()) { error = "Open a project before importing a voice clip."; return false; }
    const auto file = fs::canonical(fs::u8path(source), ec);
    if (ec) { error = "Could not read the selected voice clip."; return false; }
    const auto existing = file.lexically_relative(base).generic_u8string();
    if (normalised.empty() && existing.rfind("Content/Audio/", 0) == 0 && IsDialogueVoicePath(existing))
    {
        relative = existing;
        return true;
    }
    const auto size = fs::file_size(file, ec);
    if (ec || size > 128ull * 1024 * 1024) { error = "Choose a voice clip smaller than 128 MB."; return false; }
    auto stem = file.stem().u8string();
    for (auto &c : stem) if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') c = '_';
    stem.resize(std::min(size_t(60), stem.size()));
    auto extension = file.extension().u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    const auto path = std::string(folder) == "Content/Audio/UI/" ?
        std::string(folder) + GenerateStableId() + "/" + file.filename().u8string() :
        std::string(folder) + stem + "-" + GenerateStableId() + extension;
    std::ifstream in(file, std::ios::binary);
    ProjectDocumentWrite write;
    write.destinationPath = (base / fs::u8path(path)).generic_u8string();
    if(!normalised.empty())write.content=std::move(normalised);
    else
    {
        write.content.resize(size_t(size));
        if (!in.read(reinterpret_cast<char *>(write.content.data()), std::streamsize(size)))
        { error = "Could not read the complete voice clip."; return false; }
    }
    ProjectDocumentTransactionOptions options;
    options.allowedRoot = root;
    options.journalDirectory = (base / "Saved/Transactions").generic_u8string();
    const auto result = ProjectDocumentTransaction().Execute({std::move(write)}, options);
    if (!result.success) { error = result.message; return false; }
    relative = path;
    return true;
}
bool ImportDialogueVoice(const std::string& root,const std::string& source,std::string& relative,std::string& error)
{
    return ImportProjectAudio(root,source,"Content/Audio/Dialogue/",relative,error);
}
bool ImportUiClickSound(const std::string& root,const std::string& source,std::string& relative,std::string& error)
{
    return ImportProjectAudio(root,source,"Content/Audio/UI/",relative,error);
}
bool ResolveDialogueVoicePath(const std::string &root, const std::string &relative, std::string &absolute, std::string &error)
{
    absolute.clear();
    if (relative.empty()) return true;
    if (!IsDialogueVoicePath(relative)) { error = "Choose a project audio clip in Content."; return false; }
    std::error_code ec;
    const auto base = fs::weakly_canonical(fs::u8path(root), ec);
    if (ec) { error = "Could not resolve the project audio folder."; return false; }
    const auto file = fs::weakly_canonical(base / fs::u8path(relative), ec);
    const auto rel = file.lexically_relative(base);
    if (ec || rel.empty() || *rel.begin() == ".." || !fs::is_regular_file(file, ec) || ec)
    { error = "Voice clip is missing or outside this project: " + relative; return false; }
    absolute = file.generic_u8string();
    return true;
}
bool SnapshotDialogueVoices(const std::string &sourceRoot, const std::string &destinationRoot,
                            const DialogueDocument &document, std::string &error)
{
    for (const auto &line : document.lines)
    {
        if (line.voice.empty()) continue;
        std::string source;
        if (!ResolveDialogueVoicePath(sourceRoot, line.voice, source, error)) return false;
        std::error_code ec;
        const auto destinationBase = fs::weakly_canonical(fs::u8path(destinationRoot), ec);
        if (ec) { error = "Could not resolve voice snapshot folder."; return false; }
        const auto destination = fs::weakly_canonical(destinationBase / fs::u8path(line.voice), ec);
        const auto relative = destination.lexically_relative(destinationBase);
        if (ec || relative.empty() || *relative.begin() == "..")
        { error = "Voice snapshot path leaves the destination project."; return false; }
        fs::create_directories(destination.parent_path(), ec);
        if (!ec) fs::copy_file(fs::u8path(source), destination, fs::copy_options::overwrite_existing, ec);
        if (ec) { error = "Could not snapshot voice clip: " + ec.message(); return false; }
    }
    return true;
}
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
    auto *existing = scene_->metadatas.GetComponent(entity_);
    auto &m = existing ? *existing : scene_->metadatas.Create(entity_);
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
