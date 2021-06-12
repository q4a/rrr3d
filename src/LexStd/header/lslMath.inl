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

inline float ScalarTransform(float scalar, const D3DXVECTOR3& vec, const D3DXMATRIX& mat)
{
	D3DXVECTOR3 res;
	D3DXVec3TransformNormal(&res, &(vec * scalar), &mat);
	float len = D3DXVec3Length(&res);
	return scalar < 0 ? -len : len;
}




inline void BuildWorldMatrix(const D3DXVECTOR3& pos, const D3DXVECTOR3& scale, const D3DXQUATERNION& rot, D3DXMATRIX& outMat)
{
	D3DXMATRIX scaleMat;
	D3DXMatrixScaling(&scaleMat, scale.x, scale.y, scale.z);	

	D3DXMATRIX rotMat;
	D3DXMatrixRotationQuaternion(&rotMat, &rot);

	D3DXMATRIX transMat;
	D3DXMatrixTranslation(&transMat, pos.x, pos.y, pos.z);

	outMat = scaleMat * rotMat * transMat;
}

inline D3DXMATRIX BuildWorldMatrix(const D3DXVECTOR3& pos, const D3DXVECTOR3& scale, const D3DXQUATERNION& rot)
{
	D3DXMATRIX res;
	BuildWorldMatrix(pos, scale, rot, res);
	return res;
}

inline void MatrixRotationFromAxis(const D3DXVECTOR3& xVec, const D3DXVECTOR3& yVec, const D3DXVECTOR3& zVec, D3DXMATRIX& matOut)
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

inline void MatrixSetTranslation(const D3DXVECTOR3& vec, D3DXMATRIX& outMat)
{
	outMat._41 = vec.x;
	outMat._42 = vec.y;
	outMat._43 = vec.z;
}

inline void MatrixTranslate(const D3DXVECTOR3& vec, D3DXMATRIX& outMat)
{
	outMat._41 += vec.x;
	outMat._42 += vec.y;
	outMat._43 += vec.z;
}

inline void MatrixSetScale(const D3DXVECTOR3& vec, D3DXMATRIX& outMat)
{
	outMat._11 = vec.x;
	outMat._22 = vec.y;
	outMat._33 = vec.z;
}

inline void MatrixScale(const D3DXVECTOR3& vec, D3DXMATRIX& outMat)
{
	outMat._11 *= vec.x;
	outMat._22 *= vec.y;
	outMat._33 *= vec.z;
}

inline void MatGetPos(const D3DXMATRIX& mat, D3DXVECTOR3& outPos)
{
	outPos = mat.m[3];
}

inline D3DXVECTOR3 MatGetPos(const D3DXMATRIX& mat)
{
	D3DXVECTOR3 res;
	MatGetPos(mat, res);
	return res;
}




inline D3DXVECTOR2 Vec2TransformCoord(const D3DXVECTOR2& vec, const D3DXMATRIX& mat)
{
	D3DXVECTOR2 res;
	D3DXVec2TransformCoord(&res, &vec, &mat);
	return res;
}

//Поворот вектора на 90 градуос против часовой стрелки, иначе говоря его нормаль
inline void Vec2NormCCW(const D3DXVECTOR2& vec, D3DXVECTOR2& outVec)
{
	//На случай если &vec2 == &outVec
	float tmpX = vec.x;

	outVec.x = -vec.y;
	outVec.y = tmpX;
}

inline D3DXVECTOR2 Vec2NormCCW(const D3DXVECTOR2& vec2)
{
	D3DXVECTOR2 res;
	Vec2NormCCW(vec2, res);
	return res;
}

//Поворот вектора на 90 градуос по часовой стрелки, иначе говоря его нормаль
inline void Vec2NormCW(const D3DXVECTOR2& vec, D3DXVECTOR2& outVec)
{
	//На случай если &vec2 == &outVec
	float tmpX = vec.x;

	outVec.x = vec.y;
	outVec.y = -tmpX;
}

