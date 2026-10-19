#pragma once
#include "renegade/bridge/GameplayAttributeDocumentService.h"
#include <filesystem>
#include <fstream>
#include <iterator>
namespace renegade::bridge
{
    // Separate gameplay-document history. Screen editing cannot mutate this state.
    class GameplayAttributeAuthoringSession
    {
        struct Entry{GameplayAttributeDocument document;std::uint64_t identity=0;};
    public:
        bool Open(const std::string& path,const StableId& projectId,std::string& error)
        {
            GameplayAttributeDocument doc;if(!ReadGameplayAttributeDocument(path,projectId,doc,error))return false;
            std::string baseline;if(!ReadBytes(path,baseline,error))return false;
            path_=path;projectId_=projectId;history_={{std::move(doc),++nextIdentity_}};index_=0;
            baseline_=std::move(baseline);existed_=true;savedIdentity_=history_[0].identity;++revision_;error.clear();return true;
        }
        bool CreateDraft(const std::string& path,GameplayAttributeDocument document,std::string& error)
        {
            if(std::filesystem::exists(std::filesystem::u8path(path)))
            {error="Refusing to replace an existing gameplay attribute document.";return false;}
            if(!ValidateGameplayAttributeDocument(document,document.envelope.projectId,error))return false;
            projectId_=document.envelope.projectId;path_=path;history_={{std::move(document),++nextIdentity_}};index_=0;
            existed_=false;baseline_.clear();savedIdentity_=0;++revision_;error.clear();return true;
        }
        bool Replace(GameplayAttributeDocument document,std::string& error)
        {
            if(!IsLoaded()||document.envelope.documentId!=Document().envelope.documentId||
                document.envelope.pathHint!=Document().envelope.pathHint)
            {error="Gameplay profile edit cannot change the active document identity or location.";return false;}
            if(!ValidateGameplayAttributeDocument(document,projectId_,error))return false;
            history_.erase(history_.begin()+index_+1,history_.end());
            history_.push_back({std::move(document),++nextIdentity_});index_=history_.size()-1;
            if(history_.size()>128){history_.erase(history_.begin());--index_;}
            ++revision_;error.clear();return true;
        }
        bool Undo(std::string& error)
        {if(!IsLoaded()||index_==0){error="No gameplay profile Undo is available.";return false;}--index_;++revision_;error.clear();return true;}
        bool Redo(std::string& error)
        {if(!IsLoaded()||index_+1>=history_.size()){error="No gameplay profile Redo is available.";return false;}++index_;++revision_;error.clear();return true;}
        bool Save(std::string& error)
        {
            if(!IsLoaded()){error="No gameplay attribute document is open.";return false;}
            const bool exists=std::filesystem::exists(std::filesystem::u8path(path_));
            if(exists!=existed_){error="Gameplay attribute file existence changed externally. Reload before saving.";return false;}
            if(existed_)
            {
                std::string current;if(!ReadBytes(path_,current,error))return false;
                if(current!=baseline_){error="Gameplay attribute document changed externally. Reload before saving.";return false;}
            }
            std::error_code ec;std::filesystem::create_directories(std::filesystem::u8path(path_).parent_path(),ec);
            if(ec){error="Cannot create gameplay attribute document directory: "+ec.message();return false;}
            if(!WriteGameplayAttributeDocument(path_,Document(),error))return false;
            std::string current;if(!ReadBytes(path_,current,error))return false;
            baseline_=std::move(current);existed_=true;savedIdentity_=history_[index_].identity;error.clear();return true;
        }
        void Clear() noexcept{path_.clear();projectId_.clear();history_.clear();baseline_.clear();index_=0;existed_=false;savedIdentity_=0;++revision_;}
        bool IsLoaded() const noexcept{return !history_.empty();}
        bool Dirty() const noexcept{return IsLoaded()&&history_[index_].identity!=savedIdentity_;}
        bool CanUndo() const noexcept{return IsLoaded()&&index_>0;}
        bool CanRedo() const noexcept{return IsLoaded()&&index_+1<history_.size();}
        std::uint64_t Revision() const noexcept{return revision_;}
        const GameplayAttributeDocument& Document() const{return history_.at(index_).document;}
        const std::string& Path() const noexcept{return path_;}
    private:
        static bool ReadBytes(const std::string& path,std::string& bytes,std::string& error)
        {
            std::ifstream input(std::filesystem::u8path(path),std::ios::binary);
            if(!input){error="Cannot read gameplay attribute file baseline.";return false;}
            bytes.assign(std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>());
            if(input.bad()){error="Gameplay attribute baseline read failed.";return false;}return true;
        }
        std::vector<Entry> history_;std::size_t index_=0;std::uint64_t revision_=0,nextIdentity_=0,savedIdentity_=0;
        std::string path_,projectId_,baseline_;bool existed_=false;
    };
}
