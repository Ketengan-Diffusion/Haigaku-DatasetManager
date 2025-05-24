#include "StatisticsDialog.h"
#include "WordCloudWidget.h" // Added
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QImageReader>
#include <QDir>
#include <QDebug>
#include <QTabWidget> // Added
#include <QRegularExpression> // For splitting tags

// Implementation of DatasetStatistics::toString()
QString DatasetStatistics::toString() const {
    QString result;
    result += QString("Total Media Size: %1 MB\n").arg(totalMediaSize / (1024.0 * 1024.0), 0, 'f', 2);
    result += QString("Captioned Media Count: %1\n").arg(captionedMediaCount);
    result += QString("Image Count: %1\n").arg(imageCount);
    result += QString("Video Count: %1\n").arg(videoCount);
    result += QString("Unpaired Caption Files: %1\n").arg(unpairedCaptionCount);
    result += QString("Max Pixels (W*H): %1\n").arg(maxPixels);
    result += QString("Average Pixels (W*H): %1\n").arg(averagePixels, 0, 'f', 0);
    if (minCaptionLengthWords != -1) {
        result += QString("Min Caption Length (words): %1\n").arg(minCaptionLengthWords);
    } else {
        result += QString("Min Caption Length (words): N/A\n");
    }
    result += QString("Max Caption Length (words): %1\n").arg(maxCaptionLengthWords);
    result += QString("Average Caption Length (words): %1\n").arg(averageCaptionLengthWords, 0, 'f', 2);
    result += QString("Total Files Processed: %1\n").arg(totalFilesProcessed);
    return result;
}


StatisticsDialog::StatisticsDialog(const QStringList &mediaFiles, const QString &currentDirectory, QWidget *parent)
    : QDialog(parent), m_mediaFilePaths(mediaFiles), m_baseDirectoryPath(currentDirectory)
{
    setWindowTitle(tr("Dataset Statistics"));
    setMinimumSize(500, 400);
    setupUI();
    connect(m_calculateButton, &QPushButton::clicked, this, &StatisticsDialog::calculateStatistics);
    connect(m_closeButton, &QPushButton::clicked, this, &StatisticsDialog::accept);
    connect(this, &StatisticsDialog::progressUpdated, this, &StatisticsDialog::updateProgress);
}

StatisticsDialog::~StatisticsDialog()
{
}

void StatisticsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this); // Create tab widget

    // Statistics Text Tab
    QWidget *textStatsPage = new QWidget(this);
    QVBoxLayout *textStatsLayout = new QVBoxLayout(textStatsPage);
    m_statisticsTextDisplay = new QTextEdit(this);
    m_statisticsTextDisplay->setReadOnly(true);
    textStatsLayout->addWidget(m_statisticsTextDisplay);
    textStatsPage->setLayout(textStatsLayout);
    m_tabWidget->addTab(textStatsPage, tr("Summary"));

    // Word Cloud Tab
    m_wordCloudWidget = new WordCloudWidget(this);
    m_tabWidget->addTab(m_wordCloudWidget, tr("Word Cloud"));
    
    mainLayout->addWidget(m_tabWidget); // Add tab widget to main layout

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);

    QHBoxLayout *bottomButtonLayout = new QHBoxLayout(); // Renamed for clarity
    m_calculateButton = new QPushButton(tr("Calculate Statistics"), this);
    m_closeButton = new QPushButton(tr("Close"), this);

    // Zoom buttons
    m_zoomInButton = new QPushButton("+", this);
    m_zoomInButton->setToolTip(tr("Zoom In Word Cloud"));
    m_zoomInButton->setFixedSize(30, 30);
    m_zoomOutButton = new QPushButton("-", this);
    m_zoomOutButton->setToolTip(tr("Zoom Out Word Cloud"));
    m_zoomOutButton->setFixedSize(30, 30);

    bottomButtonLayout->addWidget(m_calculateButton);
    bottomButtonLayout->addStretch();
    bottomButtonLayout->addWidget(m_zoomOutButton);
    bottomButtonLayout->addWidget(m_zoomInButton);
    bottomButtonLayout->addSpacing(20); // Space before close button
    bottomButtonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(bottomButtonLayout);

    setLayout(mainLayout);

    // Connect zoom buttons
    if (m_wordCloudWidget) {
        connect(m_zoomInButton, &QPushButton::clicked, m_wordCloudWidget, &WordCloudWidget::zoomIn);
        connect(m_zoomOutButton, &QPushButton::clicked, m_wordCloudWidget, &WordCloudWidget::zoomOut);
    }
}

void StatisticsDialog::calculateStatistics()
{
    m_calculateButton->setEnabled(false);
    m_progressBar->setValue(0);
    DatasetStatistics stats = performCalculations();
    updateStatisticsDisplay(stats);
    m_calculateButton->setEnabled(true);
}

void StatisticsDialog::updateStatisticsDisplay(const DatasetStatistics &stats)
{
    m_statisticsTextDisplay->setText(stats.toString());
    if (m_wordCloudWidget) {
        m_wordCloudWidget->setWordData(stats.tagFrequencies);
    }
}

void StatisticsDialog::updateProgress(int processedCount, int totalCount)
{
    if (totalCount > 0) {
        int percentage = static_cast<int>((static_cast<double>(processedCount) / totalCount) * 100.0);
        m_progressBar->setValue(percentage);
    }
}

