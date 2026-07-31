#ifndef NX_VEC3_H
#define NX_VEC3_H

/*
 * PhysX 2.8's NxVec3.
 *
 * Transcribed from extern/physx/include/Foundation/NxVec3.h. The *exact
 * overload set is load-bearing* and is not a place to tidy up, because every
 * piece of D3DX interop in the game goes through implicit conversions that
 * only exist if these signatures match:
 *
 *   - NxVec3(const NxReal v[]) is NOT explicit. That is what makes
 *     `NxVec3(someD3DXVECTOR3)` compile -- D3DXVECTOR3 converts to FLOAT*, and
 *     this constructor takes it from there. Marking it explicit silently breaks
 *     dozens of call sites.
 *
 *   - NxVec3(NxReal a) IS explicit, so a bare float does not become a vector by
 *     accident. Dropping the keyword changes overload resolution at sites like
 *     `setPlane(NxVec3(value), dist)`.
 *
 *   - get() exists twice, const and non-const, returning NxReal*. The game does
 *     `D3DXVECTOR3(pair.sumNormalForce.get())` on an rvalue and
 *     `desc.localPose.t.get(_pos)` on an lvalue; both spellings are used.
 *
 * Two methods read as if they return a value and do not: cross() and the
 * arithmetic helpers compute *into this*. Getting that wrong would compile and
 * silently produce zeroes.
 *
 * Layout is three floats, no base and no vtable, matching 2.8 -- the game
 * memcpys these in places and the saved-game format depends on it.
 */

#include "NxSimpleTypes.h"

#include <cmath>

class NxVec3;

/* The POD form 2.8 exposes alongside the class. */
typedef struct _Nx3F32
{
	NxReal x, y, z;
} Nx3F32;

