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

inline glm::mat4 Matrix4DxToGlm(const D3DMATRIX &mat)
{
	// mat - DX, mat.m[0] 1-st row, mat.m[0][1] = mat._12 - 1-st row 2-nd column
	glm::mat4 outMat(mat._11, mat._21, mat._31, mat._41, // 1-st column
	                  mat._12, mat._22, mat._32, mat._42, // 2-nd column
	                  mat._13, mat._23, mat._33, mat._43, // 3-rd column
	                  mat._14, mat._24, mat._34, mat._44);// 4-th column
	return outMat;
}

inline D3DMATRIX Matrix4GlmToDx(const glm::mat4 &mat)
{
	// mat - GLM, mat[0] 1-st column, mat[0][1] = mat[0].y - 1-st column 2-nd row
	D3DMATRIX outMat;
	outMat.m[0][0] = mat[0].x;
	outMat.m[0][1] = mat[1].x;
	outMat.m[0][2] = mat[2].x;
	outMat.m[0][3] = mat[3].x;
	outMat.m[1][0] = mat[0].y;
	outMat.m[1][1] = mat[1].y;
	outMat.m[1][2] = mat[2].y;
	outMat.m[1][3] = mat[3].y;
	outMat.m[2][0] = mat[0].z;
	outMat.m[2][1] = mat[1].z;
	outMat.m[2][2] = mat[2].z;
	outMat.m[2][3] = mat[3].z;
	outMat.m[3][0] = mat[0].w;
	outMat.m[3][1] = mat[1].w;
	outMat.m[3][2] = mat[2].w;
	outMat.m[3][3] = mat[3].w;
	return outMat;
}

inline D3DVECTOR Vec3GlmToDx(const glm::vec3 v3)
{
	D3DVECTOR outVec;
	outVec.x = v3.x;
	outVec.y = v3.y;
	outVec.z = v3.z;
	return outVec;
}

inline D3DCOLOR Vec4ToColor(const glm::vec4 &vec)
{
	return D3DCOLOR_COLORVALUE(vec.r, vec.g, vec.b, vec.a);
}

inline glm::vec4 ColorToVec4(const D3DCOLOR &col)
{
	glm::vec4 outVec;
	outVec.r = (col >> 16 & 255) / 255.0f;
	outVec.g = (col >> 8 & 255) / 255.0f;
	outVec.b = (col & 255) / 255.0f;
	outVec.a = (col >> 24 & 255) / 255.0f;
	return outVec;
}

inline glm::vec4 ColorVToVec4(const D3DCOLORVALUE &cv)
{
	glm::vec4 outVec(cv.r, cv.g, cv.b, cv.a);
	return outVec;
}

inline glm::vec3 Vec3TransformCoord(const glm::vec3 &vec, const D3DMATRIX &mat)
{
	glm::vec3 outVec;
	float norm = mat.m[0][3] * vec.x + mat.m[1][3] * vec.y + mat.m[2][3] * vec.z + mat.m[3][3];

	outVec.x = (mat.m[0][0] * vec.x + mat.m[1][0] * vec.y + mat.m[2][0] * vec.z + mat.m[3][0]) / norm;
	outVec.y = (mat.m[0][1] * vec.x + mat.m[1][1] * vec.y + mat.m[2][1] * vec.z + mat.m[3][1]) / norm;
	outVec.z = (mat.m[0][2] * vec.x + mat.m[1][2] * vec.y + mat.m[2][2] * vec.z + mat.m[3][2]) / norm;
	return outVec;
}

inline glm::vec3 Vec3TransformNormal(const glm::vec3 &vec, const D3DMATRIX &mat)
{
	glm::vec3 outVec;
	outVec.x = mat.m[0][0] * vec.x + mat.m[1][0] * vec.y + mat.m[2][0] * vec.z;
	outVec.y = mat.m[0][1] * vec.x + mat.m[1][1] * vec.y + mat.m[2][1] * vec.z;
	outVec.z = mat.m[0][2] * vec.x + mat.m[1][2] * vec.y + mat.m[2][2] * vec.z;
	return outVec;
}

