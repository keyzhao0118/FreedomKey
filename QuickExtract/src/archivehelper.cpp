#include "archivehelper.h"
#include "instreamwrapper.h"
#include <QFile>
#include <QLibrary>

namespace
{

extern "C" const GUID CLSID_CFormatZip;
extern "C" const GUID CLSID_CFormat7z;
extern "C" const GUID CLSID_CFormatRar;
extern "C" const GUID CLSID_CFormatRar5;
typedef UINT32(WINAPI* CreateObjectFunc)(const GUID* clsID, const GUID* iid, void** outObject);

bool isZip(const uint8_t* p)
{
	return p[0] == 0x50 && p[1] == 0x4B &&
		(p[2] == 0x03 || p[2] == 0x05 || p[2] == 0x07) &&
		(p[3] == 0x04 || p[3] == 0x06 || p[3] == 0x08);
}

bool is7z(const uint8_t* p)
{
	static const uint8_t sig[6] = {
		0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C
	};
	return memcmp(p, sig, 6) == 0;
}

bool isRar4(const uint8_t* p)
{
	static const uint8_t sig[7] = {
		0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00
	};
	return memcmp(p, sig, 7) == 0;
}

bool isRar5(const uint8_t* p)
{
	static const uint8_t sig[8] = {
		0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00
	};
	return memcmp(p, sig, 8) == 0;
}

} // anonymous namespace

CLSID ArchiveHelper::detectArchiveType(const QString& archivePath)
{
	QFile file(archivePath);
	if (!file.open(QIODevice::ReadOnly))
		return CLSID_NULL;

	uint8_t header[32] = { 0 };
	const qint64 readSize = file.read(reinterpret_cast<char*>(header), sizeof(header));
	file.close();

	if (readSize < 8)
		return CLSID_NULL;

	if (is7z(header))
		return CLSID_CFormat7z;

	if (isRar5(header))
		return CLSID_CFormatRar5;

	if (isRar4(header))
		return CLSID_CFormatRar;

	if (isZip(header))
		return CLSID_CFormatZip;

	return CLSID_NULL;
}

HRESULT ArchiveHelper::createOutArchive(
	const QString& format,
	CMyComPtr<IOutArchive>& outArchive)
{
	outArchive.Release();

	QLibrary sevenZipLib("7zip.dll");
	if (!sevenZipLib.load())
		return S_FALSE;

	CreateObjectFunc createObjectFunc = (CreateObjectFunc)sevenZipLib.resolve("CreateObject");
	if (!createObjectFunc)
		return S_FALSE;

	const GUID* clsid = nullptr;
	if (format.compare("7z", Qt::CaseInsensitive) == 0)
		clsid = &CLSID_CFormat7z;
	else if (format.compare("zip", Qt::CaseInsensitive) == 0)
		clsid = &CLSID_CFormatZip;
	else
		return S_FALSE;

	//extern const GUID IID_IOutArchive;
	CMyComPtr<IOutArchive> archive;
	if (createObjectFunc(clsid, &IID_IOutArchive, (void**)&archive) != S_OK)
		return S_FALSE;

	outArchive = archive;
	return S_OK;
}

HRESULT ArchiveHelper::tryOpenArchive(
	const QString& archivePath,
	IArchiveOpenCallback* openCallback,
	CMyComPtr<IInArchive>& outInArchive)
{
	outInArchive.Release();

	QLibrary sevenZipLib("7zip.dll");
	if (!sevenZipLib.load())
		return S_FALSE;

	CreateObjectFunc createObjectFunc = (CreateObjectFunc)sevenZipLib.resolve("CreateObject");
	if (!createObjectFunc)
		return S_FALSE;

	const GUID clsid = detectArchiveType(archivePath);
	if (clsid == CLSID_NULL)
		return S_FALSE;

	CMyComPtr<IInArchive> archive;
	if (createObjectFunc(&clsid, &IID_IInArchive, (void**)&archive) != S_OK)
		return S_FALSE;

	InStreamWrapper* inStreamSpec = new InStreamWrapper(archivePath);
	CMyComPtr<IInStream> inStream(inStreamSpec);
	if (!inStreamSpec->isOpen())
		return S_FALSE;

	HRESULT hr = archive->Open(inStream, nullptr, openCallback);
	if (hr == S_OK)
		outInArchive = archive;

	return hr;
}
