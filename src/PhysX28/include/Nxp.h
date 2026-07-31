#ifndef NXP_H
#define NXP_H

/*
 * The PhysX 2.8 enumerations and constants -- the numeric truth of the shim.
 *
 * Every value here is transcribed from the 2.8 SDK headers under
 * extern/physx/include, with the source file recorded beside it. They are not
 * written from memory and they are not free to change, because **the numbers
 * are serialised into shipped game data**:
 *
 *   bin/Debug/db.xml carries <wheelFlags>64</wheelFlags>, which is
 *   NX_WF_CLAMPED_FRICTION; <flags>20</flags>, which is NX_AF_LOCK_COM |
 *   NX_AF_CONTACT_MODIFICATION; and <flags>2304</flags>, which is
 *   NX_BF_VISUALIZATION | NX_BF_ENERGY_SLEEP_TEST.
 *
 * Get one wrong and a track silently loses its friction tuning, or a car loses
 * its centre-of-mass lock, with no error anywhere. src/PhysX28/test asserts the
 * ones the shipped data depends on.
 *
 * Only the enumerators this game actually names are guaranteed; the rest are
 * carried so the numbering stays right, since most of these enums are
 * positional and dropping a member would shift everything after it.
 */

/* ------------------------------------------------- Physics/Nxp.h -------- */

enum NxShapeType
	{
	NX_SHAPE_PLANE			= 0,
	NX_SHAPE_SPHERE			= 1,
	NX_SHAPE_BOX			= 2,
	NX_SHAPE_CAPSULE		= 3,
	NX_SHAPE_WHEEL			= 4,
	NX_SHAPE_CONVEX			= 5,
	NX_SHAPE_MESH			= 6,
	NX_SHAPE_HEIGHTFIELD	= 7,
	NX_SHAPE_RAW_MESH		= 8,
	NX_SHAPE_COMPOUND		= 9,
	NX_SHAPE_COUNT			= 10,
	NX_SHAPE_FORCE_DWORD	= 0x7fffffff
	};

enum NxActorFlag
	{
	NX_AF_DISABLE_COLLISION			= (1<<0),
	NX_AF_DISABLE_RESPONSE			= (1<<1),
	NX_AF_LOCK_COM					= (1<<2),
	NX_AF_FLUID_DISABLE_COLLISION	= (1<<3),
	NX_AF_CONTACT_MODIFICATION		= (1<<4),
	NX_AF_FORCE_CONE_FRICTION		= (1<<5),
	NX_AF_USER_ACTOR_PAIR_FILTERING	= (1<<6)
	};

enum NxBodyFlag
	{
	NX_BF_DISABLE_GRAVITY	= (1<<0),
	NX_BF_FROZEN_POS_X		= (1<<1),
	NX_BF_FROZEN_POS_Y		= (1<<2),
	NX_BF_FROZEN_POS_Z		= (1<<3),
	NX_BF_FROZEN_ROT_X		= (1<<4),
	NX_BF_FROZEN_ROT_Y		= (1<<5),
	NX_BF_FROZEN_ROT_Z		= (1<<6),
	NX_BF_FROZEN_POS		= NX_BF_FROZEN_POS_X|NX_BF_FROZEN_POS_Y|NX_BF_FROZEN_POS_Z,
	NX_BF_FROZEN_ROT		= NX_BF_FROZEN_ROT_X|NX_BF_FROZEN_ROT_Y|NX_BF_FROZEN_ROT_Z,
	NX_BF_FROZEN			= NX_BF_FROZEN_POS|NX_BF_FROZEN_ROT,
	NX_BF_KINEMATIC			= (1<<7),
	NX_BF_VISUALIZATION		= (1<<8),
	NX_BF_DUMMY_0			= (1<<9),
	NX_BF_FILTER_SLEEP_VEL	= (1<<10),
	NX_BF_ENERGY_SLEEP_TEST	= (1<<11)
	};

/*
 * Positional, and the ordering matters: the game passes NX_ACCELERATION and
 * NX_SMOOTH_VELOCITY_CHANGE, which are 5 and 4 only because of what precedes
 * them.
 */
enum NxForceMode
	{
	NX_FORCE					= 0,
	NX_IMPULSE					= 1,
	NX_VELOCITY_CHANGE			= 2,
	NX_SMOOTH_IMPULSE			= 3,
	NX_SMOOTH_VELOCITY_CHANGE	= 4,
	NX_ACCELERATION				= 5
	};

/* Only the two the game sets; the numbering is the SDK's. */
enum NxParameter
	{
	NX_PENALTY_FORCE	= 0,
	NX_SKIN_WIDTH		= 1,
	NX_ADAPTIVE_FORCE	= 68,
	NX_PARAMS_NUM_VALUES = 103,
	NX_PARAMS_FORCE_DWORD = 0x7fffffff
	};

enum NxFilterOp
	{
	NX_FILTEROP_AND			= 0,
	NX_FILTEROP_OR			= 1,
	NX_FILTEROP_XOR			= 2,
	NX_FILTEROP_NAND		= 3,
	NX_FILTEROP_NOR			= 4,
	NX_FILTEROP_NXOR		= 5,
	NX_FILTEROP_SWAP_AND	= 6
	};

