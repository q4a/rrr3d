#include "lslMath.h"

//namespace r3d
//{

template<class _Res> inline _Res Floor(float value)
{
	return static_cast<_Res>(floor(value));
}

template<class _Res> inline _Res Ceil(float value)
{
	return static_cast<_Res>(ceil(value));
}

template<class _Res> inline _Res Round(_Res value)
{
	_Res f = static_cast<_Res>(floor(value));
	if (value - static_cast<_Res>(0.5) < f)
		return f;
	else
		return f + 1;
}

inline float Random()
{
	return rand() / static_cast<float>(RAND_MAX);
}

//from (inclusive) ... to (inclusive)
inline float RandomRange(float from, float to)
{
	return from + rand() * (to - from) / RAND_MAX;
}

//from (inclusive) ... to (inclusive)
inline int RandomRange(int from, int to)
{
	return from + Floor<int>(rand() * (to + 1 - from) / static_cast<float>(RAND_MAX + 1));
}

inline float NumAbsAdd(float absVal, float addVal)
{
	return absVal > 0 ? absVal + addVal : absVal - addVal;
}

inline glm::mat4 Matrix4DxToGlm(const D3DXMATRIX &mat)
{
	// mat - DX, mat.m[0] 1-st row, mat.m[0][1] = mat._12 - 1-st row 2-nd column
	glm::mat4 mat4glm(mat._11, mat._21, mat._31, mat._41, // 1-st column
	                  mat._12, mat._22, mat._32, mat._42, // 2-nd column
	                  mat._13, mat._23, mat._33, mat._43, // 3-rd column
	                  mat._14, mat._24, mat._34, mat._44);// 4-th column
	return mat4glm;
}

inline D3DXMATRIX Matrix4GlmToDx(const glm::mat4 &mat)
{
	// mat - GLM, mat[0] 1-st column, mat[0][1] = mat[0].y - 1-st column 2-nd row
	D3DXMATRIX matrix(mat[0].x, mat[1].x, mat[2].x, mat[3].x,
	                  mat[0].y, mat[1].y, mat[2].y, mat[3].y,
	                  mat[0].z, mat[1].z, mat[2].z, mat[3].z,
	                  mat[0].w, mat[1].w, mat[2].w, mat[3].w);
	return matrix;
}

inline glm::vec3 Vec3DxToGlm(const D3DXVECTOR3 v3)
{
	glm::vec3 v3glm(v3.x, v3.y, v3.z);
	return v3glm;
}

inline D3DXVECTOR3 Vec3GlmToDx(const glm::vec3 v3)
{
	D3DXVECTOR3 v3dx(v3.x, v3.y, v3.z);
	return v3dx;
}

inline glm::vec4 Vec4DxToGlm(const D3DXVECTOR4 v4)
{
	glm::vec4 v4glm(v4.x, v4.y, v4.z, v4.w);
	return v4glm;
}

inline D3DXVECTOR4 Vec4GlmToDx(const glm::vec4 v4)
{
	D3DXVECTOR4 v4Dx(v4.x, v4.y, v4.z, v4.w);
	return v4Dx;
}

inline D3DXPLANE Vec4ToPlane(const glm::vec4 v4)
{
	D3DXPLANE plane(v4.x, v4.y, v4.z, v4.w);
	return plane;
}

inline D3DCOLOR Vec4ToColor(const glm::vec4 &vec)
{
	return D3DCOLOR_COLORVALUE(vec.r, vec.g, vec.b, vec.a);
}

inline glm::vec4 ColorToVec4(const D3DCOLOR &col)
{
	glm::vec4 vec;
	vec.r = (col >> 16 & 255) / 255.0f;
	vec.g = (col >> 8 & 255) / 255.0f;
	vec.b = (col & 255) / 255.0f;
	vec.a = (col >> 24 & 255) / 255.0f;
	return vec;
}

inline glm::vec4 ColorVToVec4(const D3DCOLORVALUE &cv)
{
	glm::vec4 v4glm(cv.r, cv.g, cv.b, cv.a);
	return v4glm;
}

