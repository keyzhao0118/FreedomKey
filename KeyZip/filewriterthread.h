#pragma once

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <windows.h>

class WriteBufferQueue;

struct FileMetadata
{
	FILETIME ctime = {};
	FILETIME atime = {};
	FILETIME mtime = {};
	DWORD attributes = 0;
	bool hasTime = false;
	bool hasAttributes = false;
	bool isDir = false;
};

class FileWriterThread : public QThread
{
	Q_OBJECT

public:
	explicit FileWriterThread(QObject* parent = nullptr);
	~FileWriterThread();

	void startWriteFile(WriteBufferQueue* queue, const QString& filePath, const FileMetadata& meta);
	void waitForCurrentFile();
	void stop();

protected:
	void run() override;

private:
	void applyMetadata(const QString& filePath, const FileMetadata& meta);

	QMutex m_mutex;
	QWaitCondition m_taskReady;
	QWaitCondition m_taskDone;

	WriteBufferQueue* m_currentQueue = nullptr;
	QString m_currentFilePath;
	FileMetadata m_currentMeta;
	bool m_hasTask = false;
	bool m_fileDone = true;
	bool m_stopping = false;
};
