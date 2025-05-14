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

AutoCaptionSettingsPanel::AutoCaptionSettingsPanel(QWidget *parent) // Renamed
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
    m_statusLabel->setObjectName("statusLabel"); // For MainWindow to find if needed
    m_statusLabel->setStyleSheet("color: red;"); 
    panelLayout->addWidget(m_statusLabel);

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
}
