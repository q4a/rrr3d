#ifndef NX_SHAPE_DESC_H
#define NX_SHAPE_DESC_H

/*
 * PhysX 2.8's shape descriptors -- the base and all seven concrete forms.
 *
 * Transcribed from extern/physx/include/Physics/NxShapeDesc.h and the seven
 * Nx*ShapeDesc.h files beside it.
 *
 * One deliberate deviation: 2.8 puts each subclass in its own header. They are
 * together here because the game never includes any of them directly -- it
 * includes NxPhysics.h and nothing else -- so the split would buy nothing and
 * cost eight files. Every name, default and validity rule is unchanged.
 *
 * The rule with teeth is isValid(), and it is not a formality:
 *
 *   Actor::CreateNxShape uses !isValid() as its "the mesh has not loaded yet,
 *   defer this shape" signal. So NxTriangleMeshShapeDesc::isValid() MUST be
 *   false when meshData is null. Returning true there does not fail -- it
 *   creates every mesh shape in the game twice.
 *
 * isValid() is virtual and delegates to checkValid(), which returns a numbered
 * reason rather than a bool. The numbering is 2.8's, including its odd habit of
 * multiplying the base class's result -- zero stays zero, and any non-zero
 * reason survives scaled. Kept as-is so a failure reports what it reported
 * before.
 */

#include "Nxp.h"
#include "NxSimpleTypes.h"
#include "NxVec3.h"
#include "NxMat34.h"
#include "NxSpringDesc.h"
#include "NxTireFunctionDesc.h"

class NxTriangleMesh;
class NxConvexMesh;
class NxUserWheelContactModify;

class NxShapeDesc
	{
	protected:
	NX_INLINE NxShapeDesc(NxShapeType t): type(t) { setToDefault(); }

	public:
	virtual ~NxShapeDesc() {}

	NxShapeType			type;
	NxMat34				localPose;
	NxU32				shapeFlags;
	NxCollisionGroup	group;
	NxMaterialIndex		materialIndex;
	NxReal				density;
	NxReal				mass;
	/** -1 means "use the SDK's global skin width" -- which Manager::InitSDK
	    sets to 0.025.

	    Not every shape leaves it there, and the difference is shipped data:
	    bin/Debug/db.xml has 228 shapes at -1 and 69 at 0.1, four times the
	    global. So skin width cannot be implemented as one scene-wide value;
	    whatever reproduces 2.8's resting interpenetration has to do it per
	    shape. */
	NxReal				skinWidth;
	void*				userData;
	const char*			name;
	NxGroupsMask		groupsMask;
	NxU32				nonInteractingCompartmentTypes;

	NX_INLINE void setToDefault()
		{
		localPose.id();
		shapeFlags		= NX_SF_VISUALIZATION | NX_SF_CLOTH_TWOWAY | NX_SF_SOFTBODY_TWOWAY;
		group			= 0;
		materialIndex	= 0;
		skinWidth		= -1.0f;
		density			=  1.0f;
		mass			= -1.0f;   /* by default the mass follows from the density */
		userData		= NULL;
		name			= NULL;
		groupsMask.bits0 = 0;
		groupsMask.bits1 = 0;
		groupsMask.bits2 = 0;
		groupsMask.bits3 = 0;
		nonInteractingCompartmentTypes = 0;
		}

	NX_INLINE NxU32 checkValid() const
		{
		if (!localPose.isFinite())  return 1;
		if (group >= 32)            return 2;   /* only 32 groups are supported */
		if (type >= NX_SHAPE_COUNT) return 4;
		return 0;
		}

	NX_INLINE virtual bool isValid() const { return !checkValid(); }
	};

class NxPlaneShapeDesc : public NxShapeDesc
	{
	public:
	NxVec3 normal;
	NxReal d;

	NX_INLINE NxPlaneShapeDesc(): NxShapeDesc(NX_SHAPE_PLANE) { setToDefault(); }

	NX_INLINE void setToDefault()
		{
		NxShapeDesc::setToDefault();
		normal.set(0, 1, 0);
		d = 0.0f;
		}

	NX_INLINE NxU32 checkValid() const
		{
		if (!normal.isFinite())          return 1;
		if (!std::isfinite(double(d)))   return 2;
		return 3 * NxShapeDesc::checkValid();
		}
	NX_INLINE virtual bool isValid() const { return !checkValid(); }
	};

class NxSphereShapeDesc : public NxShapeDesc
	{
	public:
	NxReal radius;

	NX_INLINE NxSphereShapeDesc(): NxShapeDesc(NX_SHAPE_SPHERE) { setToDefault(); }

	NX_INLINE void setToDefault() { NxShapeDesc::setToDefault(); radius = 1.0f; }

	NX_INLINE NxU32 checkValid() const
		{
		if (!std::isfinite(double(radius))) return 1;
		if (radius <= 0.0f)                 return 2;
		return 3 * NxShapeDesc::checkValid();
		}
	NX_INLINE virtual bool isValid() const { return !checkValid(); }
	};

