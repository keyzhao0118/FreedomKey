#include "archiveextractcallback.h"
#include "asyncoutstreamwrapper.h"
#include "writebufferqueue.h"
#include "filewriterthread.h"
#include "commonhelper.h"
#include <QDir>
#include <QFileInfo>

void ArchiveExtractCallBack::init(IInArchive* archive, const QString& entryPath, const QString& destDirPath,
	const QString& password, FileWriterThread* writerThread,
	QAtomicInteger<quint64>* progressCounter, QAtomicInt* interruptionFlag)
{
	m_archive = archive;
	m_entryPath = QDir::toNativeSeparators(entryPath);
	m_destDirPath = QDir::toNativeSeparators(destDirPath);
	m_password = password;
	m_writerThread = writerThread;
	m_progressCounter = progressCounter;
	m_interruptionFlag = interruptionFlag;
}

STDMETHODIMP ArchiveExtractCallBack::SetTotal(const UInt64 size)
{
	CommonHelper::LogKeyZipDebugMsg("ArchiveExtractCallBack: Total size to extract: " + QString::number(size));
	return S_OK;
}

STDMETHODIMP ArchiveExtractCallBack::SetCompleted(const UInt64* completedSize)
{
	if (completedSize && m_progressCounter)
		m_progressCounter->storeRelease(static_cast<quint64>(*completedSize));

	if (m_interruptionFlag && m_interruptionFlag->loadAcquire())
		return E_ABORT;

	return S_OK;
}

STDMETHODIMP ArchiveExtractCallBack::GetStream(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode)
{
	*outStream = nullptr;
	m_currentIndex = index;
	m_bSkipCurrent = false;

	if (askExtractMode != NArchive::NExtract::NAskMode::kExtract)
		return S_OK;

	PROPVARIANT propPath;	PropVariantInit(&propPath);
	PROPVARIANT propIsDir;	PropVariantInit(&propIsDir);
	m_archive->GetProperty(index, kpidPath, &propPath);
	m_archive->GetProperty(index, kpidIsDir, &propIsDir);

	QString path = QDir::toNativeSeparators(QString::fromWCharArray(propPath.bstrVal));
	if (m_entryPath.isEmpty())
	{
		m_currentFullPath = m_destDirPath + QDir::separator() + path;
	}
	else
	{
		if (!path.startsWith(m_entryPath))
		{
			m_bSkipCurrent = true;
			PropVariantClear(&propPath);
			PropVariantClear(&propIsDir);
			return S_OK;
		}

		QString relativePath = path.mid(m_entryPath.size());
		if (!relativePath.isEmpty() && !relativePath.startsWith(QDir::separator()))
		{
			m_bSkipCurrent = true;
			PropVariantClear(&propPath);
			PropVariantClear(&propIsDir);
			return S_OK;
		}

		int pos = m_entryPath.lastIndexOf(QDir::separator()) + 1;
		m_currentFullPath = m_destDirPath + QDir::separator() + path.mid(pos);
	}
	m_currentIsDir = propIsDir.boolVal != VARIANT_FALSE;

	PropVariantClear(&propPath);
	PropVariantClear(&propIsDir);

	if (m_currentIsDir)
	{
		if (!QDir().mkpath(m_currentFullPath))
			m_bSkipCurrent = true;
		return S_OK;
	}

	const QString parentDir = QFileInfo(m_currentFullPath).absolutePath();
	if (!QDir().mkpath(parentDir))
	{
		m_bSkipCurrent = true;
		return S_OK;
	}

	// Pre-fetch file metadata for the writer thread
	FileMetadata meta;
	meta.isDir = false;

	PROPVARIANT propCTime;	PropVariantInit(&propCTime);
	PROPVARIANT propATime;	PropVariantInit(&propATime);
	PROPVARIANT propMTime;	PropVariantInit(&propMTime);
	PROPVARIANT propAttrib;	PropVariantInit(&propAttrib);

	m_archive->GetProperty(index, kpidCTime, &propCTime);
	m_archive->GetProperty(index, kpidATime, &propATime);
	m_archive->GetProperty(index, kpidMTime, &propMTime);
	m_archive->GetProperty(index, kpidAttrib, &propAttrib);

	if (propCTime.vt == VT_FILETIME)	{ meta.ctime = propCTime.filetime; meta.hasTime = true; }
	if (propATime.vt == VT_FILETIME)	{ meta.atime = propATime.filetime; meta.hasTime = true; }
	if (propMTime.vt == VT_FILETIME)	{ meta.mtime = propMTime.filetime; meta.hasTime = true; }
	if (propAttrib.vt == VT_UI4)		{ meta.attributes = propAttrib.ulVal; meta.hasAttributes = true; }

	PropVariantClear(&propCTime);
	PropVariantClear(&propATime);
	PropVariantClear(&propMTime);
	PropVariantClear(&propAttrib);

	// Create async IO pipeline: buffer queue + async out stream + writer task
	m_currentBuffer = new WriteBufferQueue();

	m_writerThread->startWriteFile(m_currentBuffer, m_currentFullPath, meta);

	AsyncOutStreamWrapper* asyncStreamSpec = new AsyncOutStreamWrapper(m_currentBuffer);
	CMyComPtr<ISequentialOutStream> sequentialOutStream(asyncStreamSpec);

	*outStream = sequentialOutStream.Detach();
	return S_OK;
}

STDMETHODIMP ArchiveExtractCallBack::PrepareOperation(Int32 askExtractMode)
{
	return S_OK;
}

STDMETHODIMP ArchiveExtractCallBack::SetOperationResult(Int32 opRes)
{
	if (m_bSkipCurrent || !m_currentBuffer)
		return S_OK;

	m_currentBuffer->markFinished();
	m_writerThread->waitForCurrentFile();

	delete m_currentBuffer;
	m_currentBuffer = nullptr;

	return S_OK;
}

STDMETHODIMP ArchiveExtractCallBack::CryptoGetTextPassword(BSTR* password)
{
	if (!password)
		return E_INVALIDARG;

	if (m_password.isEmpty())
		return E_ABORT;

	*password = SysAllocString(reinterpret_cast<const OLECHAR*>(m_password.utf16()));
	return S_OK;
}
