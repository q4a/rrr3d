#ifndef R3D_GRAPH_TYPES
#define R3D_GRAPH_TYPES

#include "DriverTypes.h"
#include "r3dMath.h"

namespace r3d
{

const VertexPNT PlanePNT[4] = {VertexPNT(D3DXVECTOR3(-0.5f, -0.5f, 0.0f), ZVector, D3DXVECTOR2(0.0f, 0.0f)), 
                               VertexPNT(D3DXVECTOR3(0.5f, -0.5f, 0.0f), ZVector, D3DXVECTOR2(1.0f, 0.0f)),
                               VertexPNT(D3DXVECTOR3(0.5f, 0.5f, 0.0f), ZVector, D3DXVECTOR2(1.0f, 1.0f)),
                               VertexPNT(D3DXVECTOR3(-0.5f, 0.5f, 0.0f), ZVector, D3DXVECTOR2(0.0f, 1.0f))};

}

#endif