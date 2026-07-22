#include "physics/PhysicsBackend.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace r3d::physics
{
namespace
{

constexpr float pi = 3.14159265358979323846F;
constexpr float twoPi = 2.0F * pi;
constexpr float vehicleRadius = 1.15F;
constexpr float vehicleRideHeight = 0.68F;
constexpr float maximumStep = 1.0F / 30.0F;

float clampUnit(float value) noexcept
{
	return std::clamp(value, -1.0F, 1.0F);
}

float wrapAngle(float angle) noexcept
{
	while (angle > pi)
		angle -= twoPi;
	while (angle < -pi)
		angle += twoPi;
	return angle;
}

float positiveModulo(float value, float modulus) noexcept
{
	const float result = std::fmod(value, modulus);
	return result < 0.0F ? result + modulus : result;
}

struct Projection
{
	float x = 0.0F;
	float z = 0.0F;
	float distanceSquared = std::numeric_limits<float>::max();
	float trackDistance = 0.0F;
};

class MinimalVehiclePhysics final : public PhysicsWorld
{
  public:
	explicit MinimalVehiclePhysics(TrackGeometry track) : track_(track)
	{
		track_.radius = std::max(track_.radius, 4.0F);
		track_.roadHalfWidth = std::clamp(track_.roadHalfWidth, vehicleRadius + 0.5F, track_.radius - 0.5F);
		if (track_.rightCenterX <= track_.leftCenterX + 8.0F)
			track_.rightCenterX = track_.leftCenterX + 8.0F;
		straightLength_ = track_.rightCenterX - track_.leftCenterX;
		trackLength_ = 2.0F * straightLength_ + twoPi * track_.radius;
		startDistance_ = std::clamp(track_.startX - track_.leftCenterX, 1.0F, straightLength_ - 1.0F);
		reset();
	}

	void reset() noexcept override
	{
		const std::uint32_t reset_count = state_.resetCount;
		state_ = {};
		state_.position = {track_.startX, groundHeight(track_.startX, track_.centerZ - track_.radius) + vehicleRideHeight,
		                   track_.centerZ - track_.radius};
		state_.heading = 0.0F;
		state_.grounded = true;
		state_.resetCount = reset_count + 1U;
		lastTrackDistance_ = startDistance_;
		raceDistance_ = 0.0F;
		collisionCooldown_ = 0.0F;
	}

	void reset(const Vec3 &position, float heading) noexcept override
	{
		const std::uint32_t reset_count = state_.resetCount;
		state_ = {};
		state_.position = position;
		state_.heading = wrapAngle(heading);
		state_.position.y = std::max(state_.position.y, groundHeight(position.x, position.z) + vehicleRideHeight);
		state_.grounded = state_.position.y <= groundHeight(position.x, position.z) + vehicleRideHeight + 0.01F;
		state_.resetCount = reset_count + 1U;
		lastTrackDistance_ = nearestTrackDistance(state_.position);
		raceDistance_ = 0.0F;
		collisionCooldown_ = 0.0F;
	}

	void step(float seconds, const VehicleInput &raw_input) noexcept override
	{
		float remaining = std::clamp(seconds, 0.0F, 0.25F);
		while (remaining > 0.0F)
		{
			const float dt = std::min(remaining, maximumStep);
			stepFixed(dt, raw_input);
			remaining -= dt;
		}
	}

	const VehicleState &vehicle() const noexcept override
	{
		return state_;
	}

	const TrackGeometry &track() const noexcept override
	{
		return track_;
	}

	float trackLength() const noexcept override
	{
		return trackLength_;
	}

	TrackSample sampleTrack(float distance) const noexcept override
	{
		const float s = positiveModulo(distance, trackLength_);
		TrackSample sample;
		sample.distance = s;
		const float bottom_z = track_.centerZ - track_.radius;
		const float top_z = track_.centerZ + track_.radius;
		if (s < straightLength_)
		{
			sample.position = {track_.leftCenterX + s, 0.0F, bottom_z};
			sample.tangent = {1.0F, 0.0F, 0.0F};
		}
		else if (s < straightLength_ + pi * track_.radius)
		{
			const float angle = -0.5F * pi + (s - straightLength_) / track_.radius;
			sample.position = {track_.rightCenterX + std::cos(angle) * track_.radius, 0.0F,
			                   track_.centerZ + std::sin(angle) * track_.radius};
			sample.tangent = {-std::sin(angle), 0.0F, std::cos(angle)};
		}
		else if (s < 2.0F * straightLength_ + pi * track_.radius)
		{
			const float along = s - straightLength_ - pi * track_.radius;
			sample.position = {track_.rightCenterX - along, 0.0F, top_z};
			sample.tangent = {-1.0F, 0.0F, 0.0F};
		}
		else
		{
			const float angle = 0.5F * pi + (s - 2.0F * straightLength_ - pi * track_.radius) / track_.radius;
			sample.position = {track_.leftCenterX + std::cos(angle) * track_.radius, 0.0F,
			                   track_.centerZ + std::sin(angle) * track_.radius};
			sample.tangent = {-std::sin(angle), 0.0F, std::cos(angle)};
		}
		sample.position.y = groundHeight(sample.position.x, sample.position.z);
		return sample;
	}

	float nearestTrackDistance(const Vec3 &position) const noexcept override
	{
		return project(position.x, position.z).trackDistance;
	}

  private:
	Projection project(float x, float z) const noexcept
	{
		Projection best;
		auto consider = [&](float candidate_x, float candidate_z, float distance) {
			const float dx = x - candidate_x;
			const float dz = z - candidate_z;
			const float squared = dx * dx + dz * dz;
			if (squared < best.distanceSquared)
				best = {candidate_x, candidate_z, squared, positiveModulo(distance, trackLength_)};
		};

		const float clamped_x = std::clamp(x, track_.leftCenterX, track_.rightCenterX);
		consider(clamped_x, track_.centerZ - track_.radius, clamped_x - track_.leftCenterX);
		consider(clamped_x, track_.centerZ + track_.radius,
		         straightLength_ + pi * track_.radius + track_.rightCenterX - clamped_x);

		float right_angle = std::atan2(z - track_.centerZ, x - track_.rightCenterX);
		right_angle = std::clamp(right_angle, -0.5F * pi, 0.5F * pi);
		consider(track_.rightCenterX + std::cos(right_angle) * track_.radius,
		         track_.centerZ + std::sin(right_angle) * track_.radius,
		         straightLength_ + (right_angle + 0.5F * pi) * track_.radius);

		float left_angle = std::atan2(z - track_.centerZ, x - track_.leftCenterX);
		if (left_angle < 0.5F * pi)
			left_angle += twoPi;
		left_angle = std::clamp(left_angle, 0.5F * pi, 1.5F * pi);
		consider(track_.leftCenterX + std::cos(left_angle) * track_.radius,
		         track_.centerZ + std::sin(left_angle) * track_.radius,
		         2.0F * straightLength_ + pi * track_.radius + (left_angle - 0.5F * pi) * track_.radius);
		return best;
	}

	float groundHeight(float x, float z) const noexcept
	{
		const float bottom_z = track_.centerZ - track_.radius;
		if (std::abs(z - bottom_z) > track_.roadHalfWidth || x < track_.rampStartX || x > track_.rampPeakX)
			return 0.0F;
		const float ramp_length = std::max(track_.rampPeakX - track_.rampStartX, 0.1F);
		return track_.rampHeight * (x - track_.rampStartX) / ramp_length;
	}

	void updateRaceProgress() noexcept
	{
		const float current = nearestTrackDistance(state_.position);
		float delta = current - lastTrackDistance_;
		if (delta > 0.5F * trackLength_)
			delta -= trackLength_;
		else if (delta < -0.5F * trackLength_)
			delta += trackLength_;
		if (std::abs(delta) < 8.0F)
			raceDistance_ = std::max(0.0F, raceDistance_ + delta);
		lastTrackDistance_ = current;

		const float progress = raceDistance_ / trackLength_;
		state_.lapProgress = std::clamp(progress, 0.0F, 1.0F);
		while (state_.checkpointsPassed < 3U &&
		       progress >= 0.25F * static_cast<float>(state_.checkpointsPassed + 1U))
		{
			++state_.checkpointsPassed;
		}
		if (state_.checkpointsPassed == 3U && progress >= 1.0F)
			state_.finished = true;
	}

	void stepFixed(float dt, const VehicleInput &raw_input) noexcept
	{
		VehicleInput input = raw_input;
		input.throttle = std::clamp(input.throttle, 0.0F, 1.0F);
		input.brake = std::clamp(input.brake, 0.0F, 1.0F);
		input.steering = clampUnit(input.steering);

		const float drive_force = input.throttle * 11.5F;
		const float brake_force = input.brake * 22.0F;
		const float drag = 0.018F * state_.speed * state_.speed + 0.32F;
		state_.speed = std::clamp(state_.speed + (drive_force - brake_force - drag) * dt, 0.0F, 34.0F);
		if (state_.finished)
			state_.speed = std::max(0.0F, state_.speed - 6.0F * dt);

		const float steering_rate = input.steering * std::min(1.65F, state_.speed * 0.075F);
		state_.heading = wrapAngle(state_.heading + steering_rate * dt);
		state_.position.x += std::cos(state_.heading) * state_.speed * dt;
		state_.position.z += std::sin(state_.heading) * state_.speed * dt;

		const Projection projection = project(state_.position.x, state_.position.z);
		const float distance = std::sqrt(projection.distanceSquared);
		const float permitted = track_.roadHalfWidth - vehicleRadius;
		if (distance > permitted)
		{
			const float inverse_distance = distance > 0.0001F ? 1.0F / distance : 0.0F;
			const float nx = (state_.position.x - projection.x) * inverse_distance;
			const float nz = (state_.position.z - projection.z) * inverse_distance;
			state_.position.x = projection.x + nx * permitted;
			state_.position.z = projection.z + nz * permitted;

			float velocity_x = std::cos(state_.heading) * state_.speed;
			float velocity_z = std::sin(state_.heading) * state_.speed;
			const float outward = velocity_x * nx + velocity_z * nz;
			if (outward > 0.0F)
			{
				velocity_x -= 1.25F * outward * nx;
				velocity_z -= 1.25F * outward * nz;
				state_.speed = std::sqrt(velocity_x * velocity_x + velocity_z * velocity_z) * 0.72F;
				if (state_.speed > 0.01F)
					state_.heading = std::atan2(velocity_z, velocity_x);
			}
			if (collisionCooldown_ <= 0.0F)
			{
				++state_.collisionCount;
				collisionCooldown_ = 0.18F;
			}
		}
		collisionCooldown_ = std::max(0.0F, collisionCooldown_ - dt);

		const float surface = groundHeight(state_.position.x, state_.position.z) + vehicleRideHeight;
		if (state_.grounded)
		{
			if (surface + 0.12F < state_.position.y)
			{
				state_.grounded = false;
			}
			else
			{
				state_.verticalVelocity = std::max(0.0F, (surface - state_.position.y) / dt);
				state_.position.y = surface;
			}
		}
		if (!state_.grounded)
		{
			state_.verticalVelocity -= 9.81F * dt;
			state_.position.y += state_.verticalVelocity * dt;
			if (state_.position.y <= surface && state_.verticalVelocity <= 0.0F)
			{
				state_.position.y = surface;
				state_.verticalVelocity = 0.0F;
				state_.grounded = true;
			}
		}

		state_.elapsedSeconds += dt;
		updateRaceProgress();
	}

	TrackGeometry track_;
	VehicleState state_;
	float straightLength_ = 0.0F;
	float trackLength_ = 0.0F;
	float startDistance_ = 0.0F;
	float lastTrackDistance_ = 0.0F;
	float raceDistance_ = 0.0F;
	float collisionCooldown_ = 0.0F;
};

bool approximatelyEqual(const VehicleState &left, const VehicleState &right) noexcept
{
	return std::abs(left.position.x - right.position.x) < 0.0005F &&
	       std::abs(left.position.y - right.position.y) < 0.0005F &&
	       std::abs(left.position.z - right.position.z) < 0.0005F &&
	       std::abs(left.heading - right.heading) < 0.0005F && std::abs(left.speed - right.speed) < 0.0005F;
}

} // namespace

std::unique_ptr<PhysicsWorld> createMinimalVehiclePhysics(const TrackGeometry &track)
{
	return std::make_unique<MinimalVehiclePhysics>(track);
}

bool runMinimalVehiclePhysicsSmokeTest(const TrackGeometry &track, std::string &error)
{
	auto world = createMinimalVehiclePhysics(track);
	VehicleInput input;
	input.throttle = 1.0F;
	for (int step = 0; step < 240; ++step)
		world->step(1.0F / 120.0F, input);
	const float accelerated_speed = world->vehicle().speed;
	if (accelerated_speed < 12.0F)
	{
		error = "acceleration did not reach the deterministic speed threshold";
		return false;
	}

	input = {};
	input.brake = 1.0F;
	for (int step = 0; step < 120; ++step)
		world->step(1.0F / 120.0F, input);
	if (world->vehicle().speed >= accelerated_speed * 0.45F)
	{
		error = "braking did not reduce speed";
		return false;
	}

	world->reset();
	input = {};
	input.throttle = 1.0F;
	input.steering = 0.65F;
	float maximum_heading_change = 0.0F;
	for (int step = 0; step < 240; ++step)
	{
		world->step(1.0F / 120.0F, input);
		maximum_heading_change = std::max(maximum_heading_change, std::abs(world->vehicle().heading));
	}
	if (maximum_heading_change < 0.25F)
	{
		error = "steering did not change vehicle heading";
		return false;
	}

	const float bottom_z = track.centerZ - track.radius;
	world->reset({track.startX, vehicleRideHeight, bottom_z - track.roadHalfWidth + vehicleRadius + 0.05F}, -0.5F * pi);
	input = {};
	input.throttle = 1.0F;
	for (int step = 0; step < 180; ++step)
		world->step(1.0F / 120.0F, input);
	if (world->vehicle().collisionCount == 0U)
	{
		error = "vehicle did not collide with the track wall";
		return false;
	}

	world->reset({track.rampStartX - 2.0F, vehicleRideHeight, bottom_z}, 0.0F);
	input = {};
	input.throttle = 1.0F;
	bool became_airborne = false;
	bool landed = false;
	for (int step = 0; step < 720; ++step)
	{
		world->step(1.0F / 120.0F, input);
		became_airborne = became_airborne || !world->vehicle().grounded;
		landed = landed || (became_airborne && world->vehicle().grounded);
		if (landed)
			break;
	}
	if (!became_airborne || !landed)
	{
		error = "ramp take-off/landing cycle did not complete";
		return false;
	}

	auto replay_a = createMinimalVehiclePhysics(track);
	auto replay_b = createMinimalVehiclePhysics(track);
	input = {};
	input.throttle = 0.82F;
	input.steering = -0.17F;
	for (int step = 0; step < 480; ++step)
	{
		replay_a->step(1.0F / 120.0F, input);
		replay_b->step(1.0F / 120.0F, input);
	}
	if (!approximatelyEqual(replay_a->vehicle(), replay_b->vehicle()))
	{
		error = "fixed-step replay is not deterministic";
		return false;
	}

	error.clear();
	return true;
}

} // namespace r3d::physics
