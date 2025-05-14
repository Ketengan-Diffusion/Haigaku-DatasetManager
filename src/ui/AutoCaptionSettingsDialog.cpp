#include "AutoCaptionSettingsDialog.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel> // For messages if no settings for a model
#include <QDebug>

AutoCaptionSettingsDialog::AutoCaptionSettingsDialog(const QString &modelName, 
                                                     const QVariantMap &currentSettings, 
                                                     QWidget *parent)
    : QDialog(parent), m_currentSettings(currentSettings)
{
    setWindowTitle(tr("Advanced Settings for %1").arg(modelName));
    setMinimumWidth(350);
    setupUi(modelName);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &AutoCaptionSettingsDialog::onAccepted);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

AutoCaptionSettingsDialog::~AutoCaptionSettingsDialog()
{
}

void AutoCaptionSettingsDialog::setupUi(const QString &modelName)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Dynamically add settings based on modelName
    // For now, only implementing for "SmilingWolf/wd-vit-tagger-v3"
    if (modelName == "SmilingWolf/wd-vit-tagger-v3") {
        m_charTagsFirstCheckBox = new QCheckBox(tr("Character tags first"), this);
        m_charTagsFirstCheckBox->setChecked(m_currentSettings.value("char_tags_first", false).toBool());
        mainLayout->addWidget(m_charTagsFirstCheckBox);

        m_hideRatingTagsCheckBox = new QCheckBox(tr("Hide rating tags (safe, questionable, explicit)"), this);
        m_hideRatingTagsCheckBox->setChecked(m_currentSettings.value("hide_rating_tags", false).toBool());
        mainLayout->addWidget(m_hideRatingTagsCheckBox);

        m_removeSeparatorCheckBox = new QCheckBox(tr("Remove '_' from tags (use spaces)"), this); // Corrected text
        m_removeSeparatorCheckBox->setChecked(m_currentSettings.value("remove_separator", true).toBool()); // Default true
        mainLayout->addWidget(m_removeSeparatorCheckBox);
        
        // Add more settings specific to this model here if needed
    } else {
        mainLayout->addWidget(new QLabel(tr("No advanced settings available for this model."), this));
    }

    mainLayout->addStretch();

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    setLayout(mainLayout);
}

void AutoCaptionSettingsDialog::onAccepted()
{
    // Update m_currentSettings from UI elements before accepting
    // This needs to be model-specific as well
    if (m_charTagsFirstCheckBox) { // Check if UI elements were created
        m_currentSettings["char_tags_first"] = m_charTagsFirstCheckBox->isChecked();
    }
    if (m_hideRatingTagsCheckBox) {
        m_currentSettings["hide_rating_tags"] = m_hideRatingTagsCheckBox->isChecked();
    }
    if (m_removeSeparatorCheckBox) {
        m_currentSettings["remove_separator"] = m_removeSeparatorCheckBox->isChecked();
    }
    
    QDialog::accept();
}

QVariantMap AutoCaptionSettingsDialog::getSettings() const
{
    return m_currentSettings;
}
