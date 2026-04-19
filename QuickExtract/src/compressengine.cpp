#include "compressengine.h"
#include "archivehelper.h"
#include "compressoutstream.h"

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>

extern const GUID IID_ISetProperties;

CompressEngine::CompressEngine(const QStringList& inputPaths, const CompressOptions& options, QObject* parent)
	: QThread(parent)
	, m_inputPaths(inputPaths)
	, m_options(options)
{
}

void CompressEngine::cancel()
{
	m_cancelFlag.storeRelease(1);
}

void CompressEngine::run()
{
	QElapsedTimer timer;
	timer.start();

	QVector<FileItem> items = enumerateFiles(m_inputPaths);
	if (items.isEmpty())
	{
		emit compressionFinished(false, tr("No files to compress"), timer.elapsed());
		return;
	}

	if (m_cancelFlag.loadAcquire())
	{
		emit compressionFinished(false, tr("Cancelled"), timer.elapsed());
		return;
	}

	CMyComPtr<IOutArchive> outArchive;
	HRESULT hr = ArchiveHelper::createOutArchive(m_options.format, outArchive);
	if (hr != S_OK)
	{
		emit compressionFinished(false, tr("Failed to create archive handler"), timer.elapsed());
		return;
	}

	// Set properties (encryption, header encryption)
	if (!m_options.password.isEmpty())
	{
		CMyComPtr<ISetProperties> setProps;
		outArchive->QueryInterface(IID_ISetProperties, (void**)&setProps);
		if (setProps)
		{
			QVector<const wchar_t*> names;
			QVector<PROPVARIANT> values;

			// Header encryption for 7z
			PROPVARIANT heVal;
			PropVariantInit(&heVal);
			if (m_options.format.compare("7z", Qt::CaseInsensitive) == 0 && m_options.encryptHeaders)
			{
				static const wchar_t heName[] = L"he";
				names.append(heName);
				heVal.vt = VT_BSTR;
				heVal.bstrVal = SysAllocString(L"on");
				values.append(heVal);
			}

			if (!names.isEmpty())
			{
				setProps->SetProperties(names.data(), values.data(), static_cast<UInt32>(names.size()));

				for (auto& v : values)
					PropVariantClear(&v);
			}
		}
	}

	CompressOutStream* outStreamSpec = new CompressOutStream(m_options.outputPath);
	CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);
	if (!outStreamSpec->isOpen())
	{
		emit compressionFinished(false, tr("Failed to create output file"), timer.elapsed());
		return;
	}

	UpdateCallback* updateCbSpec = new UpdateCallback();
	CMyComPtr<IArchiveUpdateCallback> updateCb(updateCbSpec);
	updateCbSpec->init(items, m_options.password);
	updateCbSpec->setCancelFlag(&m_cancelFlag);

	connect(updateCbSpec, &UpdateCallback::progressUpdated,
		this, &CompressEngine::compressionProgress, Qt::QueuedConnection);

	HRESULT hrUpdate = outArchive->UpdateItems(outStream, static_cast<UInt32>(items.size()), updateCb);

	const qint64 elapsed = timer.elapsed();

	if (hrUpdate == E_ABORT)
	{
		emit compressionFinished(false, tr("Cancelled"), elapsed);
		return;
	}

	if (hrUpdate != S_OK)
	{
		emit compressionFinished(false,
			tr("Compression failed (0x%1)").arg(static_cast<quint32>(hrUpdate), 8, 16, QChar('0')), elapsed);
		return;
	}

	emit compressionFinished(true, QString(), elapsed);
}

QVector<FileItem> CompressEngine::enumerateFiles(const QStringList& inputPaths)
{
	QVector<FileItem> items;

	for (const QString& path : inputPaths)
	{
		QFileInfo fi(path);
		if (!fi.exists())
			continue;

		if (fi.isDir())
		{
			QString dirName = fi.fileName();
			FileItem dirItem = makeFileItem(fi.absoluteFilePath(), dirName);
			dirItem.isDir = true;
			dirItem.size = 0;
			items.append(dirItem);

			enumerateDir(fi.absoluteFilePath(), dirName, items);
		}
		else
		{
			QString relativePath = fi.fileName();
			items.append(makeFileItem(fi.absoluteFilePath(), relativePath));
		}
	}

	return items;
}

void CompressEngine::enumerateDir(const QString& dirPath, const QString& baseDir, QVector<FileItem>& items)
{
	QDirIterator it(dirPath, QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

	while (it.hasNext())
	{
		it.next();
		QFileInfo fi = it.fileInfo();

		QDir parentDir(dirPath);
		QString relative = baseDir + QDir::separator() + parentDir.relativeFilePath(fi.absoluteFilePath());

		FileItem item = makeFileItem(fi.absoluteFilePath(), relative);
		if (fi.isDir())
		{
			item.isDir = true;
			item.size = 0;
		}
		items.append(item);
	}
}

FileItem CompressEngine::makeFileItem(const QString& fullPath, const QString& relativePath)
{
	QFileInfo fi(fullPath);
	FileItem item;
	item.fullDiskPath = fi.absoluteFilePath();
	item.relativePath = relativePath;
	item.isDir = fi.isDir();
	item.size = fi.isDir() ? 0 : static_cast<quint64>(fi.size());

	// Convert file attributes
	item.attributes = 0;
	if (fi.isDir())
		item.attributes |= FILE_ATTRIBUTE_DIRECTORY;
	if (fi.isHidden())
		item.attributes |= FILE_ATTRIBUTE_HIDDEN;
	if (!fi.isWritable())
		item.attributes |= FILE_ATTRIBUTE_READONLY;
	if (item.attributes == 0)
		item.attributes = FILE_ATTRIBUTE_NORMAL;

	// Use Win32 API to get accurate file times
	WIN32_FILE_ATTRIBUTE_DATA attrData;
	if (GetFileAttributesExW(reinterpret_cast<const wchar_t*>(fi.absoluteFilePath().utf16()),
		GetFileExInfoStandard, &attrData))
	{
		item.cTime = attrData.ftCreationTime;
		item.mTime = attrData.ftLastWriteTime;
		item.aTime = attrData.ftLastAccessTime;
		item.attributes = attrData.dwFileAttributes;
	}
	else
	{
		memset(&item.cTime, 0, sizeof(FILETIME));
		memset(&item.mTime, 0, sizeof(FILETIME));
		memset(&item.aTime, 0, sizeof(FILETIME));
	}

	return item;
}
