#include "compressoutstream.h"

CompressOutStream::CompressOutStream(const QString& filePath)
{
	m_file.setFileName(filePath);
	m_file.open(QIODevice::ReadWrite | QIODevice::Truncate);
}

CompressOutStream::~CompressOutStream()
{
	if (m_file.isOpen())
		m_file.close();
}

bool CompressOutStream::isOpen() const
{
	return m_file.isOpen();
}

STDMETHODIMP CompressOutStream::Write(const void* data, UInt32 size, UInt32* processedSize)
{
	if (!m_file.isOpen())
		return E_FAIL;

	qint64 written = m_file.write(static_cast<const char*>(data), size);

	if (processedSize)
		*processedSize = (written > 0) ? static_cast<UInt32>(written) : 0;

	return (written == size) ? S_OK : E_FAIL;
}

STDMETHODIMP CompressOutStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition)
{
	qint64 newPosValue = 0;
	switch (seekOrigin)
	{
	case STREAM_SEEK_SET:
		newPosValue = offset;
		break;
	case STREAM_SEEK_CUR:
		newPosValue = m_file.pos() + offset;
		break;
	case STREAM_SEEK_END:
		newPosValue = m_file.size() + offset;
		break;
	default:
		return STG_E_INVALIDFUNCTION;
	}

	if (m_file.seek(newPosValue))
	{
		if (newPosition)
			*newPosition = m_file.pos();
		return S_OK;
	}
	return E_FAIL;
}

STDMETHODIMP CompressOutStream::SetSize(UInt64 newSize)
{
	return m_file.resize(static_cast<qint64>(newSize)) ? S_OK : E_FAIL;
}
