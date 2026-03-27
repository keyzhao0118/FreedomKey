#include "opencallback.h"

STDMETHODIMP OpenCallback::SetTotal(const UInt64* files, const UInt64* bytes)
{
	return S_OK;
}

STDMETHODIMP OpenCallback::SetCompleted(const UInt64* files, const UInt64* bytes)
{
	if (m_cancelFlag && m_cancelFlag->loadAcquire())
		return E_ABORT;

	return S_OK;
}

STDMETHODIMP OpenCallback::CryptoGetTextPassword(BSTR* password)
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
