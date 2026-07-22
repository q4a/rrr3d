#pragma once

#include "PortableRace.h"
#include "renderer/Renderer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rrr3d::race
{

class PortableRaceRenderer
{
  public:
	bool initialize(r3d::renderer::GraphicsDevice &device, const PortableRaceSession &race, std::string &error);
	void shutdown(r3d::renderer::GraphicsDevice &device) noexcept;

	r3d::renderer::Camera makeCamera(const r3d::renderer::GraphicsDevice &device, const PortableRaceSession &race,
	                                 std::uint32_t width, std::uint32_t height) const noexcept;
	void draw(r3d::renderer::GraphicsDevice &device, r3d::renderer::Shader shader,
	          const PortableRaceSession &race, const r3d::renderer::PipelineState &pipeline) const;

  private:
	r3d::renderer::Mesh ground_;
	r3d::renderer::Mesh road_;
	r3d::renderer::Mesh cube_;
	r3d::renderer::Texture grass_;
	r3d::renderer::Texture roadTexture_;
	r3d::renderer::Texture railTexture_;
	r3d::renderer::Texture carTexture_;
	r3d::renderer::Texture cabinTexture_;
	r3d::renderer::Texture tireTexture_;
	r3d::renderer::Texture markerTexture_;
	std::vector<r3d::renderer::Transform> rails_;
	std::vector<r3d::renderer::Transform> scenery_;
};

} // namespace rrr3d::race
