#include "extractcallback.h"
#include "asyncoutstreamwrapper.h"
#include "writebufferqueue.h"
#include "filewriterthread.h"
#include <QDir>
#include <QFileInfo>

void ExtractCallback::init(IInArchive* archive, const QString& destDirPath,
	const QString& password, FileWriterThread* writerThread)
{
	m_archive = archive;
	m_destDirPath = QDir::toNativeSeparators(destDirPath);
	m_password = password;
	m_writerThread = writerThread;
}

void ExtractCallback::cleanupPendingBuffer()
{
	if (m_currentBuffer)
	{
		m_currentBuffer->markFinished();
		if (m_writerThread)
			m_writerThread->waitForCurrentFile();
		delete m_currentBuffer;
		m_currentBuffer = nullptr;
	}
}

STDMETHODIMP ExtractCallback::SetTotal(const UInt64 size)
{
	m_totalSize = size;
	return S_OK;
}

STDMETHODIMP ExtractCallback::SetCompleted(const UInt64* completedSize)
{
	if (completedSize)
		emit progressUpdated(*completedSize, m_totalSize);

	if (m_cancelFlag && m_cancelFlag->loadAcquire())
		return E_ABORT;

	return S_OK;
}

STDMETHODIMP ExtractCallback::GetStream(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode)
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
	m_currentFullPath = m_destDirPath + QDir::separator() + path;
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

	m_currentBuffer = new WriteBufferQueue();

	m_writerThread->startWriteFile(m_currentBuffer, m_currentFullPath, meta);

	AsyncOutStreamWrapper* asyncStreamSpec = new AsyncOutStreamWrapper(m_currentBuffer);
	CMyComPtr<ISequentialOutStream> sequentialOutStream(asyncStreamSpec);

	*outStream = sequentialOutStream.Detach();
	return S_OK;
}

STDMETHODIMP ExtractCallback::PrepareOperation(Int32 askExtractMode)
{
	return S_OK;
}

STDMETHODIMP ExtractCallback::SetOperationResult(Int32 opRes)
{
	if (!m_bSkipCurrent && m_currentBuffer)
	{
		m_currentBuffer->markFinished();
		m_writerThread->waitForCurrentFile();

		delete m_currentBuffer;
		m_currentBuffer = nullptr;
	}

	if (opRes == NArchive::NExtract::NOperationResult::kOK)
	{
		++m_successCount;
	}
	else
	{
		++m_errorCount;
		if (m_firstFailureReason == 0)
			m_firstFailureReason = opRes;
	}

	return S_OK;
}

STDMETHODIMP ExtractCallback::CryptoGetTextPassword(BSTR* password)
{
	if (!password)
		return E_INVALIDARG;

	if (!m_password.isEmpty())
	{
		*password = SysAllocString(reinterpret_cast<const OLECHAR*>(m_password.utf16()));
		return S_OK;
	}

	emit passwordRequired(m_password);
	if (m_cancelFlag && m_cancelFlag->loadAcquire())
		return E_ABORT;

	*password = SysAllocString(reinterpret_cast<const OLECHAR*>(m_password.utf16()));
	return S_OK;
}