inline glm::vec4 Vec3Transform(const glm::vec3 &vec, const D3DMATRIX &mat)
{
	glm::vec4 outVec;
	outVec.x = mat.m[0][0] * vec.x + mat.m[1][0] * vec.y + mat.m[2][0] * vec.z + mat.m[3][0];
	outVec.y = mat.m[0][1] * vec.x + mat.m[1][1] * vec.y + mat.m[2][1] * vec.z + mat.m[3][1];
	outVec.z = mat.m[0][2] * vec.x + mat.m[1][2] * vec.y + mat.m[2][2] * vec.z + mat.m[3][2];
	outVec.w = mat.m[0][3] * vec.x + mat.m[1][3] * vec.y + mat.m[2][3] * vec.z + mat.m[3][3];
	return outVec;
}

inline glm::vec4 Vec4Transform(const glm::vec4 &vec, const D3DMATRIX &mat)
{
	glm::vec4 outVec;
	outVec.x = mat.m[0][0] * vec.x + mat.m[1][0] * vec.y + mat.m[2][0] * vec.z + mat.m[3][0] * vec.w;
	outVec.y = mat.m[0][1] * vec.x + mat.m[1][1] * vec.y + mat.m[2][1] * vec.z + mat.m[3][1] * vec.w;
	outVec.z = mat.m[0][2] * vec.x + mat.m[1][2] * vec.y + mat.m[2][2] * vec.z + mat.m[3][2] * vec.w;
	outVec.w = mat.m[0][3] * vec.x + mat.m[1][3] * vec.y + mat.m[2][3] * vec.z + mat.m[3][3] * vec.w;
	return outVec;
}

inline float ScalarTransform(float scalar, const glm::vec3& vec, const D3DMATRIX& mat)
{
	glm::vec3 outVec = Vec3TransformNormal(vec * scalar, mat);
	float len = glm::length(outVec);
	return scalar < 0 ? -len : len;
}

inline D3DMATRIX MakeMatrix(float _11, float _12, float _13, float _14,
                            float _21, float _22, float _23, float _24,
                            float _31, float _32, float _33, float _34,
                            float _41, float _42, float _43, float _44)
{
	D3DMATRIX outMat;
	outMat.m[0][0] = _11;
	outMat.m[0][1] = _12;
	outMat.m[0][2] = _13;
	outMat.m[0][3] = _14;
	outMat.m[1][0] = _21;
	outMat.m[1][1] = _22;
	outMat.m[1][2] = _23;
	outMat.m[1][3] = _24;
	outMat.m[2][0] = _31;
	outMat.m[2][1] = _32;
	outMat.m[2][2] = _33;
	outMat.m[2][3] = _34;
	outMat.m[3][0] = _41;
	outMat.m[3][1] = _42;
	outMat.m[3][2] = _43;
	outMat.m[3][3] = _44;
	return outMat;
}

inline D3DMATRIX MatrixMultiply(const D3DMATRIX &mat1, const D3DMATRIX &mat2)
{
	D3DMATRIX outMat;
	int i, j;
	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			outMat.m[i][j] = mat1.m[i][0] * mat2.m[0][j] + mat1.m[i][1] * mat2.m[1][j] + mat1.m[i][2] * mat2.m[2][j] +
			                 mat1.m[i][3] * mat2.m[3][j];
		}
	}
	return outMat;
}

inline D3DMATRIX MatrixScaling(float sx, float sy, float sz)
{
	D3DMATRIX outMat = IdentityMatrix;
	outMat.m[0][0] = sx;
	outMat.m[1][1] = sy;
	outMat.m[2][2] = sz;
	return outMat;
}

inline D3DMATRIX MatrixTranslation(float x, float y, float z)
{
	D3DMATRIX outMat = IdentityMatrix;
	outMat.m[3][0] = x;
	outMat.m[3][1] = y;
	outMat.m[3][2] = z;
	return outMat;
}

inline D3DMATRIX MatrixTranspose(const D3DMATRIX &mat)
{
	D3DMATRIX outMat;
	int i, j;
	for (i = 0; i < 4; ++i)
		for (j = 0; j < 4; ++j)
			outMat.m[i][j] = mat.m[j][i];

	return outMat;
}

