#ifndef R3D_MATH
#define R3D_MATH

#include "lslCommon.h"
#include "lslMath.h"

//namespace r3d
//{

struct AABB2
{
	enum SpaceContains {scNoOverlap, scContainsFully, scContainsPartially};

	AABB2();
	explicit AABB2(float size);
	explicit AABB2(const glm::vec2& size);
	AABB2(const glm::vec2& mMin, const glm::vec2& mMax);

	//static void Transform(const AABB2 &aabb, const glm::mat4 &m, AABB2 &rOut);
	static void Transform(const AABB2 &aabb, const D3DXMATRIX &m, AABB2 &rOut);
	static void Include(const AABB2& aabb, const glm::vec2& vec, AABB2& rOut);
	static void Add(const AABB2& aabb1, const AABB2& aabb2, AABB2& rOut);
	static void Offset(const AABB2& aabb, const glm::vec2& vec, AABB2& rOut);

	void Transform(const D3DXMATRIX &m);
	void Include(const glm::vec2& vec);
	void Add(const AABB2& aabb);
	void Offset(const glm::vec2& vec);

	bool ContainsPoint(const glm::vec2& point) const;
	SpaceContains ContainsAABB(const AABB2& test) const;

	glm::vec2 GetCenter() const;
	glm::vec2 GetSize() const;

	glm::vec2 min;
	glm::vec2 max;
};

struct BoundBox;

struct AABB
{
	static const unsigned cLeftPlane   = 0; //-X
	static const unsigned cTopPlane    = 1; //-Y
	static const unsigned cBackPlane   = 2; //-Z
	static const unsigned cRightPlane  = 3; //X
	static const unsigned cBottomPlane = 4; //Y
	static const unsigned cFrontPlane  = 5; //Z

	//Edges are stored in this way:
	//                Y
	//      Z        /|
	//     /|\       /
	//      | /7---------/6(max)
	//      |/  |  /    / |
	//      |   | /    /  |
	//      4---------5   |
	//      |   3- - -| -2
	//      |  /      |  /
	//      |/        | /
	//      0(min)----1/----->X
	//

	typedef glm::vec3 Corners[8];
	enum SpaceContains {scNoOverlap, scContainsFully, scContainsPartially};

	//Статические методы могут быть ползены для неконстантных операций, т.е. там где происходит преобразование объекта и результат может быть записан в другое место
	//Эти методы в любом случае имеет смысл делать инлайновыми(и определять их в *.cpp), поскольку аналогичные унарные операции реализуются через них
	static void Transform(const AABB& aabb, const D3DXMATRIX& m, AABB& rOut);
	static void Offset(const AABB& aabb, const glm::vec3& vec, AABB& rOut);
	static void Add(const AABB& aabb1, const AABB& aabb2, AABB& rOut);
	static void Include(const AABB& aabb, const glm::vec3& vec, AABB& rOut);
	static void Scale(const AABB& aabb, const glm::vec3& vec, AABB& rOut);
	static void Scale(const AABB& aabb, float f, AABB& rOut);

	AABB();
	explicit AABB(float size);
	explicit AABB(const glm::vec3& sizes);
	explicit AABB(const glm::vec3& minPoint, const glm::vec3& maxPoint);

	void FromPoints(const glm::vec3& pnt1, const glm::vec3& pnt2);
	void FromDimensions(const glm::vec3& dimensions);
	void FromPlaneAndScale(const D3DXPLANE& plane, const glm::vec3& vec);
	void Transform(const D3DXMATRIX& m);
	void Offset(const glm::vec3& vec);
	void Add(const AABB& aabb);
	void Include(const glm::vec3& vec);
	void Scale(const glm::vec3& vec);
	void Scale(float f);

	void ExtractCorners(Corners& corners) const;
	bool ContainsPoint(const glm::vec3& point) const;
	SpaceContains ContainsAABB(const AABB& test) const;

	//Тест на пересечения с this
	//Пересечения линией (прямой)
	bool LineCastIntersect(const glm::vec3& lineStart, const glm::vec3& lineVec, float& tNear, float& tFar) const;
	//Пересечения линией (прямой)
	bool LineCastIntersect(const glm::vec3& lineStart, const glm::vec3& lineVec, glm::vec3& nearVec, glm::vec3& farVec) const;
	//Пересечение лучом
	unsigned RayCastIntersect(const glm::vec3& rayStart, const glm::vec3& rayVec, float& tNear, float& tFar) const;
	//Пересечение лучом
	unsigned RayCastIntersect(const glm::vec3& rayStart, const glm::vec3& rayVec, glm::vec3& nearVec, glm::vec3& farVec) const;

	//Пересечение направленным объемом AABB как линией
	bool AABBLineCastIntersect(const AABB& aabb, const glm::vec3& rayVec, float& minDist) const;
	//
	//start - направленный объем
	//vec - вектор направления в локальной мистеме координат start
	//startTolocal - преобразование из start в локальную систему координат this
	//localToStart - преобразование из this в локальную систему координат start
	//minDist - выходная минимальная дистанция между this и направленным объемом start
	bool AABBLineCastIntersect(const AABB& start, const glm::vec3& vec, const D3DXMATRIX& startTolocal, const D3DXMATRIX& localToStart, float& minDist) const;
	//Пересечение направленным объемом AABB как лучом
	bool AABBRayCastIntersect(const AABB& aabb, const glm::vec3& rayVec, float& minDist, const float error = 0) const;

	glm::vec3 GetCenter() const;
	glm::vec3 GetSizes() const;
	float GetDiameter() const;
	float GetRadius() const;
	glm::vec3 GetVertex(unsigned index) const;
	D3DXPLANE GetPlane(unsigned index) const;
	//vertex[0] - min
	//vertex[1] - max
	glm::vec3 GetPlaneVert(unsigned index, unsigned vertex) const;

