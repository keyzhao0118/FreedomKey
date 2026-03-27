#pragma once

#include <QObject>
#include <7zip/Archive/IArchive.h>
#include <7zip/IPassword.h>
#include <Common/MyCom.h>
#include <QString>
#include <QAtomicInt>

class FileWriterThread;
class WriteBufferQueue;

class ExtractCallback
	: public QObject
	, public IArchiveExtractCallback
	, public ICryptoGetTextPassword
	, public CMyUnknownImp
{
	Q_OBJECT
	Z7_COM_UNKNOWN_IMP_2(IArchiveExtractCallback, ICryptoGetTextPassword)

public:
	void init(IInArchive* archive, const QString& destDirPath,
		const QString& password, FileWriterThread* writerThread);

	void cleanupPendingBuffer();
	void setCancelFlag(QAtomicInt* flag) { m_cancelFlag = flag; }
	UInt64 totalSize() const { return m_totalSize; }
	int errorCount() const { return m_errorCount; }
	int successCount() const { return m_successCount; }
	Int32 firstFailureReason() const { return m_firstFailureReason; }

	// IArchiveExtractCallback
	STDMETHOD(SetTotal)(const UInt64 size) override;
	STDMETHOD(SetCompleted)(const UInt64* completedSize) override;
	STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode) override;
	STDMETHOD(PrepareOperation)(Int32 askExtractMode) override;
	STDMETHOD(SetOperationResult)(Int32 opRes) override;

	// ICryptoGetTextPassword
	STDMETHOD(CryptoGetTextPassword)(BSTR* password) override;

signals:
	void progressUpdated(quint64 completed, quint64 total);
	void passwordRequired(QString& password);

private:
	CMyComPtr<IInArchive> m_archive;
	QString m_destDirPath;
	QString m_password;

	FileWriterThread* m_writerThread = nullptr;
	WriteBufferQueue* m_currentBuffer = nullptr;

	UInt64 m_totalSize = 0;
	UInt32 m_currentIndex = static_cast<UInt32>(-1);
	QString m_currentFullPath;
	QAtomicInt* m_cancelFlag = nullptr;
	Int32 m_firstFailureReason = 0;
	int m_errorCount = 0;
	int m_successCount = 0;
	bool m_currentIsDir = false;
	bool m_bSkipCurrent = false;
};
