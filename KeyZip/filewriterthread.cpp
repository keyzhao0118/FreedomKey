#include "filewriterthread.h"
#include "writebufferqueue.h"
#include "commonhelper.h"
#include <QFile>

FileWriterThread::FileWriterThread(QObject* parent)
	: QThread(parent)
{
}

FileWriterThread::~FileWriterThread()
{
	stop();
}

void FileWriterThread::startWriteFile(WriteBufferQueue* queue, const QString& filePath, const FileMetadata& meta)
{
	QMutexLocker locker(&m_mutex);

	while (!m_fileDone && !m_stopping)
		m_taskDone.wait(&m_mutex);

	m_currentQueue = queue;
	m_currentFilePath = filePath;
	m_currentMeta = meta;
	m_hasTask = true;
	m_fileDone = false;
	m_taskReady.wakeOne();
}

void FileWriterThread::waitForCurrentFile()
{
	QMutexLocker locker(&m_mutex);
	while (!m_fileDone && !m_stopping)
		m_taskDone.wait(&m_mutex);
}

void FileWriterThread::stop()
{
	{
		QMutexLocker locker(&m_mutex);
		m_stopping = true;
		m_taskReady.wakeAll();
		m_taskDone.wakeAll();
	}
	if (!wait(5000))
		CommonHelper::LogKeyZipDebugMsg("FileWriterThread: Failed to stop within 5 seconds.");
}

void FileWriterThread::run()
{
	while (true)
	{
		WriteBufferQueue* queue = nullptr;
		QString filePath;
		FileMetadata meta;

		{
			QMutexLocker locker(&m_mutex);
			while (!m_hasTask && !m_stopping)
				m_taskReady.wait(&m_mutex);

			if (m_stopping)
				return;

			queue = m_currentQueue;
			filePath = m_currentFilePath;
			meta = m_currentMeta;
			m_hasTask = false;
		}

		QFile file(filePath);
		if (file.open(QIODevice::WriteOnly))
		{
			while (true)
			{
				QByteArray block = queue->dequeue();
				if (block.isEmpty())
					break;
				file.write(block);
			}
			file.close();
			applyMetadata(filePath, meta);
		}
		else
		{
			CommonHelper::LogKeyZipDebugMsg("FileWriterThread: Failed to open file: " + filePath);
			while (!queue->dequeue().isEmpty()) {}
		}

		{
			QMutexLocker locker(&m_mutex);
			m_fileDone = true;
			m_taskDone.wakeAll();
		}
	}
}

void FileWriterThread::applyMetadata(const QString& filePath, const FileMetadata& meta)
{
	if (meta.hasTime)
	{
		DWORD flags = meta.isDir ? FILE_FLAG_BACKUP_SEMANTICS : 0;
		HANDLE h = CreateFileW(
			reinterpret_cast<LPCWSTR>(filePath.utf16()),
			GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, flags, nullptr);
		if (h != INVALID_HANDLE_VALUE)
		{
			const FILETIME* pCT = (meta.ctime.dwLowDateTime || meta.ctime.dwHighDateTime) ? &meta.ctime : nullptr;
			const FILETIME* pAT = (meta.atime.dwLowDateTime || meta.atime.dwHighDateTime) ? &meta.atime : nullptr;
			const FILETIME* pMT = (meta.mtime.dwLowDateTime || meta.mtime.dwHighDateTime) ? &meta.mtime : nullptr;
			SetFileTime(h, pCT, pAT, pMT);
			CloseHandle(h);
		}
	}

	if (meta.hasAttributes)
		SetFileAttributesW(reinterpret_cast<LPCWSTR>(filePath.utf16()), meta.attributes);
}
