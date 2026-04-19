#include "extractcallback.h"
#include "outstreamwrapper.h"

#include <QDir>
#include <QFileInfo>

void ExtractCallback::init(IInArchive* archive, const QString& destDirPath, const QString& password)
{
	m_archive = archive;
	m_destDirPath = QDir::toNativeSeparators(destDirPath);
	m_password = password;
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
	if (askExtractMode != NArchive::NExtract::NAskMode::kExtract)
		return S_OK;

	PROPVARIANT propPath;	PropVariantInit(&propPath);
	PROPVARIANT propIsDir;	PropVariantInit(&propIsDir);
	m_archive->GetProperty(index, kpidPath, &propPath);
	m_archive->GetProperty(index, kpidIsDir, &propIsDir);

	QString path = QDir::toNativeSeparators(QString::fromWCharArray(propPath.bstrVal));
	QString currentFullPath = m_destDirPath + QDir::separator() + path;
	bool currentIsDir = propIsDir.boolVal != VARIANT_FALSE;

	PropVariantClear(&propPath);
	PropVariantClear(&propIsDir);

	if (currentIsDir)
	{
		if (!QDir().exists(currentFullPath))
			QDir().mkpath(currentFullPath);
		return S_OK;
	}

	const QString parentDir = QFileInfo(currentFullPath).absolutePath();
	if (!QDir().exists(parentDir))
		QDir().mkpath(parentDir);

	OutStreamWrapper* outStreamSpec = new OutStreamWrapper(currentFullPath);
	CMyComPtr<ISequentialOutStream> sequentialOutStream(outStreamSpec);

	*outStream = sequentialOutStream.Detach();
	return S_OK;
}

STDMETHODIMP ExtractCallback::PrepareOperation(Int32 askExtractMode)
{
	return S_OK;
}

STDMETHODIMP ExtractCallback::SetOperationResult(Int32 opRes)
{
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
