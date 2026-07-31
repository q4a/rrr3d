#ifndef NX_MAT33_H
#define NX_MAT33_H

/*
 * PhysX 2.8's NxMat33.
 *
 * Transcribed from extern/physx/include/Foundation/NxMat33.h and Nx9F32.h.
 *
 * The convention, stated once because it is the thing that goes wrong:
 *
 *   Storage is row-major. _ij is row i, column j, and the union with m[3][3]
 *   indexes as m[row][col].
 *
 *   Multiplication is COLUMN-VECTOR. M * v computes
 *       x = _11*v.x + _12*v.y + _13*v.z
 *   which is v treated as a column on the right.
 *
 * D3DXMATRIX stores row-major too, so the memory layouts agree -- but D3DX
 * multiplies ROW-vector, v * M. The same nine floats therefore mean the
 * transpose of each other as rotations, and a basis built with setColumn here
 * is a basis built with rows there. GameCar::OnContactModify builds a friction
 * frame this way, which is why it matters.
 *
 * The three-vector constructor takes ROWS, not columns, despite setColumn being
 * the more used accessor.
 */

#include "NxSimpleTypes.h"
#include "NxVec3.h"
#include "NxQuat.h"

enum NxMatrixType
	{
	NX_ZERO_MATRIX,
	NX_IDENTITY_MATRIX
	};

/* The union 2.8 exposes: named elements over a [row][col] array. */
class Nx9Real
	{
	public:
	/* Declared outside the union: a type cannot be defined inside an anonymous
	   one, though 2.8's own header nests it. */
	struct S
		{
		NxReal _11, _12, _13;
		NxReal _21, _22, _23;
		NxReal _31, _32, _33;
		};

	union
		{
		S s;
		NxReal m[3][3];
		};
	};

typedef Nx9Real Mat33DataType;

