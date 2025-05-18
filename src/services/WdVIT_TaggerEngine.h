#ifndef WDVIT_TAGGERENGINE_H
#define WDVIT_TAGGERENGINE_H

#include <QString>
#include <QStringList>
#include <QImage>
#include <QVariantMap>
#include <vector>
#include <memory> // For std::unique_ptr

// ONNX Runtime C++ API
// Ensure this path is correct based on your ONNXRUNTIME_INCLUDE_DIR setup
#include "onnxruntime_cxx_api.h" 

class WdVIT_TaggerEngine
{
public:
    WdVIT_TaggerEngine();
    ~WdVIT_TaggerEngine();

    bool loadModel(const QString &modelPath, const QString &tagsCsvPath, 
                   bool useCpu = true, bool useDirectML = false, bool useCuda = false);
    bool loadTagVocabulary(const QString &tagsCsvPath); // New method
    void unloadModel(); // Will also clear vocabulary
    void unloadVocabulary(); // New method
    bool isModelLoaded() const;
    bool isVocabularyLoaded() const; // New method
    QStringList getKnownTags() const; 

    QStringList generateTags(const QImage &image, const QVariantMap &settings);

private:
    struct PreprocessedImage {
        std::vector<float> tensorValues;
        std::vector<int64_t> shape; // Should be {1, 3, H, W}
    };

    PreprocessedImage preprocessImage(const QImage &image, int targetHeight, int targetWidth);
    QStringList postprocessOutput(const Ort::Value &outputTensor, const QVariantMap &settings);

    Ort::Env m_ortEnv;
    std::unique_ptr<Ort::Session> m_ortSession;
    Ort::AllocatorWithDefaultOptions m_allocator;
    
    std::vector<const char*> m_inputNodeNames;  // Store from session
    std::vector<const char*> m_outputNodeNames; // Store from session
    std::vector<int64_t> m_inputShape; 

    std::vector<QString> m_tagVocabulary; 
    std::vector<int> m_tagCategories; // Added to store category for each tag

    // Model-specific settings (placeholders for now)
    float m_generalThreshold;
    // float m_characterThreshold; // If different thresholds are used

    bool m_modelLoaded;
    bool m_vocabularyLoaded; // New flag
};

#endif // WDVIT_TAGGERENGINE_H
