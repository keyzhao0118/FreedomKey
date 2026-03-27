#pragma once

#include <QWidget>
#include <QSystemTrayIcon>
#include <QStringList>

class QLabel;
class QProgressBar;
class QPushButton;
class QDialog;
class ExtractEngine;

class MainWindow : public QWidget
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow();

protected:
	bool eventFilter(QObject* obj, QEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragLeaveEvent(QDragLeaveEvent* event) override;
	void dropEvent(QDropEvent* event) override;
	void closeEvent(QCloseEvent* event) override;

private:
	void selectFiles();
	void startExtraction(const QStringList& archivePaths);
	void cancelExtraction();
	void stopExtraction();
	void exitApplication();
	void setExtracting(bool extracting);
	void onPasswordRequired(QString& password);
	void showExtractionSummary();
	void setupTrayIcon();
	void setDropHighlight(bool highlight);

	static QString formatElapsed(qint64 ms);

	QLabel* m_dropLabel = nullptr;

	QWidget* m_progressSection = nullptr;
	QLabel* m_archiveLabel = nullptr;
	QProgressBar* m_progressBar = nullptr;
	QPushButton* m_cancelButton = nullptr;

	QSystemTrayIcon* m_trayIcon = nullptr;

	ExtractEngine* m_engine = nullptr;
	QDialog* m_passwordDialog = nullptr;
	bool m_extracting = false;
	bool m_quitting = false;
	QStringList m_extractionLog;
};
