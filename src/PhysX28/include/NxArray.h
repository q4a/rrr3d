#ifndef NX_ARRAY_H
#define NX_ARRAY_H

/*
 * PhysX 2.8's NxArray, reduced to what this game uses.
 *
 * The engine names it once, in Actor::_NxShapeDescList:
 *
 *   typedef NxArray<NxShapeDesc*, NxAllocatorDefault> _NxShapeDescList;
 *
 * and uses push_back, clear, empty, size, iteration and indexing. The SDK's
 * version is a full custom container with an allocator policy; there is nothing
 * to be gained by reproducing that, so this is std::vector underneath with the
 * 2.8 spelling on top and the allocator parameter accepted and ignored.
 *
 * NxAllocatorDefault is an empty tag type for the same reason -- it exists to
 * satisfy the template argument the engine writes, and does no allocating.
 */

#include "NxSimpleTypes.h"

#include <vector>

class NxAllocatorDefault
	{
	};

template<class T, class Alloc = NxAllocatorDefault>
class NxArray
	{
	public:
	typedef typename std::vector<T>::iterator iterator;
	typedef typename std::vector<T>::const_iterator const_iterator;

	NX_INLINE void push_back(const T &value) { _items.push_back(value); }
	NX_INLINE void pushBack(const T &value)  { _items.push_back(value); }
	NX_INLINE void clear()                   { _items.clear(); }
	NX_INLINE bool empty() const             { return _items.empty(); }
	NX_INLINE NxU32 size() const             { return NxU32(_items.size()); }

	NX_INLINE iterator begin()               { return _items.begin(); }
	NX_INLINE iterator end()                 { return _items.end(); }
	NX_INLINE const_iterator begin() const   { return _items.begin(); }
	NX_INLINE const_iterator end() const     { return _items.end(); }

	NX_INLINE T &operator[](NxU32 i)             { return _items[i]; }
	NX_INLINE const T &operator[](NxU32 i) const { return _items[i]; }

	private:
	std::vector<T> _items;
	};

#endif /* NX_ARRAY_H */
