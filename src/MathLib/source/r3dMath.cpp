#include "stdafx.h"

#include "r3dMath.h"

//namespace r3d
//{

PlanBB cBBPlans = {{0, 1, 2, 3},
                   {4, 5, 6, 7},
                   {0, 4, 7, 3},
                   {1, 5, 6, 2},
                   {0, 1, 5, 4},
                   {2, 3, 7, 6}};

DirPlan cDirPlan = {0, 0, 1, 1, 2, 2};

template<class _Value> _Value AbsMin(const _Value& val1, const _Value& val2)
{
	return abs(val1) > abs(val2) ? val2 : val1;
}

template<class _Value> _Value AbsMax(const _Value& val1, const _Value& val2)
{
	return abs(val1) < abs(val2) ? val2 : val1;
}

AABB2::AABB2()
{
}

AABB2::AABB2(float size)
{
	max.x = size/2.0f;
	max.y = size/2.0f;
	min = -max;
}

AABB2::AABB2(const glm::vec2& size)
{
	max = size/2.0f;
	min = -max;
}

AABB2::AABB2(const glm::vec2& mMin, const glm::vec2& mMax): min(mMin), max(mMax)
{
}

void AABB2::Transform(const AABB2 &aabb, const D3DMATRIX &mIn, AABB2 &rOut)
{
	auto m = Matrix4DxToGlm(mIn); // remove after D3DMATRIX replacement
	glm::vec2 oldMin = aabb.min;
	glm::vec2 oldMax = aabb.max;
	rOut.min = Vec2TransformCoord(oldMin, m);
	rOut.max = rOut.min;

	rOut.Include(Vec2TransformCoord(glm::vec2(oldMin.x, oldMin.y), m));
	rOut.Include(Vec2TransformCoord(glm::vec2(oldMin.x, oldMax.y), m));
	rOut.Include(Vec2TransformCoord(glm::vec2(oldMax.x, oldMin.y), m));
	rOut.Include(Vec2TransformCoord(glm::vec2(oldMax.x, oldMax.y), m));
	rOut.Include(Vec2TransformCoord(oldMax, m));
}

void AABB2::Include(const AABB2& aabb, const glm::vec2& vec, AABB2& rOut)
{
	rOut.min = Vec2Minimize(aabb.min, vec);
	rOut.max = Vec2Maximize(aabb.max, vec);
}

void AABB2::Add(const AABB2& aabb1, const AABB2& aabb2, AABB2& rOut)
{
	rOut.min = Vec2Minimize(aabb1.min, aabb2.min);
	rOut.max = Vec2Maximize(aabb1.max, aabb2.max);
}

void AABB2::Offset(const AABB2& aabb, const glm::vec2& vec, AABB2& rOut)
{
	rOut.min = aabb.min + vec;
	rOut.max = aabb.max + vec;
}

void AABB2::Transform(const D3DMATRIX &m)
{
	Transform(*this, m, *this);
}

void AABB2::Include(const glm::vec2& vec)
{
	Include(*this, vec, *this);
}

void AABB2::Add(const AABB2& aabb)
{
	Add(*this, aabb, *this);
}

void AABB2::Offset(const glm::vec2& vec)
{
	Offset(*this, vec, *this);
}

bool AABB2::ContainsPoint(const glm::vec2& point) const
{
	return (point.x > min.x && point.y > min.y &&
			point.x < max.x && point.y < max.y);
}

AABB2::SpaceContains AABB2::ContainsAABB(const AABB2& test) const
{
	if (min[0] < test.max[0] &&
		min[1] < test.max[1] &&

		test.min[0] < max[0] &&
		test.min[1] < max[1])

		if (test.min[0] >= min[0] &&
			test.min[1] >= min[1] &&

			test.max[0] <= max[0] &&
			test.max[1] <= max[1])

			return scContainsFully;
		else
			return scContainsPartially;
	else
		return scNoOverlap;
}

glm::vec2 AABB2::GetCenter() const
{
	return (min + max) / 2.0f;
}

glm::vec2 AABB2::GetSize() const
{
	return max - min;
}

AABB::AABB()
{
}

AABB::AABB(float size)
{
	float maxS = std::max(size / 2.0f, 0.0f);
	float minS = -maxS;
	min = glm::vec3(minS, minS, minS);
	max = glm::vec3(maxS, maxS, maxS);
}

AABB::AABB(const glm::vec3& sizes)
{
	FromDimensions(sizes / 2.0f);
}

