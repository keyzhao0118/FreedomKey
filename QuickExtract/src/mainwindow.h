#pragma once

#include <QWidget>
#include <QSystemTrayIcon>
#include <QStringList>

class QLabel;
class QProgressBar;
class QPushButton;
class QDialog;
class QTabWidget;
class ExtractEngine;
class CompressEngine;

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
	bool isBusy() const;
	int currentTab() const;

	// Extract
	void selectExtractFiles();
	void startExtraction(const QStringList& archivePaths);
	void cancelExtraction();
	void stopExtraction();
	void setExtracting(bool extracting);
	void onPasswordRequired(QString& password);
	void showExtractionSummary();

	// Compress
	void selectCompressFiles();
	void startCompression(const QStringList& paths);
	void cancelCompression();
	void stopCompression();
	void setCompressing(bool compressing);
	void showCompressionSummary(bool success, const QString& errorMsg, qint64 elapsedMs);

	void exitApplication();
	void setupTrayIcon();
	void setDropHighlight(QLabel* label, bool highlight);

	static QString formatElapsed(qint64 ms);

	QTabWidget* m_tabWidget = nullptr;

	// Extract tab widgets
	QLabel* m_extractDropLabel = nullptr;
	QWidget* m_extractProgressSection = nullptr;
	QLabel* m_extractArchiveLabel = nullptr;
	QProgressBar* m_extractProgressBar = nullptr;
	QPushButton* m_extractCancelButton = nullptr;

	// Compress tab widgets
	QLabel* m_compressDropLabel = nullptr;
	QWidget* m_compressProgressSection = nullptr;
	QLabel* m_compressStatusLabel = nullptr;
	QProgressBar* m_compressProgressBar = nullptr;
	QPushButton* m_compressCancelButton = nullptr;

	QSystemTrayIcon* m_trayIcon = nullptr;

	ExtractEngine* m_extractEngine = nullptr;
	CompressEngine* m_compressEngine = nullptr;
	QDialog* m_passwordDialog = nullptr;
	bool m_extracting = false;
	bool m_compressing = false;
	bool m_quitting = false;
	QStringList m_extractionLog;
};
