#include "renegade/bridge/ImpactAudioService.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include "json.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

namespace renegade::bridge
{
    namespace fs = std::filesystem;
    namespace
    {
        std::uint32_t Le32(const std::uint8_t* p)
        { return p[0] | (std::uint32_t(p[1])<<8) | (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24); }
        unsigned Le16(const std::uint8_t* p) { return p[0] | (unsigned(p[1])<<8); }
        bool ReadBankFile(const std::string& path, const StableId& projectId,
            ImpactAudioBank& bank, std::string& error)
        {
            bank = {}; bank.projectId = projectId;
            std::ifstream in(fs::u8path(path), std::ios::binary);
            if (!in) { error = "Impact audio bank is unreadable."; return false; }
            auto j = nlohmann::json::parse(in, nullptr, false);
            try
            {
                if (j.is_discarded() || j.at("format") != "renegade-impact-audio" ||
                    j.at("schema_version") != 1 || j.at("project_id") != projectId ||
                    !j.at("surfaces").is_object())
                    throw std::runtime_error("identity/schema");
                for (auto it = j.at("surfaces").begin(); it != j.at("surfaces").end(); ++it)
                {
                    ImpactSurfaceType type;
                    if (!ParseImpactSurfaceType(it.key(), type) || it.key().empty() ||
                        it.key() != ImpactSurfaceTypeToken(type))
                        throw std::runtime_error("surface");
                    bank.surfaces[static_cast<unsigned>(type)] = it.value().get<std::vector<StableId>>();
                }
                return ValidateImpactAudioBank(bank, error);
            }
            catch (const std::exception&) { error = "Impact audio bank has invalid schema, identity or bindings."; return false; }
        }
    }

    bool ValidateImpactAudioBank(const ImpactAudioBank& bank, std::string& error)
    {
        if (!IsValidStableId(bank.projectId)) { error = "Invalid impact audio project identity."; return false; }
        for (const auto& ids : bank.surfaces)
        {
            std::set<StableId> unique;
            if (ids.size() > 8) { error = "Impact audio supports at most eight variants per surface."; return false; }
            for (const auto& id : ids)
                if (!IsValidStableId(id) || !unique.insert(id).second)
                { error = "Invalid or duplicate impact sound identity."; return false; }
        }
        error.clear(); return true;
    }

    bool ReadImpactAudioBank(const std::string& root, const StableId& projectId,
        ImpactAudioBank& bank, std::string& error)
    {
        bank = {}; bank.projectId = projectId;
        const auto path = ResolveDependencyPath(root, ImpactAudioBankPath);
        if (!path.accepted) { error = path.error; return false; }
        if (!path.exists) { error.clear(); return true; } // Old projects stay silent.
        return ReadBankFile(path.absolutePath, projectId, bank, error);
    }

    bool WriteImpactAudioBank(const std::string& root, const ImpactAudioBank& bank, std::string& error)
    {
        if (!ValidateImpactAudioBank(bank, error)) return false;
        const auto path = ResolveDependencyPath(root, ImpactAudioBankPath);
        if (!path.accepted) { error = path.error; return false; }
        nlohmann::json j = {{"format","renegade-impact-audio"},{"schema_version",1},
            {"project_id",bank.projectId},{"surfaces",nlohmann::json::object()}};
        for (unsigned i=0; i<bank.surfaces.size(); ++i)
            if (!bank.surfaces[i].empty())
                j["surfaces"][ImpactSurfaceTypeToken(static_cast<ImpactSurfaceType>(i))] = bank.surfaces[i];
        const auto text = j.dump(2) + "\n";
        ProjectDocumentWrite write;
        write.destinationPath = path.absolutePath;
        write.content.assign(text.begin(),text.end());
        write.validator = [id=bank.projectId](const std::string& p, std::string& e)
        { ImpactAudioBank check; return ReadBankFile(p,id,check,e); };
        ProjectDocumentTransactionOptions options;
        options.allowedRoot = fs::absolute(fs::u8path(root)).generic_u8string();
        options.journalDirectory = (fs::u8path(root)/"Intermediate"/"Transactions").generic_u8string();
        auto result = ProjectDocumentTransaction().Execute({std::move(write)},options);
        if (!result.success) { error = result.message; return false; }
        error.clear(); return true;
    }