inline glm::vec3 Vec3TransformCoord(const glm::vec3 &vec, const D3DXMATRIX &mat)
{
	D3DXVECTOR3 res;
	D3DXVec3TransformCoord(&res, &Vec3GlmToDx(vec), &mat);
	return Vec3DxToGlm(res);
}

inline glm::vec3 Vec3TransformNormal(const glm::vec3 &vec, const D3DXMATRIX &mat)
{
	D3DXVECTOR3 res;
	D3DXVec3TransformNormal(&res, &Vec3GlmToDx(vec), &mat);
	return Vec3DxToGlm(res);
}

inline glm::vec4 Vec3Transform(const glm::vec3 &vec, const D3DXMATRIX &mat)
{
	D3DXVECTOR4 res;
	D3DXVec3Transform(&res, &Vec3GlmToDx(vec), &mat);
	return Vec4DxToGlm(res);
}


inline glm::vec4 Vec4Transform(const glm::vec4 &vec, const D3DXMATRIX &mat)
{
	D3DXVECTOR4 res;
	D3DXVec4Transform(&res, &Vec4GlmToDx(vec), &mat);
	return Vec4DxToGlm(res);
}

inline float ScalarTransform(float scalar, const glm::vec3& vec, const D3DXMATRIX& mat)
{
	D3DXVECTOR3 res;
	D3DXVec3TransformNormal(&res, &Vec3GlmToDx(vec * scalar), &mat);
	float len = D3DXVec3Length(&res);
	return scalar < 0 ? -len : len;
}

inline void BuildWorldMatrix(const glm::vec3& pos, const glm::vec3& scale, const glm::quat& rot, D3DXMATRIX& outMat)
{
	D3DXMATRIX scaleMat;
	D3DXMatrixScaling(&scaleMat, scale.x, scale.y, scale.z);

	D3DXMATRIX rotMat = Matrix4GlmToDx(glm::transpose(glm::mat4_cast(rot)));

	D3DXMATRIX transMat;
	D3DXMatrixTranslation(&transMat, pos.x, pos.y, pos.z);

	outMat = scaleMat * rotMat * transMat;
}

inline D3DXMATRIX BuildWorldMatrix(const glm::vec3& pos, const glm::vec3& scale, const glm::quat& rot)
{
	D3DXMATRIX res;
	BuildWorldMatrix(pos, scale, rot, res);
	return res;
}

inline void MatrixRotationFromAxis(const glm::vec3& xVec, const glm::vec3& yVec, const glm::vec3& zVec, D3DXMATRIX& matOut)
{
	matOut._11 = xVec.x;
	matOut._12 = xVec.y;
	matOut._13 = xVec.z;
	matOut._14 = 0.0f;
	matOut._21 = yVec.x;
	matOut._22 = yVec.y;
	matOut._23 = yVec.z;
	matOut._24 = 0.0f;
	matOut._31 = zVec.x;
	matOut._32 = zVec.y;
	matOut._33 = zVec.z;
	matOut._34 = 0.0f;
	matOut._41 = 0.0f;
	matOut._42 = 0.0f;
	matOut._43 = 0.0f;
	matOut._44 = 1.0f;
}

inline void MatrixSetTranslation(const glm::vec3& vec, D3DXMATRIX& outMat)
{
	outMat._41 = vec.x;
	outMat._42 = vec.y;
	outMat._43 = vec.z;
}

inline void MatrixTranslate(const glm::vec3& vec, D3DXMATRIX& outMat)
{
	outMat._41 += vec.x;
	outMat._42 += vec.y;
	outMat._43 += vec.z;
}

inline void MatrixSetScale(const glm::vec3& vec, D3DXMATRIX& outMat)
{
	outMat._11 = vec.x;
	outMat._22 = vec.y;
	outMat._33 = vec.z;
}

inline void MatrixScale(const glm::vec3& vec, D3DXMATRIX& outMat)
{
	outMat._11 *= vec.x;
	outMat._22 *= vec.y;
	outMat._33 *= vec.z;
}

inline void MatGetPos(const D3DXMATRIX& mat, glm::vec3& outPos)
{
	outPos = Vec3DxToGlm(mat.m[3]);
}

inline glm::vec3 MatGetPos(const D3DXMATRIX& mat)
{
	glm::vec3 res;
	MatGetPos(mat, res);
	return res;
}

