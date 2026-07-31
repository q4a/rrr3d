#ifndef NX_MAT34_H
#define NX_MAT34_H

/*
 * PhysX 2.8's NxMat34 -- a rotation and a translation, as a rigid transform.
 *
 * Transcribed from extern/physx/include/Foundation/NxMat34.h.
 *
 * M and t are public members and the game reaches into both directly:
 * Actor::InitRootNxActor fills globalPose.M and globalPose.t, and
 * Shape::AssignFromDesc reads localPose.t straight into a D3DXVECTOR3. So the
 * names and the order are part of the contract, not an implementation detail.
 *
 * Two things that read as details and are not:
 *
 *   The constructor takes a bool that defaults to TRUE, and true means
 *   *initialise to identity*. NxMat34() is therefore the identity, not
 *   uninitialised memory -- the opposite of what the equivalent constructor
 *   does in most matrix libraries, and the game relies on it.
 *
 *   Composition is [aR at] * [bR bt] = [aR*bR, aR*bt + at], and 2.8 computes t
 *   before M specifically so the destination may alias either operand. Doing it
 *   the other way round works until someone writes a.multiply(a, b).
 */

#include "NxSimpleTypes.h"
#include "NxVec3.h"
#include "NxMat33.h"

class NxMat34
	{
	public:
	/* init defaults to true, and true means identity. */
	explicit NX_INLINE NxMat34(bool init = true)
		{
		if (init)
			{
			t.zero();
			M.id();
			}
		}

	NX_INLINE NxMat34(const NxMat33 &rot, const NxVec3 &trans): M(rot), t(trans) {}

	NX_INLINE void id() { M.id(); t.zero(); }
	NX_INLINE void zero() { M.zero(); t.zero(); }

	NX_INLINE void setRowMajor44(const NxF32 *d)
		{
		/* Stride 4, not 3 -- the rotation sits in the top-left of a 4x4. */
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				M.data.m[r][c] = NxReal(d[r * 4 + c]);
		t.set(NxReal(d[3]), NxReal(d[7]), NxReal(d[11]));
		}
	NX_INLINE void getRowMajor44(NxF32 *d) const
		{
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				d[r * 4 + c] = NxF32(M.data.m[r][c]);
		d[3] = NxF32(t.x); d[7] = NxF32(t.y); d[11] = NxF32(t.z);
		d[12] = d[13] = d[14] = 0.0f; d[15] = 1.0f;
		}
	NX_INLINE void setColumnMajor44(const NxF32 *d)
		{
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				M.data.m[r][c] = NxReal(d[c * 4 + r]);
		t.set(d[12], d[13], d[14]);
		}
	NX_INLINE void getColumnMajor44(NxF32 *d) const
		{
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				d[c * 4 + r] = NxF32(M.data.m[r][c]);
		d[12] = NxF32(t.x); d[13] = NxF32(t.y); d[14] = NxF32(t.z);
		d[3] = d[7] = d[11] = 0.0f; d[15] = 1.0f;
		}

	/* dst = M * src + t */
	NX_INLINE void multiply(const NxVec3 &src, NxVec3 &dst) const { dst = M * src + t; }

	/* The inverse of a rigid transform, without inverting the matrix:
	   dst = M^T * (src - t). */
	NX_INLINE void multiplyByInverseRT(const NxVec3 &src, NxVec3 &dst) const
		{
		const NxVec3 rel = src - t;
		M.multiplyByTranspose(rel, dst);
		}

	/* t is computed before M so this works when dest aliases either operand. */
	NX_INLINE void multiply(const NxMat34 &left, const NxMat34 &right)
		{
		t = left.M * right.t + left.t;
		M.multiply(left.M, right.M);
		}

	NX_INLINE void multiplyInverseRTLeft(const NxMat34 &left, const NxMat34 &right)
		{
		NxVec3 rel = right.t - left.t;
		left.M.multiplyByTranspose(rel, t);
		NxMat33 leftT;
		leftT.setTransposed(left.M);
		M.multiply(leftT, right.M);
		}

	NX_INLINE void multiplyInverseRTRight(const NxMat34 &left, const NxMat34 &right)
		{
		NxMat33 rightT;
		rightT.setTransposed(right.M);
		M.multiply(left.M, rightT);
		NxVec3 rotated;
		M.multiply(right.t, rotated);
		t = left.t - rotated;
		}

	NX_INLINE bool getInverse(NxMat34 &dest) const
		{
		NxMat33 inv;
		if (!M.getInverse(inv)) return false;
		dest.M = inv;
		NxVec3 negT;
		inv.multiply(t, negT);
		dest.t = -negT;
		return true;
		}

	/* Cheaper inverse valid only for a rotation plus translation. */
	NX_INLINE bool getInverseRT(NxMat34 &dest) const
		{
		dest.M.setTransposed(M);
		NxVec3 rotated;
		dest.M.multiply(t, rotated);
		dest.t = -rotated;
		return true;
		}

	NX_INLINE bool isFinite() const { return M.isFinite() && t.isFinite(); }
	NX_INLINE bool isIdentity() const
		{ return M.isIdentity() && t.x == NxReal(0) && t.y == NxReal(0) && t.z == NxReal(0); }

	NX_INLINE NxMat34 operator*(const NxMat34 &right) const
		{ NxMat34 dest(false); dest.multiply(*this, right); return dest; }
	NX_INLINE NxVec3 operator*(const NxVec3 &src) const
		{ NxVec3 dest; multiply(src, dest); return dest; }
	NX_INLINE NxVec3 operator%(const NxVec3 &src) const
		{ NxVec3 dest; multiplyByInverseRT(src, dest); return dest; }

	NxMat33 M;
	NxVec3 t;
	};

#endif /* NX_MAT34_H */