inline float Vec2Proj(const D3DXVECTOR2& vec1, const D3DXVECTOR2& vec2)
{
	return D3DXVec2Dot(&vec1, &vec2) / D3DXVec2Length(&vec2);
}

inline void operator*=(D3DXVECTOR2& vec1, const D3DXVECTOR2& vec2)
{
	vec1.x *= vec2.x;
	vec1.y *= vec2.y;
}

inline D3DXVECTOR2 operator*(const D3DXVECTOR2& vec1, const D3DXVECTOR2& vec2)
{
	D3DXVECTOR2 res = vec1;
	res *= vec2;

	return res;	
}

inline void operator/=(D3DXVECTOR2& vec1, const D3DXVECTOR2& vec2)
{
	vec1.x /= vec2.x;
	vec1.y /= vec2.y;
}

inline D3DXVECTOR2 operator/(const D3DXVECTOR2& vec1, const D3DXVECTOR2& vec2)
{
	D3DXVECTOR2 res = vec1;
	res /= vec2;

	return res;
}




inline D3DXVECTOR3 Vec3FromVec2(const D3DXVECTOR2& vec)
{
	return D3DXVECTOR3(vec.x, vec.y, 0.0f);
}

inline void Vec3Invert(const D3DXVECTOR3& vec, D3DXVECTOR3& rOut)
{
	rOut.x = 1 / vec.x;
	rOut.y = 1 / vec.y;
	rOut.z = 1 / vec.z;
};

inline D3DXVECTOR3 Vec3Invert(const D3DXVECTOR3& vec)
{
	D3DXVECTOR3 res;
	Vec3Invert(vec, res);
	return res;
};

inline D3DXVECTOR3 Vec3TransformCoord(const D3DXVECTOR3& vec, const D3DXMATRIX& mat)
{
	D3DXVECTOR3 res;
	D3DXVec3TransformCoord(&res, &vec, &mat);
	return res;
}

inline void Vec3Abs(const D3DXVECTOR3& vec, D3DXVECTOR3& rOut)
{
	rOut.x = abs(vec.x);
	rOut.y = abs(vec.y);
	rOut.z = abs(vec.z);
}

inline D3DXVECTOR3 Vec3Abs(const D3DXVECTOR3& vec)
{
	D3DXVECTOR3 res;
	Vec3Abs(vec, res);
	return res;
}

inline void Vec3Rotate(const D3DXVECTOR3& v, const D3DXQUATERNION& quat, D3DXVECTOR3& outVec)
{
	D3DXQUATERNION q(v.x * quat.w + v.z * quat.y - v.y * quat.z,
                	 v.y * quat.w + v.x * quat.z - v.z * quat.x,
					 v.z * quat.w + v.y * quat.x - v.x * quat.y,
					 v.x * quat.x + v.y * quat.y + v.z * quat.z);

	outVec.x = quat.w * q.x + quat.x * q.w + quat.y * q.z - quat.z * q.y;
	outVec.y = quat.w * q.y + quat.y * q.w + quat.z * q.x - quat.x * q.z;
	outVec.z = quat.w * q.z + quat.z * q.w + quat.x * q.y - quat.y * q.x;

	outVec /= D3DXQuaternionLengthSq(&quat);
}

inline void operator*=(D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2)
{
	vec1.x *= vec2.x;
	vec1.y *= vec2.y;
	vec1.z *= vec2.z;
}

inline D3DXVECTOR3 operator*(const D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2)
{
	D3DXVECTOR3 res = vec1;
	res *= vec2;

	return res;
}

inline void operator/=(D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2)
{
	vec1.x /= vec2.x;
	vec1.y /= vec2.y;
	vec1.z /= vec2.z;
}

inline D3DXVECTOR3 operator/(const D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2)
{
	D3DXVECTOR3 res = vec1;
	res /= vec2;

	return res;
}

inline bool operator>=(const D3DXVECTOR3& vec1, float scalar)
{
	return vec1.x >= scalar &&
		   vec1.y >= scalar &&
		   vec1.z >= scalar;
}

