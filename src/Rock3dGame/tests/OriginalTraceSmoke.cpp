#include "OriginalTrace.h"

#include <cmath>
#include <iostream>

namespace source = r3d::game::originalrace::source;

namespace
{

bool near(float first, float second, float tolerance = 0.001F)
{
    return std::abs(first - second) <= tolerance;
}

} // namespace

int main()
{
    source::Trace trace(4U);
    auto* first = trace.AddPoint(10U);
    first->SetPos({0.0F, 0.0F, 0.0F});
    first->SetSize(10.0F);
    auto* middle = trace.AddPoint(20U);
    middle->SetPos({100.0F, 0.0F, 0.0F});
    middle->SetSize(20.0F);
    auto* last = trace.AddPoint(30U);
    last->SetPos({100.0F, 100.0F, 10.0F});
    last->SetSize(10.0F);

    auto* path = trace.AddPath();
    auto* firstNode = path->Add(first);
    auto* middleNode = path->Add(middle);
    auto* lastNode = path->Add(last);
    if (firstNode == nullptr || middleNode == nullptr || lastNode == nullptr ||
        path->GetCount() != 3U || firstNode->GetNext() != middleNode ||
        middleNode->GetPrev() != firstNode || middleNode->GetNext() != lastNode)
        return 1;

    // Trace.cpp includes the terminal node's half-width in GetFinishDist.
    if (!near(path->GetLength(), 205.0F) ||
        !near(firstNode->GetTile().GetFinishDist(), 205.0F) ||
        !near(middleNode->GetTile().GetStartDist(), 100.0F))
        return 2;

    const auto& tile = firstNode->GetTile();
    if (!near(tile.GetDir().x, 1.0F) || !near(tile.GetDir().y, 0.0F) ||
        !near(tile.ComputeCoordX({50.0F, 0.0F}), 0.5F) ||
        !near(tile.ComputeWidth(0.5F), 15.0F) ||
        !tile.IsContains({50.0F, 7.0F, 0.0F}) ||
        tile.IsContains({50.0F, 8.0F, 0.0F}))
        return 3;

    auto* found = trace.IsTileContains({100.0F, 50.0F, 5.0F});
    if (found != middleNode || trace.GetNodeRef(found).path != 0U ||
        trace.GetNodeRef(found).node != 1U ||
        trace.FindClosestNode({98.0F, 1.0F, 0.0F}) != middleNode)
        return 4;

    if (tile.ComputeTrackInd({50.0F, 7.0F}) >= 4U ||
        source::Trace::NodeRef{}.valid())
        return 5;

    auto* branch = trace.AddPath();
    branch->Add(first);
    branch->Add(last);
    if (!first->IsFind(branch) || first->GetNodes().size() != 2U ||
        trace.GetPathCount() != 2U)
        return 6;
    trace.DelPath(branch);
    if (first->GetNodes().size() != 1U)
        return 7;

    std::cout << "original Trace/WayPath/WayNode source rules passed\n";
    return 0;
}
