#include "compressoptionsdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDialogButtonBox>

CompressOptionsDialog::CompressOptionsDialog(const QStringList& inputPaths, QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Compression Options"));
	setMinimumWidth(520);

	QFileInfo firstFile(inputPaths.first());
	m_outputDir = firstFile.absolutePath();
	QString defaultName = firstFile.completeBaseName();
	if (defaultName.isEmpty())
		defaultName = firstFile.fileName();

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setSpacing(12);

	auto* grid = new QGridLayout();
	grid->setSpacing(8);
	int row = 0;

	// Output directory
	grid->addWidget(new QLabel(tr("Save to:")), row, 0);
	auto* dirRow = new QHBoxLayout();
	m_dirEdit = new QLineEdit(QDir::toNativeSeparators(m_outputDir));
	m_dirEdit->setReadOnly(true);
	dirRow->addWidget(m_dirEdit, 1);
	auto* browseBtn = new QPushButton(tr("Browse..."));
	browseBtn->setFixedWidth(80);
	connect(browseBtn, &QPushButton::clicked, this, &CompressOptionsDialog::onBrowseDir);
	dirRow->addWidget(browseBtn);
	grid->addLayout(dirRow, row, 1);
	++row;

	// Filename + extension
	grid->addWidget(new QLabel(tr("Filename:")), row, 0);
	auto* nameRow = new QHBoxLayout();
	m_nameEdit = new QLineEdit(defaultName);
	nameRow->addWidget(m_nameEdit, 1);
	m_extLabel = new QLabel(".7z");
	m_extLabel->setFixedWidth(32);
	nameRow->addWidget(m_extLabel);
	grid->addLayout(nameRow, row, 1);
	++row;

	// Format
	grid->addWidget(new QLabel(tr("Format:")), row, 0);
	m_formatCombo = new QComboBox();
	m_formatCombo->addItem("7z");
	m_formatCombo->addItem("zip");
	connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, [this]() { onFormatChanged(); });
	grid->addWidget(m_formatCombo, row, 1);
	++row;

	// Encryption checkbox
	grid->addWidget(new QLabel(tr("Encryption:")), row, 0);
	m_encryptCheck = new QCheckBox(tr("Encrypt archive"));
	connect(m_encryptCheck, &QCheckBox::toggled, this, &CompressOptionsDialog::onEncryptToggled);
	grid->addWidget(m_encryptCheck, row, 1);
	++row;

	// Password
	grid->addWidget(new QLabel(tr("Password:")), row, 0);
	m_passwordEdit = new QLineEdit();
	m_passwordEdit->setEchoMode(QLineEdit::Password);
	m_passwordEdit->setEnabled(false);
	grid->addWidget(m_passwordEdit, row, 1);
	++row;

	// Header encryption (7z only)
	grid->addWidget(new QLabel(), row, 0);
	m_headerEncryptCheck = new QCheckBox(tr("Encrypt file headers (7z only)"));
	m_headerEncryptCheck->setEnabled(false);
	grid->addWidget(m_headerEncryptCheck, row, 1);
	++row;

	mainLayout->addLayout(grid);
	mainLayout->addStretch();

	auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Compress"));
	connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	mainLayout->addWidget(buttonBox);
}

CompressOptions CompressOptionsDialog::options() const
{
	CompressOptions opts;
	QString ext = m_extLabel->text();
	opts.outputPath = m_outputDir + QDir::separator() + m_nameEdit->text() + ext;
	opts.format = m_formatCombo->currentText();
	opts.password = m_encryptCheck->isChecked() ? m_passwordEdit->text() : QString();
	opts.encryptHeaders = m_headerEncryptCheck->isChecked() && m_headerEncryptCheck->isEnabled();
	return opts;
}

void CompressOptionsDialog::onBrowseDir()
{
	QString dir = QFileDialog::getExistingDirectory(this,
		tr("Select output directory"), m_outputDir);
	if (!dir.isEmpty())
	{
		m_outputDir = dir;
		m_dirEdit->setText(QDir::toNativeSeparators(dir));
	}
}

void CompressOptionsDialog::onFormatChanged()
{
	QString format = m_formatCombo->currentText();
	m_extLabel->setText("." + format);

	bool is7z = (format == "7z");
	m_headerEncryptCheck->setEnabled(is7z && m_encryptCheck->isChecked());
	if (!is7z)
		m_headerEncryptCheck->setChecked(false);
}

void CompressOptionsDialog::onEncryptToggled(bool checked)
{
	m_passwordEdit->setEnabled(checked);
	if (!checked)
	{
		m_passwordEdit->clear();
		m_headerEncryptCheck->setChecked(false);
	}

	bool is7z = (m_formatCombo->currentText() == "7z");
	m_headerEncryptCheck->setEnabled(checked && is7z);
}
