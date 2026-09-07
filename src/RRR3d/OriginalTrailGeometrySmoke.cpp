#include "OriginalTrailGeometry.h"
#include "effects/OriginalTrailEmitter.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

int main()
{
    using namespace rrr3d::race;
    try
    {
        const auto fade = [](float frame) {
            return std::array<float, 4U>{1.0F, 1.0F, 1.0F, 1.0F - frame};
        };
        // Two generations/racers in one submission. Their groups have
        // different ages, with a right-angle turn in the first strip.
        const std::vector<TrailSample> samples{
            {{0.0F, 0.0F, 0.01F}, 0.8F, true},
            {{1.0F, 0.0F, 0.01F}, 0.2F, false},
            {{1.0F, 1.0F, 0.01F}, 0.0F, false},
            {{100.0F, 50.0F, 2.01F}, 0.0F, true},
            {{100.0F, 51.0F, 2.01F}, 0.5F, false},
            {{100.0F, 52.0F, 2.01F}, 0.5F, false}};
        std::vector<r3d::renderer::StaticMeshVertex> vertices;
        std::vector<std::uint32_t> indices;
        const auto build = [&](const auto& points) {
            buildTrailGeometry(points, 0.3F, {0.0F, 0.0F, 1.0F},
                true, {0.0F, -10.0F, 10.0F}, fade, vertices, indices);
        };
        build(samples);
        if (vertices.size() != 16U || indices.size() != 24U)
            throw std::runtime_error("trail generations gained a connecting segment");
        const std::array<std::uint32_t, 4U> alphas{51U, 204U, 255U, 128U};
        for (std::size_t segment = 0U; segment < alphas.size(); ++segment)
        {
            for (std::size_t corner = 0U; corner < 4U; ++corner)
                if ((vertices[segment * 4U + corner].color >> 24U) !=
                    alphas[segment])
                    throw std::runtime_error("FxTrail group age was lost or interpolated");
            for (std::size_t index = 0U; index < 6U; ++index)
                if (indices[segment * 6U + index] / 4U != segment)
                    throw std::runtime_error("indices crossed source group boundary");
        }
        // Source BuildVertexLine uses the previous direction at a particle
        // and the newest direction at the terminal system point.
        if (std::abs(vertices[4U].x - 1.0F) > 0.001F ||
            std::abs(vertices[4U].y - 0.3F) > 0.001F ||
            std::abs(vertices[6U].x - 0.7F) > 0.001F ||
            vertices[8U].u != 0.0F || vertices[10U].u != 1.0F)
            throw std::runtime_error("source turn direction/UV restart changed");
        build(std::vector<TrailSample>{
            {{0.0F, 0.0F, 0.01F}, 0.0F, true},
            {{0.0F, 0.0F, 0.01F}, 0.0F, false}});
        for (const auto& vertex : vertices)
            if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) ||
                !std::isfinite(vertex.z))
                throw std::runtime_error("stationary emitter produced NaN geometry");
        build(std::vector<TrailSample>{});
        if (!vertices.empty() || !indices.empty())
            throw std::runtime_error("expired effects kept old batch geometry");
        r3d::effects::FxTrailEmitter emitter(1.0F, 10.0F, 100U);
        emitter.OnProgress(0.1F, {}, false);
        emitter.OnProgress(0.1F, {1.0F, 0.0F, 0.0F}, false);
        if (emitter.GetCntParticle() != 1U)
            throw std::runtime_error("sotDist emitted at the exact distance boundary");
        emitter.OnProgress(0.1F, {4.0F, 0.0F, 0.0F}, false);
        if (emitter.GetCntParticle() != 4U)
            throw std::runtime_error("sotDist did not catch up after a slow frame");
        for (std::size_t group = 1U; group < 4U; ++group)
        {
            const auto& particle = emitter.GetGroups()[group];
            if (std::abs(particle.position.x - static_cast<float>(group)) >
                    0.001F || particle.GetFrame() != 0.0F ||
                particle.particleTime <= 0.0F)
                throw std::runtime_error("source group birth/particle offset changed");
        }
        const float oldLife = emitter.GetGroups().front().life;
        emitter.OnProgress(0.1F, {4.0F, 0.0F, 0.0F}, false);
        if (emitter.GetCntParticle() != 4U ||
            emitter.GetGroups().front().life >= oldLife)
            throw std::runtime_error("stationary trail emitted or stopped aging");

        r3d::effects::FxTrailEmitter capacity(1.0F, 1.0F, 3U);
        capacity.OnProgress(0.0F, {}, false);
        capacity.OnProgress(0.1F, {3.1F, 0.0F, 0.0F}, false);
        capacity.OnProgress(0.1F, {50.0F, 0.0F, 0.0F}, false);
        if (capacity.GetCntParticle() != 3U ||
            capacity.GetGroups().front().index != 0U ||
            capacity.GetGroups().back().index != 2U)
            throw std::runtime_error("mnaWaitingFree replaced live particles");
        capacity.OnProgress(0.85F, {51.0F, 0.0F, 0.0F}, false);
        if (capacity.GetCntParticle() != 3U ||
            capacity.GetGroups().front().index != 1U ||
            capacity.GetGroups().back().index != 3U)
            throw std::runtime_error("expired source capacity was not reused");
        capacity.OnProgress(1.0F, {55.0F, 0.0F, 0.0F}, true);
        if (capacity.GetCntParticle() != 0U ||
            capacity.GetRemainingLife() != 0.0F)
            throw std::runtime_error("fading source emitted or failed to expire");

        r3d::effects::FxTrailEmitter catchUp(1.0F, 10.0F, 0U);
        catchUp.OnProgress(0.0F, {}, false);
        catchUp.OnProgress(0.1F, {100.0F, 0.0F, 0.0F}, false);
        if (catchUp.GetCntParticle() != 21U ||
            std::abs(catchUp.GetGroups()[1U].position.x - 80.0F) > 0.001F ||
            std::abs(catchUp.GetGroups().back().position.x - 99.0F) > 0.001F)
            throw std::runtime_error("sotDist source 20-interval catch-up limit changed");
        std::cout << "FxTrail source group fade, strip boundaries, direction, "
                     "UV restart, sotDist catch-up, waiting-free capacity "
                     "and particle lifetime passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
