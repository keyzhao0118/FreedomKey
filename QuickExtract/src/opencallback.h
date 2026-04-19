#pragma once

#include <QObject>
#include <QAtomicInt>
#include <7zip/Archive/IArchive.h>
#include <7zip/IPassword.h>
#include <Common/MyCom.h>

class OpenCallback
	: public QObject
	, public IArchiveOpenCallback
	, public ICryptoGetTextPassword
	, public CMyUnknownImp
{
	Q_OBJECT
	Z7_COM_UNKNOWN_IMP_2(IArchiveOpenCallback, ICryptoGetTextPassword)

public:
	void setPassword(const QString& password) { m_password = password; }
	void setCancelFlag(QAtomicInt* flag) { m_cancelFlag = flag; }
	QString getPassword() const { return m_password; }

	// IArchiveOpenCallback
	STDMETHOD(SetTotal)(const UInt64* files, const UInt64* bytes) override;
	STDMETHOD(SetCompleted)(const UInt64* files, const UInt64* bytes) override;

	// ICryptoGetTextPassword
	STDMETHOD(CryptoGetTextPassword)(BSTR* password) override;

signals:
	void passwordRequired(QString& password);

private:
	QString m_password;
	QAtomicInt* m_cancelFlag = nullptr;
};
