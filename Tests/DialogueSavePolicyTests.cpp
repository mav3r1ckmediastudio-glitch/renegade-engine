#include "../Runtime/src/RuntimeSingleLevelSavePolicy.h"
#include <iostream>
using namespace renegade::bridge;
using renegade::runtime::SupportsSingleLevelQuickSave;
#define CHECK(v)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(v))                                                                                                      \
        {                                                                                                              \
            std::cerr << "Save policy failure at " << __LINE__ << '\n';                                                \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
int main()
{
    FlowDocument doc;
    doc.startNodeId = "start";
    FlowNode start;
    start.id = "start";
    start.kind = FlowNodeKind::GameStart;
    FlowNode level;
    level.id = "level";
    level.kind = FlowNodeKind::Level;
    FlowNode end;
    end.id = "end";
    end.kind = FlowNodeKind::CompleteGame;
    doc.nodes = {start, level, end};
    FlowRoute enter;
    enter.sourceNodeId = "start";
    enter.destinationNodeId = "level";
    enter.outcome = GameStartOutcome;
    FlowRoute finish;
    finish.sourceNodeId = "level";
    finish.destinationNodeId = "end";
    finish.outcome = "level.complete";
    doc.routes = {enter, finish};
    CHECK(SupportsSingleLevelQuickSave(doc, &level));
    CHECK(!SupportsSingleLevelQuickSave(doc, nullptr));
    CHECK(!SupportsSingleLevelQuickSave(doc, &start));
    auto changed = doc;
    changed.nodes[2].kind = FlowNodeKind::Screen;
    CHECK(!SupportsSingleLevelQuickSave(changed, &level));
    changed = doc;
    changed.nodes.push_back(level);
    CHECK(!SupportsSingleLevelQuickSave(changed, &level));
    changed = doc;
    changed.routes[0].conditions.push_back({"flag", FlowConditionOperator::Equals, "1"});
    CHECK(!SupportsSingleLevelQuickSave(changed, &level));
    changed = doc;
    changed.routes[0].destinationNodeId = "end";
    CHECK(!SupportsSingleLevelQuickSave(changed, &level));
    changed = doc;
    changed.routes[0].outcome = "wrong";
    CHECK(!SupportsSingleLevelQuickSave(changed, &level));
    changed = doc;
    changed.routes.push_back(finish);
    CHECK(!SupportsSingleLevelQuickSave(changed, &level));
    changed = doc;
    changed.startNodeId = "missing";
    CHECK(!SupportsSingleLevelQuickSave(changed, &level));
    std::cout << "10 single-level dialogue save policy checks passed\n";
}
