#pragma once

#include <QThread>
#include <QVector>
#include <QAtomicInteger>

struct IInArchive;

class ArchiveExtractor : public QThread
{
	Q_OBJECT

public:
	explicit ArchiveExtractor(QObject* parent = nullptr);
	~ArchiveExtractor();

	void extractArchive(const QString& archivePath, const QString& entryPath, const QString& destDirPath);

signals:
	void requirePassword(bool& bCancel, QString& password);
	void updateProgress(quint64 completed, quint64 total);

	void extractFailed();
	void extractSucceed();
	void extractCanceled();

protected:
	void run() override;

private:
	bool detectSolid(IInArchive* archive);
	void collectFileIndices(IInArchive* archive, bool isSolid,
		QVector<quint32>& outIndices, quint64& outTotalSize);

	QString m_archivePath;
	QString m_entryPath;
	QString m_destDirPath;
	QString m_password;
};