inline bool operator<(const D3DXVECTOR3& vec1, float scalar)
{
	return vec1.x < scalar &&
		   vec1.y < scalar &&
		   vec1.z < scalar;
}

inline bool operator>(const D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2)
{
	return vec1.x > vec2.x && 
		   vec1.y > vec2.y && 
		   vec1.z > vec2.z;
}

inline bool operator<(const D3DXVECTOR3& vec1, const D3DXVECTOR3& vec2)
{
	return vec1.x < vec2.x && 
		   vec1.y < vec2.y && 
		   vec1.z < vec2.z;
}




inline D3DXVECTOR4 Vec4FromVec2(const D3DXVECTOR2& vec)
{
	return D3DXVECTOR4(vec.x, vec.y, 0, 0);
}

inline D3DXVECTOR4 Vec4FromVec3(const D3DXVECTOR3& vec)
{
	return D3DXVECTOR4(vec.x, vec.y, vec.z, 0);
}




inline void operator*=(D3DXCOLOR& vec1, const D3DXCOLOR& vec2)
{
	vec1.r *= vec2.r;
	vec1.g *= vec2.g;
	vec1.b *= vec2.b;
	vec1.a *= vec2.a;
}

inline D3DXCOLOR operator*(const D3DXCOLOR& vec1, const D3DXCOLOR& vec2)
{
	D3DXCOLOR res = vec1;
	res *= vec2;

	return res;	
}




