#include "AutoCaptionManager.h"
#include <QDebug>
#include <QTimer> 
#include <QCoreApplication> 
#include <QDir>             
#include <QImage>           
#include <QtConcurrent/QtConcurrent> 
#include <QNetworkRequest> // Added
#include <QFileInfo>       // Added

AutoCaptionManager::AutoCaptionManager(QObject *parent) 
    : QObject(parent), 
      m_selectedDevice(Device::CPU), 
      m_useAmdGpu(false),
      m_isModelLoaded(false),
      m_enableSuggestionWhileTyping(true), 
      m_taggerEngine(nullptr),
      m_tagGenerationWatcher(nullptr),
      m_networkManager(new QNetworkAccessManager(this)), // Initialize network manager
      m_currentReply(nullptr),
      m_downloadedFile(nullptr)
{
    m_taggerEngine = new WdVIT_TaggerEngine(); 
    m_tagGenerationWatcher = new QFutureWatcher<QStringList>(this);
    m_modelLoadWatcher = new QFutureWatcher<QPair<bool, QString>>(this); 
    m_modelSettings["remove_separator"] = true; 
    connect(m_modelLoadWatcher, &QFutureWatcher<QPair<bool, QString>>::finished, this, &AutoCaptionManager::handleModelLoadFinished);
    connect(this, &AutoCaptionManager::allDownloadsCompleted, this, &AutoCaptionManager::onAllDownloadsCompleted);
    
    qDebug() << "AutoCaptionManager created.";
    emit modelStatusChanged(tr("Model: Unloaded"), "red");
}

AutoCaptionManager::~AutoCaptionManager()
{
    if (m_currentReply) { // Clean up ongoing download if any
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
    if (m_downloadedFile && m_downloadedFile->isOpen()) {
        m_downloadedFile->close();
        m_downloadedFile->remove(); // Remove partially downloaded file
    }
    delete m_downloadedFile;
    delete m_taggerEngine; 
    qDebug() << "AutoCaptionManager destroyed.";
}

void AutoCaptionManager::loadModel(const QString &modelName)
{
    if (!m_taggerEngine) { // Should not happen normally
        m_taggerEngine = new WdVIT_TaggerEngine();
    }

    qDebug() << "Request to load model:" << modelName << "on device" << (m_selectedDevice == Device::GPU ? "GPU" : "CPU") << (m_useAmdGpu && m_selectedDevice == Device::GPU ? "(AMD)" : "");
    
    m_modelNameToLoadAfterDownload = modelName; // Store for when downloads (if any) complete

    QString appDirPath = QCoreApplication::applicationDirPath();
    QString modelBasePath = QDir(appDirPath).filePath("models/" + modelName); // Use modelName for subfolder
    QString onnxPath = QDir(modelBasePath).filePath("model.onnx");
    QString csvPath = QDir(modelBasePath).filePath("selected_tags.csv");
    
    QFileInfo onnxFileInfo(onnxPath);
    QFileInfo csvFileInfo(csvPath);

    if (onnxFileInfo.exists() && csvFileInfo.exists()) {
        qDebug() << "Model files found locally. Proceeding to load.";
        m_currentModelName = modelName; // Set for status updates during direct load
        emit modelStatusChanged(tr("Model: Loading %1...").arg(modelName), "yellow");
        QCoreApplication::processEvents(); 

        if (m_modelLoadWatcher->isRunning()) {
            qDebug() << "Model loading already in progress.";
            return;
        }
        Device currentDevice = m_selectedDevice;
        bool useAmd = m_useAmdGpu;
        
        QFuture<QPair<bool, QString>> future = QtConcurrent::run([this, onnxPath, csvPath, currentDevice, useAmd]() {
            if (!m_taggerEngine) return qMakePair(false, QString("Tagger engine not initialized."));
            bool useCuda_thread = currentDevice == Device::GPU && !useAmd;
            bool success_thread = m_taggerEngine->loadModel(onnxPath, csvPath,
                                                     currentDevice == Device::CPU, useAmd, useCuda_thread);
            QString deviceStr_thread = (currentDevice == Device::GPU ? "GPU" : "CPU");
            if (currentDevice == Device::GPU) {
                if(useAmd) deviceStr_thread += " (DirectML)"; else deviceStr_thread += " (CUDA/Default)";
            }
            return qMakePair(success_thread, deviceStr_thread);
        });
        m_modelLoadWatcher->setFuture(future);
    } else {
        qDebug() << "Model files not found locally. Starting download sequence for" << modelName;
        emit modelStatusChanged(tr("Model files for %1 not found. Downloading...").arg(modelName), "orange");
        QCoreApplication::processEvents();

        QDir modelDir(modelBasePath);
        if (!modelDir.exists()) {
            if (!modelDir.mkpath(".")) {
                emit errorOccurred(tr("Could not create model directory: %1").arg(modelDir.path()));
                emit modelStatusChanged(tr("Error creating directory for %1").arg(modelName), "red");
                return;
            }
        }

        m_downloadQueue.clear();
        // Assuming modelName is like "SmilingWolf/wd-vit-tagger-v3"
        // We need to construct URLs based on this.
        // For now, hardcoding for "SmilingWolf/wd-vit-tagger-v3"
        if (modelName == "SmilingWolf/wd-vit-tagger-v3") {
             m_downloadQueue.append({QUrl("https://huggingface.co/SmilingWolf/wd-vit-tagger-v3/resolve/main/model.onnx?download=true"), onnxPath});
             m_downloadQueue.append({QUrl("https://huggingface.co/SmilingWolf/wd-vit-tagger-v3/resolve/main/selected_tags.csv?download=true"), csvPath});
        } else {
            emit errorOccurred(tr("Download URLs not defined for model: %1").arg(modelName));
            emit modelStatusChanged(tr("Cannot download %1").arg(modelName), "red");
            return;
        }
        processNextDownload();
    }
}

void AutoCaptionManager::processNextDownload() {
    if (m_currentReply && m_currentReply->isRunning()) { // Should not happen if logic is correct
        qWarning() << "processNextDownload called while a download is already in progress.";
        return;
    }

    if (m_downloadQueue.isEmpty()) {
        emit allDownloadsCompleted();
        return;
    }

    QPair<QUrl, QString> currentDownload = m_downloadQueue.takeFirst();
    QUrl url = currentDownload.first;
    QString targetPath = currentDownload.second;

    m_currentDownloadingFileNameForUI = QFileInfo(targetPath).fileName();
    
    m_downloadedFile = new QFile(targetPath);
    if (!m_downloadedFile->open(QIODevice::WriteOnly)) {
        qWarning() << "Could not open file for writing:" << targetPath;
        emit errorOccurred(tr("Failed to open file for download: %1").arg(m_currentDownloadingFileNameForUI));
        emit downloadComplete(m_currentDownloadingFileNameForUI, false, tr("File open error"));
        delete m_downloadedFile;
        m_downloadedFile = nullptr;
        // Optionally try next file or stop all downloads
        m_downloadQueue.clear(); // Stop all on critical error like this
        emit modelStatusChanged(tr("Download error for %1").arg(m_modelNameToLoadAfterDownload), "red");
        return;
    }

    QNetworkRequest request(url);
    // Removed problematic setAttribute for redirect policy. Will handle redirects manually.
    m_currentReply = m_networkManager->get(request);

    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &AutoCaptionManager::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &AutoCaptionManager::onDownloadFinished);
    // Error signal (optional, as finished() also indicates errors)
    // connect(m_currentReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred), this, &AutoCaptionManager::onDownloadError);

    emit modelStatusChanged(tr("Downloading %1...").arg(m_currentDownloadingFileNameForUI), "blue");
    emit downloadProgress(m_currentDownloadingFileNameForUI, 0, 0); // Initial progress update
}

void AutoCaptionManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    emit downloadProgress(m_currentDownloadingFileNameForUI, bytesReceived, bytesTotal);
}

void AutoCaptionManager::onDownloadFinished() {
    if (!m_currentReply) return; // Should not happen

    bool success = false;
    QString errorString;

    // Check for redirection
    QUrl redirectUrl = m_currentReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirectUrl.isValid() && !redirectUrl.isEmpty() && redirectUrl != m_currentReply->url()) {
        qDebug() << "Download for" << m_currentDownloadingFileNameForUI << "redirected from" << m_currentReply->url().toString() << "to" << redirectUrl.toString();
        
        // Clean up the current reply object before creating a new one
        // but keep m_downloadedFile open as we are still downloading the same logical file.
        m_currentReply->deleteLater(); 
        m_currentReply = nullptr;

        QNetworkRequest newRequest(redirectUrl);
        // No redirect policy needed here as we are handling one level.
        // If further redirects are possible, this logic might need to be recursive or loop.
        // For typical HuggingFace downloads, one level of HTTP -> HTTPS redirect is common.
        m_currentReply = m_networkManager->get(newRequest);
        connect(m_currentReply, &QNetworkReply::downloadProgress, this, &AutoCaptionManager::onDownloadProgress);
        connect(m_currentReply, &QNetworkReply::finished, this, &AutoCaptionManager::onDownloadFinished);
        // Note: We don't emit downloadProgress here, the new reply will.
        return; // Important: exit here, the new reply's finished signal will trigger this slot again.
    }


    if (m_currentReply->error() == QNetworkReply::NoError) {
        if (m_downloadedFile && m_downloadedFile->isOpen()) {
            m_downloadedFile->write(m_currentReply->readAll());
            m_downloadedFile->close(); // Close file on successful write of this segment/final part
            success = true;
            qDebug() << "Successfully downloaded and saved" << m_currentDownloadingFileNameForUI;
        } else {
            errorString = tr("File not open for writing after download.");
            qWarning() << errorString;
        }
    } else {
        errorString = m_currentReply->errorString();
        qWarning() << "Download failed for" << m_currentDownloadingFileNameForUI << ":" << errorString;
        if (m_downloadedFile && m_downloadedFile->isOpen()) {
            m_downloadedFile->close();
            m_downloadedFile->remove(); // Remove partial file on error
        }
    }

    delete m_downloadedFile; // m_downloadedFile is now closed or removed
    m_downloadedFile = nullptr;
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    emit downloadComplete(m_currentDownloadingFileNameForUI, success, errorString);

    if (success) {
        processNextDownload(); // Try next file in queue
    } else {
        m_downloadQueue.clear(); // Stop queue on error
        emit modelStatusChanged(tr("Download failed for %1").arg(m_modelNameToLoadAfterDownload), "red");
        emit errorOccurred(tr("Failed to download %1: %2").arg(m_currentDownloadingFileNameForUI).arg(errorString));
    }
}

