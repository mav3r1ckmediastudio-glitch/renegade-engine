#pragma once
// Player-owned; never persisted into a project, Screen or package dependency.
#include <Windows.h>
#include "renegade/bridge/ScreenAccessibility.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <sstream>
#include <locale>
#include <iomanip>

namespace renegade::runtime
{
    struct RuntimeUiAccessibilityPreferences
    {
        bool reducedMotion=false;
        bridge::ScreenAccessibilitySettings presentation;

        static bool Load(const std::filesystem::path& path,
            RuntimeUiAccessibilityPreferences& out,std::string& error)
        {
            if(path.empty()){error="UI accessibility preference path is empty.";return false;}
            std::error_code ec;
            if(!std::filesystem::exists(path,ec))
            {
                if(ec){error="Could not inspect UI preference file: "+ec.message();return false;}
                out={};error.clear();return true;
            }
            std::ifstream stream(path,std::ios::binary);
            if(!stream){error="Could not read player UI accessibility preferences.";return false;}
            RuntimeUiAccessibilityPreferences candidate;
            const auto line=[&](std::string& value)
            {
                if(!std::getline(stream,value)||value.size()>128)return false;
                if(!value.empty()&&value.back()=='\r')value.pop_back();
                return true;
            };
            std::string version,motion;
            if(!line(version)||!line(motion)||
                (motion!="reduced_motion=0"&&motion!="reduced_motion=1")||
                (version!="renegade_ui_preferences=1"&&version!="renegade_ui_preferences=2"&&version!="renegade_ui_preferences=3"))
            {error="Invalid or unsupported player UI accessibility preferences.";return false;}
            candidate.reducedMotion=motion=="reduced_motion=1";
            if(version=="renegade_ui_preferences=2"||version=="renegade_ui_preferences=3")
            {
                const auto number=[&](const char* key,auto& output)
                {
                    std::string value;
                    if(!line(value)||value.rfind(key,0)!=0)return false;
                    std::istringstream parser(value.substr(std::char_traits<char>::length(key)));
                    parser.imbue(std::locale::classic());
                    parser>>std::noskipws>>output;
                    return !parser.fail()&&parser.peek()==std::char_traits<char>::eof();
                };
                if(!number("text_scale=",candidate.presentation.textScale)||
                    !number("contrast=",candidate.presentation.contrast)||
                    !number("focus_style=",candidate.presentation.focusStyle)||
                    !number("focus_width=",candidate.presentation.focusWidth)||
                    (version=="renegade_ui_preferences=3"&&
                        (!number("ui_scale=",candidate.presentation.uiScale)||
                         !number("minimum_target_size=",candidate.presentation.minimumTargetSize)))||
                    !bridge::ValidateScreenAccessibilitySettings(candidate.presentation))
                {error="Invalid player UI accessibility settings.";return false;}
            }
            if(stream.peek()!=std::char_traits<char>::eof())
            {error="Unexpected data in player UI accessibility preferences.";return false;}
            out=candidate;error.clear();return true;
        }

        bool Save(const std::filesystem::path& path,std::string& error) const
        {
            if(path.empty()){error="UI accessibility preference path is empty.";return false;}
            if(!bridge::ValidateScreenAccessibilitySettings(presentation))
            {error="Invalid player UI accessibility settings.";return false;}
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(),ec);
            if(ec){error="Could not prepare UI accessibility preferences: "+ec.message();return false;}
            auto temporary=path;
            temporary+=L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64());
            {
                std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);
                if(!stream){error="Could not write temporary UI accessibility preferences.";return false;}
                stream.imbue(std::locale::classic());
                stream<<std::setprecision(9)<<"renegade_ui_preferences=3\nreduced_motion="<<(reducedMotion?'1':'0')
                    <<"\ntext_scale="<<presentation.textScale<<"\ncontrast="<<presentation.contrast
                    <<"\nfocus_style="<<presentation.focusStyle<<"\nfocus_width="<<presentation.focusWidth<<"\nui_scale="<<presentation.uiScale
                    <<"\nminimum_target_size="<<presentation.minimumTargetSize<<"\n";
                stream.flush();
                if(!stream){error="Could not flush player UI preferences.";stream.close();std::filesystem::remove(temporary,ec);return false;}
            }
            if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            {
                std::filesystem::remove(temporary,ec);
                error="Could not commit player UI accessibility preferences.";return false;
            }
            error.clear();return true;
        }
    };
}
