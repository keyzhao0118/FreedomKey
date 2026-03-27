#pragma once

#include <QThread>
#include <QStringList>
#include <QAtomicInt>

class ExtractEngine : public QThread
{
	Q_OBJECT

public:
	explicit ExtractEngine(const QStringList& archivePaths, const QString& targetDir, QObject* parent = nullptr);

	void cancel();

signals:
	void archiveStarted(int index, int total, const QString& archiveName);
	void archiveProgress(quint64 completed, quint64 total);
	void archiveFinished(const QString& archiveName, bool success, const QString& errorMsg, qint64 elapsedMs);
	void allFinished();

	void passwordRequired(QString& password);

protected:
	void run() override;

private:
	QString computeDestDir(const QString& archivePath) const;

	QStringList m_archivePaths;
	QString m_targetDir;
	QAtomicInt m_cancelFlag;
};
