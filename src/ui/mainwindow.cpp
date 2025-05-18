#include "MainWindow.h" 
#include "StatisticsDialog.h" 
#include "AutoCaptionSettingsPanel.h" 
#include "AutoCaptionSettingsDialog.h" 
#include "models/ThumbnailListModel.h" 
#include "services/ThumbnailLoader.h"  
#include "services/AutoCaptionManager.h" 
#include "ui/TagEditorWidget.h" 
#include "utils/QFlowLayout.h" 

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QTextEdit>
#include <QListView>      
#include <QScrollBar> 
#include <QSplitter>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout> 
#include <QStackedWidget> 
#include <QStatusBar>
#include <QPixmap>
#include <QImageReader>
#include <QFileInfo>
#include <QDir>
#include <QKeyEvent>
#include <QMouseEvent> 
#include <QPainter> 
#include <QDebug>
#include <QIcon> 
#include <QTimer> 
#include <QStandardPaths> 
#include <QDataStream> 
#include <QToolButton>    
#include <QToolBar>       
#include <QPropertyAnimation> 
#include <QButtonGroup> 
#include <QGraphicsOpacityEffect> 
#include <QToolTip> 
#include <QSettings> 

#include <QtConcurrent/QtConcurrent> 
#include <QFuture>

#include <QVideoWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QPushButton>
#include <QSlider>
#include <QStyle> 

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , imageDisplayLabel(nullptr)
    , videoDisplayWidget(nullptr)
    , mediaDisplayContainer(nullptr)
    , imageScrollArea(nullptr)
    , m_captionInputStackedWidget(nullptr) 
    , captionEditor(nullptr)               
    , m_tagEditorWidget(nullptr)           
    , thumbnailListView(nullptr) 
    , m_thumbnailModel(nullptr)  
    , m_thumbnailLoaderService(nullptr) 
    , m_bulbButton(nullptr)            
    , m_sparkleActionButton(nullptr)       
    , m_autoCaptionSettingsPanel(nullptr)   
    , m_settingsPanelAnimation(nullptr)     
    , m_isAutoCaptionPanelVisible(false)    
    , m_autoCaptionManager(nullptr)     
    , m_fabAutoHideTimer(nullptr)
    , m_fabOpacityEffect(nullptr)
    , m_fabFadeAnimation(nullptr)
    , m_isFabDragging(false)                
    , m_fabDragStartPosition()              
    , m_captionModeSwitchGroup(nullptr) 
    , m_nlpModeRadioMain(nullptr)       
    , m_tagsModeRadioMain(nullptr)      
    , fileDetailsLabel(nullptr)
    , videoControlsWidget(nullptr)
    , playPauseButton(nullptr)
    , seekerSlider(nullptr)
    , durationLabel(nullptr)
    , mainSplitter(nullptr)
    , rightPanelSplitter(nullptr) 
    , mediaPlayer(nullptr)
    , audioOutput(nullptr)
    , currentMediaIndex(-1)
    , captionChangedSinceLoad(false)
    , thumbnailDefaultSize(180, 100)
    , autoSaveTimer(nullptr)
    , m_scrollStopTimer(nullptr) 
    , openDirAction(nullptr) 
    , openProjectAction(nullptr)
    , saveProjectAction(nullptr)
    , saveProjectAsAction(nullptr)
    , exitAction(nullptr)
    , aboutAction(nullptr)
    , aboutQtAction(nullptr)
    , statisticsAction(nullptr)
    , refreshThumbnailsAction(nullptr)
    , m_currentProjectPath("") 
    , m_storeManualTagsWithUnderscores(false) // Initialize setting
{
    resize(1200, 800);
    setMouseTracking(true); 

    // Load persistent settings
    QSettings settings("KetenganDiffusion", "HaigakuManager");
    m_storeManualTagsWithUnderscores = settings.value("storeManualTagsWithUnderscores", false).toBool();


    mediaPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    mediaPlayer->setAudioOutput(audioOutput);

    m_thumbnailModel = new ThumbnailListModel(this);
    m_thumbnailLoaderService = new ThumbnailLoader(this);
    m_thumbnailModel->setThumbnailLoader(m_thumbnailLoaderService);
    m_thumbnailModel->setThumbnailSize(thumbnailDefaultSize);

    m_autoCaptionManager = new AutoCaptionManager(this); 

    m_scrollStopTimer = new QTimer(this);
    m_scrollStopTimer->setSingleShot(true);
    m_scrollStopTimer->setInterval(500); 
    connect(m_scrollStopTimer, &QTimer::timeout, this, &MainWindow::loadVisibleThumbnails);
    
    m_fabAutoHideTimer = new QTimer(this);
    m_fabAutoHideTimer->setSingleShot(true);
    m_fabAutoHideTimer->setInterval(3000); 
    connect(m_fabAutoHideTimer, &QTimer::timeout, this, &MainWindow::fabAutoHideTimeout);

    m_fabOpacityEffect = new QGraphicsOpacityEffect(this);
    m_fabFadeAnimation = new QPropertyAnimation(m_fabOpacityEffect, "opacity", this);
    m_fabFadeAnimation->setDuration(300); 
    connect(m_fabFadeAnimation, &QPropertyAnimation::finished, this, &MainWindow::fabAnimationFinished);

    setupUI(); // m_tagEditorWidget is created here
    if (m_tagEditorWidget) { // Apply persistent setting after UI setup
        m_tagEditorWidget->setStoreTagsWithUnderscores(m_storeManualTagsWithUnderscores);
    }

    createMenus();
    createStatusBar();

    connect(m_autoCaptionManager, &AutoCaptionManager::captionGenerated, this, &MainWindow::updateCaptionWithSuggestion);
    connect(m_autoCaptionManager, &AutoCaptionManager::errorOccurred, this, &MainWindow::handleAutoCaptionError);
    connect(m_autoCaptionManager, &AutoCaptionManager::modelStatusChanged, this, &MainWindow::handleModelStatusChanged); 
    connect(m_autoCaptionManager, &AutoCaptionManager::vocabularyReady, this, [this](const QStringList &vocabularyWithUnderscores){ 
        if (m_tagEditorWidget && !vocabularyWithUnderscores.isEmpty()) {
            m_tagEditorWidget->setKnownTagsVocabulary(vocabularyWithUnderscores);
            qDebug() << "MainWindow: Vocabulary set for TagEditorWidget via vocabularyReady signal:" << vocabularyWithUnderscores.size() << "tags.";
        } else {
            qDebug() << "MainWindow: vocabularyReady signal received, but vocab empty or tagEditorWidget null.";
        }
    });
    
    if (m_autoCaptionSettingsPanel) { 
        connect(m_autoCaptionSettingsPanel, &AutoCaptionSettingsPanel::loadModelClicked, m_autoCaptionManager, &AutoCaptionManager::loadModel);
        connect(m_autoCaptionSettingsPanel, &AutoCaptionSettingsPanel::unloadModelClicked, m_autoCaptionManager, &AutoCaptionManager::unloadModel);
        connect(m_autoCaptionSettingsPanel, &AutoCaptionSettingsPanel::deviceSelectionChanged, m_autoCaptionManager, &AutoCaptionManager::setSelectedDevice);
        connect(m_autoCaptionSettingsPanel, &AutoCaptionSettingsPanel::useAmdGpuChanged, m_autoCaptionManager, &AutoCaptionManager::setUseAmdGpu);
        connect(m_autoCaptionSettingsPanel, &AutoCaptionSettingsPanel::advancedSettingsClicked, this, &MainWindow::showAutoCaptionSettingsDialog);
        connect(m_autoCaptionSettingsPanel, &AutoCaptionSettingsPanel::enableSuggestionWhileTypingChanged, m_autoCaptionManager, &AutoCaptionManager::setEnableSuggestionWhileTyping);
        connect(m_autoCaptionManager, &AutoCaptionManager::downloadProgress,
                m_autoCaptionSettingsPanel, &AutoCaptionSettingsPanel::showDownloadProgress);
    }
    
    if (m_nlpModeRadioMain) {
        connect(m_nlpModeRadioMain, &QRadioButton::toggled, this, &MainWindow::onCaptionEditingModeChanged);
    }
    if (m_tagsModeRadioMain) {
         connect(m_tagsModeRadioMain, &QRadioButton::toggled, this, &MainWindow::onCaptionEditingModeChanged);
    }

    connect(mediaPlayer, &QMediaPlayer::durationChanged, this, [this](qint64 duration){
        if(seekerSlider) seekerSlider->setRange(0, duration / 1000); 
        qint64 secs = duration / 1000; qint64 mins = secs / 60; secs %= 60;
        if (durationLabel) durationLabel->setText(QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0')));
    });
    connect(mediaPlayer, &QMediaPlayer::positionChanged, this, [this](qint64 position){
        if (seekerSlider && !seekerSlider->isSliderDown()) seekerSlider->setValue(position / 1000);
    });
    connect(mediaPlayer, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state){
        if(playPauseButton) {
            if (state == QMediaPlayer::PlayingState) {
                playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause)); playPauseButton->setToolTip(tr("Pause"));
            } else {
                playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay)); playPauseButton->setToolTip(tr("Play"));
            }
        }
    });
    connect(mediaPlayer, &QMediaPlayer::errorChanged, this, [this](){
        if (mediaPlayer->error() != QMediaPlayer::NoError) {
            qWarning() << "MediaPlayer Error:" << mediaPlayer->errorString();
            statusBar()->showMessage(tr("Error playing media: %1").arg(mediaPlayer->errorString()), 5000);
        }
    });
    
    autoSaveTimer = new QTimer(this);
    connect(autoSaveTimer, &QTimer::timeout, this, &MainWindow::performAutoSave);
    autoSaveTimer->start(30 * 60 * 1000); 

    statusBar()->showMessage(tr("Ready. Please open a directory."));
    if (m_sparkleActionButton) {
        m_sparkleActionButton->setGraphicsEffect(m_fabOpacityEffect);
        m_fabOpacityEffect->setOpacity(1.0); 
        m_fabAutoHideTimer->start(); 
    }
    onCaptionEditingModeChanged(); 
    if (m_autoCaptionManager) {
        m_autoCaptionManager->ensureVocabularyLoaded(); 
    }
}

