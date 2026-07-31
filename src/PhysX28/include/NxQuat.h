#ifndef NX_QUAT_H
#define NX_QUAT_H

/*
 * PhysX 2.8's NxQuat.
 *
 * Transcribed from extern/physx/include/Foundation/NxQuat.h.
 *
 * The trap here is the angle unit, and it is not symmetric:
 *
 *   fromAngleAxis(angle, axis)      takes DEGREES.  The SDK computes
 *                                   degToRad(angle * 0.5) and its own comment
 *                                   reads "this used to be w/o deg to rad",
 *                                   so the conversion is deliberate and recent
 *                                   enough that someone was surprised by it.
 *
 *   fromAngleAxisFast(angle, axis)  takes RADIANS.
 *
 *   NxQuat(angle, axis)             delegates to fromAngleAxis, so DEGREES.
 *
 * The game uses both: DataBase.cpp builds wheel poses with fromAngleAxis(90,
 * ...) and GameCar.cpp steers with fromAngleAxisFast(radians, ...). Getting the
 * pair the same way round would rotate every wheel by 90 radians, which is
 * visible only once something renders.
 *
 * Storage order is x, y, z, w -- which is why setXYZW and setWXYZ both exist
 * and why the game, which stores quaternions as xyzw, uses the former.
 */

#include "NxSimpleTypes.h"
#include "NxVec3.h"

#include <cmath>

class NxMat33;

/* The SDK's angle helpers, in the spelling its own inline bodies use. */
namespace NxMath
{
	NX_INLINE NxReal degToRad(NxReal a) { return NxReal(a * 0.01745329251994329547); }
	NX_INLINE NxReal radToDeg(NxReal a) { return NxReal(a * 57.29577951308232286465); }
	NX_INLINE NxReal sqrt(NxReal a)     { return NxReal(std::sqrt(double(a))); }
	NX_INLINE NxReal sin(NxReal a)      { return NxReal(std::sin(double(a))); }
	NX_INLINE NxReal cos(NxReal a)      { return NxReal(std::cos(double(a))); }
	NX_INLINE void sinCos(NxReal a, NxReal &s, NxReal &c)
		{ s = NxReal(std::sin(double(a))); c = NxReal(std::cos(double(a))); }
}

