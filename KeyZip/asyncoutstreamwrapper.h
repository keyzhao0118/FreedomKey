#pragma once

#include <Common/MyCom.h>
#include <7zip/IStream.h>

class WriteBufferQueue;

class AsyncOutStreamWrapper :
	public ISequentialOutStream,
	public CMyUnknownImp
{
	Z7_COM_UNKNOWN_IMP_1(ISequentialOutStream)

public:
	explicit AsyncOutStreamWrapper(WriteBufferQueue* bufferQueue);

	// ISequentialOutStream
	STDMETHOD(Write)(const void* data, UInt32 size, UInt32* processedSize) override;

private:
	WriteBufferQueue* m_bufferQueue;
};
