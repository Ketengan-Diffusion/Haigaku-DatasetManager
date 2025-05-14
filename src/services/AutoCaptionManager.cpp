#include "AutoCaptionManager.h"
#include <QDebug>
#include <QTimer> 
#include <QCoreApplication> 
#include <QDir>             
#include <QImage>           
#include <QtConcurrent/QtConcurrent> // Added for QtConcurrent::run

AutoCaptionManager::AutoCaptionManager(QObject *parent) 
    : QObject(parent), 
      m_selectedDevice(Device::CPU), 
      m_useAmdGpu(false),
      m_isModelLoaded(false),
      m_enableSuggestionWhileTyping(true), 
      // m_removeSeparator removed, will use m_modelSettings
      m_taggerEngine(nullptr),
      m_tagGenerationWatcher(nullptr) 
{
    m_taggerEngine = new WdVIT_TaggerEngine(); 
    m_tagGenerationWatcher = new QFutureWatcher<QStringList>(this);
    m_modelLoadWatcher = new QFutureWatcher<QPair<bool, QString>>(this); // Initialize new watcher
    m_modelSettings["remove_separator"] = true; 
    connect(m_modelLoadWatcher, &QFutureWatcher<QPair<bool, QString>>::finished, this, &AutoCaptionManager::handleModelLoadFinished);
    // Need to handle the imagePath as well. QFutureWatcher doesn't easily pass extra args.
    // We can use a lambda with QObject::connect for the finished signal, capturing imagePath.
    // Or, more complex, subclass QFutureWatcher or use a helper object.

    // For simplicity now, the lambda in generateCaptionForImage will handle it.
    // A more robust solution might involve a struct for results or a map of watchers.
    qDebug() << "AutoCaptionManager created.";
    emit modelStatusChanged(tr("Model: Unloaded"), "red");
}

AutoCaptionManager::~AutoCaptionManager()
{
    delete m_taggerEngine; // Clean up
    qDebug() << "AutoCaptionManager destroyed.";
}

void AutoCaptionManager::loadModel(const QString &modelName)
{
    if (!m_taggerEngine) {
        m_taggerEngine = new WdVIT_TaggerEngine();
    }

    qDebug() << "Attempting to load model:" << modelName << "on device" << (m_selectedDevice == Device::GPU ? "GPU" : "CPU") << (m_useAmdGpu && m_selectedDevice == Device::GPU ? "(AMD)" : "");
    m_currentModelName = modelName;
    
    emit modelStatusChanged(tr("Model: Loading %1...").arg(modelName), "yellow");
    QCoreApplication::processEvents(); // Force UI update for "Loading..." message

    // Construct path relative to the application executable directory
    QString appDirPath = QCoreApplication::applicationDirPath();
    QString modelBasePath = QDir(appDirPath).filePath("models/wd-vit-tagger-v3"); 
    QString onnxPath = QDir(modelBasePath).filePath("model.onnx");
    QString csvPath = QDir(modelBasePath).filePath("selected_tags.csv");
    
    qDebug() << "Attempting to load ONNX model from:" << onnxPath;
    qDebug() << "Attempting to load CSV from:" << csvPath;

    if (m_modelLoadWatcher->isRunning()) {
        qDebug() << "Model loading already in progress.";
        // Optionally emit an error or status update
        return;
    }

    // Capture necessary variables for the lambda
    Device currentDevice = m_selectedDevice;
    bool useAmd = m_useAmdGpu;
    QString currentModelNameCopy = m_currentModelName; // To use in the finished handler

    QFuture<QPair<bool, QString>> future = QtConcurrent::run([this, onnxPath, csvPath, currentDevice, useAmd, currentModelNameCopy]() {
        // This code runs in a background thread
        if (!m_taggerEngine) { // Should not happen if constructor ran
            return qMakePair(false, QString("Tagger engine not initialized."));
        }
        bool useCuda_thread = currentDevice == Device::GPU && !useAmd;
        bool success_thread = m_taggerEngine->loadModel(onnxPath, csvPath,
                                                 currentDevice == Device::CPU,
                                                 useAmd,
                                                 useCuda_thread);
        QString deviceStr_thread = (currentDevice == Device::GPU ? "GPU" : "CPU");
        if (currentDevice == Device::GPU) {
            if(useAmd) deviceStr_thread += " (DirectML)";
            else deviceStr_thread += " (CUDA/Default)";
        }
        // Return success status and the device string for the status message
        return qMakePair(success_thread, deviceStr_thread);
    });
    m_modelLoadWatcher->setFuture(future);
}