inline glm::vec2 Vec2Minimize(const glm::vec2 &vec1, const glm::vec2 &vec2)
{
	glm::vec2 pOut;
	pOut.x = vec1.x < vec2.x ? vec1.x : vec2.x;
	pOut.y = vec1.y < vec2.y ? vec1.y : vec2.y;
	return pOut;
}

inline glm::vec2 Vec2Maximize(const glm::vec2 &vec1, const glm::vec2 &vec2)
{
	glm::vec2 pOut;
	pOut.x = vec1.x > vec2.x ? vec1.x : vec2.x;
	pOut.y = vec1.y > vec2.y ? vec1.y : vec2.y;
	return pOut;
}

inline glm::vec2 Vec2TransformCoord(const glm::vec2 &vec, const glm::mat4 &mat)
{
	glm::vec4 res4 = glm::vec4(vec.x, vec.y, 0, 1) * mat;
	return glm::vec2(res4.x, res4.y);
}

inline glm::vec2 Vec2TransformNormal(const glm::vec2 &vec, const glm::mat4 &mat)
{
	glm::vec4 res4 = glm::vec4(vec.x, vec.y, 0, 0) * mat;
	return glm::vec2(res4.x, res4.y);
}

inline float Vec2Dot(const glm::vec2 &vec1, const glm::vec2 &vec2)
{
	return vec1.x * vec2.x + vec1.y * vec2.y;
}

inline glm::vec2 Vec2Lerp(const glm::vec2 &vec1, const glm::vec2 &vec2, float scalar)
{
	glm::vec2 res;
	res.x = vec1.x + scalar * (vec2.x - vec1.x);
	res.y = vec1.y + scalar * (vec2.y - vec1.y);
	return res;
}

inline float Vec2CCW(const glm::vec2 &vec1, const glm::vec2 &vec2)
{
	return vec1.x * vec2.y - vec1.y * vec2.x;
}

//Поворот вектора на 90 градуос против часовой стрелки, иначе говоря его нормаль
inline void Vec2NormCCW(const glm::vec2& vec, glm::vec2& outVec)
{
	//На случай если &vec2 == &outVec
	float tmpX = vec.x;

	outVec.x = -vec.y;
	outVec.y = tmpX;
}

inline glm::vec2 Vec2NormCCW(const glm::vec2& vec2)
{
	glm::vec2 res;
	Vec2NormCCW(vec2, res);
	return res;
}

//Поворот вектора на 90 градуос по часовой стрелки, иначе говоря его нормаль
inline void Vec2NormCW(const glm::vec2& vec, glm::vec2& outVec)
{
	//На случай если &vec2 == &outVec
	float tmpX = vec.x;

	outVec.x = vec.y;
	outVec.y = -tmpX;
}

inline float Vec2Proj(const glm::vec2& vec1, const glm::vec2& vec2)
{
	return Vec2Dot(vec1, vec2) / glm::length(vec2);
}

inline void operator*=(glm::vec2& vec1, const glm::vec2& vec2)
{
	vec1.x *= vec2.x;
	vec1.y *= vec2.y;
}

inline glm::vec2 operator*(const glm::vec2& vec1, const glm::vec2& vec2)
{
	glm::vec2 res = vec1;
	res *= vec2;

	return res;
}

inline void operator/=(glm::vec2& vec1, const glm::vec2& vec2)
{
	vec1.x /= vec2.x;
	vec1.y /= vec2.y;
}

inline glm::vec2 operator/(const glm::vec2& vec1, const glm::vec2& vec2)
{
	glm::vec2 res = vec1;
	res /= vec2;

	return res;
}

inline glm::vec3 Vec3FromVec2(const glm::vec2& vec)
{
	return glm::vec3(vec.x, vec.y, 0.0f);
}

inline void Vec3Invert(const glm::vec3& vec, glm::vec3& rOut)
{
	rOut.x = 1 / vec.x;
	rOut.y = 1 / vec.y;
	rOut.z = 1 / vec.z;
};

inline glm::vec3 Vec3Invert(const glm::vec3& vec)
{
	glm::vec3 res;
	Vec3Invert(vec, res);
	return res;
};

