#include "archiveextractor.h"
#include "commonhelper.h"
#include "archiveopencallback.h"
#include "extractworker.h"
#include <QDir>
#include <QElapsedTimer>
#include <QThreadPool>
#include <7zip/Archive/IArchive.h>

ArchiveExtractor::ArchiveExtractor(QObject* parent)
	: QThread(parent)
{
}

ArchiveExtractor::~ArchiveExtractor()
{
	requestInterruption();
	if (!wait(3000))
		CommonHelper::LogKeyZipDebugMsg("ArchiveExtractor: Failed to stop thread within 3 seconds.");
}

void ArchiveExtractor::extractArchive(const QString& archivePath, const QString& entryPath, const QString& destDirPath)
{
	m_archivePath = archivePath;
	m_entryPath = entryPath;
	m_destDirPath = destDirPath;
	m_password.clear();
	start();
}

void ArchiveExtractor::run()
{
	QElapsedTimer elapsedTimer;
	elapsedTimer.start();

	// 1. Open archive for inspection (detect solid, enumerate items)
	ArchiveOpenCallBack* openCallBackSpec = new ArchiveOpenCallBack();
	CMyComPtr<IArchiveOpenCallback> openCallBack(openCallBackSpec);
	connect(openCallBackSpec, &ArchiveOpenCallBack::requirePassword,
		this, &ArchiveExtractor::requirePassword, Qt::DirectConnection);

	CMyComPtr<IInArchive> archive;
	HRESULT hrOpen = CommonHelper::tryOpenArchive(m_archivePath, openCallBack, archive);
	if (hrOpen == E_ABORT)
	{
		emit extractCanceled();
		return;
	}
	if (hrOpen != S_OK)
	{
		CommonHelper::LogKeyZipDebugMsg("ArchiveExtractor: Failed to open archive.");
		emit extractFailed();
		return;
	}

	m_password = openCallBackSpec->getPassword();

	// 2. Detect solid
	bool isSolid = detectSolid(archive);
	CommonHelper::LogKeyZipDebugMsg(
		QString("ArchiveExtractor: Archive is %1.").arg(isSolid ? "solid" : "non-solid"));

	// 3. Collect file indices and compute total size
	QVector<quint32> fileIndices;
	quint64 totalSize = 0;
	collectFileIndices(archive, isSolid, fileIndices, totalSize);

	// Close inspection archive; workers open their own instances
	archive->Close();
	archive.Release();

	if (fileIndices.isEmpty() && !isSolid)
	{
		emit extractSucceed();
		CommonHelper::LogKeyZipDebugMsg("ArchiveExtractor: No files to extract.");
		return;
	}

	// 4. Determine worker count
	int workerCount;
	if (isSolid || fileIndices.size() <= 1)
	{
		workerCount = 1;
	}
	else
	{
		workerCount = qMin(1, fileIndices.size());
		workerCount = qMax(1, workerCount);
	}

	CommonHelper::LogKeyZipDebugMsg(
		QString("ArchiveExtractor: Using %1 decode worker(s) for %2 files, total size %3.")
			.arg(workerCount).arg(fileIndices.size()).arg(CommonHelper::formatFileSize(totalSize)));

	// 5. Partition indices across workers
	QVector<QVector<quint32>> partitions(workerCount);
	if (!isSolid)
	{
		for (int i = 0; i < fileIndices.size(); i++)
			partitions[i % workerCount].append(fileIndices[i]);
	}
	// For solid: partitions[0] remains empty -> worker uses nullptr (extract all)

	// 6. Create shared state
	QAtomicInteger<quint64>* progressCounters = new QAtomicInteger<quint64>[workerCount]();
	QAtomicInt interruptionFlag(0);
	QAtomicInt errorFlag(0);

	// 7. Create thread pool and submit workers
	QThreadPool pool;
	pool.setMaxThreadCount(workerCount);

	for (int i = 0; i < workerCount; i++)
	{
		ExtractWorker* worker = new ExtractWorker();
		worker->setArchivePath(m_archivePath);
		worker->setEntryPath(m_entryPath);
		worker->setDestDirPath(m_destDirPath);
		worker->setPassword(m_password);
		worker->setIndices(partitions[i]);
		worker->setProgressCounter(&progressCounters[i]);
		worker->setInterruptionFlag(&interruptionFlag);
		worker->setResultFlag(&errorFlag);
		pool.start(worker);
	}

	// 8. Poll progress until all workers complete
	while (!pool.waitForDone(200))
	{
		if (isInterruptionRequested())
		{
			interruptionFlag.storeRelease(1);
			pool.waitForDone();
			delete[] progressCounters;
			emit extractCanceled();
			CommonHelper::LogKeyZipDebugMsg("ArchiveExtractor: Extraction interrupted.");
			return;
		}

		quint64 totalCompleted = 0;
		for (int i = 0; i < workerCount; i++)
			totalCompleted += progressCounters[i].loadAcquire();

		if (totalSize > 0)
			emit updateProgress(totalCompleted, totalSize);
	}

	// Final progress update
	{
		quint64 totalCompleted = 0;
		for (int i = 0; i < workerCount; i++)
			totalCompleted += progressCounters[i].loadAcquire();
		if (totalSize > 0)
			emit updateProgress(totalCompleted, totalSize);
	}

	delete[] progressCounters;

	// 9. Check results
	if (interruptionFlag.loadAcquire())
	{
		emit extractCanceled();
		return;
	}
	if (errorFlag.loadAcquire())
	{
		emit extractFailed();
		return;
	}

	emit extractSucceed();
	CommonHelper::LogKeyZipDebugMsg(
		"ArchiveExtractor: Extraction completed in " + QString::number(elapsedTimer.elapsed()) + " ms.");
}

