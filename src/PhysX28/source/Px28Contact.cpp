/*
 * NxContactStreamIterator, and the collection pass that builds what it walks.
 */

#include "Px28Impl.h"
#include "Px28Contact.h"

namespace
{

/* One before the beginning. 2.8's traversal is `while (goNextPair())`, so the
   first call has to land on the first element rather than past it. */
const NxU32 cBeforeFirst = 0xffffffffu;

const px28::ContactPairRecord* CurrentPair(NxConstContactStream stream, NxU32 pair)
	{
	const px28::ContactStreamRecord* record = px28::FromStream(stream);
	if (!record || pair == cBeforeFirst || pair >= record->pairs.size())
		return NULL;

	return &record->pairs[pair];
	}

const px28::ContactPatchRecord* CurrentPatch(NxConstContactStream stream,
                                             NxU32 pair, NxU32 patch)
	{
	const px28::ContactPairRecord* record = CurrentPair(stream, pair);
	if (!record || patch == cBeforeFirst || patch >= record->patches.size())
		return NULL;

	return &record->patches[patch];
	}

const px28::ContactPointRecord* CurrentPoint(NxConstContactStream stream,
                                             NxU32 pair, NxU32 patch, NxU32 point)
	{
	const px28::ContactPatchRecord* record = CurrentPatch(stream, pair, patch);
	if (!record || point == cBeforeFirst || point >= record->points.size())
		return NULL;

	return &record->points[point];
	}

/* Returned by reference from getPatchNormal/getPoint when the cursor is not on
   anything. 2.8 returns a reference, so there has to be something to refer to. */
const NxVec3 cZero(0.0f, 0.0f, 0.0f);

} /* namespace */

NxContactStreamIterator::NxContactStreamIterator(NxConstContactStream stream)
	: _stream(stream), _pair(cBeforeFirst), _patch(cBeforeFirst), _point(cBeforeFirst)
	{
	}

/*
 * Advancing a level resets the levels below it to "before first", which is what
 * makes the game's nested `while (goNextPair()) while (goNextPatch()) while
 * (goNextPoint())` walk every point of every patch of every pair.
 */
bool NxContactStreamIterator::goNextPair()
	{
	const px28::ContactStreamRecord* record = px28::FromStream(_stream);
	if (!record)
		return false;

	const NxU32 next = (_pair == cBeforeFirst) ? 0 : _pair + 1;
	if (next >= record->pairs.size())
		return false;

	_pair = next;
	_patch = cBeforeFirst;
	_point = cBeforeFirst;

	return true;
	}

bool NxContactStreamIterator::goNextPatch()
	{
	const px28::ContactPairRecord* record = CurrentPair(_stream, _pair);
	if (!record)
		return false;

	const NxU32 next = (_patch == cBeforeFirst) ? 0 : _patch + 1;
	if (next >= record->patches.size())
		return false;

	_patch = next;
	_point = cBeforeFirst;

	return true;
	}

bool NxContactStreamIterator::goNextPoint()
	{
	const px28::ContactPatchRecord* record = CurrentPatch(_stream, _pair, _patch);
	if (!record)
		return false;

	const NxU32 next = (_point == cBeforeFirst) ? 0 : _point + 1;
	if (next >= record->points.size())
		return false;

	_point = next;

	return true;
	}

NxU32 NxContactStreamIterator::getNumPairs()
	{
	const px28::ContactStreamRecord* record = px28::FromStream(_stream);
	return record ? static_cast<NxU32>(record->pairs.size()) : 0;
	}

NxShape* NxContactStreamIterator::getShape(NxU32 shapeIndex)
	{
	const px28::ContactPairRecord* record = CurrentPair(_stream, _pair);
	return record && shapeIndex < 2 ? record->shapes[shapeIndex] : NULL;
	}

bool NxContactStreamIterator::isDeletedShape(NxU32 shapeIndex)
	{
	return getShape(shapeIndex) == NULL;
	}

NxU16 NxContactStreamIterator::getShapeFlags()
	{
	const px28::ContactPairRecord* record = CurrentPair(_stream, _pair);
	return record ? record->shapeFlags : 0;
	}

NxU32 NxContactStreamIterator::getNumPatches()
	{
	const px28::ContactPairRecord* record = CurrentPair(_stream, _pair);
	return record ? static_cast<NxU32>(record->patches.size()) : 0;
	}

NxU32 NxContactStreamIterator::getNumPatchesRemaining()
	{
	const px28::ContactPairRecord* record = CurrentPair(_stream, _pair);
	if (!record)
		return 0;

	const NxU32 consumed = (_patch == cBeforeFirst) ? 0 : _patch + 1;
	return static_cast<NxU32>(record->patches.size()) - consumed;
	}

NxU32 NxContactStreamIterator::getNumPoints()
	{
	const px28::ContactPatchRecord* record = CurrentPatch(_stream, _pair, _patch);
	return record ? static_cast<NxU32>(record->points.size()) : 0;
	}

NxU32 NxContactStreamIterator::getNumPointsRemaining()
	{
	const px28::ContactPatchRecord* record = CurrentPatch(_stream, _pair, _patch);
	if (!record)
		return 0;

	const NxU32 consumed = (_point == cBeforeFirst) ? 0 : _point + 1;
	return static_cast<NxU32>(record->points.size()) - consumed;
	}

const NxVec3& NxContactStreamIterator::getPatchNormal()
	{
	const px28::ContactPatchRecord* record = CurrentPatch(_stream, _pair, _patch);
	return record ? record->normal : cZero;
	}

const NxVec3& NxContactStreamIterator::getPoint()
	{
	const px28::ContactPointRecord* record = CurrentPoint(_stream, _pair, _patch, _point);
	return record ? record->point : cZero;
	}

NxReal NxContactStreamIterator::getSeparation()
	{
	const px28::ContactPointRecord* record = CurrentPoint(_stream, _pair, _patch, _point);
	return record ? record->separation : 0.0f;
	}

NxReal NxContactStreamIterator::getPointNormalForce()
	{
	const px28::ContactPointRecord* record = CurrentPoint(_stream, _pair, _patch, _point);
	return record ? record->normalForce : 0.0f;
	}

NxU32 NxContactStreamIterator::getFeatureIndex0()
	{
	const px28::ContactPointRecord* record = CurrentPoint(_stream, _pair, _patch, _point);
	return record ? record->featureIndex0 : 0;
	}

NxU32 NxContactStreamIterator::getFeatureIndex1()
	{
	const px28::ContactPointRecord* record = CurrentPoint(_stream, _pair, _patch, _point);
	return record ? record->featureIndex1 : 0;
	}
