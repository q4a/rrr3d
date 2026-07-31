// NxMat33 and NxMat34 conventions.
//
// The one that costs the most if it is wrong is the multiplication convention:
// NxMat33 is column-vector (M * v), D3DXMATRIX is row-vector (v * M), and the
// two store the same nine floats. So the same memory means the transpose of
// itself depending on which library is reading, and a basis built with
// setColumn here is a basis built with rows there.

#include "NxMat34.h"

#include <cstdio>
#include <cmath>
#include <type_traits>

static int gFailures = 0;

static void Check(const char *what, bool ok)
{
	printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) ++gFailures;
}

static bool Near(NxReal a, NxReal b) { return std::fabs(double(a - b)) < 1e-4; }
static bool NearVec(const NxVec3 &v, NxReal x, NxReal y, NxReal z)
{ return Near(v.x, x) && Near(v.y, y) && Near(v.z, z); }

int RunMatTests()
{
	printf("NxMat33 / NxMat34 conventions\n\n");

	Check("NxMat33 is nine floats", sizeof(NxMat33) == 9 * sizeof(NxReal));
	Check("NxMat34 is a NxMat33 then a NxVec3",
	      sizeof(NxMat34) == sizeof(NxMat33) + sizeof(NxVec3));

	// _ij is row i, column j, and it aliases m[row][col].
	{
		NxMat33 m;
		m.zero();
		m.data.s._12 = 7;
		Check("_12 is row 0, column 1", m.data.m[0][1] == 7);
		m.data.m[2][0] = 9;
		Check("m[2][0] is _31", m.data.s._31 == 9);
	}

	// The three-vector constructor takes ROWS, even though setColumn is the
	// more used accessor.
	{
		const NxMat33 m(NxVec3(1, 2, 3), NxVec3(4, 5, 6), NxVec3(7, 8, 9));
		Check("the three-vector ctor takes rows", NearVec(m.getRow(0), 1, 2, 3));
		Check("...so column 0 is the first element of each row",
		      NearVec(m.getColumn(0), 1, 4, 7));
	}

	// Column-vector multiplication: M * v, not v * M.
	{
		NxMat33 m;
		m.zero();
		// A matrix that maps +X to +Y under column-vector convention has its
		// 1 at row 1, column 0.
		m.data.s._21 = 1; m.data.s._12 = 0; m.data.s._33 = 1;
		Check("M * v treats v as a column (maps +X to +Y)",
		      NearVec(m * NxVec3(1, 0, 0), 0, 1, 0));
	}

	// operator% is multiply-by-transpose, which is the inverse for a rotation.
	{
		NxQuat q;
		q.fromAngleAxis(37, NxVec3(0, 0, 1));
		NxMat33 m(q);
		const NxVec3 v(1, 2, 3);
		Check("operator% undoes operator* for a rotation",
		      NearVec(m % (m * v), 1, 2, 3));
	}

	// setColumn writes a vertical slice; getColumn reads it back.
	{
		NxMat33 m;
		m.zero();
		m.setColumn(1, NxVec3(4, 5, 6));
		Check("setColumn writes down a column",
		      m.data.m[0][1] == 4 && m.data.m[1][1] == 5 && m.data.m[2][1] == 6);
		Check("getColumn reads it back", NearVec(m.getColumn(1), 4, 5, 6));
	}

	// A rotation must survive the quaternion round trip both ways.
	{
		NxQuat q;
		q.fromAngleAxis(50, NxVec3(0, 1, 0));
		NxMat33 m(q);
		NxQuat back;
		m.toQuat(back);
		const NxVec3 v(1, 2, 3);
		Check("fromQuat/toQuat round-trips a rotation",
		      NearVec(back.rot(v), q.rot(v).x, q.rot(v).y, q.rot(v).z));
		Check("the matrix rotates the same way the quaternion does",
		      NearVec(m * v, q.rot(v).x, q.rot(v).y, q.rot(v).z));
	}

	{
		NxMat33 m(NxVec3(2, 0, 0), NxVec3(0, 4, 0), NxVec3(0, 0, 8));
		NxMat33 inv;
		Check("getInverse succeeds on a non-singular matrix", m.getInverse(inv));
		Check("...and inverts it", NearVec(inv * (m * NxVec3(1, 1, 1)), 1, 1, 1));

		NxMat33 singular;
		singular.zero();
		NxMat33 dummy;
		Check("getInverse fails on a singular matrix", !singular.getInverse(dummy));
	}

	// NxMat34's default constructor initialises to identity. Most matrix
	// libraries leave it uninitialised, and the game relies on the opposite.
	{
		NxMat34 m;
		Check("NxMat34() defaults to the IDENTITY, not to garbage", m.isIdentity());
		NxMat34 uninit(false);
		(void)uninit; // just has to compile -- the explicit false form exists
		Check("NxMat34(false) is available for the uninitialised case", true);
	}

	// dst = M * src + t
	{
		NxMat34 m;
		m.M.id();
		m.t.set(10, 20, 30);
		Check("NxMat34 applies rotation then translation",
		      NearVec(m * NxVec3(1, 2, 3), 11, 22, 33));
	}

	// Composition, and the aliasing 2.8 is careful about.
	{
		NxQuat q;
		q.fromAngleAxis(90, NxVec3(0, 0, 1));
		NxMat34 a, b;
		a.M.fromQuat(q); a.t.set(1, 0, 0);
		b.M.id();        b.t.set(0, 1, 0);

		const NxMat34 ab = a * b;
		const NxVec3 v(1, 1, 1);
		Check("composition applies the right transform first",
		      NearVec(ab * v, (a * (b * v)).x, (a * (b * v)).y, (a * (b * v)).z));

		NxMat34 self(a);
		self.multiply(self, b);
		Check("multiply() tolerates the destination aliasing an operand",
		      NearVec(self * v, (ab * v).x, (ab * v).y, (ab * v).z));
	}

	// getInverseRT is the cheap rigid inverse; % uses the same path.
	{
		NxQuat q;
		q.fromAngleAxis(25, NxVec3(1, 1, 0));
		NxMat34 m;
		m.M.fromQuat(q);
		m.t.set(3, -2, 5);
		const NxVec3 v(1, 2, 3);
		Check("operator% is the rigid inverse of operator*",
		      NearVec(m % (m * v), 1, 2, 3));

		NxMat34 inv(false);
		m.getInverseRT(inv);
		Check("getInverseRT agrees with operator%",
		      NearVec(inv * (m * v), 1, 2, 3));
	}

	printf("\n%s (%d failure%s)\n", gFailures ? "FAILED" : "all conventions hold",
	       gFailures, gFailures == 1 ? "" : "s");
	return gFailures;
}
