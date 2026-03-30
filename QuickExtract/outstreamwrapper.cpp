#include "outstreamwrapper.h"

OutStreamWrapper::OutStreamWrapper(const QString& filePath) 
{
	m_file.setFileName(filePath);
	m_file.open(QIODevice::WriteOnly);
}

OutStreamWrapper::~OutStreamWrapper()
{
	if (m_file.isOpen())
		m_file.close();
}

STDMETHODIMP OutStreamWrapper::Write(const void* data, UInt32 size, UInt32* processedSize)
{
	if (!m_file.isOpen())
		return E_FAIL;

	qint64 written = m_file.write(static_cast<const char*>(data), size);
	
	if (processedSize) 
		*processedSize = (written > 0) ? static_cast<UInt32>(written) : 0;

	return (written == size) ? S_OK : E_FAIL;
}
