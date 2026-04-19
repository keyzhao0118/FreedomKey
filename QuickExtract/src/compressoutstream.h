#pragma once

#include <Common/MyCom.h>
#include <7zip/IStream.h>
#include <QFile>

class CompressOutStream :
	public IOutStream,
	public CMyUnknownImp
{
	Z7_COM_UNKNOWN_IMP_1(IOutStream)

public:
	CompressOutStream(const QString& filePath);
	~CompressOutStream();

	bool isOpen() const;

	// ISequentialOutStream
	STDMETHOD(Write)(const void* data, UInt32 size, UInt32* processedSize) override;

	// IOutStream
	STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) override;
	STDMETHOD(SetSize)(UInt64 newSize) override;

private:
	QFile m_file;
};
