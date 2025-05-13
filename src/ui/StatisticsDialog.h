#ifndef STATISTICSDIALOG_H
#define STATISTICSDIALOG_H

#include <QDialog>
#include <QStringList> // To pass media file list

QT_BEGIN_NAMESPACE
class QTextEdit;
class QPushButton;
class QVBoxLayout;
class QProgressBar; // To show calculation progress
QT_END_NAMESPACE

// Struct to hold calculated statistics
struct DatasetStatistics {
    long long totalMediaSize = 0;
    int captionedMediaCount = 0;
    int imageCount = 0;
    int videoCount = 0;
    long long maxPixels = 0; // width * height
    double averagePixels = 0.0;
    int unpairedCaptionCount = 0;
    int maxCaptionLengthWords = 0;
    int minCaptionLengthWords = -1; // -1 indicates not set or no non-empty captions
    double averageCaptionLengthWords = 0.0;
    int totalFilesProcessed = 0; // For progress tracking

    QString toString() const; // Helper to format for display
};


class StatisticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StatisticsDialog(const QStringList &mediaFiles, const QString &currentDirectory, QWidget *parent = nullptr);
    ~StatisticsDialog();

private slots:
    void calculateStatistics();
    void updateStatisticsDisplay(const DatasetStatistics &stats);
    void updateProgress(int processedCount, int totalCount); // Slot to update progress bar

signals:
    void progressUpdated(int processedCount, int totalCount); // Signal for progress

private:
    void setupUI();
    DatasetStatistics performCalculations(); // Takes no args, uses member variables


    QTextEdit *statisticsDisplay;
    QPushButton *calculateButton;
    QPushButton *closeButton;
    QProgressBar *progressBar;

    QStringList mediaFilePaths; // Copy of the list from MainWindow
    QString baseDirectoryPath;  // The directory being analyzed
};

#endif // STATISTICSDIALOG_H
