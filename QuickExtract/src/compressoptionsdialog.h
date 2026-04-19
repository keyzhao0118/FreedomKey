#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;

struct CompressOptions
{
	QString outputPath;
	QString format;
	QString password;
	bool encryptHeaders = false;
};

class CompressOptionsDialog : public QDialog
{
	Q_OBJECT

public:
	explicit CompressOptionsDialog(const QStringList& inputPaths, QWidget* parent = nullptr);

	CompressOptions options() const;

private:
	void onBrowseDir();
	void onFormatChanged();
	void onEncryptToggled(bool checked);
	void updateOutputPath();

	QLineEdit* m_dirEdit = nullptr;
	QLineEdit* m_nameEdit = nullptr;
	QLabel* m_extLabel = nullptr;
	QComboBox* m_formatCombo = nullptr;
	QCheckBox* m_encryptCheck = nullptr;
	QLineEdit* m_passwordEdit = nullptr;
	QCheckBox* m_headerEncryptCheck = nullptr;

	QString m_outputDir;
};