AABB::AABB(const glm::vec3& minPoint, const glm::vec3& maxPoint): min(minPoint), max(maxPoint)
{
}

inline void AABB::Transform(const AABB& aabb, const D3DMATRIX& m, AABB& rOut)
{
	glm::vec3 oldMin = aabb.min;
	glm::vec3 oldMax = aabb.max;
	rOut.min = Vec3TransformCoord(oldMin, m);
	rOut.max = rOut.min;

	rOut.Include(Vec3TransformCoord(glm::vec3(oldMin[0], oldMin[1], oldMax[2]), m));
	rOut.Include(Vec3TransformCoord(glm::vec3(oldMin[0], oldMax[1], oldMin[2]), m));
	rOut.Include(Vec3TransformCoord(glm::vec3(oldMin[0], oldMax[1], oldMax[2]), m));
	rOut.Include(Vec3TransformCoord(glm::vec3(oldMax[0], oldMin[1], oldMin[2]), m));
	rOut.Include(Vec3TransformCoord(glm::vec3(oldMax[0], oldMin[1], oldMax[2]), m));
	rOut.Include(Vec3TransformCoord(glm::vec3(oldMax[0], oldMax[1], oldMin[2]), m));
	rOut.Include(Vec3TransformCoord(oldMax, m));
}

inline void AABB::Offset(const AABB& aabb, const glm::vec3& vec, AABB& rOut)
{
	rOut.min = aabb.min + vec;
	rOut.max = aabb.max + vec;
}

inline void AABB::Add(const AABB& aabb1, const AABB& aabb2, AABB& rOut)
{
	rOut.min = glm::min(aabb1.min, aabb2.min);
	rOut.max = glm::max(aabb1.max, aabb2.max);
}

inline void AABB::Include(const AABB& aabb, const glm::vec3& vec, AABB& rOut)
{
	rOut.min = glm::min(aabb.min, vec);
	rOut.max = glm::max(aabb.max, vec);
}

inline void AABB::Scale(const AABB& aabb, const glm::vec3& vec, AABB& rOut)
{
	rOut.min = aabb.min * vec;
	rOut.max = aabb.max * vec;
}

inline void AABB::Scale(const AABB& aabb, float f, AABB& rOut)
{
	rOut.min = aabb.min * f;
	rOut.max = aabb.max * f;
}

void AABB::FromPoints(const glm::vec3& pnt1, const glm::vec3& pnt2)
{
	min = glm::min(pnt1, pnt2);
	max = glm::max(pnt1, pnt2);
}

void AABB::FromDimensions(const glm::vec3& dimensions)
{
	max = glm::max(dimensions, NullVector);
	min = -max;
}

void AABB::Transform(const D3DMATRIX& m)
{
	Transform(*this, m, *this);
}

void AABB::Offset(const glm::vec3& vec)
{
	Offset(*this, vec, *this);
}

void AABB::Add(const AABB& aabb)
{
	Add(*this, aabb, *this);
}

void AABB::Include(const glm::vec3& vec)
{
	Include(*this, vec, *this);
}

void AABB::Scale(const glm::vec3& vec)
{
	Scale(*this, vec, *this);
}

void AABB::Scale(float f)
{
	Scale(*this, f, *this);
}

void AABB::ExtractCorners(Corners& corners) const
{
	corners[0] = glm::vec3(min[0], min[1], min[2]);
	corners[1] = glm::vec3(min[0], min[1], max[2]);
	corners[2] = glm::vec3(min[0], max[1], min[2]);
	corners[3] = glm::vec3(min[0], max[1], max[2]);

	corners[4] = glm::vec3(max[0], min[1], min[2]);
	corners[5] = glm::vec3(max[0], min[1], max[2]);
	corners[6] = glm::vec3(max[0], max[1], min[2]);
	corners[7] = glm::vec3(max[0], max[1], max[2]);
}

bool AABB::ContainsPoint(const glm::vec3& point) const
{
	return point > min && point < max;
}

AABB::SpaceContains AABB::ContainsAABB(const AABB& test) const
{
	if (min[0] < test.max[0] &&
		min[1] < test.max[1] &&
		min[2] < test.max[2] &&

		test.min[0] < max[0] &&
		test.min[1] < max[1] &&
		test.min[2] < max[2])

		if (test.min[0] >= min[0] &&
			test.min[1] >= min[1] &&
			test.min[2] >= min[2] &&

			test.max[0] <= max[0] &&
			test.max[1] <= max[1] &&
			test.max[2] <= max[2])

			return AABB::scContainsFully;
		else
			return AABB::scContainsPartially;
	else
		return AABB::scNoOverlap;
}

