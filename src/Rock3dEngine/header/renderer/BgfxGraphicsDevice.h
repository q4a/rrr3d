#pragma once

#include "renderer/Renderer.h"

#include <memory>

namespace r3d::renderer
{

std::unique_ptr<GraphicsDevice> createBgfxGraphicsDevice();

} // namespace r3d::renderer
