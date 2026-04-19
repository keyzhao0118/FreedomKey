#pragma once

#include <QObject>
#include <QVector>
#include <QAtomicInt>
#include <7zip/Archive/IArchive.h>
#include <7zip/IPassword.h>
#include <Common/MyCom.h>
#include <QString>

struct FileItem
{
	QString relativePath;
	QString fullDiskPath;
	bool isDir;
	quint64 size;
	quint32 attributes;
	FILETIME cTime;
	FILETIME mTime;
	FILETIME aTime;
};

class UpdateCallback
	: public QObject
	, public IArchiveUpdateCallback
	, public ICryptoGetTextPassword2
	, public CMyUnknownImp
{
	Q_OBJECT
	Z7_COM_UNKNOWN_IMP_2(IArchiveUpdateCallback, ICryptoGetTextPassword2)

public:
	void init(const QVector<FileItem>& items, const QString& password);
	void setCancelFlag(QAtomicInt* flag) { m_cancelFlag = flag; }

	int errorCount() const { return m_errorCount; }
	int successCount() const { return m_successCount; }

	// IProgress
	STDMETHOD(SetTotal)(UInt64 total) override;
	STDMETHOD(SetCompleted)(const UInt64* completeValue) override;

	// IArchiveUpdateCallback
	STDMETHOD(GetUpdateItemInfo)(UInt32 index, Int32* newData, Int32* newProps, UInt32* indexInArchive) override;
	STDMETHOD(GetProperty)(UInt32 index, PROPID propID, PROPVARIANT* value) override;
	STDMETHOD(GetStream)(UInt32 index, ISequentialInStream** inStream) override;
	STDMETHOD(SetOperationResult)(Int32 operationResult) override;

	// ICryptoGetTextPassword2
	STDMETHOD(CryptoGetTextPassword2)(Int32* passwordIsDefined, BSTR* password) override;

signals:
	void progressUpdated(quint64 completed, quint64 total);

private:
	QVector<FileItem> m_items;
	QString m_password;
	UInt64 m_totalSize = 0;
	QAtomicInt* m_cancelFlag = nullptr;
	int m_errorCount = 0;
	int m_successCount = 0;
};