MainWindow::~MainWindow() {
    QSettings settings("KetenganDiffusion", "HaigakuManager");
    settings.setValue("storeManualTagsWithUnderscores", m_storeManualTagsWithUnderscores);
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *rootVLayout = new QVBoxLayout(centralWidget); 
    rootVLayout->setContentsMargins(0,0,0,0);
    rootVLayout->setSpacing(0);

    mainSplitter = new QSplitter(Qt::Horizontal, centralWidget); 
    thumbnailListView = new QListView(mainSplitter);
    thumbnailListView->setModel(m_thumbnailModel);
    thumbnailListView->setFixedWidth(thumbnailDefaultSize.width() + 40); 
    thumbnailListView->setViewMode(QListView::IconMode);
    thumbnailListView->setIconSize(thumbnailDefaultSize);
    thumbnailListView->setResizeMode(QListView::Adjust); 
    thumbnailListView->setMovement(QListView::Static); 
    thumbnailListView->setWordWrap(true); 
    connect(thumbnailListView, &QListView::clicked, this, &MainWindow::onThumbnailViewClicked);
    connect(thumbnailListView->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::onThumbnailViewScrolled);
    mainSplitter->addWidget(thumbnailListView);
    
    QWidget *centerPanelWidget = new QWidget(mainSplitter);
    QVBoxLayout *centerPanelLayout = new QVBoxLayout(centerPanelWidget);
    centerPanelLayout->setContentsMargins(0,0,0,0);
    mediaDisplayContainer = new QStackedWidget(centerPanelWidget);
    imageScrollArea = new QScrollArea(mediaDisplayContainer);
    imageScrollArea->setBackgroundRole(QPalette::Dark); imageScrollArea->setAlignment(Qt::AlignCenter);
    imageDisplayLabel = new QLabel(imageScrollArea);
    imageDisplayLabel->setObjectName("imageDisplayLabel"); imageDisplayLabel->setBackgroundRole(QPalette::Dark);
    imageDisplayLabel->setAlignment(Qt::AlignCenter); imageDisplayLabel->setScaledContents(false);
    imageScrollArea->setWidget(imageDisplayLabel); imageScrollArea->setWidgetResizable(true);
    mediaDisplayContainer->addWidget(imageScrollArea);
    videoDisplayWidget = new QVideoWidget(mediaDisplayContainer);
    mediaPlayer->setVideoOutput(videoDisplayWidget);
    mediaDisplayContainer->addWidget(videoDisplayWidget);
    centerPanelLayout->addWidget(mediaDisplayContainer, 1);
    videoControlsWidget = new QWidget(centerPanelWidget);
    QHBoxLayout *controlsLayout = new QHBoxLayout(videoControlsWidget);
    controlsLayout->setContentsMargins(5,2,5,2);
    playPauseButton = new QPushButton(videoControlsWidget);
    playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay)); playPauseButton->setToolTip(tr("Play"));
    connect(playPauseButton, &QPushButton::clicked, this, [this](){ if (mediaPlayer->playbackState() == QMediaPlayer::PlayingState) mediaPlayer->pause(); else mediaPlayer->play(); });
    controlsLayout->addWidget(playPauseButton);
    seekerSlider = new QSlider(Qt::Horizontal, videoControlsWidget);
    seekerSlider->setRange(0,0);
    connect(seekerSlider, &QSlider::sliderMoved, this, [this](int position){ mediaPlayer->setPosition(static_cast<qint64>(position) * 1000); });
    controlsLayout->addWidget(seekerSlider);
    durationLabel = new QLabel("--:--", videoControlsWidget);
    controlsLayout->addWidget(durationLabel);
    centerPanelLayout->addWidget(videoControlsWidget);
    videoControlsWidget->setVisible(false);
    mainSplitter->addWidget(centerPanelWidget);
    
    QWidget *rightPanelContainer = new QWidget(); 
    QVBoxLayout *rightPanelVLayout = new QVBoxLayout(rightPanelContainer);
    rightPanelVLayout->setContentsMargins(2,2,2,2); 
    
    QHBoxLayout* captionHeaderLayout = new QHBoxLayout();
    m_nlpModeRadioMain = new QRadioButton(tr("NLP"), rightPanelContainer);
    m_tagsModeRadioMain = new QRadioButton(tr("Tags"), rightPanelContainer);
    m_nlpModeRadioMain->setChecked(true); 
    QHBoxLayout *nlpTagsLayout = new QHBoxLayout(); 
    nlpTagsLayout->addWidget(m_nlpModeRadioMain);
    nlpTagsLayout->addWidget(m_tagsModeRadioMain);
    nlpTagsLayout->setContentsMargins(0,0,0,0); 
    m_captionModeSwitchGroup = new QButtonGroup(this); 
    m_captionModeSwitchGroup->addButton(m_nlpModeRadioMain);
    m_captionModeSwitchGroup->addButton(m_tagsModeRadioMain);
    captionHeaderLayout->addLayout(nlpTagsLayout); 
    captionHeaderLayout->addStretch(); 
    m_bulbButton = new QToolButton(rightPanelContainer);
    m_bulbButton->setIcon(style()->standardIcon(QStyle::SP_DialogHelpButton)); 
    m_bulbButton->setToolTip(tr("Generate Auto-Caption Suggestion"));
    connect(m_bulbButton, &QToolButton::clicked, this, &MainWindow::onBulbButtonClicked);
    captionHeaderLayout->addWidget(m_bulbButton);
    rightPanelVLayout->addLayout(captionHeaderLayout);

    m_captionInputStackedWidget = new QStackedWidget(rightPanelContainer);
    captionEditor = new QTextEdit(m_captionInputStackedWidget); 
    captionEditor->setPlaceholderText(tr("NLP caption will appear here..."));
    captionEditor->installEventFilter(this); 
    connect(captionEditor, &QTextEdit::textChanged, this, [this]() { 
        captionChangedSinceLoad = true; 
        if (currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
            if(captionEditor && m_nlpModeRadioMain->isChecked()) unsavedCaptions[mediaFiles.at(currentMediaIndex)] = captionEditor->toPlainText();
        }
    });
    m_captionInputStackedWidget->addWidget(captionEditor);

    m_tagEditorWidget = new TagEditorWidget(m_captionInputStackedWidget);
    connect(m_tagEditorWidget, &TagEditorWidget::tagsChanged, this, [this]() {
        captionChangedSinceLoad = true;
        if (currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
            if(m_tagsModeRadioMain->isChecked()) {
                // For unsavedCaptions, always store with spaces for internal consistency
                unsavedCaptions[mediaFiles.at(currentMediaIndex)] = m_tagEditorWidget->getTags(false).join(", ");
            }
        }
    });
    m_captionInputStackedWidget->addWidget(m_tagEditorWidget);
    
    rightPanelVLayout->addWidget(m_captionInputStackedWidget, 1); 

    fileDetailsLabel = new QLabel(tr("File details will appear here..."), rightPanelContainer); 
    fileDetailsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft); fileDetailsLabel->setWordWrap(true);
    fileDetailsLabel->setFixedHeight(fileDetailsLabel->sizeHint().height() * 3); 
    rightPanelVLayout->addWidget(fileDetailsLabel);
    mainSplitter->addWidget(rightPanelContainer); 
    mainSplitter->setStretchFactor(1, 1); 
    mainSplitter->setStretchFactor(2, 0); 
    mainSplitter->setSizes({thumbnailDefaultSize.width() + 40, 700, 300});

    rootVLayout->addWidget(mainSplitter, 1); 
    setCentralWidget(centralWidget); 

    m_sparkleActionButton = new QToolButton(this);
    m_sparkleActionButton->setGraphicsEffect(m_fabOpacityEffect); 
    m_sparkleActionButton->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton)); 
    m_sparkleActionButton->setIconSize(QSize(24, 24)); 
    m_sparkleActionButton->setFixedSize(QSize(36, 36)); 
    m_sparkleActionButton->setToolTip(tr("Auto-Caption Settings"));
    m_sparkleActionButton->setStyleSheet("QToolButton { border: 1px solid #8f8f91; border-radius: 18px; background-color: palette(window); } QToolButton:hover { background-color: palette(highlight); }");
    m_sparkleActionButton->move(width() - m_sparkleActionButton->width() - 20, height() - m_sparkleActionButton->height() - 20); 
    m_sparkleActionButton->installEventFilter(this); 

    m_autoCaptionSettingsPanel = new AutoCaptionSettingsPanel(this); 
    m_autoCaptionSettingsPanel->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint); 
    m_autoCaptionSettingsPanel->setFixedSize(m_autoCaptionSettingsPanel->sizeHint()); 
    m_autoCaptionSettingsPanel->setVisible(false);
    m_isAutoCaptionPanelVisible = false;
}