inline void BuildWorldMatrix(const glm::vec3& pos, const glm::vec3& scale, const glm::quat& rot, D3DMATRIX& outMat)
{
	D3DMATRIX scaleMat = MatrixScaling(scale.x, scale.y, scale.z);
	D3DMATRIX rotMat = Matrix4GlmToDx(glm::transpose(glm::mat4_cast(rot)));
	D3DMATRIX transMat = MatrixTranslation(pos.x, pos.y, pos.z);
	outMat = MatrixMultiply(MatrixMultiply(scaleMat, rotMat), transMat);
}

inline D3DMATRIX BuildWorldMatrix(const glm::vec3& pos, const glm::vec3& scale, const glm::quat& rot)
{
	D3DMATRIX outMat;
	BuildWorldMatrix(pos, scale, rot, outMat);
	return outMat;
}

inline D3DMATRIX *MatrixInverse(D3DMATRIX *outMat, float *pdeterminant, const D3DMATRIX &mat)
{
	float det, t[3], v[16];
	int i, j;

	t[0] = mat.m[2][2] * mat.m[3][3] - mat.m[2][3] * mat.m[3][2];
	t[1] = mat.m[1][2] * mat.m[3][3] - mat.m[1][3] * mat.m[3][2];
	t[2] = mat.m[1][2] * mat.m[2][3] - mat.m[1][3] * mat.m[2][2];
	v[0] = mat.m[1][1] * t[0] - mat.m[2][1] * t[1] + mat.m[3][1] * t[2];
	v[4] = -mat.m[1][0] * t[0] + mat.m[2][0] * t[1] - mat.m[3][0] * t[2];

	t[0] = mat.m[1][0] * mat.m[2][1] - mat.m[2][0] * mat.m[1][1];
	t[1] = mat.m[1][0] * mat.m[3][1] - mat.m[3][0] * mat.m[1][1];
	t[2] = mat.m[2][0] * mat.m[3][1] - mat.m[3][0] * mat.m[2][1];
	v[8] = mat.m[3][3] * t[0] - mat.m[2][3] * t[1] + mat.m[1][3] * t[2];
	v[12] = -mat.m[3][2] * t[0] + mat.m[2][2] * t[1] - mat.m[1][2] * t[2];

	det = mat.m[0][0] * v[0] + mat.m[0][1] * v[4] + mat.m[0][2] * v[8] + mat.m[0][3] * v[12];
	if (det == 0.0f)
		return nullptr;
	if (pdeterminant)
		*pdeterminant = det;

	t[0] = mat.m[2][2] * mat.m[3][3] - mat.m[2][3] * mat.m[3][2];
	t[1] = mat.m[0][2] * mat.m[3][3] - mat.m[0][3] * mat.m[3][2];
	t[2] = mat.m[0][2] * mat.m[2][3] - mat.m[0][3] * mat.m[2][2];
	v[1] = -mat.m[0][1] * t[0] + mat.m[2][1] * t[1] - mat.m[3][1] * t[2];
	v[5] = mat.m[0][0] * t[0] - mat.m[2][0] * t[1] + mat.m[3][0] * t[2];

	t[0] = mat.m[0][0] * mat.m[2][1] - mat.m[2][0] * mat.m[0][1];
	t[1] = mat.m[3][0] * mat.m[0][1] - mat.m[0][0] * mat.m[3][1];
	t[2] = mat.m[2][0] * mat.m[3][1] - mat.m[3][0] * mat.m[2][1];
	v[9] = -mat.m[3][3] * t[0] - mat.m[2][3] * t[1] - mat.m[0][3] * t[2];
	v[13] = mat.m[3][2] * t[0] + mat.m[2][2] * t[1] + mat.m[0][2] * t[2];

	t[0] = mat.m[1][2] * mat.m[3][3] - mat.m[1][3] * mat.m[3][2];
	t[1] = mat.m[0][2] * mat.m[3][3] - mat.m[0][3] * mat.m[3][2];
	t[2] = mat.m[0][2] * mat.m[1][3] - mat.m[0][3] * mat.m[1][2];
	v[2] = mat.m[0][1] * t[0] - mat.m[1][1] * t[1] + mat.m[3][1] * t[2];
	v[6] = -mat.m[0][0] * t[0] + mat.m[1][0] * t[1] - mat.m[3][0] * t[2];

	t[0] = mat.m[0][0] * mat.m[1][1] - mat.m[1][0] * mat.m[0][1];
	t[1] = mat.m[3][0] * mat.m[0][1] - mat.m[0][0] * mat.m[3][1];
	t[2] = mat.m[1][0] * mat.m[3][1] - mat.m[3][0] * mat.m[1][1];
	v[10] = mat.m[3][3] * t[0] + mat.m[1][3] * t[1] + mat.m[0][3] * t[2];
	v[14] = -mat.m[3][2] * t[0] - mat.m[1][2] * t[1] - mat.m[0][2] * t[2];

	t[0] = mat.m[1][2] * mat.m[2][3] - mat.m[1][3] * mat.m[2][2];
	t[1] = mat.m[0][2] * mat.m[2][3] - mat.m[0][3] * mat.m[2][2];
	t[2] = mat.m[0][2] * mat.m[1][3] - mat.m[0][3] * mat.m[1][2];
	v[3] = -mat.m[0][1] * t[0] + mat.m[1][1] * t[1] - mat.m[2][1] * t[2];
	v[7] = mat.m[0][0] * t[0] - mat.m[1][0] * t[1] + mat.m[2][0] * t[2];

	v[11] = -mat.m[0][0] * (mat.m[1][1] * mat.m[2][3] - mat.m[1][3] * mat.m[2][1]) +
	        mat.m[1][0] * (mat.m[0][1] * mat.m[2][3] - mat.m[0][3] * mat.m[2][1]) -
	        mat.m[2][0] * (mat.m[0][1] * mat.m[1][3] - mat.m[0][3] * mat.m[1][1]);

	v[15] = mat.m[0][0] * (mat.m[1][1] * mat.m[2][2] - mat.m[1][2] * mat.m[2][1]) -
	        mat.m[1][0] * (mat.m[0][1] * mat.m[2][2] - mat.m[0][2] * mat.m[2][1]) +
	        mat.m[2][0] * (mat.m[0][1] * mat.m[1][2] - mat.m[0][2] * mat.m[1][1]);

	det = 1.0f / det;

	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			outMat->m[i][j] = v[4 * i + j] * det;

	return outMat;
}

