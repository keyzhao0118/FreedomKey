#pragma once

#include "compressoptionsdialog.h"
#include "updatecallback.h"

#include <QThread>
#include <QStringList>
#include <QAtomicInt>
#include <QVector>

class CompressEngine : public QThread
{
	Q_OBJECT

public:
	explicit CompressEngine(const QStringList& inputPaths, const CompressOptions& options, QObject* parent = nullptr);

	void cancel();

signals:
	void compressionProgress(quint64 completed, quint64 total);
	void compressionFinished(bool success, const QString& errorMsg, qint64 elapsedMs);

protected:
	void run() override;

private:
	static QVector<FileItem> enumerateFiles(const QStringList& inputPaths);
	static void enumerateDir(const QString& dirPath, const QString& baseDir, QVector<FileItem>& items);
	static FileItem makeFileItem(const QString& fullPath, const QString& relativePath);

	QStringList m_inputPaths;
	CompressOptions m_options;
	QAtomicInt m_cancelFlag;
};
