#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
namespace renegade::bridge {
struct GameplayEvent { std::uint64_t sequence=0; std::string name; std::string payload; std::string senderEntityId; std::string targetEntityId; };
class GameplayEventService final {
public: static constexpr std::size_t MaxEvents=1024, MaxNameLength=64, MaxPayloadLength=4096;
bool Enqueue(GameplayEvent event,std::string& error); bool TryDequeue(GameplayEvent& event) noexcept; void Clear() noexcept;
std::size_t Size() const noexcept{return events_.size();} std::size_t DroppedCount() const noexcept{return dropped_;}
private: std::deque<GameplayEvent> events_; std::uint64_t nextSequence_=1; std::size_t dropped_=0;
}; }