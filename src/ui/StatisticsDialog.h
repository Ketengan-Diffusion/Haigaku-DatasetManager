#ifndef STATISTICSDIALOG_H
#define STATISTICSDIALOG_H

#include <QDialog>
#include <QStringList> 
#include <QMap> // For tag frequencies

QT_BEGIN_NAMESPACE
class QTextEdit;
class QPushButton;
class QVBoxLayout;
class QProgressBar; 
class QTabWidget; // Added for tabbing
QT_END_NAMESPACE

class WordCloudWidget; // Forward declaration

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
    int totalFilesProcessed = 0; 
    QMap<QString, int> tagFrequencies; // Added to hold tag counts

    QString toString() const; 
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
    void updateProgress(int processedCount, int totalCount); 

signals:
    void progressUpdated(int processedCount, int totalCount); 

private:
    void setupUI();
    DatasetStatistics performCalculations(); 


    QTabWidget *m_tabWidget; // Added
    QTextEdit *m_statisticsTextDisplay; // Renamed for clarity
    WordCloudWidget *m_wordCloudWidget; // Added

    QPushButton *m_calculateButton; // Renamed
    QPushButton *m_closeButton;    // Renamed
    QProgressBar *m_progressBar;   

    QPushButton *m_zoomInButton;    // Added
    QPushButton *m_zoomOutButton;   // Added

    QStringList m_mediaFilePaths; 
    QString m_baseDirectoryPath; 
};

#endif // STATISTICSDIALOG_H
