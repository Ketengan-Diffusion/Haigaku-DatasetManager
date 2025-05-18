#include "AutoCaptionSettingsPanel.h" // Renamed
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QButtonGroup>
#include <QStyle> 
#include <QDebug>
#include <QProgressBar> // Added

AutoCaptionSettingsPanel::AutoCaptionSettingsPanel(QWidget *parent) 
    : QWidget(parent)
    // Initialize members 
    , m_modelComboBox(nullptr)
    , m_loadModelButton(nullptr)
    , m_unloadModelButton(nullptr)
    , m_statusLabel(nullptr)
    // , m_captionModeGroupBox(nullptr) // Removed
    // , m_nlpModeRadioButton(nullptr) // Removed
    // , m_tagsModeRadioButton(nullptr) // Removed
    , m_deviceGroupBox(nullptr)
    , m_cpuRadioButton(nullptr)
    , m_gpuRadioButton(nullptr)
    , m_amdGpuCheckBox(nullptr)
    , m_enableSuggestionCheckBox(nullptr)
    , m_advancedSettingsButton(nullptr)
    , m_downloadStatusLabel(nullptr)    // Initialize new members
    , m_downloadProgressBar(nullptr)
{
    setupUI();
}

AutoCaptionSettingsPanel::~AutoCaptionSettingsPanel() // Renamed
{
}

void AutoCaptionSettingsPanel::setupUI() // Renamed
{
    QVBoxLayout *panelLayout = new QVBoxLayout(this); // Main layout for this panel
    panelLayout->setContentsMargins(5,5,5,5);

    // Model Selection
    QHBoxLayout *modelLayout = new QHBoxLayout();
    modelLayout->addWidget(new QLabel(tr("Model:"), this));
    m_modelComboBox = new QComboBox(this);
    m_modelComboBox->addItem("SmilingWolf/wd-vit-tagger-v3"); 
    modelLayout->addWidget(m_modelComboBox);
    panelLayout->addLayout(modelLayout);

    // Load/Unload Buttons
    QHBoxLayout *loadButtonsLayout = new QHBoxLayout();
    m_loadModelButton = new QPushButton(tr("Load Model"), this);
    connect(m_loadModelButton, &QPushButton::clicked, this, [this](){
        emit loadModelClicked(m_modelComboBox->currentText());
    });
    m_unloadModelButton = new QPushButton(tr("Unload Model"), this);
    m_unloadModelButton->setEnabled(false); 
    connect(m_unloadModelButton, &QPushButton::clicked, this, &AutoCaptionSettingsPanel::unloadModelClicked);
    loadButtonsLayout->addWidget(m_loadModelButton);
    loadButtonsLayout->addWidget(m_unloadModelButton);
    panelLayout->addLayout(loadButtonsLayout);
    
    // Status Label
    m_statusLabel = new QLabel(tr("Model: Unloaded"), this);
    m_statusLabel->setObjectName("statusLabel"); 
    m_statusLabel->setStyleSheet("color: red;"); 
    panelLayout->addWidget(m_statusLabel);

    // Download Progress UI (initially hidden)
    m_downloadStatusLabel = new QLabel(this);
    m_downloadStatusLabel->setVisible(false);
    panelLayout->addWidget(m_downloadStatusLabel);

    m_downloadProgressBar = new QProgressBar(this);
    m_downloadProgressBar->setVisible(false);
    m_downloadProgressBar->setTextVisible(true); // Show percentage text
    m_downloadProgressBar->setMinimum(0);
    m_downloadProgressBar->setValue(0);
    panelLayout->addWidget(m_downloadProgressBar);

    // Device Selection
    m_deviceGroupBox = new QGroupBox(tr("Device"), this);
    QHBoxLayout *deviceLayout = new QHBoxLayout();
    m_cpuRadioButton = new QRadioButton(tr("CPU"), m_deviceGroupBox);
    m_gpuRadioButton = new QRadioButton(tr("GPU"), m_deviceGroupBox);
    m_cpuRadioButton->setChecked(true); 
    deviceLayout->addWidget(m_cpuRadioButton);
    deviceLayout->addWidget(m_gpuRadioButton);
    m_deviceGroupBox->setLayout(deviceLayout);
    panelLayout->addWidget(m_deviceGroupBox);
    connect(m_cpuRadioButton, &QRadioButton::toggled, this, &AutoCaptionSettingsPanel::onDeviceChanged);
    
    m_amdGpuCheckBox = new QCheckBox(tr("Use AMD GPU (DirectML)"), this);
    m_amdGpuCheckBox->setEnabled(false); 
    connect(m_amdGpuCheckBox, &QCheckBox::toggled, this, &AutoCaptionSettingsPanel::useAmdGpuChanged);
    panelLayout->addWidget(m_amdGpuCheckBox);

    m_enableSuggestionCheckBox = new QCheckBox(tr("Enable suggestion while typing"), this);
    m_enableSuggestionCheckBox->setChecked(true); // Default to enabled
    connect(m_enableSuggestionCheckBox, &QCheckBox::toggled, this, &AutoCaptionSettingsPanel::enableSuggestionWhileTypingChanged);
    panelLayout->addWidget(m_enableSuggestionCheckBox);

    // QCheckBox *removeSeparatorCheckBox = new QCheckBox(tr("Remove '_' from tags"), this); // Moved to Advanced Dialog
    // removeSeparatorCheckBox->setChecked(true); 
    // connect(removeSeparatorCheckBox, &QCheckBox::toggled, this, &AutoCaptionSettingsPanel::removeSeparatorChanged);
    // panelLayout->addWidget(removeSeparatorCheckBox);

    m_advancedSettingsButton = new QPushButton(tr("Advanced Model Settings..."), this);
    connect(m_advancedSettingsButton, &QPushButton::clicked, this, &AutoCaptionSettingsPanel::advancedSettingsClicked);
    panelLayout->addWidget(m_advancedSettingsButton);

    // panelLayout->addStretch(); // Remove stretch to make panel compact
    setLayout(panelLayout);
}