void MainWindow::onCaptionEditingModeChanged() {
    if (!m_captionInputStackedWidget || !captionEditor || !m_tagEditorWidget || !m_nlpModeRadioMain || !m_tagsModeRadioMain) {
        return;
    }
    
    QString currentTextToStore;
    QWidget* previousEditor = m_captionInputStackedWidget->currentWidget();
    if (previousEditor == captionEditor) {
        currentTextToStore = captionEditor->toPlainText();
    } else if (previousEditor == m_tagEditorWidget) {
        // When switching modes, get tags with spaces for consistency
        currentTextToStore = m_tagEditorWidget->getTags(false).join(", ");
    }

    if (m_nlpModeRadioMain->isChecked()) {
        if (m_captionInputStackedWidget->currentWidget() != captionEditor) {
             QStringList tags = currentTextToStore.split(',', Qt::SkipEmptyParts);
             QStringList cleanedTags;
             for(const QString &tag : tags) cleanedTags.append(tag.trimmed());
             captionEditor->setPlainText(cleanedTags.join(", "));
        }
        m_captionInputStackedWidget->setCurrentWidget(captionEditor);
        captionEditor->setPlaceholderText(tr("NLP caption will appear here..."));
    } else if (m_tagsModeRadioMain->isChecked()) {
        if (m_captionInputStackedWidget->currentWidget() != m_tagEditorWidget) {
            QStringList tags = currentTextToStore.split(',', Qt::SkipEmptyParts);
            QStringList cleanedTags;
            for(const QString &tag : tags) cleanedTags.append(tag.trimmed());
            // When setting tags from NLP to Tags mode, assume input has spaces (no underscores)
            m_tagEditorWidget->setTags(cleanedTags, false); 
        }
        m_captionInputStackedWidget->setCurrentWidget(m_tagEditorWidget);
    }
}


void MainWindow::toggleAutoCaptionPanel() {
    if (!m_autoCaptionSettingsPanel || !m_sparkleActionButton || !m_fabOpacityEffect || !m_fabFadeAnimation) return;

    if (m_isAutoCaptionPanelVisible) {
        m_autoCaptionSettingsPanel->hide();
        m_fabAutoHideTimer->start(); 
    } else {
        m_fabAutoHideTimer->stop(); 
        m_fabFadeAnimation->stop();
        m_fabOpacityEffect->setOpacity(1.0);
        m_sparkleActionButton->show(); 

        QPoint buttonPos = m_sparkleActionButton->mapToGlobal(QPoint(0,0));
        QPoint panelPos(buttonPos.x() - m_autoCaptionSettingsPanel->width() + m_sparkleActionButton->width()/2 , 
                        buttonPos.y() - m_autoCaptionSettingsPanel->height());
        
        if (panelPos.x() < 0) panelPos.setX(0);
        if (panelPos.y() < 0) panelPos.setY(0);
        
        m_autoCaptionSettingsPanel->move(panelPos);
        m_autoCaptionSettingsPanel->show();
        m_autoCaptionSettingsPanel->raise(); 
    }
    m_isAutoCaptionPanelVisible = !m_isAutoCaptionPanelVisible;
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (m_sparkleActionButton && !m_isFabDragging) { 
        int fabX = m_sparkleActionButton->x();
        int fabY = m_sparkleActionButton->y();
        int newX = width() - m_sparkleActionButton->width() - 20;
        int newY = height() - m_sparkleActionButton->height() - 20;

        if (fabX > width() - m_sparkleActionButton->width() * 2 || fabY > height() - m_sparkleActionButton->height() * 2) {
             m_sparkleActionButton->move(newX, newY);
        } else { 
            fabX = qBound(0, fabX, width() - m_sparkleActionButton->width());
            fabY = qBound(0, fabY, height() - m_sparkleActionButton->height());
            m_sparkleActionButton->move(fabX, fabY);
        }
    }
    if (m_isAutoCaptionPanelVisible && m_autoCaptionSettingsPanel && m_sparkleActionButton) {
        QPoint buttonPos = m_sparkleActionButton->mapToGlobal(QPoint(0,0));
        QPoint panelPos(buttonPos.x() - m_autoCaptionSettingsPanel->width() + m_sparkleActionButton->width()/2 , 
                        buttonPos.y() - m_autoCaptionSettingsPanel->height());
        panelPos = mapFromGlobal(panelPos); 
        
        panelPos.setX(qBound(0, panelPos.x(), width() - m_autoCaptionSettingsPanel->width()));
        panelPos.setY(qBound(0, panelPos.y(), height() - m_autoCaptionSettingsPanel->height()));
        
        m_autoCaptionSettingsPanel->move(mapToGlobal(panelPos)); 
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_sparkleActionButton) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event); 
        switch (event->type()) {
            case QEvent::MouseButtonPress:
                if (mouseEvent->button() == Qt::LeftButton) {
                    m_isFabDragging = false; 
                    m_fabDragStartPosition = mouseEvent->pos(); 
                    return true; 
                }
                break; 
            case QEvent::MouseMove:
                if (mouseEvent->buttons() & Qt::LeftButton) { 
                    if (!m_isFabDragging) {
                        if ((mouseEvent->pos() - m_fabDragStartPosition).manhattanLength() >= QApplication::startDragDistance()) {
                            m_isFabDragging = true;
                        }
                    }
                    if (m_isFabDragging) {
                        QPoint newPos = m_sparkleActionButton->pos() + mouseEvent->pos() - m_fabDragStartPosition;
                        newPos.setX(qBound(0, newPos.x(), width() - m_sparkleActionButton->width()));
                        newPos.setY(qBound(0, newPos.y(), height() - m_sparkleActionButton->height()));
                        m_sparkleActionButton->move(newPos);
                    }
                    return true; 
                }
                break; 
            case QEvent::MouseButtonRelease:
                if (mouseEvent->button() == Qt::LeftButton) {
                    bool wasDragging = m_isFabDragging;
                    m_isFabDragging = false; 
                    if (!wasDragging) { 
                        toggleAutoCaptionPanel(); 
                    }
                    return true; 
                }
                break; 
            default:
                return false; 
        }
        return false; 
    } else if (watched == captionEditor) { 
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Tab && !m_suggestedCaption.isEmpty() && m_nlpModeRadioMain->isChecked()) {
                qDebug() << "Tab pressed in captionEditor (NLP mode). m_suggestedCaption:" << m_suggestedCaption;
                captionEditor->setPlainText(m_suggestedCaption);
                captionEditor->setPlaceholderText(""); 
                QTextCursor cursor = captionEditor->textCursor();
                cursor.movePosition(QTextCursor::End);
                captionEditor->setTextCursor(cursor);
                m_suggestedCaption.clear();
                captionChangedSinceLoad = true;
                return true; 
            }
        }
    }
    return QMainWindow::eventFilter(watched, event); 
}

