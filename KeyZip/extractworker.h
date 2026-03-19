#pragma once

#include <QRunnable>
#include <QString>
#include <QVector>
#include <QAtomicInteger>

class ExtractWorker : public QRunnable
{
public:
	ExtractWorker();
	~ExtractWorker();

	void setArchivePath(const QString& archivePath);
	void setEntryPath(const QString& entryPath);
	void setDestDirPath(const QString& destDirPath);
	void setPassword(const QString& password);
	void setIndices(const QVector<quint32>& indices);
	void setProgressCounter(QAtomicInteger<quint64>* counter);
	void setInterruptionFlag(QAtomicInt* flag);
	void setResultFlag(QAtomicInt* flag);

	void run() override;

private:
	QString m_archivePath;
	QString m_entryPath;
	QString m_destDirPath;
	QString m_password;
	QVector<quint32> m_indices;

	QAtomicInteger<quint64>* m_progressCounter = nullptr;
	QAtomicInt* m_interruptionFlag = nullptr;
	QAtomicInt* m_resultFlag = nullptr;
};
