#ifndef NX_MATERIAL_DESC_H
#define NX_MATERIAL_DESC_H

/*
 * PhysX 2.8's NxMaterialDesc.
 *
 * Transcribed from extern/physx/include/Physics/NxMaterialDesc.h.
 *
 * This is the descriptor that decided the backend. Anisotropic friction --
 * dirOfAnisotropy with staticFrictionV and dynamicFrictionV, enabled by
 * NX_MF_ANISOTROPIC -- was removed outright in PhysX 3, and Jolt never had it.
 * Bullet's btCollisionObject::setAnisotropicFriction is the same idea in one
 * call, which is why the shim can carry these fields through rather than
 * document them as a permanent gap. See docs/physics-backend-trial.md.
 *
 * Both car materials in this game use it, and the reason is specific: it is
 * what stops a car sliding along a wall from climbing it.
 *
 * Note the material index is NOT stored here. NxScene::createMaterial hands one
 * out, sequentially from 1 with 0 reserved for the scene default, and db.xml
 * serialises the result -- <materialIndex>4</materialIndex> is the track and 5
 * is the border, because DataBase.cpp creates them fifth and sixth. The
 * allocation order is part of the contract even though the number never appears
 * in this file.
 */

#include "Nxp.h"
#include "NxSimpleTypes.h"
#include "NxVec3.h"

class NxMaterialDesc
	{
	public:
	NxReal dynamicFriction;
	NxReal staticFriction;
	NxReal restitution;
	/** The friction along dirOfAnisotropy, when NX_MF_ANISOTROPIC is set. */
	NxReal dynamicFrictionV;
	NxReal staticFrictionV;
	NxVec3 dirOfAnisotropy;
	NxU32  flags;
	NxCombineMode frictionCombineMode;
	NxCombineMode restitutionCombineMode;
	NxReal spring;

	NX_INLINE NxMaterialDesc() { setToDefault(); }

	NX_INLINE void setToDefault()
		{
		dynamicFriction  = 0.0f;
		staticFriction   = 0.0f;
		restitution      = 0.0f;
		dynamicFrictionV = 0.0f;
		staticFrictionV  = 0.0f;
		dirOfAnisotropy.set(1, 0, 0);
		flags = 0;
		frictionCombineMode    = NX_CM_AVERAGE;
		restitutionCombineMode = NX_CM_AVERAGE;
		spring = 0;
		}

	NX_INLINE bool isValid() const
		{
		if (dynamicFriction < 0.0f)                    return false;
		if (staticFriction < 0.0f)                     return false;
		if (restitution < 0.0f || restitution > 1.0f)  return false;
		if (!dirOfAnisotropy.isFinite())               return false;
		return true;
		}
	};

#endif /* NX_MATERIAL_DESC_H */
