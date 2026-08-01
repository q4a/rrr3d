#ifndef STREAM_H
#define STREAM_H

#include "PxStream.h"

namespace physx
{

class MemoryWriteBuffer : public PxStream
	{
	public:
								MemoryWriteBuffer();
	virtual						~MemoryWriteBuffer();
				void			clear();

	virtual		PxU8			readByte()								const	{ PX_ASSERT(0);	return 0;	}
	virtual		PxU16			readWord()								const	{ PX_ASSERT(0);	return 0;	}
	virtual		PxU32			readDword()								const	{ PX_ASSERT(0);	return 0;	}
	virtual		float			readFloat()								const	{ PX_ASSERT(0);	return 0.0f;}
	virtual		double			readDouble()							const	{ PX_ASSERT(0);	return 0.0;	}
	virtual		void			readBuffer(void* buffer, PxU32 size)	const	{ PX_ASSERT(0);				}

	virtual		PxStream&		storeByte(PxU8 b);
	virtual		PxStream&		storeWord(PxU16 w);
	virtual		PxStream&		storeDword(PxU32 d);
	virtual		PxStream&		storeFloat(PxReal f);
	virtual		PxStream&		storeDouble(PxF64 f);
	virtual		PxStream&		storeBuffer(const void* buffer, PxU32 size);

				PxU32			currentSize;
				PxU32			maxSize;
				PxU8*			data;
	};

class MemoryReadBuffer : public PxStream
	{
	public:
								MemoryReadBuffer(const PxU8* data);
	virtual						~MemoryReadBuffer();

	virtual		PxU8			readByte()								const;
	virtual		PxU16			readWord()								const;
	virtual		PxU32			readDword()								const;
	virtual		float			readFloat()								const;
	virtual		double			readDouble()							const;
	virtual		void			readBuffer(void* buffer, PxU32 size)	const;

	virtual		PxStream&		storeByte(PxU8 b)							{ PX_ASSERT(0);	return *this;	}
	virtual		PxStream&		storeWord(PxU16 w)							{ PX_ASSERT(0);	return *this;	}
	virtual		PxStream&		storeDword(PxU32 d)							{ PX_ASSERT(0);	return *this;	}
	virtual		PxStream&		storeFloat(PxReal f)						{ PX_ASSERT(0);	return *this;	}
	virtual		PxStream&		storeDouble(PxF64 f)						{ PX_ASSERT(0);	return *this;	}
	virtual		PxStream&		storeBuffer(const void* buffer, PxU32 size)	{ PX_ASSERT(0);	return *this;	}

	mutable		const PxU8*		buffer;
	};

}
#endif
