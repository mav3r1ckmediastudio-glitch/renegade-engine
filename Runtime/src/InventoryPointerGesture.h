#pragma once
#include <cmath>
#include <string>
#include <utility>
namespace renegade::runtime {
// Runtime-facing input policy without Wicked/graphics dependencies. One
// pointer press owns at most one item gesture. Native Widget focus/drag
// callbacks are NOT trusted to own a multi-frame inventory transaction.
class InventoryPointerGesture final {
public:
    enum class Kind { None, Began, ClickReleased, DragReleased };
    struct Event {
        Kind kind=Kind::None;
        std::string sourceAction;
        bool fullStack=false; // Shift captured at mouse-down.
    };
    Event Step(bool leftDown,bool shiftDown,float x,float y,
               const std::string& hoveredAction) {
        if(!std::isfinite(x)||!std::isfinite(y)) {
            Reset();return {};
        }
        if(leftDown && !wasDown_) {
            wasDown_=true;
            source_=hoveredAction;
            fullStack_=shiftDown;
            startX_=x;startY_=y;moved_=false;
            ignoreClick_.clear(); // New click always re-arms the control.
            if(source_.empty())return {};
            return {Kind::Began,source_,fullStack_};
        }
        if(leftDown && wasDown_) {
            if(!source_.empty()) {
                const auto dx=static_cast<double>(x)-startX_;
                const auto dy=static_cast<double>(y)-startY_;
                if(dx*dx+dy*dy>=144.0)moved_=true;
            }
            return {};
        }
        if(!leftDown && wasDown_) {
            wasDown_=false;
            if(source_.empty())return {};
            const auto dx=static_cast<double>(x)-startX_;
            const auto dy=static_cast<double>(y)-startY_;
            if(dx*dx+dy*dy>=144.0)moved_=true;
            const Event result{moved_?Kind::DragReleased:Kind::ClickReleased,
                               source_,fullStack_};
            if(moved_)ignoreClick_=source_;
            source_.clear();moved_=false;fullStack_=false;
            return result;
        }
        return {};
    }
    // Resolve a single, ordinary click through the authoritative inventory
    // slot hit-test, regardless of whether a native button is covered by an
    // authored icon or text badge. Releasing outside the originating slot
    // never activates an item.
    static bool ConfirmedSlotClick(const Event& event,
                                   const std::string& releaseAction) noexcept {
        return event.kind==Kind::ClickReleased &&
               !event.sourceAction.empty() &&
               event.sourceAction==releaseAction;
    }
    // A native Wicked Button may also dispatch a Click after DragEnd when
    // released back over its original rect. Never transfer twice.
    bool ConsumeNativeMouseClick(const std::string& actionId) {
        if(actionId.empty())return false;
        if((moved_&&!source_.empty()&&source_==actionId)||
           (!ignoreClick_.empty()&&ignoreClick_==actionId)) {
            ignoreClick_.clear();return true;
        }
        return false;
    }
    void Reset() {
        wasDown_=false;moved_=false;fullStack_=false;
        source_.clear();ignoreClick_.clear();startX_=startY_=0;
    }
private:
    bool wasDown_=false,moved_=false,fullStack_=false;
    float startX_=0,startY_=0;
    std::string source_,ignoreClick_;
};
} // namespace renegade::runtime