void AutoCaptionManager::handleModelLoadFinished()
{
    if (!m_modelLoadWatcher) return;

    QPair<bool, QString> result = m_modelLoadWatcher->result();
    bool success = result.first;
    QString deviceStr = result.second;

    if (success) {
        m_isModelLoaded = true;
        emit modelStatusChanged(tr("Model: %1 Loaded (%2)").arg(m_currentModelName).arg(deviceStr), "green");
    } else {
        m_isModelLoaded = false;
        emit modelStatusChanged(tr("Error loading %1").arg(m_currentModelName), "red");
        // The 'deviceStr' might contain an error message from the thread if we designed it that way
        // For now, using a generic error.
        emit errorOccurred(tr("Failed to load model files. Check paths, model integrity, or console output."));
    }
}

void AutoCaptionManager::unloadModel()
{
    qDebug() << "Unloading model:" << m_currentModelName;
    if (m_taggerEngine) {
        m_taggerEngine->unloadModel();
    }
    m_isModelLoaded = false;
    m_currentModelName.clear();
    emit modelStatusChanged(tr("Model: Unloaded"), "red");
}

void AutoCaptionManager::setSelectedDevice(const QString &device)
{
    qDebug() << "Device selected:" << device;
    if (device.toLower() == "gpu") {
        m_selectedDevice = Device::GPU;
    } else {
        m_selectedDevice = Device::CPU;
        m_useAmdGpu = false; // Cannot use AMD GPU if CPU is selected
    }
    // If a model is already loaded, ideally it should be reloaded on the new device.
    // For now, this setting applies to the *next* model load.
    if (m_isModelLoaded) {
         emit modelStatusChanged(tr("Device changed. Reload model to apply."), "orange");
    }
}

void AutoCaptionManager::setUseAmdGpu(bool useAmd)
{
    qDebug() << "Use AMD GPU (DirectML) set to:" << useAmd;
    m_useAmdGpu = useAmd;
    if (m_selectedDevice == Device::GPU && m_isModelLoaded) {
        emit modelStatusChanged(tr("Device setting changed. Reload model to apply."), "orange");
    }
}

void AutoCaptionManager::applyModelSettings(const QString &modelName, const QVariantMap &settings)
{
    qDebug() << "Applying settings for model" << modelName << ":" << settings;
    m_modelSettings = settings;
    // if (m_taggerEngine && m_currentModelName == modelName) {
    //     m_taggerEngine->updateSettings(settings);
    // }
}

