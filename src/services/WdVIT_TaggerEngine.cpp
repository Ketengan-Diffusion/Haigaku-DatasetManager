#include "WdVIT_TaggerEngine.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <stdexcept> // For std::runtime_error
#include <QPainter>  // Added for QPainter

// Constructor: Initialize ONNX Runtime environment
WdVIT_TaggerEngine::WdVIT_TaggerEngine()
    : m_ortEnv(ORT_LOGGING_LEVEL_WARNING, "HaigakuTagger") 
    , m_ortSession(nullptr)
    , m_modelLoaded(false)
    , m_vocabularyLoaded(false) // Initialize new flag
    , m_generalThreshold(0.35f) 
{
    qDebug() << "WdVIT_TaggerEngine created.";
}

WdVIT_TaggerEngine::~WdVIT_TaggerEngine()
{
    unloadModel(); // Ensure session is released
    qDebug() << "WdVIT_TaggerEngine destroyed.";
}

bool WdVIT_TaggerEngine::loadModel(const QString &modelPath, const QString &tagsCsvPath, 
                                   bool useCpu, bool useDirectML, bool useCuda)
{
    if (m_modelLoaded) { // If full model is loaded, unload it first
        unloadModel(); 
    } else if (m_vocabularyLoaded) { // If only vocab is loaded, unload that
        unloadVocabulary();
    }

    // Load vocabulary first
    if (!loadTagVocabulary(tagsCsvPath)) {
        qWarning() << "Failed to load tag vocabulary, model loading aborted.";
        return false;
    }

    try {
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1); 

        // TODO: Add execution provider logic (DirectML, CUDA)
        // For now, defaults to CPU
        if (useDirectML) {
            qDebug() << "Attempting to use DirectML.";
            // OrtSessionOptionsAppendExecutionProvider_DML(session_options, 0); // Example, API might vary
        } else if (useCuda) {
            qDebug() << "Attempting to use CUDA.";
            // OrtCUDAProviderOptions cuda_options{};
            // session_options.AppendExecutionProvider_CUDA(cuda_options); // Example
        } else {
            qDebug() << "Using CPU execution provider.";
        }
        
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

        #ifdef _WIN32
            std::wstring modelPathW = modelPath.toStdWString();
            m_ortSession = std::make_unique<Ort::Session>(m_ortEnv, modelPathW.c_str(), session_options);
        #else
            m_ortSession = std::make_unique<Ort::Session>(m_ortEnv, modelPath.toStdString().c_str(), session_options);
        #endif

        // Vocabulary is already loaded by loadTagVocabulary() call above.

        Ort::AllocatorWithDefaultOptions allocator; 
        // Ort::AllocatedStringPtr input_name_ptr = m_ortSession->GetInputNameAllocated(0, allocator); // Correct API
        // m_inputNodeNames_str.push_back(input_name_ptr.get()); // Store as std::string
        // m_inputNodeNames.push_back(m_inputNodeNames_str.back().c_str()); // Then get char* if needed by API
        
        // Simpler for now if Ort::Session manages these strings, but need to be careful with lifetime
        // Let's use a temporary std::string to own the copy
        auto input_name_holder = m_ortSession->GetInputNameAllocated(0, allocator);
        std::string temp_input_name = input_name_holder.get(); // Make a copy
        // To store const char*, we need to ensure the string data outlives the pointer.
        // For now, let's assume the Run call takes std::vector<const char*> where these are valid.
        // This part is tricky with Ort C++ API string management.
        // A robust way is to copy them into std::string members and then get const char* from those.
        // For now, this might lead to dangling pointers if input_name_holder goes out of scope before Run.
        // Let's store the std::string.
        // m_inputNodeNames_std_str.push_back(input_name_holder.get());
        // m_inputNodeNames.push_back(m_inputNodeNames_std_str.back().c_str());
        // For simplicity in this stub, we'll assume the char* is valid for the session lifetime
        // or we'll need to manage memory for these names.
        // The example from ONNX Runtime often shows direct use.
        m_inputNodeNames.push_back(m_ortSession->GetInputNameAllocated(0, allocator).release()); // release ownership from unique_ptr like object

        Ort::TypeInfo input_type_info = m_ortSession->GetInputTypeInfo(0);
        auto input_tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
        m_inputShape = input_tensor_info.GetShape();
        // Note: m_inputShape might contain -1 for dynamic batch/height/width.
        // We expect {1, 3, targetHeight, targetWidth} after preprocessing.

        m_outputNodeNames.push_back(m_ortSession->GetOutputNameAllocated(0, allocator).release());

        qDebug() << "ONNX Model loaded successfully:" << modelPath;
        QString shapeStr = "{";
        for(size_t i = 0; i < m_inputShape.size(); ++i) {
            shapeStr += QString::number(m_inputShape[i]) + (i == m_inputShape.size() - 1 ? "" : ", ");
        }
        shapeStr += "}";
        qDebug() << "Input node name:" << m_inputNodeNames[0] << "Expected shape (from model):" << shapeStr;
        qDebug() << "Output node name:" << m_outputNodeNames[0];

        m_modelLoaded = true;
        return true;

    } catch (const Ort::Exception& e) {
        qWarning() << "ONNX Runtime exception during model load:" << e.what();
        m_ortSession.reset();
        m_modelLoaded = false;
        return false;
    } catch (const std::exception& e) {
        qWarning() << "Standard exception during model load:" << e.what();
        m_ortSession.reset();
        m_modelLoaded = false;
        return false;
    }
}

