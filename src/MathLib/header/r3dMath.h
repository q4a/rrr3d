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

	static void Transform(const AABB2 &aabb, const D3DMATRIX &m, AABB2 &rOut);
	static void Include(const AABB2& aabb, const glm::vec2& vec, AABB2& rOut);
	static void Add(const AABB2& aabb1, const AABB2& aabb2, AABB2& rOut);
	static void Offset(const AABB2& aabb, const glm::vec2& vec, AABB2& rOut);

	void Transform(const D3DMATRIX &m);
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

	//����������� ������ ����� ���� ������� ��� ������������� ��������, �.�. ��� ��� ���������� �������������� ������� � ��������� ����� ���� ������� � ������ �����
	//��� ������ � ����� ������ ����� ����� ������ �����������(� ���������� �� � *.cpp), ��������� ����������� ������� �������� ����������� ����� ���
	static void Transform(const AABB& aabb, const D3DMATRIX& m, AABB& rOut);
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
	void FromPlaneAndScale(const glm::vec4& plane, const glm::vec3& vec);
	void Transform(const D3DMATRIX& m);
	void Offset(const glm::vec3& vec);
	void Add(const AABB& aabb);
	void Include(const glm::vec3& vec);
	void Scale(const glm::vec3& vec);
	void Scale(float f);

	void ExtractCorners(Corners& corners) const;
	bool ContainsPoint(const glm::vec3& point) const;
	SpaceContains ContainsAABB(const AABB& test) const;

	//���� �� ����������� � this
	//����������� ������ (������)
	bool LineCastIntersect(const glm::vec3& lineStart, const glm::vec3& lineVec, float& tNear, float& tFar) const;
	//����������� ������ (������)
	bool LineCastIntersect(const glm::vec3& lineStart, const glm::vec3& lineVec, glm::vec3& nearVec, glm::vec3& farVec) const;
	//����������� �����
	unsigned RayCastIntersect(const glm::vec3& rayStart, const glm::vec3& rayVec, float& tNear, float& tFar) const;
	//����������� �����
	unsigned RayCastIntersect(const glm::vec3& rayStart, const glm::vec3& rayVec, glm::vec3& nearVec, glm::vec3& farVec) const;

	//����������� ������������ ������� AABB ��� ������
	bool AABBLineCastIntersect(const AABB& aabb, const glm::vec3& rayVec, float& minDist) const;
	//
	//start - ������������ �����
	//vec - ������ ����������� � ��������� ������� ��������� start
	//startTolocal - �������������� �� start � ��������� ������� ��������� this
	//localToStart - �������������� �� this � ��������� ������� ��������� start
	//minDist - �������� ����������� ��������� ����� this � ������������ ������� start
	bool AABBLineCastIntersect(const AABB& start, const glm::vec3& vec, const D3DMATRIX& startTolocal, const D3DMATRIX& localToStart, float& minDist) const;
	//����������� ������������ ������� AABB ��� �����
	bool AABBRayCastIntersect(const AABB& aabb, const glm::vec3& rayVec, float& minDist, const float error = 0) const;

	glm::vec3 GetCenter() const;
	glm::vec3 GetSizes() const;
	float GetDiameter() const;
	float GetRadius() const;
	glm::vec3 GetVertex(unsigned index) const;
	glm::vec4 GetPlane(unsigned index) const;
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

	static void Transform(const BoundBox& bb, const D3DMATRIX& m, BoundBox& rOut);

	BoundBox();
	explicit BoundBox(const AABB& aabb);

	void SetPlan(const int numPlan, const float valeur);
	void Transform(const D3DMATRIX& m);

	void ToAABB(AABB& aabb) const;

	glm::vec3 v[8];
};

//����������������� ������ ������������ �����������...
struct Frustum
{
	typedef glm::vec3 Corners[8];

	enum SpaceContains {scNoOverlap, scContainsFully, scContainsPartially};

	Frustum();

	static void CalculateCorners(Corners& pPoints, const D3DMATRIX& invViewProj);

	void Refresh(const D3DMATRIX& viewProjMat);

	SpaceContains ContainsAABB(const AABB& aabb) const;