bool ArchiveExtractor::detectSolid(IInArchive* archive)
{
	PROPVARIANT prop;
	PropVariantInit(&prop);
	HRESULT hr = archive->GetArchiveProperty(kpidSolid, &prop);
	if (hr != S_OK || prop.vt != VT_BOOL)
	{
		PropVariantClear(&prop);
		return false;
	}
	bool solid = (prop.boolVal != VARIANT_FALSE);
	PropVariantClear(&prop);
	return solid;
}

void ArchiveExtractor::collectFileIndices(IInArchive* archive, bool isSolid,
	QVector<quint32>& outIndices, quint64& outTotalSize)
{
	outIndices.clear();
	outTotalSize = 0;

	UInt32 numItems = 0;
	archive->GetNumberOfItems(&numItems);

	QString nativeEntryPath = QDir::toNativeSeparators(m_entryPath);

	for (UInt32 i = 0; i < numItems; i++)
	{
		bool matches = true;

		if (!nativeEntryPath.isEmpty())
		{
			PROPVARIANT propPath;
			PropVariantInit(&propPath);
			archive->GetProperty(i, kpidPath, &propPath);
			QString path = QDir::toNativeSeparators(QString::fromWCharArray(propPath.bstrVal));
			PropVariantClear(&propPath);

			if (!path.startsWith(nativeEntryPath))
			{
				matches = false;
			}
			else
			{
				QString remainder = path.mid(nativeEntryPath.size());
				if (!remainder.isEmpty() && !remainder.startsWith(QDir::separator()))
					matches = false;
			}
		}

		PROPVARIANT propSize;
		PropVariantInit(&propSize);
		archive->GetProperty(i, kpidSize, &propSize);
		quint64 itemSize = 0;
		if (propSize.vt == VT_UI8)
			itemSize = propSize.uhVal.QuadPart;
		else if (propSize.vt == VT_UI4)
			itemSize = propSize.ulVal;
		PropVariantClear(&propSize);

		if (isSolid)
		{
			// For solid archives, count total size of ALL items (SDK decodes everything)
			outTotalSize += itemSize;
			if (matches)
				outIndices.append(i);
		}
		else
		{
			if (matches)
			{
				outIndices.append(i);
				outTotalSize += itemSize;
			}
		}
	}
}
