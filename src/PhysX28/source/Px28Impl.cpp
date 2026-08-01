/*
 * Shared plumbing: the transform conversions and the Unimplemented trap.
 */

#include "Px28Impl.h"

#include <cstdio>
#include <cstdlib>

namespace px28
{

/*
 * NxMat34 is a rotation matrix plus a translation, and so is btTransform.
 * NxMat33 stores by column (getColumn/setColumn is its natural accessor) and
 * btMatrix3x3 is constructed row by row, so the transpose here is the whole
 * substance of the conversion -- getting it backwards mirrors every rotation
 * in the game and is exactly the class of error the "no conventions move"
 * rule exists to prevent.
 */
btTransform ToBullet(const NxMat34& m)
	{
	NxVec3 c0, c1, c2;
	m.M.getColumn(0, c0);
	m.M.getColumn(1, c1);
	m.M.getColumn(2, c2);

	/* btMatrix3x3's constructor takes elements in row-major order. */
	const btMatrix3x3 basis(c0.x, c1.x, c2.x,
	                        c0.y, c1.y, c2.y,
	                        c0.z, c1.z, c2.z);

	return btTransform(basis, ToBullet(m.t));
	}

NxMat34 ToNx(const btTransform& t)
	{
	const btMatrix3x3& basis = t.getBasis();

	NxMat34 out;
	out.M.setColumn(0, NxVec3(basis[0][0], basis[1][0], basis[2][0]));
	out.M.setColumn(1, NxVec3(basis[0][1], basis[1][1], basis[2][1]));
	out.M.setColumn(2, NxVec3(basis[0][2], basis[1][2], basis[2][2]));
	out.t = ToNx(t.getOrigin());

	return out;
	}

void Unimplemented(const char* what)
	{
	std::fprintf(stderr,
	             "\nPhysX28 shim: %s is not implemented yet.\n"
	             "\n"
	             "This is deliberate. Returning a default would let the game run\n"
	             "and behave subtly wrongly, with nothing to see and nothing in a\n"
	             "log -- which is the failure this shim exists to avoid.\n",
	             what);
	std::abort();
	}

} /* namespace px28 */
