#ifndef NX_STREAM_H
#define NX_STREAM_H

/*
 * PhysX 2.8's NxStream.
 *
 * Transcribed from extern/physx/include/Foundation/NxStream.h.
 *
 * This one is exact by necessity rather than by principle: the game DERIVES
 * from it. src/Rock3dEngine/header/px/Stream.h defines MemoryWriteBuffer,
 * MemoryReadBuffer and UserStream as NxStream subclasses and overrides every
 * method below, with these exact signatures and const qualifiers. A missing
 * const or a differently-spelled return type does not degrade gracefully -- the
 * override stops overriding and the class stays abstract.
 *
 * Note the asymmetry, which is 2.8's: the read methods are const and return
 * values, the store methods are non-const and return NxStream& for chaining.
 */

#include "NxSimpleTypes.h"

class NxStream
	{
	public:
	virtual ~NxStream() {}

	virtual NxU8   readByte() const = 0;
	virtual NxU16  readWord() const = 0;
	virtual NxU32  readDword() const = 0;
	virtual float  readFloat() const = 0;
	virtual double readDouble() const = 0;
	virtual void   readBuffer(void* buffer, NxU32 size) const = 0;

	virtual NxStream& storeByte(NxU8 b) = 0;
	virtual NxStream& storeWord(NxU16 w) = 0;
	virtual NxStream& storeDword(NxU32 d) = 0;
	virtual NxStream& storeFloat(NxReal f) = 0;
	virtual NxStream& storeDouble(NxF64 f) = 0;
	virtual NxStream& storeBuffer(const void* buffer, NxU32 size) = 0;

	NX_INLINE NxStream& storeByte(char b)    { return storeByte(NxU8(b)); }
	NX_INLINE NxStream& storeByte(bool b)    { return storeByte(NxU8(b)); }
	NX_INLINE NxStream& storeWord(short w)   { return storeWord(NxU16(w)); }
	NX_INLINE NxStream& storeDword(int d)    { return storeDword(NxU32(d)); }
	};

#endif /* NX_STREAM_H */