void AutoCaptionManager::onAllDownloadsCompleted() {
    qDebug() << "All model files downloaded for" << m_modelNameToLoadAfterDownload << ". Proceeding to load model.";
    emit modelStatusChanged(tr("Downloads complete. Loading %1...").arg(m_modelNameToLoadAfterDownload), "yellow");
    
    // Now trigger the actual model loading logic (similar to when files existed initially)
    m_currentModelName = m_modelNameToLoadAfterDownload; // Ensure m_currentModelName is set for handleModelLoadFinished

    QString appDirPath = QCoreApplication::applicationDirPath();
    QString modelBasePath = QDir(appDirPath).filePath("models/" + m_currentModelName);
    QString onnxPath = QDir(modelBasePath).filePath("model.onnx");
    QString csvPath = QDir(modelBasePath).filePath("selected_tags.csv");

    if (m_modelLoadWatcher->isRunning()) {
        qDebug() << "Model loading already in progress (after download)."; // Should ideally not happen
        return;
    }
    Device currentDevice = m_selectedDevice;
    bool useAmd = m_useAmdGpu;
    
    QFuture<QPair<bool, QString>> future = QtConcurrent::run([this, onnxPath, csvPath, currentDevice, useAmd]() {
        if (!m_taggerEngine) return qMakePair(false, QString("Tagger engine not initialized."));
        bool useCuda_thread = currentDevice == Device::GPU && !useAmd;
        bool success_thread = m_taggerEngine->loadModel(onnxPath, csvPath,
                                                 currentDevice == Device::CPU, useAmd, useCuda_thread);
        QString deviceStr_thread = (currentDevice == Device::GPU ? "GPU" : "CPU");
        if (currentDevice == Device::GPU) {
            if(useAmd) deviceStr_thread += " (DirectML)"; else deviceStr_thread += " (CUDA/Default)";
        }
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
    m_modelNameToLoadAfterDownload.clear();
    emit modelStatusChanged(tr("Model: Unloaded"), "red");
}

void AutoCaptionManager::setSelectedDevice(const QString &device)
{
    qDebug() << "Device selected:" << device;
    if (device.toLower() == "gpu") {
        m_selectedDevice = Device::GPU;
    } else {
        m_selectedDevice = Device::CPU;
        m_useAmdGpu = false; 
    }
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
    
    if (m_tagGenerationWatcher->isRunning()) {
        qDebug() << "Tag generation already in progress.";
        emit errorOccurred(tr("Tag generation is already in progress. Please wait."));
        return;
    }

    QFuture<QStringList> future = QtConcurrent::run([this, imagePath, settings = m_modelSettings]() {
        QImage image(imagePath); 
        if (image.isNull()) {
            qWarning() << "Worker Thread: Failed to load image for captioning:" << imagePath;
            return QStringList(); 
        }
        if (!m_taggerEngine || !m_taggerEngine->isModelLoaded()) {
             qWarning() << "Worker Thread: Tagger engine not ready.";
             return QStringList();
        }
        return m_taggerEngine->generateTags(image, settings);
    });

    disconnect(m_tagGenerationWatcher, &QFutureWatcher<QStringList>::finished, nullptr, nullptr);

    connect(m_tagGenerationWatcher, &QFutureWatcher<QStringList>::finished, 
            this, [this, imagePath]() {
        QStringList tags = m_tagGenerationWatcher->result();
        handleTagsGenerated(tags, imagePath); 
    });
    
    m_tagGenerationWatcher->setFuture(future);
    emit modelStatusChanged(tr("Model: Generating tags..."), "blue"); 
}

void AutoCaptionManager::handleTagsGenerated(const QStringList &tags, const QString &forImagePath)
{
    QStringList processedTags = tags;
    bool removeSeparatorFromSettings = m_modelSettings.value("remove_separator", true).toBool(); 
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

QVariantMap AutoCaptionManager::getModelSettings() const
{
    return m_modelSettings;
}
