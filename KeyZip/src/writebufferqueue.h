#pragma once

#include <QByteArray>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>

class WriteBufferQueue
{
public:
	explicit WriteBufferQueue(qint64 maxBufferBytes = 4 * 1024 * 1024);

	void enqueue(const char* data, quint32 size);
	QByteArray dequeue();
	void markFinished();
	void reset();

private:
	QQueue<QByteArray> m_queue;
	QMutex m_mutex;
	QWaitCondition m_notFull;
	QWaitCondition m_notEmpty;
	qint64 m_currentBytes = 0;
	qint64 m_maxBytes;
	bool m_finished = false;
};