class NxQuat
	{
	public:
	NX_INLINE NxQuat() {}
	NX_INLINE NxQuat(const NxQuat &q): x(q.x), y(q.y), z(q.z), w(q.w) {}
	NX_INLINE NxQuat(NxReal ix, NxReal iy, NxReal iz, NxReal iw): x(ix), y(iy), z(iz), w(iw) {}
	explicit NX_INLINE NxQuat(const NxVec3 &v, NxReal iw = 0): x(v.x), y(v.y), z(v.z), w(iw) {}
	/* Degrees, because it delegates to fromAngleAxis. */
	NX_INLINE NxQuat(const NxReal angle, const NxVec3 &axis) { fromAngleAxis(angle, axis); }
	NxQuat(const NxMat33 &m); /* defined in NxMat33.h, as 2.8 does */

	NX_INLINE NxQuat &operator=(const NxQuat &q)
		{ x = q.x; y = q.y; z = q.z; w = q.w; return *this; }

	NX_INLINE void zero() { x = y = z = NxReal(0); w = NxReal(1); }
	NX_INLINE void id()   { x = y = z = NxReal(0); w = NxReal(1); }

	NX_INLINE void setx(const NxReal &d) { x = d; }
	NX_INLINE void sety(const NxReal &d) { y = d; }
	NX_INLINE void setz(const NxReal &d) { z = d; }
	NX_INLINE void setw(const NxReal &d) { w = d; }

	NX_INLINE void setXYZW(NxReal ix, NxReal iy, NxReal iz, NxReal iw)
		{ x = ix; y = iy; z = iz; w = iw; }
	NX_INLINE void setXYZW(const NxReal *v) { x = v[0]; y = v[1]; z = v[2]; w = v[3]; }
	NX_INLINE void setWXYZ(NxReal iw, NxReal ix, NxReal iy, NxReal iz)
		{ w = iw; x = ix; y = iy; z = iz; }
	NX_INLINE void setWXYZ(const NxReal *v) { w = v[0]; x = v[1]; y = v[2]; z = v[3]; }

	NX_INLINE void getXYZW(NxF32 *d) const { d[0] = NxF32(x); d[1] = NxF32(y); d[2] = NxF32(z); d[3] = NxF32(w); }
	NX_INLINE void getXYZW(NxF64 *d) const { d[0] = NxF64(x); d[1] = NxF64(y); d[2] = NxF64(z); d[3] = NxF64(w); }
	NX_INLINE void getWXYZ(NxF32 *d) const { d[0] = NxF32(w); d[1] = NxF32(x); d[2] = NxF32(y); d[3] = NxF32(z); }
	NX_INLINE void getWXYZ(NxF64 *d) const { d[0] = NxF64(w); d[1] = NxF64(x); d[2] = NxF64(y); d[3] = NxF64(z); }

	/* ---- angle-axis: read the unit, twice ------------------------------ */

	/* DEGREES. */
	NX_INLINE void fromAngleAxis(NxReal angle, const NxVec3 &axis)
		{
		x = axis.x; y = axis.y; z = axis.z;
		const NxReal length = NxMath::sqrt(x * x + y * y + z * z);
		if (length != NxReal(0))
			{
			const NxReal inv = NxReal(1) / length;
			x *= inv; y *= inv; z *= inv;
			}
		const NxReal half = NxMath::degToRad(angle * NxReal(0.5));
		w = NxMath::cos(half);
		const NxReal s = NxMath::sin(half);
		x *= s; y *= s; z *= s;
		}

	/* RADIANS. */
	NX_INLINE void fromAngleAxisFast(NxReal angleRadians, const NxVec3 &axis)
		{
		NxReal s;
		NxMath::sinCos(angleRadians * NxReal(0.5), s, w);
		x = axis.x * s; y = axis.y * s; z = axis.z * s;
		}

	/* Returns DEGREES, matching fromAngleAxis. */
	NX_INLINE void getAngleAxis(NxReal &angle, NxVec3 &axis) const
		{
		angle = NxMath::radToDeg(NxReal(std::acos(double(w))) * NxReal(2));
		NxReal s = NxMath::sqrt(NxReal(1) - w * w);
		if (s < NxReal(1e-6))
			{ axis.set(NxReal(0), NxReal(0), NxReal(0)); }
		else
			{ s = NxReal(1) / s; axis.set(x * s, y * s, z * s); }
		}

	NX_INLINE NxReal getAngle() const
		{ return NxMath::radToDeg(NxReal(std::acos(double(w))) * NxReal(2)); }
	NX_INLINE NxReal getAngle(const NxQuat &q) const
		{ return NxMath::radToDeg(NxReal(std::acos(double(dot(q)))) * NxReal(2)); }

	/* ---- queries and in-place operations ------------------------------- */

	NX_INLINE NxReal dot(const NxQuat &o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
	NX_INLINE NxReal magnitudeSquared() const   { return x * x + y * y + z * z + w * w; }

	NX_INLINE void normalize()
		{
		const NxReal m = NxMath::sqrt(magnitudeSquared());
		if (m != NxReal(0))
			{ const NxReal inv = NxReal(1) / m; x *= inv; y *= inv; z *= inv; w *= inv; }
		}

	NX_INLINE void conjugate() { x = -x; y = -y; z = -z; }
	NX_INLINE void negate()    { x = -x; y = -y; z = -z; w = -w; }

	/* Inverse of a unit quaternion: the conjugate. 2.8 does not renormalise. */
	NX_INLINE void invert() { conjugate(); }

	NX_INLINE bool isFinite() const
		{
		return std::isfinite(double(x)) && std::isfinite(double(y))
		    && std::isfinite(double(z)) && std::isfinite(double(w));
		}
	NX_INLINE bool isIdentityRotation() const
		{ return x == NxReal(0) && y == NxReal(0) && z == NxReal(0) && (w == NxReal(1) || w == NxReal(-1)); }

	/* ---- rotation ------------------------------------------------------ */

	NX_INLINE const NxVec3 rot(const NxVec3 &v) const
		{
		const NxVec3 qv(x, y, z);
		const NxVec3 t = (qv ^ v) * NxReal(2);
		return v + t * w + (qv ^ t);
		}

	NX_INLINE const NxVec3 invRot(const NxVec3 &v) const
		{
		const NxVec3 qv(-x, -y, -z);
		const NxVec3 t = (qv ^ v) * NxReal(2);
		return v + t * w + (qv ^ t);
		}

	NX_INLINE void rotate(NxVec3 &v) const        { v = rot(v); }
	NX_INLINE void inverseRotate(NxVec3 &v) const { v = invRot(v); }

	NX_INLINE const NxVec3 transform(const NxVec3 &v, const NxVec3 &p) const { return rot(v) + p; }
	NX_INLINE const NxVec3 invTransform(const NxVec3 &v, const NxVec3 &p) const { return invRot(v - p); }

	/* ---- composition --------------------------------------------------- */

	NX_INLINE void multiply(const NxQuat &a, const NxQuat &b)
		{
		const NxReal rx = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
		const NxReal ry = a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z;
		const NxReal rz = a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x;
		const NxReal rw = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
		x = rx; y = ry; z = rz; w = rw;
		}

	NX_INLINE void multiply(const NxQuat &a, const NxVec3 &v)
		{
		const NxReal rx =  a.w * v.x + a.y * v.z - a.z * v.y;
		const NxReal ry =  a.w * v.y + a.z * v.x - a.x * v.z;
		const NxReal rz =  a.w * v.z + a.x * v.y - a.y * v.x;
		const NxReal rw = -a.x * v.x - a.y * v.y - a.z * v.z;
		x = rx; y = ry; z = rz; w = rw;
		}

	NX_INLINE void slerp(const NxReal t, const NxQuat &a, const NxQuat &b)
		{
		NxReal cosine = a.dot(b);
		NxQuat end = b;
		if (cosine < NxReal(0)) { cosine = -cosine; end.negate(); }

		NxReal sa = NxReal(1) - t, sb = t;
		if (cosine < NxReal(0.999))
			{
			const NxReal theta = NxReal(std::acos(double(cosine)));
			const NxReal inv = NxReal(1) / NxMath::sin(theta);
			sa = NxMath::sin(theta * (NxReal(1) - t)) * inv;
			sb = NxMath::sin(theta * t) * inv;
			}
		x = a.x * sa + end.x * sb;
		y = a.y * sa + end.y * sb;
		z = a.z * sa + end.z * sb;
		w = a.w * sa + end.w * sb;
		}

	NX_INLINE NxQuat operator*(const NxQuat &q) const { NxQuat r; r.multiply(*this, q); return r; }
	NX_INLINE NxQuat operator-() const { NxQuat r(*this); r.conjugate(); return r; }
	NX_INLINE NxQuat operator!() const { NxQuat r(*this); r.conjugate(); return r; }
	NX_INLINE NxQuat operator-(const NxQuat &q) const { return NxQuat(x - q.x, y - q.y, z - q.z, w - q.w); }

	NX_INLINE NxQuat &operator*=(const NxQuat &q) { multiply(NxQuat(*this), q); return *this; }
	NX_INLINE NxQuat &operator*=(const NxReal s)  { x *= s; y *= s; z *= s; w *= s; return *this; }
	NX_INLINE NxQuat &operator+=(const NxQuat &q) { x += q.x; y += q.y; z += q.z; w += q.w; return *this; }
	NX_INLINE NxQuat &operator-=(const NxQuat &q) { x -= q.x; y -= q.y; z -= q.z; w -= q.w; return *this; }

	NxReal x, y, z, w;
	};

#endif /* NX_QUAT_H */
