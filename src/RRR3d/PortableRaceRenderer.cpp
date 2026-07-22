#include "PortableRaceRenderer.h"

#include <bx/math.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace rrr3d::race
{
namespace
{

using namespace r3d::renderer;

bool valid(Mesh mesh) noexcept
{
	return mesh.vertices.value != invalid_resource && mesh.indices.value != invalid_resource;
}

bool valid(Texture texture) noexcept
{
	return texture.value != invalid_resource;
}

Transform transform(float sx, float sy, float sz, float ry, float tx, float ty, float tz) noexcept
{
	Transform result;
	bx::mtxSRT(result.matrix.data(), sx, sy, sz, 0.0F, ry, 0.0F, tx, ty, tz);
	return result;
}

Texture solidTexture(GraphicsDevice &device, std::array<std::uint8_t, 3> rgb)
{
	const std::array<std::uint8_t, 4> pixel = {rgb[0], rgb[1], rgb[2], 255};
	return device.createTextureRgba8(1, 1, pixel.data(), pixel.size());
}

constexpr std::array<Vertex, 24> cubeVertices = {{
	{-1.0F, 1.0F, 1.0F, 0xffffffffU, 0.0F, 0.0F},
	{1.0F, 1.0F, 1.0F, 0xffffffffU, 1.0F, 0.0F},
	{-1.0F, -1.0F, 1.0F, 0xffffffffU, 0.0F, 1.0F},
	{1.0F, -1.0F, 1.0F, 0xffffffffU, 1.0F, 1.0F},
	{1.0F, 1.0F, -1.0F, 0xffffffffU, 0.0F, 0.0F},
	{-1.0F, 1.0F, -1.0F, 0xffffffffU, 1.0F, 0.0F},
	{1.0F, -1.0F, -1.0F, 0xffffffffU, 0.0F, 1.0F},
	{-1.0F, -1.0F, -1.0F, 0xffffffffU, 1.0F, 1.0F},
	{-1.0F, 1.0F, -1.0F, 0xffffffffU, 0.0F, 0.0F},
	{-1.0F, 1.0F, 1.0F, 0xffffffffU, 1.0F, 0.0F},
	{-1.0F, -1.0F, -1.0F, 0xffffffffU, 0.0F, 1.0F},
	{-1.0F, -1.0F, 1.0F, 0xffffffffU, 1.0F, 1.0F},
	{1.0F, 1.0F, 1.0F, 0xffffffffU, 0.0F, 0.0F},
	{1.0F, 1.0F, -1.0F, 0xffffffffU, 1.0F, 0.0F},
	{1.0F, -1.0F, 1.0F, 0xffffffffU, 0.0F, 1.0F},
	{1.0F, -1.0F, -1.0F, 0xffffffffU, 1.0F, 1.0F},
	{-1.0F, 1.0F, -1.0F, 0xffffffffU, 0.0F, 0.0F},
	{1.0F, 1.0F, -1.0F, 0xffffffffU, 1.0F, 0.0F},
	{-1.0F, 1.0F, 1.0F, 0xffffffffU, 0.0F, 1.0F},
	{1.0F, 1.0F, 1.0F, 0xffffffffU, 1.0F, 1.0F},
	{-1.0F, -1.0F, 1.0F, 0xffffffffU, 0.0F, 0.0F},
	{1.0F, -1.0F, 1.0F, 0xffffffffU, 1.0F, 0.0F},
	{-1.0F, -1.0F, -1.0F, 0xffffffffU, 0.0F, 1.0F},
	{1.0F, -1.0F, -1.0F, 0xffffffffU, 1.0F, 1.0F},
}};

constexpr std::array<std::uint16_t, 36> cubeIndices = {{
	0, 1, 2, 1, 3, 2, 4, 5, 6, 5, 7, 6, 8, 9, 10, 9, 11, 10,
	12, 13, 14, 13, 15, 14, 16, 17, 18, 17, 19, 18, 20, 21, 22, 21, 23, 22,
}};

} // namespace

bool PortableRaceRenderer::initialize(GraphicsDevice &device, const PortableRaceSession &race, std::string &error)
{
	const auto &track = race.track();
	const float margin = track.roadHalfWidth + 24.0F;
	const float minimum_x = track.leftCenterX - track.radius - margin;
	const float maximum_x = track.rightCenterX + track.radius + margin;
	const float minimum_z = track.centerZ - track.radius - margin;
	const float maximum_z = track.centerZ + track.radius + margin;
	const std::array<Vertex, 4> ground_vertices = {{
		{minimum_x, -0.18F, minimum_z, 0xffffffffU, 0.0F, 0.0F},
		{maximum_x, -0.18F, minimum_z, 0xffffffffU, 1.0F, 0.0F},
		{minimum_x, -0.18F, maximum_z, 0xffffffffU, 0.0F, 1.0F},
		{maximum_x, -0.18F, maximum_z, 0xffffffffU, 1.0F, 1.0F},
	}};
	constexpr std::array<std::uint16_t, 6> quad_indices = {{0, 2, 1, 1, 2, 3}};
	ground_ = device.createMesh(ground_vertices.data(), ground_vertices.size(), quad_indices.data(), quad_indices.size());

	constexpr std::size_t road_segments = 192;
	std::vector<Vertex> road_vertices;
	std::vector<std::uint16_t> road_indices;
	road_vertices.reserve((road_segments + 1U) * 2U);
	road_indices.reserve(road_segments * 6U);
	for (std::size_t index = 0; index <= road_segments; ++index)
	{
		const float distance = race.trackLength() * static_cast<float>(index) / static_cast<float>(road_segments);
		const auto sample = race.sampleTrack(distance);
		const float normal_x = -sample.tangent.z;
		const float normal_z = sample.tangent.x;
		const float y = sample.position.y + 0.02F;
		const float u = static_cast<float>(index) * 0.125F;
		road_vertices.push_back({sample.position.x + normal_x * track.roadHalfWidth, y,
		                         sample.position.z + normal_z * track.roadHalfWidth, 0xffffffffU, u, 0.0F});
		road_vertices.push_back({sample.position.x - normal_x * track.roadHalfWidth, y,
		                         sample.position.z - normal_z * track.roadHalfWidth, 0xffffffffU, u, 1.0F});
		if (index < road_segments)
		{
			const auto base = static_cast<std::uint16_t>(index * 2U);
			road_indices.insert(road_indices.end(), {base, static_cast<std::uint16_t>(base + 2U),
			                                         static_cast<std::uint16_t>(base + 1U),
			                                         static_cast<std::uint16_t>(base + 1U),
			                                         static_cast<std::uint16_t>(base + 2U),
			                                         static_cast<std::uint16_t>(base + 3U)});
		}
	}
	road_ = device.createMesh(road_vertices.data(), road_vertices.size(), road_indices.data(), road_indices.size());
	cube_ = device.createMesh(cubeVertices.data(), cubeVertices.size(), cubeIndices.data(), cubeIndices.size());

	grass_ = solidTexture(device, {17, 49, 28});
	roadTexture_ = solidTexture(device, {70, 72, 78});
	railTexture_ = solidTexture(device, {218, 91, 35});
	carTexture_ = solidTexture(device, {206, 38, 31});
	cabinTexture_ = solidTexture(device, {37, 55, 68});
	tireTexture_ = solidTexture(device, {18, 18, 21});
	markerTexture_ = solidTexture(device, {243, 226, 103});

	constexpr std::size_t rail_segments = 72;
	const float rail_length = race.trackLength() / static_cast<float>(rail_segments);
	rails_.reserve(rail_segments * 2U);
	for (std::size_t index = 0; index < rail_segments; ++index)
	{
		const auto sample = race.sampleTrack(race.trackLength() * (static_cast<float>(index) + 0.5F) /
		                                     static_cast<float>(rail_segments));
		const float normal_x = -sample.tangent.z;
		const float normal_z = sample.tangent.x;
		const float heading = std::atan2(sample.tangent.z, sample.tangent.x);
		for (float side : {-1.0F, 1.0F})
		{
			rails_.push_back(transform(rail_length * 0.48F, 0.38F, 0.16F, -heading,
			                           sample.position.x + normal_x * track.roadHalfWidth * side,
			                           sample.position.y + 0.38F,
			                           sample.position.z + normal_z * track.roadHalfWidth * side));
		}
	}

	for (int index = 0; index < 18; ++index)
	{
		const float x = minimum_x + 10.0F + static_cast<float>((index * 37) % 170);
		const float z = index % 2 == 0 ? minimum_z + 8.0F + static_cast<float>(index % 4) * 3.0F
		                                 : maximum_z - 8.0F - static_cast<float>(index % 4) * 3.0F;
		scenery_.push_back(transform(1.2F, 2.0F + static_cast<float>(index % 3), 1.2F, 0.0F, x, 1.8F, z));
	}

	if (!valid(ground_) || !valid(road_) || !valid(cube_) || !valid(grass_) || !valid(roadTexture_) ||
	    !valid(railTexture_) || !valid(carTexture_) || !valid(cabinTexture_) || !valid(tireTexture_) ||
	    !valid(markerTexture_))
	{
		error = "Unable to allocate the portable race GPU resources";
		shutdown(device);
		return false;
	}
	error.clear();
	return true;
}

void PortableRaceRenderer::shutdown(GraphicsDevice &device) noexcept
{
	for (Texture texture : {markerTexture_, tireTexture_, cabinTexture_, carTexture_, railTexture_, roadTexture_, grass_})
	{
		if (valid(texture))
			device.destroy(texture);
	}
	for (Mesh mesh : {cube_, road_, ground_})
	{
		if (valid(mesh))
			device.destroy(mesh);
	}
	ground_ = {};
	road_ = {};
	cube_ = {};
	grass_ = {};
	roadTexture_ = {};
	railTexture_ = {};
	carTexture_ = {};
	cabinTexture_ = {};
	tireTexture_ = {};
	markerTexture_ = {};
	rails_.clear();
	scenery_.clear();
}

Camera PortableRaceRenderer::makeCamera(const GraphicsDevice &device, const PortableRaceSession &race,
	                                      std::uint32_t width, std::uint32_t height) const noexcept
{
	const auto &vehicle = race.vehicle();
	const float direction_x = std::cos(vehicle.heading);
	const float direction_z = std::sin(vehicle.heading);
	const bx::Vec3 eye = {vehicle.position.x - direction_x * 13.0F, vehicle.position.y + 7.5F,
	                     vehicle.position.z - direction_z * 13.0F};
	const bx::Vec3 at = {vehicle.position.x + direction_x * 5.0F, vehicle.position.y + 0.5F,
	                    vehicle.position.z + direction_z * 5.0F};
	Camera camera;
	bx::mtxLookAt(camera.view.data(), eye, at);
	bx::mtxProj(camera.projection.data(), 62.0F, static_cast<float>(std::max(width, 1U)) /
	                                                      static_cast<float>(std::max(height, 1U)),
	            0.1F, 450.0F, device.usesHomogeneousDepth());
	return camera;
}

void PortableRaceRenderer::draw(GraphicsDevice &device, Shader shader, const PortableRaceSession &race,
	                              const PipelineState &pipeline) const
{
	Transform identity;
	bx::mtxIdentity(identity.matrix.data());
	device.draw(ground_, shader, grass_, identity, pipeline);
	device.draw(road_, shader, roadTexture_, identity, pipeline);
	for (const auto &rail : rails_)
		device.draw(cube_, shader, railTexture_, rail, pipeline);
	for (const auto &object : scenery_)
		device.draw(cube_, shader, grass_, object, pipeline);

	const auto start = race.sampleTrack(race.track().startX - race.track().leftCenterX);
	device.draw(cube_, shader, markerTexture_,
	            transform(0.32F, 0.035F, race.track().roadHalfWidth * 0.92F, 0.0F, start.position.x,
	                      start.position.y + 0.08F, start.position.z),
	            pipeline);
	for (int checkpoint = 1; checkpoint <= 3; ++checkpoint)
	{
		const auto marker = race.sampleTrack((race.track().startX - race.track().leftCenterX) +
		                                     race.trackLength() * static_cast<float>(checkpoint) * 0.25F);
		device.draw(cube_, shader, markerTexture_,
		            transform(0.28F, 1.25F, 0.28F, 0.0F, marker.position.x, marker.position.y + 1.25F,
		                      marker.position.z),
		            pipeline);
	}

	const auto &car = race.vehicle();
	const float rotation = -car.heading;
	device.draw(cube_, shader, carTexture_,
	            transform(1.65F, 0.42F, 0.88F, rotation, car.position.x, car.position.y, car.position.z), pipeline);
	device.draw(cube_, shader, cabinTexture_,
	            transform(0.67F, 0.36F, 0.72F, rotation, car.position.x - std::cos(car.heading) * 0.18F,
	                      car.position.y + 0.61F, car.position.z - std::sin(car.heading) * 0.18F),
	            pipeline);
	const float forward_x = std::cos(car.heading);
	const float forward_z = std::sin(car.heading);
	const float side_x = -forward_z;
	const float side_z = forward_x;
	for (float longitudinal : {-1.05F, 1.05F})
	{
		for (float lateral : {-0.92F, 0.92F})
		{
			device.draw(cube_, shader, tireTexture_,
			            transform(0.38F, 0.38F, 0.19F, rotation,
			                      car.position.x + forward_x * longitudinal + side_x * lateral, car.position.y - 0.34F,
			                      car.position.z + forward_z * longitudinal + side_z * lateral),
			            pipeline);
		}
	}
}

} // namespace rrr3d::race
