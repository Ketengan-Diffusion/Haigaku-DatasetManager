#ifndef AUTOCAPTIONMANAGER_H
#define AUTOCAPTIONMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include "WdVIT_TaggerEngine.h" 
#include <QtConcurrent>   
#include <QFutureWatcher> 
#include <QNetworkAccessManager> // Added
#include <QNetworkReply>         // Added
#include <QFile>                 // Added
#include <QUrl>                  // Added

class AutoCaptionManager : public QObject
{
    Q_OBJECT

public:
    explicit AutoCaptionManager(QObject *parent = nullptr);
    ~AutoCaptionManager();

    enum class Device { CPU, GPU };

    QVariantMap getModelSettings() const; // Added getter

public slots:
    // Slots to be called from UI (Task Pane, Settings Dialog)
    void loadModel(const QString &modelName);
    void unloadModel();
    void setSelectedDevice(const QString &device); 
    void setUseAmdGpu(bool useAmd);
    void applyModelSettings(const QString &modelName, const QVariantMap &settings);
    void setEnableSuggestionWhileTyping(bool enabled); 
    // void setRemoveSeparator(bool enabled); // Setting now comes via applyModelSettings
    
    // Slot to be called from MainWindow (bulb button)
    void generateCaptionForImage(const QString &imagePath);


signals:
    void modelStatusChanged(const QString &statusMessage, const QString &color);
    void captionGenerated(const QStringList &tags, const QString &forImagePath, bool autoFill); 
    void errorOccurred(const QString &errorMessage);
    // Download signals
    void downloadProgress(const QString &fileName, qint64 bytesReceived, qint64 bytesTotal);
    void downloadComplete(const QString &fileName, bool success, const QString &errorString); // Renamed
    void allDownloadsCompleted();


private slots: 
    void handleTagsGenerated(const QStringList &tags, const QString &forImagePath);
    void handleModelLoadFinished(); 
    // Download slots
    void processNextDownload();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onAllDownloadsCompleted();


private:
    WdVIT_TaggerEngine *m_taggerEngine; 
    QFutureWatcher<QStringList> *m_tagGenerationWatcher; 
    QFutureWatcher<QPair<bool, QString>> *m_modelLoadWatcher; 

    // Download members
    QNetworkAccessManager *m_networkManager;
    QList<QPair<QUrl, QString>> m_downloadQueue; // Url, TargetFilePath
    QNetworkReply *m_currentReply;
    QFile *m_downloadedFile;
    QString m_currentDownloadingFileNameForUI;
    QString m_modelNameToLoadAfterDownload;

    QString m_currentModelName; // Still used for general model context
    Device m_selectedDevice;
    bool m_useAmdGpu;
    bool m_enableSuggestionWhileTyping; 
    // bool m_removeSeparator; // Setting now comes via m_modelSettings
    QVariantMap m_modelSettings; 
    bool m_isModelLoaded;

    // Placeholder for model files path
    // QString m_modelBasePath; 
};

#endif // AUTOCAPTIONMANAGER_H