DatasetStatistics StatisticsDialog::performCalculations()
{
    DatasetStatistics stats;
    stats.totalFilesProcessed = m_mediaFilePaths.size();
    int currentFileNum = 0;

    long long totalPixelSum = 0;
    int mediaWithPixelsCount = 0;
    long long totalCaptionWords = 0;
    int captionsWithWordsCount = 0;

    QRegularExpression commaSeparator(QStringLiteral("\\s*,\\s*")); // Matches comma possibly surrounded by whitespace

    for (const QString &filePath : m_mediaFilePaths) {
        emit progressUpdated(++currentFileNum, stats.totalFilesProcessed);
        QCoreApplication::processEvents(); // Keep UI responsive

        QFileInfo fileInfo(filePath);
        stats.totalMediaSize += fileInfo.size();

        QString extension = fileInfo.suffix().toLower();
        if (extension == "jpg" || extension == "jpeg" || extension == "png" || extension == "bmp" || extension == "gif" || extension == "webp") {
            stats.imageCount++;
            QImageReader reader(filePath);
            if (reader.canRead()) {
                QSize size = reader.size();
                if (size.isValid()) {
                    long long pixels = static_cast<long long>(size.width()) * size.height();
                    totalPixelSum += pixels;
                    mediaWithPixelsCount++;
                    if (pixels > stats.maxPixels) {
                        stats.maxPixels = pixels;
                    }
                }
            }
        } else if (extension == "mp4" || extension == "mkv" || extension == "avi" || extension == "mov" || extension == "webm" || extension == "flv") {
            stats.videoCount++;
            // Video pixel/resolution stats might require a multimedia library like FFmpeg, skip for now for simplicity
        }

        QString captionPath = fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + ".txt";
        QFile captionFile(captionPath);
        if (captionFile.exists() && captionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            stats.captionedMediaCount++;
            QTextStream in(&captionFile);
            QString content = in.readAll().trimmed();
            captionFile.close();

            if (!content.isEmpty()) {
                QStringList tags = content.split(commaSeparator, Qt::SkipEmptyParts);
                int wordCount = 0;
                for (const QString& tag : tags) {
                    QString cleanTag = tag.trimmed();
                    if (!cleanTag.isEmpty()) {
                        wordCount++; // Each comma-separated item is a "word" or "tag" for this purpose
                        stats.tagFrequencies[cleanTag.toLower()]++; // Case-insensitive counting
                    }
                }

                if (wordCount > 0) {
                    totalCaptionWords += wordCount;
                    captionsWithWordsCount++;
                    if (wordCount > stats.maxCaptionLengthWords) {
                        stats.maxCaptionLengthWords = wordCount;
                    }
                    if (stats.minCaptionLengthWords == -1 || wordCount < stats.minCaptionLengthWords) {
                        stats.minCaptionLengthWords = wordCount;
                    }
                }
            }
        } else {
            // Check if a .caption file exists if .txt doesn't
            captionPath = fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + ".caption";
            QFile captionFileAlternate(captionPath);
            if (captionFileAlternate.exists() && captionFileAlternate.open(QIODevice::ReadOnly | QIODevice::Text)) {
                 stats.captionedMediaCount++;
                QTextStream in(&captionFileAlternate);
                QString content = in.readAll().trimmed();
                captionFileAlternate.close();
                if (!content.isEmpty()) {
                    QStringList tags = content.split(commaSeparator, Qt::SkipEmptyParts);
                     int wordCount = 0;
                    for (const QString& tag : tags) {
                        QString cleanTag = tag.trimmed();
                        if (!cleanTag.isEmpty()) {
                            wordCount++;
                            stats.tagFrequencies[cleanTag.toLower()]++;
                        }
                    }
                    if (wordCount > 0) {
                        totalCaptionWords += wordCount;
                        captionsWithWordsCount++;
                        if (wordCount > stats.maxCaptionLengthWords) stats.maxCaptionLengthWords = wordCount;
                        if (stats.minCaptionLengthWords == -1 || wordCount < stats.minCaptionLengthWords) stats.minCaptionLengthWords = wordCount;
                    }
                }
            } else if (QFile(fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + ".txt").exists() == false && 
                       QFile(fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + ".caption").exists() == false) {
                // This logic is a bit off, unpaired should be if media exists but caption doesn't
            }
        }
    }
    
    // Calculate unpaired captions (media files without any corresponding .txt or .caption)
    // This needs to be done carefully. The current loop structure is per media file.
    // A separate loop or check might be better for unpaired captions.
    // For now, let's assume the current logic for captionedMediaCount is primary.
    // stats.unpairedCaptionCount = stats.totalFilesProcessed - stats.captionedMediaCount; // This is a simplification

    if (mediaWithPixelsCount > 0) {
        stats.averagePixels = static_cast<double>(totalPixelSum) / mediaWithPixelsCount;
    }
    if (captionsWithWordsCount > 0) {
        stats.averageCaptionLengthWords = static_cast<double>(totalCaptionWords) / captionsWithWordsCount;
    }
    
    // Placeholder for unpaired caption logic - needs refinement
    // Iterate all .txt and .caption files in the directory and see which ones don't have matching media
    // Or, more simply, for each media file, if no caption was found, increment unpaired.
    // The current logic for captionedMediaCount already does this implicitly.
    // So, unpaired is total - captioned.
    int actualCaptionedCount = 0;
    QMap<QString, bool> mediaHasCaption;

    for (const QString &filePath : m_mediaFilePaths) {
        QFileInfo fileInfo(filePath);
        QString baseName = fileInfo.completeBaseName();
        if (QFile(fileInfo.absolutePath() + "/" + baseName + ".txt").exists() ||
            QFile(fileInfo.absolutePath() + "/" + baseName + ".caption").exists()) {
            actualCaptionedCount++;
        }
    }
    stats.captionedMediaCount = actualCaptionedCount; // Corrected count
    stats.unpairedCaptionCount = stats.totalFilesProcessed - stats.captionedMediaCount;


    emit progressUpdated(stats.totalFilesProcessed, stats.totalFilesProcessed); // Final update
    return stats;
}