void WdVIT_TaggerEngine::unloadModel()
{
    if (m_ortSession) {
        m_ortSession.reset(); 
    }
    m_inputNodeNames.clear();
    m_outputNodeNames.clear();
    // m_tagVocabulary.clear(); // Vocabulary is cleared by unloadVocabulary
    // m_tagCategories.clear(); // Vocabulary is cleared by unloadVocabulary
    m_modelLoaded = false;
    unloadVocabulary(); // Also unload vocabulary when model is unloaded
    qDebug() << "ONNX Model unloaded.";
}

void WdVIT_TaggerEngine::unloadVocabulary()
{
    m_tagVocabulary.clear();
    m_tagCategories.clear();
    m_vocabularyLoaded = false;
    qDebug() << "Tag vocabulary unloaded.";
}

bool WdVIT_TaggerEngine::isModelLoaded() const
{
    return m_modelLoaded;
}

bool WdVIT_TaggerEngine::isVocabularyLoaded() const
{
    return m_vocabularyLoaded;
}

QStringList WdVIT_TaggerEngine::getKnownTags() const
{
    QStringList tags;
    if (!m_vocabularyLoaded) {
        qWarning() << "getKnownTags called but vocabulary is not loaded.";
        return tags;
    }
    for(const QString& tag : m_tagVocabulary) {
        tags.append(tag);
    }
    return tags;
}

bool WdVIT_TaggerEngine::loadTagVocabulary(const QString &tagsCsvPath)
{
    if (m_vocabularyLoaded) {
        unloadVocabulary();
    }
    QFile csvFile(tagsCsvPath);
    if (!csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open tags CSV file:" << tagsCsvPath;
        return false;
    }
    m_tagVocabulary.clear();
    m_tagCategories.clear();
    QTextStream in(&csvFile);
    
    if (!in.atEnd()) {
        in.readLine(); // Discard header line
    }

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.trimmed().isEmpty()) { 
            continue;
        }
        QStringList parts = line.split(',');
        if (parts.size() >= 3) { 
            QString tagName = parts.at(1).trimmed();
            bool ok;
            int category = parts.at(2).trimmed().toInt(&ok);
            
            if (!tagName.isEmpty() && ok) {
                m_tagVocabulary.push_back(tagName);
                m_tagCategories.push_back(category);
            } else {
                qWarning() << "Skipping malformed CSV line:" << line;
            }
        }
    }
    csvFile.close();
    if (m_tagVocabulary.empty() || m_tagCategories.size() != m_tagVocabulary.size()) {
        qWarning() << "Tag vocabulary is empty or inconsistent after reading CSV:" << tagsCsvPath;
        m_vocabularyLoaded = false;
        return false;
    }
    m_vocabularyLoaded = true;
    qDebug() << "Loaded" << m_tagVocabulary.size() << "tags from" << tagsCsvPath;
    return true;
}