	glm::vec3 min;
	glm::vec3 max;
};

typedef int PlanIndices[4];
typedef PlanIndices PlanBB[6];
typedef int DirPlan[6];

struct BoundBox
{
	//Edges are stored in this way:
	//                Y
	//      Z        /|
	//     /|\       /
	//      | /7---------/6(max)
	//      |/  |  /    / |
	//      |   | /    /  |
	//      4---------5   |
	//      |   3- - -| -2
	//      |  /      |  /
	//      |/        | /
	//      0(min)----1/----->X
	//

	static void Transform(const BoundBox& bb, const D3DXMATRIX& m, BoundBox& rOut);

	BoundBox();
	explicit BoundBox(const AABB& aabb);

	void SetPlan(const int numPlan, const float valeur);
	void Transform(const D3DXMATRIX& m);

	void ToAABB(AABB& aabb) const;

	glm::vec3 v[8];
};

//Неопеределенность насчет допустимости конструкции...
struct Frustum
{
	typedef glm::vec3 Corners[8];

	enum SpaceContains {scNoOverlap, scContainsFully, scContainsPartially};

	Frustum();

	static void CalculateCorners(Corners& pPoints, const D3DXMATRIX& invViewProj);

	void Refresh(const D3DXMATRIX& viewProjMat);

	SpaceContains ContainsAABB(const AABB& aabb) const;

	union
	{
		struct
		{
			D3DXPLANE left;
			D3DXPLANE top;
			D3DXPLANE right;
			D3DXPLANE bottom;
			D3DXPLANE pNear;
			D3DXPLANE pFar;
		};
		struct
		{
			D3DXPLANE planes[6];
		};
	};
};

//error - ошибка проникновения, т.е. совпадающие координаты или координаты остающие на величину ошибки считаются нестабильными поэтому луч также проходит черех них даже если они сзади на величину error
bool RayCastIntersectPlane(const glm::vec3& rayStart, const glm::vec3& rayVec, const D3DXPLANE& plane, float& outT);
bool RayCastIntersectPlane(const glm::vec3& rayStart, const glm::vec3& rayVec, const D3DXPLANE& plane, glm::vec3& outVec);
//Погрешность 10%
bool RayCastIntersectSquare(const glm::vec3& rayStart, const glm::vec3& rayVec, const glm::vec3& min, const glm::vec3& max, const D3DXPLANE& plane, float* outT, glm::vec3* outVec, const float error = 0);
void GetSampleOffsetsDownScale3x3(DWORD dwWidth, DWORD dwHeight, glm::vec2 avSampleOffsets[9]);
void GetSampleOffsetsDownScale4x4(DWORD dwWidth, DWORD dwHeight, glm::vec2 avSampleOffsets[16]);

const D3DXCOLOR clrBlack    (0.0f,  0.0f,  0.0f , 1.0f);
const D3DXCOLOR clrGray05   (0.05f, 0.05f, 0.05f, 1.0f);
const D3DXCOLOR clrGray10   (0.10f, 0.10f, 0.10f, 1.0f);
const D3DXCOLOR clrGray15   (0.15f, 0.15f, 0.15f, 1.0f);
const D3DXCOLOR clrGray20   (0.20f, 0.20f, 0.20f, 1.0f);
const D3DXCOLOR clrGray25   (0.25f, 0.25f, 0.25f, 1.0f);
const D3DXCOLOR clrGray30   (0.30f, 0.30f, 0.30f, 1.0f);
const D3DXCOLOR clrGray35   (0.35f, 0.35f, 0.35f, 1.0f);
const D3DXCOLOR clrGray40   (0.40f, 0.40f, 0.40f, 1.0f);
const D3DXCOLOR clrGray45   (0.45f, 0.45f, 0.45f, 1.0f);
const D3DXCOLOR clrGray50   (0.50f, 0.50f, 0.50f, 1.0f);
const D3DXCOLOR clrGray55   (0.55f, 0.55f, 0.55f, 1.0f);
const D3DXCOLOR clrGray60   (0.60f, 0.60f, 0.60f, 1.0f);
const D3DXCOLOR clrGray65   (0.65f, 0.65f, 0.65f, 1.0f);
const D3DXCOLOR clrGray70   (0.70f, 0.70f, 0.70f, 1.0f);
const D3DXCOLOR clrGray75   (0.75f, 0.75f, 0.75f, 1.0f);
const D3DXCOLOR clrGray80   (0.80f, 0.80f, 0.80f, 1.0f);
const D3DXCOLOR clrGray85   (0.85f, 0.85f, 0.85f, 1.0f);
const D3DXCOLOR clrGray90   (0.90f, 0.90f, 0.90f, 1.0f);
const D3DXCOLOR clrGray95   (0.95f, 0.95f, 0.95f, 1.0f);
const D3DXCOLOR clrWhite    (1.0f,  1.0f,  1.0f,  1.0f);
const D3DXCOLOR clrRed      (1.0f,  0.0f,  0.0f,  1.0f);
const D3DXCOLOR clrGreen    (0.0f,  1.0f,  0.0f,  1.0f);
const D3DXCOLOR clrBlue     (0.0f,  0.0f,  1.0f,  1.0f);
const D3DXCOLOR clrYellow   (1.0f,  1.0f,  0.0f,  1.0f);

const D3DXPLANE        XPlane (1.0f, 0.0f, 0.0f, 0.0f);
const D3DXPLANE        YPlane (0.0f, 1.0f, 0.0f, 0.0f);
const D3DXPLANE        ZPlane (0.0f, 0.0f, 1.0f, 0.0f);
const AABB             NullAABB(0);

//}

#endif