inline void QuatShortestArc(const D3DXVECTOR3& from, const D3DXVECTOR3& to, D3DXQUATERNION& outQuat)
{
	float angle = D3DXVec3Dot(&from, &to);
	if (abs(angle) > 1.0f)
	{
		outQuat = NullQuaternion;
		return;
	}

	angle = acos(angle);

	if (abs(angle) > 0.1f)
	{
		D3DXVECTOR3 axe;
		D3DXVec3Cross(&axe, &from, &to);
		D3DXQUATERNION rot;
		D3DXQuaternionRotationAxis(&outQuat, &axe, angle);
	}
	else
		outQuat = NullQuaternion;

	/*const float TINY = 1e8;

	D3DXVECTOR3 c;
	D3DXVec3Cross(&c, &from, &to);
	outQuat = D3DXQUATERNION(c.x, c.y, c.z, D3DXVec3Dot(&from, &to));

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

inline D3DXQUATERNION QuatShortestArc(const D3DXVECTOR3& from, const D3DXVECTOR3& to)
{
	D3DXQUATERNION outQuat;
	QuatShortestArc(from, to, outQuat);

	return outQuat;
}

inline float QuatAngle(const D3DXQUATERNION& quat1, const D3DXQUATERNION& quat2)
{
	return acos(abs(D3DXQuaternionDot(&quat1, &quat2)/(D3DXQuaternionLength(&quat1) * D3DXQuaternionLength(&quat2)))) * 2;
}

inline const D3DXQUATERNION& QuatRotation(D3DXQUATERNION& quat, const D3DXQUATERNION& quat1, const D3DXQUATERNION& quat2)
{
	quat = (*D3DXQuaternionInverse(&quat, &quat1)) * quat2;
	return quat;
}

inline const D3DXVECTOR3& QuatRotateVec3(D3DXVECTOR3& res, const D3DXVECTOR3& vec, const D3DXQUATERNION& quat)
{
	D3DXQUATERNION q(vec.x * quat.w + vec.z * quat.y - vec.y * quat.z,
		vec.y * quat.w + vec.x * quat.z - vec.z * quat.x,
		vec.z * quat.w + vec.y * quat.x - vec.x * quat.y,
		vec.x * quat.x + vec.y * quat.y + vec.z * quat.z);
	float norm = D3DXQuaternionLengthSq(&quat);

	res = D3DXVECTOR3(quat.w * q.x + quat.x * q.w + quat.y * q.z - quat.z * q.y,
		quat.w * q.y + quat.y * q.w + quat.z * q.x - quat.x * q.z,
		quat.w * q.z + quat.z * q.w + quat.x * q.y - quat.y * q.x) * (1.0f/norm);

	return res;
}




inline void Line2FromNorm(const D3DXVECTOR2& norm, const D3DXVECTOR2& point, D3DXVECTOR3& outLine)
{
	//Уравнение разделяющей прямой через нормаль и точку
	//(N,X) + D = 0
	//Нормаль
	outLine.x = norm.x;
	outLine.y = norm.y;
	outLine.z = -D3DXVec2Dot(&norm, &point);
}

inline D3DXVECTOR3 Line2FromNorm(const D3DXVECTOR2& norm, const D3DXVECTOR2& point)
{
	D3DXVECTOR3 res;
	Line2FromNorm(norm, point, res);	
	return res;
}

inline void Line2FromDir(const D3DXVECTOR2& dir, const D3DXVECTOR2& point, D3DXVECTOR3& outLine)
{
	D3DXVECTOR2 norm;
	Vec2NormCW(dir, norm);
	Line2FromNorm(norm, point, outLine);
}

inline D3DXVECTOR3 Line2FromDir(const D3DXVECTOR2& dir, const D3DXVECTOR2& point)
{
	D3DXVECTOR3 res;
	Line2FromDir(dir, point, res);
	return res;
}

inline void Line2GetNorm(const D3DXVECTOR3& line, D3DXVECTOR2& norm)
{
	norm.x = line.x;
	norm.y = line.y;
}

inline void Line2GetDir(const D3DXVECTOR3& line, D3DXVECTOR2& dir)
{
	dir.x = line.x;
	dir.y = line.y;
	Vec2NormCCW(dir, dir);
}

inline void Line2GetRadiusVec(const D3DXVECTOR3& line, D3DXVECTOR2& outVec)
{
	outVec.x = -line.z * line.x;
	outVec.y = -line.z * line.y;
}

inline D3DXVECTOR2 Line2GetRadiusVec(const D3DXVECTOR3& line)
{
	D3DXVECTOR2 res;
	Line2GetRadiusVec(line, res);
	return res;
}

inline D3DXVECTOR2 Line2GetNorm(const D3DXVECTOR3& line)
{
	D3DXVECTOR2 res;
	Line2GetNorm(line, res);
	return res;
}

inline float Line2DistToPoint(const D3DXVECTOR3& line, const D3DXVECTOR2& point)
{
	return D3DXVec3Dot(&line,  &D3DXVECTOR3(point.x, point.y, 1.0f));
}

inline void Line2NormVecToPoint(const D3DXVECTOR3& line, const D3DXVECTOR2& point, D3DXVECTOR2& outNormVec)
{
	D3DXVECTOR2 lineNorm = Line2GetNorm(line);
	float dist = Line2DistToPoint(line, point);
	outNormVec = lineNorm * dist;
}

inline D3DXVECTOR2 Line2NormVecToPoint(const D3DXVECTOR3& line, const D3DXVECTOR2& point)
{
	D3DXVECTOR2 res;
	Line2NormVecToPoint(line, point, res);
	return res;
}




inline Vec3Range operator*(const Vec3Range& val1, float val2)
{
	Vec3Range res = val1;
	res *= val2;

	return res;
}

inline Vec3Range operator*(const Vec3Range& val1, const D3DXVECTOR3& val2)
{
	Vec3Range res = val1;
	res *= val2;

	return res;
}




//ray = rayPos + t * rayVec
//return t
inline bool RayCastIntersectSphere(const D3DXVECTOR3& rayPos, const D3DXVECTOR3& rayVec, const D3DXVECTOR3& spherePos, float sphereRadius, float* t)
{
	D3DXVECTOR3 v = rayPos - spherePos;
	
	float b = 2.0f * D3DXVec3Dot(&rayVec, &v);
	float c = D3DXVec3Dot(&v, &v) - sphereRadius * sphereRadius;

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

inline float PlaneDistToPoint(const D3DXPLANE& plane, const D3DXVECTOR3& point)
{
	return D3DXPlaneDotCoord(&plane, &point);
};

//}