class NxMat33
	{
	public:
	NX_INLINE NxMat33() {}
	NX_INLINE NxMat33(const NxMat33 &o) { data = o.data; }
	NX_INLINE NxMat33(NxMatrixType type) { if (type == NX_IDENTITY_MATRIX) id(); else zero(); }
	NX_INLINE NxMat33(const NxQuat &q) { fromQuat(q); }
	/* Rows, not columns. */
	NX_INLINE NxMat33(const NxVec3 &row0, const NxVec3 &row1, const NxVec3 &row2)
		{
		data.s._11 = row0.x; data.s._12 = row0.y; data.s._13 = row0.z;
		data.s._21 = row1.x; data.s._22 = row1.y; data.s._23 = row1.z;
		data.s._31 = row2.x; data.s._32 = row2.y; data.s._33 = row2.z;
		}
	NX_INLINE ~NxMat33() {}

	NX_INLINE const NxMat33 &operator=(const NxMat33 &o) { data = o.data; return *this; }

	NX_INLINE void id()
		{
		data.s._11 = NxReal(1); data.s._12 = NxReal(0); data.s._13 = NxReal(0);
		data.s._21 = NxReal(0); data.s._22 = NxReal(1); data.s._23 = NxReal(0);
		data.s._31 = NxReal(0); data.s._32 = NxReal(0); data.s._33 = NxReal(1);
		}
	NX_INLINE void zero()
		{
		data.s._11 = data.s._12 = data.s._13 = NxReal(0);
		data.s._21 = data.s._22 = data.s._23 = NxReal(0);
		data.s._31 = data.s._32 = data.s._33 = NxReal(0);
		}
	NX_INLINE void diagonal(const NxVec3 &v)
		{ zero(); data.s._11 = v.x; data.s._22 = v.y; data.s._33 = v.z; }

	/* The skew-symmetric cross-product matrix, as 2.8 names it. */
	NX_INLINE void star(const NxVec3 &v)
		{
		data.s._11 = NxReal(0); data.s._12 = -v.z;      data.s._13 = v.y;
		data.s._21 = v.z;       data.s._22 = NxReal(0); data.s._23 = -v.x;
		data.s._31 = -v.y;      data.s._32 = v.x;       data.s._33 = NxReal(0);
		}

	NX_INLINE NxReal &operator()(int row, int col)             { return data.m[row][col]; }
	NX_INLINE const NxReal &operator()(int row, int col) const { return data.m[row][col]; }

	/* A column is a vertical slice: rows 0..2 of one column index. */
	NX_INLINE NxVec3 getColumn(int col) const
		{ return NxVec3(data.m[0][col], data.m[1][col], data.m[2][col]); }
	NX_INLINE void getColumn(int col, NxVec3 &v) const { v = getColumn(col); }
	NX_INLINE void setColumn(int col, const NxVec3 &v)
		{ data.m[0][col] = v.x; data.m[1][col] = v.y; data.m[2][col] = v.z; }

	NX_INLINE NxVec3 getRow(int row) const
		{ return NxVec3(data.m[row][0], data.m[row][1], data.m[row][2]); }
	NX_INLINE void getRow(int row, NxVec3 &v) const { v = getRow(row); }
	NX_INLINE void setRow(int row, const NxVec3 &v)
		{ data.m[row][0] = v.x; data.m[row][1] = v.y; data.m[row][2] = v.z; }

	NX_INLINE void setRowMajor(const NxF32 *d)
		{ for (int i = 0; i < 9; ++i) data.m[i / 3][i % 3] = NxReal(d[i]); }
	NX_INLINE void getRowMajor(NxF32 *d) const
		{ for (int i = 0; i < 9; ++i) d[i] = NxF32(data.m[i / 3][i % 3]); }
	NX_INLINE void setColumnMajor(const NxF32 *d)
		{ for (int i = 0; i < 9; ++i) data.m[i % 3][i / 3] = NxReal(d[i]); }
	NX_INLINE void getColumnMajor(NxF32 *d) const
		{ for (int i = 0; i < 9; ++i) d[i] = NxF32(data.m[i % 3][i / 3]); }

	NX_INLINE void setTransposed()
		{
		NxReal t;
		t = data.s._12; data.s._12 = data.s._21; data.s._21 = t;
		t = data.s._13; data.s._13 = data.s._31; data.s._31 = t;
		t = data.s._23; data.s._23 = data.s._32; data.s._32 = t;
		}
	NX_INLINE void setTransposed(const NxMat33 &o) { *this = o; setTransposed(); }
	NX_INLINE void setNegative()
		{ for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) data.m[r][c] = -data.m[r][c]; }

	/* Column-vector: dst = M * src. Temporaries so src may alias dst. */
	NX_INLINE void multiply(const NxVec3 &src, NxVec3 &dst) const
		{
		const NxReal x = data.s._11 * src.x + data.s._12 * src.y + data.s._13 * src.z;
		const NxReal y = data.s._21 * src.x + data.s._22 * src.y + data.s._23 * src.z;
		const NxReal z = data.s._31 * src.x + data.s._32 * src.y + data.s._33 * src.z;
		dst.set(x, y, z);
		}

	NX_INLINE void multiplyByTranspose(const NxVec3 &src, NxVec3 &dst) const
		{
		const NxReal x = data.s._11 * src.x + data.s._21 * src.y + data.s._31 * src.z;
		const NxReal y = data.s._12 * src.x + data.s._22 * src.y + data.s._32 * src.z;
		const NxReal z = data.s._13 * src.x + data.s._23 * src.y + data.s._33 * src.z;
		dst.set(x, y, z);
		}

	NX_INLINE void multiply(const NxMat33 &left, const NxMat33 &right)
		{
		NxReal out[3][3];
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				out[r][c] = left.data.m[r][0] * right.data.m[0][c]
				          + left.data.m[r][1] * right.data.m[1][c]
				          + left.data.m[r][2] * right.data.m[2][c];
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				data.m[r][c] = out[r][c];
		}

	NX_INLINE void multiply(NxReal s, const NxMat33 &a)
		{ for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) data.m[r][c] = a.data.m[r][c] * s; }
	NX_INLINE void add(const NxMat33 &a, const NxMat33 &b)
		{ for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) data.m[r][c] = a.data.m[r][c] + b.data.m[r][c]; }
	NX_INLINE void subtract(const NxMat33 &a, const NxMat33 &b)
		{ for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) data.m[r][c] = a.data.m[r][c] - b.data.m[r][c]; }

	NX_INLINE NxReal determinant() const
		{
		return data.s._11 * (data.s._22 * data.s._33 - data.s._23 * data.s._32)
		     - data.s._12 * (data.s._21 * data.s._33 - data.s._23 * data.s._31)
		     + data.s._13 * (data.s._21 * data.s._32 - data.s._22 * data.s._31);
		}

	NX_INLINE bool getInverse(NxMat33 &dest) const
		{
		const NxReal det = determinant();
		if (det == NxReal(0)) return false;
		const NxReal inv = NxReal(1) / det;
		dest.data.s._11 =  (data.s._22 * data.s._33 - data.s._23 * data.s._32) * inv;
		dest.data.s._12 = -(data.s._12 * data.s._33 - data.s._13 * data.s._32) * inv;
		dest.data.s._13 =  (data.s._12 * data.s._23 - data.s._13 * data.s._22) * inv;
		dest.data.s._21 = -(data.s._21 * data.s._33 - data.s._23 * data.s._31) * inv;
		dest.data.s._22 =  (data.s._11 * data.s._33 - data.s._13 * data.s._31) * inv;
		dest.data.s._23 = -(data.s._11 * data.s._23 - data.s._13 * data.s._21) * inv;
		dest.data.s._31 =  (data.s._21 * data.s._32 - data.s._22 * data.s._31) * inv;
		dest.data.s._32 = -(data.s._11 * data.s._32 - data.s._12 * data.s._31) * inv;
		dest.data.s._33 =  (data.s._11 * data.s._22 - data.s._12 * data.s._21) * inv;
		return true;
		}

	NX_INLINE void fromQuat(const NxQuat &q)
		{
		const NxReal xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
		const NxReal xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
		const NxReal wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
		data.s._11 = NxReal(1) - NxReal(2) * (yy + zz);
		data.s._12 = NxReal(2) * (xy - wz);
		data.s._13 = NxReal(2) * (xz + wy);
		data.s._21 = NxReal(2) * (xy + wz);
		data.s._22 = NxReal(1) - NxReal(2) * (xx + zz);
		data.s._23 = NxReal(2) * (yz - wx);
		data.s._31 = NxReal(2) * (xz - wy);
		data.s._32 = NxReal(2) * (yz + wx);
		data.s._33 = NxReal(1) - NxReal(2) * (xx + yy);
		}

	NX_INLINE void toQuat(NxQuat &q) const
		{
		const NxReal trace = data.s._11 + data.s._22 + data.s._33;
		if (trace > NxReal(0))
			{
			NxReal s = NxMath::sqrt(trace + NxReal(1)) * NxReal(2);
			q.w = NxReal(0.25) * s;
			q.x = (data.s._32 - data.s._23) / s;
			q.y = (data.s._13 - data.s._31) / s;
			q.z = (data.s._21 - data.s._12) / s;
			}
		else if (data.s._11 > data.s._22 && data.s._11 > data.s._33)
			{
			NxReal s = NxMath::sqrt(NxReal(1) + data.s._11 - data.s._22 - data.s._33) * NxReal(2);
			q.w = (data.s._32 - data.s._23) / s;
			q.x = NxReal(0.25) * s;
			q.y = (data.s._12 + data.s._21) / s;
			q.z = (data.s._13 + data.s._31) / s;
			}
		else if (data.s._22 > data.s._33)
			{
			NxReal s = NxMath::sqrt(NxReal(1) + data.s._22 - data.s._11 - data.s._33) * NxReal(2);
			q.w = (data.s._13 - data.s._31) / s;
			q.x = (data.s._12 + data.s._21) / s;
			q.y = NxReal(0.25) * s;
			q.z = (data.s._23 + data.s._32) / s;
			}
		else
			{
			NxReal s = NxMath::sqrt(NxReal(1) + data.s._33 - data.s._11 - data.s._22) * NxReal(2);
			q.w = (data.s._21 - data.s._12) / s;
			q.x = (data.s._13 + data.s._31) / s;
			q.y = (data.s._23 + data.s._32) / s;
			q.z = NxReal(0.25) * s;
			}
		}

	NX_INLINE bool isFinite() const
		{
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				if (!std::isfinite(double(data.m[r][c]))) return false;
		return true;
		}
	NX_INLINE bool isZero() const
		{
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				if (data.m[r][c] != NxReal(0)) return false;
		return true;
		}
	NX_INLINE bool isIdentity() const
		{
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				if (data.m[r][c] != (r == c ? NxReal(1) : NxReal(0))) return false;
		return true;
		}

	NX_INLINE NxVec3 operator*(const NxVec3 &src) const { NxVec3 d; multiply(src, d); return d; }
	NX_INLINE NxVec3 operator%(const NxVec3 &src) const { NxVec3 d; multiplyByTranspose(src, d); return d; }
	NX_INLINE NxMat33 operator*(const NxMat33 &o) const { NxMat33 d; d.multiply(*this, o); return d; }
	NX_INLINE NxMat33 operator*(float s) const { NxMat33 d; d.multiply(NxReal(s), *this); return d; }
	NX_INLINE NxMat33 operator+(const NxMat33 &o) const { NxMat33 d; d.add(*this, o); return d; }
	NX_INLINE NxMat33 operator-(const NxMat33 &o) const { NxMat33 d; d.subtract(*this, o); return d; }

	NX_INLINE NxMat33 &operator*=(const NxMat33 &o) { multiply(NxMat33(*this), o); return *this; }
	NX_INLINE const NxMat33 &operator*=(NxReal s)
		{ for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) data.m[r][c] *= s; return *this; }
	NX_INLINE const NxMat33 &operator-=(const NxMat33 &o) { subtract(*this, o); return *this; }

	Mat33DataType data;
	};

/* 2.8 declares this on NxQuat and defines it once NxMat33 is complete. */
NX_INLINE NxQuat::NxQuat(const NxMat33 &m) { m.toQuat(*this); }

#endif /* NX_MAT33_H */
