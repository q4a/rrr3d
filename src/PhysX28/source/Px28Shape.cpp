/*
 * NxShape and the convex primitives, over Bullet's collision shapes.
 */

#include "Px28Impl.h"

namespace px28
{

ShapeState::ShapeState(Actor& actor, const NxShapeDesc& desc, NxShapeType type)
	: _actor(&actor), _bulletShape(NULL), _localPose(ToBullet(desc.localPose)),
	  _group(desc.group), _groupsMask(desc.groupsMask),
	  _material(desc.materialIndex), _flags(desc.shapeFlags),
	  _skinWidth(desc.skinWidth), _type(type)
	{
	}

ShapeState::~ShapeState()
	{
	delete _bulletShape;
	}

Material::Material(const NxMaterialDesc& desc, NxMaterialIndex index)
	: _desc(desc), _index(index)
	{
	}

template<class NxInterface>
NxActor& ShapeImpl<NxInterface>::getActor() const
	{
	return *_actor;
	}

template<class NxInterface>
void ShapeImpl<NxInterface>::setLocalPose(const NxMat34& mat)
	{
	_localPose = ToBullet(mat);
	_actor->rebuildCompoundShape();
	}

template<class NxInterface>
void ShapeImpl<NxInterface>::setLocalPosition(const NxVec3& vec)
	{
	_localPose.setOrigin(ToBullet(vec));
	_actor->rebuildCompoundShape();
	}

template<class NxInterface>
void ShapeImpl<NxInterface>::setLocalOrientation(const NxMat33& mat)
	{
	NxMat34 pose;
	pose.M = mat;
	pose.t = ToNx(_localPose.getOrigin());

	_localPose = ToBullet(pose);
	_actor->rebuildCompoundShape();
	}

/* The instantiations that exist. Any other Nx*Shape needs adding here, and the
   linker says so rather than the code silently missing a case. */
template class ShapeImpl<NxBoxShape>;
template class ShapeImpl<NxSphereShape>;
template class ShapeImpl<NxCapsuleShape>;

/* --------------------------------------------------------------------- box */

/* 2.8's `dimensions` are half-extents, and so are btBoxShape's, so this is a
   straight pass-through. Getting it wrong doubles or halves every box in the
   game, which is the kind of error that looks like a physics problem. */
BoxShape::BoxShape(Actor& actor, const NxBoxShapeDesc& desc)
	: ShapeImpl<NxBoxShape>(actor, desc, NX_SHAPE_BOX)
	{
	_bulletShape = new btBoxShape(ToBullet(desc.dimensions));
	}

void BoxShape::setDimensions(const NxVec3& dimensions)
	{
	delete _bulletShape;
	_bulletShape = new btBoxShape(ToBullet(dimensions));
	_actor->rebuildCompoundShape();
	}

NxVec3 BoxShape::getDimensions() const
	{
	/* getHalfExtentsWithMargin, not getHalfExtentsWithoutMargin: Bullet stores
	   the box shrunk by its collision margin and adds the margin back at query
	   time, so the "without" variant would return something smaller than what
	   was passed in. */
	return ToNx(static_cast<const btBoxShape*>(_bulletShape)->getHalfExtentsWithMargin());
	}

void BoxShape::saveToDesc(NxBoxShapeDesc& desc) const
	{
	desc.dimensions = getDimensions();
	desc.localPose = getLocalPose();
	desc.group = _group;
	desc.materialIndex = _material;
	desc.skinWidth = _skinWidth;
	desc.shapeFlags = _flags;
	}

/* ------------------------------------------------------------------ sphere */

SphereShape::SphereShape(Actor& actor, const NxSphereShapeDesc& desc)
	: ShapeImpl<NxSphereShape>(actor, desc, NX_SHAPE_SPHERE)
	{
	_bulletShape = new btSphereShape(desc.radius);
	}

void SphereShape::setRadius(NxReal radius)
	{
	static_cast<btSphereShape*>(_bulletShape)->setUnscaledRadius(radius);
	_actor->rebuildCompoundShape();
	}

NxReal SphereShape::getRadius() const
	{
	return static_cast<const btSphereShape*>(_bulletShape)->getRadius();
	}

void SphereShape::saveToDesc(NxSphereShapeDesc& desc) const
	{
	desc.radius = getRadius();
	desc.localPose = getLocalPose();
	desc.group = _group;
	desc.materialIndex = _material;
	desc.skinWidth = _skinWidth;
	desc.shapeFlags = _flags;
	}

/* ----------------------------------------------------------------- capsule */

/*
 * Axis and height are the two things to get right, and both are confirmed at
 * the one construction site, DataBase.cpp:295-301, which sets column 1 (Y) to
 * the long axis and passes the full extent.
 *
 * 2.8: axis along Y, `height` is the FULL cylinder length between cap centres,
 * so total length is height + 2*radius.
 * Bullet: btCapsuleShape is also Y-axis, and its constructor also takes the
 * full cylinder height between cap centres.
 *
 * So this is a pass-through -- but only because both agree, which is worth
 * saying out loud: btCapsuleShapeX and btCapsuleShapeZ exist, and picking one
 * of those would rotate every capsule in the game by ninety degrees.
 */
CapsuleShape::CapsuleShape(Actor& actor, const NxCapsuleShapeDesc& desc)
	: ShapeImpl<NxCapsuleShape>(actor, desc, NX_SHAPE_CAPSULE),
	  _radius(desc.radius), _height(desc.height)
	{
	_bulletShape = new btCapsuleShape(_radius, _height);
	}

void CapsuleShape::setRadius(NxReal radius)
	{
	_radius = radius;
	delete _bulletShape;
	_bulletShape = new btCapsuleShape(_radius, _height);
	_actor->rebuildCompoundShape();
	}

NxReal CapsuleShape::getRadius() const
	{
	return _radius;
	}

void CapsuleShape::setHeight(NxReal height)
	{
	_height = height;
	delete _bulletShape;
	_bulletShape = new btCapsuleShape(_radius, _height);
	_actor->rebuildCompoundShape();
	}

NxReal CapsuleShape::getHeight() const
	{
	return _height;
	}

void CapsuleShape::saveToDesc(NxCapsuleShapeDesc& desc) const
	{
	desc.radius = _radius;
	desc.height = _height;
	desc.localPose = getLocalPose();
	desc.group = _group;
	desc.materialIndex = _material;
	desc.skinWidth = _skinWidth;
	desc.shapeFlags = _flags;
	}

} /* namespace px28 */
