#pragma once

#include <7zip/Archive/IArchive.h>
#include <Common/MyCom.h>
#include <QString>

namespace ArchiveHelper
{
	CLSID detectArchiveType(const QString& archivePath);

	HRESULT tryOpenArchive(
		const QString& archivePath,
		IArchiveOpenCallback* openCallback,
		CMyComPtr<IInArchive>& outInArchive);

	HRESULT createOutArchive(
		const QString& format,
		CMyComPtr<IOutArchive>& outArchive);
}
