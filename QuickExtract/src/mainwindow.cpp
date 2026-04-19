#include "mainwindow.h"
#include "extractengine.h"
#include "compressengine.h"
#include "compressoptionsdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QApplication>
#include <QDialog>
#include <QTextEdit>

MainWindow::MainWindow(QWidget* parent)
	: QWidget(parent)
{
	setWindowTitle("QuickExtract");
	setMinimumSize(480, 340);
	setAcceptDrops(true);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(12, 10, 12, 12);
	mainLayout->setSpacing(0);

	m_tabWidget = new QTabWidget();

	// --- Extract tab ---
	auto* extractTab = new QWidget();
	auto* extractLayout = new QVBoxLayout(extractTab);
	extractLayout->setContentsMargins(12, 12, 12, 12);
	extractLayout->setSpacing(12);

	m_extractDropLabel = new QLabel();
	m_extractDropLabel->setAlignment(Qt::AlignCenter);
	m_extractDropLabel->setMinimumHeight(160);
	m_extractDropLabel->setText(tr("Drop archive files here\nor click to select\n\nSupported: ZIP, 7Z, RAR"));
	m_extractDropLabel->setCursor(Qt::PointingHandCursor);
	m_extractDropLabel->installEventFilter(this);
	setDropHighlight(m_extractDropLabel, false);
	extractLayout->addWidget(m_extractDropLabel, 1);

	m_extractProgressSection = new QWidget();
	auto* extractProgressLayout = new QVBoxLayout(m_extractProgressSection);
	extractProgressLayout->setContentsMargins(0, 8, 0, 0);
	extractProgressLayout->setSpacing(6);

	m_extractArchiveLabel = new QLabel();
	m_extractCancelButton = new QPushButton(tr("Cancel"));
	m_extractCancelButton->setFixedHeight(32);
	connect(m_extractCancelButton, &QPushButton::clicked, this, &MainWindow::cancelExtraction);

	auto* extractLabLayout = new QHBoxLayout();
	extractLabLayout->addWidget(m_extractArchiveLabel);
	extractLabLayout->addStretch();
	extractLabLayout->addWidget(m_extractCancelButton);
	extractProgressLayout->addLayout(extractLabLayout);

	m_extractProgressBar = new QProgressBar();
	m_extractProgressBar->setRange(0, 1000);
	m_extractProgressBar->setValue(0);
	m_extractProgressBar->setTextVisible(true);
	extractProgressLayout->addWidget(m_extractProgressBar);

	extractLayout->addWidget(m_extractProgressSection);
	m_extractProgressSection->hide();

	m_tabWidget->addTab(extractTab, tr("Extract"));

	// --- Compress tab ---
	auto* compressTab = new QWidget();
	auto* compressLayout = new QVBoxLayout(compressTab);
	compressLayout->setContentsMargins(12, 12, 12, 12);
	compressLayout->setSpacing(12);

	m_compressDropLabel = new QLabel();
	m_compressDropLabel->setAlignment(Qt::AlignCenter);
	m_compressDropLabel->setMinimumHeight(160);
	m_compressDropLabel->setText(tr("Drop files here to compress\nor click to select"));
	m_compressDropLabel->setCursor(Qt::PointingHandCursor);
	m_compressDropLabel->installEventFilter(this);
	setDropHighlight(m_compressDropLabel, false);
	compressLayout->addWidget(m_compressDropLabel, 1);

	m_compressProgressSection = new QWidget();
	auto* compressProgressLayout = new QVBoxLayout(m_compressProgressSection);
	compressProgressLayout->setContentsMargins(0, 8, 0, 0);
	compressProgressLayout->setSpacing(6);

	m_compressStatusLabel = new QLabel();
	m_compressCancelButton = new QPushButton(tr("Cancel"));
	m_compressCancelButton->setFixedHeight(32);
	connect(m_compressCancelButton, &QPushButton::clicked, this, &MainWindow::cancelCompression);

	auto* compressLabLayout = new QHBoxLayout();
	compressLabLayout->addWidget(m_compressStatusLabel);
	compressLabLayout->addStretch();
	compressLabLayout->addWidget(m_compressCancelButton);
	compressProgressLayout->addLayout(compressLabLayout);

	m_compressProgressBar = new QProgressBar();
	m_compressProgressBar->setRange(0, 1000);
	m_compressProgressBar->setValue(0);
	m_compressProgressBar->setTextVisible(true);
	compressProgressLayout->addWidget(m_compressProgressBar);

	compressLayout->addWidget(m_compressProgressSection);
	m_compressProgressSection->hide();

	m_tabWidget->addTab(compressTab, tr("Compress"));

	mainLayout->addWidget(m_tabWidget);

	setupTrayIcon();
}