    bool ValidateImpactAudioPayload(const std::vector<std::uint8_t>& p, std::string& error)
    {
        error = "Impact audio requires a bounded PCM16 mono/stereo WAV.";
        if (p.size()<44 || p.size()>16*1024*1024 || std::memcmp(p.data(),"RIFF",4) ||
            std::memcmp(p.data()+8,"WAVE",4) || std::uint64_t(Le32(p.data()+4))+8 != p.size()) return false;
        bool fmt=false, data=false; unsigned align=0, rate=0;
        for (std::size_t at=12; at<p.size();)
        {
            if (p.size()-at<8) return false;
            const auto size=Le32(p.data()+at+4);
            if (size>p.size()-at-8) return false;
            const auto* body=p.data()+at+8;
            if (!std::memcmp(p.data()+at,"fmt ",4))
            {
                if (fmt || (size!=16 && size!=18) || Le16(body)!=1 ||
                    (Le16(body+2)!=1 && Le16(body+2)!=2) || Le16(body+14)!=16) return false;
                align=Le16(body+2)*2; rate=Le32(body+4);
                if (rate<8000 || rate>192000 || Le16(body+12)!=align || Le32(body+8)!=rate*align ||
                    (size==18 && Le16(body+16)!=0)) return false;
                fmt=true;
            }
            else if (!std::memcmp(p.data()+at,"data",4))
            {
                if (!fmt || data || size==0 || size%align || size>rate*align*10ull) return false;
                data=true;
            }
            const std::uint64_t next=std::uint64_t(at)+8+size+(size&1);
            if (next>p.size()) return false;
            at=static_cast<std::size_t>(next);
        }
        if (!fmt || !data) return false;
        error.clear(); return true;
    }

    bool PrepareImpactAudioAsset(const std::string& root, const std::string& packageRoot,
        const StableId& projectId, const StableId& id, std::vector<std::uint8_t>& payload,
        std::string& error)
    {
        payload.clear();
        if (!IsValidStableId(projectId) || !IsValidStableId(id))
        { error="Invalid impact audio identity."; return false; }
        if (!packageRoot.empty())
        {
            PackagedResourceAsset asset;
            if (!PreparePackagedResourceAsset(packageRoot,projectId,id,asset,error)) return false;
            if (asset.resourceClass!=ResourceClass::Audio || asset.sourceFormat!=ResourceSourceFormat::Wav)
            { error="Impact sound is not a packaged WAV audio asset."; return false; }
            payload=std::move(asset.payload);
        }
        else
        {
            AssetRegistry registry;
            if (!ReadAssetRegistry(root,projectId,registry,error)) return false;
            auto record=std::find_if(registry.records.begin(),registry.records.end(),
                [&](const AssetRecord& r){return r.assetId==id;});
            auto provenance=std::find_if(registry.importedProducts.begin(),registry.importedProducts.end(),
                [&](const ImportedProductRecord& r){return r.productAssetId==id;});
            if (record==registry.records.end() || provenance==registry.importedProducts.end() ||
                record->dependencyClass!=DependencyClass::Audio || record->provider!="lp08.rasset" ||
                record->providerVersion!=1 || record->requirement!=DependencyRequirement::Required ||
                !record->sourceAvailable || provenance->importer!="wicked.resourcemanager" ||
                provenance->importerVersion!=1)
            { error="Impact sound lacks an available governed audio product/provenance."; return false; }
            const auto path=ResolveDependencyPath(root,record->projectRelativePath);
            if (!path.accepted || !path.exists || record->projectRelativePath.rfind("Content/",0)!=0)
            { error="Impact sound product is missing or outside Content."; return false; }
            ResourceAssetDocument asset;
            if (!ReadResourceAssetDocument(path.absolutePath,asset,error)) return false;
            if (asset.manifest.projectId!=projectId || asset.manifest.assetId!=id ||
                asset.manifest.sourceAssetId!=provenance->sourceAssetId ||
                asset.manifest.resourceClass!=ResourceClass::Audio ||
                asset.manifest.sourceFormat!=ResourceSourceFormat::Wav)
            { error="Impact sound product contradicts governed identity/class."; return false; }
            payload=std::move(asset.payload);
        }
        if (!ValidateImpactAudioPayload(payload,error)) { payload.clear(); return false; }
        return true;
    }

