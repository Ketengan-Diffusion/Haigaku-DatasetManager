#include "AutoCaptionTaskPane.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QButtonGroup> // Added for radio button exclusivity
#include <QPropertyAnimation>
#include <QStyle> // For standard icons
#include <QDebug>

AutoCaptionTaskPane::AutoCaptionTaskPane(QWidget *parent)
    : QWidget(parent), m_settingsPanelVisible(false)
{
    setupUI();
}

AutoCaptionTaskPane::~AutoCaptionTaskPane()
{
}

void AutoCaptionTaskPane::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0); // No margins for the main widget of the dock

    m_toggleButton = new QToolButton(this);
    // Placeholder icon for "sparkle star" - using SP_DialogYesButton for now
    m_toggleButton->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton)); 
    m_toggleButton->setText(tr(" Auto Caption Settings")); // Text next to icon
    m_toggleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toggleButton->setCheckable(true); // To represent open/closed state
    m_toggleButton->setChecked(false);
    m_toggleButton->setToolTip(tr("Toggle Auto-Caption Settings Panel"));
    connect(m_toggleButton, &QToolButton::toggled, this, &AutoCaptionTaskPane::toggleSettingsPanel);
    mainLayout->addWidget(m_toggleButton);

    m_settingsPanel = new QWidget(this);
    QVBoxLayout *panelLayout = new QVBoxLayout(m_settingsPanel);
    panelLayout->setContentsMargins(5,5,5,5);

    // Model Selection
    QHBoxLayout *modelLayout = new QHBoxLayout();
    modelLayout->addWidget(new QLabel(tr("Model:"), m_settingsPanel));
    m_modelComboBox = new QComboBox(m_settingsPanel);
    m_modelComboBox->addItem("SmilingWolf/wd-vit-tagger-v3"); // Hardcoded for now
    // m_modelComboBox->addItem("Other Model..."); // Example for future
    modelLayout->addWidget(m_modelComboBox);
    panelLayout->addLayout(modelLayout);

    // Load/Unload Buttons
    QHBoxLayout *loadButtonsLayout = new QHBoxLayout();
    m_loadModelButton = new QPushButton(tr("Load Model"), m_settingsPanel);
    connect(m_loadModelButton, &QPushButton::clicked, this, [this](){
        emit loadModelClicked(m_modelComboBox->currentText());
    });
    m_unloadModelButton = new QPushButton(tr("Unload Model"), m_settingsPanel);
    m_unloadModelButton->setEnabled(false); // Initially no model loaded
    connect(m_unloadModelButton, &QPushButton::clicked, this, &AutoCaptionTaskPane::unloadModelClicked);
    loadButtonsLayout->addWidget(m_loadModelButton);
    loadButtonsLayout->addWidget(m_unloadModelButton);
    panelLayout->addLayout(loadButtonsLayout);
    
    // Status Label
    m_statusLabel = new QLabel(tr("Model: Unloaded"), m_settingsPanel);
    m_statusLabel->setStyleSheet("color: red;"); 
    panelLayout->addWidget(m_statusLabel);

    // Caption Mode (NLP/Tags)
    QGroupBox *captionModeGroup = new QGroupBox(tr("Captioning Mode"), m_settingsPanel);
    QHBoxLayout *captionModeLayout = new QHBoxLayout();
    m_nlpModeRadioButton = new QRadioButton(tr("NLP (Natural Language)"), captionModeGroup); // Use member
    m_tagsModeRadioButton = new QRadioButton(tr("Tags (Booru-style)"), captionModeGroup); // Use member
    m_nlpModeRadioButton->setChecked(true); // Default
    captionModeLayout->addWidget(m_nlpModeRadioButton);
    captionModeLayout->addWidget(m_tagsModeRadioButton);
    captionModeGroup->setLayout(captionModeLayout);
    panelLayout->addWidget(captionModeGroup);
    
    QButtonGroup *captionModeButtonGroup = new QButtonGroup(this); 
    captionModeButtonGroup->addButton(m_nlpModeRadioButton);
    captionModeButtonGroup->addButton(m_tagsModeRadioButton);
    // Connect toggled signal of one of them, or use QButtonGroup::buttonToggled
    connect(m_nlpModeRadioButton, &QRadioButton::toggled, this, &AutoCaptionTaskPane::onCaptioningModeChanged);

    // Device Selection
    m_deviceGroupBox = new QGroupBox(tr("Device"), m_settingsPanel);
    QHBoxLayout *deviceLayout = new QHBoxLayout();
    m_cpuRadioButton = new QRadioButton(tr("CPU"), m_deviceGroupBox);
    m_gpuRadioButton = new QRadioButton(tr("GPU"), m_deviceGroupBox);
    m_cpuRadioButton->setChecked(true); // Default to CPU
    deviceLayout->addWidget(m_cpuRadioButton);
    deviceLayout->addWidget(m_gpuRadioButton);
    m_deviceGroupBox->setLayout(deviceLayout);
    panelLayout->addWidget(m_deviceGroupBox);
    connect(m_cpuRadioButton, &QRadioButton::toggled, this, &AutoCaptionTaskPane::onDeviceChanged);
    // GPU specific options
    m_amdGpuCheckBox = new QCheckBox(tr("Use AMD GPU (DirectML)"), m_settingsPanel);
    m_amdGpuCheckBox->setEnabled(false); // Enabled only if GPU is selected
    connect(m_amdGpuCheckBox, &QCheckBox::toggled, this, &AutoCaptionTaskPane::useAmdGpuChanged);
    panelLayout->addWidget(m_amdGpuCheckBox);


    // Other Options
    m_enableSuggestionCheckBox = new QCheckBox(tr("Enable suggestion while typing"), m_settingsPanel);
    connect(m_enableSuggestionCheckBox, &QCheckBox::toggled, this, &AutoCaptionTaskPane::enableSuggestionWhileTypingChanged);
    panelLayout->addWidget(m_enableSuggestionCheckBox);

    m_advancedSettingsButton = new QPushButton(tr("Advanced Model Settings..."), m_settingsPanel);
    connect(m_advancedSettingsButton, &QPushButton::clicked, this, &AutoCaptionTaskPane::advancedSettingsClicked);
    panelLayout->addWidget(m_advancedSettingsButton);

    m_settingsPanel->setLayout(panelLayout);
    mainLayout->addWidget(m_settingsPanel);
    
    // Animation setup
    m_slideAnimation = new QPropertyAnimation(m_settingsPanel, "maximumHeight", this);
    m_slideAnimation->setDuration(300); // 300ms animation
    
    m_settingsPanel->setMaximumHeight(0); // Initially collapsed
    m_settingsPanelVisible = false;

    mainLayout->addStretch(); // Pushes everything up
    setLayout(mainLayout);
}

