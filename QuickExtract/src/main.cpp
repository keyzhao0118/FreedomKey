#include <QApplication>
#include <QDir>
#include <QLockFile>
#include <QStandardPaths>

#include "mainwindow.h"

namespace
{
	QString singleInstanceLockPath()
	{
		QString lockDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
		if (lockDirectory.isEmpty())
			lockDirectory = QDir::tempPath();

		return QDir(lockDirectory).filePath("FreedomKey_QuickExtract.lock");
	}

	bool isSingleInstance(QLockFile& lockFile)
	{
		lockFile.setStaleLockTime(0);

		if (lockFile.tryLock())
			return true;

		return false;
	}
}

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QApplication::setQuitOnLastWindowClosed(false);
	QLockFile singleInstanceLock(singleInstanceLockPath());

	if (!isSingleInstance(singleInstanceLock))
		return 0;

	MainWindow w;
	w.show();

	return app.exec();
}
