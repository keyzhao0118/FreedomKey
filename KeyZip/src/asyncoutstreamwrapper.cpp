#include "asyncoutstreamwrapper.h"
#include "writebufferqueue.h"

AsyncOutStreamWrapper::AsyncOutStreamWrapper(WriteBufferQueue* bufferQueue)
	: m_bufferQueue(bufferQueue)
{
}

STDMETHODIMP AsyncOutStreamWrapper::Write(const void* data, UInt32 size, UInt32* processedSize)
{
	if (!m_bufferQueue)
		return E_FAIL;

	m_bufferQueue->enqueue(static_cast<const char*>(data), size);

	if (processedSize)
		*processedSize = size;
	return S_OK;
}
