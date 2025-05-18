#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QFutureWatcher>
#include <QMap> 
#include <QTimer> 
#include "models/ThumbnailListModel.h" 
#include "services/ThumbnailLoader.h"  
#include "ui/AutoCaptionSettingsPanel.h" 
#include "services/AutoCaptionManager.h"
#include "ui/TagEditorWidget.h" // Added

// Forward declarations
QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QLabel;
class QTextEdit;
class QListView;      
class QSplitter;
class QScrollArea;
class QVideoWidget; 
class QMediaPlayer; 
class QAudioOutput; 
class QPushButton;  
class QToolButton; 
class QSlider;      
class QStackedWidget; 
class QToolBar; 
class QPropertyAnimation; 
class QGraphicsOpacityEffect; 
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
    void performAutoSave(); 
    void showStatisticsDialog(); 
    void onThumbnailViewClicked(const QModelIndex &index); 
    void onThumbnailViewScrolled();     
    void loadVisibleThumbnails();       
    void deleteCurrentMediaItem(); 
    void toggleAutoCaptionPanel(); 
    
    void onBulbButtonClicked();
    void updateCaptionWithSuggestion(const QStringList &tags, const QString &forImagePath, bool autoFill); 
    void handleAutoCaptionError(const QString &errorMessage);
    void showAutoCaptionSettingsDialog(); 
    void onCaptionEditingModeChanged(); 
    void handleModelStatusChanged(const QString &status, const QString &color); // New slot

    void fabAutoHideTimeout(); 
    void onRefreshThumbnails(); 
    void fabAnimationFinished(); 
    void openProject();          
    void saveProject();          
    void saveProjectAs();        

private:
    void setupUI();
    void createMenus();
    void createStatusBar();
    void loadFiles(const QString &dirPath);
    void updateFileDetails(const QString &filePath);
    void loadCaptionForCurrentImage();
    void saveCurrentCaption(); 
    void applyScoreToCaption(int score); 


    // UI Elements
    QLabel *imageDisplayLabel;       
    QVideoWidget *videoDisplayWidget; 
    QStackedWidget *mediaDisplayContainer;  
    QScrollArea *imageScrollArea;    
    
    QStackedWidget *m_captionInputStackedWidget; // Added
    QTextEdit *captionEditor;      // For NLP mode
    TagEditorWidget *m_tagEditorWidget; // For Tags mode

    QListView *thumbnailListView; 
    QLabel *fileDetailsLabel;
    QToolButton *m_bulbButton; 

    // Thumbnail Handling
    ThumbnailListModel *m_thumbnailModel;
    ThumbnailLoader *m_thumbnailLoaderService;

    // Auto Captioning UI & Logic
    QToolButton *m_sparkleActionButton;          
    AutoCaptionSettingsPanel *m_autoCaptionSettingsPanel; 
    QPropertyAnimation *m_settingsPanelAnimation;  
    bool m_isAutoCaptionPanelVisible;
    
    // Floating Action Button (FAB) related
    QTimer *m_fabAutoHideTimer;                 
    QGraphicsOpacityEffect *m_fabOpacityEffect; 
    QPropertyAnimation *m_fabFadeAnimation;     
    bool m_isFabDragging;                       
    QPoint m_fabDragStartPosition;              

    AutoCaptionManager *m_autoCaptionManager;
    QString m_suggestedCaption; 

    QButtonGroup *m_captionModeSwitchGroup; // Corrected type to QButtonGroup
    QRadioButton *m_nlpModeRadioMain;         
    QRadioButton *m_tagsModeRadioMain;        

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
    QString m_currentProjectPath; 
    QStringList mediaFiles; 
    int currentMediaIndex;
    bool captionChangedSinceLoad; 
    QSize thumbnailDefaultSize; 
    QMap<QString, QString> unsavedCaptions; 
    QTimer *autoSaveTimer;
    QTimer *m_scrollStopTimer; 

    // Menu actions
    QAction *openDirAction;
    QAction *openProjectAction;      
    QAction *saveProjectAction;      
    QAction *saveProjectAsAction;    
    QAction *exitAction;
    QAction *aboutAction;
    QAction *aboutQtAction; 
    QAction *statisticsAction; 
    QAction *refreshThumbnailsAction; 

    bool m_storeManualTagsWithUnderscores; // User preference for tag storage

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override; 
    bool eventFilter(QObject *watched, QEvent *event) override; 
    void mouseMoveEvent(QMouseEvent *event) override; 
};

#endif // MAINWINDOW_H
