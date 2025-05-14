#ifndef AUTOCAPTIONTASKPANE_H
#define AUTOCAPTIONTASKPANE_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QToolButton;
class QComboBox;
class QPushButton;
class QLabel;
class QRadioButton;
class QCheckBox;
class QGroupBox;
class QVBoxLayout;
class QPropertyAnimation;
QT_END_NAMESPACE

class AutoCaptionTaskPane : public QWidget
{
    Q_OBJECT

public:
    explicit AutoCaptionTaskPane(QWidget *parent = nullptr);
    ~AutoCaptionTaskPane();

signals:
    // Signals to communicate user actions to AutoCaptionManager or MainWindow
    void loadModelClicked(const QString &modelName);
    void unloadModelClicked();
    void deviceSelectionChanged(const QString &device); // "CPU" or "GPU"
    void useAmdGpuChanged(bool useAmd);
    void advancedSettingsClicked();
    void enableSuggestionWhileTypingChanged(bool enabled);
    void captioningModeChanged(const QString &mode); // "NLP" or "Tags"


private slots:
    void toggleSettingsPanel();
    void onDeviceChanged(); 
    void onCaptioningModeChanged(); // Slot to handle internal radio button changes

private:
    void setupUI();

    QToolButton *m_toggleButton; // The "sparkle star"
    QWidget *m_settingsPanel;    // Panel that slides
    QPropertyAnimation *m_slideAnimation;

    // UI elements within m_settingsPanel
    QComboBox *m_modelComboBox;
    QPushButton *m_loadModelButton;
    QPushButton *m_unloadModelButton;
    QLabel *m_statusLabel;
    QGroupBox *m_deviceGroupBox;
    QRadioButton *m_cpuRadioButton;
    QRadioButton *m_gpuRadioButton;
    QCheckBox *m_amdGpuCheckBox;
    QCheckBox *m_enableSuggestionCheckBox;
    QPushButton *m_advancedSettingsButton;

    // Caption Mode Radio Buttons
    QRadioButton *m_nlpModeRadioButton;
    QRadioButton *m_tagsModeRadioButton;

    bool m_settingsPanelVisible;
};

#endif // AUTOCAPTIONTASKPANE_H