inline bool AABB::LineCastIntersect(const glm::vec3& lineStart, const glm::vec3& lineVec, float& tNear, float& tFar) const
{
	glm::vec3 oMin = (min - lineStart) / lineVec;
	glm::vec3 oMax = (max - lineStart) / lineVec;

	glm::vec3 tMin;
	tMin = glm::min(oMin, oMax);
	glm::vec3 tMax;
	tMax = glm::max(oMin, oMax);

	tNear = std::max(tMin.x, std::max(tMin.y, tMin.z));
	tFar = std::min(tMax.x, std::min(tMax.y, tMax.z));

	return tNear <= tFar;
}

bool AABB::LineCastIntersect(const glm::vec3& lineStart, const glm::vec3& lineVec, glm::vec3& nearVec, glm::vec3& farVec) const
{
	float tNear;
	float tFar;
	if (LineCastIntersect(lineStart, lineVec, tNear, tFar))
	{
		nearVec = lineStart + lineVec * tNear;
		farVec = lineStart + lineVec * tFar;
		return true;
	}
	return false;
}

unsigned AABB::RayCastIntersect(const glm::vec3& rayStart, const glm::vec3& rayVec, float& tNear, float& tFar) const
{
	bool rayInters = LineCastIntersect(rayStart, rayVec, tNear, tFar) && tFar > 0;
	return rayInters ? (tNear > 0 ? 2 : 1) : 0;
}

unsigned AABB::RayCastIntersect(const glm::vec3& rayStart, const glm::vec3& rayVec, glm::vec3& nearVec, glm::vec3& farVec) const
{
	float tNear;
	float tFar;
	switch (RayCastIntersect(rayStart, rayVec, tNear, tFar))
	{
	case 0:
		return 0;

	case 1:
		nearVec = rayStart + tNear * rayVec;
		return 1;

	case 2:
		nearVec = rayStart + tNear * rayVec;
		farVec = rayStart + tFar * rayVec;
		return 2;

	default:
		LSL_ASSERT(false);
		return 0;
	}
}

bool AABB::AABBLineCastIntersect(const AABB& aabb, const glm::vec3& rayVec, float& minDist) const
{
	bool res = false;
	for (int i = 0; i < 8; ++i)
	{
		glm::vec3 curV = aabb.GetVertex(i);
		float tNear;
		float tFar;
		//Прямое направление
		if (LineCastIntersect(curV, rayVec, tNear, tFar))
		{
			float t = AbsMin(tNear, tFar);
			minDist = (res) ? AbsMin(minDist, t) : t;
			res = true;
		}
		//Обратное направление
		curV = GetVertex(i);
		if (aabb.LineCastIntersect(curV, -rayVec, tNear, tFar))
		{
			float t = AbsMin(tNear, tFar);
			minDist = (res) ? AbsMin(minDist, t) : t;
			res = true;
		}
	}

	///Контроль на пересечение относительно центра. Берутся две проекции одного центра относительно линии пересечения чтобы учесть все возможные случаи проникновения
	glm::vec3 centerNear;
	glm::vec3 centerFar;
	if (aabb.LineCastIntersect(aabb.GetCenter(), rayVec, centerNear, centerFar))
	{
		float tNear;
		float tFar;
		//Ближняя проекция
		if (LineCastIntersect(centerNear, rayVec, tNear, tFar))
		{
			float t = AbsMin(tNear, tFar);
			minDist = (res) ? AbsMin(minDist, t) : t;
			res = true;
		}
		//Дальняя проекция
		if (LineCastIntersect(centerFar, rayVec, tNear, tFar))
		{
			float t = AbsMin(tNear, tFar);
			minDist = (res) ? AbsMin(minDist, t) : t;
			res = true;
		}
	}

	return res;
}

