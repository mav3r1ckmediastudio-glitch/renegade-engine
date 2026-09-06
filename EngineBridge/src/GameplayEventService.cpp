#include "renegade/bridge/GameplayEventService.h"
#include <utility>
namespace renegade::bridge {
bool GameplayEventService::Enqueue(GameplayEvent e,std::string& error){if(e.name.empty()||e.name.size()>MaxNameLength){error="Gameplay event name must be 1-64 bytes.";return false;}if(e.payload.size()>MaxPayloadLength){error="Gameplay event payload exceeds 4096 bytes.";return false;}if(e.senderEntityId.size()>36||e.targetEntityId.size()>36){error="Gameplay event entity references are invalid.";return false;}if(events_.size()>=MaxEvents){++dropped_;error="Gameplay event queue is full.";return false;}e.sequence=nextSequence_++;events_.push_back(std::move(e));error.clear();return true;}
bool GameplayEventService::TryDequeue(GameplayEvent& e)noexcept{if(events_.empty())return false;e=std::move(events_.front());events_.pop_front();return true;} void GameplayEventService::Clear()noexcept{events_.clear();}
}