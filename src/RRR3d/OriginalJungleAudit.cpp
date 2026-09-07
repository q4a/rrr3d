#include "OriginalJungleAudit.h"
#include "OriginalRace.h"
#include "physics/OriginalVehiclePhysics.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace rrr3d::diagnostics
{
bool auditJungleStarts(const r3d::resource::ResourceFileSystem& resources)
{
    using namespace r3d::game::originalrace;
    using namespace r3d::physics;
    try
    {
        const auto catalog = loadOriginalRace(resources, 0);
        std::size_t maps = 0, tested = 0, failed = 0;
        for (std::size_t track = 0; track < catalog.trackCatalog.size(); ++track)
        {
            if (catalog.trackCatalog[track].worldType != "wtWorld5")
                continue;
            const auto race = loadOriginalRace(resources, track);
            const auto description = makePhysicsDescription(race, resources);
            std::string error;
            auto world = createOriginalVehicleWorld(description, error);
            if (!world)
                throw std::runtime_error(error);
            std::vector<float> roadHeights;
            for (const auto& spawn : description.spawns)
            {
                WorldRayCastQuery query;
                query.origin = spawn.position;
                query.direction = {0, 0, -1};
                query.maximumDistance = 1000;
                query.trackPlaneOnly = true;
                const auto hit = world->raycast(query);
                roadHeights.push_back(hit.hit ? hit.position.z : -1000);
            }
            // This is an initial-placement/collision audit, not an AI race:
            // no invented waypoint driver, acceleration or cheat immunity.
            for (int step = 0; step < 120; ++step)
                world->step(1.0F / 60.0F, VehicleInput{});
            std::cout << race.levelPath << " start";
            for (std::size_t racer = 0; racer < world->vehicleCount(); ++racer)
            {
                const auto& state = world->vehicle(racer);
                const auto& spawn = description.spawns[racer];
                const auto& pos = state.body.position;
                const float offset = std::hypot(pos.x - spawn.position.x, pos.y - spawn.position.y);
                const bool valid = roadHeights[racer] > 0 && std::isfinite(pos.z) &&
                    pos.z > roadHeights[racer] && pos.z < spawn.position.z + 1 &&
                    offset < 3 && state.contactCount > 0;
                ++tested;
                if (!valid) ++failed;
                std::cout << ' ' << racer << ':' << (valid ? "OK" : "FAIL")
                          << "(z=" << pos.z << ",road=" << roadHeights[racer]
                          << ",offset=" << offset << ",wheels=" << state.contactCount << ')';
            }
            ++maps;
            std::cout << std::endl;
        }
        std::cout << "Jungle starts: maps=" << maps << " cars=" << tested
                  << " failures=" << failed << std::endl;
        return maps == 16 && tested > 0 && failed == 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Jungle start audit: " << error.what() << std::endl;
        return false;
    }
}
}
