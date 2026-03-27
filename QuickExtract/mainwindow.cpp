#include "mainwindow.h"
#include "extractengine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
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
	setMinimumSize(480, 300);
	setAcceptDrops(true);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(20, 16, 20, 16);
	mainLayout->setSpacing(12);

	m_dropLabel = new QLabel();
	m_dropLabel->setAlignment(Qt::AlignCenter);
	m_dropLabel->setMinimumHeight(160);
	m_dropLabel->setText(tr("Drop archive files here\nor click to select\n\nSupported: ZIP, 7Z, RAR"));
	m_dropLabel->setCursor(Qt::PointingHandCursor);
	m_dropLabel->installEventFilter(this);
	setDropHighlight(false);
	mainLayout->addWidget(m_dropLabel, 1);

	m_progressSection = new QWidget();
	auto* progressLayout = new QVBoxLayout(m_progressSection);
	progressLayout->setContentsMargins(0, 8, 0, 0);
	progressLayout->setSpacing(6);

	m_archiveLabel = new QLabel();
	m_cancelButton = new QPushButton(tr("Cancel"));
	m_cancelButton->setFixedHeight(32);
	connect(m_cancelButton, &QPushButton::clicked, this, &MainWindow::cancelExtraction);

	auto* labLayout = new QHBoxLayout();
	labLayout->addWidget(m_archiveLabel);
	labLayout->addStretch();
	labLayout->addWidget(m_cancelButton);
	progressLayout->addLayout(labLayout);

	m_progressBar = new QProgressBar();
	m_progressBar->setRange(0, 1000);
	m_progressBar->setValue(0);
	m_progressBar->setTextVisible(true);
	progressLayout->addWidget(m_progressBar);

	mainLayout->addWidget(m_progressSection);
	m_progressSection->hide();

	setupTrayIcon();
}

MainWindow::~MainWindow()
{
	stopExtraction();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
	if (obj == m_dropLabel && event->type() == QEvent::MouseButtonRelease && !m_extracting)
	{
		selectFiles();
		return true;
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

void MainWindow::setDropHighlight(bool highlight)
{
	if (highlight)
	{
		m_dropLabel->setStyleSheet(
			"QLabel { border: 2px solid #0078d7; border-radius: 8px; "
			"background: #e8f0fe; color: #0078d7; font-size: 14px; }");
	}
	else
	{
		m_dropLabel->setStyleSheet(
			"QLabel { border: 2px dashed #aaa; border-radius: 8px; "
			"background: #f8f8f8; color: #666; font-size: 14px; }");
	}
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
	if (!m_extracting && event->mimeData()->hasUrls())
	{
		event->acceptProposedAction();
		setDropHighlight(true);
	}
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent* event)
{
	Q_UNUSED(event);
	setDropHighlight(false);
}

void MainWindow::dropEvent(QDropEvent* event)
{
	setDropHighlight(false);

	if (m_extracting)
		return;

	QStringList paths;
	for (const QUrl& url : event->mimeData()->urls())
	{
		if (url.isLocalFile())
			paths.append(url.toLocalFile());
	}

	if (!paths.isEmpty())
		startExtraction(paths);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	if (m_trayIcon && m_trayIcon->isVisible())
	{
		hide();
		event->ignore();
	}
}

void MainWindow::selectFiles()
{
	if (m_extracting)
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

	m_engine = new ExtractEngine(archivePaths, targetDir, this);

	connect(m_engine, &ExtractEngine::archiveStarted,
		this, [this](int index, int total, const QString& name) {
			m_archiveLabel->setText(tr("Extracting: %1 (%2/%3)")
				.arg(name).arg(index + 1).arg(total));
			m_progressBar->setValue(0);
		});

	connect(m_engine, &ExtractEngine::archiveProgress,
		this, [this](quint64 completed, quint64 total) {
			if (total > 0)
				m_progressBar->setValue(static_cast<int>(completed * 1000 / total));
		});

	connect(m_engine, &ExtractEngine::archiveFinished,
		this, [this](const QString& archiveName, bool success, const QString& error, qint64 elapsedMs) {
			if (success)
			{
				m_progressBar->setValue(1000);
				m_extractionLog.append(tr("[OK] %1 - Succeeded (%2)")
					.arg(archiveName, formatElapsed(elapsedMs)));
			}
			else
			{
				m_extractionLog.append(tr("[FAIL] %1 - %2 (%3)")
					.arg(archiveName, error, formatElapsed(elapsedMs)));
			}
		});

	connect(m_engine, &ExtractEngine::passwordRequired,
		this, [this](QString& password) {
			onPasswordRequired(password);
		});

	connect(m_engine, &ExtractEngine::finished,
		this, [this]() {
			m_engine->wait();
			m_engine->deleteLater();
			m_engine = nullptr;
			setExtracting(false);
			if (m_quitting)
				qApp->quit();
			else
				showExtractionSummary();
		});

	m_engine->start();
}

void MainWindow::cancelExtraction()
{
	if (m_engine)
	{
		m_cancelButton->setEnabled(false);
		m_engine->cancel();
	}
}

void MainWindow::stopExtraction()
{
	if (m_engine)
	{
		m_engine->cancel();
		if (m_passwordDialog)
			m_passwordDialog->reject();
		m_engine->wait();
	}
}

void MainWindow::exitApplication()
{
	m_quitting = true;

	if (m_engine)
	{
		m_engine->cancel();
		if (m_passwordDialog)
			m_passwordDialog->reject();
	}
	else
	{
		qApp->quit();
	}
}

void MainWindow::setExtracting(bool extracting)
{
	m_extracting = extracting;
	m_cancelButton->setVisible(extracting);
	m_cancelButton->setEnabled(extracting);

	if (extracting)
	{
		m_progressSection->show();
		m_dropLabel->setText(tr("Extraction in progress..."));
		m_dropLabel->setCursor(Qt::ArrowCursor);
	}
	else
	{
		m_progressSection->hide();
		m_dropLabel->setText(tr("Drop archive files here\nor click to select\n\nSupported: ZIP, 7Z, RAR"));
		m_dropLabel->setCursor(Qt::PointingHandCursor);
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
		if (m_engine)
			m_engine->cancel();
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
