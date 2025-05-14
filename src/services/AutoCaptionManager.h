#ifndef AUTOCAPTIONMANAGER_H
#define AUTOCAPTIONMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include "WdVIT_TaggerEngine.h" 
#include <QtConcurrent>   // Changed from <QtConcurrent/QtFuture>
#include <QFutureWatcher> 

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
    void captionGenerated(const QStringList &tags, const QString &forImagePath, bool autoFill); // Added autoFill
    void errorOccurred(const QString &errorMessage);

private slots: 
    void handleTagsGenerated(const QStringList &tags, const QString &forImagePath);
    void handleModelLoadFinished(); // New slot

private:
    WdVIT_TaggerEngine *m_taggerEngine; 
    QFutureWatcher<QStringList> *m_tagGenerationWatcher; 
    QFutureWatcher<QPair<bool, QString>> *m_modelLoadWatcher; // New watcher

    QString m_currentModelName;
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