bool AABB::AABBLineCastIntersect(const AABB& start, const glm::vec3& vec, const D3DMATRIX& startTolocal, const D3DMATRIX& localToStart, float& minDist) const
{
	BoundBox startBB(start);
	startBB.Transform(startTolocal);
	BoundBox testBB(*this);
	testBB.Transform(localToStart);

	glm::vec3 localVec = Vec3TransformNormal(vec, startTolocal);

	bool res = false;
	for (int i = 0; i < 8; ++i)
	{
		float tNear;
		float tFar;
		//Прямое направление
		if (LineCastIntersect(startBB.v[i], localVec, tNear, tFar))
		{
			float t = AbsMin(tNear, tFar);
			minDist = (res) ? AbsMin(minDist, t) : t;
			res = true;
		}
		if (start.LineCastIntersect(testBB.v[i], -vec, tNear, tFar))
		{
			float t = AbsMin(tNear, tFar);
			minDist = (res) ? AbsMin(minDist, t) : t;
			res = true;
		}
	}

	///Контроль на пересечение относительно центра. Берутся две проекции одного центра относительно линии пересечения чтобы учесть все возможные случаи проникновения
	glm::vec3 centerNear;
	glm::vec3 centerFar;
	if (start.LineCastIntersect(start.GetCenter(), vec, centerNear, centerFar))
	{
		centerNear = Vec3TransformCoord(centerNear, startTolocal);
		centerFar = Vec3TransformCoord(centerFar, startTolocal);
		float tNear;
		float tFar;
		//Ближняя проекция
		if (LineCastIntersect(centerNear, localVec, tNear, tFar))
		{
			float t = AbsMin(tNear, tFar);
			minDist = (res) ? AbsMin(minDist, t) : t;
			res = true;
		}
		//Дальняя проекция
		if (LineCastIntersect(centerFar, localVec, tNear, tFar))
		{
			float t = AbsMin(tNear, tFar);
			minDist = (res) ? AbsMin(minDist, t) : t;
			res = true;
		}
	}

	return res;
}

bool AABB::AABBRayCastIntersect(const AABB& aabb, const glm::vec3& rayVec, float& minDist, const float error) const
{
	bool res = false;
	for (int i = 0; i < 8; ++i)
	{
		glm::vec3 curV = aabb.GetVertex(i);
		float tNear;
		float tFar;
		//Прямое направление
		if (RayCastIntersect(curV, rayVec, tNear, tFar))
		{
			minDist = (res) ? AbsMin(minDist, tNear) : tNear;
			res = true;
		}
		//Обратное направление
		curV = GetVertex(i);
		if (aabb.RayCastIntersect(curV, -rayVec, tNear, tFar))
		{
			minDist = (res) ? AbsMin(minDist, tNear) : tNear;
			res = true;
		}
	}

	///Контроль на пересечение относительно центра
	glm::vec3 centerNear;
	glm::vec3 centerFar;
	if (aabb.RayCastIntersect(GetCenter(), rayVec, centerNear, centerFar))
	{
		float tNear;
		float tFar;
		if (RayCastIntersect(centerNear, rayVec, tNear, tFar))
		{
			minDist = (res) ? AbsMin(minDist, tNear) : tNear;
			res = true;
		}
	}

	return res;
}

glm::vec3 AABB::GetCenter() const
{
	return (min + max) / 2.0f;
}

glm::vec3 AABB::GetSizes() const
{
	return max - min;
}

float AABB::GetDiameter() const
{
	return glm::length(GetSizes());
}

float AABB::GetRadius() const
{
	return GetDiameter() / 2.0f;
}

glm::vec3 AABB::GetVertex(unsigned index) const
{
	switch (index)
	{
	case 0:
		return min;

	case 1:
		return glm::vec3(max.x, min.y, min.z);

	case 2:
		return glm::vec3(max.x, max.y, min.z);

	case 3:
		return glm::vec3(min.x, max.y, min.z);

	case 4:
		return glm::vec3(min.x, min.y, max.z);

	case 5:
		return glm::vec3(max.x, min.y, max.z);

	case 6:
		return max;

	case 7:
		return glm::vec3(min.x, max.y, max.z);

	default:
		LSL_ASSERT(false);
		return NullVector;
	}
}

glm::vec4 AABB::GetPlane(unsigned index) const
{
	glm::vec4 res;
	switch (index)
	{
	case cLeftPlane:
		res = PlaneFromPointNormal(min, -XVector);
		break;

	case cTopPlane:
		res = PlaneFromPointNormal(min, -YVector);
		break;

	case cBackPlane:
		res = PlaneFromPointNormal(min, -ZVector);
		break;

	case cRightPlane:
		res = PlaneFromPointNormal(max, XVector);
		break;

	case cBottomPlane:
		res = PlaneFromPointNormal(max, YVector);
		break;

	case cFrontPlane:
		res = PlaneFromPointNormal(max, ZVector);
		break;
	}

	return res;
}

