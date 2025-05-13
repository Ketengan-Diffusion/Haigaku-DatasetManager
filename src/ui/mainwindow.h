#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QFutureWatcher>
#include <QMap> 
#include <QTimer> 
#include "ThumbnailListModel.h" // Added
#include "ThumbnailLoader.h"  // Added


// Forward declarations
QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QLabel;
class QTextEdit;
// class QListWidget; // Replaced by QListView
class QListView;      // Added
class QSplitter;
class QScrollArea;
class QVideoWidget; 
class QMediaPlayer; 
class QAudioOutput; 
class QPushButton;  
class QSlider;      
class QStackedWidget; 
class QRadioButton;   // Added
class QGroupBox;      // Added
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openDirectory();
    void showAboutDialog();
    void displayMediaAtIndex(int index); 
    void nextMedia();                    
    void previousMedia();                
    // void updateThumbnailInList(int row, const QIcon &icon); // Removed, model handles this
    void performAutoSave(); 
    void showStatisticsDialog(); 
    void onThumbnailViewClicked(const QModelIndex &index); 
    void onThumbnailViewScrolled();     
    void loadVisibleThumbnails();       
    void onCaptionModeChanged(); 
    void deleteCurrentMediaItem(); // Slot for deleting current media

private:
    void setupUI();
    void createMenus();
    void createStatusBar();
    void loadFiles(const QString &dirPath);
    void updateFileDetails(const QString &filePath);
    void loadCaptionForCurrentImage();
    void saveCurrentCaption(); // New slot for saving
    void applyScoreToCaption(int score); // New slot for scoring


    // UI Elements
    QLabel *imageDisplayLabel;       
    QVideoWidget *videoDisplayWidget; 
    QStackedWidget *mediaDisplayContainer;  // Changed type to QStackedWidget*
    QScrollArea *imageScrollArea;    
    
    QTextEdit *captionEditor;
    QListView *thumbnailListView; // Changed from QListWidget
    QLabel *fileDetailsLabel;

    // Thumbnail Handling
    ThumbnailListModel *m_thumbnailModel;
    ThumbnailLoader *m_thumbnailLoaderService;

    // Caption Mode UI
    QGroupBox *captionModeGroupBox;
    QRadioButton *nlpModeRadioButton;
    QRadioButton *tagsModeRadioButton;

    // Video Controls
    QWidget *videoControlsWidget; 
    QPushButton *playPauseButton;
    QSlider *seekerSlider;
    QLabel *durationLabel;


    QSplitter *mainSplitter;
    QSplitter *rightPanelSplitter; 

    // Media Playback
    QMediaPlayer *mediaPlayer;
    QAudioOutput *audioOutput;


    // Data
    QString currentDirectory;
    QStringList mediaFiles; 
    int currentMediaIndex;
    bool captionChangedSinceLoad; 
    QSize thumbnailDefaultSize; 
    QMap<QString, QString> unsavedCaptions; 
    QTimer *autoSaveTimer;
    QTimer *m_scrollStopTimer; // Timer to detect when scrolling has stopped

    // Menu actions
    QAction *openDirAction;
    QAction *exitAction;
    QAction *aboutAction;
    QAction *aboutQtAction; 
    QAction *statisticsAction; // New action for statistics menu

protected:
    void keyPressEvent(QKeyEvent *event) override;
};

#endif // MAINWINDOW_H