    bool AddImpactAudioDependencies(const std::string& root, const StableId& projectId,
        DependencyCollector& collector, std::string& error)
    {
        ImpactAudioBank bank;
        if (!ReadImpactAudioBank(root,projectId,bank,error)) return false;
        const auto path=ResolveDependencyPath(root,ImpactAudioBankPath);
        if (!path.exists) return true;
        if (!collector.AddRoot({ImpactAudioBankPath,DependencyClass::Data,
            DependencyRequirement::Required,"renegade:impact-audio-bank"},error)) return false;
        AssetRegistry registry;
        if (!ReadAssetRegistry(root,projectId,registry,error)) return false;
        std::set<StableId> added;
        for (const auto& ids:bank.surfaces) for (const auto& id:ids)
        {
            if (!added.insert(id).second) continue;
            std::vector<std::uint8_t> payload;
            if (!PrepareImpactAudioAsset(root,"",projectId,id,payload,error)) return false;
            auto record=std::find_if(registry.records.begin(),registry.records.end(),
                [&](const AssetRecord& r){return r.assetId==id;});
            if (!collector.AddRoot({record->projectRelativePath,DependencyClass::Audio,
                DependencyRequirement::Required,"lp08.rasset"},error)) return false;
        }
        return true;
    }

    bool SnapshotImpactAudio(const std::string& root, const std::string& snapshot,
        const StableId& projectId, std::string& error)
    {
        ImpactAudioBank bank;
        if (!ReadImpactAudioBank(root,projectId,bank,error)) return false;
        const auto bankPath=ResolveDependencyPath(root,ImpactAudioBankPath);
        if (!bankPath.exists) return true;
        AssetRegistry registry;
        if (!ReadAssetRegistry(root,projectId,registry,error)) return false;
        for (const auto& ids:bank.surfaces) for (const auto& id:ids)
        {
            std::vector<std::uint8_t> payload;
            if (!PrepareImpactAudioAsset(root,"",projectId,id,payload,error)) return false;
            auto record=std::find_if(registry.records.begin(),registry.records.end(),
                [&](const AssetRecord& r){return r.assetId==id;});
            const auto source=ResolveDependencyPath(root,record->projectRelativePath);
            const auto destination=ResolveDependencyPath(snapshot,record->projectRelativePath);
            if (!destination.accepted) { error=destination.error; return false; }
            std::error_code ec;
            fs::create_directories(fs::u8path(destination.absolutePath).parent_path(),ec);
            if (!ec) fs::copy_file(fs::u8path(source.absolutePath),fs::u8path(destination.absolutePath),
                fs::copy_options::overwrite_existing,ec);
            if (ec) { error="Could not snapshot impact sound: "+ec.message(); return false; }
        }
        const auto source=ResolveDependencyPath(root,AssetRegistryDocumentName);
        const auto destination=ResolveDependencyPath(snapshot,AssetRegistryDocumentName);
        if (!source.accepted || !destination.accepted) { error="Invalid impact registry snapshot path."; return false; }
        std::error_code ec;
        fs::copy_file(fs::u8path(source.absolutePath),fs::u8path(destination.absolutePath),
            fs::copy_options::overwrite_existing,ec);
        if (ec) { error="Could not snapshot impact registry: "+ec.message(); return false; }
        return WriteImpactAudioBank(snapshot,bank,error);
    }

