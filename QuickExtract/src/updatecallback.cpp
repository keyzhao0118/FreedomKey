#include "updatecallback.h"
#include "instreamwrapper.h"

#include <QDir>

void UpdateCallback::init(const QVector<FileItem>& items, const QString& password)
{
	m_items = items;
	m_password = password;
}

STDMETHODIMP UpdateCallback::SetTotal(UInt64 total)
{
	m_totalSize = total;
	return S_OK;
}

STDMETHODIMP UpdateCallback::SetCompleted(const UInt64* completeValue)
{
	if (completeValue)
		emit progressUpdated(*completeValue, m_totalSize);

	if (m_cancelFlag && m_cancelFlag->loadAcquire())
		return E_ABORT;

	return S_OK;
}

STDMETHODIMP UpdateCallback::GetUpdateItemInfo(UInt32 index, Int32* newData, Int32* newProps, UInt32* indexInArchive)
{
	if (newData)
		*newData = 1;
	if (newProps)
		*newProps = 1;
	if (indexInArchive)
		*indexInArchive = static_cast<UInt32>(-1);
	return S_OK;
}

STDMETHODIMP UpdateCallback::GetProperty(UInt32 index, PROPID propID, PROPVARIANT* value)
{
	if (index >= static_cast<UInt32>(m_items.size()))
		return E_INVALIDARG;

	PropVariantInit(value);
	const FileItem& item = m_items[index];

	switch (propID)
	{
	case kpidPath:
	{
		QString path = QDir::toNativeSeparators(item.relativePath);
		value->vt = VT_BSTR;
		value->bstrVal = SysAllocString(reinterpret_cast<const OLECHAR*>(path.utf16()));
		break;
	}
	case kpidIsDir:
		value->vt = VT_BOOL;
		value->boolVal = item.isDir ? VARIANT_TRUE : VARIANT_FALSE;
		break;
	case kpidSize:
		value->vt = VT_UI8;
		value->uhVal.QuadPart = item.size;
		break;
	case kpidAttrib:
		value->vt = VT_UI4;
		value->ulVal = item.attributes;
		break;
	case kpidCTime:
		value->vt = VT_FILETIME;
		value->filetime = item.cTime;
		break;
	case kpidMTime:
		value->vt = VT_FILETIME;
		value->filetime = item.mTime;
		break;
	case kpidATime:
		value->vt = VT_FILETIME;
		value->filetime = item.aTime;
		break;
	default:
		break;
	}

	return S_OK;
}

STDMETHODIMP UpdateCallback::GetStream(UInt32 index, ISequentialInStream** inStream)
{
	if (index >= static_cast<UInt32>(m_items.size()))
		return E_INVALIDARG;

	const FileItem& item = m_items[index];

	if (item.isDir)
	{
		*inStream = nullptr;
		return S_OK;
	}

	InStreamWrapper* streamSpec = new InStreamWrapper(item.fullDiskPath);
	CMyComPtr<ISequentialInStream> stream(streamSpec);

	if (!streamSpec->isOpen())
		return S_FALSE;

	*inStream = stream.Detach();
	return S_OK;
}

STDMETHODIMP UpdateCallback::SetOperationResult(Int32 operationResult)
{
	if (operationResult == S_OK)
		++m_successCount;
	else
		++m_errorCount;
	return S_OK;
}

STDMETHODIMP UpdateCallback::CryptoGetTextPassword2(Int32* passwordIsDefined, BSTR* password)
{
	if (!passwordIsDefined || !password)
		return E_INVALIDARG;

	if (m_password.isEmpty())
	{
		*passwordIsDefined = 0;
		*password = SysAllocString(L"");
	}
	else
	{
		*passwordIsDefined = 1;
		*password = SysAllocString(reinterpret_cast<const OLECHAR*>(m_password.utf16()));
	}
	return S_OK;
}
