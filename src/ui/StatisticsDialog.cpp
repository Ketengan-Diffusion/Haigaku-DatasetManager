#include "StatisticsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QFileDialog> // For QDir, QFileInfo
#include <QImageReader> // For image dimensions
#include <QtConcurrent/QtConcurrent> // For background calculation
#include <QDebug>

QString DatasetStatistics::toString() const {
    QString result;
    result += QString("Total Media Files Processed: %1\n").arg(totalFilesProcessed);
    result += QString("Total Media Size: %1 MB\n").arg(totalMediaSize / (1024.0 * 1024.0), 0, 'f', 2);
    result += QString("Image Count: %1\n").arg(imageCount);
    result += QString("Video Count: %1\n").arg(videoCount);
    result += QString("Captioned Media: %1\n").arg(captionedMediaCount);
    result += QString("Unpaired Captions (Media without .txt/.caption): %1\n").arg(unpairedCaptionCount);
    
    if (imageCount > 0) {
        result += QString("Max Image Pixels: %L1\n").arg(maxPixels);
        result += QString("Average Image Pixels: %L1\n").arg(static_cast<long long>(averagePixels));
    }
    
    int nonEmptyCaptionCount = captionedMediaCount; // Approximation for now
    if (nonEmptyCaptionCount > 0) {
        result += QString("Max Caption Length (words): %1\n").arg(maxCaptionLengthWords);
        result += QString("Min Caption Length (words): %1\n").arg(minCaptionLengthWords == -1 ? "N/A" : QString::number(minCaptionLengthWords));
        result += QString("Average Caption Length (words): %1\n").arg(averageCaptionLengthWords, 0, 'f', 1);
    }
    return result;
}

StatisticsDialog::StatisticsDialog(const QStringList &mediaFiles, const QString &currentDirectory, QWidget *parent)
    : QDialog(parent), mediaFilePaths(mediaFiles), baseDirectoryPath(currentDirectory)
{
    setupUI();
    setWindowTitle(tr("Dataset Statistics"));
    setMinimumSize(500, 400);

    // Connect the progress signal to the slot that updates the UI
    connect(this, &StatisticsDialog::progressUpdated, this, &StatisticsDialog::updateProgress);
}

StatisticsDialog::~StatisticsDialog()
{
}

void StatisticsDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    statisticsDisplay = new QTextEdit(this);
    statisticsDisplay->setReadOnly(true);
    statisticsDisplay->setFontFamily("monospace"); // Good for aligned text
    mainLayout->addWidget(statisticsDisplay, 1);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 0); // Indeterminate initially
    progressBar->setVisible(false); // Hide until calculation starts
    mainLayout->addWidget(progressBar);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    calculateButton = new QPushButton(tr("Calculate Statistics"), this);
    connect(calculateButton, &QPushButton::clicked, this, &StatisticsDialog::calculateStatistics);
    buttonLayout->addWidget(calculateButton);

    buttonLayout->addStretch();

    closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this, &StatisticsDialog::accept);
    buttonLayout->addWidget(closeButton);
    
    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);

    statisticsDisplay->setText(tr("Click 'Calculate Statistics' to begin.\nAnalysis might take some time for large datasets."));
}

void StatisticsDialog::calculateStatistics()
{
    if (mediaFilePaths.isEmpty()) {
        statisticsDisplay->setText(tr("No media files loaded in the main application."));
        return;
    }

    calculateButton->setEnabled(false);
    closeButton->setEnabled(false);
    progressBar->setRange(0, mediaFilePaths.count());
    progressBar->setValue(0);
    progressBar->setVisible(true);
    statisticsDisplay->setText(tr("Calculating..."));

    // Run calculations in a background thread
    // performCalculations now uses member variables mediaFilePaths and baseDirectoryPath
    QFuture<DatasetStatistics> future = QtConcurrent::run([this]() {
        return this->performCalculations(); 
    });

    // Use QFutureWatcher to get the result on the main thread
    QFutureWatcher<DatasetStatistics> *watcher = new QFutureWatcher<DatasetStatistics>(this);
    connect(watcher, &QFutureWatcher<DatasetStatistics>::finished, this, [this, watcher]() {
        DatasetStatistics stats = watcher->result();
        updateStatisticsDisplay(stats);
        calculateButton->setEnabled(true);
        closeButton->setEnabled(true);
        progressBar->setVisible(false);
        watcher->deleteLater(); // Clean up watcher
    });
    // If we need progress updates from performCalculations, it would emit signals
    // connected to updateProgress slot. For now, performCalculations is synchronous internally.
    watcher->setFuture(future);
}