inline D3DMATRIX MatrixLookAtRH(const glm::vec3 &eye, const glm::vec3 &at, const glm::vec3 &up)
{
	D3DMATRIX outMat;
	glm::vec3 right, upn, vec;

	vec = glm::normalize(at - eye);
	right = glm::cross(up, vec);
	upn = glm::cross(vec, right);
	right = glm::normalize(right);
	upn = glm::normalize(upn);

	outMat.m[0][0] = -right.x;
	outMat.m[1][0] = -right.y;
	outMat.m[2][0] = -right.z;
	outMat.m[3][0] = glm::dot(right, eye);
	outMat.m[0][1] = upn.x;
	outMat.m[1][1] = upn.y;
	outMat.m[2][1] = upn.z;
	outMat.m[3][1] = -glm::dot(upn, eye);
	outMat.m[0][2] = -vec.x;
	outMat.m[1][2] = -vec.y;
	outMat.m[2][2] = -vec.z;
	outMat.m[3][2] = glm::dot(vec, eye);
	outMat.m[0][3] = 0.0f;
	outMat.m[1][3] = 0.0f;
	outMat.m[2][3] = 0.0f;
	outMat.m[3][3] = 1.0f;

	return outMat;
}

inline D3DMATRIX MatrixOrthoRH(float w, float h, float zn, float zf)
{
	D3DMATRIX outMat = IdentityMatrix;
	outMat.m[0][0] = 2.0f / w;
	outMat.m[1][1] = 2.0f / h;
	outMat.m[2][2] = 1.0f / (zn - zf);
	outMat.m[3][2] = zn / (zn - zf);
	return outMat;
}

inline D3DMATRIX MatrixPerspectiveFovRH(float fovy, float aspect, float zn, float zf)
{
	D3DMATRIX outMat = IdentityMatrix;
	outMat.m[0][0] = 1.0f / (aspect * tanf(fovy / 2.0f));
	outMat.m[1][1] = 1.0f / tanf(fovy / 2.0f);
	outMat.m[2][2] = zf / (zn - zf);
	outMat.m[2][3] = -1.0f;
	outMat.m[3][2] = (zf * zn) / (zn - zf);
	outMat.m[3][3] = 0.0f;
	return outMat;
}

