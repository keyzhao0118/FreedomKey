#include "extractworker.h"
#include "commonhelper.h"
#include "archiveopencallback.h"
#include "archiveextractcallback.h"
#include "filewriterthread.h"

ExtractWorker::ExtractWorker()
{
	setAutoDelete(true);
}

ExtractWorker::~ExtractWorker()
{
}

void ExtractWorker::setArchivePath(const QString& archivePath) { m_archivePath = archivePath; }
void ExtractWorker::setEntryPath(const QString& entryPath) { m_entryPath = entryPath; }
void ExtractWorker::setDestDirPath(const QString& destDirPath) { m_destDirPath = destDirPath; }
void ExtractWorker::setPassword(const QString& password) { m_password = password; }
void ExtractWorker::setIndices(const QVector<quint32>& indices) { m_indices = indices; }
void ExtractWorker::setProgressCounter(QAtomicInteger<quint64>* counter) { m_progressCounter = counter; }
void ExtractWorker::setInterruptionFlag(QAtomicInt* flag) { m_interruptionFlag = flag; }
void ExtractWorker::setResultFlag(QAtomicInt* flag) { m_resultFlag = flag; }

void ExtractWorker::run()
{
	ArchiveOpenCallBack* openCallBackSpec = new ArchiveOpenCallBack();
	CMyComPtr<IArchiveOpenCallback> openCallBack(openCallBackSpec);
	openCallBackSpec->setPassword(m_password);

	CMyComPtr<IInArchive> archive;
	HRESULT hrOpen = CommonHelper::tryOpenArchive(m_archivePath, openCallBack, archive);
	if (hrOpen != S_OK)
	{
		CommonHelper::LogKeyZipDebugMsg("ExtractWorker: Failed to open archive.");
		if (m_resultFlag)
			m_resultFlag->storeRelease(1);
		return;
	}

	FileWriterThread writerThread;
	writerThread.start();

	ArchiveExtractCallBack* extractCallBackSpec = new ArchiveExtractCallBack();
	CMyComPtr<IArchiveExtractCallback> extractCallBack(extractCallBackSpec);
	extractCallBackSpec->init(archive, m_entryPath, m_destDirPath, m_password,
		&writerThread, m_progressCounter, m_interruptionFlag);

	HRESULT hrExtract;
	if (m_indices.isEmpty())
	{
		hrExtract = archive->Extract(nullptr, static_cast<UInt32>(-1), false, extractCallBack);
	}
	else
	{
		hrExtract = archive->Extract(
			reinterpret_cast<const UInt32*>(m_indices.constData()),
			static_cast<UInt32>(m_indices.size()),
			false, extractCallBack);
	}

	writerThread.stop();

	if (hrExtract != S_OK && hrExtract != E_ABORT)
	{
		CommonHelper::LogKeyZipDebugMsg("ExtractWorker: Extraction failed with HRESULT " + QString::number(hrExtract, 16));
		if (m_resultFlag)
			m_resultFlag->storeRelease(1);
	}
	else if (hrExtract == E_ABORT)
	{
		CommonHelper::LogKeyZipDebugMsg("ExtractWorker: Extraction aborted.");
	}
}