void AutoCaptionSettingsPanel::onDeviceChanged() // Renamed
{
    if (m_gpuRadioButton && m_amdGpuCheckBox) {
        m_amdGpuCheckBox->setEnabled(m_gpuRadioButton->isChecked());
        if (!m_gpuRadioButton->isChecked()) {
            m_amdGpuCheckBox->setChecked(false); 
        }
    }
    if (m_cpuRadioButton && m_cpuRadioButton->isChecked()) {
        emit deviceSelectionChanged("CPU");
    } else if (m_gpuRadioButton && m_gpuRadioButton->isChecked()){
        emit deviceSelectionChanged("GPU");
    }
}

// void AutoCaptionSettingsPanel::onCaptioningModeChanged() // Removed slot implementation
// {
// }

// Public method to update status (can be called by MainWindow/AutoCaptionManager)
void AutoCaptionSettingsPanel::setModelStatus(const QString &status, const QString &color) {
    if (m_statusLabel) {
        m_statusLabel->setText(status);
        m_statusLabel->setStyleSheet(QString("color: %1;").arg(color));
    }
    if(m_loadModelButton) m_loadModelButton->setEnabled(color == "red" || color == "orange"); 
    if(m_unloadModelButton) m_unloadModelButton->setEnabled(color == "green"); 

    // If status indicates loading/error, ensure download widgets are hidden
    // unless the status is specifically about downloading.
    // This logic might need refinement based on how AutoCaptionManager emits statuses.
    if (color != "blue") { // Assuming "blue" is for download messages
         hideDownloadWidgets();
    }
}

void AutoCaptionSettingsPanel::showDownloadProgress(const QString &fileName, qint64 bytesReceived, qint64 bytesTotal) {
    if (!m_downloadStatusLabel || !m_downloadProgressBar) return;

    if (!m_downloadStatusLabel->isVisible()) m_downloadStatusLabel->setVisible(true);
    if (!m_downloadProgressBar->isVisible()) m_downloadProgressBar->setVisible(true);
    
    m_downloadStatusLabel->setText(tr("Downloading %1...").arg(fileName));
    m_downloadStatusLabel->setStyleSheet("color: blue;"); // Or another distinct color

    if (bytesTotal > 0) {
        m_downloadProgressBar->setMaximum(bytesTotal);
        m_downloadProgressBar->setValue(bytesReceived);
        m_downloadProgressBar->setFormat(tr("%p% (%1/%2 MB)")
            .arg(bytesReceived / (1024.0*1024.0), 0, 'f', 1)
            .arg(bytesTotal / (1024.0*1024.0), 0, 'f', 1));
    } else { 
        m_downloadProgressBar->setRange(0,0); // Busy indicator if total size unknown
        m_downloadProgressBar->setFormat(tr("%1 MB").arg(bytesReceived / (1024.0*1024.0), 0, 'f', 1));
    }
}

void AutoCaptionSettingsPanel::hideDownloadWidgets() {
    if (m_downloadStatusLabel) m_downloadStatusLabel->setVisible(false);
    if (m_downloadProgressBar) m_downloadProgressBar->setVisible(false);
}

void AutoCaptionSettingsPanel::setDownloadStatusMessage(const QString &message, const QString &color) {
    if (!m_downloadStatusLabel) return;

    if (message.isEmpty()) {
        hideDownloadWidgets();
    } else {
        if (!m_downloadStatusLabel->isVisible()) m_downloadStatusLabel->setVisible(true);
        m_downloadStatusLabel->setText(message);
        m_downloadStatusLabel->setStyleSheet(QString("color: %1;").arg(color));
        // If it's a final download status (not progress), hide the progress bar
        if (m_downloadProgressBar && (color == "green" || color == "red")) {
            m_downloadProgressBar->setVisible(false);
        }
    }
}