inline D3DMATRIX MatrixRotationAxis(const glm::vec3 &vec, float angle)
{
	D3DMATRIX outMat;
	glm::vec3 nv = glm::normalize(vec);

	float sangle, cangle, cdiff;
	sangle = sinf(angle);
	cangle = cosf(angle);
	cdiff = 1.0f - cangle;

	outMat.m[0][0] = cdiff * nv.x * nv.x + cangle;
	outMat.m[1][0] = cdiff * nv.x * nv.y - sangle * nv.z;
	outMat.m[2][0] = cdiff * nv.x * nv.z + sangle * nv.y;
	outMat.m[3][0] = 0.0f;
	outMat.m[0][1] = cdiff * nv.y * nv.x + sangle * nv.z;
	outMat.m[1][1] = cdiff * nv.y * nv.y + cangle;
	outMat.m[2][1] = cdiff * nv.y * nv.z - sangle * nv.x;
	outMat.m[3][1] = 0.0f;
	outMat.m[0][2] = cdiff * nv.z * nv.x - sangle * nv.y;
	outMat.m[1][2] = cdiff * nv.z * nv.y + sangle * nv.x;
	outMat.m[2][2] = cdiff * nv.z * nv.z + cangle;
	outMat.m[3][2] = 0.0f;
	outMat.m[0][3] = 0.0f;
	outMat.m[1][3] = 0.0f;
	outMat.m[2][3] = 0.0f;
	outMat.m[3][3] = 1.0f;

	return outMat;
}

inline void MatrixRotationFromAxis(const glm::vec3& xVec, const glm::vec3& yVec, const glm::vec3& zVec, D3DMATRIX& outMat)
{
	outMat._11 = xVec.x;
	outMat._12 = xVec.y;
	outMat._13 = xVec.z;
	outMat._14 = 0.0f;
	outMat._21 = yVec.x;
	outMat._22 = yVec.y;
	outMat._23 = yVec.z;
	outMat._24 = 0.0f;
	outMat._31 = zVec.x;
	outMat._32 = zVec.y;
	outMat._33 = zVec.z;
	outMat._34 = 0.0f;
	outMat._41 = 0.0f;
	outMat._42 = 0.0f;
	outMat._43 = 0.0f;
	outMat._44 = 1.0f;
}

inline void MatrixSetTranslation(const glm::vec3& vec, D3DMATRIX& outMat)
{
	outMat._41 = vec.x;
	outMat._42 = vec.y;
	outMat._43 = vec.z;
}

inline void MatrixTranslate(const glm::vec3& vec, D3DMATRIX& outMat)
{
	outMat._41 += vec.x;
	outMat._42 += vec.y;
	outMat._43 += vec.z;
}

inline void MatrixSetScale(const glm::vec3& vec, D3DMATRIX& outMat)
{
	outMat._11 = vec.x;
	outMat._22 = vec.y;
	outMat._33 = vec.z;
}

inline void MatrixScale(const glm::vec3& vec, D3DMATRIX& outMat)
{
	outMat._11 *= vec.x;
	outMat._22 *= vec.y;
	outMat._33 *= vec.z;
}

inline void MatGetPos(const D3DMATRIX& mat, glm::vec3& outPos)
{
	outPos.x = mat.m[3][0];
	outPos.y = mat.m[3][1];
	outPos.z = mat.m[3][2];
}

inline glm::vec3 MatGetPos(const D3DMATRIX& mat)
{
	glm::vec3 outPos;
	MatGetPos(mat, outPos);
	return outPos;
}

inline glm::vec2 Vec2Minimize(const glm::vec2 &vec1, const glm::vec2 &vec2)
{
	glm::vec2 outVec;
	outVec.x = vec1.x < vec2.x ? vec1.x : vec2.x;
	outVec.y = vec1.y < vec2.y ? vec1.y : vec2.y;
	return outVec;
}

inline glm::vec2 Vec2Maximize(const glm::vec2 &vec1, const glm::vec2 &vec2)
{
	glm::vec2 outVec;
	outVec.x = vec1.x > vec2.x ? vec1.x : vec2.x;
	outVec.y = vec1.y > vec2.y ? vec1.y : vec2.y;
	return outVec;
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
	glm::vec2 outVec;
	outVec.x = vec1.x + scalar * (vec2.x - vec1.x);
	outVec.y = vec1.y + scalar * (vec2.y - vec1.y);
	return outVec;
}