inline void Vec3Abs(const glm::vec3& vec, glm::vec3& rOut)
{
	rOut.x = abs(vec.x);
	rOut.y = abs(vec.y);
	rOut.z = abs(vec.z);
}

inline glm::vec3 Vec3Abs(const glm::vec3& vec)
{
	glm::vec3 res;
	Vec3Abs(vec, res);
	return res;
}

// It's possible to use glm:mix, but it gives a bit different result for Z axe.
inline glm::vec3 Vec3Lerp(const glm::vec3 &vec1, const glm::vec3 &vec2, float scalar)
{
	glm::vec3 res;
	res.x = vec1.x + scalar * (vec2.x - vec1.x);
	res.y = vec1.y + scalar * (vec2.y - vec1.y);
	res.z = vec1.z + scalar * (vec2.z - vec1.z);
	return res;
}

inline void Vec3Rotate(const glm::vec3& v, const glm::quat& quat, glm::vec3& outVec)
{
	glm::quat q(v.x * quat.x + v.y * quat.y + v.z * quat.z,
				v.x * quat.w + v.z * quat.y - v.y * quat.z,
				v.y * quat.w + v.x * quat.z - v.z * quat.x,
				v.z * quat.w + v.y * quat.x - v.x * quat.y);

	outVec.x = quat.w * q.x + quat.x * q.w + quat.y * q.z - quat.z * q.y;
	outVec.y = quat.w * q.y + quat.y * q.w + quat.z * q.x - quat.x * q.z;
	outVec.z = quat.w * q.z + quat.z * q.w + quat.x * q.y - quat.y * q.x;

	outVec /= glm::length2(quat);
}

inline void operator*=(glm::vec3& vec1, const glm::vec3& vec2)
{
	vec1.x *= vec2.x;
	vec1.y *= vec2.y;
	vec1.z *= vec2.z;
}

inline glm::vec3 operator*(const glm::vec3& vec1, const glm::vec3& vec2)
{
	glm::vec3 res = vec1;
	res *= vec2;

	return res;
}

inline void operator/=(glm::vec3& vec1, const glm::vec3& vec2)
{
	vec1.x /= vec2.x;
	vec1.y /= vec2.y;
	vec1.z /= vec2.z;
}

inline glm::vec3 operator/(const glm::vec3& vec1, const glm::vec3& vec2)
{
	glm::vec3 res = vec1;
	res /= vec2;

	return res;
}

inline bool operator>=(const glm::vec3& vec1, float scalar)
{
	return vec1.x >= scalar &&
		   vec1.y >= scalar &&
		   vec1.z >= scalar;
}

inline bool operator<(const glm::vec3& vec1, float scalar)
{
	return vec1.x < scalar &&
		   vec1.y < scalar &&
		   vec1.z < scalar;
}

inline bool operator>(const glm::vec3& vec1, const glm::vec3& vec2)
{
	return vec1.x > vec2.x &&
		   vec1.y > vec2.y &&
		   vec1.z > vec2.z;
}

inline bool operator<(const glm::vec3& vec1, const glm::vec3& vec2)
{
	return vec1.x < vec2.x &&
		   vec1.y < vec2.y &&
		   vec1.z < vec2.z;
}

inline void operator*=(glm::vec4& vec1, const glm::vec4& vec2)
{
	vec1.r *= vec2.r;
	vec1.g *= vec2.g;
	vec1.b *= vec2.b;
	vec1.a *= vec2.a;
}

inline glm::vec4 operator*(const glm::vec4& vec1, const glm::vec4& vec2)
{
	glm::vec4 res = vec1;
	res *= vec2;

	return res;
}