void MainWindow::onBulbButtonClicked() {
    if (!m_autoCaptionManager) return;
    if (currentMediaIndex < 0 || currentMediaIndex >= mediaFiles.count()) {
        statusBar()->showMessage(tr("No media selected."), 3000);
        return;
    }
    QString imagePath = mediaFiles.at(currentMediaIndex);
    m_autoCaptionManager->generateCaptionForImage(imagePath);
    statusBar()->showMessage(tr("Requesting auto-caption for %1...").arg(QFileInfo(imagePath).fileName()), 3000);
}

void MainWindow::updateCaptionWithSuggestion(const QStringList &tags, const QString &forImagePath, bool autoFill) {
    if (currentMediaIndex < 0 || currentMediaIndex >= mediaFiles.count() || mediaFiles.at(currentMediaIndex) != forImagePath) {
        return; 
    }
    m_suggestedCaption = tags.join(", "); 
    
    if (m_nlpModeRadioMain && m_nlpModeRadioMain->isChecked() && captionEditor) {
        if (autoFill) { 
            captionEditor->setPlainText(m_suggestedCaption);
            captionEditor->setPlaceholderText(""); 
        } else { 
            if (captionEditor->toPlainText().isEmpty()) {
                captionEditor->setPlaceholderText(m_suggestedCaption); 
            }
            if (!m_suggestedCaption.isEmpty()) {
                QPoint tipPos = captionEditor->mapToGlobal(QPoint(0, captionEditor->height() / 2));
                QToolTip::showText(tipPos, tr("Suggestion available. Press Tab to apply."), captionEditor, captionEditor->rect(), 3000);
                statusBar()->showMessage(tr("Suggestion available. Press Tab to apply."), 5000);
            }
        }
    } else if (m_tagsModeRadioMain && m_tagsModeRadioMain->isChecked() && m_tagEditorWidget) {
        if (autoFill) {
            QStringList newTags = m_suggestedCaption.split(',', Qt::SkipEmptyParts);
            for(QString &tag : newTags) tag = tag.trimmed();
            m_tagEditorWidget->setTags(newTags, true); 
        } else {
            statusBar()->showMessage(tr("Tags generated by model. Type to see suggestions."), 3000);
        }
    }
    qDebug() << "Suggestion received:" << m_suggestedCaption << "AutoFill:" << autoFill;
}

void MainWindow::handleAutoCaptionError(const QString &errorMessage) {
    statusBar()->showMessage(tr("Auto-Caption Error: %1").arg(errorMessage), 5000);
    QMessageBox::warning(this, tr("Auto-Caption Error"), errorMessage);
}

void MainWindow::showAutoCaptionSettingsDialog() {
    if (!m_autoCaptionManager || !m_autoCaptionSettingsPanel) return;
    QString currentModel = "SmilingWolf/wd-vit-tagger-v3"; 
    QVariantMap currentSettings = m_autoCaptionManager->getModelSettings(); 
    // Ensure current preference for storeManualTagsWithUnderscores is passed to dialog
    currentSettings["store_manual_tags_with_underscores"] = m_storeManualTagsWithUnderscores;
    
    AutoCaptionSettingsDialog dialog(currentModel, currentSettings, this);
    if (dialog.exec() == QDialog::Accepted) {
        QVariantMap newSettings = dialog.getSettings();
        m_autoCaptionManager->applyModelSettings(currentModel, newSettings);
        
        m_storeManualTagsWithUnderscores = newSettings.value("store_manual_tags_with_underscores", false).toBool();
        if (m_tagEditorWidget) {
            m_tagEditorWidget->setStoreTagsWithUnderscores(m_storeManualTagsWithUnderscores); 
        }
        qDebug() << "MainWindow: Store manual tags with underscores set to:" << m_storeManualTagsWithUnderscores;
        
        QSettings appSettings("KetenganDiffusion", "HaigakuManager");
        appSettings.setValue("storeManualTagsWithUnderscores", m_storeManualTagsWithUnderscores);
    }
}

