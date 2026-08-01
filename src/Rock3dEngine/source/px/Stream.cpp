#include "stdafx.h"

#include <stdio.h>

#include "px/Physx.h"
//#include "PxPhysicsAPI.h"
#include "internal/include/PsArray.h"
#include "px/Stream.h"

namespace physx
{

MemoryWriteBuffer::MemoryWriteBuffer() : currentSize(0), maxSize(0), data(NULL)
	{
	}

MemoryWriteBuffer::~MemoryWriteBuffer()
	{
	PX_DELETE_ARRAY(data);
	}

void MemoryWriteBuffer::clear()
	{
	currentSize = 0;
	}

PxStream& MemoryWriteBuffer::storeByte(PxU8 b)
	{
	storeBuffer(&b, sizeof(PxU8));
	return *this;
	}
PxStream& MemoryWriteBuffer::storeWord(PxU16 w)
	{
	storeBuffer(&w, sizeof(PxU16));
	return *this;
	}
PxStream& MemoryWriteBuffer::storeDword(PxU32 d)
	{
	storeBuffer(&d, sizeof(PxU32));
	return *this;
	}
PxStream& MemoryWriteBuffer::storeFloat(PxReal f)
	{
	storeBuffer(&f, sizeof(PxReal));
	return *this;
	}
PxStream& MemoryWriteBuffer::storeDouble(PxF64 f)
	{
	storeBuffer(&f, sizeof(PxF64));
	return *this;
	}
PxStream& MemoryWriteBuffer::storeBuffer(const void* buffer, PxU32 size)
	{
	PxU32 expectedSize = currentSize + size;
	if(expectedSize > maxSize)
		{
		maxSize = expectedSize + 4096;

		PxU8* newData = new PxU8[maxSize];
		PX_ASSERT(newData!=NULL);

		if(data)
			{
			memcpy(newData, data, currentSize);
			delete[] data;
			}
		data = newData;
		}
	memcpy(data+currentSize, buffer, size);
	currentSize += size;
	return *this;
	}


MemoryReadBuffer::MemoryReadBuffer(const PxU8* data) : buffer(data)
	{
	}

MemoryReadBuffer::~MemoryReadBuffer()
	{
	// We don't own the data => no delete
	}

PxU8 MemoryReadBuffer::readByte() const
	{
	PxU8 b;
	memcpy(&b, buffer, sizeof(PxU8));
	buffer += sizeof(PxU8);
	return b;
	}

PxU16 MemoryReadBuffer::readWord() const
	{
	PxU16 w;
	memcpy(&w, buffer, sizeof(PxU16));
	buffer += sizeof(PxU16);
	return w;
	}

PxU32 MemoryReadBuffer::readDword() const
	{
	PxU32 d;
	memcpy(&d, buffer, sizeof(PxU32));
	buffer += sizeof(PxU32);
	return d;
	}

float MemoryReadBuffer::readFloat() const
	{
	float f;
	memcpy(&f, buffer, sizeof(float));
	buffer += sizeof(float);
	return f;
	}

double MemoryReadBuffer::readDouble() const
	{
	double f;
	memcpy(&f, buffer, sizeof(double));
	buffer += sizeof(double);
	return f;
	}

void MemoryReadBuffer::readBuffer(void* dest, PxU32 size) const
	{
	memcpy(dest, buffer, size); 
	buffer += size; 
	}

}
