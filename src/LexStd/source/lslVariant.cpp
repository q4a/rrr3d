#include "stdafx.h"

#include "lslVariant.h"

namespace lsl
{

const unsigned Variant::cTypeSize[cTypeEnd] = {1, sizeof(int), sizeof(unsigned), sizeof(float), sizeof(double), sizeof(bool), sizeof(char)};

const unsigned VariantVec::cMyTypeSize[cMyTypeEnd] = {sizeof(D3DXVECTOR2), sizeof(D3DXVECTOR3), sizeof(D3DXVECTOR4), sizeof(D3DXMATRIX)};

}