DatasetStatistics StatisticsDialog::performCalculations() // Now uses member variables
{
    DatasetStatistics stats;
    stats.totalFilesProcessed = mediaFilePaths.count(); // Use member variable
    long long totalPixels = 0;
    int captionWordSum = 0;
    int nonEmptyCaptions = 0;

    QStringList imageExtensions = {"jpg", "jpeg", "png", "bmp", "gif", "webp", "tiff"};
    QStringList videoExtensions = {"mp4", "mkv", "webm"};

    for (int i = 0; i < mediaFilePaths.count(); ++i) { // Use member variable
        const QString &filePath = mediaFilePaths.at(i); // Use member variable
        QFileInfo fileInfo(filePath);
        stats.totalMediaSize += fileInfo.size();

        bool isImage = imageExtensions.contains(fileInfo.suffix().toLower());
        bool isVideo = videoExtensions.contains(fileInfo.suffix().toLower());

        if (isImage) {
            stats.imageCount++;
            QImageReader reader(filePath);
            QSize s = reader.size();
            if (s.isValid()) {
                long long currentPixels = static_cast<long long>(s.width()) * s.height();
                totalPixels += currentPixels;
                if (currentPixels > stats.maxPixels) {
                    stats.maxPixels = currentPixels;
                }
            }
        } else if (isVideo) {
            stats.videoCount++;
        }

        // Caption analysis
        QString captionPathTxt = fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + ".txt";
        QString captionPathCaption = fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + ".caption";
        QFile captionFile;
        bool captionExists = false;
        if (QFile::exists(captionPathTxt)) {
            captionFile.setFileName(captionPathTxt);
            captionExists = true;
        } else if (QFile::exists(captionPathCaption)) {
            captionFile.setFileName(captionPathCaption);
            captionExists = true;
        }

        if (captionExists) {
            stats.captionedMediaCount++;
            if (captionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString captionContent = captionFile.readAll();
                QStringList words = captionContent.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                int wordCount = words.count();
                if (wordCount > 0) {
                    nonEmptyCaptions++;
                    captionWordSum += wordCount;
                    if (wordCount > stats.maxCaptionLengthWords) {
                        stats.maxCaptionLengthWords = wordCount;
                    }
                    if (stats.minCaptionLengthWords == -1 || wordCount < stats.minCaptionLengthWords) {
                        stats.minCaptionLengthWords = wordCount;
                    }
                }
                captionFile.close();
            }
        } else {
            stats.unpairedCaptionCount++;
        }
        
        
        // Emit progress update
        
        // Emit progress update less frequently to reduce signal overhead
        if ((i + 1) % 10 == 0 || (i + 1) == mediaFilePaths.count()) {
            emit progressUpdated(i + 1, mediaFilePaths.count());
        }
    }

    // Ensure final progress is emitted if not caught by modulo
    if (mediaFilePaths.count() % 10 != 0) {
        emit progressUpdated(mediaFilePaths.count(), mediaFilePaths.count());
    }


    if (stats.imageCount > 0) {
        stats.averagePixels = static_cast<double>(totalPixels) / stats.imageCount;
    }
    if (nonEmptyCaptions > 0) {
        stats.averageCaptionLengthWords = static_cast<double>(captionWordSum) / nonEmptyCaptions;
    }
    if (stats.minCaptionLengthWords == -1 && nonEmptyCaptions > 0) { // If all captions were empty but existed
        stats.minCaptionLengthWords = 0;
    }


    return stats;
}

void StatisticsDialog::updateStatisticsDisplay(const DatasetStatistics &stats)
{
    statisticsDisplay->setText(stats.toString());
    progressBar->setValue(progressBar->maximum()); // Mark as complete
}

void StatisticsDialog::updateProgress(int processedCount, int totalCount)
{
    if (progressBar->maximum() != totalCount) {
        progressBar->setMaximum(totalCount);
    }
    progressBar->setValue(processedCount);
}