void AutoCaptionTaskPane::toggleSettingsPanel()
{
    int startHeight = m_settingsPanel->maximumHeight();
    int endHeight;

    if (m_settingsPanelVisible) { // Collapse
        endHeight = 0;
        m_toggleButton->setChecked(false); // Ensure button state matches
    } else { // Expand
        // Calculate the fully expanded height based on content
        endHeight = m_settingsPanel->sizeHint().height();
        m_toggleButton->setChecked(true); // Ensure button state matches
    }

    m_slideAnimation->setStartValue(startHeight);
    m_slideAnimation->setEndValue(endHeight);
    m_slideAnimation->start();
    m_settingsPanelVisible = !m_settingsPanelVisible;
}

void AutoCaptionTaskPane::onDeviceChanged()
{
    if (m_gpuRadioButton && m_amdGpuCheckBox) {
        m_amdGpuCheckBox->setEnabled(m_gpuRadioButton->isChecked());
        if (!m_gpuRadioButton->isChecked()) {
            m_amdGpuCheckBox->setChecked(false); // Uncheck AMD if CPU is selected
        }
    }
    if (m_cpuRadioButton && m_cpuRadioButton->isChecked()) {
        emit deviceSelectionChanged("CPU");
    } else if (m_gpuRadioButton && m_gpuRadioButton->isChecked()){
        emit deviceSelectionChanged("GPU");
    }
}

void AutoCaptionTaskPane::onCaptioningModeChanged()
{
    // Find which radio button is checked and emit a signal or update state
    // This slot is connected to m_nlpModeRadioButton's toggled signal.
    if (m_nlpModeRadioButton && m_nlpModeRadioButton->isChecked()) {
        qDebug() << "Task Pane: NLP Mode selected";
        emit captioningModeChanged("NLP");
    } else if (m_tagsModeRadioButton && m_tagsModeRadioButton->isChecked()) { // Check the other member
        qDebug() << "Task Pane: Tags Mode selected";
        emit captioningModeChanged("Tags");
    }
}

// Public methods to update UI from outside (e.g., from AutoCaptionManager)
// void AutoCaptionTaskPane::setModelStatus(const QString &status, const QString &color) {
//     if (m_statusLabel) {
//         m_statusLabel->setText(status);
//         m_statusLabel->setStyleSheet(QString("color: %1;").arg(color));
//     }
//     m_loadModelButton->setEnabled(color == "red"); // Enable load if unloaded
//     m_unloadModelButton->setEnabled(color != "red"); // Enable unload if loaded/loading
// }