inline void QuatShortestArc(const glm::vec3& from, const glm::vec3& to, glm::quat& outQuat)
{
	float angle = glm::dot(from, to);
	if (abs(angle) > 1.0f)
	{
		outQuat = NullQuaternion;
		return;
	}

	angle = acos(angle);

	if (abs(angle) > 0.1f)
	{
		glm::vec3 axe = glm::cross(from, to);
		outQuat = glm::angleAxis(angle, axe);
	}
	else
		outQuat = NullQuaternion;

	/*const float TINY = 1e8;

	glm::vec3 c = glm::cross(from, to);
	outQuat = D3DXQUATERNION(c.x, c.y, c.z, glm::dot(from, to));

	outQuat.w += 1.0f;      // reducing angle to halfangle
	if(outQuat.w <= TINY ) // angle close to PI
	{
		if( ( from.z*from.z ) > ( from.x*from.x ) )
			outQuat = D3DXQUATERNION(0, from.z, - from.y, outQuat.w); //from*vector3(1,0,0)
		else
			outQuat = D3DXQUATERNION(from.y, -from.x, 0, outQuat.w); //from*vector3(0,0,1)
	}
	D3DXQuaternionNormalize(&outQuat, &outQuat);*/
}

inline glm::quat QuatShortestArc(const glm::vec3& from, const glm::vec3& to)
{
	glm::quat outQuat;
	QuatShortestArc(from, to, outQuat);

	return outQuat;
}

inline float QuatAngle(const glm::quat& quat1, const glm::quat& quat2)
{
	return acos(abs(glm::dot(quat1, quat2) / (glm::length(quat1) * glm::length(quat2)))) * 2;
}

inline glm::quat QuatRotation(const glm::quat& quat1, const glm::quat& quat2)
{
	return quat2 * glm::inverse(quat1);
}

inline const glm::vec3& QuatRotateVec3(glm::vec3& res, const glm::vec3& vec, const glm::quat& quat)
{
	glm::quat q(vec.x * quat.x + vec.y * quat.y + vec.z * quat.z,
				vec.x * quat.w + vec.z * quat.y - vec.y * quat.z,
				vec.y * quat.w + vec.x * quat.z - vec.z * quat.x,
				vec.z * quat.w + vec.y * quat.x - vec.x * quat.y);
	float norm = glm::length2(quat);

	res = glm::vec3(quat.w * q.x + quat.x * q.w + quat.y * q.z - quat.z * q.y,
		quat.w * q.y + quat.y * q.w + quat.z * q.x - quat.x * q.z,
		quat.w * q.z + quat.z * q.w + quat.x * q.y - quat.y * q.x) * (1.0f/norm);

	return res;
}

inline void Line2FromNorm(const glm::vec2& norm, const glm::vec2& point, glm::vec3& outLine)
{
	//Уравнение разделяющей прямой через нормаль и точку
	//(N,X) + D = 0
	//Нормаль
	outLine.x = norm.x;
	outLine.y = norm.y;
	outLine.z = -Vec2Dot(norm, point);
}

inline glm::vec3 Line2FromNorm(const glm::vec2& norm, const glm::vec2& point)
{
	glm::vec3 res;
	Line2FromNorm(norm, point, res);
	return res;
}

inline void Line2FromDir(const glm::vec2& dir, const glm::vec2& point, glm::vec3& outLine)
{
	glm::vec2 norm;
	Vec2NormCW(dir, norm);
	Line2FromNorm(norm, point, outLine);
}

inline glm::vec3 Line2FromDir(const glm::vec2& dir, const glm::vec2& point)
{
	glm::vec3 res;
	Line2FromDir(dir, point, res);
	return res;
}

inline void Line2GetNorm(const glm::vec3& line, glm::vec2& norm)
{
	norm.x = line.x;
	norm.y = line.y;
}

inline void Line2GetDir(const glm::vec3& line, glm::vec2& dir)
{
	dir.x = line.x;
	dir.y = line.y;
	Vec2NormCCW(dir, dir);
}

inline void Line2GetRadiusVec(const glm::vec3& line, glm::vec2& outVec)
{
	outVec.x = -line.z * line.x;
	outVec.y = -line.z * line.y;
}

inline glm::vec2 Line2GetRadiusVec(const glm::vec3& line)
{
	glm::vec2 res;
	Line2GetRadiusVec(line, res);
	return res;
}

inline glm::vec2 Line2GetNorm(const glm::vec3& line)
{
	glm::vec2 res;
	Line2GetNorm(line, res);
	return res;
}

inline float Line2DistToPoint(const glm::vec3& line, const glm::vec2& point)
{
	return glm::dot(line, glm::vec3(point.x, point.y, 1.0f));
}