class NxVec3
	{
	public:
	NX_INLINE NxVec3() {}
	explicit NX_INLINE NxVec3(NxReal a): x(a), y(a), z(a) {}
	NX_INLINE NxVec3(NxReal nx, NxReal ny, NxReal nz): x(nx), y(ny), z(nz) {}
	NX_INLINE NxVec3(const Nx3F32 &a): x(a.x), y(a.y), z(a.z) {}
	/* Deliberately not explicit -- see the note above. */
	NX_INLINE NxVec3(const NxReal v[]): x(v[0]), y(v[1]), z(v[2]) {}
	NX_INLINE NxVec3(const NxVec3 &v): x(v.x), y(v.y), z(v.z) {}

	NX_INLINE const NxVec3 &operator=(const NxVec3 &v)
		{ x = v.x; y = v.y; z = v.z; return *this; }
	NX_INLINE const NxVec3 &operator=(const Nx3F32 &v)
		{ x = v.x; y = v.y; z = v.z; return *this; }

	/* ---- accessors ---------------------------------------------------- */

	NX_INLINE NxReal *get()                 { return &x; }
	NX_INLINE const NxReal *get() const     { return &x; }
	NX_INLINE void get(NxF32 *dest) const   { dest[0] = NxF32(x); dest[1] = NxF32(y); dest[2] = NxF32(z); }
	NX_INLINE void get(NxF64 *dest) const   { dest[0] = NxF64(x); dest[1] = NxF64(y); dest[2] = NxF64(z); }

	NX_INLINE NxReal operator[](int i) const { return (&x)[i]; }
	NX_INLINE NxReal &operator[](int i)      { return (&x)[i]; }

	NX_INLINE void setx(const NxReal &d) { x = d; }
	NX_INLINE void sety(const NxReal &d) { y = d; }
	NX_INLINE void setz(const NxReal &d) { z = d; }

	NX_INLINE void set(const NxF32 *v) { x = NxReal(v[0]); y = NxReal(v[1]); z = NxReal(v[2]); }
	NX_INLINE void set(const NxF64 *v) { x = NxReal(v[0]); y = NxReal(v[1]); z = NxReal(v[2]); }
	NX_INLINE void set(NxReal nx, NxReal ny, NxReal nz) { x = nx; y = ny; z = nz; }
	NX_INLINE void set(const NxVec3 &v) { x = v.x; y = v.y; z = v.z; }
	NX_INLINE void set(NxReal a) { x = a; y = a; z = a; }

	NX_INLINE void zero() { x = y = z = NxReal(0); }
	NX_INLINE void setPlusInfinity()  { x = y = z = NX_MAX_REAL; }
	NX_INLINE void setMinusInfinity() { x = y = z = NX_MIN_REAL; }

	/* ---- computed into *this*, not returned --------------------------- */

	NX_INLINE void setNegative() { x = -x; y = -y; z = -z; }
	NX_INLINE void setNegative(const NxVec3 &a) { x = -a.x; y = -a.y; z = -a.z; }

	NX_INLINE void add(const NxVec3 &a, const NxVec3 &b)
		{ x = a.x + b.x; y = a.y + b.y; z = a.z + b.z; }
	NX_INLINE void subtract(const NxVec3 &a, const NxVec3 &b)
		{ x = a.x - b.x; y = a.y - b.y; z = a.z - b.z; }
	NX_INLINE void multiply(NxReal s, const NxVec3 &a)
		{ x = a.x * s; y = a.y * s; z = a.z * s; }
	NX_INLINE void arrayMultiply(const NxVec3 &a, const NxVec3 &b)
		{ x = a.x * b.x; y = a.y * b.y; z = a.z * b.z; }
	NX_INLINE void multiplyAdd(NxReal s, const NxVec3 &a, const NxVec3 &b)
		{ x = s * a.x + b.x; y = s * a.y + b.y; z = s * a.z + b.z; }

	/* Two overloads, and they do opposite things -- which is exactly why the
	   two-argument one is easy to mistake for the whole story.
	 *
	 * The two-argument form computes left x right *into* this vector and
	 * returns nothing. The one-argument form returns this x v and leaves this
	 * alone. Both are 2.8's; GameCar.cpp:925 and :931 use the returning one
	 * ("NxVec3 firstFric = secFric.cross(triNorm);") and would not compile
	 * without it, which is what settles that it exists. */
	NX_INLINE void cross(const NxVec3 &left, const NxVec3 &right)
		{
		const NxReal cx = left.y * right.z - left.z * right.y;
		const NxReal cy = left.z * right.x - left.x * right.z;
		const NxReal cz = left.x * right.y - left.y * right.x;
		x = cx; y = cy; z = cz;
		}

	NX_INLINE NxVec3 cross(const NxVec3 &v) const
		{
		return NxVec3(y * v.z - z * v.y,
		              z * v.x - x * v.z,
		              x * v.y - y * v.x);
		}

	NX_INLINE void min(const NxVec3 &v) { if (v.x < x) x = v.x; if (v.y < y) y = v.y; if (v.z < z) z = v.z; }
	NX_INLINE void max(const NxVec3 &v) { if (v.x > x) x = v.x; if (v.y > y) y = v.y; if (v.z > z) z = v.z; }

	/* ---- queries ------------------------------------------------------ */

	NX_INLINE NxReal dot(const NxVec3 &o) const { return x * o.x + y * o.y + z * o.z; }
	NX_INLINE NxReal magnitudeSquared() const   { return x * x + y * y + z * z; }
	NX_INLINE NxReal magnitude() const          { return NxReal(std::sqrt(double(magnitudeSquared()))); }
	NX_INLINE NxReal distanceSquared(const NxVec3 &v) const
		{ const NxReal dx = x - v.x, dy = y - v.y, dz = z - v.z; return dx * dx + dy * dy + dz * dz; }
	NX_INLINE NxReal distance(const NxVec3 &v) const
		{ return NxReal(std::sqrt(double(distanceSquared(v)))); }

	/* Returns the magnitude it had before normalising, as 2.8 does -- callers
	   use the return value to detect a degenerate vector. */
	NX_INLINE NxReal normalize()
		{
		const NxReal m = magnitude();
		if (m != NxReal(0))
			{
			const NxReal inv = NxReal(1) / m;
			x *= inv; y *= inv; z *= inv;
			}
		return m;
		}

	NX_INLINE void setMagnitude(NxReal length)
		{
		const NxReal m = magnitude();
		if (m != NxReal(0))
			{
			const NxReal s = length / m;
			x *= s; y *= s; z *= s;
			}
		}

	NX_INLINE bool isFinite() const
		{ return std::isfinite(double(x)) && std::isfinite(double(y)) && std::isfinite(double(z)); }

	NX_INLINE bool equals(const NxVec3 &v, NxReal epsilon) const
		{
		return std::fabs(double(x - v.x)) < double(epsilon)
		    && std::fabs(double(y - v.y)) < double(epsilon)
		    && std::fabs(double(z - v.z)) < double(epsilon);
		}

	NX_INLINE bool operator==(const NxVec3 &v) const { return x == v.x && y == v.y && z == v.z; }
	NX_INLINE bool operator!=(const NxVec3 &v) const { return x != v.x || y != v.y || z != v.z; }

	/* ---- operators ---------------------------------------------------- */

	NX_INLINE NxVec3 operator-() const { return NxVec3(-x, -y, -z); }
	NX_INLINE NxVec3 operator+(const NxVec3 &v) const { return NxVec3(x + v.x, y + v.y, z + v.z); }
	NX_INLINE NxVec3 operator-(const NxVec3 &v) const { return NxVec3(x - v.x, y - v.y, z - v.z); }
	NX_INLINE NxVec3 operator*(NxReal f) const { return NxVec3(x * f, y * f, z * f); }
	NX_INLINE NxVec3 operator/(NxReal f) const { const NxReal i = NxReal(1) / f; return NxVec3(x * i, y * i, z * i); }

	/* The cross product, which 2.8 spells with ^ as well as with cross(). */
	NX_INLINE NxVec3 operator^(const NxVec3 &v) const
		{ return NxVec3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x); }
	/* And the dot product, which it spells with |. */
	NX_INLINE NxReal operator|(const NxVec3 &v) const { return dot(v); }

	NX_INLINE const NxVec3 &operator+=(const NxVec3 &v) { x += v.x; y += v.y; z += v.z; return *this; }
	NX_INLINE const NxVec3 &operator-=(const NxVec3 &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
	NX_INLINE const NxVec3 &operator*=(NxReal f) { x *= f; y *= f; z *= f; return *this; }
	NX_INLINE const NxVec3 &operator/=(NxReal f) { const NxReal i = NxReal(1) / f; x *= i; y *= i; z *= i; return *this; }

	NxReal x, y, z;
	};

NX_INLINE NxVec3 operator*(NxReal f, const NxVec3 &v) { return NxVec3(v.x * f, v.y * f, v.z * f); }

#endif /* NX_VEC3_H */