void AutoCaptionManager::generateCaptionForImage(const QString &imagePath)
{
    if (!m_isModelLoaded) {
        qDebug() << "Generate caption requested, but no model loaded.";
        emit errorOccurred(tr("No model loaded. Please load a model first."));
        return;
    }
    if (imagePath.isEmpty()) {
        qDebug() << "Generate caption requested, but no image path provided.";
        emit errorOccurred(tr("No image selected or path is invalid."));
        return;
    }
    if (!m_taggerEngine) {
         qDebug() << "Tagger engine not initialized.";
         emit errorOccurred(tr("Tagger engine not available."));
        return;
    }

    qDebug() << "Generating caption for:" << imagePath << "with settings:" << m_modelSettings;
    
    // Run tag generation in a background thread
    // Capture necessary variables by value for the lambda
    // Note: m_taggerEngine and m_modelSettings should be stable during this async call.
    // If m_taggerEngine can be deleted or m_modelSettings changed while a task is running,
    // we need more robust handling (e.g., clone settings, ensure engine validity).

    // Using a QFutureWatcher to get results back on the main thread via signals/slots
    // is cleaner than QMetaObject::invokeMethod from the thread.
    // However, QFutureWatcher is typically for one future at a time. If multiple requests
    // can happen, we'd need a pool of watchers or a different mechanism.
    // For now, assume one generation at a time.

    if (m_tagGenerationWatcher->isRunning()) {
        qDebug() << "Tag generation already in progress.";
        emit errorOccurred(tr("Tag generation is already in progress. Please wait."));
        return;
    }

    // It's crucial that m_taggerEngine is not deleted while the thread is using it.
    // And m_modelSettings should not be modified.
    // For a simple case, we assume they are stable.
    // A copy of m_modelSettings is implicitly made by the lambda capture.
    QFuture<QStringList> future = QtConcurrent::run([this, imagePath, settings = m_modelSettings]() {
        QImage image(imagePath); // Load image in the worker thread
        if (image.isNull()) {
            qWarning() << "Worker Thread: Failed to load image for captioning:" << imagePath;
            // How to signal error from here? Can return empty list or throw.
            // For now, return empty. Error handling can be improved.
            return QStringList(); 
        }
        if (!m_taggerEngine || !m_taggerEngine->isModelLoaded()) {
             qWarning() << "Worker Thread: Tagger engine not ready.";
             return QStringList();
        }
        return m_taggerEngine->generateTags(image, settings);
    });

    // Disconnect previous connection to avoid multiple calls if button is spammed
    // and somehow a new future is set before the old one finishes (though isRunning check prevents this).
    disconnect(m_tagGenerationWatcher, &QFutureWatcher<QStringList>::finished, nullptr, nullptr);

    // Store imagePath to use it when finished signal is emitted
    // This is a bit hacky if multiple requests overlap. A proper job queue would be better.
    // For now, using a lambda to capture imagePath for the specific connection.
    connect(m_tagGenerationWatcher, &QFutureWatcher<QStringList>::finished, 
            this, [this, imagePath]() {
        QStringList tags = m_tagGenerationWatcher->result();
        handleTagsGenerated(tags, imagePath); // Call our slot to emit the final signal
    });
    
    m_tagGenerationWatcher->setFuture(future);
    emit modelStatusChanged(tr("Model: Generating tags..."), "blue"); // Indicate activity
}

void AutoCaptionManager::handleTagsGenerated(const QStringList &tags, const QString &forImagePath)
{
    QStringList processedTags = tags;
    bool removeSeparatorFromSettings = m_modelSettings.value("remove_separator", true).toBool(); // Default true if not in map
    if (removeSeparatorFromSettings) {
        for (QString &tag : processedTags) {
            tag.replace('_', ' ');
        }
    }

    if (processedTags.isEmpty() && !forImagePath.isEmpty()) { 
        qDebug() << "Tag generation resulted in empty list for" << forImagePath;
    }
    emit captionGenerated(processedTags, forImagePath, m_enableSuggestionWhileTyping); 
    
    if (m_isModelLoaded) { 
         QString deviceStr = (m_selectedDevice == Device::GPU ? "GPU" : "CPU");
         if (m_selectedDevice == Device::GPU) {
            if(m_useAmdGpu) deviceStr += " (DirectML)";
            else deviceStr += " (CUDA/Default)";
        }
        emit modelStatusChanged(tr("Model: %1 Loaded (%2)").arg(m_currentModelName).arg(deviceStr), "green");
    } else {
        emit modelStatusChanged(tr("Model: Unloaded"), "red");
    }
}

void AutoCaptionManager::setEnableSuggestionWhileTyping(bool enabled)
{
    qDebug() << "Enable suggestion while typing set to:" << enabled;
    m_enableSuggestionWhileTyping = enabled;
}

// void AutoCaptionManager::setRemoveSeparator(bool enabled) // Removed
// {
// }

QVariantMap AutoCaptionManager::getModelSettings() const
{
    return m_modelSettings;
}
