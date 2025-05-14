#ifndef AUTOCAPTIONSETTINGSPANEL_H // Renamed
#define AUTOCAPTIONSETTINGSPANEL_H // Renamed

#include <QWidget>
#include <QComboBox>      // Include full header
#include <QPushButton>    // Include full header
#include <QLabel>         // Include full header
#include <QRadioButton>   // Include full header
#include <QCheckBox>      // Include full header
#include <QGroupBox>      // Include full header
#include <QVBoxLayout>    // Include full header

// QT_BEGIN_NAMESPACE and QT_END_NAMESPACE are not strictly needed here
// if we include the headers directly.

class AutoCaptionSettingsPanel : public QWidget 
{
    Q_OBJECT

public:
    explicit AutoCaptionSettingsPanel(QWidget *parent = nullptr); 
    ~AutoCaptionSettingsPanel();

    void setModelStatus(const QString &status, const QString &color); // Added declaration

signals:
    void loadModelClicked(const QString &modelName);
    void unloadModelClicked();
    void deviceSelectionChanged(const QString &device); 
    void useAmdGpuChanged(bool useAmd);
    void advancedSettingsClicked();
    // void captioningModeChanged(const QString &mode); 
    void enableSuggestionWhileTypingChanged(bool enabled);
    // void removeSeparatorChanged(bool enabled); // Moved to Advanced Dialog

private slots:
    void onDeviceChanged(); 
    // void onCaptioningModeChanged(); // Removed slot

private:
    void setupUI();

    // UI elements 
    QComboBox *m_modelComboBox;
    QPushButton *m_loadModelButton;
    QPushButton *m_unloadModelButton;
    QLabel *m_statusLabel;
    // QGroupBox *m_captionModeGroupBox; // Removed member
    // QRadioButton *m_nlpModeRadioButton; // Removed member
    // QRadioButton *m_tagsModeRadioButton; // Removed member
    QGroupBox *m_deviceGroupBox;
    QRadioButton *m_cpuRadioButton;
    QRadioButton *m_gpuRadioButton;
    QCheckBox *m_amdGpuCheckBox;
    QCheckBox *m_enableSuggestionCheckBox;
    QPushButton *m_advancedSettingsButton;
};

#endif // AUTOCAPTIONSETTINGSPANEL_H // Renamed
