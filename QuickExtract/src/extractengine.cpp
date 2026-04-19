#include "extractengine.h"
#include "archivehelper.h"
#include "opencallback.h"
#include "extractcallback.h"
#include <QFileInfo>
#include <QDir>
#include <QElapsedTimer>

static QString opResultToString(Int32 opRes)
{
	switch (opRes)
	{
	case NArchive::NExtract::NOperationResult::kWrongPassword:
		return ExtractEngine::tr("Wrong password");
	case NArchive::NExtract::NOperationResult::kCRCError:
		return ExtractEngine::tr("CRC error (possibly wrong password)");
	case NArchive::NExtract::NOperationResult::kDataError:
		return ExtractEngine::tr("Data error");
	case NArchive::NExtract::NOperationResult::kUnsupportedMethod:
		return ExtractEngine::tr("Unsupported compression method");
	case NArchive::NExtract::NOperationResult::kUnavailable:
		return ExtractEngine::tr("Data unavailable");
	case NArchive::NExtract::NOperationResult::kUnexpectedEnd:
		return ExtractEngine::tr("Unexpected end of archive");
	case NArchive::NExtract::NOperationResult::kHeadersError:
		return ExtractEngine::tr("Headers error");
	default:
		return ExtractEngine::tr("Unknown error (%1)").arg(opRes);
	}
}

ExtractEngine::ExtractEngine(const QStringList& archivePaths, const QString& targetDir, QObject* parent)
	: QThread(parent)
	, m_archivePaths(archivePaths)
	, m_targetDir(targetDir)
{
}

void ExtractEngine::cancel()
{
	m_cancelFlag.storeRelease(1);
}

void ExtractEngine::run()
{
	const int total = m_archivePaths.size();

	for (int i = 0; i < total; ++i)
	{
		if (m_cancelFlag.loadAcquire())
			break;

		const QString& archivePath = m_archivePaths[i];
		const QString archiveName = QFileInfo(archivePath).fileName();

		emit archiveStarted(i, total, archiveName);

		QElapsedTimer timer;
		timer.start();

		CLSID clsid = ArchiveHelper::detectArchiveType(archivePath);
		if (clsid == CLSID_NULL)
		{
			emit archiveFinished(archiveName, false, tr("Unrecognized archive format"), timer.elapsed());
			continue;
		}

		const QString destDir = computeDestDir(archivePath);
		if (!QDir().mkpath(destDir))
		{
			emit archiveFinished(archiveName, false, tr("Failed to create output folder"), timer.elapsed());
			continue;
		}

		OpenCallback* openCbSpec = new OpenCallback();
		CMyComPtr<IArchiveOpenCallback> openCb(openCbSpec);
		openCbSpec->setCancelFlag(&m_cancelFlag);

		connect(openCbSpec, &OpenCallback::passwordRequired,
			this, &ExtractEngine::passwordRequired, Qt::BlockingQueuedConnection);

		CMyComPtr<IInArchive> archive;
		HRESULT hrOpen = ArchiveHelper::tryOpenArchive(archivePath, openCb, archive);

		if (hrOpen != S_OK)
		{
			if (hrOpen == E_ABORT)
			{
				emit archiveFinished(archiveName, false, tr("Cancelled"), timer.elapsed());
				break;
			}
			emit archiveFinished(archiveName, false, tr("Failed to open archive"), timer.elapsed());
			continue;
		}

		ExtractCallback* extractCbSpec = new ExtractCallback();
		CMyComPtr<IArchiveExtractCallback> extractCb(extractCbSpec);

		QString password = openCbSpec->getPassword();
		extractCbSpec->init(archive, destDir, password);
		extractCbSpec->setCancelFlag(&m_cancelFlag);

		connect(extractCbSpec, &ExtractCallback::progressUpdated,
			this, &ExtractEngine::archiveProgress, Qt::QueuedConnection);
		connect(extractCbSpec, &ExtractCallback::passwordRequired,
			this, &ExtractEngine::passwordRequired, Qt::BlockingQueuedConnection);

		HRESULT hrExtract = archive->Extract(nullptr, static_cast<UInt32>(-1), false, extractCb);

		archive->Close();

		const qint64 elapsed = timer.elapsed();

		if (hrExtract == E_ABORT)
		{
			emit archiveFinished(archiveName, false, tr("Cancelled"), elapsed);
			break;
		}

		if (hrExtract != S_OK)
		{
			emit archiveFinished(archiveName, false,
				tr("Extraction failed (0x%1)").arg(static_cast<quint32>(hrExtract), 8, 16, QChar('0')), elapsed);
			continue;
		}

		const int errors = extractCbSpec->errorCount();
		if (errors == 0)
		{
			emit archiveFinished(archiveName, true, QString(), elapsed);
		}
		else
		{
			QString reasonText = opResultToString(extractCbSpec->firstFailureReason());
			const int ok = extractCbSpec->successCount();
			QString errorMsg;
			if (ok == 0)
				errorMsg = reasonText;
			else
				errorMsg = tr("%1 item(s) failed: %2, %3 succeeded").arg(errors).arg(reasonText).arg(ok);
			emit archiveFinished(archiveName, false, errorMsg, elapsed);
		}
	}

	emit allFinished();
}

QString ExtractEngine::computeDestDir(const QString& archivePath) const
{
	const QString baseName = QFileInfo(archivePath).completeBaseName();
	QString destDir = m_targetDir + QDir::separator() + baseName;

	if (!QDir(destDir).exists())
		return destDir;

	int suffix = 2;
	while (true)
	{
		QString candidate = m_targetDir + QDir::separator() + baseName + " (" + QString::number(suffix) + ")";
		if (!QDir(candidate).exists())
			return candidate;
		++suffix;
	}
}
