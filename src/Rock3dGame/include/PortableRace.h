#pragma once

#include "physics/PhysicsBackend.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace r3d::resource
{
class ResourceFileSystem;
}

namespace rrr3d::race
{

struct PortableRaceData
{
	r3d::physics::TrackGeometry track;
	std::string mapVirtualPath;
	std::size_t sourceTrackPieces = 0;
	std::size_t sourceCollisionMeshes = 0;
	std::uint32_t laps = 1;
};

PortableRaceData loadPortableRace(const r3d::resource::ResourceFileSystem &resources);

class PortableRaceSession
{
  public:
	explicit PortableRaceSession(const PortableRaceData &data);
	~PortableRaceSession();

	PortableRaceSession(const PortableRaceSession &) = delete;
	PortableRaceSession &operator=(const PortableRaceSession &) = delete;

	void reset() noexcept;
	void step(float seconds, const r3d::physics::VehicleInput &input) noexcept;
	r3d::physics::VehicleInput autopilotInput() const noexcept;

	const r3d::physics::VehicleState &vehicle() const noexcept;
	const r3d::physics::TrackGeometry &track() const noexcept;
	float trackLength() const noexcept;
	r3d::physics::TrackSample sampleTrack(float distance) const noexcept;
	float nearestTrackDistance(const r3d::physics::Vec3 &position) const noexcept;

  private:
	std::unique_ptr<r3d::physics::PhysicsWorld> world_;
};

struct RaceSmokeResult
{
	float finishSeconds = 0.0F;
	float maximumSpeed = 0.0F;
	std::uint32_t collisions = 0;
	std::uint8_t checkpoints = 0;
};

bool runPortableRaceSmokeTest(const PortableRaceData &data, RaceSmokeResult &result, std::string &error);

} // namespace rrr3d::race
