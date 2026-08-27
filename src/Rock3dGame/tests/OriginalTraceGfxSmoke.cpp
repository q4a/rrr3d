#include "OriginalTraceGfx.h"

#include <cmath>
#include <iostream>

namespace source = r3d::game::originalrace::source;

namespace
{

bool near(float first, float second, float tolerance = 0.0001F)
{
    return std::abs(first - second) <= tolerance;
}

int fail(const char* message)
{
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main()
{
    source::Trace trace(4U);
    auto* p0 = trace.AddPoint(10U);
    auto* p1 = trace.AddPoint(11U);
    auto* p2 = trace.AddPoint(12U);
    p0->SetPos({0.0F, 0.0F, 1.0F});
    p1->SetPos({4.0F, 0.0F, 2.0F});
    p2->SetPos({4.0F, 4.0F, 3.0F});
    p0->SetSize(2.0F);
    p1->SetSize(4.0F);
    p2->SetSize(2.0F);
    auto* path0 = trace.AddPath();
    path0->Add(p0);
    auto* selectedNode = path0->Add(p1);
    path0->Add(p2);
    auto* path1 = trace.AddPath();
    path1->Add(p0);
    path1->Add(p2);

    source::TraceGfx gfx(&trace);
    const auto& material = gfx.GetMaterialPolicy();
    if (!material.transparency || material.lighting ||
        material.depthWrite || material.depthTest ||
        !material.ignoreFog || material.cull ||
        !near(material.alpha, 0.5F))
        return fail("TraceGfx source material policy differs");

    auto draws = gfx.BuildDrawList();
    if (draws.size() != 5U ||
        draws[0].primitive != source::TraceGfxPrimitive::Box ||
        draws[0].vertices.size() != 8U ||
        draws[0].indices.size() != 36U ||
        !near(draws[0].vertices[0].x, -1.0F) ||
        !near(draws[0].color.red, 1.0F) ||
        !near(draws[3].color.red, 0.5F) ||
        !near(draws[4].color.red, 1.0F) ||
        draws[3].indices.size() != 12U)
        return fail("TraceGfx initial point/path draw list differs");

    gfx.SetSelPoint(p1);
    gfx.SetSelPath(path1);
    gfx.SetSelNode(selectedNode);
    source::TraceGfx::PointLink link(p1);
    link.SetPos({2.0F, 3.0F, 4.0F});
    gfx.SetPointLink(&link);
    draws = gfx.BuildDrawList();
    if (gfx.GetSelPoint() != p1 || gfx.GetSelPath() != path1 ||
        gfx.GetSelNode() != selectedNode ||
        gfx.GetPointLink() == nullptr || draws.size() != 7U ||
        !near(draws[1].color.green, 1.0F) ||
        !near(draws[4].color.green, 1.0F) ||
        draws[5].primitive !=
            source::TraceGfxPrimitive::SelectedTile ||
        draws[5].vertices.size() != 4U ||
        draws[5].indices.size() != 6U ||
        draws[6].primitive != source::TraceGfxPrimitive::PointLink ||
        !near(draws[6].linkStart.x, 4.0F) ||
        !near(draws[6].linkEnd.y, 3.0F) ||
        !near(draws[6].linkWidth, 4.0F))
        return fail("TraceGfx selection/link draw list differs");

    trace.DelPath(path1);
    if (gfx.GetSelPath() != nullptr)
        return fail("TraceGfx retained a removed path selection");
    gfx.SetSelPoint(nullptr);
    gfx.SetSelNode(nullptr);
    gfx.SetPointLink(nullptr);
    if (gfx.GetSelPoint() != nullptr || gfx.GetSelNode() != nullptr ||
        gfx.GetPointLink() != nullptr)
        return fail("TraceGfx source reference release differs");

    std::cout << "original TraceGfx source rules passed\n";
    return 0;
}
