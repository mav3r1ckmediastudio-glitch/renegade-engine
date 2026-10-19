#include "renegade/bridge/GameplayAttributeDocumentService.h"
#include "ScreenBindingPersistence.h"
#include "renegade/bridge/DependencyService.h"
#include <wiConfig.h>
#include <filesystem>
#include <set>
#include <sstream>
#include <iomanip>
#include <limits>
namespace renegade::bridge
{
    namespace
    {
        template<class T> bool Scalar(const wi::config::Section& s,const char* key,T& out)
        {
            if(!s.Has(key))return false;std::istringstream in(s.GetText(key));in.imbue(std::locale::classic());
            in>>std::noskipws>>out;return !in.fail()&&in.peek()==std::char_traits<char>::eof();
        }
        std::string Number(double value)
        {std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<double>::max_digits10)<<value;return out.str();}
        void WriteValue(wi::config::Section& s,const GameplayAttributeValue& value)
        {
            s.Set("value_type",int(value.index()));
            if(const auto* n=std::get_if<double>(&value))s.Set("value",Number(*n));
            else if(const auto* b=std::get_if<bool>(&value))s.Set("value",*b?"true":"false");
            else s.Set("value",detail::EncodeBindingText(std::get<std::string>(value)));
        }
        bool ReadValue(const wi::config::Section& s,GameplayAttributeValue& value)
        {
            int type=-1;if(!Scalar(s,"value_type",type)||!s.Has("value"))return false;
            if(type==0){double n=0;if(!Scalar(s,"value",n)||!std::isfinite(n))return false;value=n;}
            else if(type==1){const auto text=s.GetText("value");if(text!="true"&&text!="false")return false;value=text=="true";}
            else if(type==2){std::string text;if(!detail::DecodeBindingText(s.GetText("value"),4096,text))return false;value=std::move(text);}
            else return false;return true;
        }
        bool ReadOptionalNumber(const wi::config::Section& s,const char* key,std::optional<double>& value)
        {
            if(!s.Has(key)){value.reset();return true;}double n=0;if(!Scalar(s,key,n)||!std::isfinite(n))return false;value=n;return true;
        }
        bool NativeId(const std::string& id)
        {
            return id=="health.current"||id=="health.maximum"||id=="alive"||id=="ammo.loaded"||
                id=="ammo.capacity"||id=="ammo.reserve"||id=="ammo.reloading"||id=="equipment.equipped";
        }
    }
    bool ValidateGameplayAttributeDocument(const GameplayAttributeDocument& doc,const StableId& projectId,std::string& error)
    {
        if(!ValidateDocumentEnvelope(doc.envelope,error))return false;
        if(doc.envelope.documentType!=GameplayAttributeDocumentType||doc.envelope.projectId!=projectId||
            doc.schemaVersion!=1||doc.profiles.size()>64||doc.attachments.size()>256)
        {error="Gameplay attribute document ownership, schema or count is invalid.";return false;}
        std::map<StableId,const GameplayAttributeProfile*> profiles;std::size_t count=0;
        for(const auto& profile:doc.profiles)
        {
            std::map<std::string,GameplayAttributeDefinition> check;
            if(profile.name.size()>256){error="Gameplay profile name exceeds its bound.";return false;}
            if(!ResolveGameplayAttributeProfile(profile,{},check,error))return false;
            if(!profiles.emplace(profile.id,&profile).second){error="Duplicate gameplay profile identity.";return false;}
            count+=profile.attributes.size();
            for(const auto& definition:profile.attributes)if(NativeId(definition.id))
            {error="Native health, ammo, alive and equipment state must use their existing gameplay owners.";return false;}
        }
        if(count>4096){error="Gameplay attribute definition budget exceeded.";return false;}
        std::set<std::pair<int,StableId>> targets;std::size_t attachedValues=0;
        for(const auto& attachment:doc.attachments)
        {
            const auto profile=profiles.find(attachment.profileId);
            if(int(attachment.scope)<0||int(attachment.scope)>2||profile==profiles.end()||
                (attachment.scope==GameplayAttributeScope::Character?!IsValidStableId(attachment.targetId):!attachment.targetId.empty())||
                !targets.emplace(int(attachment.scope),attachment.targetId).second)
            {error="Gameplay profile attachment has an invalid/duplicate scope, actor or profile.";return false;}
            std::map<std::string,GameplayAttributeDefinition> check;
            if(!ResolveGameplayAttributeProfile(*profile->second,attachment.overrides,check,error))return false;
            attachedValues+=check.size();if(attachedValues>8192){error="Gameplay attached-value budget exceeded.";return false;}
        }
        error.clear();return true;
    }
    bool ReadGameplayAttributeDocument(const std::string& path,const StableId& projectId,GameplayAttributeDocument& out,std::string& error)
    {
        GameplayAttributeDocument doc;
        if(!ReadDocumentEnvelope(path,doc.envelope,error))return false;
        try
        {
            wi::config::File file;if(!file.Open(path)||!file.HasSection("attributes"))
            {error="Gameplay attribute document cannot be read.";return false;}
            const auto& root=file.GetSection("attributes");int schema=-1,profiles=-1,attachments=-1;
            if(!Scalar(root,"schema_version",schema)||schema!=1||!Scalar(root,"profile_count",profiles)||profiles<0||profiles>64||
                !Scalar(root,"attachment_count",attachments)||attachments<0||attachments>256)
            {error="Unsupported or malformed gameplay attribute document header.";return false;}
            for(int i=0;i<profiles;++i)
            {
                const auto prefix="profile_"+std::to_string(i);
                if(!file.HasSection(prefix.c_str())){error="Missing gameplay profile section.";return false;}
                const auto& s=file.GetSection(prefix.c_str());GameplayAttributeProfile profile;int count=-1;
                if(!s.Has("id")||!s.Has("name_hex")||!detail::DecodeBindingText(s.GetText("name_hex"),256,profile.name)||
                    !Scalar(s,"attribute_count",count)||count<0||count>256)
                {error="Malformed gameplay profile.";return false;}profile.id=s.GetText("id");
                for(int j=0;j<count;++j)
                {
                    const auto name=prefix+"_attribute_"+std::to_string(j);
                    if(!file.HasSection(name.c_str())){error="Missing gameplay attribute section.";return false;}
                    const auto& a=file.GetSection(name.c_str());GameplayAttributeDefinition definition;
                    if(!a.Has("id")||!a.Has("name_hex")||!detail::DecodeBindingText(a.GetText("name_hex"),256,definition.name)||
                        !ReadValue(a,definition.initial)||!ReadOptionalNumber(a,"minimum",definition.minimum)||
                        !ReadOptionalNumber(a,"maximum",definition.maximum)||!Scalar(a,"regenerate_per_second",definition.regeneratePerSecond))
                    {error="Malformed gameplay attribute definition.";return false;}
                    definition.id=a.GetText("id");profile.attributes.push_back(std::move(definition));
                }
                doc.profiles.push_back(std::move(profile));
            }
            for(int i=0;i<attachments;++i)
            {
                const auto prefix="attachment_"+std::to_string(i);
                if(!file.HasSection(prefix.c_str())){error="Missing gameplay attachment section.";return false;}
                const auto& s=file.GetSection(prefix.c_str());GameplayAttributeAttachment attachment;int scope=-1,count=-1;
                if(!Scalar(s,"scope",scope)||scope<0||scope>2||!s.Has("target")||!s.Has("profile")||
                    !Scalar(s,"override_count",count)||count<0||count>256)
                {error="Malformed gameplay profile attachment.";return false;}
                attachment.scope=static_cast<GameplayAttributeScope>(scope);attachment.targetId=s.GetText("target");attachment.profileId=s.GetText("profile");
                for(int j=0;j<count;++j)
                {
                    const auto name=prefix+"_override_"+std::to_string(j);
                    if(!file.HasSection(name.c_str())){error="Missing gameplay override section.";return false;}
                    const auto& a=file.GetSection(name.c_str());GameplayAttributeOverride value;int mask=-1;
                    if(!a.Has("id")||!Scalar(a,"mask",mask)||mask<0||mask>15)
                    {error="Malformed gameplay override mask.";return false;}value.attributeId=a.GetText("id");
                    if(mask&1){GameplayAttributeValue initial;if(!ReadValue(a,initial)){error="Malformed initial override.";return false;}value.initial=std::move(initial);}
                    if(mask&2){double n=0;if(!Scalar(a,"minimum",n)){error="Malformed minimum override.";return false;}value.minimum=n;}
                    if(mask&4){double n=0;if(!Scalar(a,"maximum",n)){error="Malformed maximum override.";return false;}value.maximum=n;}
                    if(mask&8){double n=0;if(!Scalar(a,"regenerate_per_second",n)){error="Malformed rule override.";return false;}value.regeneratePerSecond=n;}
                    attachment.overrides.push_back(std::move(value));
                }
                doc.attachments.push_back(std::move(attachment));
            }
            if(!ValidateGameplayAttributeDocument(doc,projectId,error))return false;
            out=std::move(doc);error.clear();return true;
        }
        catch(const std::exception& e){error=std::string("Gameplay attribute read failed: ")+e.what();return false;}
    }
    bool WriteGameplayAttributeDocument(const std::string& path,const GameplayAttributeDocument& doc,std::string& error)
    {
        if(!ValidateGameplayAttributeDocument(doc,doc.envelope.projectId,error))return false;
        return WriteTransactionalDocument(path,doc.envelope,false,[&](wi::config::File& file){
            auto& root=file.GetSection("attributes");root.Set("schema_version",1);
            root.Set("profile_count",int(doc.profiles.size()));root.Set("attachment_count",int(doc.attachments.size()));
            for(std::size_t i=0;i<doc.profiles.size();++i)
            {
                const auto prefix="profile_"+std::to_string(i);const auto& profile=doc.profiles[i];auto& s=file.GetSection(prefix.c_str());
                s.Set("id",profile.id);s.Set("name_hex",detail::EncodeBindingText(profile.name));s.Set("attribute_count",int(profile.attributes.size()));
                for(std::size_t j=0;j<profile.attributes.size();++j)
                {
                    const auto& definition=profile.attributes[j];auto& a=file.GetSection((prefix+"_attribute_"+std::to_string(j)).c_str());
                    a.Set("id",definition.id);a.Set("name_hex",detail::EncodeBindingText(definition.name));WriteValue(a,definition.initial);
                    if(definition.minimum)a.Set("minimum",Number(*definition.minimum));
                    if(definition.maximum)a.Set("maximum",Number(*definition.maximum));
                    a.Set("regenerate_per_second",Number(definition.regeneratePerSecond));
                }
            }
            for(std::size_t i=0;i<doc.attachments.size();++i)
            {
                const auto prefix="attachment_"+std::to_string(i);const auto& attachment=doc.attachments[i];auto& s=file.GetSection(prefix.c_str());
                s.Set("scope",int(attachment.scope));s.Set("target",attachment.targetId);s.Set("profile",attachment.profileId);s.Set("override_count",int(attachment.overrides.size()));
                for(std::size_t j=0;j<attachment.overrides.size();++j)
                {
                    const auto& value=attachment.overrides[j];auto& a=file.GetSection((prefix+"_override_"+std::to_string(j)).c_str());
                    const int mask=(value.initial?1:0)|(value.minimum?2:0)|(value.maximum?4:0)|(value.regeneratePerSecond?8:0);
                    a.Set("id",value.attributeId);a.Set("mask",mask);if(value.initial)WriteValue(a,*value.initial);
                    if(value.minimum)a.Set("minimum",Number(*value.minimum));if(value.maximum)a.Set("maximum",Number(*value.maximum));
                    if(value.regeneratePerSecond)a.Set("regenerate_per_second",Number(*value.regeneratePerSecond));
                }
            }
        },[id=doc.envelope.documentId,projectId=doc.envelope.projectId](const std::string& staged,std::string& checkError){
            GameplayAttributeDocument check;return ReadGameplayAttributeDocument(staged,projectId,check,checkError)&&check.envelope.documentId==id;
        },error);
    }
    bool ResolveGameplayAttributeDocumentPath(const std::string& root,const StableId& projectId,const StableId& id,
        const std::string& hint,std::string& resolved,std::string& error)
    {
        namespace fs=std::filesystem;
        if(!IsValidStableId(projectId)||!IsValidStableId(id))
        {error="Gameplay attribute reference lacks valid identities.";return false;}
        const auto hintPath=ResolveDependencyPath(root,hint);
        if(!hintPath.accepted||hintPath.canonicalRelativePath.rfind("Content/",0)!=0)
        {error="Gameplay attribute source must be inside project Content.";return false;}
        std::set<std::string> matches;
        const auto consider=[&](const fs::path& path){
            DocumentEnvelope envelope;std::string ignored;
            if(!ReadDocumentEnvelope(path.generic_u8string(),envelope,ignored)||envelope.documentId!=id||
                envelope.projectId!=projectId||envelope.documentType!=GameplayAttributeDocumentType)return;
            const auto relative=fs::relative(path,fs::u8path(root)).generic_u8string();
            const auto safe=ResolveDependencyPath(root,relative);
            if(safe.accepted&&safe.exists&&safe.canonicalRelativePath.rfind("Content/",0)==0)matches.insert(safe.absolutePath);
        };
        consider(fs::u8path(hintPath.absolutePath));
        std::error_code ec;fs::recursive_directory_iterator it(fs::u8path(root)/"Content",fs::directory_options::skip_permission_denied,ec),end;
        while(!ec&&it!=end){if(it->is_regular_file(ec)&&!ec&&it->path().extension()==".renegade-attributes")consider(it->path());it.increment(ec);}
        if(matches.size()!=1){error=matches.empty()?"Gameplay attribute source is missing or has incompatible ownership/type.":"Gameplay attribute identity is ambiguous.";return false;}
        resolved=*matches.begin();error.clear();return true;
    }
}