	union
	{
		struct
		{
			glm::vec4 left;
			glm::vec4 top;
			glm::vec4 right;
			glm::vec4 bottom;
			glm::vec4 pNear;
			glm::vec4 pFar;
		};
		struct
		{
			glm::vec4 planes[6];
		};
	};
};

//error - ������ �������������, �.�. ����������� ���������� ��� ���������� �������� �� �������� ������ ��������� ������������� ������� ��� ����� �������� ����� ��� ���� ���� ��� ����� �� �������� error
bool RayCastIntersectPlane(const glm::vec3& rayStart, const glm::vec3& rayVec, const glm::vec4& plane, float& outT);
bool RayCastIntersectPlane(const glm::vec3& rayStart, const glm::vec3& rayVec, const glm::vec4& plane, glm::vec3& outVec);
//����������� 10%
bool RayCastIntersectSquare(const glm::vec3& rayStart, const glm::vec3& rayVec, const glm::vec3& min, const glm::vec3& max, const glm::vec4& plane, float* outT, glm::vec3* outVec, const float error = 0);
void GetSampleOffsetsDownScale3x3(DWORD dwWidth, DWORD dwHeight, glm::vec2 avSampleOffsets[9]);
void GetSampleOffsetsDownScale4x4(DWORD dwWidth, DWORD dwHeight, glm::vec2 avSampleOffsets[16]);

const glm::vec4 clrBlack    (0.0f,  0.0f,  0.0f , 1.0f);
const glm::vec4 clrGray05   (0.05f, 0.05f, 0.05f, 1.0f);
const glm::vec4 clrGray10   (0.10f, 0.10f, 0.10f, 1.0f);
const glm::vec4 clrGray15   (0.15f, 0.15f, 0.15f, 1.0f);
const glm::vec4 clrGray20   (0.20f, 0.20f, 0.20f, 1.0f);
const glm::vec4 clrGray25   (0.25f, 0.25f, 0.25f, 1.0f);
const glm::vec4 clrGray30   (0.30f, 0.30f, 0.30f, 1.0f);
const glm::vec4 clrGray35   (0.35f, 0.35f, 0.35f, 1.0f);
const glm::vec4 clrGray40   (0.40f, 0.40f, 0.40f, 1.0f);
const glm::vec4 clrGray45   (0.45f, 0.45f, 0.45f, 1.0f);
const glm::vec4 clrGray50   (0.50f, 0.50f, 0.50f, 1.0f);
const glm::vec4 clrGray55   (0.55f, 0.55f, 0.55f, 1.0f);
const glm::vec4 clrGray60   (0.60f, 0.60f, 0.60f, 1.0f);
const glm::vec4 clrGray65   (0.65f, 0.65f, 0.65f, 1.0f);
const glm::vec4 clrGrayAF   (175.0f/255.0f, 175.0f/255.0f, 175.0f/255.0f, 1.0f); // AF = 175, 175/255 = 0,6862745
const glm::vec4 clrGray70   (0.70f, 0.70f, 0.70f, 1.0f);
const glm::vec4 clrGray75   (0.75f, 0.75f, 0.75f, 1.0f);
const glm::vec4 clrGray80   (0.80f, 0.80f, 0.80f, 1.0f);
const glm::vec4 clrGray85   (0.85f, 0.85f, 0.85f, 1.0f);
const glm::vec4 clrGray90   (0.90f, 0.90f, 0.90f, 1.0f);
const glm::vec4 clrGray95   (0.95f, 0.95f, 0.95f, 1.0f);
const glm::vec4 clrWhite    (1.0f,  1.0f,  1.0f,  1.0f);
const glm::vec4 clrRed      (1.0f,  0.0f,  0.0f,  1.0f);
const glm::vec4 clrGreen    (0.0f,  1.0f,  0.0f,  1.0f);
const glm::vec4 clrBlue     (0.0f,  0.0f,  1.0f,  1.0f);
const glm::vec4 clrYellow   (1.0f,  1.0f,  0.0f,  1.0f);

const glm::vec4        XPlane (1.0f, 0.0f, 0.0f, 0.0f);
const glm::vec4        YPlane (0.0f, 1.0f, 0.0f, 0.0f);
const glm::vec4        ZPlane (0.0f, 0.0f, 1.0f, 0.0f);
const AABB             NullAABB(0);

//}

#endif