inline void Line2NormVecToPoint(const glm::vec3& line, const glm::vec2& point, glm::vec2& outNormVec)
{
	glm::vec2 lineNorm = Line2GetNorm(line);
	float dist = Line2DistToPoint(line, point);
	outNormVec = lineNorm * dist;
}

inline glm::vec2 Line2NormVecToPoint(const glm::vec3& line, const glm::vec2& point)
{
	glm::vec2 res;
	Line2NormVecToPoint(line, point, res);
	return res;
}

inline Vec3Range operator*(const Vec3Range& val1, float val2)
{
	Vec3Range res = val1;
	res *= val2;

	return res;
}

inline Vec3Range operator*(const Vec3Range& val1, const glm::vec3& val2)
{
	Vec3Range res = val1;
	res *= val2;

	return res;
}

//ray = rayPos + t * rayVec
//return t
inline bool RayCastIntersectSphere(const glm::vec3& rayPos, const glm::vec3& rayVec, const glm::vec3& spherePos, float sphereRadius, float* t)
{
	glm::vec3 v = rayPos - spherePos;

	float b = 2.0f * glm::dot(rayVec, v);
	float c = glm::dot(v, v) - sphereRadius * sphereRadius;

	// Находим дискриминант
	float discriminant = (b * b) - (4.0f * c);
	// Проверяем на мнимые числа
	if(discriminant < 0.0f)
		return false;

	discriminant = sqrtf(discriminant);

	float s0 = (-b + discriminant) / 2.0f;
	float s1 = (-b - discriminant) / 2.0f;

	float tRay = std::max(s0, s1);
	if (t)
		*t = tRay;

	// Если есть решение > 0, луч пересекает сферу
	return tRay > 0;
}

inline glm::vec4 PlaneNormalize(const glm::vec4 &plane)
{
	glm::vec4 pOut;
	float norm = sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
	if (norm)
	{
		pOut.x = plane.x / norm;
		pOut.y = plane.y / norm;
		pOut.z = plane.z / norm;
		pOut.w = plane.w / norm;
	}
	else
	{
		pOut.x = 0.0f;
		pOut.y = 0.0f;
		pOut.z = 0.0f;
		pOut.w = 0.0f;
	}
	return pOut;
}

inline glm::vec4 PlaneTransform(const glm::vec4 &plane, const D3DXMATRIX &mat)
{
	glm::vec4 pOut;
	pOut.x = mat.m[0][0] * plane.x + mat.m[1][0] * plane.y + mat.m[2][0] * plane.z + mat.m[3][0] * plane.w;
	pOut.y = mat.m[0][1] * plane.x + mat.m[1][1] * plane.y + mat.m[2][1] * plane.z + mat.m[3][1] * plane.w;
	pOut.z = mat.m[0][2] * plane.x + mat.m[1][2] * plane.y + mat.m[2][2] * plane.z + mat.m[3][2] * plane.w;
	pOut.w = mat.m[0][3] * plane.x + mat.m[1][3] * plane.y + mat.m[2][3] * plane.z + mat.m[3][3] * plane.w;
	return pOut;
}

inline glm::vec4 PlaneFromPointNormal(const glm::vec3 &point, const glm::vec3 &normal)
{
	return glm::vec4(normal, -glm::dot(point, normal));
}

inline glm::vec4 PlaneFromPoints(const glm::vec3 &pv1, const glm::vec3 &pv2, const glm::vec3 &pv3)
{
	glm::vec3 edge1 = pv2 - pv1;
	glm::vec3 edge2 = pv3 - pv1;
	glm::vec3 normal = glm::cross(edge1, edge2);
	normal = glm::normalize(normal);
	return PlaneFromPointNormal(pv1, normal);
}

inline float PlaneDotNormal(const glm::vec4 &plane, const glm::vec3 &point)
{
	return plane.x * point.x + plane.y * point.y + plane.z * point.z;
}

inline float PlaneDotCoord(const glm::vec4 &plane, const glm::vec3 &point)
{
	return plane.x * point.x + plane.y * point.y + plane.z * point.z + plane.w;
};

//}