/* ---------------------------------------- Physics/NxMaterialDesc.h ------ */

enum NxCombineMode
	{
	NX_CM_AVERAGE	= 0,
	NX_CM_MIN		= 1,
	NX_CM_MULTIPLY	= 2,
	NX_CM_MAX		= 3,
	NX_CM_N_VALUES	= 4,
	NX_CM_PAD_32	= 0xffffffff
	};

enum NxMaterialFlag
	{
	NX_MF_ANISOTROPIC				= (1<<0),
	NX_MF_DISABLE_FRICTION			= (1<<4),
	NX_MF_DISABLE_STRONG_FRICTION	= (1<<5)
	};

/* ------------------------------------------- Physics/NxSceneDesc.h ------ */

enum NxTimeStepMethod
	{
	NX_TIMESTEP_FIXED		= 0,
	NX_TIMESTEP_VARIABLE	= 1,
	NX_TIMESTEP_INHERIT		= 2,
	NX_NUM_TIMESTEP_METHODS	= 3,
	NX_TSM_FORCE_DWORD		= 0x7fffffff
	};

/* ----------------------------------------------- Physics/NxScene.h ------ */

enum NxSimulationStatus
	{
	NX_RIGID_BODY_FINISHED	= (1<<0),
	NX_ALL_FINISHED			= (1<<0),	/* an alias; the SDK calls its own name a misnomer */
	NX_PRIMARY_FINISHED		= (1<<1)
	};

/* -------------------------------- Physics/NxUserRaycastReport.h --------- */

enum NxShapesType
	{
	NX_STATIC_SHAPES	= (1<<0),
	NX_DYNAMIC_SHAPES	= (1<<1),
	NX_ALL_SHAPES		= NX_STATIC_SHAPES|NX_DYNAMIC_SHAPES
	};

enum NxRaycastBit
	{
	NX_RAYCAST_SHAPE		= (1<<0),
	NX_RAYCAST_IMPACT		= (1<<1),
	NX_RAYCAST_NORMAL		= (1<<2),
	NX_RAYCAST_FACE_INDEX	= (1<<3),
	NX_RAYCAST_DISTANCE		= (1<<4),
	NX_RAYCAST_UV			= (1<<5),
	NX_RAYCAST_FACE_NORMAL	= (1<<6),
	NX_RAYCAST_MATERIAL		= (1<<7)
	};

/* -------------------------------- Physics/NxUserContactReport.h --------- */

enum NxContactPairFlag
	{
	NX_IGNORE_PAIR								= (1<<0),
	NX_NOTIFY_ON_START_TOUCH					= (1<<1),
	NX_NOTIFY_ON_END_TOUCH						= (1<<2),
	NX_NOTIFY_ON_TOUCH							= (1<<3),
	NX_NOTIFY_ON_IMPACT							= (1<<4),
	NX_NOTIFY_ON_ROLL							= (1<<5),
	NX_NOTIFY_ON_SLIDE							= (1<<6),
	NX_NOTIFY_FORCES							= (1<<7),
	NX_NOTIFY_ON_START_TOUCH_FORCE_THRESHOLD	= (1<<8),
	NX_NOTIFY_ON_END_TOUCH_FORCE_THRESHOLD		= (1<<9),
	NX_NOTIFY_ON_TOUCH_FORCE_THRESHOLD			= (1<<10),
	/* Deliberately NOT part of NX_NOTIFY_ALL -- the SDK excludes it for
	   performance, which is why GameCar asks for both by name. */
	NX_NOTIFY_CONTACT_MODIFICATION				= (1<<16),

	/* Ten flags, not the four an abbreviation would suggest: 0x7fe. */
	NX_NOTIFY_ALL = (NX_NOTIFY_ON_START_TOUCH|NX_NOTIFY_ON_END_TOUCH|NX_NOTIFY_ON_TOUCH|
					 NX_NOTIFY_ON_IMPACT|NX_NOTIFY_ON_ROLL|NX_NOTIFY_ON_SLIDE|NX_NOTIFY_FORCES|
					 NX_NOTIFY_ON_START_TOUCH_FORCE_THRESHOLD|NX_NOTIFY_ON_END_TOUCH_FORCE_THRESHOLD|
					 NX_NOTIFY_ON_TOUCH_FORCE_THRESHOLD)
	};

/* ----------------------------------------- Physics/NxWheelShapeDesc.h --- */

enum NxWheelFlags
	{
	NX_WF_WHEEL_AXIS_CONTACT_NORMAL	= 1 << 0,
	NX_WF_INPUT_LAT_SLIPVELOCITY	= 1 << 1,
	NX_WF_INPUT_LNG_SLIPVELOCITY	= 1 << 2,
	NX_WF_UNSCALED_SPRING_BEHAVIOR	= 1 << 3,
	NX_WF_AXLE_SPEED_OVERRIDE		= 1 << 4,
	NX_WF_EMULATE_LEGACY_WHEEL		= 1 << 5,
	NX_WF_CLAMPED_FRICTION			= 1 << 6
	};

#endif /* NXP_H */
