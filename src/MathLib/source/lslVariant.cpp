#include "stdafx.h"

#include "lslVariant.h"

//namespace lsl
//{

const unsigned Variant::cTypeSize[cTypeEnd] = {1, sizeof(int), sizeof(unsigned), sizeof(float), sizeof(double), sizeof(bool), sizeof(char)};

const unsigned VariantVec::cMyTypeSize[cMyTypeEnd] = {sizeof(glm::vec2), sizeof(D3DXVECTOR3), sizeof(D3DXVECTOR4), sizeof(glm::mat4)};

//}