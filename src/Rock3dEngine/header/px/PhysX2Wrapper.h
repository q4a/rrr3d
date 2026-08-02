#pragma once

namespace physx
{

enum MrBodyFlag
{
	// Set if gravity should be disabled for the body
	MR_BF_DISABLE_GRAVITY	= (1<<0),

	// Freezing for this body/actor
	MR_BF_FROZEN_POS_X		= (1<<1),
	MR_BF_FROZEN_POS_Y		= (1<<2),
	MR_BF_FROZEN_POS_Z		= (1<<3),
	MR_BF_FROZEN_ROT_X		= (1<<4),
	MR_BF_FROZEN_ROT_Y		= (1<<5),
	MR_BF_FROZEN_ROT_Z		= (1<<6),

	// Enable kinematic mode for this body/actor
	MR_BF_KINEMATIC			= (1<<7),

	// Enable debug renderer for this body
	MR_BF_VISUALIZATION		= (1<<8),
};

class MrBodyDesc
{
public:
	// Mass of body
	PxReal	mass;

	// Position and orientation of the center of mass
	PxVec3 massLocalPose; // call PxTransform(const PxVec3& position)

	// Combination of MrBodyFlag flags
	PxU32	flags;

	// Threshold for the energy-based sleeping algorithm
	PxReal	sleepEnergyThreshold;

	// Linear Velocity of the body
	PxVec3	linearVelocity;

	PX_INLINE MrBodyDesc();
	PX_INLINE void setToDefault();
};

PX_INLINE MrBodyDesc::MrBodyDesc() // constructor sets to default
{
	setToDefault();
}

PX_INLINE void MrBodyDesc::setToDefault()
{
	mass					= 0.0f;
	massLocalPose			= PxVec3(0.0f, 0.0f, 0.0f);
	flags					= MR_BF_VISUALIZATION;
	sleepEnergyThreshold	= -1.0f;
	linearVelocity			= PxVec3(0.0f, 0.0f, 0.0f);
}

}