glm::vec3 AABB::GetPlaneVert(unsigned index, unsigned vertex) const
{
	/*glm::vec4 plane = GetPlane(index);
	glm::vec3 norm(plane);
	glm::vec3 invNorm = IdentityVector - glm::vec3(abs(norm.x), abs(norm.y), abs(norm.z));

	glm::vec3 fixCoord;
	Vec3Scale(norm, GetSizes()/2, fixCoord);
	glm::vec3 fixCenter;
	Vec3Scale(norm, GetCenter(), fixCenter);
	fixCoord += fixCenter;

	glm::vec3 res;
	switch (vertex)
	{
	case 0:
		Vec3Scale(min, invNorm, res);
		return res + fixCoord;

	case 1:
		Vec3Scale(max, invNorm, res);
		return res + fixCoord;

	default:
		LSL_ASSERT(false);
		return NullVector;
	}*/

	int bufInd[2];
	switch (index)
	{
	case cLeftPlane:
		bufInd[0] = 0;
		bufInd[1] = 7;
		break;

	case cTopPlane:
		bufInd[0] = 0;
		bufInd[1] = 5;
		break;

	case cBackPlane:
		bufInd[0] = 0;
		bufInd[1] = 2;
		break;

	case cRightPlane:
		bufInd[0] = 1;
		bufInd[1] = 6;
		break;

	case cBottomPlane:
		bufInd[0] = 3;
		bufInd[1] = 6;
		break;

	case cFrontPlane:
		bufInd[0] = 4;
		bufInd[1] = 6;
		break;

	default:
		LSL_ASSERT(false);
	}

	return GetVertex(bufInd[vertex]);
}

BoundBox::BoundBox()
{
}

