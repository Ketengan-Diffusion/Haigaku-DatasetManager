#ifndef AUTOCAPTIONSETTINGSDIALOG_H
#define AUTOCAPTIONSETTINGSDIALOG_H

#include <QDialog>
#include <QVariantMap> // To pass settings

QT_BEGIN_NAMESPACE
class QCheckBox;
class QVBoxLayout;
class QDialogButtonBox;
QT_END_NAMESPACE

class AutoCaptionSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // Constructor will take current settings and model name to populate dynamically
    explicit AutoCaptionSettingsDialog(const QString &modelName, 
                                       const QVariantMap &currentSettings, 
                                       QWidget *parent = nullptr);
    ~AutoCaptionSettingsDialog();

    QVariantMap getSettings() const;

private slots:
    void onAccepted();

private:
    void setupUi(const QString &modelName);

    // For wd-vit-tagger-v3
    QCheckBox *m_charTagsFirstCheckBox;
    QCheckBox *m_hideRatingTagsCheckBox;
    QCheckBox *m_removeSeparatorCheckBox;
    // Add more QWidgets for other models' settings as needed

    QDialogButtonBox *m_buttonBox;
    QVariantMap m_currentSettings;
};

#endif // AUTOCAPTIONSETTINGSDIALOG_H
