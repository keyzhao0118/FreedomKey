#include "writebufferqueue.h"

WriteBufferQueue::WriteBufferQueue(qint64 maxBufferBytes)
	: m_maxBytes(maxBufferBytes)
{
}

void WriteBufferQueue::enqueue(const char* data, quint32 size)
{
	QByteArray block(data, static_cast<int>(size));

	QMutexLocker locker(&m_mutex);
	while (m_currentBytes + block.size() > m_maxBytes && !m_finished)
		m_notFull.wait(&m_mutex);

	if (m_finished)
		return;

	m_currentBytes += block.size();
	m_queue.enqueue(std::move(block));
	m_notEmpty.wakeOne();
}

QByteArray WriteBufferQueue::dequeue()
{
	QMutexLocker locker(&m_mutex);
	while (m_queue.isEmpty() && !m_finished)
		m_notEmpty.wait(&m_mutex);

	if (m_queue.isEmpty())
		return QByteArray();

	QByteArray block = m_queue.dequeue();
	m_currentBytes -= block.size();
	m_notFull.wakeOne();
	return block;
}

void WriteBufferQueue::markFinished()
{
	QMutexLocker locker(&m_mutex);
	m_finished = true;
	m_notFull.wakeAll();
	m_notEmpty.wakeAll();
}

void WriteBufferQueue::reset()
{
	QMutexLocker locker(&m_mutex);
	m_queue.clear();
	m_currentBytes = 0;
	m_finished = false;
}
