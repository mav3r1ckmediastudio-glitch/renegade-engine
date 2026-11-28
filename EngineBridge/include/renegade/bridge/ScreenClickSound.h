#pragma once
#include "renegade/bridge/AudioService.h"
#include "renegade/bridge/ProjectDocumentTransaction.h"
#include "renegade/bridge/ScreenService.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace renegade::bridge
{
    // Missing entry inherits. An explicitly empty clip silences just this button.
    struct ScreenClickSounds
    {
        using Key = std::pair<StableId, StableId>;
        std::string projectDefault;
        std::map<Key,std::string> buttons;
        std::string readError;

        std::string Resolve(const Key& key) const
        {
            const auto it = buttons.find(key);
            return it == buttons.end() ? projectDefault : it->second;
        }
        static std::filesystem::path Path(const std::string& root)
        {
            return std::filesystem::u8path(root) / "Content/UI/ButtonSounds.rui-audio";
        }
        static ScreenClickSounds Read(const std::string& root)
        {
            ScreenClickSounds settings;
            auto path = Path(root);
            std::error_code ec;
            if (!std::filesystem::exists(path,ec) && !ec) return settings;
            std::ifstream input(path);
            std::string header;
            if (std::getline(input,header) && header=="renegade-button-sounds 1" &&
                input >> std::quoted(settings.projectDefault))
            {
                Key key;
                std::string clip;
                while (input >> std::ws && !input.eof())
                {
                    if (!(input >> std::quoted(key.first) >> std::quoted(key.second) >> std::quoted(clip)) ||
                        !settings.buttons.emplace(key,clip).second) break;
                }
                if (input.eof() && !input.fail()) return settings;
            }
            settings = {};
            settings.readError = "Could not read button sound settings.";
            return settings;
        }
        bool Save(const std::string& root,std::string& error) const
        {
            if (root.empty() || !readError.empty()) { error="Button sound settings unavailable."; return false; }
            std::ostringstream out;
            out << "renegade-button-sounds 1\n" << std::quoted(projectDefault) << '\n';
            for (const auto& entry : buttons)
                out << std::quoted(entry.first.first) << ' ' << std::quoted(entry.first.second)
                    << ' ' << std::quoted(entry.second) << '\n';
            const auto text=out.str();
            ProjectDocumentWrite write;
            write.destinationPath=Path(root).generic_u8string();
            write.content.assign(text.begin(),text.end());
            ProjectDocumentTransactionOptions options;
            options.allowedRoot=root;
            options.journalDirectory=(std::filesystem::u8path(root)/"Saved/Transactions").generic_u8string();
            const auto result=ProjectDocumentTransaction().Execute({std::move(write)},options);
            error=result.message;
            return result.success;
        }
    };

    inline ScreenClickSounds& ProjectClickSounds(const std::string& root)
    {
        static std::map<std::string,ScreenClickSounds> projects;
        const auto key=std::filesystem::u8path(root).lexically_normal().generic_u8string();
        const auto inserted=projects.try_emplace(key);
        if (inserted.second) inserted.first->second=ScreenClickSounds::Read(key);
        return inserted.first->second;
    }

    inline bool PlayScreenClick(const std::string& root,const std::string& clip,std::string& error)
    {
        if (clip.empty()) return true;
        std::string path;
        if (!ResolveScreenResourcePath(root,clip,path,error)) return false;
        // Keep the voice alive when the click immediately navigates to another screen.
        static std::string previous;
        static wi::Resource resource;
        static wi::audio::SoundInstance voice;
        if (previous!=path || !resource.IsValid())
        {
            if (!ValidateAudioAssetForWicked(path,error)) return false;
            auto next=wi::resourcemanager::Load(path);
            if (!next.IsValid() || !next.GetSound().IsValid()) {error="Could not load sound.";return false;}
            resource=std::move(next);
            previous=path;
        }
        wi::audio::SoundInstance next;
        next.type=wi::audio::SUBMIX_TYPE_SOUNDEFFECT;
        next.SetLooped(false);
        if (!wi::audio::CreateSoundInstance(&resource.GetSound(),&next)) {error="Audio preview unavailable.";return false;}
        voice=std::move(next);
        wi::audio::Play(&voice);
        return true;
    }
}
