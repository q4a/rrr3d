#ifndef R3D_GRAPH_TYPES
#define R3D_GRAPH_TYPES

#include "DriverTypes.h"
#include "r3dMath.h"

namespace r3d
{

const VertexPNT PlanePNT[4] = {VertexPNT(glm::vec3(-0.5f, -0.5f, 0.0f), ZVector, glm::vec2(0.0f, 0.0f)),
                               VertexPNT(glm::vec3(0.5f,  -0.5f, 0.0f), ZVector, glm::vec2(1.0f, 0.0f)),
                               VertexPNT(glm::vec3(0.5f,   0.5f, 0.0f), ZVector, glm::vec2(1.0f, 1.0f)),
                               VertexPNT(glm::vec3(-0.5f,  0.5f, 0.0f), ZVector, glm::vec2(0.0f, 1.0f))};

}

#endif