inline float Vec2CCW(const glm::vec2 &vec1, const glm::vec2 &vec2)
{
	return vec1.x * vec2.y - vec1.y * vec2.x;
}

//������� ������� �� 90 ������� ������ ������� �������, ����� ������ ��� �������
inline void Vec2NormCCW(const glm::vec2& vec, glm::vec2& outVec)
{
	//�� ������ ���� &vec2 == &outVec
	float tmpX = vec.x;

	outVec.x = -vec.y;
	outVec.y = tmpX;
}

inline glm::vec2 Vec2NormCCW(const glm::vec2& vec2)
{
	glm::vec2 outVec;
	Vec2NormCCW(vec2, outVec);
	return outVec;
}

//������� ������� �� 90 ������� �� ������� �������, ����� ������ ��� �������
inline void Vec2NormCW(const glm::vec2& vec, glm::vec2& outVec)
{
	//�� ������ ���� &vec2 == &outVec
	float tmpX = vec.x;

	outVec.x = vec.y;
	outVec.y = -tmpX;
}

inline float Vec2Proj(const glm::vec2& vec1, const glm::vec2& vec2)
{
	return Vec2Dot(vec1, vec2) / glm::length(vec2);
}

inline void operator*=(glm::vec2& outVec1, const glm::vec2& vec2)
{
	outVec1.x *= vec2.x;
	outVec1.y *= vec2.y;
}

inline glm::vec2 operator*(const glm::vec2& vec1, const glm::vec2& vec2)
{
	glm::vec2 outVec = vec1;
	outVec *= vec2;

	return outVec;
}

inline void operator/=(glm::vec2& outVec1, const glm::vec2& vec2)
{
	outVec1.x /= vec2.x;
	outVec1.y /= vec2.y;
}

inline glm::vec2 operator/(const glm::vec2& vec1, const glm::vec2& vec2)
{
	glm::vec2 outVec = vec1;
	outVec /= vec2;

	return outVec;
}

inline glm::vec3 Vec3FromVec2(const glm::vec2& vec)
{
	return glm::vec3(vec.x, vec.y, 0.0f);
}

inline void Vec3Invert(const glm::vec3& vec, glm::vec3& outVec)
{
	outVec.x = 1 / vec.x;
	outVec.y = 1 / vec.y;
	outVec.z = 1 / vec.z;
};

inline glm::vec3 Vec3Invert(const glm::vec3& vec)
{
	glm::vec3 outVec;
	Vec3Invert(vec, outVec);
	return outVec;
};

inline void Vec3Abs(const glm::vec3& vec, glm::vec3& outVec)
{
	outVec.x = std::abs(vec.x);
	outVec.y = std::abs(vec.y);
	outVec.z = std::abs(vec.z);
}

inline glm::vec3 Vec3Abs(const glm::vec3& vec)
{
	glm::vec3 outVec;
	Vec3Abs(vec, outVec);
	return outVec;
}