MainWindow::~MainWindow()
{
	stopExtraction();
	stopCompression();
}

bool MainWindow::isBusy() const
{
	return m_extracting || m_compressing;
}

int MainWindow::currentTab() const
{
	return m_tabWidget->currentIndex();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
	if (event->type() == QEvent::MouseButtonRelease && !isBusy())
	{
		if (obj == m_extractDropLabel)
		{
			selectExtractFiles();
			return true;
		}
		if (obj == m_compressDropLabel)
		{
			selectCompressFiles();
			return true;
		}
	}
	return QWidget::eventFilter(obj, event);
}

void MainWindow::setupTrayIcon()
{
	m_trayIcon = new QSystemTrayIcon(this);

	QPixmap pix(32, 32);
	pix.fill(QColor(0, 120, 215));
	QIcon appIcon(pix);

	m_trayIcon->setIcon(appIcon);
	m_trayIcon->setToolTip("QuickExtract");
	setWindowIcon(appIcon);

	auto* menu = new QMenu(this);
	menu->addAction(tr("Show"), this, [this]() {
		show();
		raise();
		activateWindow();
	});
	menu->addSeparator();
	menu->addAction(tr("Exit"), this, &MainWindow::exitApplication);
	m_trayIcon->setContextMenu(menu);

	connect(m_trayIcon, &QSystemTrayIcon::activated,
		this, [this](QSystemTrayIcon::ActivationReason reason) {
			if (reason == QSystemTrayIcon::DoubleClick)
			{
				if (isVisible())
					hide();
				else
				{
					show();
					raise();
					activateWindow();
				}
			}
		});

	m_trayIcon->show();
}

void MainWindow::setDropHighlight(QLabel* label, bool highlight)
{
	if (highlight)
	{
		label->setStyleSheet(
			"QLabel { border: 2px solid #0078d7; border-radius: 8px; "
			"background: #e8f0fe; color: #0078d7; font-size: 14px; }");
	}
	else
	{
		label->setStyleSheet(
			"QLabel { border: 2px dashed #aaa; border-radius: 8px; "
			"background: #f8f8f8; color: #666; font-size: 14px; }");
	}
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
	if (!isBusy() && event->mimeData()->hasUrls())
	{
		event->acceptProposedAction();
		if (currentTab() == 0)
			setDropHighlight(m_extractDropLabel, true);
		else
			setDropHighlight(m_compressDropLabel, true);
	}
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent* event)
{
	Q_UNUSED(event);
	setDropHighlight(m_extractDropLabel, false);
	setDropHighlight(m_compressDropLabel, false);
}

void MainWindow::dropEvent(QDropEvent* event)
{
	setDropHighlight(m_extractDropLabel, false);
	setDropHighlight(m_compressDropLabel, false);

	if (isBusy())
		return;

	QStringList paths;
	for (const QUrl& url : event->mimeData()->urls())
	{
		if (url.isLocalFile())
			paths.append(url.toLocalFile());
	}

	if (paths.isEmpty())
	{
		event->ignore();
		return;
	}

	event->acceptProposedAction();

	const int tab = currentTab();
	QMetaObject::invokeMethod(this, [this, paths, tab]() {
		if (isBusy())
			return;
		if (tab == 0)
			startExtraction(paths);
		else
			startCompression(paths);
		}, Qt::QueuedConnection);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	if (m_trayIcon && m_trayIcon->isVisible())
	{
		hide();
		event->ignore();
	}
}

// ===================== Extract =====================

void MainWindow::selectExtractFiles()
{
	if (isBusy())
		return;

	QStringList files = QFileDialog::getOpenFileNames(this,
		tr("Select archive files"),
		QString(),
		tr("Archive files (*.zip *.7z *.rar);;All files (*.*)"));

	if (!files.isEmpty())
		startExtraction(files);
}

