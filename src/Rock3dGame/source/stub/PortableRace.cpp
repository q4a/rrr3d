#include "PortableRace.h"

#include "resource/ResourceFileSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace rrr3d::race
{
namespace
{

constexpr float pi = 3.14159265358979323846F;

float wrapAngle(float angle) noexcept
{
	while (angle > pi)
		angle -= 2.0F * pi;
	while (angle < -pi)
		angle += 2.0F * pi;
	return angle;
}

std::vector<r3d::physics::Vec3> parseTrackPositions(std::string_view map)
{
	const auto track_start = map.find("<ctTrack>");
	const auto track_end = map.find("</ctTrack>", track_start);
	if (track_start == std::string_view::npos || track_end == std::string_view::npos)
		throw std::runtime_error("debugTrack.r3dMap does not contain ctTrack");

	const auto section = map.substr(track_start, track_end - track_start);
	std::vector<r3d::physics::Vec3> positions;
	std::size_t cursor = 0;
	while ((cursor = section.find("<pos>", cursor)) != std::string_view::npos)
	{
		cursor += 5;
		const auto end = section.find("</pos>", cursor);
		if (end == std::string_view::npos)
			throw std::runtime_error("debugTrack.r3dMap has an unterminated ctTrack position");
		std::istringstream values(std::string(section.substr(cursor, end - cursor)));
		r3d::physics::Vec3 position;
		if (!(values >> position.x >> position.z >> position.y))
			throw std::runtime_error("debugTrack.r3dMap has an invalid ctTrack position");
		positions.push_back(position);
		cursor = end + 6;
	}
	return positions;
}

std::uint32_t parseLapCount(std::string_view map)
{
	constexpr std::string_view open = "<numLaps>";
	constexpr std::string_view close = "</numLaps>";
	const auto start = map.find(open);
	const auto end = map.find(close, start);
	if (start == std::string_view::npos || end == std::string_view::npos)
		return 1;
	std::uint32_t laps = 0;
	std::istringstream value(std::string(map.substr(start + open.size(), end - start - open.size())));
	value >> laps;
	return laps == 0 ? 1 : laps;
}

void requireAsset(const r3d::resource::ResourceFileSystem &resources, std::string_view virtual_path)
{
	if (!resources.exists(virtual_path) || resources.fileSize(virtual_path) == 0)
		throw std::runtime_error("Required race asset is unavailable: " + std::string(virtual_path));
}

} // namespace

PortableRaceData loadPortableRace(const r3d::resource::ResourceFileSystem &resources)
{
	PortableRaceData result;
	result.mapVirtualPath = "Data/Map/debugTrack.r3dMap";
	const std::string map = resources.readText(result.mapVirtualPath);
	const auto positions = parseTrackPositions(map);
	if (positions.size() < 12)
		throw std::runtime_error("debugTrack.r3dMap does not contain enough track pieces");

	float minimum_x = std::numeric_limits<float>::max();
	float maximum_x = std::numeric_limits<float>::lowest();
	float minimum_z = std::numeric_limits<float>::max();
	float maximum_z = std::numeric_limits<float>::lowest();
	for (const auto &position : positions)
	{
		minimum_x = std::min(minimum_x, position.x);
		maximum_x = std::max(maximum_x, position.x);
		minimum_z = std::min(minimum_z, position.z);
		maximum_z = std::max(maximum_z, position.z);
	}

	result.track.radius = std::max(8.0F, 0.5F * (maximum_z - minimum_z));
	result.track.centerZ = 0.5F * (minimum_z + maximum_z);
	result.track.leftCenterX = minimum_x + result.track.radius;
	result.track.rightCenterX = maximum_x - result.track.radius;
	result.track.roadHalfWidth = std::clamp(result.track.radius * 0.36F, 5.5F, 7.25F);
	result.track.startX = result.track.leftCenterX + 20.0F;
	const float track_center_x = 0.5F * (result.track.leftCenterX + result.track.rightCenterX);
	result.track.rampStartX = track_center_x - 6.0F;
	result.track.rampPeakX = track_center_x + 5.0F;
	result.track.rampHeight = 1.45F;
	result.sourceTrackPieces = positions.size();
	result.laps = parseLapCount(map);

	const std::vector<std::string_view> visual_assets = {
		"Data/World2/Track/track1.r3d", "Data/World2/Track/track2.r3d", "Data/Car/buggi.r3d"};
	for (const auto asset : visual_assets)
		requireAsset(resources, asset);
	const std::vector<std::string_view> collision_assets = {
		"Data/World2/Track/pxTrack1.r3d", "Data/World2/Track/pxTrack2.r3d"};
	for (const auto asset : collision_assets)
	{
		requireAsset(resources, asset);
		++result.sourceCollisionMeshes;
	}
	return result;
}

PortableRaceSession::PortableRaceSession(const PortableRaceData &data)
    : world_(r3d::physics::createMinimalVehiclePhysics(data.track))
{
}

PortableRaceSession::~PortableRaceSession() = default;

void PortableRaceSession::reset() noexcept
{
	world_->reset();
}

void PortableRaceSession::step(float seconds, const r3d::physics::VehicleInput &input) noexcept
{
	world_->step(seconds, input);
}

r3d::physics::VehicleInput PortableRaceSession::autopilotInput() const noexcept
{
	const auto &state = world_->vehicle();
	const float current = world_->nearestTrackDistance(state.position);
	const float look_ahead = 8.5F + state.speed * 0.5F;
	const auto target = world_->sampleTrack(current + look_ahead);
	const float desired_heading = std::atan2(target.position.z - state.position.z, target.position.x - state.position.x);
	const float error = wrapAngle(desired_heading - state.heading);

	r3d::physics::VehicleInput input;
	input.steering = std::clamp(error * 1.85F, -1.0F, 1.0F);
	input.throttle = std::abs(error) < 0.72F ? 1.0F : 0.62F;
	input.brake = std::abs(error) > 1.05F ? 0.38F : 0.0F;
	return input;
}

const r3d::physics::VehicleState &PortableRaceSession::vehicle() const noexcept
{
	return world_->vehicle();
}

const r3d::physics::TrackGeometry &PortableRaceSession::track() const noexcept
{
	return world_->track();
}

float PortableRaceSession::trackLength() const noexcept
{
	return world_->trackLength();
}

r3d::physics::TrackSample PortableRaceSession::sampleTrack(float distance) const noexcept
{
	return world_->sampleTrack(distance);
}

float PortableRaceSession::nearestTrackDistance(const r3d::physics::Vec3 &position) const noexcept
{
	return world_->nearestTrackDistance(position);
}

bool runPortableRaceSmokeTest(const PortableRaceData &data, RaceSmokeResult &result, std::string &error)
{
	if (!r3d::physics::runMinimalVehiclePhysicsSmokeTest(data.track, error))
		return false;

	PortableRaceSession race(data);
	result = {};
	constexpr float step_seconds = 1.0F / 120.0F;
	constexpr int maximum_steps = 120 * 90;
	for (int step = 0; step < maximum_steps && !race.vehicle().finished; ++step)
	{
		race.step(step_seconds, race.autopilotInput());
		result.maximumSpeed = std::max(result.maximumSpeed, race.vehicle().speed);
	}

	const auto &state = race.vehicle();
	result.finishSeconds = state.elapsedSeconds;
	result.collisions = state.collisionCount;
	result.checkpoints = state.checkpointsPassed;
	if (!state.finished || state.checkpointsPassed != 3U || state.lapProgress < 1.0F)
	{
		std::ostringstream message;
		message << "autopilot did not complete one lap (progress=" << state.lapProgress
		        << ", checkpoints=" << static_cast<unsigned int>(state.checkpointsPassed)
		        << ", collisions=" << state.collisionCount << ')';
		error = message.str();
		return false;
	}
	if (result.maximumSpeed < 15.0F)
	{
		error = "autopilot did not reach race speed";
		return false;
	}
	error.clear();
	return true;
}

} // namespace rrr3d::race
