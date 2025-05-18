#ifndef AUTOCAPTIONSETTINGSPANEL_H // Renamed
#define AUTOCAPTIONSETTINGSPANEL_H // Renamed

#include <QWidget>
#include <QComboBox>      // Include full header
#include <QPushButton>    // Include full header
#include <QLabel>         // Include full header
#include <QRadioButton>   
#include <QCheckBox>      
#include <QGroupBox>      
#include <QVBoxLayout>    
#include <QProgressBar>   // Added for download progress


class AutoCaptionSettingsPanel : public QWidget 
{
    Q_OBJECT

public:
    explicit AutoCaptionSettingsPanel(QWidget *parent = nullptr); 
    ~AutoCaptionSettingsPanel();

    void setModelStatus(const QString &status, const QString &color); 

public slots: // Changed from private slots to public for connection from MainWindow/Manager
    void showDownloadProgress(const QString &fileName, qint64 bytesReceived, qint64 bytesTotal);
    void hideDownloadWidgets();
    void setDownloadStatusMessage(const QString &message, const QString &color = "black");

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

    // Download UI
    QLabel *m_downloadStatusLabel;
    QProgressBar *m_downloadProgressBar;
};

#endif // AUTOCAPTIONSETTINGSPANEL_H
