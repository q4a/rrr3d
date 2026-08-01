#ifndef PX28_CONTACT_H
#define PX28_CONTACT_H

/*
 * The record NxConstContactStream actually points at.
 *
 * 2.8 types the stream `const NxU32*` and keeps its layout private to the SDK.
 * Nothing in the game does arithmetic on it -- it is constructed into an
 * NxContactStreamIterator and walked -- so the shim is free to point it at
 * whatever it likes, provided the iterator is the only thing that reads it.
 *
 * Lifetime: built during contact collection, immediately after the world steps,
 * and cleared at the start of the next collection. Every consumer reads it
 * synchronously inside onContactNotify, which is what 2.8 required too.
 */

#include "NxPhysics.h"

#include <vector>

namespace px28
{

struct ContactPointRecord
	{
	NxVec3 point;
	NxReal separation;
	NxReal normalForce;
	NxU32  featureIndex0;
	NxU32  featureIndex1;
	};

/* 2.8 groups points that share a normal into a patch. Bullet's manifolds are
   already per shape pair and its points share a normal, so a manifold maps to
   one patch -- but the level exists because the game walks it. */
struct ContactPatchRecord
	{
	NxVec3 normal;
	std::vector<ContactPointRecord> points;
	};

struct ContactPairRecord
	{
	NxShape* shapes[2];
	NxU16 shapeFlags;
	std::vector<ContactPatchRecord> patches;
	};

struct ContactStreamRecord
	{
	std::vector<ContactPairRecord> pairs;
	};

/* The two casts that bridge the opaque type, in one place so the
   reinterpret_cast does not spread. */
inline NxConstContactStream ToStream(const ContactStreamRecord* record)
	{
	return reinterpret_cast<NxConstContactStream>(record);
	}

inline const ContactStreamRecord* FromStream(NxConstContactStream stream)
	{
	return reinterpret_cast<const ContactStreamRecord*>(stream);
	}

} /* namespace px28 */

#endif /* PX28_CONTACT_H */