WdVIT_TaggerEngine::PreprocessedImage WdVIT_TaggerEngine::preprocessImage(const QImage &image, int targetHeight, int targetWidth)
{
    PreprocessedImage result;
    if (image.isNull()) {
        qWarning() << "preprocessImage: Input image is null.";
        return result;
    }

    // 1. Resize
    QImage resizedImage = image.scaled(targetWidth, targetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // If aspect ratio is kept, pad or crop to exact target size if model requires it.
    // For now, assume scaled() gives us the right dimensions or model handles slight variations if dynamic.
    // If not, create a new QImage of targetWidth x targetHeight and paint resizedImage onto it, centered.
    if (resizedImage.width() != targetWidth || resizedImage.height() != targetHeight) {
        QImage exactSizeImage(targetWidth, targetHeight, QImage::Format_RGB888); // Ensure format
        exactSizeImage.fill(Qt::white); // Pad with white, as per Python script (255,255,255)
        QPainter painter(&exactSizeImage);
        int x = (targetWidth - resizedImage.width()) / 2;
        int y = (targetHeight - resizedImage.height()) / 2;
        painter.drawImage(x, y, resizedImage);
        resizedImage = exactSizeImage;
    }
    
    resizedImage = resizedImage.convertToFormat(QImage::Format_RGB888); // Ensure 3 channels, RGB

    // 2. Convert to NHWC float tensor (as per model's expected input shape)
    // Expected shape: {1, targetHeight, targetWidth, 3}
    result.shape = {1, static_cast<int64_t>(targetHeight), static_cast<int64_t>(targetWidth), 3};
    result.tensorValues.resize(1 * targetHeight * targetWidth * 3);

    // Normalization parameters (PLACEHOLDERS - NEED ACTUAL VALUES FROM SCRIPT)
    // Common for ImageNet:
    // float mean[3] = {0.485f, 0.456f, 0.406f}; // R, G, B
    // float stddev[3] = {0.229f, 0.224f, 0.225f}; // R, G, B
    // For wd-tagger, it might just be scaling to [0,1] or [-1,1]
    // The Python script uses:
    //   img = img.convert("RGB")
    //   img = np.array(img)
    //   img = img.astype(np.float32)
    //   img = img / 255.0  <-- Python script does NOT do this before model.run!
    //                           It passes float32 array with values [0-255] and BGR order.
    //                           This is unusual. Let's replicate it first.
    //                           If it fails, scaling to [0,1] is the first thing to try.
    //   H, W, C = img.shape
    //   img = img.reshape((1, H, W, C)) # NHWC
    // The ONNX model must be expecting BGR, [0-255], NHWC or NCHW.
    // For C++ API, we MUST provide NCHW.

    // float scale = 1.0f / 255.0f; // Still NOT scaling, values remain [0-255] to match Python script

    int i = 0;
    for (int h = 0; h < targetHeight; ++h) {
        for (int w = 0; w < targetWidth; ++w) {
            QRgb pixel = resizedImage.pixel(w, h);
            // NHWC order: B, G, R
            result.tensorValues[i++] = static_cast<float>(qBlue(pixel));
            result.tensorValues[i++] = static_cast<float>(qGreen(pixel));
            result.tensorValues[i++] = static_cast<float>(qRed(pixel));
        }
    }
    qDebug() << "Preprocessing: Image tensor created with shape {1," << targetHeight << "," << targetWidth << ",3}, BGR, float32, range [0-255]";
    return result;
}

QStringList WdVIT_TaggerEngine::postprocessOutput(const Ort::Value &outputTensor, const QVariantMap &settings)
{
    QStringList tags;
    if (m_tagVocabulary.empty()) { // Changed isEmpty() to empty()
        qWarning() << "Tag vocabulary is empty, cannot postprocess.";
        return tags;
    }

    // Get pointer to output tensor float values
    const float* logits = outputTensor.GetTensorData<float>();
    auto outputShapeInfo = outputTensor.GetTensorTypeAndShapeInfo();
    auto outputShape = outputShapeInfo.GetShape(); // Should be {1, num_tags} or {num_tags}

    size_t num_tags = outputShapeInfo.GetElementCount();
    if (num_tags != m_tagVocabulary.size()) {
        qWarning() << "Output tensor size" << num_tags << "does not match vocabulary size" << m_tagVocabulary.size();
        return tags;
    }

    // Apply settings from QVariantMap
    float general_thresh = settings.value("general_threshold", m_generalThreshold).toFloat();
    // The Python script uses a separate character_thresh. We'll use general_thresh for all for now,
    // unless specific character_thresh is provided in settings.
    float character_thresh = settings.value("character_threshold", general_thresh).toFloat(); // Default to general_thresh
    bool charTagsFirst = settings.value("char_tags_first", false).toBool();
    bool hideRatingTags = settings.value("hide_rating_tags", false).toBool();

    std::vector<std::pair<float, QString>> general_tag_pairs;
    std::vector<std::pair<float, QString>> character_tag_pairs;
    // Rating tags are typically category 9 as per CSV and Python script's LabelData
    // The Python script's LabelData also considers category 4 as character.
    // CSV categories: 0=general, 1=character(unused by py script?), 2=copyright, 3=artist, 4=character(used by py script)
    // 9=rating/meta.

    for (size_t i = 0; i < num_tags; ++i) {
        int category = m_tagCategories[i];
        const QString& tagName = m_tagVocabulary[i];
        float score = logits[i];

        if (category == 9) { // Rating/Meta tag
            if (!hideRatingTags && score > general_thresh) { // Use general_thresh for ratings too for now
                // Rating tags are usually not sorted by score but appended if above threshold
                // Or handle them separately if they need different logic
                 general_tag_pairs.push_back({score, tagName}); // Treat as general for now if not hidden
            }
        } else if (category == 4) { // Character tag (as per Python script's LabelData)
            if (score > character_thresh) {
                character_tag_pairs.push_back({score, tagName});
            }
        } else if (category == 0) { // General tag
             if (score > general_thresh) {
                general_tag_pairs.push_back({score, tagName});
            }
        }
        // Other categories (1, 2, 3) are ignored for now, similar to Python script's simplified handling
    }

    // Sort character and general tags by score (descending)
    std::sort(character_tag_pairs.rbegin(), character_tag_pairs.rend());
    std::sort(general_tag_pairs.rbegin(), general_tag_pairs.rend());

    if (charTagsFirst) {
        for(const auto& p : character_tag_pairs) tags.append(p.second);
        for(const auto& p : general_tag_pairs) tags.append(p.second);
    } else {
        for(const auto& p : general_tag_pairs) tags.append(p.second);
        for(const auto& p : character_tag_pairs) tags.append(p.second);
    }
    // Note: The Python script adds rating tags at the end regardless of charTagsFirst.
    // My current logic includes them in general_tag_pairs if not hidden.
    // This might need refinement if rating tags have special ordering.

    return tags;
}


QStringList WdVIT_TaggerEngine::generateTags(const QImage &image, const QVariantMap &settings)
{
    if (!m_modelLoaded || !m_ortSession) {
        qWarning() << "Model not loaded, cannot generate tags.";
        return QStringList();
    }
    if (image.isNull()) {
        qWarning() << "Input image is null for tag generation.";
        return QStringList();
    }

    // Assuming m_inputShape from model is {1, 3, H, W} where H, W might be dynamic (-1)
    // We need to use the specific target H, W for this model (e.g., 448x448)
    int targetHeight = 448; 
    int targetWidth = 448;  
    // If model shape is NHWC: {-1, H, W, C}
    if (m_inputShape.size() == 4 && m_inputShape[0] == -1) { // Dynamic batch
        if (m_inputShape[1] > 0) targetHeight = static_cast<int>(m_inputShape[1]); // Index 1 is Height
        if (m_inputShape[2] > 0) targetWidth = static_cast<int>(m_inputShape[2]);  // Index 2 is Width
        // m_inputShape[3] should be 3 (Channels)
        qDebug() << "Using model input HxW:" << targetHeight << "x" << targetWidth;
    } else {
        qDebug() << "Warning: Model input shape not as expected or not fully defined. Using default 448x448.";
    }

    PreprocessedImage pImg = preprocessImage(image, targetHeight, targetWidth);
    if (pImg.tensorValues.empty()) {
        return QStringList();
    }

    try {
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, pImg.tensorValues.data(), pImg.tensorValues.size(),
            pImg.shape.data(), pImg.shape.size()
        );

        auto output_tensors = m_ortSession->Run(Ort::RunOptions{nullptr}, 
                                                m_inputNodeNames.data(), &input_tensor, 1, 
                                                m_outputNodeNames.data(), 1);

        if (output_tensors.empty() || !output_tensors[0].IsTensor()) {
            qWarning() << "Failed to get valid output tensor from ONNX session.";
            return QStringList();
        }
        return postprocessOutput(output_tensors[0], settings);

    } catch (const Ort::Exception& e) {
        qWarning() << "ONNX Runtime exception during inference:" << e.what();
        return QStringList();
    } catch (const std::exception& e) {
        qWarning() << "Standard exception during inference:" << e.what();
        return QStringList();
    }
}