class NxBoxShapeDesc : public NxShapeDesc
	{
	public:
	/** Half-extents, not full. */
	NxVec3 dimensions;

	NX_INLINE NxBoxShapeDesc(): NxShapeDesc(NX_SHAPE_BOX) { setToDefault(); }

	NX_INLINE void setToDefault() { NxShapeDesc::setToDefault(); dimensions.set(0.5f, 0.5f, 0.5f); }

	NX_INLINE NxU32 checkValid() const
		{
		if (!dimensions.isFinite()) return 1;
		if (dimensions.x < 0.0f)    return 2;
		if (dimensions.y < 0.0f)    return 3;
		if (dimensions.z < 0.0f)    return 4;
		return 5 * NxShapeDesc::checkValid();
		}
	NX_INLINE virtual bool isValid() const { return !checkValid(); }
	};

class NxCapsuleShapeDesc : public NxShapeDesc
	{
	public:
	NxReal radius;
	/** The FULL length of the cylindrical section, between the cap centres --
	    not the half-height most libraries take. The capsule runs along Y. */
	NxReal height;
	NxU32  flags;

	NX_INLINE NxCapsuleShapeDesc(): NxShapeDesc(NX_SHAPE_CAPSULE) { setToDefault(); }

	NX_INLINE void setToDefault()
		{
		NxShapeDesc::setToDefault();
		radius = 0.0f;
		height = 0.0f;
		flags = 0;
		}

	NX_INLINE NxU32 checkValid() const
		{
		if (!std::isfinite(double(radius))) return 1;
		if (radius <= 0.0f)                 return 2;
		if (!std::isfinite(double(height))) return 3;
		if (height <= 0.0f)                 return 4;
		return 5 * NxShapeDesc::checkValid();
		}
	NX_INLINE virtual bool isValid() const { return !checkValid(); }
	};

class NxConvexShapeDesc : public NxShapeDesc
	{
	public:
	NxConvexMesh* meshData;
	NxU32 meshFlags;

	NX_INLINE NxConvexShapeDesc(): NxShapeDesc(NX_SHAPE_CONVEX) { setToDefault(); }

	NX_INLINE void setToDefault()
		{
		NxShapeDesc::setToDefault();
		meshData = NULL;
		meshFlags = 0;
		}

	/* Null mesh is reason 1 -- the "not loaded yet" signal. */
	NX_INLINE NxU32 checkValid() const
		{
		if (!meshData) return 1;
		return 3 * NxShapeDesc::checkValid();
		}
	NX_INLINE virtual bool isValid() const { return !checkValid(); }
	};

class NxTriangleMeshShapeDesc : public NxShapeDesc
	{
	public:
	NxTriangleMesh* meshData;
	NxU32 meshFlags;

	NX_INLINE NxTriangleMeshShapeDesc(): NxShapeDesc(NX_SHAPE_MESH) { setToDefault(); }

	NX_INLINE void setToDefault()
		{
		NxShapeDesc::setToDefault();
		meshData = NULL;
		meshFlags = 0;
		}

	/* Null mesh is reason 1. Actor::CreateNxShape depends on this being
	   invalid rather than merely empty. */
	NX_INLINE NxU32 checkValid() const
		{
		if (!meshData) return 1;
		return 3 * NxShapeDesc::checkValid();
		}
	NX_INLINE virtual bool isValid() const { return !checkValid(); }
	};

class NxWheelShapeDesc : public NxShapeDesc
	{
	public:
	NxReal radius;
	NxReal suspensionTravel;
	NxSpringDesc suspension;
	NxTireFunctionDesc longitudalTireForceFunction;   /* 2.8's spelling */
	NxTireFunctionDesc lateralTireForceFunction;
	NxReal inverseWheelMass;
	NxU32  wheelFlags;
	NxReal motorTorque;
	NxReal brakeTorque;
	NxReal steerAngle;
	NxUserWheelContactModify* wheelContactModify;

	NX_INLINE NxWheelShapeDesc(): NxShapeDesc(NX_SHAPE_WHEEL) { setToDefault(true); }

	/*
	 * fromCtor is 2.8's, and it is not cosmetic: when called from the
	 * constructor the nested spring and tire descriptors are left alone,
	 * because their own constructors have already run. Only an explicit
	 * setToDefault() resets them. WheelShape's constructor takes a
	 * default-constructed NxWheelShapeDesc, so this path is the one that
	 * decides the game's default wheel tuning.
	 */
	NX_INLINE void setToDefault(bool fromCtor = false)
		{
		NxShapeDesc::setToDefault();

		radius = 1.0f;
		suspensionTravel = 1.0f;
		inverseWheelMass = 1.0f;
		wheelFlags = 0;
		motorTorque = 0.0f;
		brakeTorque = 0.0f;
		steerAngle = 0.0f;
		wheelContactModify = NULL;

		if (!fromCtor)
			{
			suspension.setToDefault();
			longitudalTireForceFunction.setToDefault();
			lateralTireForceFunction.setToDefault();
			}
		}

	NX_INLINE NxU32 checkValid() const
		{
		if (!std::isfinite(double(radius)))            return 1;
		if (radius <= 0.0f)                            return 2;
		if (!std::isfinite(double(suspensionTravel)))  return 3;
		if (suspensionTravel < 0.0f)                   return 4;
		if (!suspension.isValid())                     return 5;
		if (longitudalTireForceFunction.isValid())     return 6;
		if (lateralTireForceFunction.isValid())        return 7;
		if (inverseWheelMass <= 0.0f)                  return 8;
		return 9 * NxShapeDesc::checkValid();
		}
	NX_INLINE virtual bool isValid() const { return !checkValid(); }
	};

#endif /* NX_SHAPE_DESC_H */
