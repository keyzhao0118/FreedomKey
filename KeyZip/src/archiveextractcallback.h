#pragma once

#include <7zip/Archive/IArchive.h>
#include <7zip/IPassword.h>
#include <Common/MyCom.h>
#include <QString>
#include <QAtomicInteger>

class FileWriterThread;
class WriteBufferQueue;

class ArchiveExtractCallBack
	: public IArchiveExtractCallback
	, public ICryptoGetTextPassword
	, public CMyUnknownImp
{
	Z7_COM_UNKNOWN_IMP_2(IArchiveExtractCallback, ICryptoGetTextPassword)

public:
	void init(IInArchive* archive, const QString& entryPath, const QString& destDirPath,
		const QString& password, FileWriterThread* writerThread,
		QAtomicInteger<quint64>* progressCounter, QAtomicInt* interruptionFlag);

	// IArchiveExtractCallback
	STDMETHOD(SetTotal)(const UInt64 size) override;
	STDMETHOD(SetCompleted)(const UInt64* completedSize) override;
	STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode) override;
	STDMETHOD(PrepareOperation)(Int32 askExtractMode) override;
	STDMETHOD(SetOperationResult)(Int32 opRes) override;

	// ICryptoGetTextPassword
	STDMETHOD(CryptoGetTextPassword)(BSTR* password) override;

private:
	CMyComPtr<IInArchive> m_archive;
	QString m_entryPath;
	QString m_destDirPath;
	QString m_password;

	FileWriterThread* m_writerThread = nullptr;
	WriteBufferQueue* m_currentBuffer = nullptr;
	QAtomicInteger<quint64>* m_progressCounter = nullptr;
	QAtomicInt* m_interruptionFlag = nullptr;

	UInt32 m_currentIndex = static_cast<UInt32>(-1);
	QString m_currentFullPath;
	bool m_currentIsDir = false;
	bool m_bSkipCurrent = false;
};