    bool ImpactAudioPlayer::Prepare(const std::string& root, const std::string& packageRoot,
        const StableId& projectId, std::string& error, bool useCoreDefaults)
    {
        Reset();
        ImpactAudioBank bank;
        if (!ReadImpactAudioBank(root,projectId,bank,error)) return false;
        std::array<std::vector<Clip>,9> loaded;
        for (unsigned i=0; i<bank.surfaces.size(); ++i) for (const auto& id:bank.surfaces[i])
        {
            std::vector<std::uint8_t> payload;
            if (!PrepareImpactAudioAsset(root,packageRoot,projectId,id,payload,error)) return false;
            Clip clip; clip.id=id;
            if (!wi::audio::CreateSound(payload.data(),payload.size(),&clip.sound))
            { error="Wicked could not decode governed impact sound."; return false; }
            loaded[i].push_back(std::move(clip));
        }
        // Built-in owner-authored WAVs are packaged in the executable itself.
        // A project-owned bank wins on each populated surface; no silent overrides.
        if (useCoreDefaults)
        {
#ifdef _WIN32
            struct CoreClip { unsigned surface, resourceId; const char* label; };
            static constexpr CoreClip core[] = {
                {1,7400,"metal1"},{1,7401,"metal2"},
                {2,7402,"wood1"},{2,7403,"wood2"},
                {3,7404,"concrete1"},{3,7405,"concrete2"},
                {4,7406,"stone1"},{4,7407,"stone2"},
                {5,7408,"dirt1"},{5,7409,"dirt2"},
                {6,7410,"glass1"},{6,7411,"glass2"},
                {7,7412,"water1"},{7,7413,"water2"},
            };
            const auto module=GetModuleHandleW(nullptr);
            if (!module) {error="Could not access embedded impact audio module.";return false;}
            for (const auto& item : core)
            {
                if (!bank.surfaces[item.surface].empty()) continue;
                const auto found=FindResourceW(module,MAKEINTRESOURCEW(item.resourceId),RT_RCDATA);
                if (!found) {error="Missing original built-in impact audio: "+
                    std::string(item.label);return false;}
                const auto length=SizeofResource(module,found);
                const auto data=LockResource(LoadResource(module,found));
                if (!data || length==0)
                {error="Unreadable built-in impact audio: "+std::string(item.label);return false;}
                const auto begin=static_cast<const std::uint8_t*>(data);
                const std::vector<std::uint8_t> payload(begin,begin+length);
                if (!ValidateImpactAudioPayload(payload,error))
                {error="Invalid built-in impact audio "+std::string(item.label)+": "+error;return false;}
                Clip clip;clip.id="renegade.core.impact."+std::string(item.label);
                if (!wi::audio::CreateSound(payload.data(),payload.size(),&clip.sound))
                {error="Wicked could not decode built-in impact sound: "+
                    std::string(item.label);return false;}
                loaded[item.surface].push_back(std::move(clip));
            }
#else
            error="Original built-in impact SFX currently require a Windows Runtime.";
            return false;
#endif
        }
        clips_=std::move(loaded); error.clear(); return true;
    }
    bool ImpactAudioPlayer::Play(ImpactSurfaceType surface, const XMFLOAT3& position,
        const wi::audio::SoundInstance3D& listener, StableId& played)
    {
        played.clear();
        if (paused_ || !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
            return false;
        unsigned index=static_cast<unsigned>(surface);
        if (index>=clips_.size()) return false;
        if (clips_[index].empty()) index=0; // Only an explicitly provided Default is a fallback.
        auto& clips=clips_[index]; if (clips.empty()) return false;
        int choice=0;
        if (clips.size()>1)
        {
            std::uniform_int_distribution<int> distribution(0,static_cast<int>(clips.size())-2);
            choice=distribution(random_);
            if (previous_[index]<0) choice=std::uniform_int_distribution<int>(0,static_cast<int>(clips.size())-1)(random_);
            else if (choice>=previous_[index]) ++choice;
        }
        Voice voice; voice.position=position;
        voice.instance.type=wi::audio::SUBMIX_TYPE_SOUNDEFFECT;
        voice.instance.SetLooped(false);
        if (!wi::audio::CreateSoundInstance(&clips[choice].sound,&voice.instance)) return false;
        if (voices_.size()>=MaxVoices) { wi::audio::Stop(&voices_.front().instance); voices_.erase(voices_.begin()); }
        auto spatial=listener; spatial.emitterPos=position; spatial.emitterRadius=0.2f;
        wi::audio::Update3D(&voice.instance,spatial);
        wi::audio::SetVolume(0.65f,&voice.instance);
        wi::audio::Play(&voice.instance);
        voices_.push_back(std::move(voice)); previous_[index]=choice; played=clips[choice].id;
        return true;
    }
    void ImpactAudioPlayer::Update(float dt, const wi::audio::SoundInstance3D& listener)
    {
        if (paused_) return;
        const auto delta=std::isfinite(dt)?std::max(0.0f,dt):0.0f;
        for (auto it=voices_.begin(); it!=voices_.end();)
        {
            it->age+=delta;
            if (it->age>10.0f || wi::audio::IsEnded(&it->instance))
            { wi::audio::Stop(&it->instance); it=voices_.erase(it); continue; }
            auto spatial=listener; spatial.emitterPos=it->position; spatial.emitterRadius=0.2f;
            wi::audio::Update3D(&it->instance,spatial); ++it;
        }
    }
    void ImpactAudioPlayer::SetPaused(bool paused)
    {
        if (paused_==paused) return;
        paused_=paused;
        for (auto& voice:voices_)
            if (paused) wi::audio::Pause(&voice.instance); else wi::audio::Play(&voice.instance);
    }
    void ImpactAudioPlayer::StopVoices()
    {
        for (auto& voice:voices_) wi::audio::Stop(&voice.instance);
        voices_.clear();
    }
    void ImpactAudioPlayer::Reset()
    {
        StopVoices(); for (auto& clips:clips_) clips.clear();
        previous_.fill(-1); paused_=false;
    }
    std::size_t ImpactAudioPlayer::ClipCount() const
    { std::size_t count=0; for (const auto& clips:clips_) count+=clips.size(); return count; }
}