BoundBox::BoundBox(const AABB& aabb)
{
	const glm::vec3 vertex[2] = {aabb.min, aabb.max};
	const int index[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

	for (int i = 0; i < 8; ++i)
	{
		int ind = i % 4;

		v[i] = glm::vec3(vertex[index[ind][0]].x, vertex[index[ind][1]].y, vertex[i / 4].z);
	}
}

void BoundBox::Transform(const BoundBox& bb, const D3DMATRIX& m, BoundBox& rOut)
{
	for (int i = 0; i < 8; ++i)
		rOut.v[i] = Vec3TransformCoord(bb.v[i], m);
}

void BoundBox::SetPlan(const int numPlan, const float valeur)
{
	for (int i = 0; i < 4; ++i)
		v[cBBPlans[numPlan][i]][cDirPlan[numPlan]] = valeur;
}

void BoundBox::Transform(const D3DMATRIX& m)
{
	Transform(*this, m, *this);
}

void BoundBox::ToAABB(AABB& aabb) const
{
	aabb.min = v[0];
	aabb.max = v[7];
}

Frustum::Frustum()
{

}

void Frustum::CalculateCorners(Corners& pPoints, const D3DMATRIX& invViewProj)
{
	int i = 0;
	for (float fx = -1.0f; fx <= 1.0f; fx += 2.0f)
	for (float fy = -1.0f; fy <= 1.0f; fy += 2.0f)
	for (float fz = 0.0f; fz <= 1.0f; fz += 1.0f, ++i)
	{
		pPoints[i] = Vec3TransformCoord(glm::vec3(fx, fy, fz), invViewProj);
	}
}

void Frustum::Refresh(const D3DMATRIX& viewProjMat)
{
	//extract left plane
	left.x = viewProjMat._14 - viewProjMat._12;
	left.y = viewProjMat._24 - viewProjMat._22;
	left.z = viewProjMat._34 - viewProjMat._32;
	left.w = viewProjMat._44 - viewProjMat._42;
	left = PlaneNormalize(left);

	//extract top plane
	top.x = viewProjMat._14 + viewProjMat._11;
	top.y = viewProjMat._24 + viewProjMat._21;
	top.z = viewProjMat._34 + viewProjMat._31;
	top.w = viewProjMat._44 + viewProjMat._41;
	top = PlaneNormalize(top);

	//extract right plane
	right.x = viewProjMat._14 + viewProjMat._12;
	right.y = viewProjMat._24 + viewProjMat._22;
	right.z = viewProjMat._34 + viewProjMat._32;
	right.w = viewProjMat._44 + viewProjMat._42;
	right = PlaneNormalize(right);

	//extract bottom plane
	bottom.x = viewProjMat._14 - viewProjMat._11;
	bottom.y = viewProjMat._24 - viewProjMat._21;
	bottom.z = viewProjMat._34 - viewProjMat._31;
	bottom.w = viewProjMat._44 - viewProjMat._41;
	bottom = PlaneNormalize(bottom);

	//extract near plane
	pNear.x = viewProjMat._13;
	pNear.y = viewProjMat._23;
	pNear.z = viewProjMat._33;
	pNear.w = viewProjMat._43;
	pNear = PlaneNormalize(pNear);

	//extract far plane
	pFar.x = viewProjMat._14 - viewProjMat._13;
	pFar.y = viewProjMat._24 - viewProjMat._23;
	pFar.z = viewProjMat._34 - viewProjMat._33;
	pFar.w = viewProjMat._44 - viewProjMat._43;
	pFar = PlaneNormalize(pFar);
}

Frustum::SpaceContains Frustum::ContainsAABB(const AABB& aabb) const
{
	typedef glm::vec4 PlaneArray[6];

	AABB::Corners corners;
	aabb.ExtractCorners(corners);

	int TotalIn = 0;
	// test all 8 corners against the 6 sides
	// if all points are behind 1 specific plane, we are out
	// if we are in with all points, then we are fully in

	// For each plane
	int inCount;
	bool PointIn;
	for (int iPlane = 0; iPlane < 6; ++iPlane)
	{
		inCount = 8;
		PointIn = true;

		for (int iCorner = 0; iCorner < 8; ++iCorner)
			if (PlaneDotCoord(planes[iPlane], corners[iCorner]) < 0)
			{
				PointIn = false;
				--inCount;
			}

		if (!inCount)
			return scNoOverlap;
		else
			if (PointIn)
				++TotalIn;
	}

	if (TotalIn == 6)
		return scContainsFully;
	else
		return scContainsPartially;
}

bool RayCastIntersectPlane(const glm::vec3& rayStart, const glm::vec3& rayVec, const glm::vec4& plane, float& outT)
{
	const float EPSILON = 1.0e-10f;

	float d = PlaneDotNormal(plane, rayVec);
	if (abs(d) > EPSILON)
	{
		outT = -PlaneDotCoord(plane, rayStart) / d;
		return outT > 0;
	}
	return false;
}

bool RayCastIntersectPlane(const glm::vec3& rayStart, const glm::vec3& rayVec, const glm::vec4& plane, glm::vec3& outVec)
{
	float t;
	bool res = RayCastIntersectPlane(rayStart, rayVec, plane, t);
	if (res)
		outVec = rayStart + rayVec * t;
	return res;
}

bool RayCastIntersectSquare(const glm::vec3& rayStart, const glm::vec3& rayVec, const glm::vec3& min, const glm::vec3& max, const glm::vec4& plane, float* outT, glm::vec3* outVec, const float error)
{
	const float Arad45 = 0.707106781f;

	float t;
	if (RayCastIntersectPlane(rayStart, rayVec, plane, t) && t > -error)
	{
		glm::vec3 intPnt = rayStart + rayVec * t;
		glm::vec3 plDiag = max - min;
		plDiag = glm::normalize(plDiag);
		glm::vec3 intPnt1 = intPnt - min;
		intPnt1 = glm::normalize(intPnt1);
		glm::vec3 intPnt2 = intPnt - max;
		intPnt2 = glm::normalize(intPnt2);
		bool res = glm::dot(intPnt1, plDiag) > Arad45 && glm::dot(intPnt2, (-plDiag)) > Arad45;
		if (res)
		{
			if (outT)
				*outT = t;
			if (outVec)
				*outVec = intPnt;
		}
		return res;
	}
	else
		return false;
}

void GetSampleOffsetsDownScale3x3(DWORD dwWidth, DWORD dwHeight, glm::vec2 avSampleOffsets[9])
{
	float tU = 1.0f / dwWidth;
	float tV = 1.0f / dwHeight;

	// Sample from the 9 surrounding points.
	int index = 0;
	for (int y = -1; y <= 1; ++y)
		for (int x = -1; x <= 1; ++x)
		{
			avSampleOffsets[index].x = x * tU;
			avSampleOffsets[index].y = y * tV;

			index++;
		}
}

void GetSampleOffsetsDownScale4x4(DWORD dwWidth, DWORD dwHeight, glm::vec2 avSampleOffsets[16])
{
	float tU = 1.0f / dwWidth;
	float tV = 1.0f / dwHeight;

	// Sample from 4 surrounding points.
	int index = 0;
	for( int y = -1; y < 3; y++ )
	{
		for( int x = -1; x < 3; x++ )
		{
			avSampleOffsets[index].x = (x - 0.5f) * tU;
			avSampleOffsets[index].y = (y - 0.5f) * tV;

			index++;
		}
	}
}

//}