void MainWindow::startExtraction(const QStringList& archivePaths)
{
	QString targetDir = QFileDialog::getExistingDirectory(this,
		tr("Select extraction target folder"));

	if (targetDir.isEmpty())
		return;

	m_extractionLog.clear();
	setExtracting(true);

	m_extractEngine = new ExtractEngine(archivePaths, targetDir, this);

	connect(m_extractEngine, &ExtractEngine::archiveStarted,
		this, [this](int index, int total, const QString& name) {
			m_extractArchiveLabel->setText(tr("Extracting: %1 (%2/%3)")
				.arg(name).arg(index + 1).arg(total));
			m_extractProgressBar->setValue(0);
		});

	connect(m_extractEngine, &ExtractEngine::archiveProgress,
		this, [this](quint64 completed, quint64 total) {
			if (total > 0)
				m_extractProgressBar->setValue(static_cast<int>(completed * 1000 / total));
		});

	connect(m_extractEngine, &ExtractEngine::archiveFinished,
		this, [this](const QString& archiveName, bool success, const QString& error, qint64 elapsedMs) {
			if (success)
			{
				m_extractProgressBar->setValue(1000);
				m_extractionLog.append(tr("[OK] %1 - Succeeded (%2)")
					.arg(archiveName, formatElapsed(elapsedMs)));
			}
			else
			{
				m_extractionLog.append(tr("[FAIL] %1 - %2 (%3)")
					.arg(archiveName, error, formatElapsed(elapsedMs)));
			}
		});

	connect(m_extractEngine, &ExtractEngine::passwordRequired,
		this, [this](QString& password) {
			onPasswordRequired(password);
		});

	connect(m_extractEngine, &ExtractEngine::finished,
		this, [this]() {
			m_extractEngine->wait();
			m_extractEngine->deleteLater();
			m_extractEngine = nullptr;
			setExtracting(false);
			if (m_quitting)
				qApp->quit();
			else
				showExtractionSummary();
		});

	m_extractEngine->start();
}

void MainWindow::cancelExtraction()
{
	if (m_extractEngine)
	{
		m_extractCancelButton->setEnabled(false);
		m_extractEngine->cancel();
	}
}

void MainWindow::stopExtraction()
{
	if (m_extractEngine)
	{
		m_extractEngine->cancel();
		if (m_passwordDialog)
			m_passwordDialog->reject();
		m_extractEngine->wait();
	}
}

void MainWindow::setExtracting(bool extracting)
{
	m_extracting = extracting;
	m_extractCancelButton->setVisible(extracting);
	m_extractCancelButton->setEnabled(extracting);

	if (extracting)
	{
		m_extractProgressSection->show();
		m_extractDropLabel->setText(tr("Extraction in progress..."));
		m_extractDropLabel->setCursor(Qt::ArrowCursor);
	}
	else
	{
		m_extractProgressSection->hide();
		m_extractDropLabel->setText(tr("Drop archive files here\nor click to select\n\nSupported: ZIP, 7Z, RAR"));
		m_extractDropLabel->setCursor(Qt::PointingHandCursor);
	}
}

void MainWindow::onPasswordRequired(QString& password)
{
	QInputDialog dlg(this);
	dlg.setWindowTitle("QuickExtract");
	dlg.setLabelText(tr("This archive requires a password:"));
	dlg.setTextEchoMode(QLineEdit::Password);

	m_passwordDialog = &dlg;
	int result = dlg.exec();
	m_passwordDialog = nullptr;

	if (result != QDialog::Accepted || dlg.textValue().isEmpty())
	{
		if (m_extractEngine)
			m_extractEngine->cancel();
		return;
	}

	password = dlg.textValue();
}

void MainWindow::showExtractionSummary()
{
	if (m_extractionLog.isEmpty())
		return;

	QDialog dlg(this);
	dlg.setWindowTitle(tr("Extraction Summary"));
	dlg.setMinimumSize(520, 320);

	auto* layout = new QVBoxLayout(&dlg);

	auto* textEdit = new QTextEdit();
	textEdit->setReadOnly(true);
	textEdit->setPlainText(m_extractionLog.join('\n'));
	layout->addWidget(textEdit);

	auto* okButton = new QPushButton(tr("OK"));
	okButton->setFixedWidth(80);
	connect(okButton, &QPushButton::clicked, &dlg, &QDialog::accept);

	auto* btnLayout = new QHBoxLayout();
	btnLayout->addStretch();
	btnLayout->addWidget(okButton);
	layout->addLayout(btnLayout);

	dlg.exec();
	m_extractionLog.clear();
}