void MainWindow::onThumbnailViewClicked(const QModelIndex &index) { 
    if (captionChangedSinceLoad && currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
        QString currentCaptionText;
        if (m_nlpModeRadioMain->isChecked() && captionEditor) {
            currentCaptionText = captionEditor->toPlainText();
        } else if (m_tagsModeRadioMain->isChecked() && m_tagEditorWidget) {
            // Always get with spaces for unsavedCaptions
            currentCaptionText = m_tagEditorWidget->getTags(false).join(", ");
        }
        if (!currentCaptionText.isEmpty() || unsavedCaptions.contains(mediaFiles.at(currentMediaIndex))) {
             unsavedCaptions[mediaFiles.at(currentMediaIndex)] = currentCaptionText;
        }
    }
    displayMediaAtIndex(index.row());
}
void MainWindow::onThumbnailViewScrolled() {
    if (m_scrollStopTimer) {
        m_scrollStopTimer->start(); 
    }
}
void MainWindow::loadVisibleThumbnails() {
    if (!thumbnailListView || !m_thumbnailModel || !m_thumbnailLoaderService || mediaFiles.isEmpty()) { 
        return;
    }
    QModelIndex topLeft = thumbnailListView->indexAt(thumbnailListView->viewport()->rect().topLeft());
    QModelIndex bottomRight = thumbnailListView->indexAt(thumbnailListView->viewport()->rect().bottomRight());
    int firstVisibleRow = topLeft.isValid() ? topLeft.row() : 0;
    int lastVisibleRow = bottomRight.isValid() ? bottomRight.row() : m_thumbnailModel->rowCount() - 1;
    int buffer = 10; 
    int startRow = qMax(0, firstVisibleRow - buffer);
    int endRow = qMin(m_thumbnailModel->rowCount() - 1, lastVisibleRow + buffer);
    m_thumbnailLoaderService->clearQueue();
    QList<ThumbnailRequest> requests;
    for (int i = startRow; i <= endRow; ++i) {
        if (!m_thumbnailModel->isThumbnailLoaded(i)) { 
            requests.append({i, m_thumbnailModel->filePathAt(i), thumbnailDefaultSize});
        }
    }
    if (!requests.isEmpty()) {
        m_thumbnailLoaderService->requestThumbnailBatch(requests);
    }
}
void MainWindow::createMenus() { 
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    openDirAction = new QAction(tr("&Open Directory..."), this);
    openDirAction->setShortcuts(QKeySequence::Open);
    connect(openDirAction, &QAction::triggered, this, &MainWindow::openDirectory);
    fileMenu->addAction(openDirAction);

    openProjectAction = new QAction(tr("Open P&roject..."), this);
    connect(openProjectAction, &QAction::triggered, this, &MainWindow::openProject);
    fileMenu->addAction(openProjectAction);

    saveProjectAction = new QAction(tr("&Save Project"), this); 
    saveProjectAction->setShortcuts(QKeySequence::Save); 
    connect(saveProjectAction, &QAction::triggered, this, &MainWindow::saveProject);
    fileMenu->addAction(saveProjectAction);

    saveProjectAsAction = new QAction(tr("Save Project &As..."), this);
    connect(saveProjectAsAction, &QAction::triggered, this, &MainWindow::saveProjectAs);
    fileMenu->addAction(saveProjectAsAction);
    
    fileMenu->addSeparator();
    QAction *saveIndividualCaptionAction = new QAction(tr("Save &Caption (current file)"), this); 
    connect(saveIndividualCaptionAction, &QAction::triggered, this, &MainWindow::saveCurrentCaption);
    fileMenu->addAction(saveIndividualCaptionAction);
    fileMenu->addSeparator();
    QAction *nextAction = new QAction(tr("&Next Media"), this);
    connect(nextAction, &QAction::triggered, this, &MainWindow::nextMedia);
    fileMenu->addAction(nextAction);
    QAction *prevAction = new QAction(tr("&Previous Media"), this);
    connect(prevAction, &QAction::triggered, this, &MainWindow::previousMedia);
    fileMenu->addAction(prevAction);
    fileMenu->addSeparator();
    exitAction = new QAction(tr("E&xit"), this);
    exitAction->setShortcuts(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
    fileMenu->addAction(exitAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    refreshThumbnailsAction = new QAction(tr("&Refresh Thumbnails"), this);
    connect(refreshThumbnailsAction, &QAction::triggered, this, &MainWindow::onRefreshThumbnails);
    viewMenu->addAction(refreshThumbnailsAction);

    QMenu *statisticMenu = menuBar()->addMenu(tr("&Statistic"));
    statisticsAction = new QAction(tr("Show &Dataset Statistics..."), this);
    connect(statisticsAction, &QAction::triggered, this, &MainWindow::showStatisticsDialog);
    statisticMenu->addAction(statisticsAction);
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    aboutAction = new QAction(tr("&About Haigaku Manager"), this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);
    helpMenu->addAction(aboutAction);
    aboutQtAction = new QAction(tr("About &Qt"), this);
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
    helpMenu->addAction(aboutQtAction);
}
void MainWindow::createStatusBar() { 
    statusBar()->showMessage(tr("Ready"));
}
void MainWindow::openDirectory() { 
    if (captionChangedSinceLoad && currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
        QString currentCaptionText;
        if (m_nlpModeRadioMain->isChecked() && captionEditor) {
            currentCaptionText = captionEditor->toPlainText();
        } else if (m_tagsModeRadioMain->isChecked() && m_tagEditorWidget) {
            currentCaptionText = m_tagEditorWidget->getTags(false).join(", "); // Always spaces for unsaved
        }
         if (!currentCaptionText.isEmpty() || unsavedCaptions.contains(mediaFiles.at(currentMediaIndex))) {
            unsavedCaptions[mediaFiles.at(currentMediaIndex)] = currentCaptionText;
        }
    }

    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                                                       currentDirectory.isEmpty() ? QDir::homePath() : currentDirectory,
                                                       QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dirPath.isEmpty()) {
        currentDirectory = dirPath;
        m_currentProjectPath.clear(); 
        unsavedCaptions.clear(); 
        statusBar()->showMessage(tr("Loading files from: %1").arg(currentDirectory));
        loadFiles(currentDirectory);
        setWindowTitle(tr("Haigaku Manager - %1").arg(QDir(currentDirectory).dirName()));
    }
}
void MainWindow::loadFiles(const QString &dirPath)
{
    if(mediaPlayer) mediaPlayer->stop();
    mediaFiles.clear(); 
    if(m_thumbnailModel) m_thumbnailModel->clear(); 
    currentMediaIndex = -1;
    captionChangedSinceLoad = false; 
    QDir directory(dirPath);
    QStringList nameFilters;
    nameFilters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif" << "*.webp" << "*.tiff";
    nameFilters << "*.mp4" << "*.mkv" << "*.webm";
    QStringList filesFound = directory.entryList(nameFilters, QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    if (filesFound.isEmpty()) { 
        statusBar()->showMessage(tr("No supported media files found in %1").arg(dirPath));
        if(mediaDisplayContainer && imageScrollArea) mediaDisplayContainer->setCurrentWidget(imageScrollArea);
        if(imageDisplayLabel) { imageDisplayLabel->clear(); imageDisplayLabel->setText(tr("No media files found in directory."));}
        if(videoControlsWidget) videoControlsWidget->setVisible(false);
        
        if (m_nlpModeRadioMain && m_nlpModeRadioMain->isChecked() && captionEditor) captionEditor->clear();
        else if (m_tagsModeRadioMain && m_tagsModeRadioMain->isChecked() && m_tagEditorWidget) m_tagEditorWidget->clear();
        
        if(fileDetailsLabel) fileDetailsLabel->setText(tr("File details will appear here..."));
        return;
    }
    QStringList fullFilePaths;
    for(const QString &file : filesFound) {
        fullFilePaths.append(directory.filePath(file));
    }
    mediaFiles = fullFilePaths; 
    if(m_thumbnailModel) m_thumbnailModel->setFilePaths(fullFilePaths); 
    if (!mediaFiles.isEmpty()) {
        displayMediaAtIndex(0);
        if(thumbnailListView && m_thumbnailModel) {
            QModelIndex firstIndex = m_thumbnailModel->index(0,0);
            thumbnailListView->setCurrentIndex(firstIndex);
        }
        QTimer::singleShot(0, this, &MainWindow::loadVisibleThumbnails); 
    }
    statusBar()->showMessage(tr("Loaded %1 media files. Thumbnails loading on demand...").arg(mediaFiles.count()));
}
void MainWindow::displayMediaAtIndex(int index) { 
    if (index < 0 || index >= mediaFiles.count()) { 
        qWarning() << "displayMediaAtIndex: Index out of bounds" << index;
        return;
    }
    if(mediaPlayer) mediaPlayer->stop();
    currentMediaIndex = index;
    QString filePath = mediaFiles.at(currentMediaIndex);
    QFileInfo fileInfo(filePath);
    QStringList imageExtensions = {"jpg", "jpeg", "png", "bmp", "gif", "webp", "tiff"};
    bool isImage = imageExtensions.contains(fileInfo.suffix().toLower());
    if (isImage) {
        if(mediaDisplayContainer && imageScrollArea) mediaDisplayContainer->setCurrentWidget(imageScrollArea);
        if(videoControlsWidget) videoControlsWidget->setVisible(false);
        if(videoDisplayWidget) videoDisplayWidget->setVisible(false); 
        if(imageScrollArea) imageScrollArea->setVisible(true);
        QImageReader reader(filePath); reader.setAutoTransform(true); QImage image = reader.read();
        if (image.isNull()) {
            qWarning() << "Failed to read image:" << filePath << "Error:" << reader.errorString();
            if(imageDisplayLabel) {
                imageDisplayLabel->setText(tr("Cannot load image: %1").arg(fileInfo.fileName()));
                QPixmap errorPixmap(200, 200); errorPixmap.fill(Qt::gray); imageDisplayLabel->setPixmap(errorPixmap);
            }
        } else {
            QPixmap pixmap = QPixmap::fromImage(image);
            if (!pixmap.isNull() && imageScrollArea && imageScrollArea->viewport()) {
                QSize viewportSize = imageScrollArea->viewport()->size();
                if (viewportSize.width() > 0 && viewportSize.height() > 0) 
                     pixmap = pixmap.scaled(viewportSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            if(imageDisplayLabel) {
                imageDisplayLabel->setPixmap(pixmap);
                if (!pixmap.isNull()) imageDisplayLabel->adjustSize(); else imageDisplayLabel->setMinimumSize(1,1);
            }
        }
    } else { 
        if(mediaDisplayContainer && videoDisplayWidget) mediaDisplayContainer->setCurrentWidget(videoDisplayWidget);
        if(videoControlsWidget) videoControlsWidget->setVisible(true);
        if(imageScrollArea) imageScrollArea->setVisible(false); 
        if(videoDisplayWidget) videoDisplayWidget->setVisible(true);
        if(mediaPlayer) mediaPlayer->setSource(QUrl::fromLocalFile(filePath));
    }
    updateFileDetails(filePath);
    loadCaptionForCurrentImage(); 
    if(thumbnailListView && m_thumbnailModel) {
         thumbnailListView->setCurrentIndex(m_thumbnailModel->index(currentMediaIndex, 0));
    }
    statusBar()->showMessage(tr("Displaying: %1 (%2/%3)")
                             .arg(fileInfo.fileName()).arg(currentMediaIndex + 1).arg(mediaFiles.count()));
}
void MainWindow::updateFileDetails(const QString &filePath) { 
    QFileInfo info(filePath);
    QString detailsText = QString("<b>File:</b> %1<br><b>Path:</b> %2<br><b>Size:</b> %3 KB")
                          .arg(info.fileName()).arg(info.absoluteFilePath()).arg(info.size() / 1024);
    QStringList imageExtensions = {"jpg", "jpeg", "png", "bmp", "gif", "webp", "tiff"};
    if (imageExtensions.contains(info.suffix().toLower())) {
        QImageReader reader(filePath);
        QSize imageSize = reader.size();
        detailsText += QString("<br><b>Resolution:</b> %1x%2").arg(imageSize.width()).arg(imageSize.height());
    }
    if(fileDetailsLabel) fileDetailsLabel->setText(detailsText);
}
void MainWindow::loadCaptionForCurrentImage() { 
    QString captionToLoad = "";
    if (currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
        QString mediaPath = mediaFiles.at(currentMediaIndex);
        if (unsavedCaptions.contains(mediaPath)) {
            captionToLoad = unsavedCaptions.value(mediaPath);
        } else {
            QFileInfo mediaInfo(mediaPath);
            QString baseName = mediaInfo.absolutePath() + "/" + mediaInfo.completeBaseName();
            QString captionPathTxt = baseName + ".txt";
            QString captionPathCaption = baseName + ".caption";
            QFile captionFile;
            if (QFile::exists(captionPathTxt)) captionFile.setFileName(captionPathTxt);
            else if (QFile::exists(captionPathCaption)) captionFile.setFileName(captionPathCaption);
            if (!captionFile.fileName().isEmpty()) {
                if (captionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    captionToLoad = captionFile.readAll();
                    captionFile.close();
                } else {
                    qWarning() << "Could not open caption file:" << captionFile.fileName();
                    captionToLoad = tr("[Error reading caption file]");
                }
            }
        }
    }

    if (m_nlpModeRadioMain && m_nlpModeRadioMain->isChecked() && captionEditor) {
        captionEditor->blockSignals(true);
        captionEditor->setPlainText(captionToLoad);
        captionEditor->blockSignals(false);
    } else if (m_tagsModeRadioMain && m_tagsModeRadioMain->isChecked() && m_tagEditorWidget) {
        QStringList tags = captionToLoad.split(',', Qt::SkipEmptyParts);
        QStringList cleanedTags;
        for(const QString &tag : tags) {
            cleanedTags.append(tag.trimmed());
        }
        // When loading from file, assume tags might have underscores if that was the save format.
        // However, TagEditorWidget internally wants tags with spaces.
        // The `inputHasUnderscores` parameter of `setTags` is tricky.
        // For now, assume loaded captions are space-separated for simplicity,
        // unless we store metadata about their format.
        m_tagEditorWidget->setTags(cleanedTags, false); 
    }
    captionChangedSinceLoad = false; 
}
void MainWindow::saveCurrentCaption()  { 
    if (currentMediaIndex < 0 || currentMediaIndex >= mediaFiles.count()) return;
    
    QString captionTextToSave;
    if (m_nlpModeRadioMain && m_nlpModeRadioMain->isChecked() && captionEditor) {
        captionTextToSave = captionEditor->toPlainText();
    } else if (m_tagsModeRadioMain && m_tagsModeRadioMain->isChecked() && m_tagEditorWidget) {
        // Use the member variable m_storeManualTagsWithUnderscores to determine format for saving
        captionTextToSave = m_tagEditorWidget->getTags(m_storeManualTagsWithUnderscores).join(", ");
    } else {
        return;
    }
        
    QString mediaPath = mediaFiles.at(currentMediaIndex);
    QFileInfo mediaInfo(mediaPath);
    QString captionPath = mediaInfo.absolutePath() + "/" + mediaInfo.completeBaseName() + ".txt";
    QFile captionFile(captionPath);
    if (captionFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&captionFile);
        out << captionTextToSave;
        captionFile.close();
        captionChangedSinceLoad = false;
        unsavedCaptions.remove(mediaPath); 
        statusBar()->showMessage(tr("Caption saved for %1").arg(mediaInfo.fileName()));
    } else {
        qWarning() << "Could not save caption file:" << captionPath;
        QMessageBox::warning(this, tr("Save Error"), tr("Could not save caption to %1").arg(captionPath));
        statusBar()->showMessage(tr("Error saving caption for %1").arg(mediaInfo.fileName()));
    }
}
void MainWindow::applyScoreToCaption(int scoreValue)  { 
    if (currentMediaIndex < 0 || currentMediaIndex >= mediaFiles.count()) return;
    
    QString scoreWord;
    if (scoreValue >= 1 && scoreValue <= 3) scoreWord = "Worst";
    else if (scoreValue == 4) scoreWord = "Lousy";
    else if (scoreValue == 5) scoreWord = "adequate";
    else if (scoreValue == 6) scoreWord = "Superior";
    else if (scoreValue == 7) scoreWord = "Masterpiece";
    else if (scoreValue == 8) scoreWord = "Exceptional";
    else if (scoreValue == 9) scoreWord = "Iconic";
    else return;

    if (m_nlpModeRadioMain && m_nlpModeRadioMain->isChecked() && captionEditor) {
        QString currentCaption = captionEditor->toPlainText();
        QStringList parts = currentCaption.split(',', Qt::SkipEmptyParts);
        QStringList existingScores = {"Worst", "Lousy", "adequate", "Superior", "Masterpiece", "Exceptional", "Iconic"};
        if (!parts.isEmpty()) {
            QString firstPartTrimmed = parts.first().trimmed();
            if (existingScores.contains(firstPartTrimmed, Qt::CaseInsensitive)) {
                parts.removeFirst();
            }
        }
        parts.prepend(scoreWord);
        for(int i = 0; i < parts.size(); ++i) parts[i] = parts[i].trimmed();
        captionEditor->setPlainText(parts.join(", "));
    } else if (m_tagsModeRadioMain && m_tagsModeRadioMain->isChecked() && m_tagEditorWidget) {
        QStringList currentTags = m_tagEditorWidget->getTags(false); // Get with spaces for manipulation
        
        QString scoreWordForDisplay = scoreWord; 
        
        QStringList existingScoresDisplay = {"Worst", "Lousy", "adequate", "Superior", "Masterpiece", "Exceptional", "Iconic"};
        
        for(const QString& es : existingScoresDisplay) { // Iterate and remove all existing scores
            currentTags.removeAll(es);
        }
        
        currentTags.prepend(scoreWordForDisplay);
        m_tagEditorWidget->setTags(currentTags, false); 
    }
}
void MainWindow::keyPressEvent(QKeyEvent *event) { 
    bool editorHasFocus = false;
    if (m_nlpModeRadioMain && m_nlpModeRadioMain->isChecked() && captionEditor && captionEditor->hasFocus()){
        editorHasFocus = true;
    } else if (m_tagsModeRadioMain && m_tagsModeRadioMain->isChecked() && m_tagEditorWidget ){ 
        if (m_captionInputStackedWidget && m_captionInputStackedWidget->currentWidget() == m_tagEditorWidget) {
             if (m_tagEditorWidget->hasFocus() || (m_tagEditorWidget->focusWidget() && m_tagEditorWidget->focusWidget()->inherits("QLineEdit"))) {
                 editorHasFocus = true;
             }
        }
    }

    switch (event->key()) {
    case Qt::Key_S: saveCurrentCaption(); nextMedia(); break;
    case Qt::Key_Right: nextMedia(); break;
    case Qt::Key_Left: previousMedia(); break;
    case Qt::Key_Delete: deleteCurrentMediaItem(); break;
    case Qt::Key_1: case Qt::Key_2: case Qt::Key_3: case Qt::Key_4: case Qt::Key_5: 
    case Qt::Key_6: case Qt::Key_7: case Qt::Key_8: case Qt::Key_9:
        if (!editorHasFocus) { 
             applyScoreToCaption(event->key() - Qt::Key_0); 
             saveCurrentCaption(); 
             nextMedia();
        } else {
            QMainWindow::keyPressEvent(event);
        }
        break;
    default: QMainWindow::keyPressEvent(event);
    }
}
void MainWindow::nextMedia() { 
    if (mediaFiles.isEmpty()) return;
    int nextIndex = (currentMediaIndex + 1);
    if (nextIndex >= mediaFiles.count()) nextIndex = 0; 
    if (nextIndex != currentMediaIndex || mediaFiles.count() == 1) {
         displayMediaAtIndex(nextIndex);
    }
}
void MainWindow::previousMedia() { 
    if (mediaFiles.isEmpty()) return;
    int prevIndex = (currentMediaIndex - 1);
    if (prevIndex < 0) prevIndex = mediaFiles.count() - 1; 
     if (prevIndex != currentMediaIndex || mediaFiles.count() == 1) {
        displayMediaAtIndex(prevIndex);
    }
}
void MainWindow::performAutoSave() { 
    qDebug() << "Performing auto-save...";
    bool projectSaved = false;
    if (!m_currentProjectPath.isEmpty() && !currentDirectory.isEmpty()) {
        QSettings projectFile(m_currentProjectPath, QSettings::IniFormat);
        projectFile.setValue("Project/DirectoryPath", currentDirectory);
        projectFile.beginGroup("Captions");
        projectFile.remove(""); 
        QDir dir(currentDirectory);
        
        if (captionChangedSinceLoad && currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
            QString currentCaptionText;
             if (m_nlpModeRadioMain->isChecked() && captionEditor) {
                currentCaptionText = captionEditor->toPlainText();
            } else if (m_tagsModeRadioMain->isChecked() && m_tagEditorWidget) {
                // For unsavedCaptions, always store with spaces
                currentCaptionText = m_tagEditorWidget->getTags(false).join(", ");
            }
            unsavedCaptions[mediaFiles.at(currentMediaIndex)] = currentCaptionText;
        }

        for (auto it = unsavedCaptions.constBegin(); it != unsavedCaptions.constEnd(); ++it) {
            QString absoluteFilePath = it.key();
            QString relativeFilePath = dir.relativeFilePath(absoluteFilePath);
            if (!relativeFilePath.isEmpty()) {
                projectFile.setValue(relativeFilePath, it.value());
            } else {
                qWarning() << "Auto-save (project): Could not make file path relative for project save:" << absoluteFilePath;
            }
        }
        projectFile.endGroup();
        projectFile.sync();
        projectSaved = true;
    }
    
    int individualCaptionsSaved = 0;
    if (m_currentProjectPath.isEmpty()) { 
        QStringList keysToSave = unsavedCaptions.keys();
        for (const QString &filePathToSave : keysToSave) {
            if (!mediaFiles.contains(filePathToSave)) continue; 
            QString captionText = unsavedCaptions.value(filePathToSave);
            QFileInfo mediaInfo(filePathToSave);
            QString captionPath = mediaInfo.absolutePath() + "/" + mediaInfo.completeBaseName() + ".txt";
            QFile captionFile(captionPath);
            if (captionFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                QTextStream out(&captionFile); out << captionText; captionFile.close();
                individualCaptionsSaved++;
            } else {
                qWarning() << "Auto-save (individual .txt) failed for" << captionPath;
            }
        }
    }

    if (projectSaved) {
        statusBar()->showMessage(tr("Project auto-saved to %1.").arg(QFileInfo(m_currentProjectPath).fileName()), 5000);
    } else if (individualCaptionsSaved > 0) {
        statusBar()->showMessage(tr("Auto-saved %1 individual captions.").arg(individualCaptionsSaved), 5000);
    } else {
        statusBar()->showMessage(tr("Auto-save: No changes to save."), 3000);
    }
}
void MainWindow::deleteCurrentMediaItem() {
    if (currentMediaIndex < 0 || currentMediaIndex >= mediaFiles.count()) {
        statusBar()->showMessage(tr("No media item selected to delete."), 3000);
        return;
    }
    QString filePathToDelete = mediaFiles.at(currentMediaIndex);
    QFileInfo mediaInfo(filePathToDelete);
    if (QFile::remove(filePathToDelete)) {
        statusBar()->showMessage(tr("Deleted media file: %1").arg(mediaInfo.fileName()), 3000);
    } else {
        statusBar()->showMessage(tr("Error deleting media file: %1").arg(mediaInfo.fileName()), 3000);
        qWarning() << "Error deleting media file:" << filePathToDelete;
    }
    QString baseName = mediaInfo.absolutePath() + "/" + mediaInfo.completeBaseName();
    QString captionPathTxt = baseName + ".txt";
    QString captionPathCaption = baseName + ".caption";
    if (QFile::exists(captionPathTxt)) {
        if (QFile::remove(captionPathTxt)) qDebug() << "Deleted caption file:" << captionPathTxt;
        else qWarning() << "Error deleting caption file:" << captionPathTxt;
    }
    if (QFile::exists(captionPathCaption)) {
        if (QFile::remove(captionPathCaption)) qDebug() << "Deleted caption file:" << captionPathCaption;
        else qWarning() << "Error deleting caption file:" << captionPathCaption;
    }
    unsavedCaptions.remove(filePathToDelete);
    mediaFiles.removeAt(currentMediaIndex);
    if (m_thumbnailModel) {
        m_thumbnailModel->setFilePaths(mediaFiles); 
    }
    if (mediaFiles.isEmpty()) {
        currentMediaIndex = -1;
        if(imageDisplayLabel) imageDisplayLabel->clear();
        if(videoDisplayWidget && mediaPlayer) mediaPlayer->setSource(QUrl()); 
        if(videoControlsWidget) videoControlsWidget->setVisible(false);
        if (captionEditor) captionEditor->clear();
        if (m_tagEditorWidget) m_tagEditorWidget->clear();
        if(fileDetailsLabel) fileDetailsLabel->setText(tr("File details will appear here..."));
        statusBar()->showMessage(tr("All media deleted or directory empty."), 3000);
    } else {
        if (currentMediaIndex >= mediaFiles.count()) {
            currentMediaIndex = mediaFiles.count() - 1;
        }
        if (currentMediaIndex < 0 && !mediaFiles.isEmpty()) {
            currentMediaIndex = 0;
        }
        displayMediaAtIndex(currentMediaIndex); 
    }
    QTimer::singleShot(0, this, &MainWindow::loadVisibleThumbnails);
}
void MainWindow::showStatisticsDialog() { 
    if (mediaFiles.isEmpty() && currentDirectory.isEmpty()) {
        QMessageBox::information(this, tr("Statistics"), tr("Please open a directory first.")); return;
    }
    StatisticsDialog dialog(mediaFiles, currentDirectory, this);
    dialog.exec();
}
void MainWindow::showAboutDialog()  { 
    QMessageBox::about(this, tr("About Haigaku Manager"),
                       tr("<b>Haigaku Manager</b><br>Version 0.1 (Alpha)<br><br>"
                          "A dataset manager for images and videos.<br>"
                          "Created by Ketengan Diffusion™.<br><br>Built with Qt."));
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_sparkleActionButton && m_fabOpacityEffect && m_fabFadeAnimation && m_fabAutoHideTimer) {
        if (m_fabFadeAnimation->state() == QAbstractAnimation::Running && m_fabFadeAnimation->direction() == QAbstractAnimation::Backward) {
            m_fabFadeAnimation->stop();
        }
        if (m_fabOpacityEffect->opacity() < 1.0 || !m_sparkleActionButton->isVisible()) {
             m_sparkleActionButton->show(); 
             m_fabFadeAnimation->setDirection(QAbstractAnimation::Forward); 
             m_fabFadeAnimation->setStartValue(m_fabOpacityEffect->opacity());
             m_fabFadeAnimation->setEndValue(1.0);
             m_fabFadeAnimation->start();
        } else {
             m_sparkleActionButton->show();
        }
        m_fabAutoHideTimer->start();   
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::fabAutoHideTimeout()
{
    if (m_sparkleActionButton && m_fabOpacityEffect && m_fabFadeAnimation &&
        !m_sparkleActionButton->rect().contains(m_sparkleActionButton->mapFromGlobal(QCursor::pos())) &&
        m_autoCaptionSettingsPanel && !m_autoCaptionSettingsPanel->isVisible()) { 
        
        if (m_fabFadeAnimation->state() == QAbstractAnimation::Running && m_fabFadeAnimation->direction() == QAbstractAnimation::Forward) {
            m_fabFadeAnimation->stop();
        }
        if (m_fabOpacityEffect->opacity() > 0.0) {
            m_fabFadeAnimation->setDirection(QAbstractAnimation::Backward); 
            m_fabFadeAnimation->setStartValue(m_fabOpacityEffect->opacity());
            m_fabFadeAnimation->setEndValue(0.0);
            m_fabFadeAnimation->start();
        }
    }
}

void MainWindow::fabAnimationFinished()
{
    if (m_sparkleActionButton && m_fabOpacityEffect) {
        if (m_fabOpacityEffect->opacity() == 0.0) {
            m_sparkleActionButton->hide();
        } else if (m_fabOpacityEffect->opacity() == 1.0) {
            m_sparkleActionButton->show(); 
        }
    }
}

void MainWindow::onRefreshThumbnails()
{
    if (m_thumbnailModel) {
        m_thumbnailModel->clearCache();
    }
    if (m_thumbnailLoaderService) {
        m_thumbnailLoaderService->clearQueue(); 
    }
    loadVisibleThumbnails(); 
    statusBar()->showMessage(tr("Thumbnails refreshed."), 3000);
}

void MainWindow::saveProjectAs()
{
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save Project As..."),
                                                    m_currentProjectPath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : QFileInfo(m_currentProjectPath).path(),
                                                    tr("Haigaku Manager Project (*.hmproj)"));
    if (filePath.isEmpty()) {
        return;
    }

    if (!filePath.endsWith(".hmproj", Qt::CaseInsensitive)) {
        filePath += ".hmproj";
    }

    m_currentProjectPath = filePath;
    QSettings projectFile(m_currentProjectPath, QSettings::IniFormat);

    projectFile.setValue("Project/DirectoryPath", currentDirectory);

    projectFile.beginGroup("Captions");
    projectFile.remove(""); 

    QDir dir(currentDirectory);
    if (captionChangedSinceLoad && currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
        QString currentCaptionText;
            if (m_nlpModeRadioMain->isChecked() && captionEditor) {
            currentCaptionText = captionEditor->toPlainText();
        } else if (m_tagsModeRadioMain->isChecked() && m_tagEditorWidget) {
            // Use the member variable m_storeManualTagsWithUnderscores
            currentCaptionText = m_tagEditorWidget->getTags(m_storeManualTagsWithUnderscores).join(", ");
        }
        unsavedCaptions[mediaFiles.at(currentMediaIndex)] = currentCaptionText;
    }

    for (auto it = unsavedCaptions.constBegin(); it != unsavedCaptions.constEnd(); ++it) {
        QString absoluteFilePath = it.key();
        QString relativeFilePath = dir.relativeFilePath(absoluteFilePath);
        if (!relativeFilePath.isEmpty()) {
            projectFile.setValue(relativeFilePath, it.value());
        } else {
            qWarning() << "Could not make file path relative for project save:" << absoluteFilePath;
        }
    }
    
    projectFile.endGroup();
    projectFile.sync(); 

    setWindowTitle(tr("Haigaku Manager - %1").arg(QFileInfo(m_currentProjectPath).fileName()));
    statusBar()->showMessage(tr("Project saved to %1").arg(m_currentProjectPath), 5000);
}

void MainWindow::saveProject()
{
    if (m_currentProjectPath.isEmpty()) {
        saveProjectAs(); 
        return;
    }
    if (currentDirectory.isEmpty()){
        QMessageBox::information(this, tr("Save Project"), tr("Please open a directory first. No active directory to save."));
        return;
    }

    QSettings projectFile(m_currentProjectPath, QSettings::IniFormat);
    projectFile.setValue("Project/DirectoryPath", currentDirectory);

    projectFile.beginGroup("Captions");
    projectFile.remove(""); 

    QDir dir(currentDirectory);
    if (captionChangedSinceLoad && currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
        QString currentCaptionText;
            if (m_nlpModeRadioMain->isChecked() && captionEditor) {
            currentCaptionText = captionEditor->toPlainText();
        } else if (m_tagsModeRadioMain->isChecked() && m_tagEditorWidget) {
            // Use the member variable m_storeManualTagsWithUnderscores
            currentCaptionText = m_tagEditorWidget->getTags(m_storeManualTagsWithUnderscores).join(", ");
        }
        unsavedCaptions[mediaFiles.at(currentMediaIndex)] = currentCaptionText;
    }

    for (auto it = unsavedCaptions.constBegin(); it != unsavedCaptions.constEnd(); ++it) {
        QString absoluteFilePath = it.key();
        QString relativeFilePath = dir.relativeFilePath(absoluteFilePath);
        if (!relativeFilePath.isEmpty()) {
            projectFile.setValue(relativeFilePath, it.value());
        } else {
            qWarning() << "Could not make file path relative for project save:" << absoluteFilePath;
        }
    }
    projectFile.endGroup();
    projectFile.sync();

    statusBar()->showMessage(tr("Project saved to %1").arg(QFileInfo(m_currentProjectPath).fileName()), 5000);
}

void MainWindow::openProject()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open Project"),
                                                    m_currentProjectPath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : QFileInfo(m_currentProjectPath).path(),
                                                    tr("Haigaku Manager Project (*.hmproj)"));
    if (filePath.isEmpty()) {
        return;
    }

    QSettings projectFile(filePath, QSettings::IniFormat);
    QString projectDirectoryPath = projectFile.value("Project/DirectoryPath").toString();

    if (projectDirectoryPath.isEmpty() || !QDir(projectDirectoryPath).exists()) {
        QMessageBox::warning(this, tr("Open Project Error"), tr("Invalid or missing directory path in project file."));
        m_currentProjectPath.clear(); 
        setWindowTitle(tr("Haigaku Manager"));
        return;
    }
    
    m_currentProjectPath = filePath;
    currentDirectory = projectDirectoryPath;
    
    unsavedCaptions.clear(); 
    if(captionEditor) captionEditor->clear();
    if(m_tagEditorWidget) m_tagEditorWidget->clear();


    statusBar()->showMessage(tr("Opening project: %1...").arg(QFileInfo(m_currentProjectPath).fileName()));
    QCoreApplication::processEvents(); 

    loadFiles(currentDirectory); 

    projectFile.beginGroup("Captions");
    const QStringList relativeFilePaths = projectFile.allKeys();
    QDir dir(currentDirectory);
    for (const QString &relativeFilePath : relativeFilePaths) {
        QString absoluteFilePath = dir.filePath(relativeFilePath);
        // When loading from project, assume tags are stored in the format dictated by m_storeManualTagsWithUnderscores
        // For TagEditorWidget, we always want to set them with spaces.
        // So, if they were stored with underscores, convert them back.
        QString captionFromFile = projectFile.value(relativeFilePath).toString();
        if (m_storeManualTagsWithUnderscores) { // If they were stored with underscores
            // This logic is imperfect if the project was saved with a different setting than current.
            // A better project format would store the setting itself or always store with spaces.
            // For now, assume if m_storeManualTagsWithUnderscores is true, the file *might* have them.
        }
        unsavedCaptions[absoluteFilePath] = captionFromFile; // Store as is from file for now
    }
    projectFile.endGroup();

    if (currentMediaIndex >= 0 && currentMediaIndex < mediaFiles.count()) {
        loadCaptionForCurrentImage(); 
    } else if (!mediaFiles.isEmpty()) {
        displayMediaAtIndex(0); 
    }

    setWindowTitle(tr("Haigaku Manager - %1").arg(QFileInfo(m_currentProjectPath).fileName()));
    statusBar()->showMessage(tr("Project %1 opened.").arg(QFileInfo(m_currentProjectPath).fileName()), 5000);
}

void MainWindow::handleModelStatusChanged(const QString &status, const QString &color)
{
    qDebug() << "MainWindow::handleModelStatusChanged - Status:" << status << "Color:" << color;
    if (m_autoCaptionSettingsPanel) { 
        m_autoCaptionSettingsPanel->setModelStatus(status, color);
    }
    if (color == "green" && m_autoCaptionManager && m_tagEditorWidget) { 
        QStringList vocabWithUnderscores = m_autoCaptionManager->getVocabularyForCompletions();
        if (!vocabWithUnderscores.isEmpty()) {
            m_tagEditorWidget->setKnownTagsVocabulary(vocabWithUnderscores);
            qDebug() << "Tag vocabulary set for TagEditorWidget:" << vocabWithUnderscores.size() << "tags. First 5:" << vocabWithUnderscores.mid(0,5);
        } else {
            qDebug() << "Failed to get vocabulary or vocabulary is empty after model load (handleModelStatusChanged).";
        }
    }
}