// It's possible to use glm:mix, but it gives a bit different result for Z axe.
inline glm::vec3 Vec3Lerp(const glm::vec3 &vec1, const glm::vec3 &vec2, float scalar)
{
	glm::vec3 outVec;
	outVec.x = vec1.x + scalar * (vec2.x - vec1.x);
	outVec.y = vec1.y + scalar * (vec2.y - vec1.y);
	outVec.z = vec1.z + scalar * (vec2.z - vec1.z);
	return outVec;
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

inline void operator*=(glm::vec3& outVec1, const glm::vec3& vec2)
{
	outVec1.x *= vec2.x;
	outVec1.y *= vec2.y;
	outVec1.z *= vec2.z;
}

inline glm::vec3 operator*(const glm::vec3& vec1, const glm::vec3& vec2)
{
	glm::vec3 outVec = vec1;
	outVec *= vec2;

	return outVec;
}

inline void operator/=(glm::vec3& outVec1, const glm::vec3& vec2)
{
	outVec1.x /= vec2.x;
	outVec1.y /= vec2.y;
	outVec1.z /= vec2.z;
}

inline glm::vec3 operator/(const glm::vec3& vec1, const glm::vec3& vec2)
{
	glm::vec3 outVec = vec1;
	outVec /= vec2;

	return outVec;
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

inline void operator*=(glm::vec4& outVec1, const glm::vec4& vec2)
{
	outVec1.r *= vec2.r;
	outVec1.g *= vec2.g;
	outVec1.b *= vec2.b;
	outVec1.a *= vec2.a;
}

inline glm::vec4 operator*(const glm::vec4& vec1, const glm::vec4& vec2)
{
	glm::vec4 outVec = vec1;
	outVec *= vec2;

	return outVec;
}

inline void QuatShortestArc(const glm::vec3& from, const glm::vec3& to, glm::quat& outQuat)
{
	float angle = glm::dot(from, to);
	if (std::abs(angle) > 1.0f)
	{
		outQuat = NullQuaternion;
		return;
	}

	angle = acos(angle);

	if (std::abs(angle) > 0.1f)
	{
		glm::vec3 axe = glm::cross(from, to);
		outQuat = glm::angleAxis(angle, axe);
	}
	else
		outQuat = NullQuaternion;
}

inline glm::quat QuatShortestArc(const glm::vec3& from, const glm::vec3& to)
{
	glm::quat outQuat;
	QuatShortestArc(from, to, outQuat);

	return outQuat;
}

inline float QuatAngle(const glm::quat& quat1, const glm::quat& quat2)
{
	return acos(std::abs(glm::dot(quat1, quat2) / (glm::length(quat1) * glm::length(quat2)))) * 2;
}

inline glm::quat QuatRotation(const glm::quat& quat1, const glm::quat& quat2)
{
	return quat2 * glm::inverse(quat1);
}

inline const glm::vec3& QuatRotateVec3(glm::vec3& outVec, const glm::vec3& vec, const glm::quat& quat)
{
	glm::quat q(vec.x * quat.x + vec.y * quat.y + vec.z * quat.z,
				vec.x * quat.w + vec.z * quat.y - vec.y * quat.z,
				vec.y * quat.w + vec.x * quat.z - vec.z * quat.x,
				vec.z * quat.w + vec.y * quat.x - vec.x * quat.y);
	float norm = glm::length2(quat);

	outVec = glm::vec3(quat.w * q.x + quat.x * q.w + quat.y * q.z - quat.z * q.y,
		quat.w * q.y + quat.y * q.w + quat.z * q.x - quat.x * q.z,
		quat.w * q.z + quat.z * q.w + quat.x * q.y - quat.y * q.x) * (1.0f/norm);

	return outVec;
}

inline void Line2FromNorm(const glm::vec2& norm, const glm::vec2& point, glm::vec3& outLine)
{
	//��������� ����������� ������ ����� ������� � �����
	//(N,X) + D = 0
	//�������
	outLine.x = norm.x;
	outLine.y = norm.y;
	outLine.z = -Vec2Dot(norm, point);
}

inline glm::vec3 Line2FromNorm(const glm::vec2& norm, const glm::vec2& point)
{
	glm::vec3 outVec;
	Line2FromNorm(norm, point, outVec);
	return outVec;
}

inline void Line2FromDir(const glm::vec2& dir, const glm::vec2& point, glm::vec3& outLine)
{
	glm::vec2 norm;
	Vec2NormCW(dir, norm);
	Line2FromNorm(norm, point, outLine);
}

inline glm::vec3 Line2FromDir(const glm::vec2& dir, const glm::vec2& point)
{
	glm::vec3 outVec;
	Line2FromDir(dir, point, outVec);
	return outVec;
}

inline void Line2GetNorm(const glm::vec3& line, glm::vec2& outNorm)
{
	outNorm.x = line.x;
	outNorm.y = line.y;
}

inline void Line2GetDir(const glm::vec3& line, glm::vec2& outDir)
{
	outDir.x = line.x;
	outDir.y = line.y;
	Vec2NormCCW(outDir, outDir);
}

inline void Line2GetRadiusVec(const glm::vec3& line, glm::vec2& outVec)
{
	outVec.x = -line.z * line.x;
	outVec.y = -line.z * line.y;
}

inline glm::vec2 Line2GetRadiusVec(const glm::vec3& line)
{
	glm::vec2 outVec;
	Line2GetRadiusVec(line, outVec);
	return outVec;
}

inline glm::vec2 Line2GetNorm(const glm::vec3& line)
{
	glm::vec2 outVec;
	Line2GetNorm(line, outVec);
	return outVec;
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
	glm::vec2 outNormVec;
	Line2NormVecToPoint(line, point, outNormVec);
	return outNormVec;
}

inline Vec3Range operator*(const Vec3Range& val1, float val2)
{
	Vec3Range outVR = val1;
	outVR *= val2;

	return outVR;
}

inline Vec3Range operator*(const Vec3Range& val1, const glm::vec3& val2)
{
	Vec3Range outVR = val1;
	outVR *= val2;

	return outVR;
}

//ray = rayPos + t * rayVec
//return t
inline bool RayCastIntersectSphere(const glm::vec3& rayPos, const glm::vec3& rayVec, const glm::vec3& spherePos, float sphereRadius, float* t)
{
	glm::vec3 v = rayPos - spherePos;

	float b = 2.0f * glm::dot(rayVec, v);
	float c = glm::dot(v, v) - sphereRadius * sphereRadius;

	// ������� ������������
	float discriminant = (b * b) - (4.0f * c);
	// ��������� �� ������ �����
	if(discriminant < 0.0f)
		return false;

	discriminant = sqrtf(discriminant);

	float s0 = (-b + discriminant) / 2.0f;
	float s1 = (-b - discriminant) / 2.0f;

	float tRay = std::max(s0, s1);
	if (t)
		*t = tRay;

	// ���� ���� ������� > 0, ��� ���������� �����
	return tRay > 0;
}

inline glm::vec4 PlaneNormalize(const glm::vec4 &plane)
{
	glm::vec4 outVec;
	float norm = sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
	if (norm)
	{
		outVec.x = plane.x / norm;
		outVec.y = plane.y / norm;
		outVec.z = plane.z / norm;
		outVec.w = plane.w / norm;
	}
	else
	{
		outVec.x = 0.0f;
		outVec.y = 0.0f;
		outVec.z = 0.0f;
		outVec.w = 0.0f;
	}
	return outVec;
}

inline D3DMATRIX MatrixReflect(const glm::vec4 &plane)
{
	glm::vec4 normPlane = PlaneNormalize(plane);
	D3DMATRIX outMat;
	outMat.m[0][0] = 1.0f - 2.0f * normPlane.x * normPlane.x;
	outMat.m[0][1] = -2.0f * normPlane.x * normPlane.y;
	outMat.m[0][2] = -2.0f * normPlane.x * normPlane.z;
	outMat.m[0][3] = 0.0f;
	outMat.m[1][0] = -2.0f * normPlane.x * normPlane.y;
	outMat.m[1][1] = 1.0f - 2.0f * normPlane.y * normPlane.y;
	outMat.m[1][2] = -2.0f * normPlane.y * normPlane.z;
	outMat.m[1][3] = 0.0f;
	outMat.m[2][0] = -2.0f * normPlane.z * normPlane.x;
	outMat.m[2][1] = -2.0f * normPlane.z * normPlane.y;
	outMat.m[2][2] = 1.0f - 2.0f * normPlane.z * normPlane.z;
	outMat.m[2][3] = 0.0f;
	outMat.m[3][0] = -2.0f * normPlane.w * normPlane.x;
	outMat.m[3][1] = -2.0f * normPlane.w * normPlane.y;
	outMat.m[3][2] = -2.0f * normPlane.w * normPlane.z;
	outMat.m[3][3] = 1.0f;
	return outMat;
}

inline glm::vec4 PlaneTransform(const glm::vec4 &plane, const D3DMATRIX &mat)
{
	glm::vec4 outVec;
	outVec.x = mat.m[0][0] * plane.x + mat.m[1][0] * plane.y + mat.m[2][0] * plane.z + mat.m[3][0] * plane.w;
	outVec.y = mat.m[0][1] * plane.x + mat.m[1][1] * plane.y + mat.m[2][1] * plane.z + mat.m[3][1] * plane.w;
	outVec.z = mat.m[0][2] * plane.x + mat.m[1][2] * plane.y + mat.m[2][2] * plane.z + mat.m[3][2] * plane.w;
	outVec.w = mat.m[0][3] * plane.x + mat.m[1][3] * plane.y + mat.m[2][3] * plane.z + mat.m[3][3] * plane.w;
	return outVec;
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