// ===================== Compress =====================

void MainWindow::selectCompressFiles()
{
	if (isBusy())
		return;

	QStringList files = QFileDialog::getOpenFileNames(this,
		tr("Select files to compress"),
		QString(),
		tr("All files (*.*)"));

	if (!files.isEmpty())
		startCompression(files);
}

void MainWindow::startCompression(const QStringList& paths)
{
	CompressOptionsDialog optsDlg(paths, this);
	if (optsDlg.exec() != QDialog::Accepted)
		return;

	CompressOptions opts = optsDlg.options();
	setCompressing(true);

	m_compressStatusLabel->setText(tr("Compressing..."));
	m_compressProgressBar->setValue(0);

	m_compressEngine = new CompressEngine(paths, opts, this);

	connect(m_compressEngine, &CompressEngine::compressionProgress,
		this, [this](quint64 completed, quint64 total) {
			if (total > 0)
				m_compressProgressBar->setValue(static_cast<int>(completed * 1000 / total));
		});

	connect(m_compressEngine, &CompressEngine::compressionFinished,
		this, [this](bool success, const QString& errorMsg, qint64 elapsedMs) {
			m_compressEngine->wait();
			m_compressEngine->deleteLater();
			m_compressEngine = nullptr;
			setCompressing(false);
			if (m_quitting)
				qApp->quit();
			else
				showCompressionSummary(success, errorMsg, elapsedMs);
		});

	m_compressEngine->start();
}

void MainWindow::cancelCompression()
{
	if (m_compressEngine)
	{
		m_compressCancelButton->setEnabled(false);
		m_compressEngine->cancel();
	}
}

void MainWindow::stopCompression()
{
	if (m_compressEngine)
	{
		m_compressEngine->cancel();
		m_compressEngine->wait();
	}
}

void MainWindow::setCompressing(bool compressing)
{
	m_compressing = compressing;
	m_compressCancelButton->setVisible(compressing);
	m_compressCancelButton->setEnabled(compressing);

	if (compressing)
	{
		m_compressProgressSection->show();
		m_compressDropLabel->setText(tr("Compression in progress..."));
		m_compressDropLabel->setCursor(Qt::ArrowCursor);
	}
	else
	{
		m_compressProgressSection->hide();
		m_compressDropLabel->setText(tr("Drop files here to compress\nor click to select"));
		m_compressDropLabel->setCursor(Qt::PointingHandCursor);
	}
}

void MainWindow::showCompressionSummary(bool success, const QString& errorMsg, qint64 elapsedMs)
{
	QDialog dlg(this);
	dlg.setWindowTitle(tr("Compression Summary"));
	dlg.setMinimumSize(420, 200);

	auto* layout = new QVBoxLayout(&dlg);

	auto* textEdit = new QTextEdit();
	textEdit->setReadOnly(true);

	if (success)
		textEdit->setPlainText(tr("[OK] Compression succeeded (%1)").arg(formatElapsed(elapsedMs)));
	else
		textEdit->setPlainText(tr("[FAIL] %1 (%2)").arg(errorMsg, formatElapsed(elapsedMs)));

	layout->addWidget(textEdit);

	auto* okButton = new QPushButton(tr("OK"));
	okButton->setFixedWidth(80);
	connect(okButton, &QPushButton::clicked, &dlg, &QDialog::accept);

	auto* btnLayout = new QHBoxLayout();
	btnLayout->addStretch();
	btnLayout->addWidget(okButton);
	layout->addLayout(btnLayout);

	dlg.exec();
}

// ===================== Common =====================

void MainWindow::exitApplication()
{
	m_quitting = true;

	if (m_extractEngine)
	{
		m_extractEngine->cancel();
		if (m_passwordDialog)
			m_passwordDialog->reject();
	}
	else if (m_compressEngine)
	{
		m_compressEngine->cancel();
	}
	else
	{
		qApp->quit();
	}
}

QString MainWindow::formatElapsed(qint64 ms)
{
	if (ms < 1000)
		return QString("%1 ms").arg(ms);

	double secs = ms / 1000.0;
	if (secs < 60.0)
		return QString("%1 s").arg(secs, 0, 'f', 1);

	int mins = static_cast<int>(secs) / 60;
	int remSecs = static_cast<int>(secs) % 60;
	return QString("%1 m %2 s").arg(mins).arg(remSecs);
}
